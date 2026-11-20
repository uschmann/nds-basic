/* lang.c - read the system language from the firmware user settings */
#include <nds.h>
#include "lang.h"

bool langDE;

void lang_init(void) {
	langDE = g_envUserSettings->config.language == EnvLanguage_German;
}
