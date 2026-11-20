/* title.h - 3D title screen shown at startup */
#ifndef TITLE_H
#define TITLE_H

#include <stdbool.h>

/* Shows the title and the list of saved programs. Returns true and the
 * file name ("NAME.BAS") in chosen[16] when a program was tapped, false
 * when the user continues with a button / "NEUES PROGRAMM". */
bool title_screen(char *chosen, bool programInMemory);

#endif
