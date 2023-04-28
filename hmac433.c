
#include <stdint.h>
#include <string.h>
#include "hmac433.h"
#include "hmac.h"

typedef struct key_data_s {
    uint8_t secret_data[BLOCK_SIZE - 8];
    uint64_t new_counter;
} key_data_t;


int hmac433_authenticate(
        const uint8_t* secret_data,
        size_t secret_size,
        const uint8_t* packet_data,
        size_t packet_size,
        uint64_t* counter)
{
    uint8_t digest_data[DIGEST_SIZE];
    key_data_t key_data;

    if (packet_size <= 8) {
        // too short
        return 0;
    }
    if (secret_size > (BLOCK_SIZE - 8)) {
        secret_size = BLOCK_SIZE - 8;
    }

    // prepare the key
    memset(&key_data, 0, sizeof(key_data_t));
    memcpy(key_data.secret_data, secret_data, secret_size);

    // update counter
    key_data.new_counter = ((*counter) & ~((uint64_t) 0xff)) | (uint64_t) packet_data[packet_size - 8];
    if (key_data.new_counter < (*counter)) {
        key_data.new_counter += 0x100;
    }

    hmac_sha256((const uint8_t *) &key_data, BLOCK_SIZE,
                packet_data, packet_size - 7,
                digest_data);

    if (memcmp(digest_data, &packet_data[packet_size - 7], 7) != 0) {
        // message fails the check
        return 0;
    }
    // message ok
    (* counter) = key_data.new_counter + 1;
    return 1;
}

