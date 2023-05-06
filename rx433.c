
#include <string.h>
#include "rx433.h"


// constants for new codes
#define NC_PULSE 0x100
#define EPSILON  0x40

#define NC_SYMBOL_TIME      ((NC_PULSE * 5) + (NC_PULSE * 2 * SYMBOL_SIZE))

extern uint32_t micros();

static uint32_t old_time = 0;

// Home Easy decoder state
typedef enum { HE_RESET, HE_RECEIVED_LONG, HE_RECEIVED_SHORT, HE_READY_FOR_BIT } t_he_state;

static t_he_state he_state = HE_RESET;
static uint8_t he_bit_count = 0;
static uint32_t he_bit_data = 0;
volatile uint32_t rx433_home_easy = 0;

// New code decoder state
typedef enum { NC_RESET, NC_CONTINUE, NC_RECEIVE } t_nc_state;

static t_nc_state nc_state = NC_RESET;
static uint8_t nc_buffer[NC_DATA_SIZE] = {0};
static uint32_t nc_timebase = 0;
static uint32_t nc_count = 0;
volatile uint8_t rx433_new_code[NC_DATA_SIZE] = {0};
volatile uint8_t rx433_new_code_ready = 0;

#define IS_CLOSE(delta, centre, epsilon) \
        (((delta) + (epsilon) - (centre)) < ((epsilon) * 2))


void rx433_interrupt(void)
{
    uint32_t new_time = micros();
    uint32_t delta = new_time - old_time;

    old_time = new_time;

    // Home Easy
    // low    high  total  meaning  rounded         divided
    // 320    220   540    short    384  .. 767     3  .. 5
    // 1330   220   1550   long     1408 .. 1663    11 .. 12
    // 2700   220   2920   start    2816 .. 3071    22 .. 23
    // 10270  220   10490  gap      10240 ..        80 .. *
    // 
    switch (delta / 128) {
        case 3:
        case 4:
        case 5:
            // Home Easy short code or New long code
            switch (he_state) {
                case HE_READY_FOR_BIT:
                    he_state = HE_RECEIVED_SHORT;
                    break;
                case HE_RECEIVED_LONG:
                    // Home Easy: long then short -> bit 1
                    he_bit_data |= ((uint32_t) 1 << (uint32_t) 31) >> he_bit_count;
                    he_bit_count ++;
                    he_state = HE_READY_FOR_BIT;
                    if (he_bit_count >= 32) {
                        rx433_home_easy = he_bit_data;
                        he_state = HE_RESET;
                    }
                    break;
                default:
                    // error
                    he_state = HE_RESET;
                    break;
            }
            break;
        case 11:
        case 12:
            // Home Easy long code
            switch (he_state) {
                case HE_READY_FOR_BIT:
                    he_state = HE_RECEIVED_LONG;
                    break;
                case HE_RECEIVED_SHORT:
                    // Home Easy: short then long -> bit 0
                    he_bit_count ++;
                    he_state = HE_READY_FOR_BIT;
                    if (he_bit_count >= 32) {
                        rx433_home_easy = he_bit_data;
                        he_state = HE_RESET;
                    }
                    break;
                default:
                    // error
                    he_state = HE_RESET;
                    break;
            }
            break;
        case 22:
        case 23:
            // Home Easy start code
            he_state = HE_READY_FOR_BIT;
            he_bit_count = 0;
            he_bit_data = 0;
            break;
        default:
            he_state = HE_RESET;
            break;
    }

    // New codes
    switch (nc_state) {
        case NC_RESET:
            if (IS_CLOSE(delta, NC_PULSE * 3, EPSILON)) {
                // indicates a "11010" start code - first symbol
                nc_count = 0;
                nc_timebase = new_time;
                nc_state = NC_RECEIVE;
                memset(nc_buffer, 0, NC_DATA_SIZE);
            }
            break;
        case NC_CONTINUE:
            if (IS_CLOSE(delta, NC_PULSE * 3, EPSILON)) {
                // indicates a "11010" start code - Nth symbol?
                uint32_t delta2 = new_time - nc_timebase;

                // Skipped any symbols?
                while ((nc_count < NC_DATA_SIZE)
                && (delta2 > ((NC_SYMBOL_TIME * 3) / 2))) {
                    nc_buffer[nc_count] = 0xff; // erasure
                    nc_count++;
                    nc_timebase += NC_SYMBOL_TIME;
                    delta2 = new_time - nc_timebase;
                }

                if (nc_count >= NC_DATA_SIZE) {
                    // End of previous (incomplete) message, begin new
                    memcpy((uint8_t*)rx433_new_code, nc_buffer, NC_DATA_SIZE);
                    rx433_new_code_ready = 1;
                    nc_count = 0;
                    nc_timebase = new_time;
                    nc_state = NC_RECEIVE;
                    memset(nc_buffer, 0, NC_DATA_SIZE);
                } else if (IS_CLOSE(delta2, NC_SYMBOL_TIME, EPSILON)) {
                    // Valid timing for recovery after skipping
                    nc_timebase = new_time;
                    nc_state = NC_RECEIVE;
                } else {
                    // Not recovered from skipping - this is another
                    // error, wait more for the start of the next symbol
                }
            }
            break;
        default: // NC_RECEIVE
            {
                uint32_t bit =
                    (new_time + NC_PULSE - nc_timebase) / (NC_PULSE * 2);

                if (bit == 0) {
                    // noise within the start bit, ignored
                } else if (bit <= SYMBOL_SIZE) {
                    // bit received
                    nc_buffer[nc_count] |= (1 << SYMBOL_SIZE) >> bit;
                } else if (bit == (SYMBOL_SIZE + 1)) {
                    // stop bit
                    nc_count ++;
                    if (nc_count >= NC_DATA_SIZE) {
                        // end of message
                        memcpy((uint8_t*)rx433_new_code, nc_buffer, NC_DATA_SIZE);
                        nc_state = NC_RESET;
                        rx433_new_code_ready = 1;
                    } else {
                        // symbol ok - wait for start of next symbol
                        nc_state = NC_CONTINUE;
                    }
                } else {
                    // symbol lost? Record erasure, await start code for next symbol
                    nc_buffer[nc_count] = 0xff;
                    nc_state = NC_CONTINUE;
                }
            }
            break;
    }
}

