
#include <stdio.h>
#include <string.h>
#include "hmac.h"


int main(void)
{
    uint8_t digest[DIGEST_SIZE];

    hmac_sha256(
        (const uint8_t*) "keydata", 7,
        (const uint8_t*) "message", 7,
        digest);
    if (memcmp(digest,
            "\xea\x2e\xd4\x62\x58\x6c\x61\xa9\xc2\x95\xd7\xb5\x2c\x95\x1f\x7"
            "\x8d\x6d\x17\x61\x71\x16\xfa\x84\x38\x70\xc3\x86\x59\xf\x1f\x1d",
            DIGEST_SIZE) != 0) {
        printf("error!\n");
        return 1;
    }
    hmac_sha256(
        (const uint8_t*) "k", 1,
        (const uint8_t*) "m", 1,
        digest);
    if (memcmp(digest,
            "\xb6\x00\x90\xe3\x5\x22\x97\xae\xb5\xa0\x80\x88\x9c\xe2\xfc\x4b"
            "\xca\x95\x7e\x75\x6f\xae\xb4\xdf\x7d\x31\x80\xc\xa1\xe7\x71\xec",
            DIGEST_SIZE) != 0) {
        printf("error!\n");
        return 1;
    }
    printf("ok\n");
    return 0;
}
