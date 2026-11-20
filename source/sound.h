/* sound.h - simple PSG sequencer for BASIC sound slots */
#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>
#include "basic.h"

void snd_init(void);
void snd_play(const BasicSound *s);       /* starts playing a copy of s   */
void snd_stop(void);
void snd_frame(void);                     /* call once per video frame    */
bool snd_busy(void);
int  snd_position(void);                  /* index of current note or -1  */
void snd_preview(int note, int inst, int frames);   /* single note      */
const char *snd_note_name(int note);      /* "C#4", "---" for rest       */
const char *snd_inst_name(int inst);

#endif
