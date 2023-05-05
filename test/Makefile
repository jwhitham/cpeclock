

CFLAGS=-I.. -Wall -g

test: test_rx433.exe test_hmac433.exe test_rx433.txt test_hmac433.bin
	./test_rx433.exe
	./test_hmac433.exe

clean:
	rm -f *.o ../*.o *.exe test_rx433.txt test_hmac433.bin
	rm -rf __pycache__

test_rx433.txt: make_test_rx433.py readcode.py 
	python make_test_rx433.py

test_rx433.exe: test_rx433.c ../rx433.c ../rx433.h
	gcc -o test_rx433.exe test_rx433.c ../rx433.c $(CFLAGS) -DTEST_MODE

test_hmac433.bin: make_test_hmac433.py readcode.py 
	python make_test_hmac433.py

test_hmac433.exe: test_hmac433.c ../hmac433.c ../hmac433.h \
					../hmac.c ../hmac.h \
					../sha256.c ../sha256.h
	gcc -o test_hmac433.exe test_hmac433.c ../hmac433.c \
				../hmac.c ../sha256.c $(CFLAGS)

