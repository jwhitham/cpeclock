
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "rx433.h"
#include "hmac433.h"
#include "hal.h"
#include "mail.h"

#include "secret.h"

#define CHECK_BYTE_1_VALUE      0xae
#define CHECK_BYTE_2_VALUE      0xc2

#define NVRAM_COUNTER_0_ADDR    0x00
#define NVRAM_COUNTER_1_ADDR    0x08
#define NVRAM_CHECK_BYTE_1_ADDR 0x10
#define NVRAM_STATE_ADDR        0x11
#define NVRAM_CHECK_BYTE_2_ADDR 0x12
#define NVRAM_SIZE              0x13

static uint64_t hmac_message_counter = 0;
static uint32_t previous_msg[MAX_CODE_LENGTH / 32] = {0};

static void save_counter(void)
{
    // Which counter is currently valid? Save in the other one
    uint8_t new_state = nvram_read(NVRAM_STATE_ADDR) ^ 1;
    uint8_t new_counter_addr = (new_state & 1) ? NVRAM_COUNTER_1_ADDR : NVRAM_COUNTER_0_ADDR;
    size_t i;
    for (i = 0; i < 8; i++) {
        nvram_write(i + new_counter_addr, ((uint8_t*) &hmac_message_counter)[i]);
    }
    // The newly-written counter is now the valid one
    nvram_write(NVRAM_STATE_ADDR, new_state);
}

int mail_init(void)
{
    uint8_t state = nvram_read(NVRAM_STATE_ADDR);
    if ((nvram_read(NVRAM_CHECK_BYTE_1_ADDR) != CHECK_BYTE_1_VALUE)
    || (nvram_read(NVRAM_CHECK_BYTE_2_ADDR) != CHECK_BYTE_2_VALUE)
    || (state > 1)) {
        // nvram is garbage - reformat
        nvram_write(NVRAM_CHECK_BYTE_1_ADDR, CHECK_BYTE_1_VALUE);
        nvram_write(NVRAM_CHECK_BYTE_2_ADDR, CHECK_BYTE_2_VALUE);
        nvram_write(NVRAM_STATE_ADDR, 0);
        // check this worked
        if ((nvram_read(NVRAM_CHECK_BYTE_1_ADDR) == CHECK_BYTE_1_VALUE)
        && (nvram_read(NVRAM_CHECK_BYTE_2_ADDR) == CHECK_BYTE_2_VALUE)) {
            hmac_message_counter = 1;
            save_counter();
            display_message("NVRAM INIT");
            return 1;
        } else {
            display_message("NVRAM FAIL");
            return 0;
        }
    } else {
        // load counter
        uint8_t counter_addr = (state & 1) ? NVRAM_COUNTER_1_ADDR : NVRAM_COUNTER_0_ADDR;
        size_t i;
        for (i = 0; i < 8; i++) {
            ((uint8_t*) &hmac_message_counter)[i] = nvram_read(i + counter_addr);
        }
        return 1;
    }
}

void mail_receive_messages(void)
{
    uint32_t msg_data_words[MAX_CODE_LENGTH / 32];
    uint8_t* msg_data_bytes = (uint8_t*) msg_data_words;
    size_t msg_count_bits = 0;
    size_t msg_count_bytes = 0;
    size_t i;
    char tmp[16];
    uint8_t new_message = 0;

    // critical section to obtain any new messages
    disable_interrupts();
    if (rx433_count) {
        msg_count_bits = rx433_count;
        for (i = 0; i < (MAX_CODE_LENGTH / 32); i++) {
            msg_data_words[i] = rx433_data[i];
        }
        rx433_count = 0;
    }
    enable_interrupts();

    if (msg_count_bits == 32) {
        // Home Easy message
        snprintf(tmp, sizeof(tmp), "HE: %02x%02x%02x%02x",
                    msg_data_bytes[0], msg_data_bytes[1],
                    msg_data_bytes[2], msg_data_bytes[3]);
        display_message(tmp);
        return;
    }

    if (msg_count_bits < 72) {
        // minimum length of HMAC message is 72 bits
        // (64 bits for HMAC code and counter, 8 bits minimum payload)
        return;
    }
    // Don't reprocess the same message
    for (i = 0; i < (MAX_CODE_LENGTH / 32); i++) {
        if (previous_msg[i] != msg_data_words[i]) {
            previous_msg[i] = msg_data_words[i];
            new_message = 1;
        }
    }
    if (!new_message) {
        return;
    }

    // Authenticate message
    msg_count_bytes = (msg_count_bits + 7) / 8;
    if (!hmac433_authenticate(
                SECRET_DATA, SECRET_SIZE,
                msg_data_bytes, msg_count_bytes,
                &hmac_message_counter)) {
        // message does not authenticate
        display_message("HMAC ERROR");
        return;
    }

    // update counters
    save_counter();

    // remove HMAC code and counter
    msg_count_bytes -= 8;

    switch (msg_data_bytes[0]) {
        case 'M':
            // message for the screen
            msg_data_bytes[msg_count_bytes] = '\0';
            display_message((const char*) &msg_data_bytes[1]);
            break;
        case 'C':
            // show counter
            snprintf(tmp, sizeof(tmp), "C: %08x",
                    (uint32_t) hmac_message_counter);
            display_message(tmp);
            break;
        default:
            display_message("ACTION ERROR");
            break;
    }
}

