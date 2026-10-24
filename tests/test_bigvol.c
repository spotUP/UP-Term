/* install/bigvol.h: the disk the Install offers for UP-Term's drawer. */
#include "harness.h"
#include "../install/bigvol.h"

static void vol(struct bv_vol *v, const char *name, unsigned long type, int rw, int sys, unsigned long kb)
{
    memset(v, 0, sizeof *v);
    strcpy(v->name, name);
    v->dostype = type;
    v->writable = rw;
    v->is_sys = sys;
    v->free_kb = kb;
}

#define FFS 0x444F5301UL
#define OFS 0x444F5300UL
#define SFS 0x53465300UL
#define PFS 0x50465303UL
#define FAT 0x4D534400UL /* CrossDOS MSD\0 */

void suite_bigvol(void);
void suite_bigvol(void)
{
    struct bv_vol v[6];
    /* the DosType whitelist: the DOS\0-\7 family, SFS, PFS; not FAT, CD, exFAT, a zero type */
    CHECK(bv_dostype_ok(0x444F5300UL) && bv_dostype_ok(0x444F5307UL) && !bv_dostype_ok(0x444F5308UL));
    CHECK(bv_dostype_ok(SFS) && bv_dostype_ok(0x53465302UL) && !bv_dostype_ok(0x53465301UL));
    CHECK(bv_dostype_ok(0x50465301UL) && bv_dostype_ok(PFS) && bv_dostype_ok(0x50445302UL) && !bv_dostype_ok(0x50465300UL));
    CHECK(!bv_dostype_ok(FAT) && !bv_dostype_ok(0x43444653UL) && !bv_dostype_ok(0x45584641UL) && !bv_dostype_ok(0));
    /* free room in KiB, clamped at 0xFFFFFFFF KiB (4 TiB): 3 GB on 512-byte blocks, 6 GB on 4096-byte blocks, no wrap at 4 GiB */
    CHECK(bv_free_kb(6291456UL, 0, 512) == 3145728UL);
    CHECK(bv_free_kb(2000000UL, 500000UL, 4096) == 6000000UL); /* 5.7 GiB: more than 32 bits of bytes */
    CHECK(bv_free_kb(100, 100, 512) == 0 && bv_free_kb(100, 200, 512) == 0 && bv_free_kb(100, 0, 0) == 0);
    CHECK(bv_free_kb(0xFFFFFFFFUL, 0, 4096) == 0xFFFFFFFFUL); /* clamped, never wrapped to a small number */
    /* the most free room wins, a 3 GB partition against a 1.5 GB SYS: */
    vol(&v[0], "System", FFS, 1, 1, 1500000);
    vol(&v[1], "Work", FFS, 1, 0, 3145728);
    CHECK(bv_choose(v, 2) == 1);
    /* more than 4 GB (4294967296 bytes = 4194304 KiB) against 3 GB */
    vol(&v[2], "Big", SFS, 1, 0, 6000000);
    CHECK(bv_choose(v, 3) == 2);
    /* a tie goes to SYS: whichever order they come in */
    vol(&v[0], "System", FFS, 1, 1, 777);
    vol(&v[1], "Work", FFS, 1, 0, 777);
    CHECK(bv_choose(v, 2) == 0);
    vol(&v[0], "Work", FFS, 1, 0, 777);
    vol(&v[1], "System", FFS, 1, 1, 777);
    CHECK(bv_choose(v, 2) == 1);
    /* a tie of two others: the first */
    vol(&v[0], "A", OFS, 1, 0, 50);
    vol(&v[1], "B", PFS, 1, 0, 50);
    CHECK(bv_choose(v, 2) == 0);
    /* never: write-protected, FAT, a bigger one of an unknown file system */
    vol(&v[0], "System", FFS, 1, 1, 100);
    vol(&v[1], "Locked", FFS, 0, 0, 9000000);
    vol(&v[2], "Stick", FAT, 1, 0, 9000000);
    vol(&v[3], "Odd", 0x12345678UL, 1, 0, 9000000);
    CHECK(bv_choose(v, 4) == 0);
    /* no candidate: -1 (the Install offers SYS:UP-Term) */
    vol(&v[0], "Locked", FFS, 0, 1, 100);
    CHECK(bv_choose(v, 1) == -1 && bv_choose(v, 0) == -1);
}
