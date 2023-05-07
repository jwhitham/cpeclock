
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hmac.h"
#include "hmac433.h"

#define SECRET_DATA "secret"
#define SECRET_SIZE 6


static void test_message(const hmac433_packet_t* packet, uint64_t* rx_counter, int expect_pass)
{
    if (expect_pass != hmac433_authenticate(
            (const uint8_t*) SECRET_DATA, SECRET_SIZE,
            packet, rx_counter)) {
        char text[PACKET_PAYLOAD_SIZE + 1];
        memcpy(text, packet->payload, PACKET_PAYLOAD_SIZE);
        text[PACKET_PAYLOAD_SIZE] = '\0';
        fprintf(stderr, "error with authentication, packet '%s', expected outcome %d\n",
                    text, expect_pass);
        exit(1);
    }
}

static void encode(hmac433_packet_t* packet, uint64_t* tx_counter)
{
    hmac433_encode(
        (const uint8_t*) SECRET_DATA, SECRET_SIZE,
        packet, tx_counter);
}

int main(void)
{
    uint8_t digest[HMAC_DIGEST_SIZE];
    uint64_t zero = 0;
    uint64_t tx_counter = 0;
    uint64_t rx_counter = 0;
    unsigned i = 0;
    hmac433_packet_t packet;

    // Test HMAC algorithm and SHA256
    // This test data was made with Python's hmac module
    // e.g. d=hmac.digest(b"keydata", b"message", "sha256")
    hmac_sha256(
        (const uint8_t*) "keydata", 7,
        &zero,
        (const uint8_t*) "message", 7,
        digest);
    if (memcmp(digest,
            "\xea\x2e\xd4\x62\x58\x6c\x61\xa9\xc2\x95\xd7\xb5\x2c\x95\x1f\x07"
            "\x8d\x6d\x17\x61\x71\x16\xfa\x84\x38\x70\xc3\x86\x59\x0f\x1f\x1d",
            HMAC_DIGEST_SIZE) != 0) {
        fprintf(stderr, "error 1!\n");
        return 1;
    }
    hmac_sha256(
        (const uint8_t*) "k", 1,
        &zero,
        (const uint8_t*) "m", 1,
        digest);
    if (memcmp(digest,
            "\xb6\x00\x90\xe3\x05\x22\x97\xae\xb5\xa0\x80\x88\x9c\xe2\xfc\x4b"
            "\xca\x95\x7e\x75\x6f\xae\xb4\xdf\x7d\x31\x80\x0c\xa1\xe7\x71\xec",
            HMAC_DIGEST_SIZE) != 0) {
        fprintf(stderr, "error 2!\n");
        return 1;
    }

    // Test HMAC433 function
    memset(&packet, 0, sizeof(packet));

    // Authentic messages
    for (i = 0; i < 300; i++) {
        snprintf((char*) packet.payload, PACKET_PAYLOAD_SIZE, "msg%d", i);
        encode(&packet, &tx_counter);
        test_message(&packet, &rx_counter, 1);
    }

    // Corrupt message
    snprintf((char*) packet.payload, PACKET_PAYLOAD_SIZE, "hello");
    encode(&packet, &tx_counter);
    packet.payload[0] = 'H';
    test_message(&packet, &rx_counter, 0);

    // Bad counter
    tx_counter += 0x100;
    encode(&packet, &tx_counter);
    test_message(&packet, &rx_counter, 0);
    tx_counter -= 0x100;

    // Good counter
    encode(&packet, &tx_counter);
    test_message(&packet, &rx_counter, 1);

    // Good counter jumps forward
    for (i = 0; i < 5; i++) {
        snprintf((char*) packet.payload, PACKET_PAYLOAD_SIZE, "jmp%d", i);
        tx_counter += 0xff;
        encode(&packet, &tx_counter);
        test_message(&packet, &rx_counter, 1);
    }
    // Replay
    test_message(&packet, &rx_counter, 0);
    snprintf((char*) packet.payload, PACKET_PAYLOAD_SIZE, "replay");
    encode(&packet, &tx_counter);
    test_message(&packet, &rx_counter, 1);
    test_message(&packet, &rx_counter, 0);
    // The distant future.. the year 2000...
    tx_counter += (uint64_t) 1 << 60;
    rx_counter += (uint64_t) 1 << 60;
    for (i = 0; i < 5; i++) {
        snprintf((char*) packet.payload, PACKET_PAYLOAD_SIZE, "ftr%d", i);
        encode(&packet, &tx_counter);
        test_message(&packet, &rx_counter, 1);
    }
    tx_counter += (uint64_t) 1 << 60;
    snprintf((char*) packet.payload, PACKET_PAYLOAD_SIZE, "final%d", i);
    encode(&packet, &tx_counter);
    test_message(&packet, &rx_counter, 0);
    rx_counter += (uint64_t) 1 << 60;
    test_message(&packet, &rx_counter, 1);
    printf("ok\n");
    return 0;
}
