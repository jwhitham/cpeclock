
#include "rx433.h"


// constants for new codes
#define PERIOD              0x200   // Timing for new code
// (SYMBOL_SIZE + 1) bits per symbol (includes 1 start bit)
#define NC_FINAL_BIT        (NC_DATA_SIZE * (SYMBOL_SIZE + 1))
#define NC_BUFFER_SIZE      ((NC_FINAL_BIT + 7) / 8)

extern uint32_t micros();

static uint32_t old_time = 0;

// Home Easy decoder state
typedef enum { HE_RESET, HE_RECEIVED_LONG, HE_RECEIVED_SHORT, HE_READY_FOR_BIT } t_he_state;

static t_he_state he_state = HE_RESET;
static uint8_t he_bit_count = 0;
static uint32_t he_bit_data = 0;
volatile uint32_t rx433_home_easy = 0;

// New code decoder state
typedef enum { NC_RESET, NC_START, NC_RECEIVE } t_nc_state;

static t_nc_state nc_state = NC_RESET;
static uint8_t nc_buffer[NC_BUFFER_SIZE] = {0};
static uint32_t nc_timebase = 0;
volatile uint8_t rx433_new_code[NC_DATA_SIZE] = {0};
volatile uint8_t rx433_new_code_ready = 0;

#define EPSILON 100
#define IS_CLOSE(delta, centre) (((delta) + EPSILON - (centre)) < (EPSILON * 2))

#ifdef TEST_MODE
extern void panic();
#endif

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
            if (IS_CLOSE(delta, PERIOD * 2)) {
                // indicates a "10" start code
                nc_state = NC_START;
                nc_timebase = new_time - (PERIOD / 4);
            }
            break;
        case NC_START:
            if (IS_CLOSE(delta, PERIOD * 2)) {
                // repeat of a "10" start code
                nc_timebase = new_time - (PERIOD / 4);
            } else if (IS_CLOSE(delta, PERIOD)) {
                // indicates a "11" start code following a "10" start code
                nc_state = NC_RECEIVE;
                nc_timebase = new_time - (PERIOD / 4);
            } else if (new_time > (nc_timebase + (PERIOD * 2))) {
                // invalid code / timeout
                nc_state = NC_RESET;
            }
            break;
        default: // NC_RECEIVE
            {
                uint32_t bit = (new_time - nc_timebase) / PERIOD;
                // bit 0 = second "1" in "11" start code - undefined value in the array
                // bit 1 = most significant bit in symbol 0
                // bit 6 = first stop bit
                // ... etc...

                if (bit < NC_FINAL_BIT) {
                    nc_buffer[bit / 8] |= 0x80 >> (bit % 8);
                } else {
                    // This is the final bit, or after it
                    uint32_t i, j;
                    bit = 0;
                    // unpack bits into the rx433_new_code array
                    // Each byte in that array is a symbol (i.e. 3 unused bits)
                    for (i = 0; i < NC_DATA_SIZE; i++) {
                        uint8_t unpacked = 0;
                        bit++; // skip the unused bit at the beginning of the received symbol
                        for (j = 0; j < SYMBOL_SIZE; j++) {
                            if (nc_buffer[bit / 8] & (0x80 >> (bit % 8)) ) {
                                unpacked |= (1 << (SYMBOL_SIZE - 1)) >> j;
                            }
                            bit++; // next bit
                        }
                        rx433_new_code[i] = unpacked;
                    }
#ifdef TEST_MODE    // ensure that array size calculations are correct
                    if (bit != NC_FINAL_BIT) {
                        panic();
                    }
#endif
                    // zero the buffer for next use
                    for (i = 0; i < NC_BUFFER_SIZE; i++) {
                        nc_buffer[i] = 0;
                    }
                    // message is ready
                    nc_state = NC_RESET;
                    rx433_new_code_ready = 1;
                }
            }
            break;
    }
}

