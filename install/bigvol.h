/* install/bigvol.h: which local disk the Install offers as UP-Term's drawer.
 *
 * Header-only and free of AmigaOS types, so the host tests drive the same code
 * the Installer's helper (install/bigvol.c, kit file Files/UPBigVol) runs.
 * The Installer script cannot list volumes; the helper does, fills one bv_vol
 * per volume, and asks bv_choose() for the answer.
 *
 * A candidate is a volume of a real disk (the caller has checked: a handler
 * with a device behind it, not RAM:, a floppy, a CD or a network share), with
 * an Amiga file system, writable. The most free room wins; a tie goes to SYS:.
 * Room is counted in KiB so a 32-bit number holds 4 TiB: the Installer's own
 * getdiskspace overflows past 2 GB. */
#ifndef BIGVOL_H
#define BIGVOL_H

#include <string.h>

struct bv_vol {
    char name[32];        /* the volume name, without the colon */
    unsigned long dostype; /* the disk's DosType (InfoData id_DiskType) */
    int writable;         /* ID_VALIDATED: not write-protected, not validating */
    int is_sys;           /* the volume SYS: is on */
    unsigned long free_kb;
};

/* DOS\0..DOS\7 (OFS, FFS, intl, dir cache), SFS\0 and SFS\2, PFS\1..PFS\3 and PDS\2, PDS\3. FAT (CrossDOS
 * MSD\0), exFAT, NTFS, CD file systems and anything unknown are not. */
static int bv_dostype_ok(unsigned long t)
{
    if ((t & 0xFFFFFF00UL) == 0x444F5300UL)
        return (t & 0xFFUL) <= 7;
    if (t == 0x53465300UL || t == 0x53465302UL)
        return 1;
    if ((t & 0xFFFFFF00UL) == 0x50465300UL || (t & 0xFFFFFF00UL) == 0x50445300UL)
        return (t & 0xFFUL) >= 1 && (t & 0xFFUL) <= 3;
    return 0;
}

/* Free KiB from an InfoData's block counts, clamped at 4 TiB instead of wrapping. */
static unsigned long bv_free_kb(unsigned long numblocks, unsigned long used, unsigned long bytes_per_block)
{
    unsigned long freeb;
    if (used >= numblocks || !bytes_per_block)
        return 0;
    freeb = numblocks - used;
    if (bytes_per_block >= 1024UL) {
        unsigned long mul = bytes_per_block / 1024UL;
        return freeb > 0xFFFFFFFFUL / mul ? 0xFFFFFFFFUL : freeb * mul;
    }
    return freeb / (1024UL / bytes_per_block);
}

/* The index of the volume to offer, -1 when none qualifies. */
static int bv_choose(const struct bv_vol *v, int n)
{
    int i, best = -1;
    for (i = 0; i < n; i++) {
        if (!v[i].writable || !bv_dostype_ok(v[i].dostype))
            continue;
        if (best < 0 || v[i].free_kb > v[best].free_kb || (v[i].free_kb == v[best].free_kb && v[i].is_sys && !v[best].is_sys))
            best = i;
    }
    return best;
}

/* ---- UPBigVol ROOM: is there room for the parts a Shell install chose? ----
 * dist/install.dos (no STEP) runs `UPBigVol ROOM <drawer> <KB> <KB> ... SYS: <KB> ... [<AmiSSL disk> <KB>]` before
 * it copies anything: each path starts a group, the numbers after it are the KB (tools/kitsize.py) its parts take
 * there. Groups on one volume (the drawer SYS:UP-Term and SYS:) count together; the helper locks each volume and
 * answers SameLock, so the merge here asks a callback. */

struct bv_group {
    const char *path;  /* as given: the first path of its volume names it in the message */
    unsigned long kb;  /* this group's parts, then (when it is the first of its volume) the whole volume's */
    int same_as;       /* -1, or the earlier group of the same volume its KB went into */
};

/* A KB word: digits only, at least one. */
static int bv_is_kb(const char *w)
{
    if (!w || !*w)
        return 0;
    for (; *w; w++)
        if (*w < '0' || *w > '9')
            return 0;
    return 1;
}

/* a + b, held at 0xFFFFFFFF instead of wrapping */
static unsigned long bv_add(unsigned long a, unsigned long b)
{
    return a > 0xFFFFFFFFUL - b ? 0xFFFFFFFFUL : a + b;
}

/* A KB word's value, held at 0xFFFFFFFF. */
static unsigned long bv_kb(const char *w)
{
    unsigned long v = 0;
    for (; *w; w++) {
        unsigned long d = (unsigned long)(*w - '0');
        if (v > (0xFFFFFFFFUL - d) / 10UL)
            return 0xFFFFFFFFUL;
        v = v * 10UL + d;
    }
    return v;
}

/* The volume part of a path, colon included ("SYS:UP-Term" -> "SYS:"); "" for a path with no colon (the current
 * drawer's disk). The drawer itself does not exist yet, its volume does. 0 when it does not fit in `size`. */
static int bv_volume(const char *path, char *out, int size)
{
    const char *c = strchr(path, ':');
    int len = c ? (int)(c - path) + 1 : 0;
    if (len >= size)
        return 0;
    memcpy(out, path, len);
    out[len] = 0;
    return 1;
}

/* Words into groups: a path starts one, the KB words after it add to it. Returns the group count, -1 for a KB word
 * before any path or more than `max` groups. */
static int bv_parse(char **words, int n, struct bv_group *g, int max)
{
    int i, ng = 0;
    for (i = 0; i < n; i++) {
        if (bv_is_kb(words[i])) {
            if (!ng)
                return -1;
            g[ng - 1].kb = bv_add(g[ng - 1].kb, bv_kb(words[i]));
            continue;
        }
        if (ng == max)
            return -1;
        g[ng].path = words[i];
        g[ng].kb = 0;
        g[ng].same_as = -1;
        ng++;
    }
    return ng;
}

/* Each group whose volume is an earlier group's (same(i, j, ctx) true) adds its KB to that one and is marked. */
static void bv_merge(struct bv_group *g, int n, int (*same)(int i, int j, void *ctx), void *ctx)
{
    int i, j;
    for (i = 1; i < n; i++)
        for (j = 0; j < i; j++)
            if (g[j].same_as < 0 && same(i, j, ctx)) {
                g[j].kb = bv_add(g[j].kb, g[i].kb);
                g[i].same_as = j;
                break;
            }
}

/* Too little room: the parts need more than the volume has free. */
static int bv_short(unsigned long free_kb, unsigned long need_kb)
{
    return need_kb > free_kb;
}

#endif
