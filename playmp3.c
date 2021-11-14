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
#include <wchar.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioctl.h>

#include "minimp3.h"

static void print_id3_string(size_t len, uint8_t type, uint8_t * data) {
	if (type == 0) {
		printf("%.*s\n", len, data);
	} else if (type == 1) {
		char * out = malloc(len * 3);
		char * t = out;
		for (size_t i = 0; i < len / 2; ++i) {
			wchar_t c[] = {((uint16_t*)data)[i], 0};
			if (c[0] == 0xFEFF) continue;
			if (c[0] == 0x0000) break;
			t += wcstombs(t, c, 7);
		}
		printf("%s\n", out);
		free(out);
	} else {
		printf("(unsupported ID3 tag encoding: %d)\n", type);
	}

}

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

	if (argc < 2) {
		fprintf(stderr, "usage: %s song.mp3\n", argv[0]);
		return 1;
	}

	song = open(argv[1], O_RDONLY);

	if (spkr == -1) {
		fprintf(stderr, "no dsp\n");
		return 1;
	}

	if (song == -1) {
		fprintf(stderr, "%s: %s: %s\n", argv[0], argv[1], strerror(errno));
		return 2;
	}

	mp3 = mp3_create();

	bytes_left = lseek(song, 0, SEEK_END);
	lseek(song, 0, SEEK_SET);
	file_data = malloc(bytes_left);
	read(song, file_data, bytes_left);
	stream_pos = (unsigned char *) file_data;

	/* See if it has IDv3v2.3 tags */
	if (!memcmp(stream_pos, "ID3", 3)) {
		printf("ID3v2.%d.%d:\n", stream_pos[3], stream_pos[4]);
		int unsynchronization = (stream_pos[5] & (1 << 7));
	//	int experimental      = (stream_pos[5] & (1 << 5));
		int extendedheader    = (stream_pos[5] & (1 << 6));

		if (unsynchronization) {
			printf("This file uses unsynchronization, so this probably won't work.\n");
		}

		uint32_t size =
			((stream_pos[6] & 0x7F) << 21) |
			((stream_pos[7] & 0x7F) << 14) |
			((stream_pos[8] & 0x7F) << 7) |
			((stream_pos[9] & 0x7F) << 0);

		uint8_t * tag = &stream_pos[10];
		if (extendedheader) {
			uint32_t esize =
				((stream_pos[10] & 0x7F) << 21) |
				((stream_pos[11] & 0x7F) << 14) |
				((stream_pos[12] & 0x7F) << 7) |
				((stream_pos[13] & 0x7F) << 0);
			tag = stream_pos + 10 + esize;
		}

		while (tag) {
			uint32_t size = (tag[4] << 24) | (tag[5] << 16) | (tag[6] << 8) | (tag[7]);
			if (size == 0) break;
			//printf("Tag '%c%c%c%c', %u bytes\n", tag[0], tag[1], tag[2], tag[3], size);
			if (!memcmp(tag, "TPE1", 4) || !memcmp(tag, "TIT2", 4)) {
				print_id3_string(size-1, tag[10], &tag[11]);
			}
			tag = tag + 10 + size;
		}
	} else if (!memcmp(&stream_pos[bytes_left - 128], "TAG", 3)) {
		printf("ID3v1:\n%30s\n%30s\n%30s\n",
			&stream_pos[bytes_left - 128 + 3],
			&stream_pos[bytes_left - 128 + 33],
			&stream_pos[bytes_left - 128 + 63]);
	}

	bytes_left -= 100;

	mp3 = mp3_create();
	frame_size = mp3_decode(mp3, stream_pos, bytes_left, sample_buf, &info);
    if (!frame_size) {
		fprintf(stderr, "Invalid mp3?\n");
		return 1;
	}

	if (info.audio_bytes == -1) {
		fprintf(stderr, "This MP3 has frames I can't understand. Try re-encoding it.\n");
		return 1;
	}

	fprintf(stderr, "%d.%d kHz, %d bytes per frame\n",
		info.sample_rate / 1000, (info.sample_rate / 100) % 10,
		info.audio_bytes);

	uint64_t sample_count = sample_count = info.audio_bytes / 4;

	uint64_t expanded_length;
	int expand_ratio;
	int16_t * snd;

	if (info.sample_rate != 48000) {
		expanded_length = (sample_count * 48000) / info.sample_rate;
		expand_ratio = (sample_count << 8) / expanded_length;
		snd = malloc(expanded_length * sizeof(int16_t) * 2);
	}

	while ((bytes_left >= 0) && (frame_size > 0)) {
		if (info.sample_rate != 48000) {
			for (int CH = 0; CH < 2; ++CH) {
				for (int i = 0; i < expanded_length; ++i) {
					int src = (i * expand_ratio) >> 8;
					snd[i * 2 + CH] = sample_buf[src * 2 + CH];
				}
			}
			size_t w = 0;
			while (w < info.audio_bytes) {
				w += write(spkr, (const void *)( (char *)snd + w), expanded_length * sizeof(int16_t) * 2 - w);
			}
		} else {
			size_t w = 0;
			while (w < info.audio_bytes) {
				w += write(spkr, (const void *)( (char *)sample_buf + w), info.audio_bytes - w);
			}
		}
		stream_pos += frame_size;
		bytes_left -= frame_size;
		frame_size = mp3_decode(mp3, stream_pos, bytes_left, sample_buf, &info);
	}

	return 0;
}
