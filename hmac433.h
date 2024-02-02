
#ifndef HMAC433_H
#define HMAC433_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define PACKET_PAYLOAD_SIZE 6
#define PACKET_HMAC_SIZE    6

typedef struct hmac433_packet_s {
    uint8_t     payload[PACKET_PAYLOAD_SIZE];
    uint8_t     counter_low;
    uint8_t     hmac[PACKET_HMAC_SIZE];
    uint8_t     counter_resync_flag;
} hmac433_packet_t;

int hmac433_authenticate(
        const uint8_t* secret_data,
        size_t secret_size,
        const hmac433_packet_t* packet,
        uint64_t* counter);
void hmac433_encode(
        const uint8_t* secret_data,
        size_t secret_size,
        hmac433_packet_t* packet,
        uint64_t* counter);


#ifdef __cplusplus
}
#endif

#endif

