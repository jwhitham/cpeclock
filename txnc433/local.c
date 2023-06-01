#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>


#include "libnc.h"
#include "hmac433.h"
#include "rx433.h"


int local_message(const uint8_t* payload, size_t payload_size)
{
    uint8_t message[NC_DATA_SIZE];
    int     fd;

    fd = open("/dev/tx433", O_WRONLY);
    if (fd < 0) {
        perror("Unable to open /dev/tx433");
        return 0;
    }

    if (!libnc_encode(payload, payload_size, message, NC_DATA_SIZE)) {
        close(fd);
        return 0;
    }

    if (write(fd, message, NC_DATA_SIZE) != NC_DATA_SIZE) {
        perror("Unable to write message to /dev/tx433");
        close(fd);
        return 0;
    }

    close(fd);
    return 1;
}
