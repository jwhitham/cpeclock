
#include "hmac.h"
#include "sha256.h"


void hmac_sha256(
        const uint8_t* key_data,
        size_t key_size,
        const uint8_t* message_data,
        size_t message_size,
        uint8_t* digest_data)
{
    uint8_t k_ipad[BLOCK_SIZE];
    uint8_t k_opad[BLOCK_SIZE];
    SHA256_CTX ctx;
    size_t i;

    for (i = 0; (i < key_size) && (i < BLOCK_SIZE); i++) {
        k_ipad[i] = key_data[i] ^ 0x36;
        k_opad[i] = key_data[i] ^ 0x5c;
    }
    for (; i < BLOCK_SIZE; i++) {
        k_ipad[i] = 0x36;
        k_opad[i] = 0x5c;
    }
    sha256_init(&ctx);
    sha256_update(&ctx, k_ipad, BLOCK_SIZE);
    sha256_update(&ctx, message_data, message_size);
    sha256_final(&ctx, digest_data);

    sha256_init(&ctx);
    sha256_update(&ctx, k_opad, BLOCK_SIZE);
    sha256_update(&ctx, digest_data, DIGEST_SIZE);
    sha256_final(&ctx, digest_data);
}

