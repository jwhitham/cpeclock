
#ifndef ALARM_H
#define ALARM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ALARM_DISABLED = 0,
    ALARM_ENABLED,
    ALARM_ACTIVE
} alarm_state_t;

extern void alarm_set(uint8_t hour, uint8_t minute, alarm_state_t state, int force_future);

#ifdef __cplusplus
}
#endif
#endif

