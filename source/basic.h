/*
 * basic.h - interpreter API (historic basic_* names; implemented by php.c).
 *
 * The interpreter is step driven: the frontend feeds lines with
 * basic_enter() and then repeatedly calls basic_run_slice() while
 * basic_state() == BASIC_RUNNING. When a program executes INPUT the state
 * becomes BASIC_INPUT and the frontend answers with basic_input().
 */
#ifndef BASIC_H
#define BASIC_H

typedef struct {
	void (*putch)(char c);          /* write one char, '\n' = newline   */
	void (*cls)(void);              /* clear output screen              */
	int  (*inkey)(void);            /* next pending key or 0 (optional) */
	void (*locate)(int col, int row); /* move cursor (optional)          */
	void (*color)(int c);           /* set text colour 0-15 (optional)  */
	int width;                      /* output width in characters       */
	void (*message)(const char *m); /* SAVE/LOAD status text (optional) */
	/* graphics (optional): colours 0-15, -1 = empty / transparent */
	void (*pset)(int x, int y, int c);
	int  (*pget)(int x, int y);
	void (*gcls)(void);
	int gw, gh;                     /* graphics size in pixels          */
	/* sound (optional): play slot 0-9, -1 = stop; busy = still playing */
	void (*sound)(int slot);
	int  (*sound_busy)(void);
	void (*exit)(void);             /* EXIT: back to title screen (opt.) */
	int  (*joy)(void);              /* JOY: held buttons as bit mask    */
} BasicIO;

typedef enum {
	BASIC_IDLE = 0,
	BASIC_RUNNING,
	BASIC_INPUT
} BasicState;

typedef enum {
	BASIC_ENTER_EMPTY = 0,   /* nothing entered                       */
	BASIC_ENTER_STORED,      /* program line stored / replaced        */
	BASIC_ENTER_DELETED,     /* program line deleted                  */
	BASIC_ENTER_EXEC,        /* immediate statement started           */
	BASIC_ENTER_ERROR        /* bad line number / too many lines      */
} BasicEnterResult;

void        basic_init(const BasicIO *io);
int         basic_enter(const char *line, int *lineNum);
BasicState  basic_state(void);
int         basic_run_slice(int maxStatements);
void        basic_frame(void);           /* call once per video frame   */
void        basic_input(const char *line);
void        basic_break(void);
void        basic_seed(unsigned seed);
const char *basic_last_error(void);      /* "" when the last run was ok */
int         basic_current_line(void);    /* line number executing or -1 */

/* sprites: BASIC_SPRITES slots of 8x8 pixels, colour 0-15 or -1 = transparent */
#define BASIC_SPRITES 256
int         basic_sprite_get(int slot, int x, int y);
void        basic_sprite_set(int slot, int x, int y, int c);

/* hardware sprite objects shown with SPRITE o,n,x,y[,s] */
#define BASIC_OBJECTS   64
#define BASIC_OBJ_SCALE 4
typedef struct {
	unsigned char visible;
	unsigned char slot;          /* sprite slot (graphic)                 */
	unsigned char scale;         /* 1-BASIC_OBJ_SCALE                      */
	unsigned char dirty;         /* graphic must be re-rendered            */
	int x, y;                    /* top left in pixels                     */
} BasicObject;
BasicObject *basic_object(int o);

/* sounds: BASIC_SOUNDS slots, each a sequence of up to BASIC_SOUND_LEN notes */
#define BASIC_SOUNDS    10
#define BASIC_SOUND_LEN 64
enum { SND_SQ12 = 0, SND_SQ25, SND_SQ50, SND_NOISE, SND_INSTRUMENTS };
typedef struct {
	unsigned char inst;                    /* SND_SQ12 .. SND_NOISE          */
	unsigned char tempo;                   /* beats per minute (quarter)     */
	unsigned char count;                   /* number of notes                */
	unsigned char note[BASIC_SOUND_LEN];   /* MIDI note 24-108, 0 = rest     */
	unsigned char len[BASIC_SOUND_LEN];    /* length in 16th notes, 1-16     */
} BasicSound;
BasicSound *basic_sound(int slot);
void        basic_sound_clear(int slot);

/* storage for SAVE/LOAD/DIR/DELETE: directory without trailing '/', NULL = none */
void        basic_set_storage(const char *dir);
int         basic_has_storage(void);
/* sorted names ("NAME.BAS") of saved programs; -1 = no storage */
#define BASIC_MAX_FILES 64
int         basic_list_files(char names[][16], int max);

/* program access for the listing view */
int         basic_line_count(void);
int         basic_line_number(int index);
const char *basic_line_text(int index);
int         basic_find_index(int lineNum); /* first index with num >= lineNum */

#endif
