/*
 * basic.h - small, platform independent line-numbered BASIC interpreter.
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

/* program access for the listing view */
int         basic_line_count(void);
int         basic_line_number(int index);
const char *basic_line_text(int index);
int         basic_find_index(int lineNum); /* first index with num >= lineNum */

#endif
