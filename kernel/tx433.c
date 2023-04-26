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
#define VERSION             3
#define MIN_CODE_LENGTH     32      // bits for Home Easy
#define MAX_CODE_LENGTH     128     // max bits for new codes

#define MIN_HEX_DATA_SIZE ((((MIN_CODE_LENGTH / 8) * 2)) + 1)   // Home Easy
#define MAX_HEX_DATA_SIZE ((((MAX_CODE_LENGTH / 8) * 2)) + 1)   // New codes

// #define TX_PIN           24      // physical pin 18
#define TX_PIN              26      // physical pin 37


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

static inline void send_high(unsigned us)
{
    GPIO_SET(1 << TX_PIN);
    await(us);
    GPIO_CLR(1 << TX_PIN);
}

typedef struct code_properties_s {
    unsigned    high_time;
    unsigned    start_low_time;
    unsigned    short_low_time;
    unsigned    long_low_time;
    unsigned    finish_low_time;
    unsigned    repeats;
} code_properties_t;


static const code_properties_t home_easy_properties = {
    .high_time = 220,
    .start_low_time = 2700,
    .short_low_time = 320,
    .long_low_time = 1330,
    .finish_low_time = 10270,
    .repeats = 5,
};

static const code_properties_t new_code_properties = {
    .high_time =       0x080,
    .start_low_time =  0x300,
    .short_low_time =  0x080,
    .long_low_time =   0x180,
    .finish_low_time = 0x400,
    .repeats = 3,
};

static unsigned transmit_code(
        uint8_t* bytes,
        size_t num_bytes,
        const code_properties_t* cp)
{
    unsigned i, j, k, start, stop;

    do_gettimeofday(&initial);
    await_timer = start = micros();
    for (i = cp->repeats; i != 0; i--) {
        // Send start code
        send_high(cp->high_time);
        await(cp->start_low_time);
        for (j = 0; j < num_bytes; j++) {
            uint8_t byte = bytes[j];
            for (k = 0; k < 8; k++) {
                if (byte & 0x80) {
                    // Send 'one' bit
                    send_high(cp->high_time);
                    await(cp->long_low_time);
                    send_high(cp->high_time);
                    await(cp->short_low_time);
                } else {
                    // Send 'zero' bit
                    send_high(cp->high_time);
                    await(cp->short_low_time);
                    send_high(cp->high_time);
                    await(cp->long_low_time);
                }
                byte = byte << 1;
            }
        }
        // Send finish code
        send_high(cp->high_time);
        await(cp->finish_low_time);
    }
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
    char hex_data[MAX_HEX_DATA_SIZE];
    uint8_t bytes[MAX_HEX_DATA_SIZE / 2];
    unsigned long flags;
    unsigned timing = 0;
    size_t i;

    // The code provided to /dev/tx433 should be an even
    // number of hexadecimal digits ending in '\n'
    if ((count < MIN_HEX_DATA_SIZE)
    || (count > MAX_HEX_DATA_SIZE)
    || ((count & 1) == 0)) {
        return -EINVAL;
    }

    if (copy_from_user(hex_data, buf, count)) {
        return -EFAULT;
    }

    // check for final '\n'
    if (hex_data[count - 1] != '\n') {
        return -EINVAL;
    }

    // convert each pair of hex digits to a byte
    for (i = 0; i < (count - 1); i += 2) {
        char save_next = hex_data[i + 2];
        long val = 0;
        hex_data[i + 2] = '\0';
        if (kstrtol(&hex_data[i], 16, &val) != 0) {
            return -EINVAL;
        }
        bytes[i / 2] = val;
        hex_data[i + 2] = save_next;
    }
    // ready for transmission
    local_irq_save(flags);
    if (count == MIN_HEX_DATA_SIZE) {
        // 32-bit codes use the Home Easy protocol
        timing = transmit_code(bytes, (unsigned) ((count - 1) / 2),
                               &home_easy_properties);
    } else {
        // Longer codes use the new code protocol
        timing = transmit_code(bytes, (unsigned) ((count - 1) / 2),
                               &new_code_properties);
    }
    local_irq_restore(flags);

    // debug output
    hex_data[count - 1] = '\0';
    printk(KERN_ERR DEV_NAME ": send %s took %u\n", hex_data, timing);
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
    printk(KERN_ERR DEV_NAME ": physical %p logical %p version %d\n",
        (void *) physical, gpio, VERSION);
    misc_register(&tx433_misc_device);
    local_irq_save(flags);
    INP_GPIO(TX_PIN);
    OUT_GPIO(TX_PIN);
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

