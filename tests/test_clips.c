/* Clipboard history (amarchy/clips_core.c): the ring, its file form, the labels and the menu level. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "harness.h"
#include "../amarchy/clips_core.h"
#include "../amarchy/index_core.h"
#include "../amarchy/leaf.h"
#include "../config/keymap.h"

static void add(clip_ring *r, const char *s)
{
    clips_prepend(r, s, (long)strlen(s));
}

static void prepend_and_dedupe(void)
{
    clip_ring r;
    memset(&r, 0, sizeof r);
    add(&r, "one");
    add(&r, "two");
    add(&r, "three");
    CHECK_INT(r.n, 3);
    CHECK_STR(r.text[0], "three");                      /* newest first */
    CHECK_STR(r.text[2], "one");
    CHECK_INT(clips_prepend(&r, "one", 3), 1);          /* a repeat moves to the front */
    CHECK_INT(r.n, 3);                                  /* without a second copy */
    CHECK_STR(r.text[0], "one");
    CHECK_STR(r.text[1], "three");
    CHECK_STR(r.text[2], "two");
    CHECK_INT(clips_prepend(&r, "one", 3), 0);          /* already first: no change */
    CHECK_INT(clips_prepend(&r, "", 0), 0);             /* the empty clip is ignored */
    CHECK_INT(clips_prepend(&r, "\0x", 2), 0);
    CHECK_INT(r.n, 3);
    clips_free(&r);
    CHECK_INT(r.n, 0);
}

static void cap_of_twenty(void)
{
    clip_ring r;
    char t[16];
    int i;
    memset(&r, 0, sizeof r);
    for (i = 0; i < 21; i++) {
        snprintf(t, sizeof t, "clip %d", i);
        add(&r, t);
    }
    CHECK_INT(r.n, CLIPS_MAX);
    CHECK_STR(r.text[0], "clip 20");
    CHECK_STR(r.text[19], "clip 1");                    /* the 21st dropped the oldest, clip 0 */
    clips_free(&r);
}

static void labels(void)
{
    char out[CLIPS_LABEL * 4 + 1];
    const char *e;
    clips_label("line one\nline\ttwo\x7f", 18, out, sizeof out);
    CHECK_STR(out, "line one line two ");               /* controls become spaces, one line */
    e = "0123456789012345678901234567890123456789ABCDEF";
    clips_label(e, (long)strlen(e), out, sizeof out);
    CHECK_STR(out, "0123456789012345678901234567890123456789");   /* 40 characters */
    /* two-byte characters count once and stay whole */
    e = "\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5"
        "\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5"
        "\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5"
        "\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5\xc3\xa5Z";
    clips_label(e, (long)strlen(e), out, sizeof out);
    CHECK_INT((int)strlen(out), 80);
    /* a buffer that ends inside a character drops the character, never half of it */
    clips_label("ab\xc3\xa5x", 5, out, 4);
    CHECK_STR(out, "ab");
    clips_label("ab\xc3\xa5x", 5, out, 5);
    CHECK_STR(out, "ab\xc3\xa5");
    clips_label("ab\xc3", 3, out, sizeof out);          /* a torn sequence is one stray byte */
    CHECK_INT((int)strlen(out), 3);
}

static void file_round_trip(void)
{
    clip_ring a, b;
    long len = 0;
    char *bytes;
    char path[64];
    FILE *f;
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    add(&a, "plain");
    add(&a, "two\nlines\n\nand a blank");
    add(&a, "12\n34");                                  /* text that looks like a record header */
    add(&a, "bin\x01\xff\x80\n\n");                    /* LF and binary bytes, no NUL */
    bytes = clips_serialize(&a, &len);
    CHECK(bytes != 0);
    clips_parse(&b, bytes, len);
    CHECK_INT(b.n, 4);
    CHECK_STR(b.text[0], "bin\x01\xff\x80\n\n");
    CHECK_INT((int)b.len[0], 8);
    CHECK_STR(b.text[1], "12\n34");
    CHECK_STR(b.text[2], "two\nlines\n\nand a blank");
    CHECK_INT((int)b.len[2], 22);
    CHECK_STR(b.text[3], "plain");
    clips_parse(&b, bytes, len - 3);                    /* a torn tail loses that entry only */
    CHECK_INT(b.n, 3);
    clips_parse(&b, "junk", 4);
    CHECK_INT(b.n, 0);
    clips_parse(&b, "", 0);
    CHECK_INT(b.n, 0);
    free(bytes);

    snprintf(path, sizeof path, "/tmp/vtcon_clips_%ld.txt", (long)getpid());
    CHECK_INT(clips_save_file(&a, path), 1);
    CHECK_INT(clips_load_file(&b, path), 4);
    CHECK_STR(b.text[3], "plain");
    f = fopen(path, "wb");                              /* a truncated file: cut mid-record */
    if (f) {
        fputs("5\nab", f);
        fclose(f);
    }
    CHECK_INT(clips_load_file(&b, path), 0);
    remove(path);
    CHECK_INT(clips_load_file(&b, path), 0);            /* a missing file loads as empty */
    CHECK_INT(b.n, 0);
    clips_free(&a);
    clips_free(&b);
}

static void the_menu_level(void)
{
    static const char km_text[] = "[menu]\ntrigger.clipboard = Clipboard history | menu:trigger.clipboard\n";
    static keymap km;
    ix_index x;
    mn_item it[MN_MAX_ITEMS + 40];
    const char *labs[20];
    char names[20][16];
    char path[64];
    int i, n;
    CHECK(keymap_parse(&km, km_text, (long)strlen(km_text)));
    memset(&x, 0, sizeof x);
    ix_load_text(&km, &x, 0);
    CHECK_INT(ix_add_clips(&x, 0, 0), 1);               /* empty ring: one row that does nothing */
    n = ix_level(&km, &x, IX_CLIPS_NODE, "", it, 60);
    CHECK_INT(n, 1);
    CHECK_STR(it[0].label, "Nothing copied yet");
    CHECK(ix_launch_path(&x, &it[0], path, sizeof path));
    CHECK_INT(lf_clip_entry(path), -1);
    for (i = 0; i < 20; i++) {
        snprintf(names[i], sizeof names[i], "entry %d", i);
        labs[i] = names[i];
    }
    CHECK_INT(ix_add_clips(&x, labs, 20), 1);           /* replaces the placeholder */
    n = ix_level(&km, &x, IX_CLIPS_NODE, "", it, 60);
    CHECK_INT(n, 20);
    CHECK_STR(it[0].label, "entry 0");
    CHECK_STR(it[19].label, "entry 19");
    CHECK(ix_launch_path(&x, &it[7], path, sizeof path));
    CHECK_INT(lf_clip_entry(path), 7);
    CHECK_INT(lf_clip_entry("UP-Term:bin/tool"), -2);
    n = ix_level(&km, &x, "trigger", "", it, 60);       /* the row is a group, the ring is not in the root */
    CHECK_INT(n, 1);
    CHECK(MN_IS_GROUP(&it[0]));
    CHECK_STR(it[0].seg, "clipboard");
    n = ix_level(&km, &x, "", "", it, 60);
    CHECK_INT(n, 1);
    CHECK(MN_IS_GROUP(&it[0]));
    CHECK_STR(it[0].label, "Trigger");
    ix_free(&x);
}

void suite_clips(void)
{
    prepend_and_dedupe();
    cap_of_twenty();
    labels();
    file_round_trip();
    the_menu_level();
}
