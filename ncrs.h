#ifndef NCRS_H
#define NCRS_H

#include <stdint.h>
#include "rx433.h"


#ifdef __cplusplus
extern "C" {
#endif

#define MSG_SYMBOLS         (21)    // 21 message symbols

#define DECODED_DATA_BITS   (MSG_SYMBOLS * SYMBOL_SIZE)
#define DECODED_DATA_BYTES  ((DECODED_DATA_BITS + 7) / 8)

int ncrs_init(void);
int ncrs_decode(uint8_t *original_message, const uint8_t *encoded_message);
void ncrs_encode(uint8_t *encoded_message, const uint8_t *original_message);

#ifdef __cplusplus
}
#endif

#endif

