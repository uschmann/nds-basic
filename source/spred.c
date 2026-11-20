/*
 * spred.c - 8x8 sprite editor on the bottom screen
 *
 *  +--------------------------------------------+
 *  | SPRITE EDITOR                    SLOT 012  |
 *  | +----------------+  [palette 6x3]          |
 *  | |  8x8 grid,     |  [preview 4x 2x 1x]     |
 *  | |  20px cells    |  [<]  [>]               |
 *  | |                |  [CPY][PST]             |
 *  | +----------------+  [CLR][OK]              |
 *  +--------------------------------------------+
 */
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include "basic.h"
#include "draw.h"
#include "spred.h"

#define GX   6          /* grid origin and cell size */
#define GY   26
#define CELL 20
#define PX   174        /* right panel */
#define PALY 26
#define PAL_W 12
#define PAL_H 14
#define PREVY 74

#define COL_CHECK1 C(20, 20, 20)
#define COL_CHECK2 C(14, 14, 14)
#define COL_SEL    C(31, 31, 31)

typedef struct { short x, y, w, h; const char *label; } Btn;
enum { B_PREV, B_NEXT, B_COPY, B_PASTE, B_CLEAR, B_OK, NBTN };
static const Btn btns[NBTN] = {
	{ PX,      112, 37, 18, "\x1b" }, { PX + 39, 112, 37, 18, "\x1a" },
	{ PX,      134, 37, 18, "CPY" },  { PX + 39, 134, 37, 18, "PST" },
	{ PX,      156, 37, 18, "CLR" },  { PX + 39, 156, 37, 18, "OK" },
};

static int slot, color = 1, pressedBtn = -1;
static bool penDown, ignoreTouch, hasClip;
static signed char clip[64];

static u16 col16(int c) { return basicPalette[c] | BIT(15); }

/* transparent pixels are shown as a checkerboard */
static void draw_px(int x, int y, int size, int c) {
	if (c >= 0) { draw_fill(x, y, size, size, col16(c)); return; }
	int h = size / 2 ? size / 2 : 1;
	for (int j = 0; j < size; j += h)
		for (int i = 0; i < size; i += h)
			draw_fill(x + i, y + j, i + h > size ? size - i : h, j + h > size ? size - j : h,
			          ((i + j) / h) & 1 ? COL_CHECK2 : COL_CHECK1);
}

static void draw_cell(int px, int py) {
	draw_px(GX + px * CELL, GY + py * CELL, CELL - 1, basic_sprite_get(slot, px, py));
}

static void draw_preview(void) {
	draw_fill(PX - 1, PREVY - 1, 78, 34, COL_BORDER);
	draw_fill(PX, PREVY, 76, 32, COL_BODY_DARK);
	static const int scales[3] = { 4, 2, 1 }, xs[3] = { PX, PX + 40, PX + 64 };
	for (int s = 0; s < 3; s++) {
		int sc = scales[s], ox = xs[s], oy = PREVY + 16 - 4 * sc;
		for (int y = 0; y < 8; y++)
			for (int x = 0; x < 8; x++) {
				int c = basic_sprite_get(slot, x, y);
				if (c >= 0) draw_fill(ox + x * sc, oy + y * sc, sc, sc, col16(c));
				else draw_fill(ox + x * sc, oy + y * sc, sc, sc, ((x + y) & 1) ? COL_CHECK2 : COL_CHECK1);
			}
	}
}

static void draw_palette(void) {
	for (int i = 0; i < 17; i++) {
		int c = i - 1;                       /* swatch 0 = transparent */
		int x = PX + (i % 6) * PAL_W, y = PALY + (i / 6) * PAL_H;
		bool sel = (c == color);
		draw_fill(x, y, PAL_W, PAL_H, sel ? COL_SEL : COL_BORDER);
		if (c >= 0) draw_fill(x + 2, y + 2, PAL_W - 4, PAL_H - 4, col16(c));
		else {
			draw_fill(x + 2, y + 2, PAL_W - 4, PAL_H - 4, COL_CHECK1);
			draw_fill(x + 2, y + 2, (PAL_W - 4) / 2, (PAL_H - 4) / 2, COL_CHECK2);
			draw_fill(x + 2 + (PAL_W - 4) / 2, y + 2 + (PAL_H - 4) / 2, (PAL_W - 4) / 2, (PAL_H - 4) / 2, COL_CHECK2);
		}
	}
}

static void draw_btn(int i) {
	const Btn *b = &btns[i];
	u16 face = i == B_OK ? C(21, 10, 4) : C(14, 13, 12);
	draw_button(b->x, b->y, b->w, b->h, b->label, face, i == pressedBtn);
}

static void draw_header(void) {
	char buf[16];
	draw_fill(0, 0, 256, 20, COL_BAND);
	draw_text(6, 6, "SPRITE EDITOR", COL_TITLE, 1);
	sprintf(buf, "SLOT %03d", slot);
	draw_text(250 - 8 * 8, 6, buf, COL_TITLE, 1);
}

static void draw_slot(void) {
	draw_header();
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++) draw_cell(x, y);
	draw_preview();
}

static void draw_all(void) {
	draw_fill(0, 0, 256, 192, COL_BODY);
	draw_fill(GX - 1, GY - 1, 8 * CELL + 1, 8 * CELL + 1, COL_BORDER);
	draw_palette();
	for (int i = 0; i < NBTN; i++) draw_btn(i);
	draw_slot();
}

void spred_open(int s) {
	slot = s & 255;
	pressedBtn = -1;
	penDown = false;
	ignoreTouch = true;     /* the touch that opened us (if any) */
	draw_all();
}

int spred_slot(void) { return slot; }

void spred_set_slot(int s) {
	slot = (s % BASIC_SPRITES + BASIC_SPRITES) % BASIC_SPRITES;
	draw_slot();
}

static int paint_at(int px, int py) {
	int x = (px - GX) / CELL, y = (py - GY) / CELL;
	if (px < GX || py < GY || x > 7 || y > 7) return SPRED_NONE;
	if (basic_sprite_get(slot, x, y) == color) return SPRED_NONE;
	basic_sprite_set(slot, x, y, color);
	draw_cell(x, y);
	draw_preview();
	return SPRED_CHANGED;
}

static int press_button(int i) {
	int y, x;
	switch (i) {
	case B_PREV: spred_set_slot(slot - 1); return SPRED_SLOT;
	case B_NEXT: spred_set_slot(slot + 1); return SPRED_SLOT;
	case B_COPY:
		for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) clip[y * 8 + x] = (signed char)basic_sprite_get(slot, x, y);
		hasClip = true;
		return SPRED_NONE;
	case B_PASTE:
		if (!hasClip) return SPRED_NONE;
		for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) basic_sprite_set(slot, x, y, clip[y * 8 + x]);
		draw_slot();
		return SPRED_CHANGED;
	case B_CLEAR:
		for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) basic_sprite_set(slot, x, y, -1);
		draw_slot();
		return SPRED_CHANGED;
	case B_OK: return SPRED_EXIT;
	}
	return SPRED_NONE;
}

int spred_poll(bool touching, int px, int py) {
	if (!touching) {
		ignoreTouch = false;
		penDown = false;
		if (pressedBtn >= 0) { int b = pressedBtn; pressedBtn = -1; draw_btn(b); }
		pressedBtn = -1;
		return SPRED_NONE;
	}
	if (ignoreTouch) return SPRED_NONE;
	if (penDown) return paint_at(px, py);
	if (pressedBtn != -1) return SPRED_NONE;         /* wait for release */

	/* new touch */
	if (px >= GX && py >= GY && px < GX + 8 * CELL && py < GY + 8 * CELL) {
		penDown = true;
		return paint_at(px, py);
	}
	if (px >= PX && px < PX + 6 * PAL_W && py >= PALY && py < PALY + 3 * PAL_H) {
		int i = ((py - PALY) / PAL_H) * 6 + (px - PX) / PAL_W;
		if (i < 17) { color = i - 1; draw_palette(); }
		pressedBtn = -2;                              /* consume until release */
		return SPRED_NONE;
	}
	for (int i = 0; i < NBTN; i++) {
		const Btn *b = &btns[i];
		if (px >= b->x && px < b->x + b->w && py >= b->y && py < b->y + b->h) {
			pressedBtn = i;
			draw_btn(i);
			return press_button(i);
		}
	}
	pressedBtn = -2;
	return SPRED_NONE;
}
