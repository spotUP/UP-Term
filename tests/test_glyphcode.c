/* render/glyphcode: a plane-15 Nerd Font icon is drawn only by an engine
 * that takes 32-bit glyph codes; one that ignores the 32-bit tag (as
 * ttf.library 0.8.5 does, answering success) gives the replacement box,
 * never the glyph selected before. */
#include "harness.h"
#include "../render/glyphcode.h"

/* a fake glyph engine: the font has the probe letters, two BMP icons and
 * two plane-15 icons; drawing gives a glyph's signature (its code + 1) */
struct fake {
    unsigned long cur;
    int takes32;  /* honours the 32-bit tag */
    int err32;    /* answers an error for it, as diskfonttag.h says */
};

static int has(unsigned long c)
{
    return c == VT_GC_PROBE_A || c == VT_GC_PROBE_B || c == 0xEBCF || c == 0xF011 ||
           c == 0xF003B || c == 0xF0249;
}

static int fake_set(void *engine, int wide, unsigned long code)
{
    struct fake *f = (struct fake *)engine;
    if (wide && f->err32)
        return 1;
    if (wide && !f->takes32)
        return 0; /* ttf.library 0.8.5: tag skipped, success answered */
    f->cur = wide ? code : (code & 0xFFFFUL);
    return 0;
}

static unsigned long fake_draw(void *engine)
{
    struct fake *f = (struct fake *)engine;
    return has(f->cur) ? f->cur + 1 : 0;
}

/* select cp and draw it: 0 when the cell gets the replacement box */
static unsigned long cell(struct fake *f, int codes32, unsigned long cp)
{
    if (vt_gc_select(f, fake_set, codes32, cp) != 0)
        return 0;
    return fake_draw(f);
}

static void plane15_icon_is_a_box_not_the_previous_glyph_on_ttf_0_8_5(void)
{
    struct fake f = { 0, 0, 0 };
    int codes32 = vt_gc_probe32(&f, fake_set, fake_draw);
    CHECK_INT(codes32, 0);
    CHECK_INT((long)cell(&f, codes32, 0xF011), 0xF011 + 1);  /* BMP icon draws */
    CHECK_INT((long)cell(&f, codes32, 0xF003B), 0);          /* box, not U+F011 */
    CHECK_INT((long)cell(&f, codes32, 0xEBCF), 0xEBCF + 1);
    CHECK_INT((long)cell(&f, codes32, 0xF0249), 0);
}

static void plane15_icon_draws_on_an_engine_with_32_bit_codes(void)
{
    struct fake f = { 0, 1, 0 };
    int codes32 = vt_gc_probe32(&f, fake_set, fake_draw);
    CHECK_INT(codes32, 1);
    CHECK_INT((long)cell(&f, codes32, 0xF003B), 0xF003B + 1);
    CHECK_INT((long)cell(&f, codes32, 0xF0249), 0xF0249 + 1);
    CHECK_INT((long)cell(&f, codes32, 0xF011), 0xF011 + 1);
    CHECK_INT((long)cell(&f, codes32, 0x1F600), 0); /* not in the font: missing */
}

static void an_engine_refusing_the_32_bit_tag_has_no_32_bit_codes(void)
{
    struct fake f = { 0, 1, 1 };
    CHECK_INT(vt_gc_probe32(&f, fake_set, fake_draw), 0);
    CHECK_INT((long)cell(&f, 0, 0xF003B), 0);
    CHECK_INT((long)cell(&f, 0, 0xEBCF), 0xEBCF + 1);
}

void suite_glyphcode(void)
{
    plane15_icon_is_a_box_not_the_previous_glyph_on_ttf_0_8_5();
    plane15_icon_draws_on_an_engine_with_32_bit_codes();
    an_engine_refusing_the_32_bit_tag_has_no_32_bit_codes();
}
