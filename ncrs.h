#ifndef NCRS_H
#define NCRS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "rx433.h"


#define MSG_SYMBOLS         (21)    // 21 message symbols

#define DECODED_DATA_BITS   (MSG_SYMBOLS * SYMBOL_SIZE)
#define DECODED_DATA_BYTES  ((DECODED_DATA_BITS + 7) / 8)

int ncrs_init(void);
int ncrs_decode(uint8_t *decoded_message, const uint8_t *encoded_message);

#ifdef __cplusplus
}
#endif

#endif

