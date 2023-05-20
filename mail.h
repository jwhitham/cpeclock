
#ifndef MAIL_H
#define MAIL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ALARM_DISABLED = 0,
    ALARM_ENABLED,
    ALARM_ACTIVE
} alarm_state_t;

extern void set_clock(uint8_t hour, uint8_t minute, uint8_t second);
extern void set_alarm(uint8_t hour, uint8_t minute, alarm_state_t state);

void mail_receive_messages(void);
int mail_init(void);
void mail_cancel_alarm(void);
void mail_reload_alarm(uint8_t always_enable);

#ifdef __cplusplus
}
#endif
#endif

