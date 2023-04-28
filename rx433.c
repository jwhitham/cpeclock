
#include "rx433.h"


extern uint32_t micros();

volatile uint32_t rx433_data[MAX_CODE_LENGTH / 32] = {0};
volatile uint32_t rx433_count = 0;

void rx433_interrupt(void)
{
    typedef enum { RESET, FINISHED,
                   HE_RECEIVED_LONG, HE_RECEIVED_SHORT, HE_READY_FOR_BIT,
                   NC_RECEIVED_LONG, NC_RECEIVED_SHORT, NC_READY_FOR_BIT,
                } t_state;

    static uint32_t old_time = 0;
    static t_state state = RESET;
    static uint32_t bit_count = 0;
    static uint32_t bit_data[MAX_CODE_LENGTH / 32] = {0};

    uint32_t new_time = micros();
    uint32_t units = (new_time - old_time) >> 7;
    uint32_t i;

    old_time = new_time;

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
        case 1:
        case 2:
            // New short code
            switch (state) {
                case NC_READY_FOR_BIT:
                    state = NC_RECEIVED_SHORT;
                    break;
                case NC_RECEIVED_LONG:
                    // New Code: long then short -> bit 1
                    bit_data[bit_count >> 5] |= (uint32_t) (1 << 31) >> (bit_count & 31);
                    bit_count ++;
                    state = NC_READY_FOR_BIT;
                    if (bit_count >= MAX_CODE_LENGTH) {
                        state = FINISHED;
                    }
                    break;
                default:
                    // error
                    state = RESET;
                    break;
            }
            break;
        case 3:
        case 4:
        case 5:
            // Home Easy short code or New long code
            switch (state) {
                case HE_READY_FOR_BIT:
                    state = HE_RECEIVED_SHORT;
                    break;
                case HE_RECEIVED_LONG:
                    // Home Easy: long then short -> bit 1
                    bit_data[bit_count >> 5] |= (uint32_t) (1 << 31) >> (bit_count & 31);
                    bit_count ++;
                    state = HE_READY_FOR_BIT;
                    if (bit_count >= MIN_CODE_LENGTH) {
                        state = FINISHED;
                    }
                    break;
                case NC_READY_FOR_BIT:
                    state = NC_RECEIVED_LONG;
                    break;
                case NC_RECEIVED_SHORT:
                    // New Code: short then long -> bit 0
                    bit_count ++;
                    state = NC_READY_FOR_BIT;
                    if (bit_count >= MAX_CODE_LENGTH) {
                        state = FINISHED;
                    }
                    break;
                default:
                    // error
                    state = RESET;
                    break;
            }
            break;
        case 6:
        case 7:
            // New start code
            state = NC_READY_FOR_BIT;
            bit_count = 0;
            for (i = 0; i < (MAX_CODE_LENGTH / 32); i++) {
                bit_data[i] = 0;
            }
            break;
        case 8:
        case 9:
        case 10:
            // New finish code
            switch (state) {
                case NC_READY_FOR_BIT:
                    state = RESET;
                    if ((bit_count >= MIN_CODE_LENGTH)
                    && ((bit_count & 7) == 0)) {
                        state = FINISHED;
                    }
                    break;
                default:
                    // error
                    state = RESET;
                    break;
            }
            break;
        case 11:
        case 12:
            // Home Easy long code
            switch (state) {
                case HE_READY_FOR_BIT:
                    state = HE_RECEIVED_LONG;
                    break;
                case HE_RECEIVED_SHORT:
                    // Home Easy: short then long -> bit 0
                    bit_count ++;
                    state = HE_READY_FOR_BIT;
                    if (bit_count >= MIN_CODE_LENGTH) {
                        state = FINISHED;
                    }
                    break;
                default:
                    // error
                    state = RESET;
                    break;
            }
            break;
        case 22:
        case 23:
            // Home Easy start code
            state = HE_READY_FOR_BIT;
            bit_count = 0;
            for (i = 0; i < (MAX_CODE_LENGTH / 32); i++) {
                bit_data[i] = 0;
            }
            break;
        default:
            state = RESET;
            break;
    }

    if (state == FINISHED) {
        // Transmission finished
        for (i = 0; i < (MAX_CODE_LENGTH / 32); i++) {
            rx433_data[i] = bit_data[i];
        }
        rx433_count = bit_count;
        state = RESET;
    }
}

