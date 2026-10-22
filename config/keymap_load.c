#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include "keymap_load.h"

/* path into buf (cap bytes, NUL-ended); the bytes read, -1 when the file is not there */
static LONG read_file(const char *path, char *buf, LONG cap)
{
    BPTR f = Open((STRPTR)path, MODE_OLDFILE);
    LONG got;
    if (!f)
        return -1;
    got = Read(f, buf, cap);
    Close(f);
    if (got < 1)
        return -1; /* an empty file is as good as none: the shipped table stands */
    buf[got] = 0;
    return got;
}

static int load(keymap *km, const char *def_text, char *buf)
{
    LONG got = -1;
    int ok = 0;
    if (buf) {
        got = read_file("ENV:up-term/keymap", buf, KM_FILE_MAX);
        if (got < 0)
            got = read_file("ENVARC:up-term/keymap", buf, KM_FILE_MAX);
        if (got > 0)
            ok = keymap_parse(km, buf, got);
    }
    if (!ok) {
        if (!def_text)
            return 0;
        keymap_parse(km, def_text, (long)strlen(def_text));
    }
    if (buf) {
        got = read_file("ENV:up-term/keymap.user", buf, KM_FILE_MAX);
        if (got < 0)
            got = read_file("ENVARC:up-term/keymap.user", buf, KM_FILE_MAX);
        if (got > 0)
            keymap_overlay(km, buf, got);
    }
    return ok;
}

int keymap_load(keymap *km, const char *def_text)
{
    char *buf = (char *)AllocVec(KM_FILE_MAX + 2, MEMF_ANY);
    int ok = load(km, def_text, buf);
    if (buf)
        FreeVec(buf);
    return ok;
}

int keymap_reload(keymap *km)
{
    keymap t;
    char *buf = (char *)AllocVec(KM_FILE_MAX + 2, MEMF_ANY);
    int ok = 0;
    memset(&t, 0, sizeof t);
    if (buf && load(&t, 0, buf)) {
        keymap_swap(km, &t);    /* the old table is released only now */
        ok = 1;
    }
    keymap_free(&t);            /* a refused scratch; empty after a swap */
    if (buf)
        FreeVec(buf);
    return ok;
}
