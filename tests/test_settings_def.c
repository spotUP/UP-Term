/* The settings table (prefs/settings_def.c) against the two things it stands for: the controls of the Prefs
 * window (pg_items) and the stores (prefs_fields through the file, am_prefs through amconf). Every count is
 * taken from walking the tables, none is written here. */
#include <stdio.h>
#include <string.h>
#include "../prefs/settings_def.h"
#include "../prefs/prefs_core.h"
#include "../amarchy/amconf_core.h"
#include "harness.h"

static int pg_item_at(int id)
{
    int i, found = 0;
    for (i = 0; i < pg_nitems; i++)
        if (pg_items[i].id == id)
            found++;
    return found;
}

/* how many controls of rows claim the pg_items id */
static int claims(int id)
{
    int i, n, c = 0;
    for (i = 0; i < sd_nsettings; i++) {
        const sd_setting *s = &sd_settings[i];
        for (n = 0; n < s->count; n++) {
            c += id == s->gadget + n;
            c += s->aux && id == s->aux + n;
            c += s->aux2 && id == s->aux2 + n;
        }
    }
    return c;
}

static void test_settings_gadgets(void)
{
    int i, n, checked = 0, ctl = 0, claimed = 0;
    int per_page[PG_NPAGES];

    /* every control that holds a value is claimed by exactly one row */
    for (i = 0; i < pg_nitems; i++) {
        const pg_item *p = &pg_items[i];
        if (p->page == PG_COMMON || p->kind == PGK_BUTTON)
            continue;
        ctl++;
        if (claims(p->id) != 1)
            printf("pg_items id %d (%s) claimed %d times\n", p->id, pg_label(p) ? pg_label(p) : "-", claims(p->id));
        CHECK_INT(claims(p->id), 1);
    }
    /* every control a row names is one pg_items entry, on the row's page */
    for (i = 0; i < sd_nsettings; i++) {
        const sd_setting *s = &sd_settings[i];
        CHECK_INT(s->id, i);
        for (n = 0; n < s->count; n++) {
            int ids[3];
            int k;
            ids[0] = s->gadget + n;
            ids[1] = s->aux ? s->aux + n : 0;
            ids[2] = s->aux2 ? s->aux2 + n : 0;
            for (k = 0; k < 3; k++) {
                int j;
                if (!ids[k])
                    continue;
                claimed++;
                CHECK_INT(pg_item_at(ids[k]), 1);
                for (j = 0; j < pg_nitems; j++)
                    if (pg_items[j].id == ids[k]) {
                        CHECK_INT(pg_items[j].page, s->page);
                        CHECK(pg_items[j].kind != PGK_BUTTON || s->kind == SD_THEME);
                    }
            }
        }
        checked++;
    }
    CHECK_INT(checked, sd_nsettings);
    /* the controls the rows claim are the controls that hold a value, plus the Theme button */
    CHECK_INT(claimed, ctl + 1);
    /* each page has rows */
    memset(per_page, 0, sizeof(per_page));
    for (i = 0; i < sd_nsettings; i++)
        per_page[sd_settings[i].page]++;
    for (i = 0; i < PG_NPAGES; i++)
        CHECK(per_page[i] > 0);
    /* sd_find_gadget agrees with the walk */
    for (i = 0; i < pg_nitems; i++) {
        const pg_item *p = &pg_items[i];
        if (p->page == PG_COMMON || p->kind == PGK_BUTTON)
            continue;
        CHECK(sd_find_gadget(p->id) != 0);
    }
    CHECK(sd_find_gadget(PG_SAVE) == 0);
}

static void test_settings_table(void)
{
    int i, j;
    for (i = 0; i < sd_nsettings; i++) {
        const sd_setting *s = &sd_settings[i];
        CHECK(s->label && s->help && *s->label && *s->help);
        CHECK(strchr(s->label, '_') != 0 || s->kind == SD_PALETTE);
        CHECK_INT(s->count, s->kind == SD_PALETTE ? 16 : 1);
        if (s->kind == SD_CHECK)
            CHECK_INT(s->nvalues, 2);
        if (s->kind == SD_CYCLE)
            CHECK(s->nvalues >= 2);
        if (s->key)
            CHECK(*s->key != 0);
        else
            CHECK(s->ftype == SDF_NONE);
        /* an id, and a key unless the rows share one (kingcon-mode) */
        for (j = 0; j < i; j++) {
            const sd_setting *t = &sd_settings[j];
            if (s->key && t->key && s->store == t->store && !strcmp(s->key, t->key))
                CHECK(s->ftype >= SDF_KC_MATCH && t->ftype >= SDF_KC_MATCH);
        }
        /* a default a cycle can show */
        if (s->nvalues) {
            int d = sd_def_index(s);
            CHECK(d >= 0 && d < s->nvalues);
            CHECK_STR(s->values[d].file, s->def);
        }
    }
}

/* the Amarchy rows are the am_keys[] they name */
static void test_settings_amarchy(void)
{
    int i, k, rows = 0, seen[AM_NKEYS];
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < sd_nsettings; i++) {
        const sd_setting *s = &sd_settings[i];
        if (s->store != SD_AMARCHY)
            continue;
        rows++;
        CHECK_INT(s->ftype, SDF_AMSTR);
        CHECK(s->off >= offsetof(am_prefs, val));
        k = (int)((s->off - offsetof(am_prefs, val)) / UC_MAX_VALUE);
        CHECK(k >= 0 && k < AM_NKEYS);
        CHECK_INT((s->off - offsetof(am_prefs, val)) % UC_MAX_VALUE, 0);
        CHECK_INT(s->size, UC_MAX_VALUE);
        if (k >= 0 && k < AM_NKEYS) {
            seen[k]++;
            CHECK_STR(s->key, am_keys[k].name);
            CHECK_STR(s->def, am_keys[k].def);
        }
    }
    /* each am_keys entry has one row, except the two with no gadget */
    for (k = 0; k < AM_NKEYS; k++)
        CHECK(seen[k] == 1 || (seen[k] == 0 && (k == AM_SKIP || k == AM_VOLUMES)));
    CHECK(rows > 0);
}

/* the defaults the table states are the defaults the parser gives */
static void test_settings_defaults(void)
{
    static upconf conf;
    prefs_fields f;
    am_prefs a;
    int i;
    static const char none[] = "";
    CHECK_INT(prefs_load(&conf, none, 0, UC_MAX_FILE), PREFS_LOAD_NONE);
    prefs_from_conf(&f, &conf, "default");
    amconf_defaults(&a);
    for (i = 0; i < sd_nsettings; i++) {
        const sd_setting *s = &sd_settings[i];
        const void *st = s->store == SD_AMARCHY ? (const void *)&a : (const void *)&f;
        if (s->nvalues) {
            if (sd_index(s, st) != sd_def_index(s))
                printf("default of %s: index %d, table says %d\n", s->key, sd_index(s, st), sd_def_index(s));
            CHECK_INT(sd_index(s, st), sd_def_index(s));
        } else if (s->ftype == SDF_STR || s->ftype == SDF_AMSTR) {
            CHECK_STR(sd_string((sd_setting *)s, (void *)st, 0), s->def);
        }
    }
}

/* every value of every enumerated row survives the file */
static void test_settings_roundtrip(void)
{
    static char buf[UC_MAX_FILE + 1];
    static upconf conf, work;
    prefs_fields f, g;
    am_prefs a, b;
    int i, v;

    CHECK_INT(prefs_load(&conf, "", 0, UC_MAX_FILE), PREFS_LOAD_NONE);
    for (i = 0; i < sd_nsettings; i++) {
        const sd_setting *s = &sd_settings[i];
        if (!s->nvalues)
            continue;
        for (v = 0; v < s->nvalues; v++) {
            long len;
            if (s->store == SD_AMARCHY) {
                amconf_defaults(&a);
                sd_set_index(s, &a, v);
                CHECK_INT(sd_index(s, &a), v);
                len = amconf_serialize(&a, buf, sizeof(buf));
                CHECK(len > 0);
                CHECK(amconf_parse(buf, len, &b) >= 0);
                if (sd_index(s, &b) != v)
                    printf("%s value %d came back %d\n", s->key, v, sd_index(s, &b));
                CHECK_INT(sd_index(s, &b), v);
                continue;
            }
            prefs_from_conf(&f, &conf, "default");
            sd_set_index(s, &f, v);
            CHECK_INT(sd_index(s, &f), v);
            CHECK_INT(prefs_validate(&f), 0);
            len = prefs_stage(&work, &conf, "default", &f, buf, sizeof(buf));
            CHECK(len > 0);
            if (len > 0) {
                static upconf got;
                CHECK_INT(prefs_load(&got, buf, len, UC_MAX_FILE), PREFS_LOAD_OK);
                prefs_from_conf(&g, &got, "default");
                if (sd_index(s, &g) != v)
                    printf("%s value %d came back %d\n", s->key, v, sd_index(s, &g));
                CHECK_INT(sd_index(s, &g), v);
            }
        }
    }
}

/* A garbage value ("bold-bright = zzz") is the row's default for every reader of the file: prefs_from_conf and
 * the handler's window setup (sd_conf_key_index) give the same index, the default's. The handler reads a switch
 * through sd_conf_key_index alone; bold-bright is on by no value, so a parse of its own read zzz as off. */
static void test_settings_garbage_value(void)
{
    static upconf conf;
    prefs_fields f;
    char text[128];
    int i, bold_on = -1;
    for (i = 0; i < sd_nsettings; i++) {
        const sd_setting *s = &sd_settings[i];
        if (s->store != SD_UPTERM || !s->key || s->ftype != SDF_INT || !s->nvalues)
            continue;
        strcpy(text, "[profile default]\n");
        strcat(text, s->key);
        strcat(text, " = zzz\n");
        CHECK_INT(prefs_load(&conf, text, (long)strlen(text), UC_MAX_FILE), PREFS_LOAD_OK);
        prefs_from_conf(&f, &conf, "default");
        if (sd_conf_key_index(s->key, &conf, "default") != sd_def_index(s))
            printf("garbage %s: handler path %d, default %d\n", s->key,
                   sd_conf_key_index(s->key, &conf, "default"), sd_def_index(s));
        CHECK_INT(sd_conf_key_index(s->key, &conf, "default"), sd_def_index(s));
        CHECK_INT(sd_conf_key_index(s->key, &conf, "default"), sd_index(s, &f));
    }
    /* the named case: default on, garbage reads on */
    strcpy(text, "[profile default]\nbold-bright = zzz\n");
    CHECK_INT(prefs_load(&conf, text, (long)strlen(text), UC_MAX_FILE), PREFS_LOAD_OK);
    bold_on = sd_conf_key_index("bold-bright", &conf, "default");
    prefs_from_conf(&f, &conf, "default");
    CHECK_INT(bold_on, 1);
    CHECK_INT(sd_index(sd_find_key(SD_UPTERM, "bold-bright"), &f), 1);
    /* and a real value still moves it */
    strcpy(text, "[profile default]\nbold-bright = off\n");
    CHECK_INT(prefs_load(&conf, text, (long)strlen(text), UC_MAX_FILE), PREFS_LOAD_OK);
    CHECK_INT(sd_conf_key_index("bold-bright", &conf, "default"), 0);
}

/* M2: sd_row_label gives "Label: Value" for every kind; the label has no Prefs "_"; a value too wide for the
 * buffer is cut and the label is not. */
static void label_is(int id, const void *store, const char *want)
{
    char buf[160];
    sd_row_label(&sd_settings[id], store, buf, (int)sizeof buf);
    if (strcmp(buf, want))
        printf("row %d: got \"%s\", want \"%s\"\n", id, buf, want);
    CHECK(!strcmp(buf, want));
}

static void test_settings_row_label(void)
{
    prefs_fields f;
    am_prefs a;
    char buf[40];
    int i, seen[16], len;
    prefs_defaults(&f);
    amconf_defaults(&a);
    memset(seen, 0, sizeof seen);
    /* every row of the table gives a label: its plain label, a colon, a value */
    for (i = 0; i < sd_nsettings; i++) {
        const sd_setting *s = &sd_settings[i];
        char plain[80], want[200], got[200];
        sd_label_plain(s, plain, sizeof plain);
        CHECK(strchr(plain, '_') == 0);
        sd_row_label(s, s->store == SD_AMARCHY ? (void *)&a : (void *)&f, got, sizeof got);
        strcpy(want, plain);
        strcat(want, ": ");
        CHECK(!strncmp(got, want, strlen(want)));
        CHECK(got[strlen(want)] != 0);
        seen[s->kind]++;
    }
    for (i = 0; i <= SD_PROFILE; i++)
        CHECK(seen[i] > 0);                 /* the walk covered every kind */
    /* one value per kind */
    f.blink = 1;
    label_is(SDI_BLINK, &f, "Cursor blinks: On");
    f.blink = 0;
    label_is(SDI_BLINK, &f, "Cursor blinks: Off");
    f.cursor = 1;
    label_is(SDI_CURSOR, &f, "Cursor: Underline");
    strcpy(f.sb, "5000");
    label_is(SDI_SCROLLBACK, &f, "Scrollback: 5000");
    strcpy(f.term, "xterm");
    label_is(SDI_TERM, &f, "Terminal type: xterm");
    f.term[0] = 0;
    label_is(SDI_TERM, &f, "Terminal type: (empty)");
    strcpy(f.fg, "A0B0C0");
    label_is(SDI_FG, &f, "Text colour: #A0B0C0");
    label_is(SDI_BG, &f, "Background: default");
    strcpy(f.font, "topaz.font:8");
    label_is(SDI_FONT, &f, "Font: topaz.font:8");
    strcpy(f.screenmode, "0x00029000");
    label_is(SDI_SCREEN_MODE, &f, "Screen mode: 0x00029000");
    label_is(SDI_PALETTE, &f, "Palette: ...");
    label_is(SDI_THEME, &f, "Theme...: ...");
    label_is(SDI_PROFILE, &f, "Profile: ...");
    strcpy(a.val[AM_GTHEME], "nord");
    label_is(SDI_AM_GTHEME, &a, "Menu theme: nord");
    /* too wide: the value is cut, the label stays whole, the buffer is never overrun */
    strcpy(f.font, "a-very-long-font-name-indeed.font:12");
    len = sd_row_label(&sd_settings[SDI_FONT], &f, buf, 16);
    CHECK_INT(len, 15);
    CHECK(!strcmp(buf, "Font: a-very-.."));
    len = sd_row_label(&sd_settings[SDI_FALLBACK], &f, buf, 12);    /* label alone does not fit: kept whole as far as the buffer goes */
    CHECK(!strncmp(buf, "Fallback fon", 11) || !strncmp(buf, "Fallback fo", 11));
    CHECK_INT((int)strlen(buf) < 12, 1);
}

/* The screen mode row shows the display database's name, the hex id only when there is no name. */
static int fake_mode_name(unsigned long id, char *out, int cap)
{
    if (id != 0x29000UL)
        return 0;
    snprintf(out, (size_t)cap, "%s", "PAL:High Res");
    return 1;
}

static void test_settings_mode_name(void)
{
    prefs_fields f;
    memset(&f, 0, sizeof f);
    strcpy(f.screenmode, "0x00029000");
    label_is(SDI_SCREEN_MODE, &f, "Screen mode: 0x00029000");      /* no lookup registered */
    sd_set_mode_namer(fake_mode_name);
    label_is(SDI_SCREEN_MODE, &f, "Screen mode: PAL:High Res");
    strcpy(f.screenmode, "0x00011000");
    label_is(SDI_SCREEN_MODE, &f, "Screen mode: 0x00011000");      /* the lookup knows no such mode: hex stays */
    f.screenmode[0] = 0;
    label_is(SDI_SCREEN_MODE, &f, "Screen mode: default");
    sd_set_mode_namer(0);
}

static void test_settings_text_check(void)
{
    const sd_setting *sb = &sd_settings[SDI_SCROLLBACK], *lk = &sd_settings[SDI_LINK];
    char big[UC_MAX_VALUE + 8];
    CHECK_INT(sd_text_check(sb, "99999"), SD_TEXT_OK);
    CHECK_INT(sd_text_check(sb, "0"), SD_TEXT_OK);
    CHECK_INT(sd_text_check(sb, ""), SD_TEXT_OK);
    CHECK_INT(sd_text_check(sb, "100000"), SD_TEXT_RANGE);
    CHECK_INT(sd_text_check(sb, "99999999999999999999"), SD_TEXT_RANGE);
    CHECK_INT(sd_text_check(sb, "-1"), SD_TEXT_NAN);
    CHECK_INT(sd_text_check(sb, "1 2"), SD_TEXT_NAN);
    CHECK_INT(sd_text_check(lk, "run %s"), SD_TEXT_OK);
    CHECK_INT(sd_text_check(lk, "a\nb"), SD_TEXT_CTRL);
    memset(big, 'x', sizeof big);
    big[lk->size] = 0;
    CHECK_INT(sd_text_check(lk, big), SD_TEXT_LONG);
    big[lk->size - 1] = 0;
    CHECK_INT(sd_text_check(lk, big), SD_TEXT_OK);
    CHECK_INT(sd_text_check(&sd_settings[SDI_BLINK], "1"), SD_TEXT_KIND);
}

void suite_settings_def(void)
{
    test_settings_row_label();
    test_settings_mode_name();
    test_settings_text_check();
    test_settings_garbage_value();
    test_settings_gadgets();
    test_settings_table();
    test_settings_amarchy();
    test_settings_defaults();
    test_settings_roundtrip();
}
