
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
    uint8_t message[NC_DATA_SIZE];
    int     i, fd;

    if (!libnc_init()) {
        return 1;
    }

    memset(payload, 0, sizeof(payload));
    for (i = 1; (i < argc) && (i <= PACKET_PAYLOAD_SIZE); i++) {
        payload[i - 1] = (uint8_t) strtol(argv[i], NULL, 0);
    }

    if (!libnc_encode(payload, i, message, NC_DATA_SIZE)) {
        return 1;
    }

    fd = open("/dev/tx433", O_WRONLY);
    if (fd < 0) {
        perror("Unable to open /dev/tx433");
        return 0;
    } else {
        if (write(fd, message, NC_DATA_SIZE) != NC_DATA_SIZE) {
            perror("Unable to write message");
            close(fd);
            return 0;
        } else {
            close(fd);
            return 1;
        }
    }
    return 0;
 }
