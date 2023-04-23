
#include <stdint.h>


extern uint32_t micros();

volatile uint32_t rx433_data = 0;
volatile uint32_t rx433_count = 0;

void rx433_interrupt(void)
{
    typedef enum { RESET, RECEIVED_LONG, RECEIVED_SHORT, READY_FOR_BIT } t_state;

    static uint32_t old_time = 0;
    static t_state state = RESET;
    static uint8_t bit_count = 0;
    static uint32_t bit_data = 0;

    uint32_t new_time = micros();
    uint32_t units = (new_time - old_time) >> 7;

    old_time = new_time;

    switch (units) {
        case 22:
        case 23:
            // start code
            state = READY_FOR_BIT;
            bit_count = 0;
            bit_data = 0;
            break;
        case 3:
        case 4:
        case 5:
            // short code
            switch (state) {
                case READY_FOR_BIT:
                    state = RECEIVED_SHORT;
                    break;
                case RECEIVED_LONG:
                    // long then short -> bit 1
                    bit_data = (bit_data << 1) | 1;
                    bit_count ++;
                    state = READY_FOR_BIT;
                    break;
                default:
                    // error
                    state = RESET;
                    break;
            }
            break;
        case 11:
        case 12:
            // long code
            switch (state) {
                case READY_FOR_BIT:
                    state = RECEIVED_LONG;
                    break;
                case RECEIVED_SHORT:
                    // short then long -> bit 0
                    bit_data = bit_data << 1;
                    bit_count ++;
                    state = READY_FOR_BIT;
                    break;
                default:
                    // error
                    state = RESET;
                    break;
            }
            break;
        default:
            state = RESET;
            break;
    }

    if ((state == READY_FOR_BIT) && (bit_count >= 32)) {
        /* done */
        if (rx433_data == bit_data) {
            rx433_count ++;
        } else {
            rx433_data = bit_data;
            rx433_count = 1;
        }
        state = RESET;
    }
}

