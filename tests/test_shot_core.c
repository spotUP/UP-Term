/* shot_core (amarchy/shot_core.c): ByteRun1 packer edges, chunky to planes, the ILBM writer byte for byte against
 * tests/data/shot_4x3_d2.ilbm (made by hand from the IFF spec), and a round trip through the independent decoder
 * tools/ilbm_decode.py (no shared code). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "harness.h"
#include "../amarchy/shot_core.h"

typedef struct { unsigned char *b; long n, cap; int calls_write, calls_patch, fail_after; } msink;

static int m_write(void *ud, const void *buf, long n)
{
    msink *m = (msink *)ud;
    m->calls_write++;
    if (m->fail_after > 0 && m->calls_write > m->fail_after)
        return 1;
    if (m->n + n > m->cap) {
        m->cap = (m->n + n) * 2;
        m->b = (unsigned char *)realloc(m->b, (size_t)m->cap);
    }
    memcpy(m->b + m->n, buf, (size_t)n);
    m->n += n;
    return 0;
}

static int m_patch(void *ud, long off, unsigned long v)
{
    msink *m = (msink *)ud;
    m->calls_patch++;
    if (off < 0 || off + 4 > m->n)
        return 1;
    m->b[off] = (unsigned char)(v >> 24);
    m->b[off + 1] = (unsigned char)(v >> 16);
    m->b[off + 2] = (unsigned char)(v >> 8);
    m->b[off + 3] = (unsigned char)v;
    return 0;
}

static long pack_of(int byte_run_kind, long n, unsigned char *out)
{
    unsigned char in[600];
    long i;
    for (i = 0; i < n; i++)
        in[i] = (unsigned char)(byte_run_kind == 0 ? 7 : (i & 1 ? 1 : 2));
    return shot_pack(in, n, out);
}

static void packer_edges(void)
{
    unsigned char o[700], in[300];
    long i, n;

    n = pack_of(0, 1, o);                                   /* 1 equal byte: a literal of 1 */
    CHECK_INT(n, 2); CHECK_INT(o[0], 0); CHECK_INT(o[1], 7);
    n = pack_of(0, 2, o);                                   /* 2 equal: literal 2 (a run costs as much) */
    CHECK_INT(n, 3); CHECK_INT(o[0], 1);
    n = pack_of(0, 3, o);                                   /* 3 equal: a run */
    CHECK_INT(n, 2); CHECK_INT(o[0], 254); CHECK_INT(o[1], 7);
    n = pack_of(0, 127, o);
    CHECK_INT(n, 2); CHECK_INT(o[0], 257 - 127);
    n = pack_of(0, 128, o);                                 /* the longest run: 0x81 */
    CHECK_INT(n, 2); CHECK_INT(o[0], 0x81);
    n = pack_of(0, 129, o);                                 /* 128-run + 1 literal */
    CHECK_INT(n, 4); CHECK_INT(o[0], 0x81); CHECK_INT(o[2], 0); CHECK_INT(o[3], 7);
    n = pack_of(1, 128, o);                                 /* 128 alternating: one literal of 128 */
    CHECK_INT(n, 129); CHECK_INT(o[0], 127);
    n = pack_of(1, 129, o);                                 /* literal 128 then literal 1 */
    CHECK_INT(n, 131); CHECK_INT(o[0], 127); CHECK_INT(o[129], 0);
    /* a run inside literals splits them: ab cccc d */
    memcpy(in, "abccccd", 7);
    n = shot_pack(in, 7, o);
    CHECK_INT(n, 3 + 2 + 2); CHECK_INT(o[0], 1); CHECK_INT(o[3], 253); CHECK_INT(o[4], 'c'); CHECK_INT(o[5], 0);
    /* a pair beside literals stays literal: abbc is one literal of 4 */
    memcpy(in, "abbc", 4);
    n = shot_pack(in, 4, o);
    CHECK_INT(n, 5); CHECK_INT(o[0], 3);
    for (i = 0; i < 300; i++) in[i] = (unsigned char)(i * 7);
    CHECK(shot_pack_max(300) >= shot_pack(in, 300, o));
}

static void planes_and_padding(void)
{
    unsigned char pix[19], out[2 * 4 * 3];
    long i;
    for (i = 0; i < 19; i++) pix[i] = (unsigned char)(i & 7);
    shot_planes(pix, 19, 3, out);                           /* 19 pixels: row padded to 4 bytes per plane */
    CHECK_INT(out[0], 0x55);                                /* plane 0: 0,1,0,1... */
    CHECK_INT(out[4], 0x33);                                /* plane 1: 0,0,1,1,... */
    CHECK_INT(out[8], 0x0F);                                /* plane 2 */
    CHECK_INT(out[3], 0);                                   /* padding bits and bytes clear */
    CHECK_INT(out[2] & 0x1F, 0);
}

static void golden_bytes(void)
{
    static const unsigned char cmap[12] = { 0,0,0, 255,0,0, 0,255,0, 0,0,255 };
    static const unsigned char rows[3][4] = { { 0,1,2,3 }, { 3,3,3,3 }, { 0,0,0,0 } };
    msink m;
    shot_sink s;
    shot_writer w;
    unsigned char g[256];
    FILE *f = fopen("tests/data/shot_4x3_d2.ilbm", "rb");
    long gn, i;

    CHECK(f != NULL);
    if (!f) return;
    gn = (long)fread(g, 1, sizeof g, f);
    fclose(f);
    memset(&m, 0, sizeof m);
    s.ud = &m; s.write = m_write; s.patch32 = m_patch;
    CHECK_INT(shot_begin(&w, &s, 4, 3, 2, cmap, 0x8000), 0);
    for (i = 0; i < 3; i++)
        CHECK_INT(shot_row(&w, rows[i]), 0);
    CHECK_INT(shot_end(&w), 0);
    CHECK_INT(m.calls_patch, 2);                            /* sentinel: sizes patched, not guessed */
    CHECK_INT(m.n, gn);
    CHECK(m.n == gn && memcmp(m.b, g, (size_t)gn) == 0);
    free(m.b);
}

static void refusals(void)
{
    static const unsigned char cmap[6] = { 0,0,0, 1,1,1 };
    unsigned char row[8] = { 0 };
    msink m;
    shot_sink s;
    shot_writer w;

    memset(&m, 0, sizeof m);
    s.ud = &m; s.write = m_write; s.patch32 = m_patch;
    CHECK_INT(shot_begin(&w, &s, 0, 3, 1, cmap, 0), -1);
    CHECK_INT(shot_begin(&w, &s, 8, 3, 9, cmap, 0), -1);
    CHECK_INT(shot_begin(&w, &s, 8, 1, 1, cmap, 0), 0);
    CHECK_INT(shot_end(&w), -1);                            /* a row short */
    CHECK_INT(shot_begin(&w, &s, 8, 1, 1, cmap, 0), 0);
    CHECK_INT(shot_row(&w, row), 0);
    CHECK_INT(shot_row(&w, row), -1);                       /* a row too many */
    CHECK_INT(shot_end(&w), 0);
    free(m.b);
    memset(&m, 0, sizeof m);
    m.fail_after = 3;                                       /* the sink fails mid-header */
    CHECK_INT(shot_begin(&w, &s, 8, 1, 1, cmap, 0x8000), -1);
    free(m.b);
}

/* A deterministic picture with long runs, noise and an odd width, written, saved, decoded by Python, compared. */
static void round_trip(long wd, long ht, int depth)
{
    unsigned char cmap[768], *img, row[512];
    msink m;
    shot_sink s;
    shot_writer w;
    char line[8192], *p;
    FILE *f, *pp;
    long x, y, i;
    unsigned seed = 12345;
    int ok = 1, mask = (1 << depth) - 1;

    img = (unsigned char *)malloc((size_t)(wd * ht));
    for (y = 0; y < ht; y++)
        for (x = 0; x < wd; x++) {
            seed = seed * 1103515245u + 12345u;
            img[y * wd + x] = (unsigned char)((y % 3 == 0 ? 5 : (x < wd / 2 ? (seed >> 16) : 2)) & mask);
        }
    for (i = 0; i < 768; i++) cmap[i] = (unsigned char)i;
    memset(&m, 0, sizeof m);
    s.ud = &m; s.write = m_write; s.patch32 = m_patch;
    CHECK_INT(shot_begin(&w, &s, wd, ht, depth, cmap, 0x1000), 0);
    for (y = 0; y < ht; y++) {
        memcpy(row, img + y * wd, (size_t)wd);
        CHECK_INT(shot_row(&w, row), 0);
    }
    CHECK_INT(shot_end(&w), 0);
    f = fopen("/tmp/shot_core_rt.ilbm", "wb");
    CHECK(f != NULL);
    if (!f) { free(img); free(m.b); return; }
    fwrite(m.b, 1, (size_t)m.n, f);
    fclose(f);
    pp = popen("python3 -I tools/ilbm_decode.py /tmp/shot_core_rt.ilbm 2>&1", "r");
    CHECK(pp != NULL);
    if (pp) {
        long hw = 0, hh = 0, hd = 0, hc = 0;
        CHECK(fgets(line, sizeof line, pp) != NULL && sscanf(line, "%ld %ld %ld %ld", &hw, &hh, &hd, &hc) == 4);
        CHECK_INT(hw, wd); CHECK_INT(hh, ht); CHECK_INT(hd, depth); CHECK_INT(hc, 0x1000);
        for (y = 0; y < ht && ok; y++) {
            if (!fgets(line, sizeof line, pp)) { ok = 0; break; }
            p = line;
            for (x = 0; x < wd; x++) {
                long v = strtol(p, &p, 10);
                if (v != img[y * wd + x]) ok = 0;
            }
        }
        CHECK(ok);
        pclose(pp);
    }
    remove("/tmp/shot_core_rt.ilbm");
    free(img);
    free(m.b);
}

void suite_shot_core(void)
{
    packer_edges();
    planes_and_padding();
    golden_bytes();
    refusals();
    round_trip(37, 7, 5);                                   /* odd width, padded rows */
    round_trip(320, 4, 8);
    round_trip(16, 3, 1);
    round_trip(129, 5, 3);
}
