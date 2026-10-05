/* theme -- see theme.h. Colours are the 16 ANSI ones only: 30-37 / 90-97
 * foregrounds, 40-47 backgrounds. Light themes trade the bright yellow and
 * cyan (unreadable on white) for their dark halves; the colour-blind ones
 * draw a diff in blue and orange-ish yellow instead of green and red. */
#include <string.h>
#include "theme.h"

const cl_theme cl_themes[THEME_COUNT] = {
    { "dark", "Dark mode", "\033[90m", "\033[33m", "\033[36m", "\033[33m", "\033[35m", "\033[36m", "\033[95m",
      "\033[94m", "\033[32m", "\033[31m", "\033[30;42m", "\033[97;41m", "\033[2m", "ansi" },
    { "light", "Light mode", "\033[90m", "\033[31m", "\033[34m", "\033[31m", "\033[35m", "\033[34m", "\033[35m",
      "\033[34m", "\033[32m", "\033[31m", "\033[30;42m", "\033[97;41m", "\033[2m", "ansi" },
    { "dark-daltonized", "Dark mode (colorblind-friendly)", "\033[90m", "\033[33m", "\033[36m", "\033[33m",
      "\033[35m", "\033[36m", "\033[95m", "\033[94m", "\033[34m", "\033[33m", "\033[97;44m", "\033[30;43m",
      "\033[2m", "ansi" },
    { "light-daltonized", "Light mode (colorblind-friendly)", "\033[90m", "\033[34m", "\033[34m", "\033[34m",
      "\033[35m", "\033[34m", "\033[35m", "\033[34m", "\033[34m", "\033[33m", "\033[97;44m", "\033[30;43m",
      "\033[2m", "ansi" },
    { "dark-ansi", "Dark mode (ANSI colors only)", "\033[90m", "\033[33m", "\033[36m", "\033[33m", "\033[35m",
      "\033[36m", "\033[95m", "\033[94m", "\033[32m", "\033[31m", "\033[30;42m", "\033[97;41m", "\033[2m",
      "ansi" },
    { "light-ansi", "Light mode (ANSI colors only)", "\033[90m", "\033[31m", "\033[34m", "\033[31m", "\033[35m",
      "\033[34m", "\033[35m", "\033[34m", "\033[32m", "\033[31m", "\033[30;42m", "\033[97;41m", "\033[2m",
      "ansi" },
    { "monochrome", "Monochrome (no colour: 2- and 4-colour screens)", "", "\033[1m", "\033[1m", "\033[1m",
      "\033[1m", "\033[1m", "\033[1m", "\033[1m", "\033[1m", "\033[1m", "\033[1;4m", "\033[7m", "\033[2m",
      "mono" }
};

const cl_theme *theme_get(const char *name)
{
    int i;
    for (i = 0; name && i < THEME_COUNT; i++)
        if (!strcmp(cl_themes[i].name, name))
            return &cl_themes[i];
    return &cl_themes[0];
}

int theme_index(const cl_theme *t)
{
    int i;
    for (i = 0; i < THEME_COUNT; i++)
        if (&cl_themes[i] == t)
            return i;
    return 0;
}
