
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
    unsigned test_bad = 0;
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
        rx433_home_easy = 0;
        rx433_new_code_ready = 0;
        rx433_interrupt();
        if (rx433_home_easy != 0) {
            fprintf(stderr, "invalid Home Easy code received: %08x\n", rx433_home_easy);
            return 1;
        }
        if (rx433_new_code_ready) {
            uint8_t data[MSG_SYMBOLS];
            uint8_t copy[MSG_SYMBOLS];
            int erasures[NC_DATA_SIZE];
            uint16_t parity[NROOTS];
            int num_erasures = 0;
            int i, num_errors;
            int wrong = 0;
            int nonzero = 0;
            double test_time_f = test_time / 1e6;
            int show = 0;

            for (i = 0; i < NC_DATA_SIZE; i++) {
                uint8_t value = rx433_new_code[i];
                if (value >= (1 << SYMBOL_SIZE)) {
                    erasures[num_erasures] = i;
                    num_erasures++;
                    value = 0;
                }
                if (i < MSG_SYMBOLS) {
                    data[i] = value;
                    copy[i] = value;
                } else {
                    parity[i - MSG_SYMBOLS] = value;
                }
            }

            // Don't tell it the erasures! It doesn't correct them!
            num_erasures = 0;
            num_errors = decode_rs8(rs, data, parity, MSG_SYMBOLS, NULL, num_erasures, erasures, 0, NULL);
            wrong = 0;
            for (i = 0; i < MSG_SYMBOLS; i++) {
                if (data[i] != (i ^ ((1 << SYMBOL_SIZE) - 1))) {
                    wrong = 1;
                }
                if (data[i] != 0) {
                    nonzero = 1;
                }
            }
            if (wrong && (!nonzero) && (num_errors >= 0)) {
                printf("%1.1f: Zero\n", test_time_f);
            } else if (wrong && (num_errors >= 0)) {
                printf("%1.1f: Bad message; not detected by ECC (err %d):", test_time_f, num_errors);
                show = 1;
                test_bad++;
            } else if (wrong) {
                printf("%1.1f: Bad message rejected as uncorrectable: ", test_time_f);
                show = 1;
            } else if (num_errors < 0) {
                printf("%1.1f: Good message rejected as uncorrectable\n", test_time_f);
                test_bad++;
            } else if (num_errors > 0) {
                printf("%1.1f: Good message corrected by ECC (err: %d)\n", test_time_f, num_errors);
                test_ok++;
            } else {
                printf("%1.1f: Good message without correction\n", test_time_f);
                test_ok++;
            }
            if (show) {
                for (i = 0; i < MSG_SYMBOLS; i++) {
                    if (data[i] != copy[i]) {
                        printf(" %d>%d", copy[i], data[i]);
                    } else {
                        printf(" %d", data[i]);
                    }
                }
                printf("\n");
            }
        }
    }
    fclose(fd);
    free_rs(rs);
    printf("%u %u\n", test_ok, test_bad);
    return 0;
}


