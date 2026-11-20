/* draw.h - primitives for the 16-bit bitmap on the bottom screen */
#ifndef DRAW_H
#define DRAW_H

#include <nds.h>

/* colour with the bitmap "opaque" bit set */
#define C(r, g, b) ((u16)(RGB15(r, g, b) | BIT(15)))

/* shared "breadbin" colours */
#define COL_BODY      C(22, 20, 16)
#define COL_BODY_DARK C(17, 15, 12)
#define COL_BAND      C(6, 5, 4)
#define COL_TITLE     C(30, 28, 22)
#define COL_BORDER    C(2, 2, 1)
#define COL_LABEL     C(30, 28, 24)

void draw_init(u16 *framebuffer);
void draw_fill(int x, int y, int w, int h, u16 c);
void draw_glyph(int x, int y, unsigned char ch, u16 c, int scale);
void draw_text(int x, int y, const char *s, u16 c, int scale);
/* raised button in the keyboard style; down = pressed look */
void draw_button(int x, int y, int w, int h, const char *label, u16 face, bool down);

#endif
