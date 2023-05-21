
#ifndef ALARM_H
#define ALARM_H

#ifdef __cplusplus
extern "C" {
#endif

// Called as a result of an incoming message. Alarm is set for some time in the future.
void alarm_set(uint8_t hour, uint8_t minute);

// Called as a result of an incoming message, or pressing the left button. Alarm is unset.
void alarm_unset(void);

// Called as a result of pressing the right button. Alarm is set for the same time again - but in the future.
void alarm_reset(void);

// Get the alarm time
void alarm_get(uint8_t* hour, uint8_t* minute);

// Returns 1 if the alarm should be sounding now.
int alarm_update(uint8_t now_hour, uint8_t now_minute);

// Called during boot
void alarm_init(void);


#ifdef __cplusplus
}
#endif
#endif

