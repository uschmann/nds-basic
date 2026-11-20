/*
 * snded.c - sound editor on the bottom screen
 *
 *  | SOUND EDITOR                       SLOT 0 |
 *  | [<][>] [SQ50] [-]120[+]           [PLAY]  |
 *  | [16][8][4][2][1]   [O-] O4 [O+]           |
 *  | [REST][DEL][CLR]                  [OK]    |
 *  | LEN 1/4   OCT 4   NOTES 12/64             |
 *  | piano keyboard, 2 octaves                 |
 *
 * Tapping a piano key plays the note and appends it with the selected
 * length to the current slot.
 */
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include "basic.h"
#include "draw.h"
#include "sound.h"
#include "snded.h"

#define PIANO_X 2
#define PIANO_Y 114
#define PIANO_H 76
#define WHITE_W 18
#define BLACK_W 12
#define BLACK_H 46
#define INFO_Y  92

#define COL_GREY   COL_KEY_FN
#define COL_ORANGE COL_ACCENT
#define COL_RED    COL_ACCENT
#define COL_SELECT COL_TINT
#define COL_LCD    COL_CARD
#define COL_LCDTXT COL_TEXT
#define COL_WHITEKEY COL_CARD
#define COL_BLACKKEY C(5, 5, 5)
#define COL_KEYDOWN  COL_ACCENT

enum {
	B_PREV, B_NEXT, B_INST, B_TDOWN, B_TUP, B_PLAY,
	B_L16, B_L8, B_L4, B_L2, B_L1, B_ODOWN, B_OUP,
	B_REST, B_DEL, B_CLR, B_OK, NBTN
};
typedef struct { short x, y, w, h; } Rect;
static const Rect btn[NBTN] = {
	{ 4, 24, 26, 18 }, { 32, 24, 26, 18 }, { 62, 24, 52, 18 },
	{ 118, 24, 22, 18 }, { 176, 24, 22, 18 }, { 206, 24, 46, 18 },
	{ 4, 46, 26, 18 }, { 32, 46, 26, 18 }, { 60, 46, 26, 18 }, { 88, 46, 26, 18 }, { 116, 46, 26, 18 },
	{ 150, 46, 26, 18 }, { 226, 46, 26, 18 },
	{ 4, 68, 48, 18 }, { 56, 68, 40, 18 }, { 100, 68, 40, 18 }, { 206, 68, 46, 18 },
};
static const int lengths[5] = { 1, 2, 4, 8, 16 };
static const char *lenNames[5] = { "1/16", "1/8", "1/4", "1/2", "1/1" };

static int slot, octave = 4, lenSel = 2, pressedBtn = -1, pressedKey = -1;
static bool ignoreTouch, wasPlaying;

/* ---------------- piano geometry ---------------- */

static const int whiteSemi[7] = { 0, 2, 4, 5, 7, 9, 11 };
/* black key after white key w (in octave), -1 = none */
static const int blackAfter[7] = { 1, 3, -1, 6, 8, 10, -1 };

static void white_rect(int w, Rect *r) {
	r->x = PIANO_X + w * WHITE_W; r->y = PIANO_Y; r->w = WHITE_W - 1; r->h = PIANO_H;
}
static void black_rect(int w, Rect *r) {
	r->x = PIANO_X + (w + 1) * WHITE_W - BLACK_W / 2 - 1; r->y = PIANO_Y; r->w = BLACK_W; r->h = BLACK_H;
}
/* key index k: semitone 0..23 relative to C of the base octave */
static int key_at(int px, int py) {
	if (py < PIANO_Y || py >= PIANO_Y + PIANO_H) return -1;
	Rect r;
	for (int w = 0; w < 14; w++) {
		int b = blackAfter[w % 7];
		if (b < 0) continue;
		black_rect(w, &r);
		if (px >= r.x && px < r.x + r.w && py < r.y + r.h) return (w / 7) * 12 + b;
	}
	int w = (px - PIANO_X) / WHITE_W;
	if (px < PIANO_X || w > 13) return -1;
	return (w / 7) * 12 + whiteSemi[w % 7];
}

static void draw_piano(void) {
	Rect r;
	draw_fill(0, PIANO_Y - 2, 256, 192 - PIANO_Y + 2, COL_BORDER);
	for (int w = 0; w < 14; w++) {
		int k = (w / 7) * 12 + whiteSemi[w % 7];
		white_rect(w, &r);
		draw_fill(r.x, r.y, r.w, r.h, k == pressedKey ? COL_KEYDOWN : COL_WHITEKEY);
		draw_fill(r.x, r.y + r.h - 4, r.w, 4, COL_LINE);
		if (w % 7 == 0) {               /* octave label on C */
			char b[4];
			sprintf(b, "C%d", octave + w / 7);
			draw_text(r.x + 1, r.y + r.h - 14, b, COL_MUTED, 1);
		}
	}
	for (int w = 0; w < 14; w++) {
		int b = blackAfter[w % 7];
		if (b < 0) continue;
		int k = (w / 7) * 12 + b;
		black_rect(w, &r);
		draw_fill(r.x, r.y, r.w, r.h, k == pressedKey ? COL_KEYDOWN : COL_BLACKKEY);
		draw_fill(r.x + 2, r.y + r.h - 5, r.w - 4, 3, C(9, 8, 8));
	}
}

/* ---------------- buttons / info ---------------- */

static void draw_btn(int i) {
	const Rect *b = &btn[i];
	char label[12];
	u16 face = COL_GREY;
	BasicSound *s = basic_sound(slot);
	switch (i) {
	case B_PREV:  strcpy(label, "\x1b"); break;
	case B_NEXT:  strcpy(label, "\x1a"); break;
	case B_INST:  strcpy(label, snd_inst_name(s->inst)); break;
	case B_TDOWN: case B_ODOWN: strcpy(label, "-"); break;
	case B_TUP:   case B_OUP:   strcpy(label, "+"); break;
	case B_PLAY:  strcpy(label, snd_busy() ? "STOP" : "PLAY"); face = COL_RED; break;
	case B_REST:  strcpy(label, "REST"); break;
	case B_DEL:   strcpy(label, "DEL"); face = COL_ORANGE; break;
	case B_CLR:   strcpy(label, "CLR"); break;
	case B_OK:    strcpy(label, "OK"); face = COL_ORANGE; break;
	default: {
		int l = i - B_L16;
		sprintf(label, "%d", 16 / lengths[l]);
		if (l == lenSel) face = COL_SELECT;
	}
	}
	draw_button(b->x, b->y, b->w, b->h, label, face, i == pressedBtn);
}

static void lcd_box(int x, int y, int w, const char *t) {
	draw_fill(x, y, w, 18, COL_BORDER);
	draw_fill(x + 1, y + 1, w - 2, 16, COL_LCD);
	draw_text(x + w / 2 - (int)strlen(t) * 4, y + 5, t, COL_LCDTXT, 1);
}

static void draw_values(void) {
	char b[40];
	BasicSound *s = basic_sound(slot);
	sprintf(b, "%d", s->tempo);
	lcd_box(142, 24, 32, b);
	sprintf(b, "O%d", octave);
	lcd_box(178, 46, 46, b);
	draw_fill(4, INFO_Y - 2, 248, 20, COL_BORDER);
	draw_fill(5, INFO_Y - 1, 246, 18, COL_LCD);
	sprintf(b, "LEN %-4s BPM %3d  %2d/%d NOTES", lenNames[lenSel], s->tempo, s->count, BASIC_SOUND_LEN);
	draw_text(9, INFO_Y + 4, b, COL_LCDTXT, 1);
}

static void draw_header(void) {
	char b[16];
	draw_fill(0, 0, 256, 19, COL_PAGE);
	draw_fill(0, 19, 256, 1, COL_LINE);
	draw_text(6, 6, "SOUND EDITOR", COL_TEXT, 1);
	sprintf(b, "SLOT %d", slot);
	draw_text(250 - 8 * (int)strlen(b), 6, b, COL_ACCENT, 1);
}

static void draw_all(void) {
	draw_fill(0, 0, 256, 192, COL_BODY);
	draw_header();
	for (int i = 0; i < NBTN; i++) draw_btn(i);
	draw_values();
	draw_piano();
}

void snded_open(int s) {
	slot = (s % BASIC_SOUNDS + BASIC_SOUNDS) % BASIC_SOUNDS;
	pressedBtn = pressedKey = -1;
	ignoreTouch = true;
	wasPlaying = snd_busy();
	draw_all();
}

int snded_slot(void) { return slot; }
int snded_octave(void) { return octave; }

void snded_change_octave(int d) {
	octave += d;
	if (octave < 1) octave = 1;
	if (octave > 7) octave = 7;
	draw_values();
	draw_piano();
}

void snded_set_slot(int s) {
	slot = (s % BASIC_SOUNDS + BASIC_SOUNDS) % BASIC_SOUNDS;
	draw_header();
	draw_btn(B_INST);
	draw_values();
}

static int press(int i) {
	BasicSound *s = basic_sound(slot);
	switch (i) {
	case B_PREV: snded_set_slot(slot - 1); return SNDED_SLOT;
	case B_NEXT: snded_set_slot(slot + 1); return SNDED_SLOT;
	case B_INST: s->inst = (unsigned char)((s->inst + 1) % SND_INSTRUMENTS); draw_btn(B_INST); return SNDED_CHANGED;
	case B_TDOWN: if (s->tempo > 40) s->tempo -= 10; draw_values(); return SNDED_CHANGED;
	case B_TUP:   if (s->tempo < 250) s->tempo += 10; draw_values(); return SNDED_CHANGED;
	case B_PLAY:
		if (snd_busy()) snd_stop(); else snd_play(s);
		return SNDED_NONE;
	case B_ODOWN: if (octave > 1) octave--; draw_values(); draw_piano(); return SNDED_NONE;
	case B_OUP:   if (octave < 7) octave++; draw_values(); draw_piano(); return SNDED_NONE;
	case B_REST:
		if (s->count >= BASIC_SOUND_LEN) return SNDED_NONE;
		s->note[s->count] = 0;
		s->len[s->count++] = (unsigned char)lengths[lenSel];
		draw_values();
		return SNDED_CHANGED;
	case B_DEL: if (s->count) s->count--; draw_values(); return SNDED_CHANGED;
	case B_CLR: s->count = 0; snd_stop(); draw_values(); return SNDED_CHANGED;
	case B_OK: snd_stop(); return SNDED_EXIT;
	default: {
		int old = lenSel;
		lenSel = i - B_L16;
		draw_btn(B_L16 + old);
		draw_values();
		return SNDED_NONE;
	}
	}
}

int snded_poll(bool touching, int px, int py) {
	if (snd_busy() != wasPlaying) { wasPlaying = snd_busy(); draw_btn(B_PLAY); }
	if (!touching) {
		ignoreTouch = false;
		if (pressedBtn >= 0) { int b = pressedBtn; pressedBtn = -1; draw_btn(b); }
		if (pressedKey >= 0) { pressedKey = -1; draw_piano(); }
		pressedBtn = -1;
		return SNDED_NONE;
	}
	if (ignoreTouch || pressedBtn != -1 || pressedKey != -1) return SNDED_NONE;

	int k = key_at(px, py);
	if (k >= 0) {
		BasicSound *s = basic_sound(slot);
		int note = (octave + 1) * 12 + k;
		if (note > 108) note = 108;
		pressedKey = k;
		draw_piano();
		snd_preview(note, s->inst, 12);
		if (s->count >= BASIC_SOUND_LEN) return SNDED_NONE;
		s->note[s->count] = (unsigned char)note;
		s->len[s->count++] = (unsigned char)lengths[lenSel];
		draw_values();
		return SNDED_CHANGED;
	}
	for (int i = 0; i < NBTN; i++) {
		const Rect *b = &btn[i];
		if (px >= b->x && px < b->x + b->w && py >= b->y && py < b->y + b->h) {
			pressedBtn = i;
			int r = press(i);
			draw_btn(i);
			return r;
		}
	}
	pressedBtn = -2;
	return SNDED_NONE;
}
