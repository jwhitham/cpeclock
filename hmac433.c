
#include <stdint.h>
#include <string.h>
#include "hmac433.h"
#include "hmac.h"


int hmac433_authenticate(
        const uint8_t* secret_data,
        size_t secret_size,
        const uint8_t* packet_data,
        size_t packet_size,
        uint64_t* counter)
{
    uint8_t digest_data[HMAC_DIGEST_SIZE];
    uint64_t new_counter;

    if (packet_size <= 8) {
        // too short
        return 0;
    }
    if (secret_size > (HMAC_BLOCK_SIZE - 8)) {
        secret_size = HMAC_BLOCK_SIZE - 8;
    }

    // update counter
    new_counter = ((*counter) & ~((uint64_t) 0xff)) | (uint64_t) packet_data[packet_size - 8];
    if (new_counter < (*counter)) {
        new_counter += 0x100;
    }

    // compute HMAC
    hmac_sha256(secret_data, secret_size,
                &new_counter,
                packet_data, packet_size - 7,
                digest_data);

    if (memcmp(digest_data, &packet_data[packet_size - 7], 7) != 0) {
        // message fails the check
        return 0;
    }
    // message ok
    (* counter) = new_counter + 1;
    return 1;
}

