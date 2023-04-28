
#ifndef HMAC_H
#define HMAC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define HMAC_DIGEST_SIZE     32
#define HMAC_BLOCK_SIZE      64


void hmac_sha256(
        const uint8_t* key_data,
        size_t key_size,
        const uint64_t* counter,
        const uint8_t* message_data,
        size_t message_size,
        uint8_t* digest_data);


#ifdef __cplusplus
}
#endif

#endif
