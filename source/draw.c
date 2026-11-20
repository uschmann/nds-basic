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

void draw_box(int x, int y, int w, int h, u16 face, u16 bg, bool down) {
	int off = down ? 1 : 0, bh = h - 2;
	u16 border = shade(face, -3);
	draw_fill(x, y, w, h, bg);
	if (!down) draw_fill(x + 1, y + 2, w - 2, bh, shade(face, -6));   /* shadow */
	draw_fill(x, y + off, w, bh, border);
	draw_fill(x + 1, y + 1 + off, w - 2, bh - 2, face);
	/* rounded corners */
	draw_fill(x, y + off, 1, 1, bg);
	draw_fill(x + w - 1, y + off, 1, 1, bg);
	draw_fill(x, y + off + bh - 1, 1, 1, down ? bg : shade(face, -6));
	draw_fill(x + w - 1, y + off + bh - 1, 1, 1, down ? bg : shade(face, -6));
}

u16 draw_label_color(u16 face) {
	int lum = (face & 31) + ((face >> 5) & 31) + ((face >> 10) & 31);
	return lum > 60 ? COL_TEXT : C(31, 31, 31);
}

void draw_button(int x, int y, int w, int h, const char *label, u16 face, bool down) {
	draw_box(x, y, w, h, face, COL_PAGE, down);
	int len = strlen(label);
	draw_text(x + w / 2 - len * 4, y + (h - 2 - 8) / 2 + (down ? 1 : 0), label, draw_label_color(face), 1);
}

void draw_page_header(int h) {
	draw_fill(0, 0, 256, h, COL_PAGE);
	int y = (h - 16) / 2;
	/* small accent tile with "<>" before the name */
	draw_fill(8, y, 16, 16, COL_ACCENT);
	draw_fill(8, y, 1, 1, COL_PAGE); draw_fill(23, y, 1, 1, COL_PAGE);
	draw_fill(8, y + 15, 1, 1, COL_PAGE); draw_fill(23, y + 15, 1, 1, COL_PAGE);
	draw_glyph(9, y + 4, '<', C(31, 31, 31), 1);
	draw_glyph(15, y + 4, '>', C(31, 31, 31), 1);
	draw_text(30, y, "NDS", COL_TEXT, 2);
	draw_text(30 + 4 * 16, y, "PHP", COL_ACCENT, 2);
	draw_fill(0, h - 1, 256, 1, COL_LINE);
}
