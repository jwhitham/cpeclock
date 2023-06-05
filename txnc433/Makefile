

CFLAGS=-Wall -g -O2
CC=gcc

SRCS = libnc.c ../hmac433.c ../hmac.c \
        ../reed_solomon.c ../ncrs.c ../sha256.c \
        udp.c local.c txnc433.c

txnc433: $(SRCS)
	gcc -o txnc433 $(SRCS) -I.. $(CFLAGS)

