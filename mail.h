
#ifndef MAIL_H
#define MAIL_H

#include "alarm.h"

#ifdef __cplusplus
extern "C" {
#endif

void mail_receive_messages(void);
int mail_init(void);

#ifdef __cplusplus
}
#endif
#endif

