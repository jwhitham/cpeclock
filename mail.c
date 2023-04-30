
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "rx433.h"
#include "hmac433.h"
#include "hal.h"
#include "mail.h"

#include "secret.h"

#define CHECK_BYTE_1_VALUE      0xae
#define CHECK_BYTE_2_VALUE      0xc1

#define NVRAM_COUNTER_0_ADDR    0x00
#define NVRAM_COUNTER_1_ADDR    0x08
#define NVRAM_CHECK_BYTE_1_ADDR 0x10
#define NVRAM_STATE_ADDR        0x11
#define NVRAM_CHECK_BYTE_2_ADDR 0x12
#define NVRAM_SIZE              0x13

static uint64_t hmac_message_counter = 0;
static uint16_t previous_counter = 0x100;

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
    const uint8_t* ref = "\x4D\x68\x65\x6C\x6C\x6F\x20\x77\x6F\x72\x6C\x64\x03\x4E\x06\x69\x10\x1E\x7C\x1E";

    // critical section to obtain any new messages
    disable_interrupts();
    if (rx433_count) {
        uint32_t msg_count_words;
        msg_count_bits = rx433_count;
        msg_count_words = (msg_count_bits + 31) / 32;
        for (i = 0; i < msg_count_words; i++) {
            msg_data_words[i] = rx433_data[i];
        }
        rx433_count = 0;
    }
    enable_interrupts();

    if (msg_count_bits == 32) {
        // Home Easy message
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "HE: %08lx", (unsigned long) msg_data_words[0]);
        display_message(tmp);
        return;
    }

    if (msg_count_bits < 72) {
        // minimum length of HMAC message is 72 bits (64 bits for HMAC code and counter)
        return;
    }
    msg_count_bytes = (msg_count_bits + 7) / 8;
    for (i = 0; i < msg_count_bytes; i++) {
        if (msg_data_bytes[i] != ref[i]) {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%d %02x != %02x", i, msg_data_bytes[i], ref[i]);
            display_message(tmp);
            return;
        }
    }
    hmac_message_counter = 1;

    if (!hmac433_authenticate(
                SECRET_DATA, SECRET_SIZE,
                msg_data_bytes, msg_count_bytes,
                &hmac_message_counter)) {
        // message does not authenticate
        if (msg_data_bytes[msg_count_bytes - 8] == previous_counter) {
            // The counter value in the message did not change,
            // so most likely, this is just a repeat of the last message,
            // rather than a deliberate replay attack.
        } else {
            char tmp[16];
            snprintf(tmp, sizeof(tmp), "%08lx", (unsigned long) hmac_message_counter);
            display_message(tmp);
            previous_counter = 0x100;
        }
        return;
    }

    // update counters
    previous_counter = msg_data_bytes[msg_count_bytes - 8];
    save_counter();

    // remove HMAC code and counter
    msg_count_bytes -= 8;

    switch (msg_data_bytes[0]) {
        case 'M':
            // message for the screen
            msg_data_bytes[msg_count_bytes] = '\0';
            display_message((const char*) &msg_data_bytes[1]);
            break;
        default:
            display_message("ACTION ERROR");
            break;
    }
}

