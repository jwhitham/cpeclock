

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rx433.h"
#include "rslib.h"
#include "ncrs.h"
#include "hal.h"

#define NROOTS          (10)    // 10 parity symbols
#define GFPOLY          (0x25)  // Reed Solomon Galois field polynomial
#define FCR             (1)     // First Consecutive Root
#define PRIM            (1)     // Primitive Element
#define PAD             (0)     // No padding

static struct rs_control *rs = NULL;

int ncrs_init(void)
{
    rs = init_rs(SYMBOL_SIZE, GFPOLY, FCR, PRIM, NROOTS);
    if (!rs) {
        display_message("RS INIT ERROR");
        return 0;
    }
    return 1;
}

int ncrs_decode(uint8_t *decoded_message, const uint8_t *encoded_message)
{
    uint8_t data[MSG_SYMBOLS];
    uint16_t parity[NROOTS];
    size_t i, j, k;
    int corrections;

    for (i = 0; i < NC_DATA_SIZE; i++) {
        uint8_t value = encoded_message[i];
        if (i < MSG_SYMBOLS) {
            data[i] = value;
        } else {
            parity[i - MSG_SYMBOLS] = value;
        }
    }
    corrections = decode_rs8(rs, data, parity, MSG_SYMBOLS, NULL, 0, NULL, 0, NULL);
    if (corrections < 0) {
        // too many errors, cannot decode
        return 0;
    }
    memset(decoded_message, 0, DECODED_DATA_BYTES);
    for (i = k = 0; i < MSG_SYMBOLS; i++) {
        for (j = SYMBOL_SIZE; j > 0; j--, k++) {
            decoded_message[k / 8] |= (((data[i] >> (j - 1))) & 1) << (7 - (k % 8));
        }
    }
    return corrections + 1;
}

