/* Patina's own settings file (config/amconf_core.c): the key table, parse, serialize, the UP-Term file as the
 * legacy source, the load chain, the migration and the shared install path, against a file table in memory. */
#include <stdio.h>
#include <string.h>
#include "../config/amconf_core.h"
#include "../patina/leaf.h"
#include "../prefs/settings_def.h"
#include "../prefs/prefs_ui.h"
#include "harness.h"
#include "memfs.h"

#define AP "ENVARC:Patina/prefs"
#define EP "ENV:Patina/prefs"
#define UP "ENV:up-term/up-term"
#define UA "ENVARC:up-term/up-term"

static amconf_io io_of(memfs *m, prefs_fs *fs)
{
    amconf_io io;
    mf_init(m, fs);
    io.fs = fs;
    io.read = mf_read;
    io.dir = mf_dir;
    io.alloc = mf_prefs_alloc;
    io.release = mf_prefs_release;
    return io;
}

static void test_lf_setting_check_toggle(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    const sd_setting *s = &sd_settings[SDI_BLINK];
    upconf c;
    prefs_fields f;
    char buf[UC_MAX_FILE + 2];
    long got;
    int failed = -1;
    mf_put(&m, UP, "[default]\nfont = topaz\n");
    CHECK_INT(lf_setting_step(&io, s, &failed), LF_SET_OK);
    got = io.read(io.fs->ctx, UP, buf, UC_MAX_FILE + 1);
    buf[got < 0 ? 0 : got] = 0;
    CHECK_INT(prefs_load(&c, buf, got, UC_MAX_FILE), PREFS_LOAD_OK);
    prefs_from_conf(&f, &c, LF_PROFILE);
    CHECK_INT(sd_index(s, &f), 1);
    CHECK(strstr(buf, "font") != 0);                         /* the other keys stay */
    CHECK(mf_get(&m, UA) != 0);                              /* ENVARC: written too (keep) */
    CHECK_STR(mf_get(&m, "ENV:up-term/highlight-cat"), "on\n");
    CHECK_INT(lf_setting_step(&io, s, &failed), LF_SET_OK);  /* toggles back */
    got = io.read(io.fs->ctx, UP, buf, UC_MAX_FILE + 1);
    buf[got] = 0;
    prefs_load(&c, buf, got, UC_MAX_FILE);
    prefs_from_conf(&f, &c, LF_PROFILE);
    CHECK_INT(sd_index(s, &f), 0);
}

static void test_lf_setting_cycle_next_and_choices(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    const sd_setting *s = &sd_settings[SDI_AM_ICONS];
    mn_item it[8];
    am_prefs a;
    int v, failed = -1;
    CHECK_INT(s->nvalues >= 2, 1);
    CHECK_INT(lf_setting_next(s, 0), 1);
    CHECK_INT(lf_setting_next(s, s->nvalues - 1), 0);        /* wraps */
    CHECK_INT(lf_setting_next(&sd_settings[SDI_SCROLLBACK], 0), -1);
    CHECK_INT(lf_setting_step(&io, s, &failed), LF_SET_OK);  /* Patina's own file */
    CHECK(mf_get(&m, AP) != 0);
    amconf_load_with(&io, &a);
    CHECK_INT(sd_index(s, &a), (sd_def_index(s) + 1) % s->nvalues);
    memset(it, 0, sizeof it);
    for (v = 0; v < s->nvalues && v < 8; v++) {
        it[v].row = MN_ROW_CHOICE;
        it[v].ix = s->id;
        it[v].seg[0] = (char)('0' + v);
        it[v].seg[1] = 0;
        strcpy(it[v].label, s->values[v].text);
    }
    lf_setting_labels(&io, it, v);
    for (v = 0; v < s->nvalues && v < 8; v++)
        CHECK_INT(it[v].label[0] == '*', v == sd_index(s, &a));
    it[0].row = MN_ROW_SETTING;                              /* a setting row shows "Label: Value" */
    lf_setting_labels(&io, it, 1);
    CHECK(strchr(it[0].label, ':') != 0);
}

static void test_lf_setting_text(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    const sd_setting *sb = &sd_settings[SDI_SCROLLBACK], *mu = &sd_settings[SDI_AM_MUSIC];
    char out[UC_MAX_VALUE + 2], before[UC_MAX_FILE + 2];
    const char *f;
    am_prefs a;
    int failed = -1;
    mf_put(&m, UP, "[default]\nfont = topaz\nscrollback = 300\n");
    mf_put(&m, UA, "[default]\nscrollback = 300\n");
    CHECK_INT(lf_setting_text(&io, sb, out, sizeof out), 1);
    CHECK_STR(out, "300");                                   /* the prompt's prefill */
    f = mf_get(&m, UP);
    snprintf(before, sizeof before, "%s", f);
    CHECK_INT(lf_setting_set_text(&io, sb, "100000", &failed), LF_SET_INVALID);   /* over 99999 */
    CHECK_INT(lf_setting_set_text(&io, sb, "12x", &failed), LF_SET_INVALID);
    CHECK_INT(lf_setting_set_text(&io, sb, "-5", &failed), LF_SET_INVALID);
    CHECK_STR(mf_get(&m, UP), before);                       /* file bytes unchanged */
    CHECK_STR(mf_get(&m, UA), "[default]\nscrollback = 300\n");   /* and ENVARC: untouched */
    CHECK_INT(lf_setting_set_text(&io, sb, "1200", &failed), LF_SET_OK);
    lf_setting_text(&io, sb, out, sizeof out);
    CHECK_STR(out, "1200");                                  /* written and reloaded */
    CHECK(strstr(mf_get(&m, UP), "font = topaz") != 0);
    CHECK_INT(lf_setting_set_text(&io, mu, "Mods:Music", &failed), LF_SET_OK);   /* Patina's file */
    amconf_load_with(&io, &a);
    CHECK_STR(a.val[AM_MUSIC], "Mods:Music");
    CHECK_INT(lf_setting_set_text(&io, mu, "a\tb", &failed), LF_SET_INVALID);
    CHECK_INT(lf_setting_set_text(&io, &sd_settings[SDI_BLINK], "1", &failed), LF_SET_KIND);
    CHECK_INT(sb->max, PREFS_SB_MAX);                        /* the row's bound is Prefs' cut */
    /* presets (S87): a preset writes its text, Custom keeps the range check, a value no preset names survives */
    {
        const sd_setting *tm = &sd_settings[SDI_TERM];
        mn_item ch[2];
        CHECK(SD_HAS_PRESETS(sb) && SD_HAS_PRESETS(tm) && !SD_HAS_PRESETS(mu));
        CHECK_INT(lf_setting_set_preset(&io, sb, 2, 0, &failed), LF_SET_OK);   /* 1000 */
        lf_setting_text(&io, sb, out, sizeof out);
        CHECK_STR(out, "1000");
        CHECK_INT(lf_setting_set_preset(&io, sb, sb->nvalues - 1, 0, &failed), LF_SET_KIND);   /* Custom needs its text */
        CHECK_INT(lf_setting_set_preset(&io, sb, sb->nvalues - 1, "100000", &failed), LF_SET_INVALID);
        CHECK_INT(lf_setting_set_preset(&io, sb, sb->nvalues - 1, "777", &failed), LF_SET_OK);
        lf_setting_text(&io, sb, out, sizeof out);
        CHECK_STR(out, "777");
        memset(ch, 0, sizeof ch);
        ch[0].row = MN_ROW_CHOICE;
        ch[0].ix = sb->id;
        strcpy(ch[0].seg, "1");   /* 500 */
        ch[1] = ch[0];
        strcpy(ch[1].seg, "5");   /* Custom */
        lf_setting_labels(&io, ch, 2);
        CHECK_STR(ch[0].label, "  500");
        CHECK_STR(ch[1].label, "* Custom...");                /* 777 is no preset: Custom is marked */
        CHECK_STR(sd_preset_next(sb, "5000"), "20000");
        CHECK_STR(sd_preset_next(sb, "20000"), "100");        /* wraps */
        CHECK_STR(sd_preset_next(sb, "777"), "1000");         /* between presets: the next larger */
        CHECK_STR(sd_preset_next(sb, "99999"), "100");
        CHECK_INT(lf_setting_set_preset(&io, tm, 2, 0, &failed), LF_SET_OK);
        lf_setting_text(&io, tm, out, sizeof out);
        CHECK_STR(out, "xterm-256color");
        CHECK_INT(lf_setting_set_preset(&io, tm, tm->nvalues - 1, "my-term", &failed), LF_SET_OK);
        lf_setting_text(&io, tm, out, sizeof out);
        CHECK_STR(out, "my-term");                            /* any text survives through Other */
        CHECK_STR(sd_preset_next(tm, "my-term"), "vtcon");
        CHECK_STR(sd_preset_next(tm, ""), "xterm");           /* empty is vtcon: the next is xterm */
        CHECK_INT(lf_setting_set_text(&io, tm, "", &failed), LF_SET_OK);
        ch[0].ix = tm->id;
        strcpy(ch[0].seg, "0");   /* vtcon */
        lf_setting_labels(&io, ch, 1);
        CHECK_STR(ch[0].label, "* vtcon");                    /* empty term marks vtcon */
        CHECK_STR(sd_preset_next(tm, "linux"), "vtcon");
    }
}

/* S89: under an active shipped Patina theme the Colors, Palette and Cursor rows read and write the theme (term_* keys in its user
 * copy, GT_TERM_FILE rewritten); the shipped file and the profile stay as they were */
static void test_lf_setting_term_theme(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    const sd_setting *pal = &sd_settings[SDI_PALETTE], *fg = &sd_settings[SDI_FG], *cur = &sd_settings[SDI_CURSOR_COLOUR];
    const char *shipped = "mode = \"dark\"\nbackground = \"#2e3440\"\nforeground = \"#d8dee9\"\nred = \"#bf616a\"\n";
    char before[UC_MAX_FILE + 2], hex[16], out[16];
    int failed = -1;
    mf_put(&m, AP, "patina-gui-theme = nord\n");
    mf_put(&m, UP, "[default]\nfont = topaz\nfg = 112233\n");
    mf_put(&m, "UP-Term:Patina/Themes/nord.theme", shipped);
    snprintf(before, sizeof before, "%s", mf_get(&m, UP));
    lf_colour_current(&io, fg, 0, out);
    CHECK_STR(out, "D8DEE9");                                /* reads the theme's terminal colour, not the profile's */
    CHECK_INT(lf_setting_set_colour(&io, fg, SD_PICK_CUSTOM, "aabbcc", &failed), LF_SET_OK);
    CHECK_STR(mf_get(&m, UP), before);                       /* profile untouched */
    CHECK(strstr(mf_get(&m, "ENVARC:Patina/themes/nord.theme"), "term_fg = AABBCC") != 0);   /* user copy */
    CHECK_STR(mf_get(&m, "UP-Term:Patina/Themes/nord.theme"), shipped);   /* shipped file unchanged */
    CHECK(strstr(mf_get(&m, "ENV:up-term/theme-active"), "fg = AABBCC") != 0);
    lf_colour_current(&io, fg, 0, out);
    CHECK_STR(out, "AABBCC");
    CHECK_INT(lf_setting_set_colour(&io, pal, 5, "#aa11BB", &failed), LF_SET_OK);
    lf_palette_hex(&io, 5, hex);
    CHECK_STR(hex, "AA11BB");
    CHECK(strstr(mf_get(&m, "ENVARC:Patina/themes/nord.theme"), "term_palette") != 0);
    CHECK(strstr(mf_get(&m, "ENV:up-term/theme-active"), "5,AA11BB") != 0);
    CHECK_INT(lf_setting_set_colour(&io, cur, SD_PICK_CUSTOM, "ff0000", &failed), LF_SET_OK);
    CHECK(strstr(mf_get(&m, "ENV:up-term/theme-active"), "cursor-color = FF0000") != 0);
    CHECK_STR(mf_get(&m, UP), before);
    /* S90: deferred sets leave GT_TERM_FILE alone; one flush writes the last value */
    mf_put(&m, "ENV:up-term/theme-active", "stale\n");
    lf_term_defer(1);
    CHECK_INT(lf_setting_set_colour(&io, fg, SD_PICK_CUSTOM, "010203", &failed), LF_SET_OK);
    CHECK_INT(lf_setting_set_colour(&io, fg, SD_PICK_CUSTOM, "040506", &failed), LF_SET_OK);
    CHECK_INT(lf_setting_set_colour(&io, fg, SD_PICK_CUSTOM, "070809", &failed), LF_SET_OK);
    lf_term_defer(0);
    CHECK_STR(mf_get(&m, "ENV:up-term/theme-active"), "stale\n");
    CHECK(strstr(mf_get(&m, "ENVARC:Patina/themes/nord.theme"), "term_fg = 070809") != 0);
    CHECK_INT(lf_term_flush(&io), 1);
    CHECK(strstr(mf_get(&m, "ENV:up-term/theme-active"), "fg = 070809") != 0);
    CHECK_INT(lf_term_flush(&io), 0);                        /* nothing pending: no second write */
}

/* LF_SETTING colour pickers (plan 2026-10-26-patina-prefs-inline.md, M5): picking palette entry 5 writes that
 * slot to ENV: and ENVARC:; a swatch takes a palette entry or a typed hex; bad hex leaves the files alone. */
static void test_lf_setting_colour_picker(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    const sd_setting *pal = &sd_settings[SDI_PALETTE], *fg = &sd_settings[SDI_FG];
    char before[UC_MAX_FILE + 2], hex[16];
    mn_item it[20];
    upconf c;
    prefs_fields f;
    char buf[UC_MAX_FILE + 2];
    long got;
    int failed = -1, t;
    mf_put(&m, UP, "[default]\nfont = topaz\n");
    mf_put(&m, UA, "[default]\nfont = topaz\n");
    snprintf(before, sizeof before, "%s", mf_get(&m, UP));
    CHECK_INT(lf_setting_set_colour(&io, pal, 5, "zz0000", &failed), LF_SET_INVALID);   /* bad hex */
    CHECK_INT(lf_setting_set_colour(&io, pal, 5, "12345", &failed), LF_SET_INVALID);
    CHECK_INT(lf_setting_set_colour(&io, pal, 16, "112233", &failed), LF_SET_KIND);
    CHECK_INT(lf_setting_set_colour(&io, pal, 5, 0, &failed), LF_SET_KIND);
    CHECK_STR(mf_get(&m, UP), before);                       /* ENV: unchanged */
    CHECK_STR(mf_get(&m, UA), before);                       /* ENVARC: unchanged */
    CHECK_INT(lf_setting_set_colour(&io, pal, 5, "#aa11BB", &failed), LF_SET_OK);
    for (t = 0; t < 2; t++) {
        got = io.read(io.fs->ctx, t ? UA : UP, buf, UC_MAX_FILE + 1);
        buf[got < 0 ? 0 : got] = 0;
        CHECK_INT(prefs_load(&c, buf, got, UC_MAX_FILE), PREFS_LOAD_OK);
        prefs_from_conf(&f, &c, LF_PROFILE);
        CHECK_STR(f.pal[5], "AA11BB");                       /* the slot, in both files */
        CHECK_STR(f.pal[4], "");                             /* and only that slot */
        CHECK(strstr(buf, "font") != 0);
    }
    lf_palette_hex(&io, 5, hex);
    CHECK_STR(hex, "AA11BB");
    lf_palette_hex(&io, 1, hex);
    CHECK_STR(hex, "CD0000");                                /* unset entry: xterm's built-in */
    /* a swatch picks palette entry 5, then a typed hex */
    CHECK_INT(lf_setting_set_colour(&io, fg, 5, 0, &failed), LF_SET_OK);
    got = io.read(io.fs->ctx, UP, buf, UC_MAX_FILE + 1);
    buf[got] = 0;
    prefs_load(&c, buf, got, UC_MAX_FILE);
    prefs_from_conf(&f, &c, LF_PROFILE);
    CHECK_STR(f.fg, "AA11BB");
    CHECK_INT(lf_setting_set_colour(&io, fg, SD_PICK_CUSTOM, "nothex", &failed), LF_SET_INVALID);
    CHECK_INT(lf_setting_set_colour(&io, fg, SD_PICK_CUSTOM, "010203", &failed), LF_SET_OK);
    lf_colour_current(&io, fg, 0, hex);
    CHECK_STR(hex, "010203");
    /* the picker level's rows: 16 entries (+ Custom for a swatch), labelled with the entry name and hex */
    memset(it, 0, sizeof it);
    it[0].row = MN_ROW_CHOICE; it[0].ix = SDI_PALETTE; strcpy(it[0].seg, "5");
    it[1].row = MN_ROW_CHOICE; it[1].ix = SDI_FG; strcpy(it[1].seg, "16");
    lf_setting_labels(&io, it, 2);
    CHECK(strstr(it[0].label, "AA11BB") != 0);
    CHECK_STR(it[1].label, "Custom...");
    /* M8: the drawn menu's chip colour per row (lf_row_chip), the text menu's hex in the label */
    {
        long chip[6];
        memset(it, 0, sizeof it);
        it[0].row = MN_ROW_SETTING; it[0].ix = SDI_FG;
        it[1].row = MN_ROW_CHOICE; it[1].ix = SDI_PALETTE; strcpy(it[1].seg, "5");
        it[2].row = MN_ROW_CHOICE; it[2].ix = SDI_FG; strcpy(it[2].seg, "1");
        it[3].row = MN_ROW_CHOICE; it[3].ix = SDI_FG; strcpy(it[3].seg, "16");
        it[4].row = MN_ROW_SETTING; it[4].ix = SDI_BLINK;
        it[5].row = MN_ROW_SETTING; it[5].ix = SDI_PALETTE;
        lf_setting_chips(&io, it, 6, chip);
        CHECK_INT(chip[0], 0x010203L);                       /* a swatch row with its colour set */
        CHECK_INT(chip[1], 0xAA11BBL);                       /* palette entry 5 as written */
        CHECK_INT(chip[2], 0xCD0000L);                       /* an unset entry: xterm's built-in */
        CHECK_INT(chip[3], -1L);                             /* Custom... has no chip */
        CHECK_INT(chip[4], -1L);                             /* a cycle row has none */
        CHECK_INT(chip[5], -1L);                             /* nor the palette row itself ("...") */
        lf_setting_labels(&io, it, 1);
        CHECK(strstr(it[0].label, "#010203") != 0);          /* text menu: the value as hex */
    }
}


/* fake requesters: a pick, or a cancel; calls counted so the test proves the fake ran */
typedef struct fake_asl { int ok, font_calls, mode_calls, fixed; char in_name[64]; long in_size; unsigned long in_id; int in_depth; int otag_calls; char otag_path[64]; } fake_asl;
static int fake_font(void *ctx, const char *name, long size, int fixed, char *out, int cap, long *osize)
{
    fake_asl *f = (fake_asl *)ctx;
    f->font_calls++;
    f->fixed = fixed;
    snprintf(f->in_name, sizeof f->in_name, "%s", name);
    f->in_size = size;
    if (!f->ok)
        return 0;
    snprintf(out, (size_t)cap, "%s", "JetBrains Mono");
    *osize = 13;
    return 1;
}
static int fake_mode(void *ctx, unsigned long id, int depth, unsigned long *oid, int *odepth)
{
    fake_asl *f = (fake_asl *)ctx;
    f->mode_calls++;
    f->in_id = id;
    f->in_depth = depth;
    if (!f->ok)
        return 0;
    *oid = 0x29000UL;
    *odepth = 6;
    return 1;
}

static int fake_otag(void *ctx, char *out, int cap)
{
    fake_asl *f = (fake_asl *)ctx;
    f->otag_calls++;
    if (!f->ok)
        return 0;
    snprintf(out, (size_t)cap, "%s", f->otag_path);
    return 1;
}

/* a theme font row (store SD_GTHEME) opens the requester and stores "name.font/size" through gte_row_set_text */
static void test_lf_theme_font_pick(void)
{
    static gt_model gm;
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    fake_asl fk;
    lf_asl asl;
    const sd_setting *row = 0;
    char out[80];
    int i, failed = 0;
    memset(&fk, 0, sizeof fk);
    memset(&gm, 0, sizeof gm);
    strcpy(gm.look.font[GT_FONT_SHELL], "courier.font/11");
    gte_open(&gm, "t");
    for (i = SDI_COUNT; i < SDI_COUNT + gte_nrows(); i++)
        if (gte_row(i)->kind == SD_FONT && gte_row(i)->aux == GT_FONT_SHELL + 1)
            row = gte_row(i);
    CHECK(row != 0);
    if (!row)
        return;
    asl.font = fake_font;
    asl.mode = fake_mode;
    asl.otag = fake_otag;
    asl.ctx = &fk;
    CHECK_INT(lf_setting_pick(&io, row, &asl, &failed), LF_SET_CANCELLED);
    CHECK_STR(fk.in_name, "courier.font");
    CHECK_INT(fk.in_size, 11);
    CHECK_INT(fk.fixed, 1);
    gte_row_text(row, out, sizeof out);
    CHECK_STR(out, "courier.font/11");                   /* cancel keeps the old value */
    fk.ok = 1;
    CHECK_INT(lf_setting_pick(&io, row, &asl, &failed), LF_SET_OK);
    gte_row_text(row, out, sizeof out);
    CHECK_STR(out, "JetBrains Mono.font/13");
}

static char drawer_start[UC_MAX_VALUE];

static int fake_drawer(void *ctx, const char *start, char *out, int cap)
{
    fake_asl *f = (fake_asl *)ctx;
    snprintf(drawer_start, sizeof drawer_start, "%s", start);
    if (!f->ok)
        return 0;
    snprintf(out, (size_t)cap, "%s", f->otag_path);
    return 1;
}

/* TI-4: the drawer rows open a drawers-only requester; a pick joins the comma list, a listed drawer leaves it */
static void test_lf_drawer_pick(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    const sd_setting *row = &sd_settings[SDI_AM_MUSIC];
    fake_asl fk;
    lf_asl asl;
    char out[UC_MAX_VALUE];
    int failed = 0;
    memset(&fk, 0, sizeof fk);
    memset(&asl, 0, sizeof asl);
    asl.drawer = fake_drawer;
    asl.ctx = &fk;
    CHECK(lf_is_drawer_row(row));
    CHECK(!lf_is_drawer_row(&sd_settings[SDI_FONT]));
    mf_put(&m, AP, "patina-music-drawers = Work:Music\n");
    CHECK_INT(lf_setting_pick(&io, row, &asl, &failed), LF_SET_CANCELLED);
    lf_setting_text(&io, row, out, sizeof out);
    CHECK_STR(out, "Work:Music");
    CHECK_STR(drawer_start, "Work:Music");               /* the requester opens at the row's drawer */
    fk.ok = 1;
    strcpy(fk.otag_path, "Work:Mods");
    CHECK_INT(lf_setting_pick(&io, row, &asl, &failed), LF_SET_OK);
    lf_setting_text(&io, row, out, sizeof out);
    CHECK_STR(out, "Work:Music,Work:Mods");
    strcpy(fk.otag_path, "Work:Music");                  /* already listed: removed */
    CHECK_INT(lf_setting_pick(&io, row, &asl, &failed), LF_SET_OK);
    lf_setting_text(&io, row, out, sizeof out);
    CHECK_STR(out, "Work:Mods");
    CHECK_STR(drawer_start, "Work:Mods");                /* last of the list "Work:Music,Work:Mods" */
    mf_init(&m, &fs);                                    /* fresh store: nothing stored */
    lf_setting_pick(&io, row, &asl, &failed);
    CHECK_STR(drawer_start, "");                         /* empty row: the default drawer */
}

/* LF_SETTING font and screen mode pickers (plan 2026-10-26-patina-prefs-inline.md, M6): a picked font or mode
 * is written to ENV: and ENVARC:, a cancelled requester leaves both files byte-identical. */
static void test_lf_setting_asl_pickers(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    const sd_setting *font = &sd_settings[SDI_FONT], *mode = &sd_settings[SDI_SCREEN_MODE], *mf = &sd_settings[SDI_AM_FONT];
    fake_asl fk;
    lf_asl asl;
    char before_u[UC_MAX_FILE + 2], before_a[UC_MAX_FILE + 2], buf[UC_MAX_FILE + 2];
    upconf c;
    prefs_fields f;
    am_prefs a;
    long got;
    int failed = -1, t;
    memset(&fk, 0, sizeof fk);
    asl.font = fake_font;
    asl.mode = fake_mode;
    asl.otag = fake_otag;
    asl.ctx = &fk;
    mf_put(&m, UP, "[default]\nfont = topaz:8\nscreen-mode = 0x11000\nscreen-depth = 4\n");
    mf_put(&m, UA, "[default]\nfont = topaz:8\nscreen-mode = 0x11000\nscreen-depth = 4\n");
    /* cancelled: nothing written, in either store */
    snprintf(before_u, sizeof before_u, "%s", mf_get(&m, UP));
    snprintf(before_a, sizeof before_a, "%s", mf_get(&m, UA));
    CHECK_INT(lf_setting_pick(&io, font, &asl, &failed), LF_SET_CANCELLED);
    CHECK_STR(fk.in_name, "topaz.font");                          /* the requester starts on the current value */
    CHECK_INT(fk.in_size, 8);
    CHECK_INT(lf_setting_pick(&io, mode, &asl, &failed), LF_SET_CANCELLED);
    CHECK_INT(fk.in_id, 0x11000);
    CHECK_INT(fk.in_depth, 4);
    CHECK_INT(lf_setting_pick(&io, mf, &asl, &failed), LF_SET_CANCELLED);
    CHECK_INT(fk.font_calls, 2);                             /* the requesters ran */
    CHECK_INT(fk.mode_calls, 1);
    CHECK_STR(mf_get(&m, UP), before_u);
    CHECK_STR(mf_get(&m, UA), before_a);
    CHECK_STR(mf_get(&m, AP), "<absent>");                /* Patina's file never created */
    /* picked */
    fk.ok = 1;
    CHECK_INT(lf_setting_pick(&io, font, &asl, &failed), LF_SET_OK);
    CHECK_INT(fk.fixed, 1);                                  /* the terminal font asks for fixed width */
    CHECK_INT(lf_setting_pick(&io, mode, &asl, &failed), LF_SET_OK);
    for (t = 0; t < 2; t++) {
        got = io.read(io.fs->ctx, t ? UA : UP, buf, UC_MAX_FILE + 1);
        buf[got < 0 ? 0 : got] = 0;
        CHECK_INT(prefs_load(&c, buf, got, UC_MAX_FILE), PREFS_LOAD_OK);
        prefs_from_conf(&f, &c, LF_PROFILE);
        CHECK_STR(f.font, "JetBrains Mono:13");
        CHECK_STR(f.screenmode, "0x00029000");
        CHECK_STR(f.screendepth, "6");
    }
    /* the menu font lives in Patina's file, and any width may be picked */
    CHECK_INT(lf_setting_pick(&io, mf, &asl, &failed), LF_SET_OK);
    CHECK_INT(fk.fixed, 0);
    amconf_load_with(&io, &a);
    CHECK_STR(a.val[AM_FONT], "JetBrains Mono:13");
    /* the fallback font is an outline file (.otag), stored as its base name in both files */
    fk.ok = 0;
    CHECK_INT(lf_setting_pick(&io, &sd_settings[SDI_FALLBACK], &asl, &failed), LF_SET_CANCELLED);
    CHECK_INT(fk.otag_calls, 1);                             /* the file requester ran, not the font one */
    fk.ok = 1;
    strcpy(fk.otag_path, "FONTS:Symbols.txt");
    CHECK_INT(lf_setting_pick(&io, &sd_settings[SDI_FALLBACK], &asl, &failed), LF_SET_INVALID);
    strcpy(fk.otag_path, "FONTS:SymbolsNerd.otag");
    CHECK_INT(lf_setting_pick(&io, &sd_settings[SDI_FALLBACK], &asl, &failed), LF_SET_OK);
    for (t = 0; t < 2; t++) {
        got = io.read(io.fs->ctx, t ? UA : UP, buf, UC_MAX_FILE + 1);
        buf[got < 0 ? 0 : got] = 0;
        CHECK_INT(prefs_load(&c, buf, got, UC_MAX_FILE), PREFS_LOAD_OK);
        prefs_from_conf(&f, &c, LF_PROFILE);
        CHECK_STR(f.fallback, "SymbolsNerd");
    }
}

/* M7: the profile list and the theme list ------------------------------------------------------------ */
static memfs *list_fs;      /* the file table the fake drawer listing scans */

static int mem_list(const char *dir, void (*add)(void *ud, const char *file), void *ud)
{
    size_t l = strlen(dir);
    int i;
    for (i = 0; i < FS_FILES; i++)
        if (list_fs->used[i] && !strncmp(list_fs->name[i], dir, l) && list_fs->name[i][l] == '/')
            add(ud, list_fs->name[i] + l + 1);
    return 1;
}

static void test_lf_profile_and_theme_lists(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    static const char file[] =
        "# my terminals\n[profile default]\nfont = topaz.font:8\ncursor-blink = off\nscrollback = 777\n\n"
        "[profile work]\n# the office box\nfont = JetBrains Mono:13\nfg = ff8800\nbg = 102030\nscrollback = 4000\n";
    char names[256];
    upconf c;
    int failed = 0, k;
    static const char *const wkeys[] = { "font", "fg", "bg", "scrollback" };
    static const char *const wvals[] = { "JetBrains Mono:13", "ff8800", "102030", "4000" };
    io.list = mem_list;
    list_fs = &m;
    mf_put(&m, UP, file);
    mf_put(&m, UA, file);
    CHECK_INT(lf_profile_use(&io, "default"), LF_SET_OK);
    CHECK_INT(lf_profile_names(&io, names, sizeof names), 2);
    CHECK_INT(lf_profile_use(&io, "work"), LF_SET_OK);
    CHECK_STR(lf_profile(), "work");
    lf_reset();                 /* a new open of the warm server starts on "default" again */
    CHECK_STR(lf_profile(), "default");
    CHECK_STR(names, "default");
    CHECK_STR(names + 8, "work");

    /* rename keeps every key, the other profile and both files; the comment goes with it */
    CHECK_INT(lf_profile_rename(&io, "work", "  [office] ", &failed), LF_SET_OK);
    upconf_parse(&c, mf_get(&m, UA), (long)strlen(mf_get(&m, UA)));
    CHECK(!prefs_profile_exists(&c, "work"));
    CHECK(prefs_profile_exists(&c, "office"));
    for (k = 0; k < 4; k++)
        CHECK_STR(upconf_str(&c, "office", wkeys[k], "<absent>"), wvals[k]);
    CHECK_STR(upconf_str(&c, "default", "scrollback", ""), "777");
    CHECK_STR(upconf_str(&c, "default", "cursor-blink", ""), "off");
    CHECK(strstr(mf_get(&m, UA), "# the office box") != 0);
    CHECK_STR(mf_get(&m, UP), mf_get(&m, UA));
    CHECK_INT(lf_profile_rename(&io, "nosuch", "x", &failed), LF_SET_MISSING);
    CHECK_INT(lf_profile_rename(&io, "office", "DEFAULT", &failed), LF_SET_EXISTS);
    CHECK_INT(lf_profile_rename(&io, "office", " [] ", &failed), LF_SET_INVALID);

    /* renaming the active profile moves the rows with it */
    CHECK_INT(lf_profile_use(&io, "office"), LF_SET_OK);
    CHECK_STR(lf_profile(), "office");
    CHECK_INT(lf_profile_rename(&io, "office", "lab", &failed), LF_SET_OK);
    CHECK_STR(lf_profile(), "lab");

    /* new: a copy of the active profile, which becomes the active one; a taken name is refused */
    CHECK_INT(lf_profile_new(&io, "copy", &failed), LF_SET_OK);
    CHECK_STR(lf_profile(), "copy");
    upconf_parse(&c, mf_get(&m, UA), (long)strlen(mf_get(&m, UA)));
    for (k = 0; k < 4; k++)
        CHECK_STR(upconf_str(&c, "copy", wkeys[k], "<absent>"), wvals[k]);
    CHECK_INT(lf_profile_new(&io, "LAB", &failed), LF_SET_EXISTS);
    CHECK_INT(lf_profile_new(&io, "  ", &failed), LF_SET_INVALID);
    CHECK_INT(lf_profile_names(&io, names, sizeof names), 3);

    /* delete: gone from both files; deleting the active one hands over to the first profile left */
    CHECK_INT(lf_profile_delete(&io, "copy", &failed), LF_SET_OK);
    CHECK_STR(lf_profile(), "default");
    CHECK(strstr(mf_get(&m, UP), "copy") == 0);
    CHECK_INT(lf_profile_delete(&io, "copy", &failed), LF_SET_MISSING);
    CHECK_INT(lf_profile_delete(&io, "lab", &failed), LF_SET_OK);
    CHECK_INT(lf_profile_delete(&io, "default", &failed), LF_SET_LAST);
    CHECK_INT(lf_profile_names(&io, names, sizeof names), 1);

    /* themes: new holds the active profile's colours; apply, rename, delete */
    mf_put(&m, UP, file);
    mf_put(&m, UA, file);
    CHECK_INT(lf_profile_use(&io, "work"), LF_SET_OK);
    CHECK_INT(lf_theme_names(&io, names, sizeof names), 0);
    CHECK_INT(lf_theme_new(&io, "Sunset", &failed), LF_SET_OK);
    CHECK_INT(lf_theme_new(&io, "Sunset", &failed), LF_SET_EXISTS);
    CHECK_INT(lf_theme_new(&io, "a/b", &failed), LF_SET_INVALID);
    CHECK_INT(lf_theme_names(&io, names, sizeof names), 1);
    CHECK_STR(names, "Sunset");
    CHECK(strstr(mf_get(&m, "ENVARC:up-term/themes/Sunset.conf"), "ff8800") != 0);
    CHECK(strstr(mf_get(&m, "ENVARC:up-term/themes/Sunset.conf"), "scrollback") == 0);   /* colours only */
    CHECK_STR(mf_get(&m, "ENV:up-term/themes/Sunset.conf"), mf_get(&m, "ENVARC:up-term/themes/Sunset.conf"));
    CHECK_INT(lf_theme_rename(&io, "Sunset", "Dusk", &failed), LF_SET_OK);
    CHECK_INT(lf_theme_names(&io, names, sizeof names), 1);
    CHECK_STR(names, "Dusk");
    CHECK_STR(mf_get(&m, "ENV:up-term/themes/Dusk.conf") , mf_get(&m, "ENVARC:up-term/themes/Dusk.conf"));
    CHECK_INT(lf_theme_rename(&io, "Sunset", "Other", &failed), LF_SET_MISSING);
    CHECK_INT(lf_profile_use(&io, "default"), LF_SET_OK);
    CHECK_INT(lf_theme_apply(&io, "Dusk", &failed), LF_SET_OK);
    upconf_parse(&c, mf_get(&m, UA), (long)strlen(mf_get(&m, UA)));
    CHECK_STR(upconf_str(&c, "default", "fg", ""), "ff8800");
    CHECK_STR(upconf_str(&c, "default", "scrollback", ""), "777");      /* the rest of the profile stays */
    CHECK_INT(lf_theme_apply(&io, "Nothing", &failed), LF_SET_MISSING);
    CHECK_INT(lf_theme_delete(&io, "Dusk", &failed), LF_SET_OK);
    CHECK_INT(lf_theme_names(&io, names, sizeof names), 0);
    CHECK_STR(mf_get(&m, "ENV:up-term/themes/Dusk.conf"), "<absent>");
    CHECK_INT(lf_theme_delete(&io, "Dusk", &failed), LF_SET_MISSING);
    CHECK_INT(lf_profile_use(&io, "default"), LF_SET_OK);
}

void suite_amconf_patina(void)
{
    test_lf_setting_check_toggle();
    test_lf_setting_cycle_next_and_choices();
    test_lf_setting_text();
    test_lf_setting_colour_picker();
    test_lf_setting_term_theme();
    test_lf_setting_asl_pickers();
    test_lf_theme_font_pick();
    test_lf_drawer_pick();
    test_lf_profile_and_theme_lists();
}
