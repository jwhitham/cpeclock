
#ifndef HAL_H
#define HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

extern void disable_interrupts(void);
extern void enable_interrupts(void);
extern uint8_t nvram_read(uint8_t addr);
extern void nvram_write(uint8_t addr, uint8_t data);
extern void display_message(const char* msg);
extern void display_message_lp(const char* msg);
extern uint32_t micros();
extern void clock_set(uint8_t hour, uint8_t minute, uint8_t second);


#ifdef __cplusplus
}
#endif
#endif

