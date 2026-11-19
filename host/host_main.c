/* host_main.c - run the BASIC interpreter on a PC terminal (for testing).
 * Build: make -C host   Usage: BASIC_DIR=/tmp/x ./host/basic < program.txt
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/basic.h"

static void h_putch(char c) { putchar(c); }
static void h_cls(void) { printf("<CLS>\n"); }

int main(void) {
	static const BasicIO io = { h_putch, h_cls, NULL, NULL, NULL, 32, NULL };
	char line[512];
	basic_init(&io);
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
		}
	}
	return 0;
}
