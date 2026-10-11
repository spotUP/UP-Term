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
#include "unorm.h"
#include "json.h"

#define EO_MAX 64

typedef struct eo_edit {
    char *olds, *news;          /* LF only (pi's normalizeToLF) once collected */
    long on, nn, at, mlen;      /* at, mlen: the match in the text matched */
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

/* ---- pi's text forms (edit-diff.ts), on UTF-8 bytes where pi has UTF-16:
 * the same matches, other offsets ---- */

/* normalizeToLF: CRLF and a lone CR become LF (malloc'ed, terminated) */
static char *eo_to_lf(const char *s, long n, long *on)
{
    char *o = (char *)malloc((size_t)n + 1);
    long i, k = 0;
    if (!o)
        return 0;
    for (i = 0; i < n; i++) {
        if (s[i] == '\r') {
            o[k++] = '\n';
            if (i + 1 < n && s[i + 1] == '\n')
                i++;
        } else
            o[k++] = s[i];
    }
    o[k] = 0;
    *on = k;
    return o;
}

/* JavaScript's white space (String.prototype.trimEnd) */
static int eo_js_space(unsigned long c)
{
    return (c >= 9 && c <= 13) || c == 0x20 || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) ||
           c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000 || c == 0xFEFF;
}

static unsigned long eo_cp(const char *s, long n, long i, int *len)
{
    unsigned long c;
    if ((unsigned char)s[i] < 0x80) {
        *len = 1;
        return (unsigned char)s[i];
    }
    *len = json_utf8(s + i, n - i, &c);
    if (*len <= 0) {
        *len = 1;
        return 0xFFFD;
    }
    return c;
}

/* normalizeForFuzzyMatch: NFKC, the white space at each line's end
 * dropped, then smart quotes, dashes and special spaces as ASCII
 * (malloc'ed, terminated); 0 out of memory */
static char *eo_fuzzy(const char *s, long n, long *on)
{
    char *k, *o;
    long kn, i, a, keep, j;
    int len;
    if (un_nfkc(s, n, &k, &kn))
        return 0;
    o = (char *)malloc((size_t)kn + 1);
    if (!o) {
        free(k);
        return 0;
    }
    for (i = 0, j = 0; i <= kn;) {
        /* one line [a, i), its kept part [a, keep) */
        for (a = i, keep = i; i < kn && k[i] != '\n'; i += len)
            if (!eo_js_space(eo_cp(k, kn, i, &len)))
                keep = i + len;
        while (a < keep) {
            unsigned long c = eo_cp(k, kn, a, &len);
            if (c >= 0x2018 && c <= 0x201B)
                o[j++] = '\'';
            else if (c >= 0x201C && c <= 0x201F)
                o[j++] = '"';
            else if ((c >= 0x2010 && c <= 0x2015) || c == 0x2212)
                o[j++] = '-';
            else if (c == 0xA0 || (c >= 0x2002 && c <= 0x200A) || c == 0x202F || c == 0x205F || c == 0x3000)
                o[j++] = ' ';
            else {
                memcpy(o + j, k + a, (size_t)len);
                j += len;
            }
            a += len;
        }
        if (i < kn)
            o[j++] = '\n';
        i++;
    }
    o[j] = 0;
    *on = j;
    free(k);
    return o;
}

/* indexOf: the first place of nd in h, -1 none (an empty nd is at 0) */
static long eo_find(const char *h, long hn, const char *nd, long nn)
{
    long first;
    if (!nn)
        return 0;
    return tl_count_of(h, hn, nd, nn, &first) ? first : -1;
}

/* split(nd).length - 1: the occurrences of nd, left to right, not
 * overlapping; split("") cuts between UTF-16 units */
static long eo_count(const char *h, long hn, const char *nd, long nn)
{
    long first, i, u = 0;
    if (nn)
        return tl_count_of(h, hn, nd, nn, &first);
    for (i = 0; i < hn; i++)
        if (((unsigned char)h[i] & 0xC0) != 0x80)
            u += (unsigned char)h[i] >= 0xF0 ? 2 : 1;
    return u - 1;
}

typedef struct eo_span {
    long s, e;
} eo_span;

/* splitLinesWithEndings: each line with its LF, a last one without (malloc'ed) */
static eo_span *eo_lines(const char *s, long n, long *cnt)
{
    eo_span *l = (eo_span *)malloc(((size_t)n + 1) * sizeof(eo_span));
    long i, a = 0, k = 0;
    if (!l)
        return 0;
    for (i = 0; i < n; i++)
        if (s[i] == '\n') {
            l[k].s = a;
            l[k++].e = i + 1;
            a = i + 1;
        }
    if (a < n) {
        l[k].s = a;
        l[k++].e = n;
    }
    *cnt = k;
    return l;
}

/* the replacements (in place order) on base[from..to) into o */
static void eo_put(jw *o, const char *base, long from, long to, eo_edit *ed, int a, int b)
{
    int i;
    for (i = a; i < b; i++) {
        jw_raw(o, base + from, ed[i].at - from);
        jw_raw(o, ed[i].news, ed[i].nn);
        from = ed[i].at + ed[i].mlen;
    }
    jw_raw(o, base + from, to - from);
}

/* applyReplacementsPreservingUnchangedLines: the lines a replacement
 * touches are rewritten from base, every other line is orig's own;
 * 0, or 1 with pi's error in m */
static int eo_keep_lines(const char *orig, long on, const char *base, long bn, eo_edit *ed, int n, jw *o, jw *m)
{
    eo_span *ol, *bl;
    long oc = 0, bc = 0, oi = 0, gs, ge;
    int i = 0, rc = 0;
    ol = eo_lines(orig, on, &oc);
    bl = eo_lines(base, bn, &bc);
    if (!ol || !bl) {
        jw_rawz(m, "out of memory");
        rc = 1;
    } else if (oc != bc) {
        jw_rawz(m, "Cannot preserve unchanged lines because the base content has a different line count.");
        rc = 1;
    }
    while (!rc && i < n) {
        int a = i;
        /* a group: the lines of ed[i], widened by the replacements that start inside them */
        for (gs = 0; gs < bc && !(ed[i].at >= bl[gs].s && ed[i].at < bl[gs].e); gs++)
            ;
        ge = gs;
        while (ge < bc && bl[ge].e < ed[i].at + ed[i].mlen)
            ge++;
        if (gs >= bc || ge >= bc) {
            jw_rawz(m, "Replacement range is outside the base content.");
            rc = 1;
            break;
        }
        ge++;
        for (i++; i < n; i++) {
            long s2, e2;
            for (s2 = 0; s2 < bc && !(ed[i].at >= bl[s2].s && ed[i].at < bl[s2].e); s2++)
                ;
            e2 = s2;
            while (e2 < bc && bl[e2].e < ed[i].at + ed[i].mlen)
                e2++;
            if (s2 >= bc || e2 >= bc) {
                jw_rawz(m, "Replacement range is outside the base content.");
                rc = 1;
                break;
            }
            if (s2 >= ge)
                break;
            if (e2 + 1 > ge)
                ge = e2 + 1;
        }
        if (rc)
            break;
        if (gs > oi)
            jw_raw(o, orig + ol[oi].s, ol[gs - 1].e - ol[oi].s);
        eo_put(o, base, bl[gs].s, bl[ge - 1].e, ed, a, i);
        oi = ge;
    }
    if (!rc && oi < oc)
        jw_raw(o, orig + ol[oi].s, on - ol[oi].s);
    free(ol);
    free(bl);
    return rc;
}

/* applyEditsToNormalizedContent on norm (LF only, no BOM): the new text in
 * o; 0, or 1 with pi's error in m */
static int eo_apply(const char *norm, long nn, eo_edit *ed, int n, const char *path, jw *o, jw *m)
{
    char num[16], *fzn = 0, *fo[EO_MAX];
    long fzl = 0, fol[EO_MAX];
    const char *base;
    long bn;
    int i, k, used = 0, rc = 1;
    memset(fo, 0, sizeof(fo));
    for (i = 0; i < n; i++)
        if (!ed[i].on) {
            eo_where(m, "oldText", i, n);
            jw_rawz(m, n == 1 ? " must not be empty in " : ".oldText must not be empty in ");
            jw_rawz(m, path);
            jw_rawz(m, ".");
            return 1;
        }
    fzn = eo_fuzzy(norm, nn, &fzl);
    for (i = 0; fzn && i < n; i++)
        if (!(fo[i] = eo_fuzzy(ed[i].olds, ed[i].on, &fol[i])))
            break;
    if (!fzn || i < n) {
        jw_rawz(m, "out of memory");
        goto done;
    }
    /* fuzzy space when any edit only matches there */
    for (i = 0; i < n; i++)
        if (eo_find(norm, nn, ed[i].olds, ed[i].on) < 0 && eo_find(fzn, fzl, fo[i], fol[i]) >= 0)
            used = 1;
    base = used ? fzn : norm;
    bn = used ? fzl : nn;
    for (i = 0; i < n; i++) {
        long c;
        ed[i].at = eo_find(base, bn, ed[i].olds, ed[i].on);
        ed[i].mlen = ed[i].on;
        if (ed[i].at < 0) {
            ed[i].at = eo_find(fzn, fzl, fo[i], fol[i]);
            ed[i].mlen = fol[i];
        }
        if (ed[i].at < 0) {
            jw_rawz(m, n == 1 ? "Could not find the exact text in " : "Could not find ");
            if (n > 1) {
                eo_where(m, "", i, n);
                jw_rawz(m, " in ");
            }
            jw_rawz(m, path);
            jw_rawz(m, n == 1 ? ". The old text must match exactly including all whitespace and newlines."
                              : ". The oldText must match exactly including all whitespace and newlines.");
            goto done;
        }
        /* pi counts in the fuzzy text, an exact match too */
        c = eo_count(fzn, fzl, fo[i], fol[i]);
        if (c > 1) {
            cl_ltoa(c, num);
            jw_rawz(m, "Found ");
            jw_rawz(m, num);
            jw_rawz(m, " occurrences of ");
            if (n == 1)
                jw_rawz(m, "the text");
            else
                eo_where(m, "", i, n);
            jw_rawz(m, " in ");
            jw_rawz(m, path);
            jw_rawz(m, n == 1 ? ". The text must be unique. Please provide more context to make it unique."
                              : ". Each oldText must be unique. Please provide more context to make it unique.");
            goto done;
        }
    }
    /* in place order (stable insertion sort, n is small), none overlapping */
    for (i = 1; i < n; i++) {
        eo_edit x = ed[i];
        for (k = i; k > 0 && ed[k - 1].at > x.at; k--)
            ed[k] = ed[k - 1];
        ed[k] = x;
    }
    for (i = 1; i < n; i++)
        if (ed[i - 1].at + ed[i - 1].mlen > ed[i].at) {
            jw_rawz(m, "edits[");
            cl_ltoa(ed[i - 1].idx, num);
            jw_rawz(m, num);
            jw_rawz(m, "] and edits[");
            cl_ltoa(ed[i].idx, num);
            jw_rawz(m, num);
            jw_rawz(m, "] overlap in ");
            jw_rawz(m, path);
            jw_rawz(m, ". Merge them into one edit or target disjoint regions.");
            goto done;
        }
    if (used) {
        if (eo_keep_lines(norm, nn, base, bn, ed, n, o, m))
            goto done;
    } else
        eo_put(o, base, 0, bn, ed, 0, n);
    if (o->oom) {
        jw_rawz(m, "out of memory");
        goto done;
    }
    if (o->n == nn && !memcmp(o->p ? o->p : "", norm, (size_t)nn)) {
        jw_rawz(m, "No changes made to ");
        jw_rawz(m, path);
        jw_rawz(m, n == 1 ? ". The replacement produced identical content. This might indicate an issue with "
                            "special characters or the text not existing as expected."
                          : ". The replacements produced identical content.");
        goto done;
    }
    rc = 0;
done:
    free(fzn);
    for (i = 0; i < n; i++)
        free(fo[i]);
    return rc;
}

void tools_edit_orig(cl_tools *t, const char *id, const char *raw, long rawn, jw *out)
{
    char full[512], what[300], num[16];
    char *arg = 0;
    eo_edit ed[EO_MAX];
    int n = 0, i, outside = 0, kind, bad = 0;
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
    /* pi: the BOM apart, the line ends as LF, the edits matched on that;
     * then the BOM and the file's line end (its first one's) back */
    {
        int bom = e.bn >= 3 && !memcmp(e.before, "\xEF\xBB\xBF", 3);
        const char *c = e.before + (bom ? 3 : 0), *lf = (const char *)memchr(c, '\n', (size_t)(e.bn - (bom ? 3 : 0)));
        int crlf = lf && lf > c && lf[-1] == '\r';
        long nn = 0;
        char *norm = eo_to_lf(c, e.bn - (bom ? 3 : 0), &nn);
        jw nw;
        jw_init(&nw);
        for (i = 0; norm && i < n; i++) {
            char *x;
            long xn;
            if (!(x = eo_to_lf(ed[i].olds, ed[i].on, &xn)))
                break;
            free(ed[i].olds);
            ed[i].olds = x;
            ed[i].on = xn;
            if (!(x = eo_to_lf(ed[i].news, ed[i].nn, &xn)))
                break;
            free(ed[i].news);
            ed[i].news = x;
            ed[i].nn = xn;
        }
        if (!norm || i < n) {
            free(norm);
            tl_error(t, out, id, "out of memory", 0);
            goto done;
        }
        bad = eo_apply(norm, nn, ed, n, arg, &nw, &m);
        free(norm);
        if (bad) {
            jw_free(&nw);
            tl_result(t, out, id, m.p ? m.p : "out of memory", m.p ? m.n : 13, 1);
            goto done;
        }
        free(e.after);
        e.after = (char *)malloc((size_t)(nw.n * 2 + 4));
        if (!e.after) {
            jw_free(&nw);
            tl_error(t, out, id, "out of memory", 0);
            goto done;
        }
        e.an = 0;
        if (bom) {
            memcpy(e.after, "\xEF\xBB\xBF", 3);
            e.an = 3;
        }
        {
            long j;
            for (j = 0; j < nw.n; j++) {
                if (crlf && nw.p[j] == '\n')
                    e.after[e.an++] = '\r';
                e.after[e.an++] = nw.p[j];
            }
        }
        e.after[e.an] = 0;
        e.first = n ? ed[0].at : 0;
        jw_free(&nw);
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

