
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>


#include "libnc.h"
#include "hmac433.h"
#include "rx433.h"

static int set_time(uint8_t* payload, unsigned trigger)
{
    struct tm* tm;
    struct timeval tv;
    int i = 20000;

    // We will try to set the clock to a specific time, HH:MM:SS,
    // but there is latency for sending a new time to the clock,
    // so we try to account for that by subtracting some latency:
    // trigger + latency = 1000000 microseconds

    // First we wait until we're at some point within a second which is
    // less than the trigger point.
    do {
        i--;
        if (i < 0) {
            fputs("unable to calibrate to specified latency\n", stderr);
            return 0;
        }
        usleep(100);
        if (gettimeofday(&tv, NULL) != 0) {
            perror("gettimeofday");
            return 0;
        }
    } while (tv.tv_usec >= trigger);

    // now we can determine HH:MM:SS for the next second
    tv.tv_sec++;
    tm = localtime(&tv.tv_sec);
    if (!tm) {
        perror("localtime");
        return 0;
    }
    payload[0] = 'T';
    payload[1] = tm->tm_hour;
    payload[2] = tm->tm_min;
    payload[3] = tm->tm_sec;

    // now wait for the trigger point and return when we reach it
    do {
        i--;
        if (i < 0) {
            fputs("unable to calibrate to specified latency\n", stderr);
            return 0;
        }
        usleep(100);
        if (gettimeofday(&tv, NULL) != 0) {
            perror("gettimeofday");
            return 0;
        }
    } while (tv.tv_usec < trigger);

    return 1;
}

int main(int argc, char** argv)
{
    uint8_t payload[PACKET_PAYLOAD_SIZE];
    int     i;
    const char *cmd = NULL;
    int     size = 0;

    if (argc >= 2) {
        if ((argc >= 3)
        && ((strcasecmp(argv[1], "local") == 0)
        || (strcasecmp(argv[1], "udp") == 0))) {
            // Skip this obsolete part of the command
            memmove(&argv[1], &argv[2], sizeof(char*) * (argc - 2));
            argc--;
        }
        cmd = argv[1];
        size = argc - 1;
    }
    if (cmd == NULL) {
        fprintf(stderr,
            "Usage: txnc433 <message>\n"
            "<message> may be any of\n"
            "  set_time = set the time\n"
            "  set_alarm <h> <m> = set the alarm to <h>:<m> (decimals)\n"
            "  unset_alarm = cancel alarm\n"
            "  message <M> = send tiny message <M>\n"
            "  set_day_night_time <hn> <mn> <hd> <md> = set the start time \n"
            "    for night as <hn>:<mn> and day as <hd>:<md>\n"
            "  counter = show HMAC counter\n"
            "  resync = resynchronise HMAC counter\n"
            "  advresync = advance HMAC counter by a long way, then resynchronise\n"
            "  or: 1..6 bytes, separated by spaces, each written\n"
            "      as a decimal or as a hex value prefixed by 0x\n");
        return 1;
    }

    if (!libnc_init()) {
        return 1;
    }

    memset(payload, 0, sizeof(payload));
    if (size > PACKET_PAYLOAD_SIZE) {
        size = PACKET_PAYLOAD_SIZE;
    }
    for (i = 0; i < size; i++) {
        payload[i] = (uint8_t) strtol(argv[i + 1], NULL, 0);
    }
    if (strcasecmp(cmd, "set_time") == 0) {
        int latency = 100000;
        if (argc >= 3) {
            latency = strtol(argv[2], NULL, 0);
        }
        if (!set_time(payload, 1000000 - latency)) {
            return 1;
        }
        size = 4;
    } else if (strcasecmp(cmd, "set_alarm") == 0) {
        payload[0] = 'A';
        size = 3;
    } else if (strcasecmp(cmd, "unset_alarm") == 0) {
        payload[0] = 'a';
        size = 1;
    } else if (strcasecmp(cmd, "set_day_night_time") == 0) {
        payload[0] = 'N';
        size = 5;
    } else if (strcasecmp(cmd, "message") == 0) {
        payload[0] = 'M';
        for (i = 1; i < PACKET_PAYLOAD_SIZE; i++) {
            payload[i] = argv[2][i - 1];
            if (payload[i] == '\0') {
                break;
            }
        }
        size = i;
    } else if (strcasecmp(cmd, "counter") == 0) {
        payload[0] = 'C';
        size = 1;
    } else if (strcasecmp(cmd, "resync") == 0) {
        size = RESYNC;
    } else if (strcasecmp(cmd, "advresync") == 0) {
        if (!libnc_advance()) {
            return 1;
        }
        size = RESYNC;
    }

    if (!udp_message(payload, size)) {
        return 1;
    }
    return 0;
 }
