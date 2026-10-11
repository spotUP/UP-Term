/* edit_orig -- pi's edit for amiga-pi (tools.h tools_edit_orig): every
 * oldText matched against the original file, once, not overlapping, all or
 * none, on Edit's own loading, question and write (tools.c through
 * tools_int.h). Its own file so C:Claude, which never calls it, does not
 * link it (sources.mk CLAUDE_PI).
 * Portable C89 over sys.h, host-tested by amiga-pi (tests/test_pi_tools.c). */
#include <stdlib.h>
#include <string.h>
#include "tools_int.h"
#include "util.h"

#define EO_MAX 64

typedef struct eo_edit {
    char *olds, *news;
    long on, nn, at;
    int idx;
} eo_edit;

/* one {oldText, newText} object: 1 taken, 0 not one, -1 out of memory */
static int eo_take(jv ed, eo_edit *e, int *n)
{
    jv o, w;
    if (json_type(ed) != J_OBJ || !json_get(ed, "oldText", &o) || !json_get(ed, "newText", &w) ||
        json_type(o) != J_STR || json_type(w) != J_STR)
        return 0;
    if (*n >= EO_MAX)
        return 0;
    e[*n].olds = json_strdup(o, &e[*n].on);
    e[*n].news = json_strdup(w, &e[*n].nn);
    e[*n].idx = *n;
    (*n)++;
    return e[*n - 1].olds && e[*n - 1].news ? 1 : -1;
}

/* the edits of a call (pi's prepareEditArguments): 0, -1 invalid */
static int eo_collect(jv in, eo_edit *e, int *n)
{
    jv x, v;
    jit it;
    char *s = 0;
    long sn = 0;
    int ok = 1;
    if (json_get(in, "edits", &x)) {
        if (json_type(x) == J_STR) {
            /* some models send the array as a JSON string */
            s = json_strdup(x, &sn);
            if (!s || json_parse(s, sn, &x)) {
                free(s);
                return -1;
            }
        }
        if (json_type(x) == J_ARR) {
            json_iter(x, &it);
            while (ok && json_next(&it, 0, &v))
                ok = eo_take(v, e, n) == 1;
        } else
            ok = eo_take(x, e, n) == 1;     /* one edit object for the array */
        free(s);
        if (!ok)
            return -1;
    }
    /* the legacy form: oldText and newText beside path */
    if (json_get(in, "oldText", &x) && json_get(in, "newText", &v) && eo_take(in, e, n) != 1)
        return -1;
    return *n > 0 ? 0 : -1;
}

/* pi's error words (edit-diff.ts) into m */
static void eo_where(jw *m, const char *what, int i, int n)
{
    char num[16];
    if (n == 1) {
        jw_rawz(m, what);
        return;
    }
    jw_rawz(m, "edits[");
    cl_ltoa(i, num);
    jw_rawz(m, num);
    jw_rawz(m, "]");
}

void tools_edit_orig(cl_tools *t, const char *id, const char *raw, long rawn, jw *out)
{
    char full[512], what[300], num[16];
    char *arg = 0;
    eo_edit ed[EO_MAX];
    int n = 0, i, k, outside = 0, kind, bad = 0;
    long newn, pos;
    pedit e;
    jv in;
    jw m;
    t->cur = T_EDIT;
    t->brief[0] = 0;
    t->cur_in = raw ? raw : "";
    t->cur_inn = raw ? rawn : 0;
    memset(&e, 0, sizeof(e));
    memset(ed, 0, sizeof(ed));
    jw_init(&m);
    if (!raw || json_parse(raw, rawn, &in) || json_type(in) != J_OBJ) {
        tl_error(t, out, id, "the tool input was not valid JSON; send the call again", 0);
        return;
    }
    arg = tl_prop(in, "path", 0);
    if (!arg) {
        tl_error(t, out, id, "out of memory", 0);
        return;
    }
    if (!arg[0] || eo_collect(in, ed, &n)) {
        tl_error(t, out, id, !arg[0] ? "Edit tool input is invalid. path must be a string."
                                     : "Edit tool input is invalid. edits must contain at least one replacement.", 0);
        goto done;
    }
    if (tl_resolve(t, arg, full, sizeof(full), &outside)) {
        tl_error(t, out, id, "not a usable path (it climbs above a volume's root): ", arg);
        goto done;
    }
    tl_summary(what, sizeof(what), full, 0);
    if (t->show)
        t->show(t->u, tools_name(T_EDIT), what);
    kind = t->sys->kind(t->sys->u, full);
    if (kind != 1) {
        jw_rawz(&m, "Could not edit file: ");
        jw_rawz(&m, arg);
        jw_rawz(&m, kind == 2 ? ". Error code: EISDIR." : ". Error code: ENOENT.");
        tl_result(t, out, id, m.p ? m.p : "", m.n, 1);
        goto done;
    }
    if (tl_edit_load(t, out, id, full, &e))
        goto done;
    /* each oldText: not empty, found once in the original */
    for (i = 0; i < n && !bad; i++) {
        long c;
        if (!ed[i].on) {
            eo_where(&m, "oldText", i, n);
            jw_rawz(&m, n == 1 ? " must not be empty in " : ".oldText must not be empty in ");
            jw_rawz(&m, arg);
            jw_rawz(&m, ".");
            bad = 1;
            break;
        }
        c = tl_count_of(e.before, e.bn, ed[i].olds, ed[i].on, &ed[i].at);
        if (!c) {
            jw_rawz(&m, n == 1 ? "Could not find the exact text in " : "Could not find ");
            if (n > 1) {
                eo_where(&m, "", i, n);
                jw_rawz(&m, " in ");
            }
            jw_rawz(&m, arg);
            jw_rawz(&m, n == 1 ? ". The old text must match exactly including all whitespace and newlines."
                               : ". The oldText must match exactly including all whitespace and newlines.");
            bad = 1;
        } else if (c > 1) {
            cl_ltoa(c, num);
            jw_rawz(&m, "Found ");
            jw_rawz(&m, num);
            jw_rawz(&m, " occurrences of ");
            if (n == 1)
                jw_rawz(&m, "the text");
            else
                eo_where(&m, "", i, n);
            jw_rawz(&m, " in ");
            jw_rawz(&m, arg);
            jw_rawz(&m, n == 1 ? ". The text must be unique. Please provide more context to make it unique."
                               : ". Each oldText must be unique. Please provide more context to make it unique.");
            bad = 1;
        }
    }
    if (!bad) {
        /* in file order (insertion sort, n is small), none overlapping */
        for (i = 1; i < n; i++) {
            eo_edit x = ed[i];
            for (k = i; k > 0 && ed[k - 1].at > x.at; k--)
                ed[k] = ed[k - 1];
            ed[k] = x;
        }
        for (i = 1; i < n && !bad; i++)
            if (ed[i - 1].at + ed[i - 1].on > ed[i].at) {
                jw_rawz(&m, "edits[");
                cl_ltoa(ed[i - 1].idx, num);
                jw_rawz(&m, num);
                jw_rawz(&m, "] and edits[");
                cl_ltoa(ed[i].idx, num);
                jw_rawz(&m, num);
                jw_rawz(&m, "] overlap in ");
                jw_rawz(&m, arg);
                jw_rawz(&m, ". Merge them into one edit or target disjoint regions.");
                bad = 1;
            }
    }
    if (bad) {
        tl_result(t, out, id, m.p ? m.p : "out of memory", m.p ? m.n : 13, 1);
        goto done;
    }
    /* the new text: the original with every match replaced */
    newn = e.bn;
    for (i = 0; i < n; i++)
        newn += ed[i].nn - ed[i].on;
    free(e.after);
    e.after = (char *)malloc((size_t)newn + 1);
    if (!e.after) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    for (i = 0, pos = 0, e.an = 0; i < n; i++) {
        memcpy(e.after + e.an, e.before + pos, (size_t)(ed[i].at - pos));
        e.an += ed[i].at - pos;
        memcpy(e.after + e.an, ed[i].news, (size_t)ed[i].nn);
        e.an += ed[i].nn;
        pos = ed[i].at + ed[i].on;
    }
    memcpy(e.after + e.an, e.before + pos, (size_t)(e.bn - pos));
    e.an += e.bn - pos;
    e.after[e.an] = 0;
    e.first = ed[0].at;
    if (e.an == e.bn && !memcmp(e.after, e.before, (size_t)e.bn)) {
        jw_rawz(&m, "No changes made to ");
        jw_rawz(&m, arg);
        jw_rawz(&m, n == 1 ? ". The replacement produced identical content. This might indicate an issue with "
                             "special characters or the text not existing as expected."
                           : ". The replacements produced identical content.");
        tl_result(t, out, id, m.p ? m.p : "", m.n, 1);
        goto done;
    }
    if (perm_refused(&t->perm, T_EDIT)) {
        tl_gate(t, out, id, T_EDIT, what, outside, 0);
        goto done;
    }
    if (t->preview)
        t->preview(t->u, T_EDIT, full, e.before, e.bn, e.after, e.an);
    if (tl_gate(t, out, id, T_EDIT, what, outside, 0))
        goto done;
    {
        long rn = e.an;
        if (e.latin)
            rn = tl_utf8_to_latin1(e.after, rn);
        if (t->sys->write(t->sys->u, full, e.after, rn)) {
            tl_error(t, out, id, "cannot write the file: ", t->sys->err(t->sys->u));
            goto done;
        }
    }
    tl_read_note(t, full);
    jw_rawz(&m, "Successfully replaced ");
    cl_ltoa(n, num);
    jw_rawz(&m, num);
    jw_rawz(&m, " block(s) in ");
    jw_rawz(&m, arg);
    jw_rawz(&m, ".");
    tl_result(t, out, id, m.p ? m.p : "", m.n, 0);
done:
    for (i = 0; i < EO_MAX; i++) {
        free(ed[i].olds);
        free(ed[i].news);
    }
    jw_free(&m);
    tl_edit_free(&e);
    free(arg);
}

