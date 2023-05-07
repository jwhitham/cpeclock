

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

int ncrs_decode(uint8_t *original_message, const uint8_t *encoded_message)
{
    uint8_t data[MSG_SYMBOLS];
    uint16_t parity[NROOTS];
    size_t i, j, k;
    int corrections;

    // Copy interleaved data and earity
    for (i = j = 0; j < (NROOTS * 2); i += 3, j += 2) {
        data[j + 0] = encoded_message[i + 0];
        data[j + 1] = encoded_message[i + 1];
        parity[j / 2] = encoded_message[i + 2];
    }
    data[j] = encoded_message[i];   // last data symbol

    corrections = decode_rs8(rs, data, parity, MSG_SYMBOLS, NULL, 0, NULL, 0, NULL);
    if (corrections < 0) {
        // too many errors, cannot decode
        return 0;
    }

    // unpack bits from symbols
    memset(original_message, 0, DECODED_DATA_BYTES);
    for (i = k = 0; i < MSG_SYMBOLS; i++) {
        for (j = SYMBOL_SIZE; j > 0; j--, k++) {
            original_message[k / 8] |= (((data[i] >> (j - 1))) & 1) << (7 - (k % 8));
        }
    }
    return corrections + 1;
}

void ncrs_encode(uint8_t *encoded_message, const uint8_t *original_message)
{
    uint8_t data[MSG_SYMBOLS];
    uint16_t parity[NROOTS];
    size_t i, j, k;

    // Pack bits into symbols
    memset(data, 0, sizeof(data));
    for (i = k = 0; i < MSG_SYMBOLS; i++) {
        for (j = SYMBOL_SIZE; j > 0; j--, k++) {
            data[i] |= ((uint8_t) (original_message[k / 8] << (k % 8)) >> 7) << (j - 1);
        }
    }
    
    // Encode
    memset(parity, 0, sizeof(parity));
    encode_rs8(rs, data, MSG_SYMBOLS, parity, 0);

    // Copy interleaved data and parity
    for (i = j = 0; j < (NROOTS * 2); i += 3, j += 2) {
        encoded_message[i + 0] = data[j + 0];
        encoded_message[i + 1] = data[j + 1];
        encoded_message[i + 2] = parity[j / 2];
    }
    encoded_message[i] = data[j]; // last data symbol
}

