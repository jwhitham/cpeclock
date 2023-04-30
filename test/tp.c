
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "rx433.h"
#include "hmac433.h"
#include "hal.h"
#include "mail.h"

#include "secret.h"

int main(void)
{
    uint32_t msg_data_words[MAX_CODE_LENGTH / 32];
    uint8_t* msg_data_bytes = (uint8_t*) msg_data_words;

    uint64_t hmac_message_counter = 1;
    size_t msg_count_bytes = 20;
    memcpy(msg_data_bytes, "\x4D\x68\x65\x6C\x6C\x6F\x20\x77\x6F\x72\x6C\x64\x03\x4E\x06\x69\x10\x1E\x7C\x1E", 20);

    if (!hmac433_authenticate(
                SECRET_DATA, SECRET_SIZE,
                msg_data_bytes, msg_count_bytes,
                &hmac_message_counter)) {
        printf("error\n");
    } else {
        printf("ok\n");
    }
    return 0;
}
