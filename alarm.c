
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "hal.h"
#include "nvram.h"
#include "alarm.h"

typedef enum {
    ALARM_DISABLED = 0,
    ALARM_ENABLED,
    ALARM_ACTIVE,
    ALARM_RESET,
    ALARM_INVALID_STATE,
} alarm_state_t;

static uint16_t alarm_time = 0; // In minutes. Midnight = 0, Midday = 720, 11pm = 1380
static alarm_state_t alarm_state = ALARM_DISABLED;



static void save_to_nvram()
{
    nvram_write(NVRAM_ALARM_HI, alarm_time >> 8);
    nvram_write(NVRAM_ALARM_LO, alarm_time);
    nvram_write(NVRAM_ALARM_STATE, (uint8_t) alarm_state);
}

// Called as a result of an incoming message. Alarm is set for some time in the future.
int alarm_set(uint8_t hour, uint8_t minute)
{
    char tmp[16];
    alarm_state_t old_state = alarm_state;
    alarm_time = (hour * 60) + minute;
    alarm_state = ALARM_RESET;
    snprintf(tmp, sizeof(tmp), "ALARM %02d:%02d", hour, minute);
    display_message(tmp);
    save_to_nvram();
    return old_state != alarm_state;
}

// Called as a result of an incoming message, or pressing the left button. Alarm is unset.
int alarm_unset(void)
{
    alarm_state_t old_state = alarm_state;
    display_message("ALARM OFF");
    alarm_state = ALARM_DISABLED;
    save_to_nvram();
    return old_state != alarm_state;
}

// Called as a result of pressing the right button. Alarm is set for the same time again - but in the future.
int alarm_reset(void)
{
    uint8_t hour, minute;
    alarm_get(&hour, &minute);
    return alarm_set(hour, minute);
}

// Get the alarm time
void alarm_get(uint8_t* hour, uint8_t* minute)
{
    *hour = alarm_time / 60;
    *minute = alarm_time % 60;
}

// Returns > 0 if the alarm should be sounding now.
// The return value is the number of minutes since activation, rounded up.
unsigned alarm_update(uint8_t now_hour, uint8_t now_minute)
{
    uint16_t now_time = (now_hour * 60) + now_minute;
    uint16_t test_time = alarm_time;
    unsigned i, in_active_region = 0;

    for (i = 1; i <= ALARM_SOUNDS_FOR; i++) {
        if (test_time >= WHOLE_DAY) {
            test_time = 0;
        }
        if (test_time == now_time) {
            in_active_region = i;
            break;
        }
        test_time++;
    }

    switch(alarm_state) {
        case ALARM_ACTIVE:
            // alarm already sounding
            if (!in_active_region) {
                // stop - timeout
                alarm_state = ALARM_DISABLED;
                save_to_nvram();
                return 0;
            } else {
                return in_active_region;
            }
        case ALARM_ENABLED:
            if (in_active_region) {
                // alarm begins to sound
                alarm_state = ALARM_ACTIVE;
                save_to_nvram();
                return in_active_region;
            } else {
                return 0;
            }
        case ALARM_RESET:
            // alarm has been set or reset
            // It won't actually be enabled until after it has left the active region.
            // Otherwise it would retrigger immediately.
            if (!in_active_region) {
                alarm_state = ALARM_ENABLED;
                save_to_nvram();
            }
            return 0;
        case ALARM_DISABLED:
            return 0;
        default:
            return 0;
    }
}

// Called during boot
void alarm_init(void)
{
    uint8_t hi = nvram_read(NVRAM_ALARM_HI);
    uint8_t lo = nvram_read(NVRAM_ALARM_LO);
    alarm_state = (alarm_state_t) nvram_read(NVRAM_ALARM_STATE);

    alarm_time = ((uint16_t) hi << 8) | (uint16_t) lo;
    if (alarm_time >= WHOLE_DAY) {
        alarm_time = 0;
    }
    if ((uint8_t) alarm_state >= (uint8_t) ALARM_INVALID_STATE) {
        alarm_state = ALARM_DISABLED;
    }
    save_to_nvram();
}

