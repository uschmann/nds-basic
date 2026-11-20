/* browser.h - file browser for the saved programs (bottom screen) */
#ifndef BROWSER_H
#define BROWSER_H

#include <nds.h>
#include <stdbool.h>

enum { BR_NONE, BR_EXIT, BR_LOAD, BR_DELETE };

void browser_open(void);
void browser_refresh(void);              /* re-read the card (after DELETE) */
void browser_set_message(const char *m);
/* call once per frame while open; returns BR_* */
int  browser_poll(bool touching, int px, int py, u32 down, u32 rep);

/* BR_LOAD / BR_DELETE: program name ("NAME" or "NAME.PHP") and for
 * BR_LOAD the file to open afterwards ("" = main.php) */
const char *browser_target(void);
const char *browser_target_file(void);

/* preview of the selected entry for the top screen */
const char *browser_preview_title(void);
int         browser_preview_count(void);
const char *browser_preview_line(int i);

#endif
