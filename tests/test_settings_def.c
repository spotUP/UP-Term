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
            printf("pg_items id %d (%s) claimed %d times\n", p->id, p->label ? p->label : "-", claims(p->id));
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

void suite_settings_def(void)
{
    test_settings_gadgets();
    test_settings_table();
    test_settings_amarchy();
    test_settings_defaults();
    test_settings_roundtrip();
}
