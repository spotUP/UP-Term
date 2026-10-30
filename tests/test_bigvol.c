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

/* the fake SameLock: groups whose volume names (the part before the colon, case folded) match */
static int same_name(int i, int j, void *ctx)
{
    const struct bv_group *g = (const struct bv_group *)ctx;
    char a[32], b[32];
    int k;
    if (!bv_volume(g[i].path, a, sizeof a) || !bv_volume(g[j].path, b, sizeof b))
        return 0;
    for (k = 0; a[k] && b[k]; k++)
        if ((a[k] | 32) != (b[k] | 32))
            return 0;
    return a[k] == b[k];
}

/* UPBigVol ROOM: the words install.dos passes, grouped, merged per volume, against the free room */
static void suite_bigvol_room(void)
{
    char out[16];
    struct bv_group g[4];
    char *w1[] = {"SYS:UP-Term", "17000", "0", "40000", "SYS:", "3000", "600", "Work:", "2500"};
    char *w2[] = {"VTC:KitTest/UP-Term", "57000", "SYS:", "3600"};
    char *bad1[] = {"12", "SYS:"};
    char *bad2[] = {"A:", "B:", "C:", "D:", "E:"};
    char *big[] = {"SYS:", "4294967295", "1"};
    CHECK(bv_is_kb("0") && bv_is_kb("40213") && !bv_is_kb("") && !bv_is_kb("SYS:") && !bv_is_kb("12k") && !bv_is_kb(0));
    CHECK(bv_kb("40213") == 40213UL && bv_kb("99999999999") == 0xFFFFFFFFUL);
    CHECK(bv_add(1, 2) == 3 && bv_add(0xFFFFFFF0UL, 0x20) == 0xFFFFFFFFUL);
    /* the volume part: the drawer does not exist yet, its disk does */
    CHECK(bv_volume("SYS:UP-Term", out, sizeof out) && !strcmp(out, "SYS:"));
    CHECK(bv_volume("DH1:KitTest-all/UP-Term", out, sizeof out) && !strcmp(out, "DH1:"));
    CHECK(bv_volume("UP-Term", out, sizeof out) && !strcmp(out, ""));
    CHECK(!bv_volume("AVolumeNameLongerThanSixteen:x", out, sizeof out));
    /* the default drawer on SYS:: one volume, every part counted against it */
    CHECK(bv_parse(w1, 9, g, 4) == 3);
    CHECK(g[0].kb == 57000 && g[1].kb == 3600 && g[2].kb == 2500 && !strcmp(g[2].path, "Work:"));
    bv_merge(g, 3, same_name, g);
    CHECK(g[0].same_as == -1 && g[1].same_as == 0 && g[2].same_as == -1);
    CHECK(g[0].kb == 60600 && g[2].kb == 2500);
    /* the 106 MB rig System disk with 50 MB free refuses; 70 MB free takes it */
    CHECK(bv_short(51200, g[0].kb) && !bv_short(71680, g[0].kb) && !bv_short(60600, 60600));
    /* another drawer: two volumes, no merge */
    CHECK(bv_parse(w2, 4, g, 4) == 2);
    bv_merge(g, 2, same_name, g);
    CHECK(g[0].same_as == -1 && g[1].same_as == -1 && g[0].kb == 57000 && g[1].kb == 3600);
    /* a KB word before any path, or more groups than room: -1 (the helper cannot tell, says so, the install goes on) */
    CHECK(bv_parse(bad1, 2, g, 4) == -1 && bv_parse(bad2, 5, g, 4) == -1 && bv_parse(bad2, 0, g, 4) == 0);
    /* KB held at 0xFFFFFFFF, never wrapped to a small number that passes */
    CHECK(bv_parse(big, 3, g, 4) == 1 && g[0].kb == 0xFFFFFFFFUL && bv_short(0xFFFFFFFEUL, g[0].kb));
    /* the refusal text, the same for the Shell install's print and the Installer's ENV:UPTROOM */
    {
        char t[160], s[12];
        bv_room_text(t, sizeof t, "SYS:UP-Term", 51200, 60600);
        CHECK(!strcmp(t, "SYS:UP-Term has 51200 KB free;\nthe parts you chose need about 60600 KB."));
        bv_room_text(t, sizeof t, "Work:", 0, 4294967295UL);
        CHECK(!strcmp(t, "Work: has 0 KB free;\nthe parts you chose need about 4294967295 KB."));
        bv_room_text(s, sizeof s, "SYS:UP-Term", 1, 2);
        CHECK(!strcmp(s, "SYS:UP-Term")); /* 11 characters and the terminator: the " has" is cut */
    }
}

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
    suite_bigvol_room();
}
