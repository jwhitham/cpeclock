/* Copyright (C) 2013-2023, Jack Whitham
 * Copyright (C) 2009-2010, University of York
 * Copyright (C) 2004-2006, Advanced Micro Devices, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */




#include <linux/platform_device.h>
#include <linux/compat.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/version.h>
#include <linux/time.h>

#include <asm/io.h>
#include <asm/delay.h>
#include <asm/atomic.h>

#include <linux/types.h>

#define DEV_NAME            "tx433"
#define VERSION             10

#define SYMBOL_SIZE         (5)     // 5 bits per symbol

#define MIN_DATA_SIZE       (9)     // Home Easy: 8 hex digits plus '\n'
#define NC_DATA_SIZE        (31)    // New codes: 31 base-32 symbols
#define NC_PULSE            (0x100) // Timing for new code

static int tx433_pin = 0;
module_param(tx433_pin, int, 0);
// 24 = physical pin 18 = used with heating2
// 26 = physical pin 37 = used with pi3

static void __iomem *gpio;
static uintptr_t gpio1;

static inline void INP_GPIO(unsigned g)
{
    // #define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
    uintptr_t addr = (4 * (g / 10)) + gpio1;
    iowrite32(ioread32((void *) addr) & ~(7<<(((g)%10)*3)), (void *) addr);
}

static inline void OUT_GPIO(unsigned g)
{
    // #define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
    uintptr_t addr = (4 * (g / 10)) + gpio1;
    iowrite32(ioread32((void *) addr) | (1<<(((g)%10)*3)), (void *) addr);
}

static inline void GPIO_SET(unsigned g)
{
    // sets   bits which are 1 ignores bits which are 0
    // #define GPIO_SET *(gpio+7)
    iowrite32(g, (void *) (gpio1 + (4 * 7)));
}

static inline void GPIO_CLR(unsigned g)
{
    // clears bits which are 1 ignores bits which are 0
    // #define GPIO_CLR *(gpio+10)
    iowrite32(g, (void *) (gpio1 + (4 * 10)));
}

static unsigned await_timer = 0;
static struct timeval initial;

static inline unsigned micros(void)
{
    struct timeval t;
    do_gettimeofday(&t);
    t.tv_sec -= initial.tv_sec;
    return ((unsigned) t.tv_sec * (unsigned) 1000000) + t.tv_usec;
}

static inline void await(unsigned us)
{
    await_timer += us;
    while(micros() < await_timer) {}
}

static inline void send_high_var(unsigned us)
{
    GPIO_SET(1 << tx433_pin);
    await(us);
    GPIO_CLR(1 << tx433_pin);
}

static inline void send_high(void)
{
    send_high_var(220);
}

static inline void send_zero(void)
{
    send_high();
    await(1330);
}

static inline void send_one(void)
{
    send_high();
    await(320);
}

static unsigned transmit_home_easy_code(unsigned tx_code, unsigned attempts)
{
    unsigned i, j, start, stop;

    do_gettimeofday(&initial);
    await_timer = start = micros();
    for (i = 0; i < attempts; i++) {
        send_high(); // Start code
        await(2700);
        j = 32;
        while (j > 0) {
            unsigned bits;
            j -= 2;
            bits = (tx_code >> (unsigned) j) & 3;
            if (bits == 0) {
                send_one(); send_zero();
                send_one(); send_zero();
            } else if (bits == 1) {
                send_one(); send_zero();
                send_zero(); send_one();
            } else if (bits == 2) {
                send_zero(); send_one();
                send_one(); send_zero();
            } else {
                send_zero(); send_one();
                send_zero(); send_one();
            }
        }
        send_high(); // End code
        await(10270); // Gap
    }
    stop = micros();
    return stop - start;
}

static unsigned transmit_new_code(uint8_t *message)
{
    size_t i, j;
    unsigned start, stop;

    do_gettimeofday(&initial);
    await_timer = start = micros();

    for (i = 0; i < NC_DATA_SIZE; i++) {
        uint8_t symbol = message[i];
        // start symbol: 11010
        send_high_var(NC_PULSE * 2);
        await(NC_PULSE);
        send_high_var(NC_PULSE);
        await(NC_PULSE);
        // send symbol
        for (j = 0; j < SYMBOL_SIZE; j++) {
            if (symbol & (1 << (SYMBOL_SIZE - 1))) {
                // 10
                send_high_var(NC_PULSE);
                await(NC_PULSE);
            } else {
                // 00
                await(NC_PULSE * 2);
            }
            symbol = symbol << 1;
        }
    }
    // end of final symbol: 10
    send_high_var(NC_PULSE);
    await(NC_PULSE);
    stop = micros();
    return stop - start;
}

static int tx433_open(struct inode *inode, struct file *file)
{
    return nonseekable_open(inode, file);
}

static int tx433_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t tx433_write(struct file *file, const char __user *buf,
                size_t count, loff_t *pos)
{
    char user_data[NC_DATA_SIZE];
    unsigned long flags;
    unsigned timing = 0;

    // The code provided to /dev/tx433 should be an even
    // number of hexadecimal digits ending in '\n'
    if ((count < MIN_DATA_SIZE)
    || (count > NC_DATA_SIZE)
    || ((count & 1) == 0)) {
        return -EINVAL;
    }

    if (copy_from_user(user_data, buf, count)) {
        return -EFAULT;
    }

    if (count == MIN_DATA_SIZE) {
        // 32-bit codes use the Home Easy protocol
        // convert each pair of hex digits to a byte
        uint32_t code = 0;
        long val = 0;

        // check for final '\n'
        if (user_data[count - 1] != '\n') {
            return -EINVAL;
        }
        user_data[count - 1] = '\0';
        if (kstrtol(user_data, 16, &val) != 0) {
            return -EINVAL;
        }
        code = val;
        if ((code == 0) || (code >= (1 << 28))) {
            return -EINVAL;
        }
        // ready for transmission
        local_irq_save(flags);
        timing = transmit_home_easy_code(code, 10);
        local_irq_restore(flags);
        printk(KERN_ERR DEV_NAME ": send %08x took %u\n", code, timing);

    } else if (count == NC_DATA_SIZE) {
        // Long codes use the new protocol - 31 symbols of 5 bits each
        // Reed Solomon encoding is done in userspace as it's not in
        // the Raspberry Pi kernel, nor even in a loadable module... :(
        size_t i;
        for (i = 0; i < NC_DATA_SIZE; i++) {
            if (((uint8_t) user_data[i]) >= (1 << SYMBOL_SIZE)) {
                return -EINVAL;
            }
        }
        local_irq_save(flags);
        timing = transmit_new_code((uint8_t *) user_data);
        local_irq_restore(flags);
        printk(KERN_ERR DEV_NAME ": send new code took %u\n", timing);
    } else {
        return -EINVAL;
    }

    return count;
}

static ssize_t tx433_read(struct file *file, char __user *buf,
                size_t count, loff_t *pos)
{
    return -EINVAL;
}

static struct file_operations tx433_fops = {
    .owner = THIS_MODULE,
    .open = tx433_open,
    .read = tx433_read,
    .write = tx433_write,
    .release = tx433_release,
};

static struct miscdevice tx433_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEV_NAME,
    .fops = &tx433_fops,
};

static int __init tx433_init(void)
{
    unsigned long flags;
    uintptr_t physical = 0;
    unsigned int id = 0;

    printk(KERN_ERR DEV_NAME ": tx433_init\n");
    gpio = NULL;
    if ((tx433_pin <= 0) || (tx433_pin >= 32)) {
        printk(KERN_ERR DEV_NAME ": must specify valid tx433_pin parameter\n");
        return -EINVAL;
    }
    asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r" (id));

    switch (id) {
        case 0x410fb767:    /* Pi1 */
            physical = (0x20200000); /* GPIO controller for RPi1 ARMv6 */
            break;
        case 0x410fd034:    /* Pi3 */
        case 0x410fc075:    /* Pi2 */
            physical = (0x3f200000); /* GPIO controller for RPi2 ARMv7 */
            break;
        default:
            /* Don't know the physical address */
            printk(KERN_ERR DEV_NAME ": unknown CPU ID 0x%08x, refusing to load tx433\n", id);
            return -ENXIO;
    }
    gpio = ioremap_nocache(physical, BLOCK_SIZE);
    if (gpio == NULL) {
        printk(KERN_ERR DEV_NAME ": ioremap_nocache failed (physical %p)\n", (void *) physical);
        return -ENXIO;
    }
    gpio1 = (uintptr_t) gpio;
    printk(KERN_ERR DEV_NAME ": physical %p logical %p version %d GPIO pin %d\n",
        (void *) physical, gpio, VERSION, tx433_pin);
    misc_register(&tx433_misc_device);
    local_irq_save(flags);
    INP_GPIO(tx433_pin);
    OUT_GPIO(tx433_pin);
    local_irq_restore(flags);
    return 0;
}

static void __exit tx433_exit(void)
{
    printk(KERN_ERR DEV_NAME ": tx433_exit\n");
    iounmap(gpio);
    misc_deregister(&tx433_misc_device);
    gpio1 = 0;
    gpio = NULL;
}

subsys_initcall(tx433_init);
module_exit(tx433_exit);

MODULE_AUTHOR("Jack Whitham");
MODULE_DESCRIPTION("433MHz transmitter driver");
MODULE_LICENSE("GPL");

