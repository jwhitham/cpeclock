#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define NC_DATA_SIZE        31

static uint8_t TEST_CODE_5[] =
{0, 29, 12, 31, 15, 25, 26, 22, 29, 31, 2, 23, 26, 27, 18, 24, 6, 10, 2, 17, 21, 9, 29, 4, 21, 19, 1, 7, 19, 20, 0};
static uint8_t TEST_CODE_6[] =
{31, 24, 31, 19, 20, 1, 2, 26, 28, 23, 16, 23, 21, 25, 11, 1, 14, 2, 20, 5, 5, 9, 10, 2, 15, 15, 11, 13, 30, 21, 31};
static uint8_t TEST_CODE_7[] =
{29, 8, 6, 28, 26, 22, 1, 13, 24, 28, 18, 31, 27, 26, 1, 30, 19, 28, 24, 2, 9, 22, 11, 22, 27, 6, 18, 31, 6, 8, 12};

static void tx(const uint8_t* message) {
    int fd = open("/dev/tx433", O_WRONLY);
    if (fd < 0) {
        perror("Unable to open device");
        exit(1);
    }
    if (write(fd, message, NC_DATA_SIZE) != NC_DATA_SIZE) {
        perror("Unable to write message");
        exit(1);
    }
    close(fd);
}

int main(void)
{
    tx(TEST_CODE_5);
    usleep(250000);
    tx(TEST_CODE_6);
    usleep(250000);
    tx(TEST_CODE_7);
    return 0;
}

