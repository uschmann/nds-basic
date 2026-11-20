/* hwspr.h - BASIC sprite objects on the main engine's hardware sprites */
#ifndef HWSPR_H
#define HWSPR_H

#include <stdbool.h>

void hwspr_init(void);              /* after videoSetMode() of the main engine */
/* sync OAM with the interpreter's objects; visible = false hides all.
 * Call once per frame during vblank. */
void hwspr_update(bool visible);

#endif
