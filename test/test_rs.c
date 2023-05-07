
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

static int scan_file(const char* filename, int allow_bad_data)
{
    int test_ok = 0;
    FILE* fd;
    size_t i;

    fd = fopen(filename, "rt");
    if (!fd) {
        perror("unable to read test data");
        return 1;
    }
    while (fscanf(fd, "%x\n", &test_time) == 1) {
        rx433_home_easy = 0;
        rx433_new_code_ready = 0;
        memset((uint8_t *) rx433_new_code, 0x55, NC_DATA_SIZE);
        rx433_interrupt();
        if (rx433_home_easy != 0) {
            fprintf(stderr, "invalid Home Easy code received: %08x\n", rx433_home_easy);
            exit(1);
        }
        if (rx433_new_code_ready) {
            uint8_t decoded[DECODED_DATA_BYTES];
            memset(decoded, 0x55, DECODED_DATA_BYTES);
            int ok = ncrs_decode(decoded, (const uint8_t*) rx433_new_code);
            if (ok <= 0) {
                fprintf(stderr, "decode failed\n");
                exit(1);
            } else {
                const uint8_t expect[] = "\xff\xbb\xcd\xeb\x38\xbd\xab\x49\xca\x30\x7b\x9a\xc5\x80";
                if (memcmp(expect, decoded, DECODED_DATA_BYTES) == 0) {
                    test_ok++;
                    if (ok > 1) {
                        test_ok += 5;
                        printf("message corrected\n");
                    } else {
                        printf("message ok\n");
                    }
                } else if (allow_bad_data) {
                    printf("message is wrong: ");
                    for (i = 0; i < DECODED_DATA_BYTES; i++) {
                        printf("%02x", decoded[i]);
                    }
                    printf("\n");
                    test_ok -= 10;
                } else {
                    fprintf(stderr, "decode seemed ok (%d) but data is incorrect: ", ok);
                    for (i = 0; i < DECODED_DATA_BYTES; i++) {
                        fprintf(stderr, "%02x", decoded[i]);
                    }
                    fprintf(stderr, "\n");
                    exit(1);
                }
            }
        }
    }
    fclose(fd);
    printf("%s got %d\n", filename, test_ok);
    return test_ok;
}

int main(void)
{
    if (!ncrs_init()) {
        printf("rs = null\n");
        return 1;
    }
    if (scan_file("test_rs.txt", 0) != 10) {
        fprintf(stderr, "score should be 10\n");
        exit(1);
    }
    if (scan_file("test_rs2.txt", 1) != 189) {
        fprintf(stderr, "score should be 189\n");
        exit(1);
    }
    return 0;
}


