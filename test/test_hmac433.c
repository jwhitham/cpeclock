
#include <stdio.h>
#include <string.h>
#include "hmac.h"


int main(void)
{
    uint8_t digest[HMAC_DIGEST_SIZE];
    uint64_t zero = 0;

    // Test HMAC algorithm and SHA256
    // This test data was made with Python's hmac module
    // e.g. d=hmac.digest(b"keydata", b"message", "sha256")
    hmac_sha256(
        (const uint8_t*) "keydata", 7,
        &zero,
        (const uint8_t*) "message", 7,
        digest);
    if (memcmp(digest,
            "\xea\x2e\xd4\x62\x58\x6c\x61\xa9\xc2\x95\xd7\xb5\x2c\x95\x1f\x7"
            "\x8d\x6d\x17\x61\x71\x16\xfa\x84\x38\x70\xc3\x86\x59\xf\x1f\x1d",
            HMAC_DIGEST_SIZE) != 0) {
        printf("error!\n");
        return 1;
    }
    hmac_sha256(
        (const uint8_t*) "k", 1,
        &zero,
        (const uint8_t*) "m", 1,
        digest);
    if (memcmp(digest,
            "\xb6\x00\x90\xe3\x5\x22\x97\xae\xb5\xa0\x80\x88\x9c\xe2\xfc\x4b"
            "\xca\x95\x7e\x75\x6f\xae\xb4\xdf\x7d\x31\x80\xc\xa1\xe7\x71\xec",
            HMAC_DIGEST_SIZE) != 0) {
        printf("error!\n");
        return 1;
    }

    // Test HMAC433 function
    /*
    fd = fopen("test_mac.bin", "rb");
    if (!fd) {
        perror("opening test_mac.bin");
        return 1;
    }
    while (fread(&size, 4, 1, fd) == 1) {
        if (fread(packet, size, 1, fd) != 1) {
            perror("reading test_mac.bin");
            return 1;
        }
int hmac433_authenticate(
        const uint8_t* secret_data,
        size_t secret_size,
        const uint8_t* packet_data,
        size_t packet_size,
        uint64_t* counter);
    */
    printf("ok\n");
    return 0;
}
