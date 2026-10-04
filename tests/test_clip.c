/* handler/clipfmt: the clipboard's text, UTF-8 both ways (gap item 10). */
#include <stdlib.h>
#include "harness.h"
#include "../handler/clipfmt.h"

/* A clip in memory, read as clipboard.device gives it: in order. */
typedef struct {
    const unsigned char *b;
    long n, at;
} mem_clip;

static long mem_read(void *u, void *buf, long n)
{
    mem_clip *m = (mem_clip *)u;
    long k = m->n - m->at < n ? m->n - m->at : n;
    memcpy(buf, m->b + m->at, (size_t)k);
    m->at += k;
    return k;
}

static void *mem_alloc(unsigned long n) { return malloc(n); }
static void mem_free(void *p) { free(p); }

static unsigned char clip[200000];
static long clip_n;

static void chunk(const char *id, const char *data, long n)
{
    memcpy(clip + clip_n, id, 4);
    clip[clip_n + 4] = (unsigned char)(n >> 24);
    clip[clip_n + 5] = (unsigned char)(n >> 16);
    clip[clip_n + 6] = (unsigned char)(n >> 8);
    clip[clip_n + 7] = (unsigned char)n;
    memcpy(clip + clip_n + 8, data, (size_t)n);
    clip_n += 8 + n;
    if (n & 1)
        clip[clip_n++] = 0;
}

static void form_begin(void)
{
    memcpy(clip, "FORM\0\0\0\0FTXT", 12);
    clip_n = 12;
}

static void form_end(void)
{
    long n = clip_n - 8;
    clip[4] = (unsigned char)(n >> 24);
    clip[5] = (unsigned char)(n >> 16);
    clip[6] = (unsigned char)(n >> 8);
    clip[7] = (unsigned char)n;
}

static char *read_clip(long *len)
{
    mem_clip m;
    m.b = clip;
    m.n = clip_n;
    m.at = 0;
    return cf_read_ftxt(mem_read, &m, mem_alloc, mem_free, len);
}

/* A copy writes CHRS (Latin-1, for every older reader) and UTF8; a paste
 * gives back the UTF-8, characters beyond Latin-1 included. */
static void copy_and_paste_round_trip_utf8(void)
{
    static const char text[] = "caf\xc3\xa9 \xe4\xb8\xad \xf0\x9f\x98\x80!"; /* odd length */
    long n = (long)strlen(text), lat_n, len;
    char lat[64];
    unsigned char mid[12];
    int k;
    char *got;
    lat_n = cf_to_latin1(text, n, lat);
    CHECK_INT(lat_n, 9);
    CHECK(!memcmp(lat, "caf\xe9 ? ?!", 9));
    cf_ftxt_head(clip, lat_n, n);
    memcpy(clip + 20, lat, (size_t)lat_n);
    k = cf_ftxt_mid(mid, lat_n, n);
    CHECK_INT(k, 9); /* the CHRS pad byte first */
    memcpy(clip + 20 + lat_n, mid, (size_t)k);
    memcpy(clip + 20 + lat_n + k, text, (size_t)n);
    clip_n = 20 + lat_n + k + n;
    if (n & 1)
        clip[clip_n++] = 0;
    /* the FORM's size is what follows its first 8 bytes */
    CHECK_INT(((long)clip[4] << 24) | ((long)clip[5] << 16) | ((long)clip[6] << 8) | clip[7], clip_n - 8);
    got = read_clip(&len);
    CHECK(got != 0);
    if (got) {
        CHECK_INT(len, n);
        CHECK_STR(got, text);
        free(got);
    }
}

/* What ConClip and the Shell write: one CHRS, Latin-1 -- read as UTF-8. */
static void latin1_clips_from_older_programs_still_paste(void)
{
    long len;
    char *got;
    form_begin();
    chunk("CHRS", "na\xefve", 5);
    form_end();
    got = read_clip(&len);
    CHECK(got != 0);
    if (got) {
        CHECK_STR(got, "na\xc3\xafve");
        CHECK_INT(len, 6);
        free(got);
    }
}

/* Chunks come in any order: UTF8 wins over a CHRS before or after it,
 * other chunks are skipped. */
static void utf8_chunk_wins_in_any_order(void)
{
    long len;
    char *got;
    form_begin();
    chunk("UTF8", "\xc3\xa9t\xc3\xa9", 6);
    chunk("CHRS", "?t?", 3);
    form_end();
    got = read_clip(&len);
    CHECK_STR(got ? got : "", "\xc3\xa9t\xc3\xa9");
    free(got);
    form_begin();
    chunk("FONS", "xyz", 3);
    chunk("CHRS", "?t?", 3);
    chunk("UTF8", "\xc3\xa9t\xc3\xa9", 6);
    form_end();
    got = read_clip(&len);
    CHECK_STR(got ? got : "", "\xc3\xa9t\xc3\xa9");
    free(got);
    form_begin();
    chunk("FONS", "xyz", 3);
    form_end();
    CHECK(read_clip(&len) == 0);
    memcpy(clip, "FORM\0\0\0\4ILBM", 12);
    clip_n = 12;
    CHECK(read_clip(&len) == 0);
}

/* No size limit: 150 KB of text in, 150 KB out (the window cut copies at
 * 16 KB and pastes at 8 KB). */
static void large_clips_are_not_cut(void)
{
    static char big[150001];
    long len, i;
    char *got;
    for (i = 0; i < 150000; i++)
        big[i] = (char)('a' + i % 26);
    form_begin();
    chunk("CHRS", big, 150000);
    form_end();
    got = read_clip(&len);
    CHECK_INT(len, 150000);
    CHECK(got && !memcmp(got, big, 150000));
    free(got);
}

/* UTF-8 decoding for the paste, and what a paste may type. */
static void paste_decodes_and_drops_controls(void)
{
    static const char s[] = "a\xc3\xa9\xf0\x9f\x98\x80\xff\xe2\x82";
    long i = 0, n = (long)strlen(s);
    CHECK_INT(cf_next(s, n, &i), 'a');
    CHECK_INT(cf_next(s, n, &i), 0xE9);
    CHECK_INT(cf_next(s, n, &i), 0x1F600);
    CHECK_INT(cf_next(s, n, &i), 0xFF); /* not UTF-8: the byte */
    CHECK_INT(cf_next(s, n, &i), 0xE2); /* cut short: the byte */
    CHECK_INT(cf_next(s, n, &i), 0x82);
    CHECK_INT(i, n);
    CHECK(cf_paste_keeps('a'));
    CHECK(cf_paste_keeps('\t'));
    CHECK(cf_paste_keeps('\n'));
    CHECK(cf_paste_keeps(0x1F600));
    CHECK(!cf_paste_keeps(0x1B)); /* ESC [ 201 ~ would end a bracketed paste */
    CHECK(!cf_paste_keeps(0x03));
    CHECK(!cf_paste_keeps(0x9B));
    CHECK(!cf_paste_keeps(0x7F));
}

void suite_clip(void)
{
    copy_and_paste_round_trip_utf8();
    latin1_clips_from_older_programs_still_paste();
    utf8_chunk_wins_in_any_order();
    large_clips_are_not_cut();
    paste_decodes_and_drops_controls();
}
