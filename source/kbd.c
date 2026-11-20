/* kbd.c - retro on-screen keyboard, drawn into the sub engine bitmap */
#include <nds.h>
#include <string.h>
#include "kbd.h"
#include "draw.h"


/* palette: home computer "breadbin" look */
#define COL_LCD       C(3, 7, 3)
#define COL_LCD_EDGE  C(1, 2, 1)
#define COL_LCD_TEXT  C(10, 29, 10)
#define COL_SHLABEL   C(26, 19, 8)

typedef enum { KT_CHAR, KT_SHIFT, KT_SPECIAL } KeyType;
typedef enum { ST_NORMAL, ST_FUNC, ST_RED, ST_GREY } KeyStyle;

typedef struct {
	const char *label;   /* label for special keys          */
	char ch, sch;        /* normal / shifted char           */
	u8 type, style, units, code;
	/* computed */
	short x, y, w, h;
} Key;

#define K(c, s)            { NULL, c, s, KT_CHAR, ST_NORMAL, 1, 0, 0, 0, 0, 0 }
#define S(l, st, u, code)  { l, 0, 0, KT_SPECIAL, st, u, code, 0, 0, 0, 0 }

static Key keys[] = {
	/* row 0 */
	K('1','!'), K('2','"'), K('3','#'), K('4','$'), K('5','%'), K('6','&'),
	K('7','\''), K('8','('), K('9',')'), K('0','@'), K('-','_'),
	S("DEL", ST_FUNC, 2, KC_DEL),
	/* row 1 */
	K('Q','q'), K('W','w'), K('E','e'), K('R','r'), K('T','t'), K('Y','y'),
	K('U','u'), K('I','i'), K('O','o'), K('P','p'), K('+','*'), K('=','^'),
	K('/','?'),
	/* row 2 */
	K('A','a'), K('S','s'), K('D','d'), K('F','f'), K('G','g'), K('H','h'),
	K('J','j'), K('K','k'), K('L','l'), K(':',';'), K(',','\''),
	S("RET", ST_FUNC, 2, KC_RET),
	/* row 3 */
	{ "SHFT", 0, 0, KT_SHIFT, ST_FUNC, 2, 0, 0, 0, 0, 0 },
	K('Z','z'), K('X','x'), K('C','c'), K('V','v'), K('B','b'), K('N','n'),
	K('M','m'), K('<','>'), K('.',','),
	S("\x1b", ST_GREY, 1, KC_LEFT), S("\x1a", ST_GREY, 1, KC_RIGHT),
	/* row 4 */
	S("RUN", ST_RED, 3, KC_RUN), S("SCRN", ST_GREY, 2, KC_SCRN),
	S("", ST_NORMAL, 5, ' '), S("CLR", ST_GREY, 3, KC_CLR),
};
#define NKEYS ((int)(sizeof(keys) / sizeof(keys[0])))
static const u8 rowLen[] = { 12, 13, 12, 12, 4 };

#define KEY_PITCH 19
#define ROW_PITCH 26
#define KEY_H     24
#define KB_X      4
#define KB_Y      62

static bool shift, shiftLock, running, extHeld;
static int pressed = -1, holdFrames, lastShiftTap = -100, frame;

#define fill  draw_fill
#define glyph draw_glyph
#define text  draw_text

/* ---------------- keys ---------------- */

static void style_colors(const Key *k, u16 *face, u16 *hi, u16 *side) {
	switch (k->style) {
	case ST_FUNC: *face = C(21, 10, 4);  *hi = C(26, 15, 7);  *side = C(11, 5, 2); break;
	case ST_RED:  *face = C(21, 4, 4);   *hi = C(27, 9, 8);   *side = C(11, 2, 2); break;
	case ST_GREY: *face = C(14, 13, 12); *hi = C(19, 18, 17); *side = C(7, 7, 6);  break;
	default:      *face = C(10, 8, 6);   *hi = C(15, 12, 9);  *side = C(5, 4, 3);  break;
	}
	if (k->type == KT_SHIFT && shift) { *face = C(28, 21, 6); *hi = C(31, 27, 12); }
	if (k->code == KC_RUN && running) { *face = C(28, 6, 4); }
}

static void draw_key(int i) {
	const Key *k = &keys[i];
	bool down = (i == pressed);
	u16 face, hi, side;
	style_colors(k, &face, &hi, &side);

	int off = down ? 2 : 0;
	fill(k->x, k->y, k->w, k->h, COL_BORDER);
	fill(k->x + 1, k->y + 1, k->w - 2, k->h - 2, side);
	fill(k->x + 2, k->y + 1 + off, k->w - 4, k->h - 6, down ? side : face);
	if (!down) {
		fill(k->x + 2, k->y + 1, k->w - 4, 1, hi);
		fill(k->x + 2, k->y + 1, 1, k->h - 6, hi);
	} else {
		fill(k->x + 3, k->y + 2 + off, k->w - 6, k->h - 8, face);
	}

	int cx = k->x + k->w / 2, ty = k->y + off;
	if (k->type == KT_CHAR) {
		bool letter = (k->ch >= 'A' && k->ch <= 'Z');
		if (letter) {
			char c = shift ? k->sch : k->ch;
			glyph(cx - 4, ty + 7, (unsigned char)c, COL_LABEL, 1);
		} else {
			glyph(cx - 4, ty + 2, (unsigned char)k->sch, shift ? COL_LABEL : COL_SHLABEL, 1);
			glyph(cx - 4, ty + 11, (unsigned char)k->ch, shift ? COL_SHLABEL : COL_LABEL, 1);
		}
	} else {
		const char *l = k->label;
		if (k->code == KC_RUN && running) l = "STOP";
		int len = strlen(l);
		text(cx - len * 4, ty + 7, l, COL_LABEL, 1);
	}
}

static void draw_all_keys(void) {
	for (int i = 0; i < NKEYS; i++) draw_key(i);
}

static void layout(void) {
	int i = 0;
	for (int r = 0; r < 5; r++) {
		int x = KB_X;
		for (int n = 0; n < rowLen[r]; n++, i++) {
			keys[i].x = x;
			keys[i].y = KB_Y + r * ROW_PITCH;
			keys[i].w = keys[i].units * KEY_PITCH - 2;
			keys[i].h = KEY_H;
			x += keys[i].units * KEY_PITCH;
		}
	}
}

static void draw_frame(void) {
	fill(0, 0, 256, 192, COL_BODY);
	/* dark title band with rainbow stripes */
	fill(0, 0, 256, 22, COL_BAND);
	static const u16 stripes[] = { C(28, 4, 4), C(30, 16, 2), C(30, 28, 4), C(6, 22, 6), C(6, 12, 28) };
	for (int s = 0; s < 5; s++)
		for (int y = 0; y < 22; y++)
			fill(176 + s * 12 + (21 - y) / 2, y, 8, 1, stripes[s]);
	text(8, 3, "NDS BASIC", COL_TITLE, 2);
	/* recessed area behind keys */
	fill(0, KB_Y - 4, 256, 192 - KB_Y + 4, COL_BODY_DARK);
	fill(0, KB_Y - 4, 256, 1, COL_BORDER);
}

static char status1[33] = "READY.", status2[33] = "";

static void draw_status(void) {
	fill(4, 26, 248, 30, COL_LCD_EDGE);
	fill(5, 27, 246, 28, COL_LCD);
	text(9, 30, status1, COL_LCD_TEXT, 1);
	text(9, 42, status2, COL_LCD_TEXT, 1);
}

void kbd_set_status(const char *l1, const char *l2) {
	if (!strncmp(l1, status1, 30) && !strncmp(l2, status2, 30)) return;
	strncpy(status1, l1, 30); status1[30] = 0;
	strncpy(status2, l2, 30); status2[30] = 0;
	draw_status();
}

void kbd_set_running(bool r) {
	if (r == running) return;
	running = r;
	for (int i = 0; i < NKEYS; i++)
		if (keys[i].code == KC_RUN) draw_key(i);
}

void kbd_redraw(void) {
	pressed = -2;   /* ignore a stylus that is still down */
	draw_frame();
	draw_status();
	draw_all_keys();
}

void kbd_init(void) {
	videoSetModeSub(MODE_5_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	int bg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
	draw_init(bgGetGfxPtr(bg));
	layout();
	draw_frame();
	draw_status();
	draw_all_keys();
}

static int hit(int px, int py) {
	for (int i = 0; i < NKEYS; i++) {
		const Key *k = &keys[i];
		if (px >= k->x - 1 && px <= k->x + k->w && py >= k->y - 1 && py <= k->y + k->h) return i;
	}
	return -1;
}

static void set_shift(bool s) {
	if (s == shift) return;
	shift = s;
	draw_all_keys();
}

static int emit(int i) {
	const Key *k = &keys[i];
	if (k->type == KT_SHIFT) {
		/* tap = one shot shift, double tap = shift lock */
		if (shift && frame - lastShiftTap < 20) { shiftLock = true; }
		else if (shift) { shiftLock = false; set_shift(false); }
		else set_shift(true);
		lastShiftTap = frame;
		draw_key(i);
		return KC_NONE;
	}
	if (k->type == KT_CHAR) {
		int c = shift ? k->sch : k->ch;
		if (!shiftLock && !extHeld) set_shift(false);
		return c;
	}
	return k->code;
}

static bool repeatable(int i) {
	const Key *k = &keys[i];
	return k->type == KT_CHAR || k->code == KC_DEL || k->code == KC_LEFT ||
	       k->code == KC_RIGHT || k->code == ' ';
}

int kbd_poll(bool touching, int px, int py, bool extShift) {
	frame++;
	if (extShift != extHeld) {
		extHeld = extShift;
		if (!shiftLock) set_shift(extShift);
	}
	int result = KC_NONE;
	if (touching) {
		int k = hit(px, py);
		if (pressed == -1) {
			if (k >= 0) {
				pressed = k;
				holdFrames = 0;
				draw_key(k);
				result = emit(k);
			} else pressed = -2;    /* touched outside: ignore until release */
		} else if (pressed >= 0) {
			if (k == pressed) {
				holdFrames++;
				if (repeatable(k) && holdFrames > 24 && (holdFrames - 24) % 4 == 0) result = emit(k);
			} else {
				int old = pressed;
				pressed = -2;
				draw_key(old);
			}
		}
	} else if (pressed != -1) {
		int old = pressed;
		pressed = -1;
		if (old >= 0) draw_key(old);
	}
	return result;
}
