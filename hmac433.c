
#include <stdint.h>
#include <string.h>
#include "hmac433.h"
#include "hmac.h"


int hmac433_authenticate(
        const uint8_t* secret_data,
        size_t secret_size,
        const hmac433_packet_t* packet,
        uint64_t* counter)
{
    uint8_t digest_data[HMAC_DIGEST_SIZE];
    uint64_t new_counter;

    if (secret_size > (HMAC_BLOCK_SIZE - 8)) {
        secret_size = HMAC_BLOCK_SIZE - 8;
    }

    // update counter
    new_counter = ((*counter) & ~((uint64_t) 0xff)) | (uint64_t) packet->counter_low;
    if (new_counter < (*counter)) {
        new_counter += 0x100;
    }

    // compute HMAC
    hmac_sha256(secret_data, secret_size,
                &new_counter,
                packet->payload, PACKET_PAYLOAD_SIZE,
                digest_data);

    if (memcmp(digest_data, packet->hmac, PACKET_HMAC_SIZE) != 0) {
        // message fails the check
        return 0;
    }
    // message ok
    (* counter) = new_counter + 1;
    return 1;
}

void hmac433_encode(
        const uint8_t* secret_data,
        size_t secret_size,
        hmac433_packet_t* packet,
        uint64_t* counter)
{
    uint8_t digest_data[HMAC_DIGEST_SIZE];

    if (secret_size > (HMAC_BLOCK_SIZE - 8)) {
        secret_size = HMAC_BLOCK_SIZE - 8;
    }

    // update counter
    (* counter) += 1;

    // compute HMAC
    hmac_sha256(secret_data, secret_size,
                counter,
                packet->payload, PACKET_PAYLOAD_SIZE,
                digest_data);

    // add HMAC and counter to packet
    memcpy(packet->hmac, digest_data, PACKET_HMAC_SIZE);
    packet->counter_low = (uint8_t) (* counter);
}

