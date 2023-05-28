

CFLAGS=-I.. -Wall -g -O2
CC=gcc

SRCS = libnc.c ../hmac433.c ../hmac.c \
        ../reed_solomon.c ../ncrs.c ../sha256.c 

OBJS = libnc.o ../hmac433.o ../hmac.o \
        ../reed_solomon.o ../ncrs.o ../sha256.o 

HEADERS = ../hmac433.h ../hmac.h ../rslib.h \
        ../decode_rs.h ../encode_rs.h ../ncrs.h ../sha256.h

main: libnc.so

libnc.a: $(OBJS) $(HEADERS)
	ar rf libnc.a $(OBJS)

libnc.so: libnc.a
	$(CC) -shared -o libnc.so \
        $(OBJS) -Wl,--version-script=libnc.version

clean:
	rm -f libnc.so libnc.a $(OBJS)


