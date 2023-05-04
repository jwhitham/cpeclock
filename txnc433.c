
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <fec.h>

#define SYMBOL_SIZE     (5)     // 5 bits per symbol
#define NROOTS          (10)    // 10 parity symbols
#define MSG_SYMBOLS     (21)    // 21 message symbols
#define GFPOLY          (0x25)  // Reed Solomon Galois field polynomial
#define FCR             (1)     // First Consecutive Root
#define PRIM            (1)     // Primitive Element
#define PAD             (0)     // No padding


int main(void)
{
    void*   reed_solomon;
    uint8_t message[MSG_SYMBOLS + NROOTS];
    size_t  i;
    int     rc;
    int     fd;

    reed_solomon = init_rs_char(SYMBOL_SIZE, GFPOLY, FCR, PRIM, NROOTS, PAD);
    if (!reed_solomon) {
        printf("init_rs failed\n");
        return 1;
    }
    memset(message, 0, sizeof(message));
    for (i = 0; i < MSG_SYMBOLS; i++) {
        message[i] = i ^ ((1 << SYMBOL_SIZE) - 1);
    }

    encode_rs_char(reed_solomon, message, &message[MSG_SYMBOLS]);

    for (i = 0; i < (MSG_SYMBOLS + NROOTS); i++) {
        printf("%02x ", message[i]);
    }
    printf("\n");

    fd = open("/dev/tx433", O_WRONLY);
    if (fd < 0) {
        perror("Unable to open device");
    } else {
        if (write(fd, message, MSG_SYMBOLS + NROOTS) != (MSG_SYMBOLS + NROOTS)) {
            perror("Unable to write message");
        }
        close(fd);
    }

    message[0] ^= 1;
    rc = decode_rs_char(reed_solomon, message, NULL, 0);
    printf("rc = %d\n", rc);
    for (i = 0; i < (MSG_SYMBOLS + NROOTS); i++) {
        printf("%02x ", message[i]);
    }
    printf("\n");

    free_rs_char(reed_solomon);
    return 0;
 }
