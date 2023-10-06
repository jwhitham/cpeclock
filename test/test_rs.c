
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ncrs.h"
#include "sha256.h"

static SHA256_CTX sha;

void display_message(const char* m)
{
    printf("message: %s\n", m);
    exit(1);
}

static uint32_t entropy(void)
{
    uint32_t hash[SHA256_BLOCK_SIZE / 4];
    SHA256_CTX shacopy;

    memcpy(&shacopy, &sha, sizeof(SHA256_CTX));
    sha256_final(&shacopy, (BYTE*) hash);
    sha256_update(&sha, (const BYTE*) hash, SHA256_BLOCK_SIZE);
    return hash[0];
}

static void add_noise(uint8_t* transmit)
{
    size_t i = entropy() % (NC_DATA_SIZE * SYMBOL_SIZE);
    transmit[i / SYMBOL_SIZE] ^= (1 << (i % SYMBOL_SIZE));
}

static void shift_left(uint8_t* transmit)
{
    size_t i;
    for (i = 0; i < (NC_DATA_SIZE - 1); i++) {
        transmit[i] = transmit[i + 1];
    }
    transmit[NC_DATA_SIZE - 1] = 0;
}

static void shift_right(uint8_t* transmit)
{
    size_t i;
    for (i = NC_DATA_SIZE; i > 0; i--) {
        transmit[i - 1] = transmit[i - 2];
    }
    transmit[0] = 0;
}

typedef enum {
    NEVER_FAILS, ALWAYS_FAILS, SOMETIMES_FAILS, ALMOST_ALWAYS_FAILS
} fail_type_t;

int main(void)
{
    unsigned test_case, j;
    int ok;
    uint8_t message[DECODED_DATA_BYTES];
    uint8_t transmit[NC_DATA_SIZE];
    uint8_t recovered[DECODED_DATA_BYTES];
    unsigned sometimes_fails_bad = 0;
    unsigned sometimes_fails_good = 0;
    unsigned almost_always_fails_bad = 0;
    unsigned almost_always_fails_good = 0;

    if (!ncrs_init()) {
        printf("rs = null\n");
        return 1;
    }

    sha256_init(&sha);
    sha256_update(&sha, (const BYTE*) "s33d", 4);

    for (test_case = 0; test_case < 10000; test_case++) {
        fail_type_t expect = SOMETIMES_FAILS;
        memset(recovered, 0x55, DECODED_DATA_BYTES);
        memset(transmit, 0x55, NC_DATA_SIZE);

        // 105 random bits:
        for (j = 0; j < DECODED_DATA_BYTES; j++) {
            message[j] = entropy() & 0xff;
        }
        message[DECODED_DATA_BYTES - 1] &= 0x80;

        // Encoded
        ncrs_encode(transmit, message);

        // Possibly messed with
        switch ((test_case / 10) % 10) {
            case 0:
                expect = NEVER_FAILS;
                break;
            case 1:
                shift_left(transmit);
                break;
            case 2:
                shift_right(transmit);
                break;
            case 3:
                shift_right(transmit);
                shift_right(transmit);
                break;
            case 4:
                shift_left(transmit);
                shift_left(transmit);
                break;
            case 5:
                shift_left(transmit);
                shift_left(transmit);
                shift_left(transmit);
                break;
            case 6:
                expect = NEVER_FAILS;
                add_noise(transmit);
                break;
            case 7:
                expect = NEVER_FAILS;
                add_noise(transmit);
                add_noise(transmit);
                break;
            case 8:
                shift_left(transmit);
                shift_left(transmit);
                shift_left(transmit);
                add_noise(transmit);
                add_noise(transmit);
                break;
            default:
                expect = NEVER_FAILS;
                break;
        }
        switch(test_case % 100) {
            case 90:
            case 98:
                // Too many symbols lost from the beginning
                expect = ALMOST_ALWAYS_FAILS;
                for (j = 0; j < 4; j++) {
                    shift_left(transmit);
                }
                break;
            case 91:
            case 99:
                // Too many symbols lost from the end 
                expect = ALMOST_ALWAYS_FAILS;
                for (j = 0; j < 4; j++) {
                    shift_right(transmit);
                }
                break;
            case 92:
                // Too much noise
                expect = ALMOST_ALWAYS_FAILS;
                for (j = 0; j < 6; j++) {
                    transmit[j] ^= 1;
                }
                break;
            case 93:
                // Almost too much noise
                expect = NEVER_FAILS;
                for (j = 0; j < 5; j++) {
                    transmit[j] ^= 1;
                }
                break;
            case 94:
                // Too much noise
                expect = ALMOST_ALWAYS_FAILS;
                for (j = 0; j < 6; j++) {
                    transmit[j * 2] ^= 1;
                }
                break;
            case 95:
                // Too much noise
                expect = ALMOST_ALWAYS_FAILS;
                for (j = 0; j < 6; j++) {
                    transmit[j * 3] ^= 1;
                }
                break;
            case 96:
                // Words lost but back in the right positions
                expect = NEVER_FAILS;
                for (j = 0; j < 5; j++) {
                    shift_left(transmit);
                }
                for (j = 0; j < 5; j++) {
                    shift_right(transmit);
                }
                break;
            case 97:
                // Too many words lost even though back in the right positions
                expect = SOMETIMES_FAILS;
                for (j = 0; j < 6; j++) {
                    shift_left(transmit);
                }
                for (j = 0; j < 6; j++) {
                    shift_right(transmit);
                }
                break;
            default:
                break;
        }


        // Decoded
        ok = ncrs_decode(recovered, transmit);
        switch (expect) {
            case NEVER_FAILS:
                if (ok <= 0) {
                    fprintf(stderr, "decode failed - errors detected - test case %u\n", test_case);
                    exit(1);
                }
                break;
            case ALWAYS_FAILS:
                if (ok > 0) {
                    fprintf(stderr, "decode unexpectedly ok - errors not detected - test case %u\n", test_case);
                    exit(1);
                }
                break;
            case SOMETIMES_FAILS:
            case ALMOST_ALWAYS_FAILS:
                break;
        }

        switch (expect) {
            case ALWAYS_FAILS:
                // Don't need to check the message - it's definitely wrong!
                break;
            case NEVER_FAILS:
                // message should be the same
                if (memcmp(recovered, message, DECODED_DATA_BYTES) != 0) {
                    fprintf(stderr, "decode failed - data different - test case %u - code %u\n",
                                    test_case, ok);
                    for (j = 0; j < NC_DATA_SIZE; j++) {
                        fprintf(stderr, "  sym  %-2d   value %-2d    ", j, transmit[j]);
                        if (j < DECODED_DATA_BYTES) {
                            fprintf(stderr, "byte %-2d   message 0x%02x  recovered 0x%02x\n",
                                        j, message[j], recovered[j]);
                        } else {
                            fprintf(stderr, "\n");
                        }
                    }
                    exit(1);
                }
                break;
            case SOMETIMES_FAILS:
                if (ok <= 0) {
                    // detected the errors, therefore good
                    sometimes_fails_good ++;
                } else if (memcmp(recovered, message, DECODED_DATA_BYTES) == 0) {
                    // recovered the data, therefore good
                    sometimes_fails_good ++;
                } else {
                    // recovered wrong data, so the HMAC step will reject this
                    sometimes_fails_bad ++;
                }
                break;
            case ALMOST_ALWAYS_FAILS:
                if (ok <= 0) {
                    // detected the errors, therefore good
                    almost_always_fails_good ++;
                } else if (memcmp(recovered, message, DECODED_DATA_BYTES) == 0) {
                    // recovered the data, therefore good
                    almost_always_fails_good ++;
                } else {
                    // recovered wrong data, so the HMAC step will reject this
                    almost_always_fails_bad ++;
                }
                break;
        }
    }
    {
        double sometimes_fails_bad_ratio =
                sometimes_fails_bad / (double) (sometimes_fails_bad + sometimes_fails_good);
        double almost_always_fails_bad_ratio =
                almost_always_fails_bad / (double) (almost_always_fails_bad + almost_always_fails_good);
        if (sometimes_fails_bad_ratio > 0.04) {
            fprintf(stderr, "Tests which sometimes fail produced bad data too frequently (%1.3f)\n",
                    sometimes_fails_bad_ratio);
            exit(1);
        }
        if (almost_always_fails_bad_ratio > 0.04) {
            fprintf(stderr, "Tests which almost always fail produced bad data too frequently (%1.3f)\n",
                    almost_always_fails_bad_ratio);
            exit(1);
        }

        printf("ok %u %1.3f %1.3f\n", test_case, sometimes_fails_bad_ratio, almost_always_fails_bad_ratio);
    }
    return 0;
}


