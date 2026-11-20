/* draw.h - primitives for the 16-bit bitmap on the bottom screen */
#ifndef DRAW_H
#define DRAW_H

#include <nds.h>

/* colour with the bitmap "opaque" bit set */
#define C(r, g, b) ((u16)(RGB15(r, g, b) | BIT(15)))

/* light, friendly colour scheme shared by all bottom screens */
#define COL_PAGE      C(31, 31, 30)   /* page background                  */
#define COL_CARD      C(31, 31, 31)   /* cards, keys                      */
#define COL_TRAY      C(30, 30, 29)   /* recessed areas                   */
#define COL_LINE      C(28, 28, 27)   /* borders, separators              */
#define COL_TEXT      C(3, 3, 3)      /* text                             */
#define COL_MUTED     C(14, 13, 13)   /* secondary text                   */
#define COL_ACCENT    C(30, 6, 0)     /* red accent                       */
#define COL_ACCENT_DK C(23, 3, 0)
#define COL_TINT      C(31, 28, 27)   /* pale red: selected / pressed     */
#define COL_KEY_FN    C(29, 29, 28)   /* function keys, secondary buttons */

/* older names used throughout the code */
#define COL_BODY      COL_PAGE
#define COL_BODY_DARK COL_TRAY
#define COL_BAND      COL_PAGE
#define COL_TITLE     COL_TEXT
#define COL_BORDER    COL_LINE
#define COL_LABEL     COL_TEXT

void draw_init(u16 *framebuffer);
void draw_fill(int x, int y, int w, int h, u16 c);
void draw_glyph(int x, int y, unsigned char ch, u16 c, int scale);
void draw_text(int x, int y, const char *s, u16 c, int scale);
/* rounded box with a soft shadow; bg = colour around it, down = pressed */
void draw_box(int x, int y, int w, int h, u16 face, u16 bg, bool down);
/* text colour that reads well on face (dark on light faces, else white) */
u16  draw_label_color(u16 face);
/* button on the page background: box + centred label */
void draw_button(int x, int y, int w, int h, const char *label, u16 face, bool down);
/* page header: "NDS PHP" with the accent colour, separator line below */
void draw_page_header(int h);

#endif
