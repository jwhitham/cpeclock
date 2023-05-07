
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ncrs.h"

void display_message(const char* m)
{
    printf("message: %s\n", m);
    exit(1);
}

static void add_noise(uint8_t* transmit)
{
    size_t i = rand() % (NC_DATA_SIZE * SYMBOL_SIZE);
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


int main(void)
{
    unsigned test_case, j;
    int ok;
    uint8_t message[DECODED_DATA_BYTES];
    uint8_t transmit[NC_DATA_SIZE];
    uint8_t recovered[DECODED_DATA_BYTES];

    if (!ncrs_init()) {
        printf("rs = null\n");
        return 1;
    }
    srand(222);

    for (test_case = 0; test_case < 100; test_case++) {
        // 105 random bits:
        for (j = 0; j < DECODED_DATA_BYTES; j++) {
            message[j] = rand() & 0xff;
        }
        message[DECODED_DATA_BYTES - 1] &= 0x80;

        // Encoded
        ncrs_encode(transmit, message);

        // Possibly messed with
        switch (test_case / 10) {
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
                add_noise(transmit);
                break;
            case 7:
                add_noise(transmit);
                add_noise(transmit);
                break;
            case 8:
                add_noise(transmit);
                add_noise(transmit);
                shift_left(transmit);
                shift_left(transmit);
                shift_left(transmit);
                break;
            default:
                break;
        }

        // Decoded
        ok = ncrs_decode(recovered, transmit);
        if (ok <= 0) {
            fprintf(stderr, "decode failed - errors detected - test case %u\n", test_case);
            exit(1);
        }

        // message should be the same
        if (memcmp(recovered, message, DECODED_DATA_BYTES) != 0) {
            fprintf(stderr, "decode failed - data different - test case %u\n", test_case);
            exit(1);
        }
    }
    return 0;
}


