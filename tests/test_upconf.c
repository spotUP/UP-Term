/* The upconf parser: sections, case, values, types, overflow, save round-trip.
 * Upconf has no OS calls, so this is a plain host suite. */
#include <stdio.h>
#include <string.h>
#include "../config/upconf.h"
#include "harness.h"

/* the stack under the next call full of non-zero bytes: an unterminated
 * buffer there reads them instead of a lucky zero */
static void dirty_stack(void)
{
    volatile char junk[4096];
    int i;
    for (i = 0; i < (int)sizeof(junk); i++)
        junk[i] = 'Z';
}

static long palette_str_dirty(const uc_u32 *in, char *out, long cap)
{
    dirty_stack();
    return upconf_palette_str(in, out, cap);
}

/* Prefs' Use updates open windows live, each only when its own profile
 * changed: the comparison behind it */
static void profile_equal(void)
{
    static upconf a, b;
    static const char one[] = "[profile default]\nbell = none\nfg = C0C0C0\n[profile vim]\nbg = 000000\n";
    static const char reordered[] = "[profile vim]\nbg = 000000\n[profile default]\nFG = C0C0C0\nbell = none\n";
    static const char vim_changed[] = "[profile default]\nbell = none\nfg = C0C0C0\n[profile vim]\nbg = 102030\n";
    static const char extra_key[] = "[profile default]\nbell = none\nfg = C0C0C0\ncursor = bar\n[profile vim]\nbg = 000000\n";
    upconf_parse(&a, one, (long)strlen(one));
    upconf_parse(&b, reordered, (long)strlen(reordered));
    CHECK(upconf_profile_equal(&a, &b, "default"));  /* order and key case aside */
    CHECK(upconf_profile_equal(&a, &b, "vim"));
    upconf_parse(&b, vim_changed, (long)strlen(vim_changed));
    CHECK(upconf_profile_equal(&a, &b, "default"));  /* another profile changed: not this one */
    CHECK(!upconf_profile_equal(&a, &b, "vim"));
    upconf_parse(&b, extra_key, (long)strlen(extra_key));
    CHECK(!upconf_profile_equal(&a, &b, "default")); /* a key more */
    CHECK(!upconf_profile_equal(&b, &a, "default")); /* a key less */
    CHECK(upconf_profile_equal(&a, &b, "nosuch"));   /* absent from both */
}

void suite_upconf(void)
{
    upconf c;
    static const char base[] =
        "; a comment\n"
        "# another\n"
        "\n"
        "early = kept\n"
        "[profile Default]\n"
        "FONT = TOPAZ 8.8.font\n"
        "fg = c0c0c0\n"
        "BG=#000000\n"
        "scrollback = 2000\n"
        "cursor = bar\n"
        "bell =\n"
        "unknown-key = xyz\n"
        "[profile vim]\n"
        "font = PARADISEC 8.8.font\n"
        "bell = none\n";
    static const char tail[] =
        "[profile Default]\n"
        "fg = FFFFFF\n";

    /* Parse an empty buffer: nothing, defaults stand. */
    CHECK(upconf_parse(&c, 0, 0) == 0);
    CHECK(upconf_get(&c, "default", "font") == 0);
    CHECK(upconf_parse(&c, "", 0) == 0);

    /* The shipped sample is all comments: it parses, changes nothing. */
    {
        static const char comments[] =
            "; a sample file\n"
            "# nothing active\n"
            ";\n"
            ";   [profile default]\n"
            ";   font = TOPAZ:8.8.font\n"
            "\n";
        CHECK(upconf_parse(&c, comments, (long)strlen(comments)) == 1);
        CHECK_INT(c.nprof, 0);
        CHECK(upconf_get(&c, "default", "font") == 0);
    }

    CHECK(upconf_parse(&c, base, (long)strlen(base)) == 1);
    CHECK(c.nprof == 2);

    /* Keys before any [profile] land in default; case-insensitive lookups. */
    CHECK_STR(upconf_get(&c, "default", "early"), "kept");
    CHECK_STR(upconf_get(&c, "DEFAULT", "FONT"), "TOPAZ 8.8.font");
    CHECK_STR(upconf_get(&c, "default", "font"), "TOPAZ 8.8.font");
    CHECK_STR(upconf_str(&c, "default", "missing", "DEF"), "DEF");
    CHECK_STR(upconf_str(&c, "default", "bell", "DEF"), "DEF"); /* empty value -> def */
    CHECK(upconf_has(&c, "default", "bell"));

    /* RGB: bare, #-prefixed, case-insensitive value, and defaults. */
    CHECK_INT(upconf_rgb(&c, "default", "fg", 0), 0xC0C0C0);
    CHECK_INT(upconf_rgb(&c, "default", "BG", 0), 0x000000);
    CHECK_INT(upconf_rgb(&c, "default", "missing", 0x123456), 0x123456);
    CHECK_INT(upconf_rgb(&c, "default", "bell", 0x123456), 0x123456); /* empty -> def */

    /* Int: value, negative, missing, non-numeric. */
    CHECK_INT(upconf_int(&c, "default", "scrollback", 0), 2000);
    CHECK_INT(upconf_int(&c, "default", "unknown-key", 0), 0); /* not a number -> def */
    CHECK_INT(upconf_int(&c, "default", "missing", -5), -5);

    /* A profile that is absent misses every key. */
    CHECK(upconf_get(&c, "nope", "font") == 0);
    CHECK_STR(upconf_str(&c, "nope", "bell", "DEF"), "DEF");

    /* A later section of the same profile overwrites its values. */
    CHECK(upconf_parse(&c, 0, 0) == 0); /* reset */
    {
        char full[512];
        strcpy(full, base);
        strcat(full, tail);
        CHECK(upconf_parse(&c, full, (long)strlen(full)) == 1);
        CHECK_INT(upconf_rgb(&c, "default", "fg", 0), 0xFFFFFF);
    }

    /* Profiles listed in the order set. */
    {
        const char *names[UC_MAX_PROFILES + 1];
        int n = upconf_profiles(&c, names);
        CHECK_INT(n, 2); /* "default" and "vim"; the 2nd [Default] reuses it */
        CHECK_STR(names[0], "default");
        CHECK_STR(names[1], "vim");
        CHECK(names[2] == 0);
    }

    /* set / del / save round-trip. */
    upconf_clear(&c);
    CHECK(upconf_set(&c, "default", "font", "A.font"));
    CHECK(upconf_set(&c, "default", "fg", "112233"));
    CHECK(upconf_set(&c, "vim", "bell", "none"));
    CHECK(upconf_set(&c, "default", "font", "B.font")); /* replace */
    CHECK_STR(upconf_get(&c, "default", "font"), "B.font");

    {
        char buf[4096];
        upconf back;
        long len = upconf_save(&c, buf, sizeof(buf));
        CHECK(len > 0);
        CHECK(upconf_parse(&back, buf, len) == 1);
        CHECK_INT(back.nprof, c.nprof);
        CHECK_STR(upconf_get(&back, "default", "font"), "B.font");
        CHECK_INT(upconf_rgb(&back, "default", "fg", 0), 0x112233);
        CHECK_STR(upconf_get(&back, "vim", "bell"), "none");
    }

    CHECK(upconf_del(&c, "default", "fg"));
    CHECK(upconf_get(&c, "default", "fg") == 0);
    CHECK(!upconf_del(&c, "default", "fg")); /* gone now */
    CHECK(!upconf_del(&c, "nope", "fg"));

    /* A value longer than UC_MAX_VALUE is truncated, not dropped, flagged. */
    upconf_clear(&c);
    {
        char longval[UC_MAX_VALUE + 40];
        int i;
        for (i = 0; i < (int)sizeof(longval); i++)
            longval[i] = 'a';
        longval[sizeof(longval) - 1] = 0;
        CHECK(upconf_set(&c, "default", "k", longval));
        CHECK(c.overflow);
        CHECK_INT((int)strlen(upconf_get(&c, "default", "k")), UC_MAX_VALUE - 1);
    }

    /* save into a too-small buffer fails cleanly. */
    upconf_clear(&c);
    CHECK(upconf_set(&c, "default", "font", "A.font"));
    {
        char tiny[8];
        CHECK(upconf_save(&c, tiny, sizeof(tiny)) < 0);
    }

    /* Palette pairs: parse, inverse, malformed stops the list. */
    {
        uc_u32 pal[16];
        char s[128];
        long n;
        CHECK_INT(upconf_palette_parse("2,FF0000,4,0x5C5CFF", pal), 2);
        CHECK_INT((int)pal[2], 0x01FF0000);
        CHECK_INT((int)pal[4], 0x015C5CFF);
        CHECK_INT((int)pal[0], 0);
        CHECK_INT((int)pal[15], 0);
        n = upconf_palette_str(pal, s, (long)sizeof(s));
        CHECK(n > 0);
        s[n] = 0;
        CHECK_STR(s, "02,FF0000,04,5C5CFF");
        {
            uc_u32 back[16];
            CHECK_INT(upconf_palette_parse(s, back), 2);
            CHECK(pal[0] == back[0] && pal[2] == back[2] &&
                  pal[4] == back[4] && pal[15] == back[15]);
        }
        /* the 0x / # prefixes and case; a malformed pair stops the list */
        CHECK_INT(upconf_palette_parse("0,#00cd00,1,0xabcdef,9,ZZZZ", pal), 2);
        CHECK_INT((int)pal[0], 0x0100CD00);
        CHECK_INT((int)pal[1], 0x01ABCDEF);
        /* out of range index and over-long colour are malformed too */
        CHECK_INT(upconf_palette_parse("16,000000", pal), 0);
        CHECK_INT(upconf_palette_parse("1,01000000", pal), 0);
        CHECK_INT(upconf_palette_parse("", pal), 0);
        CHECK_INT(upconf_palette_parse("1", pal), 0); /* no comma: a bare index */
        CHECK_INT(upconf_palette_parse("0,FF0000", pal), 1);
        CHECK_INT(upconf_palette_str(pal, s, 2), -1); /* "00,FF0000" does not fit */
    }

    /* A full 16-entry palette is the longest value the format has: nine
     * bytes an entry, 159 with the last comma dropped. It must survive the
     * table whole, or the Prefs grid cannot be written at all. */
    {
        static upconf full;
        uc_u32 pal[16], back[16];
        char s[UC_MAX_VALUE + 2];
        long n;
        int i;
        upconf_clear(&full);
        for (i = 0; i < 16; i++)
            pal[i] = 0x01000000UL | (uc_u32)(i * 0x010203UL);
        n = upconf_palette_str(pal, s, (long)sizeof(s));
        CHECK_INT(n, 159);
        s[n] = 0;
        CHECK(upconf_set(&full, "default", "palette", s));
        CHECK_INT(full.overflow, 0);
        CHECK_INT((int)strlen(upconf_get(&full, "default", "palette")), 159);
        CHECK_INT(upconf_palette_parse(upconf_get(&full, "default", "palette"), back), 16);
        CHECK(back[0] == pal[0] && back[7] == pal[7] && back[15] == pal[15]);
    }

    /* A typed colour field: exactly six hex digits and nothing after them.
     * The lenient upconf_hex (for values already in a file) cuts at the junk;
     * upconf_hex6 must refuse it, or a typo like "1122334" would silently
     * become a different colour than the field shows. */
    {
        uc_u32 rgb;
        CHECK(upconf_hex6("000000", &rgb) && rgb == 0x000000);
        CHECK(upconf_hex6("FFFFFF", &rgb) && rgb == 0xFFFFFF);
        CHECK(upconf_hex6("#0f0f0f", &rgb) && rgb == 0x0F0F0F);
        CHECK(upconf_hex6("0xABCDEF", &rgb) && rgb == 0xABCDEF);
        CHECK(upconf_hex6("  112233  ", &rgb) && rgb == 0x112233);
        CHECK(!upconf_hex6("1122334", &rgb));   /* a seventh digit */
        CHECK(!upconf_hex6("11223z", &rgb));   /* not hex */
        CHECK(!upconf_hex6("11223", &rgb));    /* too short */
        CHECK(!upconf_hex6("", &rgb));
        CHECK(!upconf_hex6("   ", &rgb));
        CHECK(!upconf_hex6("1122 33", &rgb));  /* junk in the middle */
        CHECK(!upconf_hex6("112233xyz", &rgb));/* junk after the digits */
        CHECK(!upconf_hex6("112233#", &rgb));
        CHECK(upconf_hex6(0, &rgb) == 0);
    }

    /* The selection colours a theme carries. VR_KEEP lives in the Amiga
     * renderer, which the host does not link, so the sentinel is spelled
     * out here; the point is the contract: a key that is absent leaves the
     * caller's sentinel alone, so the renderer keeps swapping the cell. */
    {
        const uc_u32 keep = 0xFFFFFFFFUL; /* VR_KEEP */
        const char *text =
            "[profile night]\n"
            "selection-bg = 87AFD7\n"
            "selection-fg = 262626\n"
            "cursor-color = BCBCBC\n"
            "palette = 0,1C1C1C,1,AF5F5F\n";
        upconf_clear(&c);
        CHECK(upconf_parse(&c, text, (long)strlen(text)));
        CHECK(upconf_rgb(&c, "night", "selection-bg", keep) == 0x87AFD7);
        CHECK(upconf_rgb(&c, "night", "selection-fg", keep) == 0x262626);
        CHECK(upconf_rgb(&c, "night", "cursor-color", keep) == 0xBCBCBC);
        /* absent keys keep the sentinel, so the swap survives */
        CHECK(upconf_rgb(&c, "night", "selection-underline", keep) == keep);
        CHECK(upconf_get(&c, "night", "selection-bg"));
        CHECK(upconf_get(&c, "night", "selection-fg"));

        /* a profile that sets only the background themes the highlight and
         * leaves the text to the swap -- the renderer asks for the other
         * half with its own sentinel and gets it back unchanged */
        text = "[profile half]\nselection-bg = 000000\n";
        upconf_clear(&c);
        CHECK(upconf_parse(&c, text, (long)strlen(text)));
        CHECK(upconf_rgb(&c, "half", "selection-bg", keep) == 0x000000);
        CHECK(upconf_rgb(&c, "half", "selection-fg", keep) == keep);

        /* black is a colour, not a missing key */
        upconf_clear(&c);
        CHECK(upconf_has(&c, "x", "selection-bg") == 0);
        CHECK(upconf_set(&c, "x", "selection-bg", "000000"));
        CHECK(upconf_has(&c, "x", "selection-bg"));
        CHECK(upconf_rgb(&c, "x", "selection-bg", keep) == 0x000000);

        /* a value with no hex digits at all falls back to the sentinel, so
         * the renderer keeps the swap; upconf_hex is the lenient parser for
         * values already in a file, so a short one is taken as written
         * (upconf_hex6 is the strict one the Prefs editor uses) */
        upconf_clear(&c);
        CHECK(upconf_set(&c, "y", "selection-fg", "nothex"));
        CHECK_INT(upconf_rgb(&c, "y", "selection-fg", keep), keep);
        CHECK(upconf_set(&c, "y", "selection-bg", ""));
        CHECK_INT(upconf_rgb(&c, "y", "selection-bg", keep), keep);
    }

    /* Delete a whole profile; the later ones keep their place. */
    upconf_clear(&c);
    CHECK(upconf_set(&c, "alpha", "k", "1"));
    CHECK(upconf_set(&c, "beta", "k", "2"));
    CHECK(upconf_set(&c, "gamma", "k", "3"));
    CHECK(upconf_rmprof(&c, "BETA"));
    CHECK_INT(c.nprof, 2);
    CHECK_STR(upconf_get(&c, "alpha", "k"), "1");
    CHECK_STR(upconf_get(&c, "gamma", "k"), "3");
    CHECK(upconf_get(&c, "beta", "k") == 0);
    CHECK(!upconf_rmprof(&c, "beta"));

    /* A palette index or colour too long for a long writes nothing out of
     * the table (an index of 2^31 wrapped negative and wrote out16[-1]). */
    {
        static uc_u32 guard[18];
        uc_u32 *pal = guard + 1;
        guard[0] = guard[17] = 0xDEADBEEFUL;
        CHECK_INT(upconf_palette_parse("2147483648,FFFFFF", pal), 0);
        CHECK_INT(upconf_palette_parse("99999999999999999999,FFFFFF", pal), 0);
        CHECK_INT(upconf_palette_parse("1,FFFFFFFFFFFFFFFFFF", pal), 0);
        CHECK_INT(upconf_palette_parse("3,00FF00", pal), 1);
        CHECK(guard[0] == 0xDEADBEEFUL && guard[17] == 0xDEADBEEFUL);
    }

    /* A line longer than the parser takes is dropped whole: its tail is no
     * line of its own (a long font value smuggled in "bell=none"). */
    {
        static char longline[400];
        int k;
        strcpy(longline, "font = ");
        for (k = 7; k < 300; k++)
            longline[k] = 'a';
        strcpy(longline + 300, "bell=none\nscrollback = 7\n");
        upconf_parse(&c, longline, (long)strlen(longline));
        CHECK(upconf_get(&c, "default", "bell") == 0);
        CHECK(upconf_get(&c, "default", "font") == 0);
        CHECK_STR(upconf_get(&c, "default", "scrollback"), "7");
        CHECK(c.overflow);
    }

    /* the palette text is exactly the entries: a stack full of junk once
     * ran into every entry (the terminator sat where a digit went) */
    {
        uc_u32 in[16];
        char out[200];
        long len;
        int k;
        for (k = 0; k < 16; k++)
            in[k] = 0x01000000UL | (uc_u32)(0x111111UL * (uc_u32)(k % 15 + 1));
        /* the text is not NUL-terminated (upconf.h): the length is the
         * answer, and strlen of the buffer read on into the junk */
        len = palette_str_dirty(in, out, sizeof(out) - 1);
        CHECK_INT(len, 16 * 10 - 1);                  /* "NN,RRGGBB," x 16, no last comma */
        out[len > 0 ? len : 0] = 0;
        CHECK(!strncmp(out, "00,111111,01,222222,", 20));
        CHECK_STR(out + 150, "15,111111");             /* the last entry whole, nothing after */
    }
    profile_equal();
}
