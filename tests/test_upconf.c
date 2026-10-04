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

/* Save keeps the comments: the parse used to drop every ; and # line and
 * the save wrote only keys, so the first Prefs save of the shipped sample
 * (45 lines of documentation) left a bare table. A comment stays where
 * it was among its profile's keys; one before every section stays first. */
static void comments_kept(void)
{
    static upconf c, back;
    static char out[UC_MAX_FILE + 1];
    static char again[UC_MAX_FILE + 1];
    static const char file[] =
        "; UP-Term settings\n"
        "# the head of the file\n"
        "\n"
        "[profile default]\n"
        "; the font I like\n"
        "font = TOPAZ 8.8.font\n"
        "  ; indented, kept as typed\n"
        "bell = none\n"
        "; backspace = bs\n"
        "[profile vim]\n"
        "# vim's own\n"
        "fg = C0C0C0\n";
    static const char want[] =
        "; UP-Term settings\n"
        "# the head of the file\n"
        "\n"
        "[profile default]\n"
        "; the font I like\n"
        "font = TOPAZ 8.8.font\n"
        "  ; indented, kept as typed\n"
        "bell = none\n"
        "; backspace = bs\n"
        "\n"
        "\n"
        "[profile vim]\n"
        "# vim's own\n"
        "fg = C0C0C0\n";
    long len, len2;
    CHECK(upconf_parse(&c, file, (long)strlen(file)) == 1);
    CHECK(!c.overflow);
    CHECK_INT(c.nprof, 2);
    CHECK_INT(c.n[0], 2); /* comments are not keys: no key slot taken */
    len = upconf_save(&c, out, sizeof(out) - 1);
    CHECK(len > 0);
    out[len > 0 ? len : 0] = 0;
    CHECK_STR(out, want);
    /* and the saved file saves the same again: nothing grows or moves */
    CHECK(upconf_parse(&back, out, len) == 1);
    len2 = upconf_save(&back, again, sizeof(again) - 1);
    again[len2 > 0 ? len2 : 0] = 0;
    CHECK_STR(again, out);

    /* a key deleted: the comment before the next key stays before it, the
     * keys keep their order; a key added goes last */
    CHECK(upconf_parse(&c, file, (long)strlen(file)) == 1);
    CHECK(upconf_set(&c, "default", "scrollback", "900"));
    CHECK(upconf_del(&c, "default", "font"));
    len = upconf_save(&c, out, sizeof(out) - 1);
    out[len > 0 ? len : 0] = 0;
    CHECK_STR(out,
        "; UP-Term settings\n"
        "# the head of the file\n"
        "\n"
        "[profile default]\n"
        "; the font I like\n"
        "  ; indented, kept as typed\n"
        "bell = none\n"
        "; backspace = bs\n"
        "scrollback = 900\n"
        "\n"
        "\n"
        "[profile vim]\n"
        "# vim's own\n"
        "fg = C0C0C0\n");

    /* a profile deleted takes its own comments, the others keep theirs */
    CHECK(upconf_parse(&c, file, (long)strlen(file)) == 1);
    CHECK(upconf_rmprof(&c, "default"));
    len = upconf_save(&c, out, sizeof(out) - 1);
    out[len > 0 ? len : 0] = 0;
    CHECK_STR(out,
        "; UP-Term settings\n"
        "# the head of the file\n"
        "\n"
        "[profile vim]\n"
        "# vim's own\n"
        "fg = C0C0C0\n");

    /* a section that holds only a comment is kept, as written */
    CHECK(upconf_parse(&c, "[profile empty]\n; nothing yet\n", 30) == 1);
    len = upconf_save(&c, out, sizeof(out) - 1);
    out[len > 0 ? len : 0] = 0;
    CHECK_STR(out, "[profile empty]\n; nothing yet\n");

    /* a comment longer than a line of settings is kept whole: dropping it
     * would lose it on the next save */
    {
        static char longc[600];
        int k;
        longc[0] = ';';
        for (k = 1; k < 500; k++)
            longc[k] = 'c';
        strcpy(longc + 500, "\nbell = none\n");
        CHECK(upconf_parse(&c, longc, (long)strlen(longc)) == 1);
        CHECK(!c.overflow);
        CHECK_STR(upconf_get(&c, "default", "bell"), "none");
        len = upconf_save(&c, out, sizeof(out) - 1);
        out[len > 0 ? len : 0] = 0;
        CHECK(!strncmp(out, longc, 501));
    }

    /* more comment text than the table holds: marked, so the editor does
     * not write the file back without it */
    {
        static char many[UC_MAX_FILE];
        long n = 0;
        while (n + 40 < (long)sizeof(many) - 1) {
            memcpy(many + n, "; a comment line of forty bytes ......\n", 39);
            n += 39;
        }
        many[n] = 0;
        CHECK(upconf_parse(&c, many, n) == 1);
        CHECK(c.overflow);
    }
}

static long app(char *buf, long len, const char *s)
{
    strcpy(buf + len, s);
    return len + (long)strlen(s);
}

/* The table is packed (research/2026-10-04_window-memory.md): one per
 * XCON: window, 56 KB in slots, about 18 KB packed. */
static void packed_table(void)
{
    static upconf c, back;
    static char file[UC_MAX_FILE + 1], saved[UC_MAX_FILE + 1];
    char val[UC_MAX_VALUE + 8];
    long len, n;
    int p, k, used;

    /* the sentinel: a climb back towards slots fails here */
    CHECK(sizeof(upconf) <= 18500);

    /* a full file -- 8 profiles x 32 keys of long values and a long comment,
     * as close to UC_MAX_FILE as it goes -- is read whole, every value kept,
     * and saved back byte for byte */
    len = 0;
    for (p = 0; p < UC_MAX_PROFILES; p++) {
        len = app(file, len, p ? "\n\n[profile p" : "[profile p");
        val[0] = (char)('0' + p);
        val[1] = 0;
        len = app(file, len, val);
        len = app(file, len, "]\n");
        if (p == 3) {
            memset(val, '0', 100);
            strcpy(val + 100, "\n");
            len = app(file, len, "; ");
            len = app(file, len, val);
        }
        for (k = 0; k < UC_MAX_KEYS; k++) {
            val[0] = 'k';
            val[1] = (char)('0' + k / 10);
            val[2] = (char)('0' + k % 10);
            strcpy(val + 3, " = ");
            memset(val + 6, 'a' + (p + k) % 26, 55);
            strcpy(val + 61, "\n");
            len = app(file, len, val);
        }
    }
    CHECK(len <= UC_MAX_FILE);
    CHECK(len > UC_MAX_FILE - 300);
    upconf_parse(&c, file, len);
    CHECK(!c.overflow);
    CHECK_INT(c.nprof, UC_MAX_PROFILES);
    CHECK(c.used <= len);
    for (p = 0; p < UC_MAX_PROFILES; p++)
        CHECK_INT(c.n[p], UC_MAX_KEYS);
    memset(val, 'a' + (5 + 9) % 26, 55);
    val[55] = 0;
    CHECK_STR(upconf_get(&c, "p5", "k09"), val);
    n = upconf_save(&c, saved, sizeof(saved));
    CHECK_INT(n, len);
    saved[n > 0 ? n : 0] = 0;
    CHECK(n == len && !memcmp(saved, file, (size_t)len));

    /* the pool is full: a set that does not fit fails, marks overflow and
     * leaves the old value standing (a table no save could write anyway) */
    upconf_clear(&c);
    memset(val, 'v', UC_MAX_VALUE - 1);
    val[UC_MAX_VALUE - 1] = 0;
    n = 0;
    for (p = 0; p < UC_MAX_PROFILES; p++)
        for (k = 0; k < UC_MAX_KEYS; k++) {
            char name[8];
            name[0] = 'p';
            name[1] = (char)('0' + p);
            name[2] = 0;
            file[0] = 'k';
            file[1] = (char)('A' + k);
            file[2] = 0;
            n += upconf_set(&c, name, file, val);
        }
    CHECK(c.overflow);
    CHECK(c.used <= UC_POOL);
    CHECK(c.used > UC_POOL - (3 + UC_MAX_VALUE + 1));
    CHECK_INT(n, UC_POOL / (3 + UC_MAX_VALUE));         /* as many as fit, whole */
    CHECK_STR(upconf_get(&c, "p0", "kA"), val);
    /* the last bytes exactly filled, then one byte more than that refused */
    {
        int left = UC_POOL - c.used - 4; /* "zz" NUL value NUL */
        CHECK(left >= 1 && left < UC_MAX_VALUE);
        val[left] = 0;
        CHECK(upconf_set(&c, "p3", "zz", val));
        CHECK_INT(c.used, UC_POOL);
        c.overflow = 0;
        val[left] = 'v';
        val[left + 1] = 0;
        CHECK(!upconf_set(&c, "p3", "zz", val));
        CHECK(c.overflow);
        CHECK_INT((long)strlen(upconf_get(&c, "p3", "zz")), left); /* the old value stands */
    }

    /* a value set from the table's own pool (a lookup handed straight
     * back): copied before anything moves */
    {
        static const char abc[] = "a = one\nb = twotwo\nc = three\n";
        upconf_parse(&c, abc, (long)strlen(abc));
    }
    CHECK(upconf_set(&c, "default", "a", upconf_get(&c, "default", "b")));
    CHECK(upconf_set(&c, "default", "c", upconf_get(&c, "default", "c")));
    CHECK(upconf_set(&c, "default", "b", upconf_get(&c, "default", "c")));
    CHECK_STR(upconf_get(&c, "default", "a"), "twotwo");
    CHECK_STR(upconf_get(&c, "default", "b"), "three");
    CHECK_STR(upconf_get(&c, "default", "c"), "three");

    /* values replaced a thousand times leave no garbage behind, and the
     * others stay whole; a delete gives its bytes back */
    used = c.used;
    for (k = 0; k < 1000; k++)
        upconf_set(&c, "default", "a", k & 1 ? "x" : "a much longer value than x");
    CHECK_INT(c.used, used - 6 + 1);
    CHECK_STR(upconf_get(&c, "default", "a"), "x");
    CHECK_STR(upconf_get(&c, "default", "b"), "three");
    CHECK(upconf_set(&c, "vim", "bell", "none"));
    CHECK(upconf_set(&c, "default", "font", "topaz 8"));
    CHECK(upconf_del(&c, "default", "b"));
    CHECK(upconf_rmprof(&c, "vim"));
    CHECK_INT(c.used, used - 6 + 1 - 8 + (int)sizeof("font") + (int)sizeof("topaz 8"));
    CHECK_STR(upconf_get(&c, "default", "font"), "topaz 8");
    CHECK_STR(upconf_get(&c, "default", "c"), "three");
    CHECK(upconf_get(&c, "default", "b") == 0);
    CHECK(upconf_get(&c, "vim", "bell") == 0);

    /* a live reload parses the new file into a used table: nothing of the
     * old one is left, and every profile reads (a profile switch) */
    {
        static const char two[] = "[profile vim]\nbg = 000000\n; note\n[profile default]\nfg = 101010\n";
        upconf_parse(&c, two, (long)strlen(two));
    }
    CHECK(upconf_get(&c, "default", "font") == 0);
    CHECK(upconf_get(&c, "default", "c") == 0);
    CHECK_STR(upconf_get(&c, "vim", "bg"), "000000");
    CHECK_STR(upconf_get(&c, "default", "fg"), "101010");
    CHECK_INT(c.nnote, 1);
    /* a copy is a plain memcpy: no pointers inside */
    memcpy(&back, &c, sizeof(c));
    upconf_set(&c, "vim", "bg", "FFFFFF");
    CHECK_STR(upconf_get(&back, "vim", "bg"), "000000");
    CHECK(!upconf_profile_equal(&c, &back, "vim"));
    CHECK(upconf_profile_equal(&c, &back, "default"));
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
    comments_kept();
    packed_table();
}
