#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "libnc.h"
#include "hmac433.h"
#include "rx433.h"

#define NC_HEADER_SIZE 2
static const char* header = "NC";

int udp_message(const uint8_t* payload, size_t payload_size)
{
    uint8_t message[NC_DATA_SIZE + NC_HEADER_SIZE];
    int     s, broadcast;
    struct sockaddr_in dest;

    broadcast = 1;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(433);
    memset(&dest.sin_addr, 0xff, sizeof(dest.sin_addr));
    memset(message, 0, NC_DATA_SIZE + NC_HEADER_SIZE);
    memcpy(message, header, NC_HEADER_SIZE);

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        perror("Unable to create UDP socket");
        return 0;
    }

    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST,
                   &broadcast, sizeof(broadcast)) < 0) {
        perror("Unable to create UDP broadcast socket");
        close(s);
        return 0;
    }

    if (!libnc_encode(payload, payload_size,
                      &message[NC_HEADER_SIZE], NC_DATA_SIZE)) {
        close(s);
        return 0;
    }
    
    if (sendto(s, message, NC_DATA_SIZE + NC_HEADER_SIZE, 0,
                (const struct sockaddr *) &dest, sizeof(dest)) < 0) {
        perror("Unable to send message");
        close(s);
        return 0;
    }

    close(s);
    return 1;
 }
