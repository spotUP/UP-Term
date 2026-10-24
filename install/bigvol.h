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

#endif
