
#include <stdio.h>
#include <string.h>
#include "hmac.h"
#include "hmac433.h"

#define MAX_PACKET_SIZE 100

int main(void)
{
    uint8_t digest[HMAC_DIGEST_SIZE];
    uint64_t zero = 0;
    uint64_t counter = 0;
    FILE* fd;

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
        fprintf(stderr, "error 1!\n");
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
        fprintf(stderr, "error 2!\n");
        return 1;
    }

    // Test HMAC433 function
    fd = fopen("test_hmac433.bin", "rb");
    if (!fd) {
        perror("opening test_hmac433.bin");
        return 1;
    }
    while (1){
        hmac433_packet_t packet;
        uint8_t size = 0;
        uint8_t action = 0;
        off_t pos = ftell(fd);
        char payload[PACKET_PAYLOAD_SIZE + 1];

        if (!((fread(&size, 1, 1, fd) == 1) 
        && (fread(&action, 1, 1, fd) == 1))) {
            break;
        }

        if (size != sizeof(hmac433_packet_t)) {
            fprintf(stderr, "error with packet size! size in file %d size in memory %d\n",
                (int) size, (int) sizeof(hmac433_packet_t));
            return 1;
        }
        if (fread(&packet, size, 1, fd) != 1) {
            perror("reading test_mac.bin");
            return 1;
        }
        memcpy(payload, packet.payload, PACKET_PAYLOAD_SIZE);
        payload[PACKET_PAYLOAD_SIZE] = '\0';
        if (action & 2) {
            counter += (uint64_t) 1 << (uint64_t) 60;
        }
        if ((action & 1) != hmac433_authenticate(
                (const uint8_t*) "secret", 6,
                &packet,
                &counter)) {
            fprintf(stderr, "error with authentication, packet at %d, expected outcome %d: %s\n",
                        (int) pos, action & 1, payload);
            return 1;
        }
    }
    fclose(fd);
    printf("ok\n");
    return 0;
}
