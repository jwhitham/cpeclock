#ifndef RX433_H
#define RX433_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


#define SYMBOL_SIZE         5
#define NC_DATA_SIZE        31      // New codes: 31 base-32 symbols
#define PERIOD              0x200   // Timing for new code

// (SYMBOL_SIZE + 1) bits per symbol (includes stop bit) plus 1 start bit
#define NC_FINAL_BIT        ((NC_DATA_SIZE * (SYMBOL_SIZE + 1)) + 1)
#define NC_BUFFER_SIZE      ((NC_FINAL_BIT + 7) / 8)

extern volatile uint32_t rx433_home_easy;
extern volatile uint8_t rx433_new_code[NC_BUFFER_SIZE];
extern volatile uint8_t rx433_new_code_ready;


void rx433_interrupt(void);

#ifdef __cplusplus
}
#endif

#endif
