
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rx433.h"
#include "ncrs.h"

uint32_t test_time = 0;
extern void rx433_interrupt(void);

uint32_t micros()
{
    return test_time;
}

void display_message(const char* m)
{
    printf("message: %s\n", m);
    exit(1);
}

int main(void)
{
    FILE* fd;
    unsigned test_ok = 0;
    unsigned test_bad = 0;


    if (!ncrs_init()) {
        printf("rs = null\n");
        return 1;
    }
    fd = fopen("test_rs.txt", "rt");
    if (!fd) {
        perror("unable to read test data");
        return 1;
    }
    while (fscanf(fd, "%x\n", &test_time) == 1) {
        rx433_home_easy = 0;
        rx433_new_code_ready = 0;
        rx433_interrupt();
        if (rx433_home_easy != 0) {
            fprintf(stderr, "invalid Home Easy code received: %08x\n", rx433_home_easy);
            return 1;
        }
        if (rx433_new_code_ready) {
            uint8_t decoded[DECODED_DATA_BYTES];
            if (!ncrs_decode(decoded, (const uint8_t*) rx433_new_code)) {
                printf("decode failed\n");
            } else {
                size_t i;
                // "ffbbcdeb38bdab49ca307b9ac580"
                printf("decode: ");
                for (i = 0; i < DECODED_DATA_BYTES; i++) {
                    printf("%02x", decoded[i]);
                }
                printf("\n");
            }
        }
    }
    fclose(fd);
    printf("%u %u\n", test_ok, test_bad);
    return 0;
}


