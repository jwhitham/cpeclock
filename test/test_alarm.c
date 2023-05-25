
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alarm.h"
#include "nvram.h"

#define X (~((unsigned)0))
#define FINAL_VALID_STATE 3

static uint8_t test_nvram[256];
static uint8_t states_covered[256];
static uint8_t now_minute = 0;
static uint8_t now_hour = 0;

uint8_t nvram_read(uint8_t addr)
{
    if ((addr != NVRAM_ALARM_HI) && (addr != NVRAM_ALARM_LO)
    && (addr != NVRAM_ALARM_STATE)) {
        fprintf(stderr, "error: read from unexpected address 0x%x\n", addr);
        exit(1);
    }
    return test_nvram[addr];
}

void nvram_write(uint8_t addr, uint8_t data)
{
    if ((addr != NVRAM_ALARM_HI) && (addr != NVRAM_ALARM_LO)
    && (addr != NVRAM_ALARM_STATE)) {
        fprintf(stderr, "error: write to unexpected address 0x%x\n", addr);
        exit(1);
    }
    if (addr == NVRAM_ALARM_STATE) {
        states_covered[data] = 1;
        if (data > FINAL_VALID_STATE) {
            fprintf(stderr, "error: write of weird value to state: 0x%x\n", data);
            exit(1);
        }
    }
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

static void run(const char* test,
                unsigned expected_return_code_before_transition,
                unsigned expected_return_code_after_transition,
                unsigned minutes_to_transition)
{
    unsigned rc;

    while (minutes_to_transition > 0) {
        rc = alarm_update(now_hour, now_minute);
        if ((rc != expected_return_code_before_transition)
        && (X != expected_return_code_before_transition)) {
            fprintf(stderr, "error: %s: expected return code %u but got %u at %02u:%02u "
                            "(%u minutes before transition)\n",
                            test, expected_return_code_before_transition, rc,
                            now_hour, now_minute, minutes_to_transition);
            exit(1);
        }
        advance();
        minutes_to_transition--;
    }
    rc = alarm_update(now_hour, now_minute);
    if ((rc != expected_return_code_after_transition)
    && (X != expected_return_code_after_transition)) {
        fprintf(stderr, "error: %s: expected return code %u but got %u at %02u:%02u\n",
                        test, expected_return_code_after_transition, rc,
                        now_hour, now_minute);
        exit(1);
    }
}

static void no_alarm_all_day(const char* test)
{
    do {
        unsigned rc = alarm_update(now_hour, now_minute);
        if (rc != 0) {
            fprintf(stderr, "error: %s: expected code 0 but got %u at %02u:%02u\n",
                            test, rc, now_hour, now_minute);
            exit(1);
        }
        advance();
    } while (now_hour || now_minute);
}

static void power_off_time_skip(uint8_t hour, uint8_t minute)
{
    alarm_init();
    now_hour = hour;
    now_minute = minute;
}


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
    uint8_t h, m;
    uint16_t hm, i;

    alarm_init();

    // test: initially, no alarms are set
    no_alarm_all_day("initial");
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
    run("1am", 0, 1, 60);                  // trigger after 60 minutes
    run("1am", X, 0, ALARM_SOUNDS_FOR);    // timeout after 10 minutes
    no_alarm_all_day("1am");            // run back to midnight
    // test: no alarm next day
    no_alarm_all_day("next day");
    // test: reset the alarm, reactivating at the same time
    alarm_reset();
    run("reset", 0, 1, 60);                  // trigger after 60 minutes
    for (i = 2; i <= ALARM_SOUNDS_FOR; i++) {
        run("reset", i - 1, i, 1); // counter increments each minute
    }
    run("reset", ALARM_SOUNDS_FOR, 0, 1);                   // timeout
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
    // subtest: setting the alarm to 23:55 at 00:00 does not cause an immediate trigger
    // subtest: trigger at 23:55
    run("23:55 tri", 0, 1, hm);
    // subtest: counter increments each minute, including after midnight
    for (i = 2; i <= ALARM_SOUNDS_FOR; i++) {
        run("23:55 cou", i - 1, i, 1);
    }
    run("23:55 tim", ALARM_SOUNDS_FOR, 0, 1);     // timeout
    no_alarm_all_day("23:55 run");  // run back to midnight - alarm does not retrigger

    // test: alarm set to 00:00 (current time) - no trigger until the next midnight
    alarm_set(0, 0);
    run("midnight", 0, 1, 24 * 60); // triggered after 24 hours
    run("midnight", X, 0, ALARM_SOUNDS_FOR); // timeout
    no_alarm_all_day("midnight");

    // test: alarm set to 00:01 (current time + 1) 
    alarm_set(0, 1);
    run("00:01", 0, 1, 1); // trigger after 1 minute
    run("00:01", X, 0, ALARM_SOUNDS_FOR); // timeout
    no_alarm_all_day("midnight+1");

    // power interrupt before alarm; power restored before alarm should be sounding
    // test: alarm set at 09:00 but power is interrupted from 08:00 .. 08:05
    alarm_set(9, 0);
    run("08:05", 0, 0, 8 * 60); // run to 08:00
    power_off_time_skip(8, 5); // power back on at 08:05
    run("09:00", 0, 1, 55); // alarm starts
    run("00:01", X, 0, ALARM_SOUNDS_FOR); // timeout
    no_alarm_all_day("08:05");

    // power interrupt before alarm; power restored while alarm should be sounding
    // test: alarm set at 09:00 but power is interrupted from 08:55 .. 09:05
    alarm_set(9, 0);
    run("09:05", 0, 0, (9 * 60) - 5); // run to 08:55
    power_off_time_skip(9, 5); // power back on at 09:05
    for (i = 6; i <= ALARM_SOUNDS_FOR; i++) {
        run("09:05", X, i, 0); // triggered immediately (begin at 6th minute)
        advance();
    }
    run("09:05", X, 0, 0); // timeout
    no_alarm_all_day("09:05");

    // power interrupt across the whole alarm time
    // test: alarm set at 08:00 but power is interrupted from 07:59 .. 09:00
    alarm_set(8, 0);
    run("08:00", 0, 0, (8 * 60) - 1); // run to 07:59
    power_off_time_skip(9, 0); // power back on at 09:00
    no_alarm_all_day("08:00"); // the alarm never triggers today
    run("08:00", 0, 1, 8 * 60); // run to 08:00 again - alarm triggers
    run("08:00", X, 0, ALARM_SOUNDS_FOR); // timeout
    no_alarm_all_day("08:00");

    // power interrupt during alarm time
    // test: alarm set at 07:00 but power is interrupted from 07:01 .. 07:03
    alarm_set(7, 0);
    run("07:00", 0, 1, 7 * 60); // run to 07:00
    power_off_time_skip(7, 3); // power back on at 07:03
    for (i = 4; i <= ALARM_SOUNDS_FOR; i++) {
        run("07:00", X, i, 0); // 4th minute
        advance();
    }
    no_alarm_all_day("07:00");
    no_alarm_all_day("07:00");

    // power interrupt during alarm time - lasts for a long time
    // test: alarm set at 06:00 but power is interrupted from 06:02 .. 07:00
    alarm_set(6, 0);
    run("06:00", 0, 1, 6 * 60); // run to 06:00
    run("06:00", X, X, 2);      // run to 06:02
    power_off_time_skip(7, 0);  // power back on at 07:00
    no_alarm_all_day("06:00");  // no alarm - already timed out
    no_alarm_all_day("06:00");

    // test: alarm set again while it is sounding
    alarm_set(1, 0);
    run("setset", 0, 1, 60); // run to 01:00
    alarm_set(2, 0);         // alarm stops
    run("setset", 0, 1, 60); // run to 02:00 - alarm starts again
    alarm_set(3, 0);         // alarm stops
    power_off_time_skip(2, 30); // power goes off for a while
    run("setset", 0, 1, 30);  // alarm sounds again at 3am
    alarm_set(4, 0);         // alarm stops
    power_off_time_skip(3, 59); // power goes off for even longer
    run("setset", 0, 1, 1);  // trigger at 4am
    run("setset", X, 0, ALARM_SOUNDS_FOR);
    no_alarm_all_day("setset");
    no_alarm_all_day("setset");

    // test: corner case - if the alarm is set, and then power immediately goes out until
    // the beginning of the alarm time, the alarm does not trigger. In the real application
    // we would do alarm_update soon after alarm_set so this would not happen.
    alarm_set(4, 0);
    power_off_time_skip(4, 0);
    no_alarm_all_day("corner"); // alarm does not sound at 4am
    power_off_time_skip(4, 0);
    run("corner", 0, 1, 0);  // trigger at 4am the next day (as there has been an alarm_update)
    run("corner", X, 0, ALARM_SOUNDS_FOR);
    no_alarm_all_day("corner");

    // test: alarm is cancelled with the left button after it sounds (immediate cancel)
    alarm_set(10, 0);
    run("10:00i", 0, 1, 10 * 60); // run to 10:00
    alarm_unset();
    no_alarm_all_day("10:00i");
    no_alarm_all_day("10:00i"); // no alarm the next day either

    // test: alarm is cancelled with the left button after it sounds (after 1 minute)
    alarm_set(10, 0);
    run("10:00", 0, 1, 10 * 60); // run to 10:00
    run("10:00", 1, 2, 1); // run to 10:01
    alarm_unset();
    no_alarm_all_day("10:00");
    no_alarm_all_day("10:00");

    // test: alarm is cancelled with the left button before it sounds
    alarm_set(10, 0);
    run("10:00x", 0, 0, 9 * 60); // run to 09:00
    alarm_unset();
    no_alarm_all_day("10:00x");
    no_alarm_all_day("10:00x");

    // test: alarm is cancelled with the left button before it sounds and also the power is cycled
    alarm_set(10, 0);
    run("10:00y", 0, 0, 9 * 60); // run to 09:00
    alarm_unset();
    run("10:00y", 0, 0, 30); // run to 09:30
    power_off_time_skip(9, 35); // power back on at 09:35
    no_alarm_all_day("10:00y");
    no_alarm_all_day("10:00y");

    // test: reactivate the alarm at the previous time
    run("react", 0, 0, 60); // run to 01:00
    alarm_reset();
    run("react", 0, 1, 9 * 60); // run to 10:00, alarm sounds
    alarm_reset();              // alarm stops (is now reset)
    no_alarm_all_day("react");
    run("react", 0, 1, 10 * 60); // run to 10:00, alarm sounds again
    alarm_unset();              // alarm stops (is now unset)
    no_alarm_all_day("react");
    no_alarm_all_day("react");  // no more alarm

    // test: alarm is set then cancelled
    alarm_set(14, 0);
    run("setca", 0, 0, 1);
    alarm_unset();
    no_alarm_all_day("setca");  // does not retrigger

    // test: alarm is set, cancelled, then reset all at once
    alarm_set(14, 0);
    alarm_unset();
    alarm_reset();
    run("14:00", 0, 1, 14 * 60);
    run("14:00", X, 0, ALARM_SOUNDS_FOR);
    no_alarm_all_day("14:00");  // run back to midnight
    no_alarm_all_day("14:00");  // does not retrigger
    alarm_reset();
    run("14:00", 0, 1, 14 * 60);
    alarm_reset();
    no_alarm_all_day("14:00");
    run("14:00", 0, 1, 14 * 60);
    alarm_unset();
    no_alarm_all_day("14:00");
    no_alarm_all_day("14:00");

    // test: reset, power off, alarm
    alarm_set(14, 1);
    for (i = 0; i < 14; i++) {
        switch (i % 4) {
            case 0:
                alarm_unset();
                run("rp", 0, 0, 60);
                break;
            case 1:
                alarm_reset();
                run("rp", 0, 0, 60);
                break;
            default:
                power_off_time_skip(now_hour, 5);
                run("rp", 0, 0, 55);
                break;
        }
    }
    run("rp", 0, 1, 1);
    run("rp", X, 0, ALARM_SOUNDS_FOR);
    no_alarm_all_day("rp");
    no_alarm_all_day("rp");

    alarm_get(&h, &m);
    hm = (h * 60) + m;
    if ((test_nvram[NVRAM_ALARM_HI] != (hm >> 8))
    || (test_nvram[NVRAM_ALARM_LO] != (hm & 255))
    || (h != 14) || (m != 1)) {
        fprintf(stderr, "error: 14:01: incorrect setting\n");
        exit(1);
    }

    // test: press right button in initial state, with all memory == 0
    // alarm triggers at 00:00
    memset(test_nvram, 0, sizeof(test_nvram));
    power_off_time_skip(12, 0);
    alarm_reset();
    no_alarm_all_day("blank");
    run("blank", 0, 1, 0);
    run("blank", X, 0, ALARM_SOUNDS_FOR);
    no_alarm_all_day("blank");

    // test: press right button in weird state, with all memory == 0xff
    // alarm triggers at 00:00
    memset(test_nvram, 0xff, sizeof(test_nvram));
    power_off_time_skip(12, 0);
    alarm_reset();
    no_alarm_all_day("ff");
    run("ff", 0, 1, 0);
    run("ff", X, 0, ALARM_SOUNDS_FOR);
    no_alarm_all_day("ff");

    for (i = 0; i < FINAL_VALID_STATE; i++) {
        if (!states_covered[i]) {
            fprintf(stderr, "error: no coverage of state %d\n", i);
            exit(1);
        }
    }

    printf("ok\n");
    return 0;
}
