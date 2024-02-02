
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

    if (!packet->counter_resync_flag) {
        // Ordinary packet: update counter based on counter_low only
        new_counter = ((*counter) & ~((uint64_t) 0xff)) | (uint64_t) packet->counter_low;
        if (new_counter < (*counter)) {
            new_counter += 0x100;
        }
    } else {
        // Special packet: update counter based on the payload
        size_t i;

        // payload stores counter bits 63..16 in big endian form
        new_counter = 0;
        for (i = 0; i < PACKET_PAYLOAD_SIZE; i++) {
            new_counter |= (uint64_t) packet->payload[i];
            new_counter = new_counter << 8;
        }
        // counter_low stores bits 15..8
        new_counter |= (uint64_t) packet->counter_low;
        new_counter = new_counter << 8;

        if (new_counter < (* counter)) {
            // This is not valid - the counter must always advance
            return 0;
        }
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

    if (!packet->counter_resync_flag) {
        // Ordinary packet: increment counter and add to the packet
        (* counter) += 1;
        packet->counter_low = (uint8_t) (* counter);
    } else {
        // Special packet: increment counter, realign and resynchronise
        uint64_t tmp;
        size_t i;

        // Counter advanced to the next multiple of 256
        (* counter) += 0x100;
        (* counter) &= ~((uint64_t) 0xff);
        tmp = (* counter);

        // payload stores counter bits 63..16 in big endian form
        for (i = 0; i < PACKET_PAYLOAD_SIZE; i++) {
            packet->payload[i] = (uint8_t) (tmp >> (uint64_t) 56);
            tmp = tmp << 8;
        }
        // counter_low stores bits 15..8
        packet->counter_low = (uint8_t) (tmp >> (uint64_t) 56);
        // and bits 7..0 are zero.
    }

    // compute HMAC
    hmac_sha256(secret_data, secret_size,
                counter,
                packet->payload, PACKET_PAYLOAD_SIZE,
                digest_data);

    // add HMAC to packet
    memcpy(packet->hmac, digest_data, PACKET_HMAC_SIZE);
}

