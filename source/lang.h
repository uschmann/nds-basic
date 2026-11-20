/* lang.h - UI language, taken from the DS firmware settings */
#ifndef LANG_H
#define LANG_H

#include <stdbool.h>

/* true when the console is set to German; every other language falls
 * back to English */
extern bool langDE;

void lang_init(void);

/* pick the German or English variant of a text */
#define L(de, en) (langDE ? (de) : (en))

#endif
