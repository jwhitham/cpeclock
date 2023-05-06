
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rx433.h"
#include "rslib.h"

#define NROOTS          (10)    // 10 parity symbols
#define MSG_SYMBOLS     (21)    // 21 message symbols
#define GFPOLY          (0x25)  // Reed Solomon Galois field polynomial
#define FCR             (1)     // First Consecutive Root
#define PRIM            (1)     // Primitive Element
#define PAD             (0)     // No padding

uint32_t test_time = 0;
extern void rx433_interrupt(void);

uint32_t micros()
{
    return test_time;
}

int main(void)
{
    FILE* fd;
    unsigned test_ok = 0;
    unsigned per_file = 1;
    struct rs_control *rs = NULL;


    rs = init_rs(SYMBOL_SIZE, GFPOLY, FCR, PRIM, NROOTS);
    if (!rs) {
        printf("rs = null\n");
        return 1;
    }
    fd = fopen("test_rs.txt", "rt");
    if (!fd) {
        perror("unable to read test data");
        return 1;
    }
    while (fscanf(fd, "%x\n", &test_time) == 1) {
        if (test_time == 0) {
            // new file
            if (per_file != 1) {
                fprintf(stderr, "Less than one message per file?\n");
                return 1;
            }
            per_file = 0;
        }

        rx433_home_easy = 0;
        rx433_new_code_ready = 0;
        rx433_interrupt();
        if (rx433_home_easy != 0) {
            fprintf(stderr, "invalid Home Easy code received: %08x\n", rx433_home_easy);
            return 1;
        }
        if (rx433_new_code_ready) {
            uint8_t data[MSG_SYMBOLS];
            int erasures[NC_DATA_SIZE];
            uint16_t corrections[NC_DATA_SIZE];
            uint16_t parity[NROOTS];
            int num_erasures = 0;
            int i, num_errors;
            int contains_data = 0;

            memset(corrections, 0, sizeof(corrections));
            for (i = 0; i < NC_DATA_SIZE; i++) {
                if (i < MSG_SYMBOLS) {
                    data[i] = rx433_new_code[i];
                } else {
                    parity[i - MSG_SYMBOLS] = rx433_new_code[i];
                }
                if (rx433_new_code[i] >= (1 << SYMBOL_SIZE)) {
                    erasures[num_erasures] = i;
                    num_erasures++;
                } else if (rx433_new_code[i] != 0) {
                    contains_data = 1;
                }
            }

            if (contains_data) {
                num_errors = decode_rs8(rs, data, parity, MSG_SYMBOLS, NULL, num_erasures, erasures, 0, corrections);
                for (i = 0; i < MSG_SYMBOLS; i++) {
                    if (data[i] != (i ^ ((1 << SYMBOL_SIZE) - 1))) {
                        fprintf(stderr, "Uncorrected error!\n");
                        return 1;
                    }
                }
                printf("Message ok, errors fixed: %d\n", num_errors);
                if (per_file != 0) {
                    fprintf(stderr, "More than one message per file?\n");
                    return 1;
                }
                test_ok++;
                per_file++;
            }
        }
    }
    fclose(fd);
    free_rs(rs);
    if (test_ok != 5) {
        fprintf(stderr, "Unexpected number of messages: %d\n", test_ok);
    }
    printf("ok %u\n", test_ok);
    return 0;
}


