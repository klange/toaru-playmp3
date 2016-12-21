/*
 *  ISC License
 *
 *  Copyright (c) 2016 Kevin Lange
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above copyright
 * notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>

#include "minimp3.h"

int main(int argc, char * argv[]) {
	/* minimp3 stuff */
	mp3_decoder_t mp3;
	mp3_info_t info;
	void *file_data;
	unsigned char *stream_pos;
	signed short sample_buf[MP3_MAX_SAMPLES_PER_FRAME];
	int bytes_left;
	int frame_size;
	int value;

	/* Speaker output stuff */
	int spkr = open("/dev/dsp", O_WRONLY);
	int song;

	if (argc < 2) return 1;

	song = open(argv[1], O_RDONLY);

	if (spkr == -1) {
		fprintf(stderr, "no dsp\n");
		return 1;
	}

	if (song == -1) {
		fprintf(stderr, "audio file not found\n");
		return 2;
	}

	mp3 = mp3_create();

	bytes_left = lseek(song, 0, SEEK_END);
	lseek(song, 0, SEEK_SET);
	file_data = malloc(bytes_left);
	read(song, file_data, bytes_left);
	stream_pos = (unsigned char *) file_data;
	fprintf(stderr, "%2x %2x %2x %2x %2x %2x %2x %2x\n",
			stream_pos[0],
			stream_pos[1],
			stream_pos[2],
			stream_pos[3],
			stream_pos[4],
			stream_pos[5],
			stream_pos[6],
			stream_pos[7]);
	bytes_left -= 100;

	mp3 = mp3_create();
	frame_size = mp3_decode(mp3, stream_pos, bytes_left, sample_buf, &info);
    if (!frame_size) {
		fprintf(stderr, "Invalid mp3?\n");
		return 1;
	}

	fprintf(stderr, "Playing, sample rate is %d, bytes left is %d\n", info.sample_rate, bytes_left);
	fprintf(stderr, "Frame size is %d, file data is at 0x%x\n", frame_size, file_data);

	while ((bytes_left >= 0) && (frame_size > 0)) {
		stream_pos += frame_size;
		bytes_left -= frame_size;
		write(spkr, (const void *) sample_buf, info.audio_bytes);
		frame_size = mp3_decode(mp3, stream_pos, bytes_left, sample_buf, NULL);
	}

	return 0;
}
