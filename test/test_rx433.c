
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

void set_int_pin(char pin)
{
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
static uint8_t TEST_CODE_5[] =
    {0, 29, 12, 31, 15, 25, 26, 22, 29, 31, 2, 23, 26, 27, 18, 24, 6, 10, 2, 17, 21, 9, 29, 4, 21, 19, 1, 7, 19, 20, 0};
static uint8_t TEST_CODE_6[] =
    {31, 24, 31, 19, 20, 1, 2, 26, 28, 23, 16, 23, 21, 25, 11, 1, 14, 2, 20, 5, 5, 9, 10, 2, 15, 15, 11, 13, 30, 21, 31};
static uint8_t TEST_CODE_7[] =
    {29, 8, 6, 28, 26, 22, 1, 13, 24, 28, 18, 31, 27, 26, 1, 30, 19, 28, 24, 2, 9, 22, 11, 22, 27, 6, 18, 31, 6, 8, 12};
static uint8_t TEST_CODE_6_NOISY[] =
    {25, 31, 19, 20, 1, 2, 26, 28, 23, 16, 23, 21, 25, 11, 1, 14, 2, 20, 5, 5, 9, 10, 2, 15, 15, 11, 13, 30, 21, 31, 0};
static uint8_t TEST_CODE_9[] =
    {8, 12, 17, 0, 0, 28, 0, 0, 4, 0, 0, 16, 0, 1, 18, 10, 2, 18, 30, 13, 0, 31, 26, 12, 11, 15, 19, 30, 22, 23, 14};


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
            int c = 0;
            if (matches_test_code(TEST_CODE_1)) { c = 1; }
            if (matches_test_code(TEST_CODE_2)) { c = 2; }
            if (matches_test_code(TEST_CODE_3)) { c = 3; }
            if (matches_test_code(TEST_CODE_4)) { c = 4; }
            if (matches_test_code(TEST_CODE_5)) { c = 5; }
            if (matches_test_code(TEST_CODE_6)) { c = 6; }
            if (matches_test_code(TEST_CODE_7)) { c = 7; }
            if (matches_test_code(TEST_CODE_6_NOISY)) { c = 8; }
            if (matches_test_code(TEST_CODE_9)) { c = 9; }

            if (c != 0) {
                test3_count++;
                printf("%d new code %d\n", test_time, c);
            } else {
                unsigned i;
                fprintf(stderr, "%d invalid new code received: ", test_time);
                for (i = 0; i < NC_DATA_SIZE; i++) {
                    fprintf(stderr, "%0d, ", rx433_new_code[i]);
                }
                fprintf(stderr, "\n");
                return 1;
            }
        }
    }
    fclose(fd);
    if ((test1_count < 15)
    || (test2_count < 15)
    || (test3_count != 9)
    || (test1_count > 20)
    || (test2_count > 20)) {
        fprintf(stderr, "incorrect results: %u %u %u\n",
                    test1_count, test2_count, test3_count);
        return 1;
    }
    printf("ok %u %u %u\n", test1_count, test2_count, test3_count);
    return 0;
}


