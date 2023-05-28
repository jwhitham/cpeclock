#ifndef NCRS_H
#define NCRS_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

int libtxnc433_init(void);
int libtxnc433_send(const uint8_t* payload, size_t size);


#ifdef __cplusplus
}
#endif

#endif

