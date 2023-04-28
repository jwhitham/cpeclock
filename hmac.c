
#include "hmac.h"
#include "sha256.h"


void hmac_sha256(
        const uint8_t* key_data,
        size_t key_size,
        const uint64_t* counter,
        const uint8_t* message_data,
        size_t message_size,
        uint8_t* digest_data)
{
    uint8_t k_ipad[HMAC_BLOCK_SIZE];
    uint8_t k_opad[HMAC_BLOCK_SIZE];
    SHA256_CTX ctx;
    size_t i, j;
    const uint8_t* counter_data = (const uint8_t *) counter;

    for (i = 0; (i < key_size) && (i < HMAC_BLOCK_SIZE); i++) {
        k_ipad[i] = key_data[i] ^ 0x36;
        k_opad[i] = k_ipad[i] ^ (0x5c ^ 0x36);
    }
    for (; (j < 8) && (i < HMAC_BLOCK_SIZE); i++, j++) {
        k_ipad[i] = counter_data[j] ^ 0x36;
        k_opad[i] = k_ipad[i] ^ (0x5c ^ 0x36);
    }
    for (; i < HMAC_BLOCK_SIZE; i++) {
        k_ipad[i] = 0x36;
        k_opad[i] = 0x5c;
    }
    sha256_init(&ctx);
    sha256_update(&ctx, k_ipad, HMAC_BLOCK_SIZE);
    sha256_update(&ctx, message_data, message_size);
    sha256_final(&ctx, digest_data);

    sha256_init(&ctx);
    sha256_update(&ctx, k_opad, HMAC_BLOCK_SIZE);
    sha256_update(&ctx, digest_data, HMAC_DIGEST_SIZE);
    sha256_final(&ctx, digest_data);
}

