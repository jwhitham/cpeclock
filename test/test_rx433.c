
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint32_t test_time = 0;
extern volatile uint32_t rx433_home_easy;
extern void rx433_interrupt(void);

uint32_t micros()
{
    return test_time;
}


int main(void)
{
    FILE* fd;
    unsigned test1_count = 0;
    unsigned test2_count = 0;
    unsigned test3_count = 0;
    unsigned test4_count = 0;
    unsigned test5_count = 0;

    fd = fopen("test_rx433.txt", "rt");
    if (!fd) {
        perror("unable to read test data");
        return 1;
    }
    while (fscanf(fd, "%x\n", &test_time) == 1) {
        rx433_home_easy = 0;
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
                fprintf(stderr, "invalid code received: %08x\n", rx433_home_easy);
                return 1;
        }
        // add tests for new codes here
    }
    fclose(fd);
    if ((test1_count < 15)
    || (test2_count < 15)
    || (test3_count < 1)
    || (test4_count < 8)
    || (test5_count < 3)
    || (test1_count > 20)
    || (test2_count > 20)
    || (test3_count > 5)
    || (test4_count > 10)
    || (test5_count > 5)) {
        fprintf(stderr, "incorrect results: %u %u %u %u %u\n",
                    test1_count, test2_count, test3_count, test4_count, test5_count);
        return 1;
    }
    printf("ok %u %u %u %u %u\n", test1_count, test2_count,
                test3_count, test4_count, test5_count);
    return 0;
}


