
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint32_t test_time = 0;
extern uint32_t rx433_data[4];
extern uint32_t rx433_count;
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
    unsigned expect_count = 0;

    fd = fopen("test_rx433.txt", "rt");
    if (!fd) {
        perror("unable to read test data");
        return 1;
    }
    while (fscanf(fd, "%x\n", &test_time) == 1) {
        memset(rx433_data, 0, sizeof(rx433_data));
        rx433_count = 0;
        rx433_interrupt();
        expect_count = 0;
        switch(rx433_data[0]) {
            case 0:
                break;
            case 0x4022b83:
                expect_count = 32;
                test1_count ++;
                printf("%d 1\n", test_time);
                break;
            case 0xad6496:
                expect_count = 32;
                test2_count ++;
                printf("%d 2\n", test_time);
                break;
            case 0x6f5902ac:
                expect_count = 128;
                if (!((rx433_data[1] == 0x237024bd)
                && (rx433_data[2] == 0xd0c176cb)
                && (rx433_data[3] == 0x93063dc4))) {
                    fprintf(stderr, "partially invalid code received: %08x\n", rx433_data[0]);
                    return 1;
                }
                test3_count ++;
                printf("%d 3\n", test_time);
                break;
            default:
                fprintf(stderr, "invalid code received: %08x\n", rx433_data[0]);
                return 1;
        }
        if (rx433_count != expect_count) {
            fprintf(stderr, "invalid count received: %d\n", rx433_count);
            return 1;
        }
    }
    fclose(fd);
    if ((test1_count < 15)
    || (test2_count < 15)
    || (test3_count < 1)
    || (test1_count > 20)
    || (test2_count > 20)
    || (test3_count > 15)) {
        fprintf(stderr, "incorrect results: %u %u %u\n",
                    test1_count, test2_count, test3_count);
        return 1;
    }
    printf("ok %u %u %u\n", test1_count, test2_count, test3_count);
    return 0;
}


