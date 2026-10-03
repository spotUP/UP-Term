/* render/otag: an outline font's .otag file is accepted as engines need it,
 * and a damaged one is refused before its offsets become pointers. */
#include <string.h>
#include "harness.h"
#include "../render/otag.h"

static unsigned char f[256];

static void put32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

/* the shape ttfinstall writes: ident, engine, family, a plain tag, done,
 * then the strings */
static long make(void)
{
    long len = 5 * 8 + 4 + 12;
    memset(f, 0, sizeof(f));
    put32(f, OTAG_FILEIDENT);
    put32(f + 4, (unsigned long)len);
    put32(f + 8, OTAG_ENGINE);
    put32(f + 12, 40);
    put32(f + 16, OTAG_TAG_USER | 0x1000UL | OTAG_INDIRECT | 0x03UL); /* OT_Family */
    put32(f + 20, 44);
    put32(f + 24, OTAG_TAG_USER | 0x1000UL | 0x10UL);                 /* OT_SymbolSet */
    put32(f + 28, 0x4C31);
    put32(f + 32, OTAG_TAG_DONE);
    memcpy(f + 40, "ttf", 4);
    memcpy(f + 44, "Symbols Nerd", 12);
    return len;
}

static void a_ttfinstall_otag_names_its_engine(void)
{
    otag_info in;
    long len = make();
    CHECK_INT(otag_check(f, len, &in), OTAG_OK);
    CHECK_INT(in.ntags, 4); /* the ident counts */
    CHECK_INT(in.engine_off, 40);
    CHECK(strcmp((const char *)f + in.engine_off, "ttf") == 0);
}

static void a_truncated_or_resized_file_is_refused(void)
{
    otag_info in;
    long len = make();
    CHECK_INT(otag_check(f, 8, &in), OTAG_SHORT);
    CHECK_INT(otag_check(f, len - 4, &in), OTAG_SIZE); /* the ident says how long it was */
    f[3] ^= 1;
    CHECK_INT(otag_check(f, len, &in), OTAG_NO_IDENT);
}

static void an_offset_outside_the_file_is_refused(void)
{
    otag_info in;
    long len = make();
    put32(f + 20, 4000);
    CHECK_INT(otag_check(f, len, &in), OTAG_BAD_OFFSET);
}

static void no_engine_or_an_unterminated_name_is_refused(void)
{
    otag_info in;
    long len = make();
    put32(f + 8, OTAG_TAG_USER | 0x1000UL | 0x10UL);
    CHECK_INT(otag_check(f, len, &in), OTAG_NO_ENGINE);
    len = make();
    put32(f + 12, (unsigned long)(len - 1)); /* the last byte, then the end */
    f[len - 1] = 'x';
    CHECK_INT(otag_check(f, len, &in), OTAG_NO_ENGINE);
}

static void a_list_without_tag_done_is_refused(void)
{
    otag_info in;
    long len = make(), at;
    for (at = 32; at + 8 <= len; at += 8)
        put32(f + at, OTAG_TAG_USER | 0x1000UL | 0x10UL);
    CHECK_INT(otag_check(f, len, &in), OTAG_NO_END);
}

void suite_otag(void)
{
    a_ttfinstall_otag_names_its_engine();
    a_truncated_or_resized_file_is_refused();
    an_offset_outside_the_file_is_refused();
    no_engine_or_an_unterminated_name_is_refused();
    a_list_without_tag_done_is_refused();
}
