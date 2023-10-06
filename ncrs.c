

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

#define MAX_SHIFT_DISTANCE (3)

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

int ncrs_decode(uint8_t *original_message, const uint8_t *encoded_message, int8_t *shifted_by)
{
    uint8_t padded_message[(MAX_SHIFT_DISTANCE * 2) + NC_DATA_SIZE];
    uint8_t data[MSG_SYMBOLS];
    uint16_t parity[NROOTS];
    size_t i, j, k, shift_attempt;
    int corrections = -1;

    // pad message with zeroes (for shift attempts)
    memset(padded_message, 0, sizeof(padded_message));
    memcpy(&padded_message[MAX_SHIFT_DISTANCE], encoded_message, NC_DATA_SIZE);

    for (shift_attempt = 0; shift_attempt <= (MAX_SHIFT_DISTANCE * 2); shift_attempt++) {
        // Determine how far to shift
        if (shift_attempt % 2) {
            i = MAX_SHIFT_DISTANCE + (shift_attempt / 2); // Shifting left (losing symbols from beginning)
        } else {
            i = MAX_SHIFT_DISTANCE - (shift_attempt / 2); // Shifting right (losing symbols from end)
        }
        *shifted_by = i;

        // Copy interleaved data and parity, applying shift
        for (j = 0; j < (NROOTS * 2); i += 3, j += 2) {
            data[j + 0] = padded_message[i + 0];
            data[j + 1] = padded_message[i + 1];
            parity[j / 2] = padded_message[i + 2];
        }
        data[j] = padded_message[i];   // last data symbol

        // Attempt decoding
        corrections = decode_rs8(rs, data, parity, MSG_SYMBOLS, NULL, 0, NULL, 0, NULL);
        if (corrections >= 0) {
            // Successfully decoded
            break;
        }
    }
    if (corrections < 0) {
        // Unable to decode
        return 0;
    }

    // unpack bits from symbols
    memset(original_message, 0, DECODED_DATA_BYTES);
    for (i = k = 0; i < MSG_SYMBOLS; i++) {
        for (j = SYMBOL_SIZE; j > 0; j--, k++) {
            original_message[k / 8] |= (((data[i] >> (j - 1))) & 1) << (7 - (k % 8));
        }
    }
    // positive return code means success - but shows what was done to recover:
    // (1 == perfect transmission)
    return 1 + shift_attempt + (corrections * 10);
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

