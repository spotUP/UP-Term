/* amarchy-icons APPLY / RESTORE (amarchy/iconapply_core.c; plan 2026-10-31-amarchy-lucide-icons.md rows L8, L9).
 * Drives the real ia_apply / ia_restore over an in-memory file system with the stock 3.2 fixtures
 * (tests/data/icons/os322-tool.info, os322-drawer.info) and a built-here lucide.dat. The sentinel of "the new code ran" is
 * the ia_ctx counters plus the bytes in the fake file system. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../amarchy/iconapply_core.h"
#include "harness.h"

/* named checks: CK(message, condition), CI(message, got, want) */
#define CK(msg, cond) do { h_checks++; if (!(cond)) { h_failures++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } } while (0)
#define CI(msg, got, want) do { long g_ = (long)(got), w_ = (long)(want); h_checks++; \
    if (g_ != w_) { h_failures++; printf("  FAIL %s:%d: %s = %ld, want %ld\n", __FILE__, __LINE__, msg, g_, w_); } } while (0)

#define NFILES 64
typedef struct mf { char path[200]; uint8_t *d; size_t len; int used; } mf;
typedef struct mfs {
    mf f[NFILES];
    int writes, removes;
    const char *fail_write;     /* a write whose path contains this fails */
    uint8_t dflt[IA_FILECAP]; long dflt_len;
} mfs;

static mf *mf_find(mfs *s, const char *p)
{
    int i;
    for (i = 0; i < NFILES; i++) if (s->f[i].used && strcmp(s->f[i].path, p) == 0) return &s->f[i];
    return NULL;
}
static void mf_put(mfs *s, const char *p, const uint8_t *d, size_t n)
{
    mf *m = mf_find(s, p);
    int i;
    if (!m) { i = 0; while (i < NFILES && s->f[i].used) i++; m = &s->f[i]; m->used = 1; strcpy(m->path, p); m->d = NULL; }
    free(m->d);
    m->d = malloc(n ? n : 1);
    memcpy(m->d, d, n);
    m->len = n;
}
static long m_read(void *c, const char *p, uint8_t *buf, size_t cap)
{
    mf *m = mf_find((mfs *)c, p);
    if (!m) return -1;
    if (m->len > cap) return -2;
    memcpy(buf, m->d, m->len);
    return (long)m->len;
}
static int m_write(void *c, const char *p, const uint8_t *buf, size_t n)
{
    mfs *s = c;
    if (s->fail_write && strstr(p, s->fail_write)) return -1;
    s->writes++;
    mf_put(s, p, buf, n);
    return 0;
}
static int m_remove(void *c, const char *p)
{
    mfs *s = c;
    mf *m = mf_find(s, p);
    if (!m) return 0;
    free(m->d); m->d = NULL; m->used = 0;
    s->removes++;
    return 0;
}
static long m_dflt(void *c, const char *match, uint8_t *buf, size_t cap)
{
    mfs *s = c;
    (void)match; (void)cap;
    if (s->dflt_len <= 0) return -1;
    memcpy(buf, s->dflt, (size_t)s->dflt_len);
    return s->dflt_len;
}
static const ia_io g_io = { NULL, m_read, m_write, m_remove, m_dflt };

static void mfs_free(mfs *s) { int i; for (i = 0; i < NFILES; i++) if (s->f[i].used) free(s->f[i].d); }

static long slurp(const char *path, uint8_t *buf, long cap)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) return -1;
    n = (long)fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return n;
}
static size_t w16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; return 2; }
static size_t w32(uint8_t *p, uint32_t v) { w16(p, v >> 16); w16(p + 2, v & 0xFFFF); return 4; }

#define NDAT 8
/* a lucide.dat with the six defaults and two table paths (sorted by strcmp), variants 32x32 and 32x16, depth 6 */
static size_t make_dat(uint8_t *d)
{
    static const char *const nm[NDAT] = { "SYS:System", "SYS:System/Shell", "def_RAM", "def_disk", "def_drawer", "def_project", "def_tool", "def_trashcan" };
    size_t pos = 4, sz0 = 6 * 32 * 4, sz1 = 6 * 16 * 4, rec = 4 + 12 + 8 + NDAT * 52, i, k;
    memcpy(d, "LUCI", 4);
    pos += w16(d + pos, 1); pos += w16(d + pos, NDAT); pos += w16(d + pos, 6); pos += w16(d + pos, 2); pos += w32(d + pos, 0);
    pos += w16(d + pos, 32); pos += w16(d + pos, 32); pos += w16(d + pos, 32); pos += w16(d + pos, 16);
    for (k = 0; k < NDAT; k++) {
        size_t base = rec + k * 2 * (sz0 + sz1);
        memset(d + pos, 0, 52);
        memcpy(d + pos, nm[k], strlen(nm[k]));
        w32(d + pos + 48, (uint32_t)base);
        pos += 52;
        for (i = 0; i < 2 * (sz0 + sz1); i++) d[base + i] = (uint8_t)(0x20 + k * 8 + (i < 2 * sz0 ? 0 : 1));
    }
    return rec + NDAT * 2 * (sz0 + sz1);
}

static uint8_t g_dat[IA_FILECAP], g_tool[IA_FILECAP], g_drawer[IA_FILECAP];
static long g_tool_n, g_drawer_n;
static uint8_t g_cur[IA_FILECAP], g_out[IA_FILECAP], g_bak[IA_FILECAP];
static li_dat g_d;

static void setup(mfs *s, ia_ctx *c, int variant)
{
    int i;
    static const char *const hv[] = { "ENVARC:Sys/def_tool.info", "ENVARC:Sys/def_drawer.info", "ENVARC:Sys/def_RAM.info",
                                      "ENV:Sys/def_tool.info", "ENV:Sys/def_drawer.info", "ENV:Sys/def_RAM.info" };
    memset(s, 0, sizeof *s);
    for (i = 0; i < 6; i++) mf_put(s, hv[i], (i % 3 == 1) ? g_drawer : g_tool, (size_t)((i % 3 == 1) ? g_drawer_n : g_tool_n));
    memcpy(s->dflt, g_drawer, (size_t)g_drawer_n);
    s->dflt_len = g_drawer_n;
    memset(c, 0, sizeof *c);
    {
        static ia_io io;
        io = g_io; io.ctx = s;
        c->io = &io;
    }
    c->dat = &g_d; c->variant = variant;
    c->cur = g_cur; c->out = g_out; c->bak = g_bak;
}

static int same(mfs *s, const char *p, const uint8_t *d, long n)
{
    mf *m = mf_find(s, p);
    return m && (long)m->len == n && memcmp(m->d, d, (size_t)n) == 0;
}

static void test_pure(void)
{
    ia_manifest m, m2;
    ia_entry e;
    char txt[1024], t[64], o[64];
    size_t n;
    uint8_t z[4] = { 1, 2, 3, 4 };

    CI("crc32 of 123456789", ia_crc32((const uint8_t *)"123456789", 9), 0xCBF43926u);
    (void)z;
    memset(&m, 0, sizeof m);
    memset(&e, 0, sizeof e);
    strcpy(e.path, "ENVARC:Sys/def_tool.info"); e.ocrc = 0xDEADBEEFu; e.wcrc = 0x12345678u; ia_man_put(&m, &e);
    strcpy(e.path, "ENV:Sys/def_disk.info"); e.absent = 1; e.ocrc = 0; e.wcrc = 0xABCDEF01u; ia_man_put(&m, &e);
    n = ia_man_format(&m, txt, sizeof txt);
    CK("manifest formats", n > 0);
    CK("manifest header", strncmp(txt, "# amarchy-icons manifest 1\n", 27) == 0);
    CK("manifest line", strstr(txt, "ENVARC:Sys/def_tool.info ; deadbeef ; 12345678\n") != NULL);
    CK("manifest absent line", strstr(txt, "ENV:Sys/def_disk.info ; absent ; abcdef01\n") != NULL);
    CI("manifest parse count", ia_man_parse(&m2, txt, n), 2);
    CK("manifest round trip", m2.e[0].ocrc == 0xDEADBEEFu && m2.e[1].absent && m2.e[1].wcrc == 0xABCDEF01u);
    ia_man_del(&m2, 0);
    CK("manifest delete", m2.n == 1 && strcmp(m2.e[0].path, "ENV:Sys/def_disk.info") == 0);
    CI("manifest ignores junk", ia_man_parse(&m2, "# x\nnonsense\n\na ; b\n", 20), 0);

    CI("atomic names", ia_atomic_names("RAM:a.info", t, o, sizeof t), 0);
    CK("atomic tmp", strcmp(t, "RAM:a.info.amt") == 0 && strcmp(o, "RAM:a.info.amo") == 0);
    CI("atomic names too long", ia_atomic_names("RAM:a.info", t, o, 12), -1);
    CI("recover nothing", ia_recover(1, 1, 0, 0, 0, 0), IA_REC_NONE);
    CI("recover amo first", ia_recover(0, 0, 1, 1, 1, 1), IA_REC_OLD_TO_NAME);
    CI("recover amt", ia_recover(0, 0, 1, 1, 0, 0), IA_REC_TMP_TO_NAME);
    CI("recover bad amt deleted", ia_recover(1, 1, 1, 0, 0, 0), IA_REC_DEL_TMP);
    CI("recover amt not over good name", ia_recover(1, 1, 1, 1, 0, 0), IA_REC_DEL_TMP);

    CI("cmd APPLY", ia_parse_cmd("apply"), IA_CMD_APPLY);
    CI("cmd RESTORE", ia_parse_cmd("  RESTORE\n"), IA_CMD_RESTORE);
    CI("cmd STATUS", ia_parse_cmd("STATUS"), IA_CMD_STATUS);
    CI("cmd help", ia_parse_cmd("?"), IA_CMD_HELP);
    CI("cmd none", ia_parse_cmd(""), IA_CMD_NONE);
    CI("cmd prefix is not a command", ia_parse_cmd("APPLYX"), IA_CMD_NONE);
}

static void test_flow(void)
{
    mfs s;
    ia_ctx c;
    char path[IA_MAXPATH], bp[200];
    const char *match;
    int i, ok, nbak = 0;
    uint8_t before[12][1]; (void)before;

    setup(&s, &c, 0);
    CI("apply writes all twelve", ia_apply(&c), 12);
    CI("counter written", (int)c.written, 12);
    ok = 1;
    for (i = 0; ia_target(i, path, sizeof path, &match); i++) {
        mf *m = mf_find(&s, path);
        li_file f;
        ok &= m && li_parse(m->d, m->len, &f) == 0;
        ok &= m && !((i % 6 == 1) ? same(&s, path, g_drawer, g_drawer_n) : same(&s, path, g_tool, g_tool_n)) ;
    }
    CK("every target is now a changed, parseable icon", ok);
    CK("manifest written", mf_find(&s, IA_MANIFEST) != NULL);
    CK("backup of an original equals the original", li_backup_path("ENVARC:Sys/def_tool.info", bp, sizeof bp) == 0 && same(&s, bp, g_tool, g_tool_n));
    CK("backup of the ENV copy", li_backup_path("ENV:Sys/def_drawer.info", bp, sizeof bp) == 0 && same(&s, bp, g_drawer, g_drawer_n));
    for (i = 0; i < NFILES; i++) if (s.f[i].used && strstr(s.f[i].path, "SYS:Storage/Icons/Amarchy/") == s.f[i].path && !strstr(s.f[i].path, "manifest")) nbak++;
    CI("six backups (one per existing original, none for the six absent ones)", nbak, 6);

    {   /* idempotence: the second APPLY must not back the Lucide icon up over the original */
        int w0 = s.writes;
        CI("second apply writes nothing", ia_apply(&c), 0);
        CI("second apply skipped_done", (int)c.skipped_done, 12);
        CI("second apply made no write", s.writes, w0);
        CK("backup still the original after two applies", li_backup_path("ENVARC:Sys/def_tool.info", bp, sizeof bp) == 0 && same(&s, bp, g_tool, g_tool_n));
    }

    CI("restore restores all", ia_restore(&c), 12);
    ok = 1;
    for (i = 0; ia_target(i, path, sizeof path, &match); i++) {
        int k = i % 6;      /* disk, drawer, tool, project, trashcan, RAM: setup() has drawer, tool and RAM only */
        if (k == 1 || k == 2 || k == 5) ok &= same(&s, path, k == 1 ? g_drawer : g_tool, k == 1 ? g_drawer_n : g_tool_n);
        else ok &= mf_find(&s, path) == NULL;
    }
    CK("restore gives byte-identical originals and removes the files that did not exist", ok);
    CK("manifest removed with the last entry", mf_find(&s, IA_MANIFEST) == NULL);
    nbak = 0;
    for (i = 0; i < NFILES; i++) if (s.f[i].used && strstr(s.f[i].path, "SYS:Storage/Icons/Amarchy/") == s.f[i].path) nbak++;
    CI("backup files removed", nbak, 0);
    mfs_free(&s);
}

/* L10: the table's path rows. APPLY over SYS:System.info (drawer) and SYS:System/Shell.info (tool) writes both with a backup;
 * a path row whose file is absent from the volume is skipped, not created. */
static void test_paths(void)
{
    mfs s;
    ia_ctx c;
    char bp[200];
    setup(&s, &c, 0);
    mf_put(&s, "SYS:System.info", g_drawer, (size_t)g_drawer_n);
    mf_put(&s, "SYS:System/Shell.info", g_tool, (size_t)g_tool_n);
    CI("apply writes twelve defaults plus two table paths", ia_apply(&c), 14);
    CK("SYS:System.info changed", !same(&s, "SYS:System.info", g_drawer, g_drawer_n));
    CK("SYS:System/Shell.info changed", !same(&s, "SYS:System/Shell.info", g_tool, g_tool_n));
    CK("backup of the drawer icon is the original", li_backup_path("SYS:System.info", bp, sizeof bp) == 0 && same(&s, bp, g_drawer, g_drawer_n));
    ia_restore(&c);
    CK("restore gives SYS:System.info back", same(&s, "SYS:System.info", g_drawer, g_drawer_n));
    CK("restore gives SYS:System/Shell.info back", same(&s, "SYS:System/Shell.info", g_tool, g_tool_n));
    mfs_free(&s);
    setup(&s, &c, 0);
    CI("table paths absent from the volume are skipped", ia_apply(&c), 12);
    CK("and not created", mf_find(&s, "SYS:System.info") == NULL);
    mfs_free(&s);
}

/* STATUS: no manifest -> original; after APPLY with two table paths the line carries the entry counts by kind */
static void test_status_counts(void)
{
    mfs s;
    ia_ctx c;
    char line[160];
    setup(&s, &c, 0);
    CK("STATUS without a manifest says original", ia_status(&c, line, sizeof line) > 0 && strcmp(line, "icons=original\n") == 0);
    mf_put(&s, "SYS:System.info", g_drawer, (size_t)g_drawer_n);
    mf_put(&s, "SYS:System/Shell.info", g_tool, (size_t)g_tool_n);
    ia_apply(&c);
    memset(line, 0, sizeof line);
    ia_status(&c, line, sizeof line);
    CK("STATUS reports manifest entry counts", strstr(line, "icons=applied entries=14 defaults=12 paths=2 walked=0\n") != NULL);
    mfs_free(&s);
}

static void test_absent(void)
{
    mfs s;
    ia_ctx c;
    int i;
    setup(&s, &c, 0);
    mf_put(&s, "x", g_tool, 1);   /* keep the table non-trivial */
    /* ENVARC:Sys/def_disk.info, def_project, def_trashcan and the ENV copies are absent: the template stands in */
    ia_apply(&c);
    CK("absent origin gets the default plus Lucide", mf_find(&s, "ENVARC:Sys/def_disk.info") != NULL);
    {
        char txt[4096];
        long n = m_read(&s, IA_MANIFEST, (uint8_t *)txt, sizeof txt - 1);
        txt[n > 0 ? n : 0] = 0;
        CK("manifest marks absent origins", strstr(txt, "ENVARC:Sys/def_disk.info ; absent ;") != NULL);
    }
    ia_restore(&c);
    CK("restore removes a file that did not exist", mf_find(&s, "ENVARC:Sys/def_disk.info") == NULL);
    CK("restore keeps the existing originals", same(&s, "ENVARC:Sys/def_tool.info", g_tool, g_tool_n));
    (void)i;
    mfs_free(&s);
}

static void test_edge(void)
{
    mfs s;
    ia_ctx c;
    char bp[200];
    int w0;

    /* RESTORE with no manifest is a no-op */
    setup(&s, &c, 0);
    w0 = s.writes;
    CI("restore without manifest", ia_restore(&c), 0);
    CK("restore without manifest wrote and removed nothing", s.writes == w0 && s.removes == 0 && c.restored == 0);
    mfs_free(&s);

    /* the icon file is rewritten by the user after APPLY: the next APPLY refreshes the backup (not the old original) */
    setup(&s, &c, 0);
    ia_apply(&c);
    li_backup_path("ENVARC:Sys/def_tool.info", bp, sizeof bp);
    {
        mf_put(&s, "ENVARC:Sys/def_tool.info", g_drawer, (size_t)g_drawer_n);   /* the user's own icon: images differ from the manifest crc */
        c.written = c.skipped_done = 0;
        ia_apply(&c);
        CK("changed icon is written again", c.written >= 1);
        CK("backup refreshed to the file the user had", same(&s, bp, g_drawer, g_drawer_n));
        CK("backup is the previous file, not the stock original", !same(&s, bp, g_tool, g_tool_n));
    }
    mfs_free(&s);

    /* crash order: the icon write fails after backup and manifest were written; the original must stay recoverable */
    setup(&s, &c, 0);
    s.fail_write = "ENVARC:Sys/def_tool.info";
    ia_apply(&c);
    CK("failed icon write left the original", same(&s, "ENVARC:Sys/def_tool.info", g_tool, g_tool_n));
    CK("failed icon write counted", c.skipped_io >= 1);
    CK("failed icon write leaves no manifest entry", ia_man_find(&c.man, "ENVARC:Sys/def_tool.info") < 0);
    CK("failed icon write leaves no manifest entry (saved file)", !mf_find(&s, IA_MANIFEST) || mf_find(&s, IA_MANIFEST)->len == 0 || !memmem(mf_find(&s, IA_MANIFEST)->d, mf_find(&s, IA_MANIFEST)->len, "ENVARC:Sys/def_tool.info", 24));
    CK("failed icon write leaves no orphan backup", li_backup_path("ENVARC:Sys/def_tool.info", bp, sizeof bp) == 0 && !mf_find(&s, bp));
    s.fail_write = NULL;
    c.written = 0;
    ia_apply(&c);
    CK("re-run after the failure applies it", c.written >= 1 && !same(&s, "ENVARC:Sys/def_tool.info", g_tool, g_tool_n));
    CK("backup is the original after the re-run", same(&s, bp, g_tool, g_tool_n));
    ia_restore(&c);
    CK("restore after the re-run gives the original", same(&s, "ENVARC:Sys/def_tool.info", g_tool, g_tool_n));
    mfs_free(&s);

    /* a missing backup keeps its manifest entry so a later run can still restore */
    setup(&s, &c, 0);
    ia_apply(&c);
    li_backup_path("ENVARC:Sys/def_tool.info", bp, sizeof bp);
    m_remove(&s, bp);
    ia_restore(&c);
    CI("restore with a lost backup reports failure", (int)c.restore_failed, 1);
    CK("entry kept", mf_find(&s, IA_MANIFEST) != NULL);
    mfs_free(&s);
}

static void test_walk(void)
{
    ia_ctx c;
    ia_walk_entry e = { "Foo.info", 0, 0, 1, 3 };
    memset(&c, 0, sizeof c);
    CI("normal file is still written", ia_walk_decide(&e), IA_DO_WRITE);
    e.vol_readonly = 1;
    CI("read-only volume skips", ia_walk_count(&c, ia_walk_decide(&e)), IA_DO_SKIP_READONLY);
    e.vol_readonly = 0; e.prot = IA_PROT_WRITE;
    CI("write-protected file skips as locked", ia_walk_count(&c, ia_walk_decide(&e)), IA_DO_SKIP_LOCKED);
    e.prot = IA_PROT_DELETE;
    CI("undeletable file skips as locked", ia_walk_decide(&e), IA_DO_SKIP_LOCKED);
    e.prot = 0x02;
    CI("an unrelated protection bit does not lock", ia_walk_decide(&e), IA_DO_WRITE);
    e.name = "Foo.info.uaem";
    CI("uaem sidecar skips", ia_walk_count(&c, ia_walk_decide(&e)), IA_DO_SKIP_UAEM);
    e.name = "Foo.INFO.UAEM"; e.vol_readonly = 1;
    CI("uaem beats read-only, case-insensitive", ia_walk_decide(&e), IA_DO_SKIP_UAEM);
    e.name = "Foo.info"; e.vol_readonly = 0; e.specific = 0;
    CI("no specific entry falls back to the type", ia_walk_count(&c, ia_walk_decide(&e)), IA_DO_FALLBACK);
    CK("type fallback names", !strcmp(ia_walk_fallback_match(2), "def_drawer") && !strcmp(ia_walk_fallback_match(3), "def_tool")
       && !ia_walk_fallback_match(99));
    CK("counters moved once each", c.skipped_readonly == 1 && c.skipped_locked == 1 && c.skipped_uaem == 1 && c.fallback == 1 && c.skipped_deep == 0);
    CI("fits: shallow short path", ia_walk_fits(&c, 10, 5, 64, 3, 24, 1), 1);
    CI("fits: too deep dir refused", ia_walk_fits(&c, 10, 5, 64, 24, 24, 1), 0);
    CI("fits: deep file still fine", ia_walk_fits(&c, 10, 5, 64, 24, 24, 0), 1);
    CI("fits: too long path refused", ia_walk_fits(&c, 60, 5, 64, 3, 24, 0), 0);
    CK("deep counter bumped twice", c.skipped_deep == 2);
}

static void test_walkfile(void)
{
    mfs s;
    ia_ctx c;
    char bp[IA_MAXPATH + 40];
    mf *m;
    setup(&s, &c, 0);
    CI("drawer fixture type", ia_info_type(g_drawer, (size_t)g_drawer_n), 2);
    CI("tool fixture type", ia_info_type(g_tool, (size_t)g_tool_n), 3);
    CK("backup drawer is skipped", ia_walk_skip_dir("Boot:Storage/Icons/Amarchy") && !ia_walk_skip_dir("Boot:Storage/Icons"));
    mf_put(&s, "Work:Games/Foo.info", g_drawer, (size_t)g_drawer_n);
    mf_put(&s, "Work:Games/Bar.info", g_tool, (size_t)g_tool_n);
    mf_put(&s, "Work:Games/Foo.info.uaem", g_tool, (size_t)g_tool_n);
    mf_put(&s, "Work:Games/Locked.info", g_tool, (size_t)g_tool_n);
    mf_put(&s, "Rom:Disk.info", g_tool, (size_t)g_tool_n);
    ia_walk_begin(&c);
    CI("no specific entry: type fallback", ia_walk_file(&c, "Work:Games/Foo.info", 0, 0), IA_DO_FALLBACK);
    CI("fallback wrote through ia_apply's path", (int)c.written, 1);
    CK("file now differs", !same(&s, "Work:Games/Foo.info", g_drawer, g_drawer_n));
    CK("original backed up under the Amarchy drawer", li_backup_path("Work:Games/Foo.info", bp, sizeof bp) == 0 && same(&s, bp, g_drawer, g_drawer_n));
    m = mf_find(&s, IA_MANIFEST);
    CK("manifest lists the walked file", m != NULL && strstr((const char *)m->d, "Work:Games/Foo.info") != NULL);
    CI("second visit is idempotent (already Lucide)", ia_walk_file(&c, "Work:Games/Foo.info", 0, 0), IA_DO_SKIP_OTHER);
    CI("written unchanged by a repeat", (int)c.written, 1);
    ia_walk_file(&c, "Work:Games/Foo.info.uaem", 0, 0);
    ia_walk_file(&c, "Work:Games/Locked.info", 0, IA_PROT_WRITE);
    ia_walk_file(&c, "Rom:Disk.info", 1, 0);
    CK("skips counted, nothing written", c.skipped_uaem == 1 && c.skipped_locked == 1 && c.skipped_readonly == 1 && c.written == 1);
    CK("locked and read-only files untouched", same(&s, "Work:Games/Locked.info", g_tool, g_tool_n) && same(&s, "Rom:Disk.info", g_tool, g_tool_n));
    ia_restore(&c);
    CK("restore puts the walked original back", same(&s, "Work:Games/Foo.info", g_drawer, g_drawer_n));
    mfs_free(&s);
}

/* rig 8: one failing entry stopped RESTORE, so the walked entries behind it were never restored */
static void test_restore_continues(void)
{
    mfs s;
    ia_ctx c;
    char bp[IA_MAXPATH + 40];
    int i, nwalk = 0;
    static const char *const w[] = { "Empty-12MB-FFS:L12/zzfallback.info", "Empty-12MB-FFS:L12/zzuaemdir.info", "Work:Games/Foo.info" };
    setup(&s, &c, 0);
    CI("defaults applied", ia_apply(&c), 12);
    for (i = 0; i < 3; i++) mf_put(&s, w[i], g_drawer, (size_t)g_drawer_n);
    ia_walk_begin(&c);
    for (i = 0; i < 3; i++) nwalk += ia_walk_file(&c, w[i], 0, 0) == IA_DO_FALLBACK;
    CI("walked entries written", nwalk, 3);
    CI("manifest has table plus walked entries", c.man.n, 15);
    {   /* rig 8/9: an absent entry whose icon write never landed (APPLY skipped-io) read back as an error and failed RESTORE for good */
        static char mt[IA_FILECAP];
        ia_entry ab;
        memset(&ab, 0, sizeof ab);
        strcpy(ab.path, "ENV:Sys/def_neverwritten.info");
        ab.absent = 1; ab.wcrc = 0x1234;
        ia_man_put(&c.man, &ab);
        mf_put(&s, IA_MANIFEST, (const uint8_t *)mt, ia_man_format(&c.man, mt, sizeof mt));   /* RESTORE reloads the file */
        mf_put(&s, ab.path, g_drawer, 0);                   /* zero-length: read returns 0, not -1 */
    }
    /* break the backup of the FIRST table entry: it fails, everything behind it must still be restored */
    for (i = 0; c.man.e[i].absent; i++) ;
    CK("backup path", li_backup_path(c.man.e[i].path, bp, sizeof bp) == 0 && mf_find(&s, bp) != NULL);
    mf_find(&s, bp)->used = 0;
    c.restored = c.restore_failed = 0;
    CI("restore returns all but the broken one", ia_restore(&c), 15);
    CI("restored", (int)c.restored, 15);
    CI("one failure counted", (int)c.restore_failed, 1);
    for (i = 0; i < 3; i++) CK("walked file restored", same(&s, w[i], g_drawer, g_drawer_n));
    CI("only the failed entry remains in the manifest", c.man.n, 1);
    mfs_free(&s);
}

void suite_iconapply(void)
{
    long dn;
    g_tool_n = slurp("tests/data/icons/os322-tool.info", g_tool, IA_FILECAP);
    g_drawer_n = slurp("tests/data/icons/os322-drawer.info", g_drawer, IA_FILECAP);
    CK("fixtures read", g_tool_n > 0 && g_drawer_n > 0);
    dn = (long)make_dat(g_dat);
    CI("dat opens", li_dat_open(g_dat, (size_t)dn, &g_d), 0);
    test_pure();
    test_flow();
    test_absent();
    test_paths();
    test_status_counts();
    test_edge();
    test_walk();
    test_walkfile();
    test_restore_continues();
}
