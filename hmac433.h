
#ifndef HMAC433_H
#define HMAC433_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

int hmac433_authenticate(
        const uint8_t* secret_data,
        size_t secret_size,
        const uint8_t* packet_data,
        size_t packet_size,
        uint64_t* counter);


#ifdef __cplusplus
}
#endif

#endif

