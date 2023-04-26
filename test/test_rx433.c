
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

uint32_t test_time = 0;
extern uint32_t rx433_data[];
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

    fd = fopen("test_rx433.txt", "rt");
    if (!fd) {
        perror("unable to read test data");
        return 1;
    }
    while (fscanf(fd, "%x\n", &test_time) == 1) {
        rx433_data[0] = 0;
        rx433_data[1] = 0;
        rx433_count = 0;
        rx433_interrupt();
        switch(rx433_count) {
            case 0:
            case 32:
                break;
            default:
                fprintf(stderr, "invalid count received: %d\n", rx433_count);
                return 1;
        }
        switch(rx433_data[0]) {
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
                fprintf(stderr, "invalid code received: %08x\n", rx433_data[0]);
                return 1;
        }
    }
    fclose(fd);
    if ((test1_count < 15)
    || (test2_count < 15)
    || (test1_count > 20)
    || (test2_count > 20)) {
        fprintf(stderr, "incorrect results: %u %u\n", test1_count, test2_count);
        return 1;
    }
    printf("ok %u %u\n", test1_count, test2_count);
    return 0;
}


