/* prefs_merge.c: see prefs_merge.h. Portable C89. */
#include <string.h>

#include "prefs_merge.h"
#include "settings_def.h"
#include "../amarchy/amconf_core.h"

/* the stores a row can live in: the UP-Term fields, or Amarchy's own */
typedef struct stores {
    void *up;
    void *am;
} stores;

static void *store_of(const sd_setting *s, const stores *st)
{
    return s->store == SD_AMARCHY ? st->am : st->up;
}

/* every row that holds a value in a store; the profile and theme rows hold none */
static int has_value(const sd_setting *s)
{
    return s->ftype != SDF_NONE;
}

static int row_differs(const sd_setting *s, const stores *a, const stores *b)
{
    const char *pa = (const char *)store_of(s, a) + s->off;
    const char *pb = (const char *)store_of(s, b) + s->off;
    int n;
    switch (s->ftype) {
    case SDF_INT:
        if (*(const int *)pa != *(const int *)pb)
            return 1;
        break;
    case SDF_STR:
    case SDF_AMSTR:
        for (n = 0; n < s->count; n++)
            if (strcmp(pa + (size_t)n * (size_t)s->size, pb + (size_t)n * (size_t)s->size))
                return 1;
        break;
    default: /* the KingCON letters: the value this row reads out of them */
        if (sd_index(s, store_of(s, a)) != sd_index(s, store_of(s, b)))
            return 1;
        break;
    }
    if (s->off2 && strcmp((const char *)store_of(s, a) + s->off2, (const char *)store_of(s, b) + s->off2))
        return 1;
    return 0;
}

static void row_copy(const sd_setting *s, const stores *dst, const stores *src)
{
    char *pd = (char *)store_of(s, dst) + s->off;
    const char *ps = (const char *)store_of(s, src) + s->off;
    size_t len;
    switch (s->ftype) {
    case SDF_INT:
        *(int *)pd = *(const int *)ps;
        break;
    case SDF_STR:
    case SDF_AMSTR:
        len = (size_t)s->count * (size_t)s->size;
        memcpy(pd, ps, len);
        break;
    default: /* the letters are shared by three rows of one page: all take the same side */
        memcpy(pd, ps, (size_t)s->size);
        break;
    }
    if (s->off2)
        memcpy((char *)store_of(s, dst) + s->off2, (const char *)store_of(s, src) + s->off2,
               sizeof(((prefs_fields *)0)->screendepth));
}

static void make_am(am_prefs *p, const prefs_fields *f)
{
    amconf_defaults(p);
    amconf_from_fields(p, f);
}

unsigned prefs_pages_edited(const prefs_fields *cur, const prefs_fields *orig)
{
    am_prefs ac, ao;
    stores sc, so;
    unsigned mask = 0;
    int i;
    make_am(&ac, cur);
    make_am(&ao, orig);
    sc.up = (void *)cur; sc.am = &ac;
    so.up = (void *)orig; so.am = &ao;
    for (i = 0; i < sd_nsettings; i++)
        if (has_value(&sd_settings[i]) && row_differs(&sd_settings[i], &sc, &so))
            mask |= 1u << sd_settings[i].page;
    return mask;
}

unsigned prefs_merge_external(const prefs_fields *cur, const prefs_fields *orig, const prefs_fields *file,
                              prefs_fields *out, prefs_fields *new_orig)
{
    am_prefs ac, ao, af, aout, anew;
    stores sc, so, sf, sout, snew;
    unsigned edited = prefs_pages_edited(cur, orig);
    unsigned changed = 0;
    int i;
    make_am(&ac, cur);
    make_am(&ao, orig);
    make_am(&af, file);
    aout = ac;
    anew = ao;
    *out = *cur;
    *new_orig = *orig;
    sc.up = (void *)cur; sc.am = &ac;
    so.up = (void *)orig; so.am = &ao;
    sf.up = (void *)file; sf.am = &af;
    sout.up = out; sout.am = &aout;
    snew.up = new_orig; snew.am = &anew;
    for (i = 0; i < sd_nsettings; i++) {
        const sd_setting *s = &sd_settings[i];
        if (!has_value(s) || (edited & (1u << s->page)))
            continue;
        if (row_differs(s, &sc, &sf))
            changed |= 1u << s->page;
        row_copy(s, &sout, &sf);
        row_copy(s, &snew, &sf);
    }
    amconf_to_fields(&aout, out);
    amconf_to_fields(&anew, new_orig);
    return changed;
}
