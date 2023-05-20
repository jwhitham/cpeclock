
#ifndef MAIL_H
#define MAIL_H

#include "alarm.h"

#ifdef __cplusplus
extern "C" {
#endif

void mail_receive_messages(void);
int mail_init(void);
void mail_set_alarm_state(alarm_state_t alarm_state);
void mail_reload_alarm(int force_enable);

#ifdef __cplusplus
}
#endif
#endif

