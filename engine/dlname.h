/* dlname.h -- the one file-name sanitiser for everything the engine saves or loads by a
 * name a remote peer chose (iTerm2 inline=0 downloads, RIP icons). Header-only statics so
 * the pure RIP files compile without vtengine.c. */
#ifndef VT_DLNAME_H
#define VT_DLNAME_H

#include <string.h>

/* raw[0..rn) as a safe file name: base name only (after the last '/', '\\' or ':'), no
 * control characters, never "." / ".." / empty (then `fallback`, which may be NULL = a
 * refused name and out is ""). Returns 1 when a name was produced. */
static int dl_clean_name(const char *raw, int rn, char *out, int max, const char *fallback)
{
    int i, o = 0, start = 0;
    for (i = 0; i < rn; i++)
        if (raw[i] == '/' || raw[i] == '\\' || raw[i] == ':')
            start = i + 1;
    for (i = start; i < rn && o < max - 1; i++)
        if ((unsigned char)raw[i] >= 0x20 && (unsigned char)raw[i] != 0x7F)
            out[o++] = raw[i];
    out[o] = 0;
    if (!o || !strcmp(out, ".") || !strcmp(out, "..")) {
        if (!fallback) {
            out[0] = 0;
            return 0;
        }
        strncpy(out, fallback, (size_t)max - 1);
        out[max - 1] = 0;
    }
    return 1;
}

#endif
