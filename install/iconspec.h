/* iconspec: the console window a Shell icon opens, rewritten for UP-Term
 * by the kit's upicon (install/upicon.c). Pure C, host-tested
 * (tests/test_iconspec.c). */
#ifndef ICONSPEC_H
#define ICONSPEC_H

/* The Shell icon's default window, when its WINDOW tooltype is missing */
#define ICONSPEC_DEFAULT "CON:0/50//130/AmigaShell/CLOSE"

/* A WINDOW tooltype value with its device swapped for dev (no colon):
 * "KCON:0/50//130/AmigaShell/CLOSE" with "XCON" is
 * "XCON:0/50//130/AmigaShell/CLOSE" -- the window's place, size, title and
 * options stay. value 0 or empty: ICONSPEC_DEFAULT's window. Into out (at
 * most max bytes). Returns 1 when out differs from value, 0 when it is the
 * same (already that device), -1 when it does not fit. */
int iconspec_window_device(const char *value, const char *dev, char *out, int max);

#endif
