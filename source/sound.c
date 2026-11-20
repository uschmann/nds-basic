/* sound.c - simple PSG sequencer for BASIC sound slots
 *
 * One note at a time on a PSG (square wave) or noise channel. Timing is
 * driven by snd_frame() at 60 Hz; a 16th note lasts 900/tempo frames.
 */
#include <nds.h>
#include <math.h>
#include <stdio.h>
#include "sound.h"

#define VOLUME 80

static BasicSound seq;
static bool playing;
static int pos = -1, framesLeft, channel = -1;
static int previewFrames;

static void note_off(void) {
	if (channel >= 0) soundKill(channel);
	channel = -1;
}

static void note_on(int note, int inst) {
	note_off();
	if (note <= 0) return;
	float hz = 440.0f * powf(2.0f, (note - 69) / 12.0f);
	if (inst == SND_NOISE) {
		channel = soundPlayNoise((u16)(hz * 8), VOLUME, 64);
	} else {
		static const DutyCycle duty[3] = { DutyCycle_12, DutyCycle_25, DutyCycle_50 };
		/* the PSG timer runs 8 steps per period: pass 8x the tone frequency */
		channel = soundPlayPSG(duty[inst < 3 ? inst : 2], (u16)(hz * 8), VOLUME, 64);
	}
}

static int note_frames(int len, int tempo) {
	int f = (len * 900 + tempo / 2) / (tempo ? tempo : 120);
	return f < 2 ? 2 : f;
}

static void start_note(void) {
	note_on(seq.note[pos], seq.inst);
	framesLeft = note_frames(seq.len[pos], seq.tempo);
}

void snd_init(void) { soundEnable(); }

void snd_play(const BasicSound *s) {
	snd_stop();
	if (!s || !s->count) return;
	seq = *s;
	playing = true;
	pos = 0;
	start_note();
}

void snd_stop(void) {
	note_off();
	playing = false;
	previewFrames = 0;
	pos = -1;
}

void snd_preview(int note, int inst, int frames) {
	if (playing) snd_stop();
	note_on(note, inst);
	previewFrames = frames;
}

void snd_frame(void) {
	if (previewFrames > 0 && --previewFrames == 0) note_off();
	if (!playing) return;
	framesLeft--;
	if (framesLeft == 1) note_off();          /* short gap between notes */
	if (framesLeft > 0) return;
	if (++pos >= seq.count) { snd_stop(); return; }
	start_note();
}

bool snd_busy(void) { return playing; }
int  snd_position(void) { return playing ? pos : -1; }

const char *snd_note_name(int note) {
	static const char *names[12] = { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
	static char buf[12];
	if (note <= 0) return "---";
	snprintf(buf, sizeof buf, "%s%d", names[note % 12], note / 12 - 1);
	return buf;
}

const char *snd_inst_name(int inst) {
	static const char *n[SND_INSTRUMENTS] = { "SQ12", "SQ25", "SQ50", "NOISE" };
	return (inst >= 0 && inst < SND_INSTRUMENTS) ? n[inst] : "?";
}
