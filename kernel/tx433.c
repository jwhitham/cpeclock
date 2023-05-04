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
#include <linux/rslib.h>

#include <asm/io.h>
#include <asm/delay.h>
#include <asm/atomic.h>

#include <linux/types.h>

#define DEV_NAME            "tx433"
#define VERSION             5

#define SYMBOL_SIZE     (5)     // 5 bits per symbol
#define NROOTS          (10)    // 10 parity symbols
#define MSG_SYMBOLS     (21)    // 21 message symbols
#define GFPOLY          (0x25)  // Reed Solomon Galois field polynomial
#define FCR             (1)     // First Consecutive Root
#define PRIM            (1)     // Primitive Element

#define MIN_DATA_SIZE (9)           // Home Easy: 8 hex digits plus '\n'
#define MAX_DATA_SIZE (MSG_SYMBOLS) // New codes: 21 base-32 symbols plus '\n'

// #define TX_PIN           24      // physical pin 18
#define TX_PIN              26      // physical pin 37


static void __iomem *gpio;
static uintptr_t gpio1;
static struct rs_control *reed_solomon;

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

static unsigned transmit_home_easy_code(unsigned tx_code, unsigned attempts)
{
    unsigned i, j, start, stop;
    const unsigned high_time = 220;        //  0x0dc
    const unsigned start_low_time = 2700;  //  0xa8c  ->  0xb68 for start code
    const unsigned short_low_time = 320;   //  0x140  ->  0x21c for short code
    const unsigned long_low_time = 1330;   //  0x532  ->  0x60e for long code
    const unsigned finish_low_time = 10270;// 0x281e  -> 0x28fa for finish code

    do_gettimeofday(&initial);
    await_timer = start = micros();
    for (i = 0; i < attempts; i++) {
        // Send start code
        send_high(high_time);
        await(start_low_time);
        j = 32;
        while (j > 0) {
            j -= 1;
            if ((tx_code >> (unsigned) j) & 1) {
                // Send 'one' bit
                send_high(high_time);
                await(long_low_time);
                send_high(high_time);
                await(short_low_time);
            } else {
                // Send 'zero' bit
                send_high(high_time);
                await(short_low_time);
                send_high(high_time);
                await(long_low_time);
            }
        }
        // Send finish code
        send_high(high_time);
        await(finish_low_time);
    }
    stop = micros();
    return stop - start;
}


typedef struct home_easy_style_code_properties_s {
    unsigned    high_time;
    unsigned    start_low_time;
    unsigned    short_low_time;
    unsigned    long_low_time;
    unsigned    finish_low_time;
    unsigned    final;
    unsigned    repeats;
} home_easy_style_code_properties_t;


static const home_easy_style_code_properties_t classic_home_easy_properties = {
    .final = 0,
    .repeats = 5,
};

static unsigned transmit_home_easy_style_code(
        uint8_t* bytes,
        size_t num_bytes,
        const home_easy_style_code_properties_t* cp)
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
    if (cp->final) {
        // Final code to mark the end of transmission
        send_high(cp->high_time);
        await(cp->short_low_time);
    }
    stop = micros();
    return stop - start;
}

static unsigned transmit_new_code(uint8_t *message)
{
    const unsigned period = 0x200;
    const unsigned high = 0x100;
    size_t i, j;
    unsigned start, stop;

    do_gettimeofday(&initial);
    await_timer = start = micros();

    // send start code: 1010101010
    for (j = 0; j < 5; j++) {
        send_high(high);
        await((period * 2) - high);
    }
    for (i = 0; i < (MAX_DATA_SIZE + NROOTS); i++) {
        uint8_t symbol = message[i];
        // send 2 symbol start bits: 11
        for (j = 0; j < 2; j++) {
            send_high(high);
            await(period - high);
        }
        // send symbol
        for (j = 0; j < SYMBOL_SIZE; j++) {
            if (symbol & (1 << (SYMBOL_SIZE - 1))) {
                send_high(high);
                await(period - high);
            } else {
                await(period);
            }
            symbol = symbol << 1;
        }
    }
    // "send" finish code: 00
    await(period * 2);
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
    char user_data[MAX_DATA_SIZE];
    unsigned long flags;
    unsigned timing = 0;

    // The code provided to /dev/tx433 should be an even
    // number of hexadecimal digits ending in '\n'
    if ((count < MIN_DATA_SIZE)
    || (count > MAX_DATA_SIZE)
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
        timing = transmit_home_easy_code(code, 5);
        local_irq_restore(flags);
        printk(KERN_ERR DEV_NAME ": send %08x took %u\n", code, timing);

    } else if (count == MSG_SYMBOLS) {
        // Long codes use the new protocol
        // 105 bits of payload, encoded as 5 bit symbols, in 21 bytes
        // to which we add 50 bits of parity data with Reed Solomon
        // for a total of 155 bits or 31 symbols
        size_t i;
        uint16_t par[NROOTS];
        uint8_t message[MSG_SYMBOLS + NROOTS];
        for (i = 0; i < MSG_SYMBOLS; i++) {
            message[i] = user_data[i];
            if (message[i] >= (1 << SYMBOL_SIZE)) {
                return -EINVAL;
            }
        }
        // Reed Solomon encoding
        if (encode_rs8(reed_solomon, message, MAX_DATA_SIZE, par, 0) < 0) {
            printk(KERN_ERR DEV_NAME ": encode_rs8 failed\n");
            return -ENOMSG;
        }
        for (i = 0; i < NROOTS; i++) {
            message[i + MSG_SYMBOLS] = par[i];
        }
        // transmit
        local_irq_save(flags);
        timing = transmit_new_code(message);
        local_irq_restore(flags);
        // Sanity check (test error recovery from deliberate corruption)
        message[0] ^= 0x1;
        if ((decode_rs8(reed_solomon, message, par, MSG_SYMBOLS,
                            NULL, 0, NULL, 0, NULL) != 1)
        || (memcmp(user_data, message, MSG_SYMBOLS) != 0)) {
            printk(KERN_ERR DEV_NAME ": decode_rs8 failed\n");
            return -ENOMSG;
        }
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
    reed_solomon = init_rs(SYMBOL_SIZE, GFPOLY, FCR, PRIM, NROOTS);
    if (!reed_solomon) {
        printk(KERN_ERR DEV_NAME ": init_rs failed\n");
        return -ENOMSG;
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
    free_rs(reed_solomon);
    gpio1 = 0;
    gpio = NULL;
}

subsys_initcall(tx433_init);
module_exit(tx433_exit);

MODULE_AUTHOR("Jack Whitham");
MODULE_DESCRIPTION("433MHz transmitter driver");
MODULE_LICENSE("GPL");

