
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rx433.h"

uint32_t test_time = 0;
extern void rx433_interrupt(void);

uint32_t micros()
{
    return test_time;
}

void panic()
{
    fprintf(stderr, "panic\n");
    exit(1);
}

static int matches_test_code(const uint8_t* expect)
{
    return memcmp(expect, (const uint8_t*) rx433_new_code, NC_DATA_SIZE) == 0;
}

static uint8_t TEST_CODE_1[] =
    {6, 23, 31, 29, 26, 21, 1, 18, 1, 4, 20, 27, 22, 27, 29, 16, 9, 22, 25, 28, 20, 13, 15, 18, 16, 21, 17, 31, 1, 1, 2};
static uint8_t TEST_CODE_2[] =
    {13, 17, 20, 16, 20, 14, 7, 29, 15, 29, 17, 1, 30, 20, 7, 7, 0, 28, 29, 1, 26, 14, 20, 22, 21, 28, 7, 21, 2, 28, 11};
static uint8_t TEST_CODE_3[] =
    {31, 20, 5, 31, 8, 31, 0, 18, 14, 12, 29, 21, 22, 7, 9, 17, 8, 6, 8, 4, 9, 29, 16, 1, 24, 18, 27, 5, 23, 29, 31};
static uint8_t TEST_CODE_4[] =
    {0, 28, 31, 29, 5, 30, 30, 20, 14, 12, 13, 4, 9, 12, 24, 10, 23, 4, 24, 29, 24, 15, 2, 17, 15, 5, 29, 0, 4, 17, 0};

int main(void)
{
    FILE* fd;
    unsigned test1_count = 0;
    unsigned test2_count = 0;
    unsigned test3_count = 0;

    fd = fopen("test_rx433.txt", "rt");
    if (!fd) {
        perror("unable to read test data");
        return 1;
    }
    while (fscanf(fd, "%x\n", &test_time) == 1) {
        rx433_home_easy = 0;
        rx433_new_code_ready = 0;
        rx433_interrupt();
        switch(rx433_home_easy) {
            case 0:
                break;
            case 0x4022b83:
                test1_count ++;
                printf("%d 1\n", test_time);
                break;
            case 0xad6496:
                test2_count ++;
                printf("%d 2\n", test_time);
                break;
            default:
                fprintf(stderr, "invalid Home Easy code received: %08x\n", rx433_home_easy);
                return 1;
        }
        if (rx433_new_code_ready) {
            if (matches_test_code(TEST_CODE_1)
            || matches_test_code(TEST_CODE_2)
            || matches_test_code(TEST_CODE_3)
            || matches_test_code(TEST_CODE_4)) {
                test3_count++;
                printf("%d new code begins %d\n", test_time, rx433_new_code[0]);
            } else {
                unsigned i;
                fprintf(stderr, "invalid new code received: ");
                for (i = 0; i < NC_DATA_SIZE; i++) {
                    fprintf(stderr, "%02x ", rx433_new_code[i]);
                }
                fprintf(stderr, "\n");
                return 1;
            }
        }
    }
    fclose(fd);
    if ((test1_count < 15)
    || (test2_count < 15)
    || (test3_count != 4)
    || (test1_count > 20)
    || (test2_count > 20)) {
        fprintf(stderr, "incorrect results: %u %u %u\n",
                    test1_count, test2_count, test3_count);
        return 1;
    }
    printf("ok %u %u %u\n", test1_count, test2_count, test3_count);
    return 0;
}


