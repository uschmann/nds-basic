/* draw.c - primitives for the 16-bit bitmap on the bottom screen */
#include <string.h>
#include "draw.h"

static u16 *fb;
static const u8 *font;

void draw_init(u16 *framebuffer) {
	fb = framebuffer;
	font = (const u8 *)consoleGetDefault()->font.gfx;   /* 1bpp, LSB = left */
}

void draw_fill(int x, int y, int w, int h, u16 c) {
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > 256) w = 256 - x;
	if (y + h > 192) h = 192 - y;
	for (int j = 0; j < h; j++) {
		u16 *p = fb + (y + j) * 256 + x;
		for (int i = 0; i < w; i++) p[i] = c;
	}
}

void draw_glyph(int x, int y, unsigned char ch, u16 c, int scale) {
	const u8 *g = font + ch * 8;
	for (int r = 0; r < 8; r++)
		for (int b = 0; b < 8; b++)
			if (g[r] & (1 << b)) draw_fill(x + b * scale, y + r * scale, scale, scale, c);
}

void draw_text(int x, int y, const char *s, u16 c, int scale) {
	for (; *s; s++, x += 8 * scale) draw_glyph(x, y, (unsigned char)*s, c, scale);
}

static u16 shade(u16 c, int d) {
	int r = (c & 31) + d, g = ((c >> 5) & 31) + d, b = ((c >> 10) & 31) + d;
	r = r < 0 ? 0 : r > 31 ? 31 : r;
	g = g < 0 ? 0 : g > 31 ? 31 : g;
	b = b < 0 ? 0 : b > 31 ? 31 : b;
	return C(r, g, b);
}

void draw_button(int x, int y, int w, int h, const char *label, u16 face, bool down) {
	u16 hi = shade(face, 5), side = shade(face, -9);
	int off = down ? 2 : 0;
	draw_fill(x, y, w, h, COL_BORDER);
	draw_fill(x + 1, y + 1, w - 2, h - 2, side);
	draw_fill(x + 2, y + 1 + off, w - 4, h - 5, face);
	if (!down) {
		draw_fill(x + 2, y + 1, w - 4, 1, hi);
		draw_fill(x + 2, y + 1, 1, h - 5, hi);
	}
	int len = strlen(label);
	draw_text(x + w / 2 - len * 4, y + (h - 8) / 2 - 1 + off, label, COL_LABEL, 1);
}
