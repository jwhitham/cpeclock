
#include "rx433.h"


extern uint32_t micros();

volatile uint32_t rx433_home_easy = 0;

typedef enum { RESET,
               RECEIVED_LONG, RECEIVED_SHORT, READY_FOR_BIT,
            } t_he_state;

// Home Easy decoder state
static t_he_state he_state = RESET;
static uint32_t he_old_time = 0;
static uint8_t he_bit_count = 0;
static uint32_t he_bit_data = 0;

void rx433_interrupt(void)
{
    uint32_t new_time = micros();
    uint32_t units = (new_time - he_old_time) >> 7;

    he_old_time = new_time;

    // Home Easy
    // low    high  total  meaning  rounded         divided
    // 320    220   540    short    384  .. 767     3  .. 5
    // 1330   220   1550   long     1408 .. 1663    11 .. 12
    // 2700   220   2920   start    2816 .. 3071    22 .. 23
    // 10270  220   10490  gap      10240 ..        80 .. *
    // 
    // New Code
    // low    high  total  meaning  rounded         divided
    // 0x80   0x80  0x100  short     0x80 .. 0x17f  1 .. 2
    // 0x180  0x80  0x200  long     0x180 .. 0x2ff  3 .. 5
    // 0x300  0x80  0x380  start    0x300 .. 0x3ff  6 .. 7
    // 0x400  0x80  0x480  gap      0x400 .. 0x4ff  8 .. 10
    switch (units) {
        case 3:
        case 4:
        case 5:
            // Home Easy short code or New long code
            switch (he_state) {
                case READY_FOR_BIT:
                    he_state = RECEIVED_SHORT;
                    break;
                case RECEIVED_LONG:
                    // Home Easy: long then short -> bit 1
                    he_bit_data |= ((uint32_t) 1 << (uint32_t) 31) >> he_bit_count;
                    he_bit_count ++;
                    he_state = READY_FOR_BIT;
                    if (he_bit_count >= 32) {
                        rx433_home_easy = he_bit_data;
                        he_state = RESET;
                    }
                    break;
                default:
                    // error
                    he_state = RESET;
                    break;
            }
            break;
        case 11:
        case 12:
            // Home Easy long code
            switch (he_state) {
                case READY_FOR_BIT:
                    he_state = RECEIVED_LONG;
                    break;
                case RECEIVED_SHORT:
                    // Home Easy: short then long -> bit 0
                    he_bit_count ++;
                    he_state = READY_FOR_BIT;
                    if (he_bit_count >= 32) {
                        rx433_home_easy = he_bit_data;
                        he_state = RESET;
                    }
                    break;
                default:
                    // error
                    he_state = RESET;
                    break;
            }
            break;
        case 22:
        case 23:
            // Home Easy start code
            he_state = READY_FOR_BIT;
            he_bit_count = 0;
            he_bit_data = 0;
            break;
        default:
            he_state = RESET;
            break;
    }

    // Decode the new codes here!
}

