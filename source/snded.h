/* snded.h - sound editor with a piano keyboard on the bottom screen */
#ifndef SNDED_H
#define SNDED_H

#include <stdbool.h>

enum { SNDED_NONE = 0, SNDED_CHANGED, SNDED_SLOT, SNDED_EXIT };

void snded_open(int slot);
int  snded_poll(bool touching, int px, int py);
int  snded_slot(void);
void snded_set_slot(int slot);      /* wraps 0-9, redraws */
int  snded_octave(void);
void snded_change_octave(int delta);

#endif
