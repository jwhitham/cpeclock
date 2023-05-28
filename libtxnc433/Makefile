

CFLAGS=-I.. -Wall -g -O2
CC=gcc

SRCS = libtxnc433.c ../hmac433.c ../hmac.c \
        ../reed_solomon.c ../ncrs.c ../sha256.c 

OBJS = libtxnc433.o ../hmac433.o ../hmac.o \
        ../reed_solomon.o ../ncrs.o ../sha256.o 

HEADERS = ../hmac433.h ../hmac.h ../rslib.h \
        ../decode_rs.h ../encode_rs.h ../ncrs.h ../sha256.h

main: libtxnc433.so

libtxnc433.a: $(OBJS) $(HEADERS)
	ar rf libtxnc433.a $(OBJS)

libtxnc433.so: libtxnc433.a
	$(CC) -shared -o libtxnc433.so \
        $(OBJS) -Wl,--version-script=libtxnc433.version

clean:
	rm -f libtxnc433.so libtxnc433.a $(OBJS)


