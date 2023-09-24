
#ifndef NIGHT_DAY_H
#define NIGHT_DAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


// Called as a result of an incoming message.
void night_day_time_set(uint8_t start_night_hour, uint8_t start_night_minute,
                        uint8_t start_day_hour, uint8_t start_day_minute);

// Returns 1 if it is night time
int night_day_time_test(uint8_t now_hour, uint8_t now_minute);

// Called during boot
void night_day_time_init(void);


#ifdef __cplusplus
}
#endif
#endif

