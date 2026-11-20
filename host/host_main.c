/* host_main.c - run the BASIC interpreter on a PC terminal (for testing).
 * Build: make -C host   Usage: BASIC_DIR=/tmp/x ./host/basic < program.txt
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/basic.h"

static void h_putch(char c) { putchar(c); }
static void h_cls(void) { printf("<CLS>\n"); }

/* 256x192 test framebuffer, -1 = empty */
static signed char fb[192][256];
static void h_pset(int x, int y, int c) { fb[y][x] = (signed char)c; }
static int  h_pget(int x, int y) { return fb[y][x]; }
static void h_gcls(void) { memset(fb, -1, sizeof fb); }

static int soundFrames;   /* fake playback: busy for a few frames */
static void h_sound(int n) {
	printf("<SOUND %d>\n", n);
	BasicSound *s = basic_sound(n);
	soundFrames = (n >= 0 && s) ? s->count * 2 : 0;
}
static int h_sound_busy(void) { return soundFrames > 0; }

int main(void) {
	static const BasicIO io = { h_putch, h_cls, NULL, NULL, NULL, 32, NULL,
	                                    h_pset, h_pget, h_gcls, 256, 192,
	                                    h_sound, h_sound_busy, NULL, NULL };
	char line[512];
	basic_init(&io);
	h_gcls();
	basic_set_storage(getenv("BASIC_DIR"));
	while (fgets(line, sizeof line, stdin)) {
		line[strcspn(line, "\r\n")] = 0;
		int num;
		int r = basic_enter(line, &num);
		if (r == BASIC_ENTER_ERROR) printf("?LINE ERROR\n");
		while (basic_state() != BASIC_IDLE) {
			if (basic_state() == BASIC_INPUT) {
				if (!fgets(line, sizeof line, stdin)) { basic_break(); break; }
				line[strcspn(line, "\r\n")] = 0;
				printf("%s\n", line);
				basic_input(line);
			}
			basic_run_slice(1000);
			basic_frame();
			if (soundFrames > 0 && --soundFrames == 0) printf("<SOUND END>\n");
		}
	}
	return 0;
}
