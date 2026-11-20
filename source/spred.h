/* spred.h - 8x8 sprite editor on the bottom screen */
#ifndef SPRED_H
#define SPRED_H

#include <stdbool.h>
#include <nds/ndstypes.h>

extern const u16 basicPalette[16];   /* defined in main.c */

enum {
	SPRED_NONE = 0,
	SPRED_CHANGED,       /* pixels of the current slot changed */
	SPRED_SLOT,          /* another slot was selected          */
	SPRED_EXIT           /* OK pressed                         */
};

void spred_open(int slot);           /* draws the editor            */
int  spred_poll(bool touching, int px, int py);
int  spred_slot(void);
void spred_set_slot(int slot);       /* wraps around 0-255, redraws */

#endif
