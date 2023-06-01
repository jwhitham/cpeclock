
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>


#include "libnc.h"
#include "hmac433.h"
#include "rx433.h"

int main(int argc, char** argv)
{
    uint8_t payload[PACKET_PAYLOAD_SIZE];
    int     i;
    int     (* dest) (const uint8_t* payload, size_t payload_size) = NULL;

    if (argc >= 3) {
        if (strcasecmp(argv[1], "local") == 0) {
            dest = local_message;
        } else if (strcasecmp(argv[1], "udp") == 0) {
            dest = udp_message;
        }
    }
    if (dest == NULL) {
        fprintf(stderr,
            "Usage: txnc433 <dest> <byte> [<byte> [<byte> ...]]\n"
            "<dest> may be 'local' or 'udp'\n"
            "  local = message sent to /dev/tx433\n"
            "  udp = message broadcast via UDP\n"
            "<byte> may be written as a decimal or as a hex value prefixed by 0x\n");
        return 1;
    }

    if (!libnc_init()) {
        return 1;
    }

    memset(payload, 0, sizeof(payload));
    for (i = 0; (i < (argc - 2)) && (i < PACKET_PAYLOAD_SIZE); i++) {
        payload[i] = (uint8_t) strtol(argv[i + 2], NULL, 0);
    }

    if (!dest(payload, i)) {
        return 1;
    }
    return 0;
 }
