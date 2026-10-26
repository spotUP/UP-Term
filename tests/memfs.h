/* A file system in memory for the install and load tests (shared by test_prefs.c and test_amconf.c),
 * with one operation made to fail. Include once per test file; the helpers are static. */
#ifndef TESTS_MEMFS_H
#define TESTS_MEMFS_H
#include <stdio.h>
#include <string.h>
#include "../prefs/prefs_core.h"

/* ---- a file system in memory, with one operation made to fail ---------------- */

#define FS_FILES 12
typedef struct {
    char name[FS_FILES][48];
    char data[FS_FILES][512];
    int used[FS_FILES];
    const char *fail_write;   /* writing this path fails */
    const char *fail_rename;  /* renaming onto this path fails */
    const char *fail_remove;  /* removing this path fails */
    int envarc_fallthrough;   /* OS 3.2 ram-handler: a missing ENV: name looked up is copied from ENVARC: */
} memfs;

static int mf_find(memfs *m, const char *path)
{
    int i;
    for (i = 0; i < FS_FILES; i++)
        if (m->used[i] && !strcmp(m->name[i], path))
            return i;
    return -1;
}

static const char *mf_get(memfs *m, const char *path)
{
    int i = mf_find(m, path);
    return i < 0 ? "<absent>" : m->data[i];
}

static void mf_put(memfs *m, const char *path, const char *data)
{
    int i = mf_find(m, path);
    if (i < 0)
        for (i = 0; i < FS_FILES && m->used[i]; i++)
            ;
    m->used[i] = 1;
    strcpy(m->name[i], path);
    strcpy(m->data[i], data);
}

static int mf_write(void *ctx, const char *path, const char *buf, long len)
{
    memfs *m = (memfs *)ctx;
    char tmp[512];
    if (m->fail_write && !strcmp(m->fail_write, path)) {
        mf_put(m, path, "<partial>"); /* a short write leaves a stub behind */
        return 0;
    }
    memcpy(tmp, buf, (size_t)len);
    tmp[len] = 0;
    mf_put(m, path, tmp);
    return 1;
}

static void mf_put(memfs *m, const char *path, const char *data);

/* mf_find as a lookup through the handler: with envarc_fallthrough, a name
 * ENV: lacks comes back as a copy of ENVARC:'s file of that name. */
static int mf_look(memfs *m, const char *path)
{
    int i = mf_find(m, path), a;
    char arc[48];
    if (i >= 0 || !m->envarc_fallthrough || strncmp(path, "ENV:", 4))
        return i;
    snprintf(arc, sizeof arc, "ENVARC:%s", path + 4);
    if ((a = mf_find(m, arc)) < 0)
        return -1;
    mf_put(m, path, m->data[a]);
    return mf_find(m, path);
}

static int mf_exists(void *ctx, const char *path)
{
    return mf_look((memfs *)ctx, path) >= 0;
}

static int mf_remove(void *ctx, const char *path)
{
    memfs *m = (memfs *)ctx;
    int i = mf_find(m, path);
    if (m->fail_remove && !strcmp(m->fail_remove, path))
        return i < 0;
    if (i >= 0)
        m->used[i] = 0;
    return 1;
}

static int mf_rename(void *ctx, const char *from, const char *to)
{
    memfs *m = (memfs *)ctx;
    int i = mf_find(m, from);
    if (i < 0 || mf_look(m, to) >= 0)
        return 0; /* AmigaDOS: no source, or the target exists */
    if (m->fail_rename && !strcmp(m->fail_rename, to))
        return 0;
    strcpy(m->name[i], to);
    return 1;
}

static void mf_init(memfs *m, prefs_fs *fs)
{
    memset(m, 0, sizeof(*m));
    fs->ctx = m;
    fs->write_file = mf_write;
    fs->exists = mf_exists;
    fs->remove = mf_remove;
    fs->rename = mf_rename;
}

/* read a file into buf (cap bytes) as the amconf load chain does: its length, -1 when it is not there
 * (a lookup, so the OS 3.2 fallthrough applies) */
__attribute__((unused)) static long mf_read(void *ctx, const char *path, char *buf, long cap)
{
    memfs *m = (memfs *)ctx;
    int i = mf_look(m, path);
    long n;
    if (i < 0)
        return -1;
    n = (long)strlen(m->data[i]);
    if (n > cap)
        n = cap;
    memcpy(buf, m->data[i], (size_t)n);
    return n;
}

__attribute__((unused)) static int mf_dir(const prefs_paths *p, int t)
{
    (void)p;
    (void)t;
    return 1;
}

#endif
