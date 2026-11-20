/*
 * hwspr.c - BASIC sprite objects on the main engine's hardware sprites
 *
 * Every object (0-63) owns a 32x32 256-colour sprite in VRAM bank B.
 * The 8x8 slot graphic is scaled 1-4x into it; the rest stays transparent.
 * Palette: index 0 = transparent, index c+1 = BASIC colour c.
 */
#include <nds.h>
#include <string.h>
#include "basic.h"
#include "hwspr.h"
#include "spred.h"   /* basicPalette */

static u16 *gfx[BASIC_OBJECTS];

void hwspr_init(void) {
	vramSetBankB(VRAM_B_MAIN_SPRITE);
	oamInit(&oamMain, SpriteMapping_1D_128, false);
	for (int c = 0; c < 16; c++) SPRITE_PALETTE[c + 1] = basicPalette[c];
	for (int o = 0; o < BASIC_OBJECTS; o++) {
		gfx[o] = oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_256Color);
		basic_object(o)->dirty = 1;
	}
	oamClear(&oamMain, 0, 128);
	oamUpdate(&oamMain);
}

/* render the scaled slot graphic into the object's 32x32 sprite (1D tiles) */
static void render(int o, const BasicObject *b) {
	static u32 buf32[1024 / 4];
	u8 *buf = (u8 *)buf32;
	memset(buf32, 0, sizeof buf32);
	int s = b->scale;
	for (int y = 0; y < 8 * s; y++)
		for (int x = 0; x < 8 * s; x++) {
			int c = basic_sprite_get(b->slot, x / s, y / s);
			if (c < 0) continue;
			int tile = (y >> 3) * 4 + (x >> 3);
			buf[tile * 64 + (y & 7) * 8 + (x & 7)] = (u8)(c + 1);
		}
	/* VRAM: 32 bit writes only */
	vu32 *dst = (vu32 *)gfx[o];
	for (int i = 0; i < 1024 / 4; i++) dst[i] = buf32[i];
}

void hwspr_update(bool visible) {
	for (int o = 0; o < BASIC_OBJECTS; o++) {
		BasicObject *b = basic_object(o);
		bool hide = !visible || !b->visible ||
		            b->x <= -32 || b->x >= 256 || b->y <= -32 || b->y >= 192;
		if (!hide && b->dirty) { render(o, b); b->dirty = 0; }
		oamSet(&oamMain, o, b->x & 511, b->y & 255, 0, 0, SpriteSize_32x32,
		       SpriteColorFormat_256Color, gfx[o], -1, false, hide, false, false, false);
	}
	oamUpdate(&oamMain);
}
