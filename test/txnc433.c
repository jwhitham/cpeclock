
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>


#include "libtxnc433.h"
#include "hmac433.h"

int main(int argc, char** argv)
{
    uint8_t payload[PACKET_PAYLOAD_SIZE];
    int     i;

    if (!libtxnc433_init()) {
        return 1;
    }

    memset(payload, 0, sizeof(payload));
    for (i = 1; (i < argc) && (i <= PACKET_PAYLOAD_SIZE); i++) {
        payload[i - 1] = (uint8_t) strtol(argv[i], NULL, 0);
    }

    if (!libtxnc433_send(payload, i)) {
        return 1;
    }
    return 0;
 }
