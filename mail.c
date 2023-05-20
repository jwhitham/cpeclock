
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "rx433.h"
#include "hmac433.h"
#include "hal.h"
#include "mail.h"
#include "ncrs.h"

#include "secret.h"

#define CHECK_BYTE_1_VALUE      0xae
#define CHECK_BYTE_2_VALUE      0xc2

#define NVRAM_COUNTER_0_ADDR    0x00
#define NVRAM_COUNTER_1_ADDR    0x08
#define NVRAM_CHECK_BYTE_1_ADDR 0x10
#define NVRAM_STATE_ADDR        0x11
#define NVRAM_CHECK_BYTE_2_ADDR 0x12
#define NVRAM_ALARM_HOUR        0x13
#define NVRAM_ALARM_MINUTE      0x14
#define NVRAM_ALARM_STATE       0x15

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

static void save_alarm_time(uint8_t alarm_hour, uint8_t alarm_minute)
{
    nvram_write(NVRAM_ALARM_HOUR, alarm_hour);
    nvram_write(NVRAM_ALARM_MINUTE, alarm_minute);
}

static void set_alarm_enable(void)
{
    nvram_write(NVRAM_ALARM_STATE, (uint8_t) ALARM_ENABLED);
}

static void clear_alarm_enable(void)
{
    nvram_write(NVRAM_ALARM_STATE, (uint8_t) ALARM_DISABLED);
}

void mail_start_alarm(void)
{
    nvram_write(NVRAM_ALARM_STATE, (uint8_t) ALARM_ACTIVE);
    clear_alarm_enable();
    unset_alarm();
}

void mail_set_alarm_state(alarm_state_t alarm_state)
{
    nvram_write(NVRAM_ALARM_STATE, alarm_state);
}

void mail_set_alarm_state(alarm_state_t alarm_state)
{
    uint8_t alarm_hour = nvram_read(NVRAM_ALARM_HOUR);
    uint8_t alarm_minute = nvram_read(NVRAM_ALARM_MINUTE);
    alarm_state_t old_alarm_state = (alarm_state_t) nvram_read(NVRAM_ALARM_STATE);

    if (force_enable) {
        alarm_state = ALARM_ENABLED;
    }
    if ((alarm_hour >= 24) || (alarm_minute >= 60)) {
        alarm_state = ALARM_DISABLED;
    }

    set_alarm(alarm_hour, alarm_minute, alarm_state);
    mail_set_alarm_state(alarm_state);
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
            save_alarm_time(0, 0);
            clear_alarm_enable();
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
        // load alarm
        mail_reload_alarm(0);
        return 1;
    }
}

static void new_home_easy_message(uint32_t msg)
{
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "HE %08x", (unsigned) msg);
    display_message(tmp);
}

static void new_packet(const uint8_t* payload, int rs_rc)
{
    char tmp[16];

    switch (payload[0]) {
        case 'M':
            // message for the screen
            memcpy(tmp, &payload[1], PACKET_PAYLOAD_SIZE - 1);
            tmp[PACKET_PAYLOAD_SIZE] = '\0';
            display_message(tmp);
            break;
        case 'C':
            // show counter
            snprintf(tmp, sizeof(tmp), "C %04x E %d",
                    (unsigned) (hmac_message_counter & 0xffff),
                    rs_rc);
            display_message(tmp);
            break;
        case 'T':
            // set time
            set_clock(payload[1], payload[2], payload[3]);
            break;
        case 'A':
            // set alarm
            save_alarm_time(payload[1], payload[2]);
            set_alarm_enable();
            set_alarm(payload[1], payload[2]);
            break;
        case 'a':
            // unset alarm
            mail_cancel_alarm();
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
        display_message("RS ERROR");
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
        display_message("HMAC ERROR");
        return;
    }

    // update HMAC counter in NVRAM
    save_counter();

    // process packet payload
    new_packet(packet.payload, rs_rc);
}

