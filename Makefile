CC = x86_64-pc-toaru-gcc

all: playmp3 libminimp3.so

libminimp3.so: minimp3.c
	$(CC) -O2 -shared -fPIC -o $@ $< -lm

playmp3: playmp3.c libminimp3.so
	$(CC) -o $@ $< -L. -lminimp3 -lm -lc

