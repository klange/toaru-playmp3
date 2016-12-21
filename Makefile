CC = i686-pc-toaru-gcc

libminimp3.so: minimp3.c
	$(CC) -shared -fPIC -o $@ $< -lm

playmp3: playmp3.c
	$(CC) -o $@ $< -lminimp3 -lm

