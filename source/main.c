/*
 * NDS BASIC - main.c
 *
 * Top screen:    program listing with line numbers + input line (edit view)
 *                or the program output terminal (output view)
 * Bottom screen: retro on-screen keyboard (kbd.c)
 */
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "basic.h"
#include "kbd.h"
#include "help.h"

#define COLS 32
#define ROWS 24

/* C64 inspired 16 colour palette (5 bit per channel) */
static const u16 palette[16] = {
	RGB15(0, 0, 0),    RGB15(31, 31, 31), RGB15(17, 7, 6),   RGB15(14, 24, 25),
	RGB15(17, 8, 19),  RGB15(11, 20, 8),  RGB15(8, 6, 19),   RGB15(28, 29, 15),
	RGB15(18, 11, 4),  RGB15(10, 8, 0),   RGB15(24, 13, 12), RGB15(9, 9, 9),
	RGB15(15, 15, 15), RGB15(20, 30, 17), RGB15(16, 14, 29), RGB15(23, 23, 23)
};
enum { BLACK, WHITE, RED, CYAN, PURPLE, GREEN, BLUE, YELLOW, ORANGE, BROWN,
       LRED, DGREY, GREY, LGREEN, LBLUE, LGREY };
#define NOFILL 0xFF
#define BGCOL BLUE
#define TEXTCOL    LBLUE

typedef struct {
	u8 ch[ROWS][COLS];
	u8 fg[ROWS][COLS];
	u8 bg[ROWS][COLS];
} Screen;

static Screen term, edit;
static u16 *map0, *map1;

/* ------------------------------------------------------------------ */
/* screens                                                             */
/* ------------------------------------------------------------------ */

static void scr_clear(Screen *s, u8 fg) {
	memset(s->ch, ' ', sizeof s->ch);
	memset(s->fg, fg, sizeof s->fg);
	memset(s->bg, NOFILL, sizeof s->bg);
}

static void scr_put(Screen *s, int x, int y, char c, u8 fg, u8 bg) {
	if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return;
	s->ch[y][x] = (u8)c;
	s->fg[y][x] = fg;
	s->bg[y][x] = bg;
}

static void scr_text(Screen *s, int x, int y, const char *t, u8 fg, u8 bg) {
	for (; *t && x < COLS; t++, x++) scr_put(s, x, y, *t, fg, bg);
}

static void scr_upload(const Screen *s, int curX, int curY) {
	for (int y = 0; y < ROWS; y++)
		for (int x = 0; x < COLS; x++) {
			u8 fg = s->fg[y][x], bg = s->bg[y][x];
			if (x == curX && y == curY) { bg = TEXTCOL; fg = BGCOL; }
			map0[y * 32 + x] = s->ch[y][x] | (fg << 12);
			map1[y * 32 + x] = bg == NOFILL ? 0 : (1 | (bg << 12));
		}
}

/* ---------------- output terminal ---------------- */

static int tx, ty;
static u8 tcol = TEXTCOL;
static bool outputSeen;

static void term_scroll(void) {
	memmove(term.ch[0], term.ch[1], (ROWS - 1) * COLS);
	memmove(term.fg[0], term.fg[1], (ROWS - 1) * COLS);
	memmove(term.bg[0], term.bg[1], (ROWS - 1) * COLS);
	memset(term.ch[ROWS - 1], ' ', COLS);
	memset(term.fg[ROWS - 1], tcol, COLS);
	memset(term.bg[ROWS - 1], NOFILL, COLS);
}

static void term_newline(void) {
	tx = 0;
	if (++ty >= ROWS) { term_scroll(); ty = ROWS - 1; }
}

static void term_putc(char c) {
	if (c == '\n') { term_newline(); return; }
	if (tx >= COLS) term_newline();
	scr_put(&term, tx++, ty, c, tcol, NOFILL);
}

static void term_puts(const char *s) { while (*s) term_putc(*s++); }

/* ------------------------------------------------------------------ */
/* BASIC I/O callbacks                                                 */
/* ------------------------------------------------------------------ */

enum { VIEW_EDIT, VIEW_OUTPUT };
static int view = VIEW_EDIT;

static char keyq[16];
static int keyqHead, keyqTail;

static void io_putch(char c) { term_putc(c); outputSeen = true; view = VIEW_OUTPUT; }
static void io_cls(void) { scr_clear(&term, tcol); tx = ty = 0; view = VIEW_OUTPUT; }
static int io_inkey(void) {
	if (keyqHead == keyqTail) return 0;
	int c = (unsigned char)keyq[keyqTail];
	keyqTail = (keyqTail + 1) % sizeof keyq;
	return c;
}
static void io_locate(int c, int r) {
	tx = c < COLS ? c : COLS - 1;
	ty = r < ROWS ? r : ROWS - 1;
}
static void io_color(int c) { tcol = (u8)c; }

static const BasicIO bio = { io_putch, io_cls, io_inkey, io_locate, io_color, COLS };

static void keyq_push(int c) {
	int next = (keyqHead + 1) % sizeof keyq;
	if (next == keyqTail) return;
	keyq[keyqHead] = (char)c;
	keyqHead = next;
}

/* ------------------------------------------------------------------ */
/* line input (shared by editor and INPUT statement)                   */
/* ------------------------------------------------------------------ */

#define INPUT_MAX 94
static char inbuf[INPUT_MAX + 1];
static int inlen, incur, inmax = INPUT_MAX;
static int inStartX, inStartY;   /* INPUT statement: start in terminal */

static void input_clear(void) { inlen = incur = 0; inbuf[0] = 0; }

static void input_set(const char *s) {
	strncpy(inbuf, s, inmax);
	inbuf[inmax] = 0;
	inlen = incur = strlen(inbuf);
}

/* returns true if RETURN was pressed */
static bool input_key(int k) {
	switch (k) {
	case KC_RET: return true;
	case KC_DEL:
		if (incur > 0) {
			memmove(inbuf + incur - 1, inbuf + incur, inlen - incur + 1);
			incur--; inlen--;
		}
		break;
	case KC_LEFT:  if (incur > 0) incur--; break;
	case KC_RIGHT: if (incur < inlen) incur++; break;
	case KC_CLR:   input_clear(); break;
	default:
		if (k >= 32 && k < 127 && inlen < inmax) {
			memmove(inbuf + incur + 1, inbuf + incur, inlen - incur + 1);
			inbuf[incur++] = (char)k;
			inlen++;
		}
	}
	return false;
}

/* draw the input buffer into a screen starting at (x,y); returns cursor pos */
static void input_draw(Screen *s, int x, int y, int clearLen, int *cx, int *cy) {
	int pos = y * COLS + x;
	for (int i = 0; i < clearLen; i++) {
		int p = pos + i;
		if (p >= ROWS * COLS) break;
		scr_put(s, p % COLS, p / COLS, i < inlen ? inbuf[i] : ' ', i < inlen ? tcol : TEXTCOL, NOFILL);
	}
	int p = pos + incur;
	*cx = p % COLS;
	*cy = p / COLS;
}

/* ------------------------------------------------------------------ */
/* edit view                                                           */
/* ------------------------------------------------------------------ */

#define LIST_TOP  1
#define LIST_ROWS 19
#define STATUS_ROW 20
#define INPUT_ROW 21

static int sel = -1, topIndex;
static char statusMsg[33] = "READY.";
static u8 statusCol = LGREEN;

static void set_status(const char *msg, u8 colr) {
	strncpy(statusMsg, msg, 32);
	statusMsg[32] = 0;
	statusCol = colr;
}

/* rows a program line occupies in the listing */
static int line_rows(int i) {
	char num[8];
	int nw = sprintf(num, "%d ", basic_line_number(i));
	int len = strlen(basic_line_text(i));
	int first = COLS - nw;
	if (len <= first) return 1;
	int rest = COLS - nw;
	return 1 + (len - first + rest - 1) / rest;
}

static void ensure_visible(void) {
	int n = basic_line_count();
	if (sel >= n) sel = n - 1;
	if (topIndex >= n) topIndex = n > 0 ? n - 1 : 0;
	if (topIndex < 0) topIndex = 0;
	if (sel < 0) return;
	if (sel < topIndex) topIndex = sel;
	for (;;) {
		int rows = 0;
		for (int i = topIndex; i <= sel; i++) rows += line_rows(i);
		if (rows <= LIST_ROWS || topIndex >= sel) break;
		topIndex++;
	}
}

static void render_edit(int *cx, int *cy) {
	char buf[40];
	scr_clear(&edit, TEXTCOL);

	/* title bar */
	for (int x = 0; x < COLS; x++) scr_put(&edit, x, 0, ' ', BGCOL, LBLUE);
	scr_text(&edit, 1, 0, "NDS BASIC V1.0", BGCOL, LBLUE);
	int n = basic_line_count();
	sprintf(buf, "%d LINE%s", n, n == 1 ? "" : "S");
	scr_text(&edit, COLS - 1 - strlen(buf), 0, buf, BGCOL, LBLUE);

	/* listing */
	int row = LIST_TOP;
	if (n == 0) {
		scr_text(&edit, 1, 2, "NO PROGRAM. TYPE E.G.", LGREY, NOFILL);
		scr_text(&edit, 3, 4, "10 PRINT \"HELLO\"", YELLOW, NOFILL);
		scr_text(&edit, 3, 5, "20 GOTO 10", YELLOW, NOFILL);
		scr_text(&edit, 1, 7, "AND PRESS RUN.", LGREY, NOFILL);
	}
	for (int i = topIndex; i < n && row < LIST_TOP + LIST_ROWS; i++) {
		u8 fill = (i == sel) ? DGREY : NOFILL;
		int nw = sprintf(buf, "%d ", basic_line_number(i));
		const char *t = basic_line_text(i);
		int x = 0;
		for (int k = 0; k < COLS; k++) scr_put(&edit, k, row, ' ', TEXTCOL, fill);
		scr_text(&edit, 0, row, buf, YELLOW, fill);
		x = nw;
		bool inStr = false;
		for (; *t; t++) {
			if (x >= COLS) {
				if (++row >= LIST_TOP + LIST_ROWS) break;
				for (int k = 0; k < COLS; k++) scr_put(&edit, k, row, ' ', TEXTCOL, fill);
				x = nw;
			}
			if (*t == '"') inStr = !inStr;
			scr_put(&edit, x++, row, *t, (inStr || *t == '"') ? WHITE : TEXTCOL, fill);
		}
		row++;
	}

	/* status bar */
	for (int x = 0; x < COLS; x++) scr_put(&edit, x, STATUS_ROW, ' ', WHITE, DGREY);
	scr_text(&edit, 0, STATUS_ROW, statusMsg, statusCol, DGREY);

	/* input line */
	scr_put(&edit, 0, INPUT_ROW, ']', YELLOW, NOFILL);
	u8 saved = tcol;
	tcol = TEXTCOL;
	input_draw(&edit, 1, INPUT_ROW, (ROWS - INPUT_ROW) * COLS - 1, cx, cy);
	tcol = saved;
}

/* ------------------------------------------------------------------ */
/* actions                                                             */
/* ------------------------------------------------------------------ */

static bool waitReturn;      /* output view shown, any key returns to editor */
static int  frameCount;

static void start_exec(const char *cmd, bool showOutput) {
	int num;
	outputSeen = false;
	if (tx) term_newline();
	term_putc(']');
	term_puts(cmd);
	term_newline();
	basic_seed(frameCount * 2654435761u + 1);
	if (basic_enter(cmd, &num) == BASIC_ENTER_EXEC && showOutput) view = VIEW_OUTPUT;
}

static void submit_edit_line(void) {
	char cmd[INPUT_MAX + 1];
	char msg[40];
	strcpy(cmd, inbuf);

	/* skip leading spaces */
	const char *p = cmd;
	while (*p == ' ') p++;

	/* LIST [n] just moves the listing in the editor */
	if (!strncasecmp(p, "LIST", 4) && (p[4] == 0 || p[4] == ' ')) {
		int n = 0;
		sscanf(p + 4, "%d", &n);
		sel = basic_line_count() ? basic_find_index(n) : -1;
		if (sel >= basic_line_count()) sel = basic_line_count() - 1;
		topIndex = sel > 0 ? sel : 0;
		ensure_visible();
		input_clear();
		set_status("READY.", LGREEN);
		return;
	}

	if (*p >= '0' && *p <= '9') {
		int num = 0;
		int r = basic_enter(cmd, &num);
		if (r == BASIC_ENTER_STORED) {
			sel = basic_find_index(num);
			sprintf(msg, "LINE %d STORED", num);
			set_status(msg, LGREEN);
		} else if (r == BASIC_ENTER_DELETED) {
			sel = basic_find_index(num);
			if (sel >= basic_line_count()) sel = basic_line_count() - 1;
			sprintf(msg, "LINE %d DELETED", num);
			set_status(msg, LGREEN);
		} else {
			set_status("?ILLEGAL LINE NUMBER", LRED);
			return;
		}
		ensure_visible();
		input_clear();
		return;
	}
	if (!*p) return;
	bool isRun = !strncasecmp(p, "RUN", 3);
	start_exec(p, isRun);
	input_clear();
}

static void finish_exec(void) {
	const char *e = basic_last_error();
	if (*e) set_status(e, LRED);
	else set_status("READY.", LGREEN);
	if (basic_line_count() == 0) { sel = -1; topIndex = 0; }
	ensure_visible();
	if (view == VIEW_OUTPUT) {
		if (tx) term_newline();
		term_puts("READY.");
		term_newline();
		waitReturn = true;
	}
}

static void begin_input_statement(void) {
	/* make sure the answer has room on the screen */
	if (tx >= COLS) term_newline();
	while (ty > ROWS - 3) { term_scroll(); ty--; }
	inStartX = tx;
	inStartY = ty;
	inmax = ROWS * COLS - (ty * COLS + tx) - 1;
	if (inmax > INPUT_MAX) inmax = INPUT_MAX;
	input_clear();
	view = VIEW_OUTPUT;
}

static void end_input_statement(void) {
	int cx, cy;
	input_draw(&term, inStartX, inStartY, inlen, &cx, &cy);
	int end = inStartY * COLS + inStartX + inlen;
	tx = end % COLS;
	ty = end / COLS;
	if (ty >= ROWS) { ty = ROWS - 1; tx = COLS; }
	term_newline();
	char answer[INPUT_MAX + 1];
	strcpy(answer, inbuf);
	inmax = INPUT_MAX;
	input_clear();
	basic_input(answer);
}

/* ------------------------------------------------------------------ */
/* help page (SELECT)                                                  */
/* ------------------------------------------------------------------ */

#define HELP_ROWS (ROWS - 2)
static Screen helpScr;
static bool helpOpen;
static int helpTop;

/* draw UTF-8 text, mapping German umlauts to the CP437 font */
static int help_text(int x, int y, const char *t, u8 fg, u8 bg) {
	while (*t && x < COLS) {
		unsigned char c = (unsigned char)*t++;
		if (c == 0xC3 && *t) {
			switch ((unsigned char)*t++) {
			case 0xA4: c = 0x84; break;  /* ä */
			case 0xB6: c = 0x94; break;  /* ö */
			case 0xBC: c = 0x81; break;  /* ü */
			case 0x84: c = 0x8E; break;  /* Ä */
			case 0x96: c = 0x99; break;  /* Ö */
			case 0x9C: c = 0x9A; break;  /* Ü */
			case 0x9F: c = 0xE1; break;  /* ß */
			default:   c = '?';
			}
		}
		scr_put(&helpScr, x++, y, (char)c, fg, bg);
	}
	return x;
}

static int help_max_top(void) {
	int m = helpLineCount - HELP_ROWS;
	return m > 0 ? m : 0;
}

static void help_scroll(int d) {
	helpTop += d;
	if (helpTop > help_max_top()) helpTop = help_max_top();
	if (helpTop < 0) helpTop = 0;
}

static void render_help(void) {
	char buf[40];
	scr_clear(&helpScr, TEXTCOL);
	for (int x = 0; x < COLS; x++) {
		scr_put(&helpScr, x, 0, ' ', BGCOL, LBLUE);
		scr_put(&helpScr, x, ROWS - 1, ' ', WHITE, DGREY);
	}
	help_text(1, 0, "HILFE - BEFEHLE", BGCOL, LBLUE);
	int pages = (helpLineCount + HELP_ROWS - 1) / HELP_ROWS;
	int page = (helpTop + HELP_ROWS - 1) / HELP_ROWS + 1;
	if (helpTop >= help_max_top()) page = pages;
	sprintf(buf, "%d/%d", page, pages);
	help_text(COLS - 1 - strlen(buf), 0, buf, BGCOL, LBLUE);
	help_text(0, ROWS - 1, "\x18\x19\x1b\x1a" "BL\xc3\xa4TTERN  SELECT/B:ZUR\xc3\x9c" "CK", WHITE, DGREY);

	for (int r = 0; r < HELP_ROWS; r++) {
		int i = helpTop + r;
		if (i >= helpLineCount) break;
		const char *t = helpText[i];
		int y = 1 + r;
		if (t[0] == '#') {
			for (int x = 0; x < COLS; x++) scr_put(&helpScr, x, y, ' ', WHITE, DGREY);
			help_text(1, y, t + 1, WHITE, DGREY);
		} else if (!strncmp(t, "@COLORS", 7)) {
			int first = t[7] == '8' ? 8 : 0;
			for (int c = first; c < first + 8; c++) {
				int x = (c - first) * 4;
				scr_put(&helpScr, x, y, (char)0xDB, c, NOFILL);
				scr_put(&helpScr, x + 1, y, (char)0xDB, c, NOFILL);
				sprintf(buf, "%-2d", c);
				help_text(x + 2, y, buf, LGREY, NOFILL);
			}
		} else {
			const char *bar = strchr(t, '|');
			if (!bar) { help_text(0, y, t, TEXTCOL, NOFILL); continue; }
			int len = bar - t;
			if (len > 31) len = 31;
			memcpy(buf, t, len);
			buf[len] = 0;
			int x = len ? help_text(0, y, buf, YELLOW, NOFILL) + 1 : 2;
			help_text(x, y, bar + 1, TEXTCOL, NOFILL);
		}
	}
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

static void video_init(void) {
	videoSetMode(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	lcdMainOnTop();

	static PrintConsole con;
	consoleInit(&con, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	int bg1 = bgInit(1, BgType_Text4bpp, BgSize_T_256x256, 30, 1);
	bgSetPriority(con.bgId, 0);
	bgSetPriority(bg1, 1);
	bgShow(con.bgId);
	bgShow(bg1);
	map0 = bgGetMapPtr(con.bgId);
	map1 = bgGetMapPtr(bg1);

	/* tile 0 transparent, tile 1 solid colour index 1 */
	u32 *tiles = (u32 *)bgGetGfxPtr(bg1);
	for (int i = 0; i < 8; i++) { tiles[i] = 0; tiles[8 + i] = 0x11111111; }

	for (int p = 0; p < 16; p++) {
		BG_PALETTE[p * 16 + 1] = palette[p];
		BG_PALETTE[p * 16 + 15] = palette[p];
	}
	BG_PALETTE[0] = palette[BGCOL];
}

static void update_status_panel(void) {
	BasicState st = basic_state();
	char l1[40];
	if (helpOpen) {
		kbd_set_status("HILFE", "\x18\x19\x1b\x1a:BL\x84TTERN SEL:ZUR\x9a" "CK");
	} else if (st == BASIC_RUNNING) {
		int ln = basic_current_line();
		if (ln >= 0) sprintf(l1, "RUNNING... LINE %d", ln);
		else strcpy(l1, "RUNNING...");
		kbd_set_status(l1, "STOP/START = BREAK");
	} else if (st == BASIC_INPUT) {
		kbd_set_status("INPUT - TYPE ANSWER + RET", "STOP/START = BREAK");
	} else if (waitReturn) {
		kbd_set_status(*basic_last_error() ? basic_last_error() : "PROGRAM ENDED.",
		               "TAP ANY KEY TO RETURN");
	} else if (view == VIEW_OUTPUT) {
		kbd_set_status("OUTPUT SCREEN", "SCRN = EDITOR  SELECT = HILFE");
	} else {
		kbd_set_status(statusMsg, "A:EDIT START:RUN SELECT:HILFE");
	}
}

int main(int argc, char *argv[]) {
	(void)argc; (void)argv;
	video_init();
	kbd_init();
	keysSetRepeat(20, 4);

	scr_clear(&term, TEXTCOL);
	term_puts("**** NDS BASIC V1.0 ****");
	term_newline();
	term_newline();
	basic_init(&bio);

	while (pmMainLoop()) {
		/* --- execute the program, using the whole frame --- */
		if (basic_state() == BASIC_RUNNING) {
			while ((REG_DISPSTAT & DISP_IN_VBLANK) && basic_state() == BASIC_RUNNING)
				basic_run_slice(8);
			while (!(REG_DISPSTAT & DISP_IN_VBLANK) && basic_state() == BASIC_RUNNING)
				basic_run_slice(8);
			if (basic_state() != BASIC_RUNNING) swiWaitForVBlank();
		} else {
			swiWaitForVBlank();
		}
		frameCount++;
		basic_frame();

		/* program reached an INPUT statement? */
		static bool inInput;
		if (basic_state() == BASIC_INPUT) {
			if (!inInput) { begin_input_statement(); inInput = true; }
		} else inInput = false;

		BasicState st = basic_state();
		kbd_set_running(st != BASIC_IDLE);

		/* --- input --- */
		scanKeys();
		u32 down = keysDown(), rep = keysDownRepeat(), held = keysHeld();
		touchPosition tp = { 0 };
		bool touching = (held & KEY_TOUCH) && touchRead(&tp);
		int k = kbd_poll(touching, tp.px, tp.py, (held & (KEY_L | KEY_R)) != 0);

		if (down & KEY_START) k = KC_RUN;
		if (rep & KEY_LEFT) k = KC_LEFT;
		if (rep & KEY_RIGHT) k = KC_RIGHT;
		if (rep & KEY_B) k = KC_DEL;

		if (helpOpen) {
			if (rep & KEY_UP) help_scroll(-1);
			if (rep & KEY_DOWN) help_scroll(1);
			if (k == KC_LEFT) help_scroll(-HELP_ROWS);
			else if (k == KC_RIGHT) help_scroll(HELP_ROWS);
			else if (k || (down & (KEY_SELECT | KEY_A | KEY_X | KEY_Y))) {
				helpOpen = false;
				if (k == KC_RUN) start_exec("RUN", true);
			}
		} else if ((down & KEY_SELECT) && st == BASIC_IDLE) {
			helpOpen = true;
			waitReturn = false;
			view = VIEW_EDIT;
		} else if (st != BASIC_IDLE) {
			if (k == KC_RUN) {
				basic_break();
			} else if (st == BASIC_INPUT) {
				if (k && input_key(k)) { inInput = false; end_input_statement(); }
			} else if (k >= 32 && k < 127) {
				keyq_push(k);
			} else if (k == KC_RET) {
				keyq_push(13);
			}
		} else if (waitReturn) {
			if (k || (down & (KEY_A | KEY_B | KEY_X | KEY_Y))) {
				waitReturn = false;
				view = VIEW_EDIT;
				if (k == KC_RUN) start_exec("RUN", true);
			}
		} else if (view == VIEW_OUTPUT) {
			if (k == KC_RUN) start_exec("RUN", true);
			else if (k) view = VIEW_EDIT;
		} else {
			/* editor */
			int n = basic_line_count();
			if ((rep & KEY_UP) && n) { sel = sel <= 0 ? 0 : sel - 1; ensure_visible(); }
			if ((rep & KEY_DOWN) && n) { sel = sel < n - 1 ? sel + 1 : n - 1; ensure_visible(); }
			if ((down & KEY_A) && sel >= 0 && sel < n) {
				char line[300];
				snprintf(line, sizeof line, "%d %s", basic_line_number(sel), basic_line_text(sel));
				input_set(line);
			}
			if (down & KEY_Y) input_clear();
			if (k == KC_RUN) start_exec("RUN", true);
			else if (k == KC_SCRN) view = VIEW_OUTPUT;
			else if (k && input_key(k)) submit_edit_line();
		}
		/* program just finished? */
		static BasicState prevState;
		BasicState now = basic_state();
		if (now == BASIC_IDLE && prevState != BASIC_IDLE) finish_exec();
		prevState = now;

		/* --- render top screen --- */
		bool blink = (frameCount & 32) == 0;
		int cx = -1, cy = -1;
		if (helpOpen) {
			render_help();
			scr_upload(&helpScr, -1, -1);
		} else if (view == VIEW_EDIT) {
			render_edit(&cx, &cy);
			if (!blink) cx = -1;
			scr_upload(&edit, cx, cy);
		} else {
			if (basic_state() == BASIC_INPUT) {
				input_draw(&term, inStartX, inStartY, inmax, &cx, &cy);
				if (!blink) cx = -1;
			}
			scr_upload(&term, cx, cy);
		}
		update_status_panel();
	}
	return 0;
}
