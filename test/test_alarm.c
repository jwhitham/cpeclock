
#include <stdio.h>
#include <stdlib.h>

#include "alarm.h"
#include "nvram.h"

static uint8_t test_nvram[256];
static uint8_t now_minute = 0;
static uint8_t now_hour = 0;

uint8_t nvram_read(uint8_t addr)
{
    return test_nvram[addr];
}

void nvram_write(uint8_t addr, uint8_t data)
{
    test_nvram[addr] = data;
}

void display_message(const char* msg)
{
}

static void advance(void)
{
    now_minute++;
    if (now_minute >= 60) {
        now_minute = 0;
        now_hour++;
        if (now_hour >= 24) {
            now_hour = 0;
        }
    }
}

static void run(const char* test, unsigned return_code, unsigned minutes)
{
    while (minutes > 0) {
        if (alarm_update(now_hour, now_minute) == return_code) {
            fprintf(stderr, "error: %s: reached return code %u too soon (%u minutes early)\n",
                            test, return_code, minutes);
            exit(1);
        }
        advance();
        minutes--;
    }
    if (alarm_update(now_hour, now_minute) != return_code) {
        fprintf(stderr, "error: %s: expected code %u\n",
                        test, return_code);
        exit(1);
    }
}

static void no_alarm_all_day(const char* test)
{
    while (now_hour || now_minute) {
        unsigned rc = alarm_update(now_hour, now_minute);
        if (rc != 0) {
            fprintf(stderr, "error: %s: expected code 0 but got %u at %02u:%02u\n",
                            test, rc, now_hour, now_minute);
            exit(1);
        }
        advance();
    }
}

// Called as a result of an incoming message. Alarm is set for some time in the future.
//void alarm_set(uint8_t hour, uint8_t minute);

// Called as a result of an incoming message, or pressing the left button. Alarm is unset.
//void alarm_unset(void);
//
// Called as a result of pressing the right button. Alarm is set for the same time again - but in the future.
//void alarm_reset(void);
//
//// Get the alarm time
//void alarm_get(uint8_t* hour, uint8_t* minute);
//
//// Returns 1 if the alarm should be sounding now.
//int alarm_update(uint8_t now_hour, uint8_t now_minute);
//
//// Called during boot
//void alarm_init(void);


int main(void)
{
    uint8_t h, m, i;
    uint16_t hm;

    alarm_init();

    // test: initially, no alarms are set
    no_alarm_all_day("initial");
    // test: alarm is set at 1am, check for correct setting
    alarm_set(1, 0);
    alarm_get(&h, &m);
    if ((test_nvram[NVRAM_ALARM_HI] != 0)
    || (test_nvram[NVRAM_ALARM_LO] != 60)
    || (test_nvram[NVRAM_ALARM_STATE] == 0)
    || (h != 1) || (m != 0)) {
        fprintf(stderr, "error: 1am: incorrect setting\n");
        exit(1);
    }
    run("1am", 1, 60);                  // trigger after 60 minutes
    run("1am", 0, ALARM_SOUNDS_FOR);    // timeout after 10 minutes
    no_alarm_all_day("1am");            // run back to midnight
    // test: no alarm next day
    no_alarm_all_day("next day");
    // test: reset the alarm, reactivating at the same time
    alarm_reset();
    run("reset", 1, 60);                  // trigger after 60 minutes
    for (i = 2; i <= ALARM_SOUNDS_FOR; i++) {
        run("reset", i, 1); // counter increments each minute
    }
    run("reset", 0, 1);                   // timeout
    no_alarm_all_day("reset");            // run back to midnight
    // test: alarm is set to 23:55
    alarm_set(23, 55);
    alarm_get(&h, &m);
    hm = (23 * 60) + 55;
    if ((test_nvram[NVRAM_ALARM_HI] != (hm >> 8))
    || (test_nvram[NVRAM_ALARM_LO] != (hm & 255))
    || (h != 23) || (m != 55)) {
        fprintf(stderr, "error: 23:55: incorrect setting\n");
        exit(1);
    }
    run("23:55", 1, hm);    // trigger after 23:55
    for (i = 2; i <= ALARM_SOUNDS_FOR; i++) {
        run("23:55", i, 1); // counter increments each minute, including after midnight
    }
    run("23:55", 0, 1);     // timeout
    no_alarm_all_day("23:55");  // run back to midnight - alarm does not retrigger
    // test: alarm set to 00:00
    alarm_set(0, 0);
    run("midnight", 1, 0);    // triggers immediately
    run("midnight", 0, ALARM_SOUNDS_FOR);
    no_alarm_all_day("midnight");


    // 
    printf("ok\n");
    return 0;
}
