#ifndef RX433_H
#define RX433_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define MIN_CODE_LENGTH     32      // bits for Home Easy
#define MAX_CODE_LENGTH     256     // max bits for new codes

extern volatile uint32_t rx433_data[MAX_CODE_LENGTH / 32];
extern volatile uint32_t rx433_count;


void rx433_interrupt(void);

#ifdef __cplusplus
}
#endif

#endif
