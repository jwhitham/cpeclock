
#ifndef ALARM_H
#define ALARM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WHOLE_DAY (24*60)
#define ALARM_SOUNDS_FOR (10) // minutes

// Called as a result of an incoming message. Alarm is set for some time in the future.
int alarm_set(uint8_t hour, uint8_t minute);

// Called as a result of an incoming message, or pressing the left button. Alarm is unset.
int alarm_unset(void);

// Called as a result of pressing the right button. Alarm is set for the same time again - but in the future.
int alarm_reset(void);

// Get the alarm time
void alarm_get(uint8_t* hour, uint8_t* minute);

// Returns > 0 if the alarm should be sounding now.
// The return value is the number of minutes since activation, rounded up.
unsigned alarm_update(uint8_t now_hour, uint8_t now_minute);

// Called during boot
void alarm_init(void);


#ifdef __cplusplus
}
#endif
#endif

