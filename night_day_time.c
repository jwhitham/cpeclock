
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "hal.h"
#include "nvram.h"
#include "night_day_time.h"
#include "alarm.h"

static uint16_t start_night_time = 0; // In minutes. Midnight = 0, Midday = 720, 11pm = 1380
static uint16_t start_day_time = 0;



static void save_to_nvram()
{
    nvram_write(NVRAM_NIGHT_TIME_HI, start_night_time >> 8);
    nvram_write(NVRAM_NIGHT_TIME_LO, start_night_time);
    nvram_write(NVRAM_DAY_TIME_HI, start_day_time >> 8);
    nvram_write(NVRAM_DAY_TIME_LO, start_day_time);
}

// Called as a result of an incoming message.
void night_day_time_set(uint8_t start_night_hour, uint8_t start_night_minute,
                        uint8_t start_day_hour, uint8_t start_day_minute)
{
    char tmp[16];
    start_night_time = (start_night_hour * 60) + start_night_minute;
    start_day_time = (start_day_hour * 60) + start_day_minute;
    snprintf(tmp, sizeof(tmp), "%02d:%02d..%02d:%02d",
            start_night_hour, start_night_minute,
            start_day_hour, start_day_minute);
    display_message(tmp);
    save_to_nvram();
}

// Returns 1 if it is night time
int night_day_time_test(uint8_t now_hour, uint8_t now_minute)
{
    uint16_t now_time = (now_hour * 60) + now_minute;

    if (start_day_time == start_night_time) {
        // permanent night time
        return 1;
    } else if (start_day_time < start_night_time) {
        // e.g. day at 7am night at 11pm
        return (now_time < start_day_time) || (now_time >= start_night_time);
    } else {
        // e.g. day at 9am night at 1am
        return (now_time >= start_night_time) && (now_time < start_day_time);
    }
}

// Called during boot
void night_day_time_init(void)
{
    start_night_time =
        ((uint16_t) nvram_read(NVRAM_NIGHT_TIME_HI) << 8) | (uint16_t) nvram_read(NVRAM_NIGHT_TIME_LO);
    start_day_time =
        ((uint16_t) nvram_read(NVRAM_DAY_TIME_HI) << 8) | (uint16_t) nvram_read(NVRAM_DAY_TIME_LO);

    if (start_night_time >= WHOLE_DAY) {
        start_night_time = 0;
    }
    if (start_day_time >= WHOLE_DAY) {
        start_day_time = 0;
    }
    save_to_nvram();
}

