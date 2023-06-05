
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>


#include "libnc.h"
#include "hmac433.h"
#include "rx433.h"

int main(int argc, char** argv)
{
    uint8_t payload[PACKET_PAYLOAD_SIZE];
    int     i;
    int     (* dest) (const uint8_t* payload, size_t payload_size) = NULL;
    const char *cmd = NULL;
    int     size = 0;

    if (argc >= 3) {
        if (strcasecmp(argv[1], "local") == 0) {
            dest = local_message;
        } else if (strcasecmp(argv[1], "udp") == 0) {
            dest = udp_message;
        }
        cmd = argv[2];
        size = argc - 2;
    }
    if ((dest == NULL) || (cmd == NULL)) {
        fprintf(stderr,
            "Usage: txnc433 <dest> <message>\n"
            "<dest> may be 'local' or 'udp'\n"
            "  local = message sent to /dev/tx433\n"
            "  udp = message broadcast via UDP\n"
            "<message> may be any of\n"
            "  set_time = set the time\n"
            "  set_alarm <h> <m> = set the alarm to <h>:<m> (decimals)\n"
            "  unset_alarm = cancel alarm\n"
            "  message <M> = send tiny message <M>\n"
            "  counter = show counter\n"
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
        payload[i] = (uint8_t) strtol(argv[i + 2], NULL, 0);
    }
    if (strcasecmp(cmd, "set_time") == 0) {
        struct tm tm;
        time_t t;

        t = time(NULL);
        if (localtime_r(&t, &tm) != 0) {
            perror("localtime");
            return 1;
        }
        payload[0] = 'T';
        payload[1] = tm.tm_hour;
        payload[2] = tm.tm_min;
        payload[3] = tm.tm_sec;
        size = 4;
    } else if (strcasecmp(cmd, "set_alarm") == 0) {
        payload[0] = 'A';
        size = 3;
    } else if (strcasecmp(cmd, "unset_alarm") == 0) {
        payload[0] = 'a';
        size = 1;
    } else if (strcasecmp(cmd, "message") == 0) {
        payload[0] = 'M';
        for (i = 1; i < size; i++) {
            payload[i] = argv[3][i - 1];
            if (payload[i] == '\0') {
                break;
            }
        }
        size = i;
    } else if (strcasecmp(cmd, "counter") == 0) {
        payload[0] = 'C';
        size = 1;
    }

    if (!dest(payload, size)) {
        return 1;
    }
    return 0;
 }
