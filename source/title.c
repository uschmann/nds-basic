/*
 * title.c - title screen: a rotating low-poly home computer rendered with
 * the DS 3D hardware on the top screen, title text on the bottom screen.
 *
 * Model (units ~ 1 = 10 cm): breadbin keyboard in front, CRT monitor
 * behind it and a floppy drive next to the monitor. Lighting is done in
 * software per face (flat shading) and edge marking gives dark outlines.
 */
#include <nds.h>
#include <math.h>
#include <string.h>
#include "draw.h"
#include "title.h"

typedef struct { float x, y, z; } V3;

static V3 lightDir;          /* light direction in model space */
static int polyId;

static V3 v3(float x, float y, float z) { V3 v = { x, y, z }; return v; }
static V3 sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static V3 cross(V3 a, V3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 norm(V3 a) {
	float l = sqrtf(dot(a, a));
	return l > 0 ? v3(a.x / l, a.y / l, a.z / l) : a;
}

/* start a new part: own polygon id so edge marking outlines it */
static void part(void) {
	polyId = (polyId % 62) + 1;
	glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(polyId));
}

/* flat shaded quad; 'inside' is a point inside the part (fixes normal direction) */
static void quad(V3 a, V3 b, V3 c, V3 d, int r, int g, int bl, V3 inside) {
	V3 n = norm(cross(sub(b, a), sub(c, a)));
	V3 mid = v3((a.x + c.x) / 2, (a.y + c.y) / 2, (a.z + c.z) / 2);
	if (dot(n, sub(mid, inside)) < 0) n = v3(-n.x, -n.y, -n.z);
	float lit = dot(n, lightDir);
	float k = 0.45f + 0.65f * (lit > 0 ? lit : 0);
	int R = (int)(r * k), G = (int)(g * k), B = (int)(bl * k);
	glColor(RGB15(R > 31 ? 31 : R, G > 31 ? 31 : G, B > 31 ? 31 : B));
	glBegin(GL_QUADS);
	glVertex3f(a.x, a.y, a.z);
	glVertex3f(b.x, b.y, b.z);
	glVertex3f(c.x, c.y, c.z);
	glVertex3f(d.x, d.y, d.z);
	glEnd();
}

/* axis aligned box, all faces but the bottom */
static void box(float x0, float y0, float z0, float x1, float y1, float z1, int r, int g, int b) {
	V3 in = v3((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2);
	quad(v3(x0, y1, z0), v3(x1, y1, z0), v3(x1, y1, z1), v3(x0, y1, z1), r, g, b, in);  /* top   */
	quad(v3(x0, y0, z1), v3(x1, y0, z1), v3(x1, y1, z1), v3(x0, y1, z1), r, g, b, in);  /* front */
	quad(v3(x0, y0, z0), v3(x1, y0, z0), v3(x1, y1, z0), v3(x0, y1, z0), r, g, b, in);  /* back  */
	quad(v3(x0, y0, z0), v3(x0, y0, z1), v3(x0, y1, z1), v3(x0, y1, z0), r, g, b, in);  /* left  */
	quad(v3(x1, y0, z0), v3(x1, y0, z1), v3(x1, y1, z1), v3(x1, y1, z0), r, g, b, in);  /* right */
}

/* the keyboard "breadbin": a wedge with a sloped top and rows of keys */
static void keyboard(void) {
	const float x0 = -1.1f, x1 = 1.1f, zf = 0.95f, zb = 0.05f, hf = 0.10f, hb = 0.28f;
	const int R = 24, G = 21, B = 16;
	V3 in = v3(0, 0.08f, 0.5f);
	part();
	quad(v3(x0, hf, zf), v3(x1, hf, zf), v3(x1, hb, zb), v3(x0, hb, zb), R, G, B, in);   /* slope */
	quad(v3(x0, 0, zf), v3(x1, 0, zf), v3(x1, hf, zf), v3(x0, hf, zf), R, G, B, in);     /* front */
	quad(v3(x0, 0, zb), v3(x1, 0, zb), v3(x1, hb, zb), v3(x0, hb, zb), R, G, B, in);     /* back  */
	quad(v3(x0, 0, zf), v3(x0, hf, zf), v3(x0, hb, zb), v3(x0, 0, zb), R, G, B, in);     /* left  */
	quad(v3(x1, 0, zf), v3(x1, hf, zf), v3(x1, hb, zb), v3(x1, 0, zb), R, G, B, in);     /* right */

	/* keys: 4 rows of 12 + space bar, sitting on the slope */
	part();
	for (int row = 0; row < 5; row++) {
		float t0 = 0.15f + row * 0.16f, t1 = t0 + 0.12f;        /* 0 = back, 1 = front */
		float za = zb + (zf - zb) * t0, zc = zb + (zf - zb) * t1;
		float ya = hb + (hf - hb) * t0, yc = hb + (hf - hb) * t1;
		int n = row < 4 ? 12 : 1;
		for (int k = 0; k < n; k++) {
			float kx0 = row < 4 ? -0.95f + k * 0.16f + (row & 1) * 0.04f : -0.5f;
			float kx1 = row < 4 ? kx0 + 0.12f : 0.5f;
			bool func = row < 4 && k == 11;
			int r = func ? 26 : 8, g = func ? 12 : 6, b = func ? 4 : 5;
			const float h = 0.04f;
			V3 kin = v3((kx0 + kx1) / 2, (ya + yc) / 2, (za + zc) / 2);
			quad(v3(kx0, ya + h, za), v3(kx1, ya + h, za), v3(kx1, yc + h, zc), v3(kx0, yc + h, zc), r, g, b, kin);
			quad(v3(kx0, yc, zc), v3(kx1, yc, zc), v3(kx1, yc + h, zc), v3(kx0, yc + h, zc), r, g, b, kin);
			quad(v3(kx0, ya, za), v3(kx0, yc, zc), v3(kx0, yc + h, zc), v3(kx0, ya + h, za), r, g, b, kin);
			quad(v3(kx1, ya, za), v3(kx1, yc, zc), v3(kx1, yc + h, zc), v3(kx1, ya + h, za), r, g, b, kin);
		}
	}
	/* rainbow badge on the right */
	part();
	static const int stripes[4][3] = { { 28, 4, 4 }, { 30, 18, 2 }, { 30, 28, 4 }, { 6, 22, 6 } };
	for (int s = 0; s < 4; s++) {
		float x = 0.80f + s * 0.05f;
		quad(v3(x, hf + 0.002f, zf - 0.02f), v3(x + 0.035f, hf + 0.002f, zf - 0.02f),
		     v3(x + 0.035f, hf + 0.03f, zf - 0.12f), v3(x, hf + 0.03f, zf - 0.12f),
		     stripes[s][0], stripes[s][1], stripes[s][2], v3(x, 0, 0.5f));
	}
}

/* CRT monitor with a tapered back and a blue "READY." screen */
static void monitor(float t) {
	const float x0 = -0.75f, x1 = 0.55f, y0 = 0.02f, y1 = 1.05f, zf = -0.10f, zm = -0.55f;
	const int R = 25, G = 23, B = 19;
	part();
	box(-0.5f, 0, -0.95f, 0.3f, 0.02f, -0.15f, 12, 11, 9);       /* foot */
	part();
	box(x0, y0, zm, x1, y1, zf, R, G, B);                       /* front case */
	part();
	/* tapered CRT back */
	V3 in = v3(-0.1f, 0.5f, -0.8f);
	const float bx0 = -0.55f, bx1 = 0.35f, by0 = 0.15f, by1 = 0.88f, zb = -1.05f;
	quad(v3(x0, y1, zm), v3(x1, y1, zm), v3(bx1, by1, zb), v3(bx0, by1, zb), R - 3, G - 3, B - 3, in);
	quad(v3(x0, y0, zm), v3(x1, y0, zm), v3(bx1, by0, zb), v3(bx0, by0, zb), R - 3, G - 3, B - 3, in);
	quad(v3(x0, y0, zm), v3(x0, y1, zm), v3(bx0, by1, zb), v3(bx0, by0, zb), R - 3, G - 3, B - 3, in);
	quad(v3(x1, y0, zm), v3(x1, y1, zm), v3(bx1, by1, zb), v3(bx1, by0, zb), R - 3, G - 3, B - 3, in);
	quad(v3(bx0, by0, zb), v3(bx1, by0, zb), v3(bx1, by1, zb), v3(bx0, by1, zb), R - 3, G - 3, B - 3, in);

	/* screen: light blue border, dark blue inside, a few text lines */
	const float sz = zf + 0.012f;   /* layers well apart: avoid z-fighting */
	V3 sin_ = v3(0, 0.5f, -0.3f);
	part();
	quad(v3(x0 + 0.10f, y0 + 0.12f, sz), v3(x1 - 0.10f, y0 + 0.12f, sz),
	     v3(x1 - 0.10f, y1 - 0.10f, sz), v3(x0 + 0.10f, y1 - 0.10f, sz), 16, 14, 29, sin_);
	part();
	quad(v3(x0 + 0.18f, y0 + 0.20f, sz + 0.024f), v3(x1 - 0.18f, y0 + 0.20f, sz + 0.024f),
	     v3(x1 - 0.18f, y1 - 0.18f, sz + 0.024f), v3(x0 + 0.18f, y1 - 0.18f, sz + 0.024f), 8, 6, 19, sin_);
	part();
	static const float lineLen[5] = { 0.75f, 0.45f, 0.0f, 0.30f, 0.0f };
	for (int l = 0; l < 5; l++) {
		float len = lineLen[l];
		if (l == 4) len = ((int)(t * 2) & 1) ? 0.06f : 0.0f;     /* blinking cursor */
		if (len <= 0) continue;
		float ly = y1 - 0.25f - l * 0.10f, lx = x0 + 0.22f;
		quad(v3(lx, ly - 0.05f, sz + 0.036f), v3(lx + len, ly - 0.05f, sz + 0.036f),
		     v3(lx + len, ly, sz + 0.036f), v3(lx, ly, sz + 0.036f), 16, 14, 29, sin_);
	}
	/* power LED */
	part();
	box(x1 - 0.12f, y0 + 0.04f, zf, x1 - 0.06f, y0 + 0.08f, zf + 0.02f, 31, 4, 4);
}

/* floppy drive to the right of the monitor */
static void floppy(void) {
	part();
	box(0.68f, 0, -0.95f, 1.18f, 0.24f, -0.15f, 24, 22, 18);
	part();
	box(0.75f, 0.11f, -0.15f, 1.11f, 0.15f, -0.14f, 5, 4, 4);   /* slot */
	part();
	box(1.05f, 0.04f, -0.15f, 1.09f, 0.07f, -0.13f, 4, 28, 6);  /* LED */
}

static void render_scene(float angle, float t) {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0.0f, 1.50f, 2.95f, 0.05f, 0.30f, -0.10f, 0.0f, 1.0f, 0.0f);
	float bob = sinf(t * 1.3f) * 0.04f;
	glTranslatef(0.0f, bob, 0.0f);
	glRotateY(angle);

	/* light from upper left front, transformed into model space */
	float a = -angle * 3.14159265f / 180.0f;
	V3 L = norm(v3(-0.5f, 0.8f, 0.6f));
	lightDir = v3(L.x * cosf(a) + L.z * sinf(a), L.y, -L.x * sinf(a) + L.z * cosf(a));

	polyId = 0;
	keyboard();
	monitor(t);
	floppy();
	glFlush(0);
}

/* ---------------- bottom screen ---------------- */

static void draw_bottom(bool blink) {
	draw_fill(0, 0, 256, 192, COL_BODY);
	draw_fill(0, 0, 256, 60, COL_BAND);
	static const u16 stripes[] = { C(28, 4, 4), C(30, 16, 2), C(30, 28, 4), C(6, 22, 6), C(6, 12, 28) };
	for (int s = 0; s < 5; s++)
		for (int y = 0; y < 60; y++)
			draw_fill(150 + s * 16 + (59 - y) / 2, y, 11, 1, stripes[s]);
	draw_text(16, 10, "NDS", COL_TITLE, 3);
	draw_text(16, 34, "BASIC", COL_TITLE, 3);
	draw_fill(0, 60, 256, 2, COL_BORDER);

	draw_text(20, 78, "**** RETRO BASIC V1.0 ****", C(10, 8, 6), 1);
	draw_text(28, 96, "GRAFIK  SPRITES  SOUND", C(10, 8, 6), 1);
	draw_text(52, 110, "SPEICHERN AUF SD", C(10, 8, 6), 1);

	draw_fill(40, 136, 176, 26, COL_BORDER);
	draw_fill(41, 137, 174, 24, C(3, 7, 3));
	if (blink) draw_text(56, 145, "PRESS ANY KEY", C(10, 29, 10), 1);
	draw_text(64, 176, "SELECT = HILFE", C(13, 11, 9), 1);
}

void title_screen(void) {
	/* bottom: 16-bit bitmap like the keyboard */
	videoSetModeSub(MODE_5_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	int bg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
	draw_init(bgGetGfxPtr(bg));

	/* top: 3D */
	videoSetMode(MODE_0_3D);
	glInit();
	glEnable(GL_ANTIALIAS | GL_OUTLINE);
	glSetOutlineColor(0, RGB15(3, 2, 2));
	glClearColor(4, 3, 12, 31);
	glClearPolyID(63);
	glClearDepth(0x7FFF);
	glViewport(0, 0, 255, 191);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(55, 256.0f / 192.0f, 0.1f, 40);

	float angle = 0.0f, t = 0;
	bool lastBlink = false;
	draw_bottom(true);
	/* ignore keys that are still held from boot */
	scanKeys();
	while (pmMainLoop()) {
		render_scene(angle, t);
		swiWaitForVBlank();
		t += 1.0f / 60.0f;
		angle = sinf(t * 0.55f) * 50.0f;   /* swing around the front view */

		bool blink = ((int)(t * 2) & 1) == 0;
		if (blink != lastBlink) {
			lastBlink = blink;
			draw_fill(41, 137, 174, 24, C(3, 7, 3));
			if (blink) draw_text(56, 145, "PRESS ANY KEY", C(10, 29, 10), 1);
		}
		scanKeys();
		if (keysDown()) break;
	}
	/* wait for release so the key/tap does not reach the editor */
	do { swiWaitForVBlank(); scanKeys(); } while (keysHeld() && pmMainLoop());
	/* leave 3D mode; video_init() sets up the 2D screens again */
	videoSetMode(MODE_0_2D);
}
