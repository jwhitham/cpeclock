
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>


#include "hmac433.h"
#include "ncrs.h"

typedef struct secret_file_s {
    uint64_t    counter;
    uint8_t     secret_data[56];
} secret_file_t;

static FILE* secret_fd = NULL;
static secret_file_t secret_file;
static const char* secret_file_name = ".hmac433.dat";

static int load_secret_file(const char* env_name)
{
    const char* env_value = getenv(env_name);
    char        tmp[BUFSIZ];
    ssize_t     rc;

    if (!env_value) {
        return 0;
    }
    snprintf(tmp, sizeof(tmp), "%s/%s", env_value, secret_file_name);
    secret_fd = fopen(tmp, "rb+");
    if (!secret_fd) {
        return 0;
    }
    rc = fread(&secret_file, 1, sizeof(secret_file), secret_fd);
    if (rc != sizeof(secret_file)) {
        if (rc < 0) {
            perror("read of secret file failed");
        }
        fprintf(stderr, "secret file '%s' does not contain %d bytes, read only %d\n",
                secret_file_name, (int) sizeof(secret_file), (int) rc);
        fclose(secret_fd);
        secret_fd = NULL;
        return 0;
    }
    return 1;
}

static int save_secret_file()
{
    rewind(secret_fd);
    if (fwrite(&secret_file, 1, sizeof(secret_file), secret_fd) != sizeof(secret_file)) {
        perror("update of secret file failed");
        return 0;
    }
    if (fflush(secret_fd) != 0) {
        return 0;
    }
    return 1;
}

void display_message(const char* msg)
{
    fprintf(stderr, "error: %s\n", msg);
}

int libtxnc433_init(void)
{
    if (secret_fd) {
        fprintf(stderr, "already initialised\n");
        return 0;
    }

    memset(&secret_file, 0, sizeof(secret_file));

    if (!ncrs_init()) {
        fprintf(stderr, "rs = null\n");
        return 0;
    }

    if (!load_secret_file("APPDATA") && !load_secret_file("HOME")) {
        fprintf(stderr,
                "Unable to find the secret file '%s' in either "
                "$HOME or $APPDATA\n", secret_file_name);
        return 0;
    }
    return 1;
}

int libtxnc433_send(const uint8_t* payload, size_t size)
{
    hmac433_packet_t    packet;
    uint8_t             message[NC_DATA_SIZE];
    unsigned            i;
    int                 fd;

    memset(&packet, 0, sizeof(packet));
    memset(&message, 0, sizeof(message));

    for (i = 0; (i < size) && (i < PACKET_PAYLOAD_SIZE); i++) {
        packet.payload[i] = payload[i];
    }

    if (!secret_fd) {
        return 0;
    }
    hmac433_encode(secret_file.secret_data, sizeof(secret_file.secret_data),
                   &packet, &secret_file.counter);
    ncrs_encode(message, (const uint8_t *) &packet);

    // Test that the packet can be decoded ok
    {
        hmac433_packet_t    packet2;
        uint64_t            counter2 = secret_file.counter - 1;

        ncrs_decode((uint8_t *) &packet2, message);
        if (memcmp(&packet2, &packet, sizeof(packet)) != 0) {
            return 0;
        }
        if (!hmac433_authenticate(secret_file.secret_data,
                        sizeof(secret_file.secret_data),
                        &packet2, &counter2)) {
            return 0;
        }
    }

    if (!save_secret_file()) {
        return 0;
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
 }
