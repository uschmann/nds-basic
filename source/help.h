/* help.h - built-in help page text (German or English) */
#ifndef HELP_H
#define HELP_H

#include <stdbool.h>

extern const char *const *helpText;
extern int helpLineCount;

/* select the German help or the English fallback */
void help_init(bool german);

#endif
