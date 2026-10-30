/* ROM-span tests, shared by up-console.device (patched console vectors) and amarchy-flat (foreign hook on the
 * sysiclass slot; plan 2026-10-29-amarchy-flat-ui-patch.md FLAT-C4). Header only, no OS calls, so the host
 * tests drive the same code the Amiga programs run. Addresses are unsigned long (32 bit on the Amiga). */
#ifndef UPTERM_ROMSPAN_H
#define UPTERM_ROMSPAN_H

/* 1 when address a lies outside [lo, hi). */
static int rom_addr_outside(unsigned long a, unsigned long lo, unsigned long hi)
{
    return a < lo || a >= hi;
}

/* Define ROMSPAN_VEC before including to get rom_vec_patched (a header-only static function nobody calls would
 * trip -Wunused-function in the other includers). */
#ifdef ROMSPAN_VEC
/* A library vector is stock when it is `jmp abs.l` (4E F9 + big-endian long) and the target lies in [lo, hi).
 * 1 = someone rewrote or redirected it (SetFunction patch), 0 = stock. */
static int rom_vec_patched(const unsigned char *v, unsigned long lo, unsigned long hi)
{
    unsigned long target;
    if (v[0] != 0x4E || v[1] != 0xF9)
        return 1;
    target = ((unsigned long)v[2] << 24) | ((unsigned long)v[3] << 16) | ((unsigned long)v[4] << 8) | (unsigned long)v[5];
    return rom_addr_outside(target, lo, hi);
}
#endif

#endif
