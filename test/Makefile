

CFLAGS=-I.. -Wall -g

test: test_rx433.exe test_hmac433.exe test_rs.exe \
        test_rx433.txt txnc433.exe test_alarm.exe
	./test_rx433.exe
	./test_hmac433.exe
	./test_rs.exe
	./test_alarm.exe

clean:
	rm -f *.o ../*.o *.exe test_rx433.txt
	rm -rf __pycache__

test_rx433.txt: make_test_rx433.py readcode.py 
	python make_test_rx433.py

test_rx433.exe: test_rx433.c ../rx433.c ../rx433.h
	gcc -o test_rx433.exe test_rx433.c ../rx433.c $(CFLAGS)

test_hmac433.exe: test_hmac433.c \
					../hmac433.c ../hmac433.h \
					../hmac.c ../hmac.h \
					../sha256.c ../sha256.h
	gcc -o test_hmac433.exe test_hmac433.c ../hmac433.c \
				../hmac.c ../sha256.c $(CFLAGS)

test_rs.exe: test_rs.c \
					../reed_solomon.c ../rslib.h ../decode_rs.h ../encode_rs.h \
					../ncrs.c ../ncrs.h \
					../sha256.c ../sha256.h
	gcc -o test_rs.exe test_rs.c ../ncrs.c ../sha256.c ../reed_solomon.c $(CFLAGS)

test_alarm.exe: test_alarm.c ../alarm.c ../alarm.h
	gcc -o test_alarm.exe test_alarm.c ../alarm.c $(CFLAGS)

../libtxnc433/libtxnc433.so:
	make -C ../libtxnc433

txnc433.exe: txnc433.c ../libtxnc433/libtxnc433.a
	gcc -o txnc433.exe txnc433.c \
        ../libtxnc433/libtxnc433.a -I../libtxnc433 \
        $(CFLAGS)
