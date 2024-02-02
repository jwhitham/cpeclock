
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "rx433.h"
#include "hmac433.h"
#include "hal.h"
#include "mail.h"
#include "ncrs.h"
#include "nvram.h"
#include "alarm.h"
#include "night_day_time.h"

#include "secret.h"

static uint64_t hmac_message_counter = 0;
static hmac433_packet_t previous_packet;

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
    uint8_t state;

    if (DECODED_DATA_BYTES != sizeof(hmac433_packet_t)) {
        // DECODED_DATA_BYTES must be equal to sizeof(hmac433_packet_t)
        display_message("SIZE ERROR");
        return 0;
    }

    state = nvram_read(NVRAM_STATE_ADDR);
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

static void new_home_easy_message(uint32_t msg)
{
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "HE %08x", (unsigned) msg);
    display_message_lp(tmp);
}


static void show_message(const uint8_t* payload)
{
    char tmp[16];
    memcpy(tmp, &payload[1], PACKET_PAYLOAD_SIZE - 1);
    tmp[PACKET_PAYLOAD_SIZE - 1] = '\0';
    display_message(tmp);
}

static void show_counter(uint64_t counter_copy, int rs_rc)
{
    size_t i;
    uint8_t tmp[14];

    // big-endian base64 encoding, e.g. 0x1234 == "AAAAAAAAEjQ="
    for (i = 0; i < 11; i++) {
        uint8_t v = (uint8_t) (counter_copy >> (uint64_t) 58);
        if (v < 26) {
            v += 'A';
        } else if (v < 52) {
            v += 'a' - 26;
        } else if (v < 62) {
            v += '0' - 52;
        } else if (v == 62) {
            v = '+';
        } else {
            v = '/';
        }
        tmp[i] = v;
        counter_copy = counter_copy << (uint64_t) 6;
    }
    tmp[11] = '=';
    tmp[12] = (rs_rc & 0xf) + '0';
    tmp[13] = '\0';
    display_message(tmp);
}

static void new_packet(const uint8_t* payload, int rs_rc)
{
    switch (payload[0]) {
        case 'M':
            // message for the screen
            show_message(payload);
            break;
        case 'C':
            // show counter
            show_counter(hmac_message_counter, rs_rc);
            break;
        case 'T':
            // set time
            clock_set(payload[1], payload[2], payload[3]);
            break;
        case 'A':
            // set alarm
            alarm_set(payload[1], payload[2]);
            break;
        case 'N':
            // set night and day time 
            night_day_time_set(payload[1], payload[2], payload[3], payload[4]);
            break;
        case 'a':
            // unset alarm, and cancel if it's active
            // same as pressing the left button
            alarm_unset();
            break;
        default:
            display_message("ACTION ERROR");
            break;
    }
}

void mail_receive_messages(void)
{
    uint32_t    copy_home_easy = 0;
    uint8_t     copy_new_code[NC_DATA_SIZE];
    uint8_t     copy_new_code_ready = 0;
    hmac433_packet_t packet;
    int         rs_rc;

    // critical section to obtain any new messages
    disable_interrupts();
    if (rx433_home_easy) {
        copy_home_easy = rx433_home_easy;
        rx433_home_easy = 0;
    }
    if (rx433_new_code_ready) {
        memcpy(copy_new_code, (const uint8_t*) rx433_new_code, NC_DATA_SIZE);
        rx433_new_code_ready = 0;
        copy_new_code_ready = 1;
    }
    enable_interrupts();

    if (copy_home_easy) {
        // Process Home Easy message
        new_home_easy_message(copy_home_easy);
    }

    if (!copy_new_code_ready) {
        // No messages
        return;
    }

    // Process new code
    // Reed Solomon decoding
    rs_rc = ncrs_decode((uint8_t*) &packet, copy_new_code);
    if (rs_rc <= 0) {
        // display_message("RS ERROR");
        return;
    }

    // Same as last code? Quickly reject a rebroadcast
    if (memcmp(&previous_packet, &packet, sizeof(packet)) == 0) {
        return;
    }
    memcpy(&previous_packet, &packet, sizeof(packet));

    // HMAC authentication
    if (!hmac433_authenticate(
                SECRET_DATA, SECRET_SIZE,
                &packet, &hmac_message_counter)) {
        display_message_lp("HMAC ERROR");
        return;
    }

    // update HMAC counter in NVRAM
    save_counter();

    if (packet.counter_resync_flag) {
        // There is no payload - we just update the counter
        display_message_lp("COUNT RESYNC");
    } else {
        // process packet payload
        new_packet(packet.payload, rs_rc);
    }
}

