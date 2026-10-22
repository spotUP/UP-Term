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

/* The read buffer (16 KB) is released as soon as the file is parsed, before anything is rebuilt. */
static int load_buf(keymap *km, const char *def_text, int (*keep)(const km_bind *), int compact)
{
    char *buf = (char *)AllocVec(KM_FILE_MAX + 2, MEMF_ANY);
    int ok = load(km, def_text, buf);
    if (buf)
        FreeVec(buf);
    if (compact && !keymap_compact(km, keep))
        keymap_free(km);   /* out of memory: no table rather than a half one */
    return ok;
}

int keymap_load(keymap *km, const char *def_text)
{
    return load_buf(km, def_text, 0, 0);
}

int keymap_load_compact(keymap *km, const char *def_text, int (*keep)(const km_bind *))
{
    return load_buf(km, def_text, keep, 1);
}

int keymap_reload_compact(keymap *km, int (*keep)(const km_bind *))
{
    keymap t;
    int ok;
    memset(&t, 0, sizeof t);
    ok = load_buf(&t, 0, keep, 1);
    if (ok)
        keymap_swap(km, &t);    /* the old table is released only now */
    keymap_free(&t);            /* a refused scratch; empty after a swap */
    return ok;
}

int keymap_reload(keymap *km)
{
    keymap t;
    int ok;
    memset(&t, 0, sizeof t);
    ok = load_buf(&t, 0, 0, 0);
    if (ok)
        keymap_swap(km, &t);
    keymap_free(&t);
    return ok;
}
