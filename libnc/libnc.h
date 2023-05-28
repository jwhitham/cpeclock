#ifndef LIBNC_H
#define LIBNC_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

int libnc_init(void);
int libnc_encode(const uint8_t* payload, size_t payload_size,
                 uint8_t* message, size_t max_message_size);


#ifdef __cplusplus
}
#endif

#endif

