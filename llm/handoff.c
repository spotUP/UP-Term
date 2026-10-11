/* see handoff.h */
#include <stdlib.h>
#include <string.h>
#include "handoff.h"

#define M32 0xffffffffUL

static const char *const IMG_USER = "(image omitted: model does not support images)";
static const char *const IMG_TOOL = "(tool image omitted: model does not support images)";

/* ---- shortHash ---- */

static unsigned long imul(unsigned long a, unsigned long b)
{
    return ((a & M32) * (b & M32)) & M32;
}

static void b36(unsigned long v, char *out)
{
    char t[16];
    int n = 0, i;
    v &= M32;
    if (!v)
        t[n++] = '0';
    while (v) {
        t[n++] = "0123456789abcdefghijklmnopqrstuvwxyz"[v % 36];
        v /= 36;
    }
    for (i = 0; i < n; i++)
        out[i] = t[n - 1 - i];
    out[n] = 0;
}

void llm_short_hash(const char *s, char *out, long cap)
{
    unsigned long h1 = 0xdeadbeefUL, h2 = 0x41c6ce57UL;
    char a[16], b[16];
    long i;
    for (i = 0; s[i]; i++) {
        unsigned long ch = (unsigned char)s[i];
        h1 = imul(h1 ^ ch, 2654435761UL);
        h2 = imul(h2 ^ ch, 1597334677UL);
    }
    h1 = (imul(h1 ^ (h1 >> 16), 2246822507UL) ^ imul(h2 ^ (h2 >> 13), 3266489909UL)) & M32;
    h2 = (imul(h2 ^ (h2 >> 16), 2246822507UL) ^ imul(h1 ^ (h1 >> 13), 3266489909UL)) & M32;
    b36(h2, a);
    b36(h1, b);
    if ((long)(strlen(a) + strlen(b)) >= cap) {
        out[0] = 0;
        return;
    }
    strcpy(out, a);
    strcat(out, b);
}

/* ---- tool-call ids ---- */

static void clean_id(char *s)
{
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_' || *s == '-'))
            *s = '_';
}

/* the id the target api takes: 1 when it differs from id (written to out) */
static int norm_id(const llm_model *to, const char *id, char *out, long cap)
{
    long l = (long)strlen(id);
    if (l >= cap)
        return 0;
    strcpy(out, id);
    if (!strcmp(to->api, "anthropic-messages")) {
        clean_id(out);
        if (strlen(out) > 64)
            out[64] = 0;
    } else if (!strcmp(to->api, "openai-completions")) {
        const char *bar = strchr(id, '|');
        if (bar) {
            char call[512], item[512], comb[1100], h[24];
            long cl = (long)(bar - id), il = l - cl - 1, k;
            if (cl >= (long)sizeof(call) || il >= (long)sizeof(item))
                return 0;
            memcpy(call, id, (size_t)cl);
            call[cl] = 0;
            memcpy(item, bar + 1, (size_t)il);
            item[il] = 0;
            clean_id(call);
            clean_id(item);
            strcpy(comb, call);
            if (il > 0) {
                strcat(comb, "_");
                strcat(comb, item);
            }
            if (strlen(comb) <= 40) {
                strcpy(out, comb);
            } else {
                llm_short_hash(id, h, sizeof(h));
                h[8] = 0;
                k = 40 - (long)strlen(h) - 1;
                if (k < 1)
                    k = 1;
                if (k > (long)strlen(call))
                    k = (long)strlen(call);
                memcpy(out, call, (size_t)k);
                out[k] = 0;
                strcat(out, "_");
                strcat(out, h);
            }
        } else if (!strcmp(to->provider, "openai") && strlen(out) > 40)
            out[40] = 0;
    }
    return strcmp(out, id) != 0;
}

/* ---- the id map: old id -> new id, in order of appearance ---- */

typedef struct idmap {
    char **from, **to;
    int n, cap;
} idmap;

static int map_add(idmap *m, const char *from, const char *to)
{
    if (m->n == m->cap) {
        int nc = m->cap ? m->cap * 2 : 16;
        char **a = (char **)realloc(m->from, (size_t)nc * sizeof(*a));
        char **b;
        if (!a)
            return -1;
        m->from = a;
        b = (char **)realloc(m->to, (size_t)nc * sizeof(*b));
        if (!b)
            return -1;
        m->to = b;
        m->cap = nc;
    }
    m->from[m->n] = (char *)malloc(strlen(from) + 1);
    m->to[m->n] = (char *)malloc(strlen(to) + 1);
    if (!m->from[m->n] || !m->to[m->n])
        return -1;
    strcpy(m->from[m->n], from);
    strcpy(m->to[m->n], to);
    m->n++;
    return 0;
}

static const char *map_get(const idmap *m, const char *from)
{
    int i;
    for (i = m->n - 1; i >= 0; i--)
        if (!strcmp(m->from[i], from))
            return m->to[i];
    return 0;
}

static void map_free(idmap *m)
{
    int i;
    for (i = 0; i < m->n; i++) {
        free(m->from[i]);
        free(m->to[i]);
    }
    free(m->from);
    free(m->to);
}

/* ---- blocks ---- */

static int blank(jv s)
{
    char *d;
    long n, i;
    int r = 1;
    d = json_strdup(s, &n);
    if (!d)
        return 1;
    for (i = 0; i < n; i++)
        if (!(d[i] == ' ' || d[i] == '\t' || d[i] == '\n' || d[i] == '\r' || d[i] == '\v' || d[i] == '\f')) {
            r = 0;
            break;
        }
    free(d);
    return r;
}

static int truthy_str(jv m, const char *key)
{
    jv v;
    return json_get(m, key, &v) && json_type(v) == J_STR && v.n > 2;
}

/* an object copied member by member without `drop`, with the string `id` for "id" when set */
static void copy_obj(jw *o, jv b, const char *drop, const char *id)
{
    jit it;
    jv k, v;
    int n = 0;
    jw_raw(o, "{", 1);
    json_iter(b, &it);
    while (json_next(&it, &k, &v)) {
        char kk[64];
        json_str(k, kk, sizeof(kk));
        if (drop && !strcmp(kk, drop))
            continue;
        jw_rawz(o, n++ ? "," : "");
        jw_raw(o, k.p, k.n);
        jw_raw(o, ":", 1);
        if (id && !strcmp(kk, "id"))
            jw_strz(o, id);
        else
            jw_raw(o, v.p, v.n);
    }
    jw_raw(o, "}", 1);
}

static void text_block(jw *o, jv text)
{
    jw_rawz(o, "{\"type\":\"text\",\"text\":");
    jw_raw(o, text.p, text.n);
    jw_raw(o, "}", 1);
}

/* an assistant's content array for model `to` */
static int assistant_content(const llm_model *to, jv c, int same, idmap *ids, jw *o)
{
    jit it;
    jv b, t, v;
    int k = 0;
    jw_raw(o, "[", 1);
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        long mark = o->n;
        if (json_type(b) != J_OBJ || !json_get(b, "type", &t)) {
            jw_rawz(o, k ? "," : "");
            jw_raw(o, b.p, b.n);
            k++;
            continue;
        }
        jw_rawz(o, k ? "," : "");
        if (json_streq(t, "thinking")) {
            jv red;
            if (json_get(b, "redacted", &red) && json_type(red) == J_TRUE) {
                if (!same) {
                    o->n = mark;
                    o->p[mark] = 0;
                    continue;
                }
                jw_raw(o, b.p, b.n);
            } else if (same && truthy_str(b, "thinkingSignature")) {
                jw_raw(o, b.p, b.n);
            } else if (!json_get(b, "thinking", &v) || json_type(v) != J_STR || blank(v)) {
                o->n = mark;
                o->p[mark] = 0;
                continue;
            } else if (same) {
                jw_raw(o, b.p, b.n);
            } else
                text_block(o, v);
        } else if (json_streq(t, "text")) {
            if (same || !json_get(b, "text", &v))
                jw_raw(o, b.p, b.n);
            else
                text_block(o, v);
        } else if (json_streq(t, "toolCall")) {
            char id[1100], nid[1100];
            int drop_sig = !same && json_get(b, "thoughtSignature", &v);
            int changed = 0;
            id[0] = 0;
            if (json_get(b, "id", &v))
                json_str(v, id, sizeof(id));
            if (!same && id[0])
                changed = norm_id(to, id, nid, sizeof(nid));
            if (changed && map_add(ids, id, nid))
                return -1;
            if (drop_sig || changed)
                copy_obj(o, b, drop_sig ? "thoughtSignature" : 0, changed ? nid : 0);
            else
                jw_raw(o, b.p, b.n);
        } else if (json_streq(t, "raw")) {
            if (!same) {
                o->n = mark;
                o->p[mark] = 0;
                continue;
            }
            jw_raw(o, b.p, b.n);
        } else
            jw_raw(o, b.p, b.n);
        k++;
    }
    jw_raw(o, "]", 1);
    return o->oom ? -1 : 0;
}

/* user / toolResult content: images -> one placeholder text each run */
static void downgrade(jv c, const char *ph, jw *o)
{
    jit it;
    jv b, t, v;
    int k = 0, prev = 0;
    jw_raw(o, "[", 1);
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        if (json_type(b) == J_OBJ && json_get(b, "type", &t) && json_streq(t, "image")) {
            if (!prev) {
                jw_rawz(o, k++ ? "," : "");
                jw_rawz(o, "{\"type\":\"text\",\"text\":");
                jw_strz(o, ph);
                jw_raw(o, "}", 1);
            }
            prev = 1;
            continue;
        }
        jw_rawz(o, k++ ? "," : "");
        jw_raw(o, b.p, b.n);
        prev = json_type(b) == J_OBJ && json_get(b, "text", &v) && json_streq(v, ph);
    }
    jw_raw(o, "]", 1);
}

static int has_image(jv c)
{
    jit it;
    jv b, t;
    json_iter(c, &it);
    while (json_next(&it, 0, &b))
        if (json_type(b) == J_OBJ && json_get(b, "type", &t) && json_streq(t, "image"))
            return 1;
    return 0;
}

/* the message text with the spans [a0,a1) -> s0 and [b0,b1) -> s1 replaced (a before b; unused spans a0 < 0) */
static int rebuild(llm_view *v, const char *msg, long n, long a0, long a1, const jw *s0, long b0, long b1,
                   const jw *s1)
{
    jw o;
    jw_init(&o);
    jw_raw(&o, msg, a0);
    jw_raw(&o, s0->p, s0->n);
    if (b0 >= 0) {
        jw_raw(&o, msg + a1, b0 - a1);
        jw_raw(&o, s1->p, s1->n);
        jw_raw(&o, msg + b1, n - b1);
    } else
        jw_raw(&o, msg + a1, n - a1);
    if (o.oom || llm_view_own(v, o.p)) {
        jw_free(&o);
        return -1;
    }
    return llm_view_put(v, o.p, o.n);
}

static int first_pass(const llm_model *to, const char *const *msgs, const long *n, int count, llm_view *v)
{
    idmap ids;
    int i, rc = 0;
    memset(&ids, 0, sizeof(ids));
    memset(v, 0, sizeof(*v));
    for (i = 0; i < count && rc == 0; i++) {
        jv m, r, c, x;
        const char *msg = msgs[i];
        jw nc;
        int has_c, null_c;
        if (json_parse(msg, n[i], &m) || !json_get(m, "role", &r)) {
            rc = -1;
            break;
        }
        has_c = json_get(m, "content", &c);
        null_c = has_c && json_type(c) == J_NULL;
        jw_init(&nc);
        if (json_streq(r, "assistant") && has_c && (json_type(c) == J_ARR || null_c)) {
            jv p, a, md;
            int same = 0;
            const char *e = "[]";
            if (json_get(m, "provider", &p) && json_get(m, "api", &a) && json_get(m, "model", &md))
                same = json_streq(p, to->provider) && json_streq(a, to->api) && json_streq(md, to->id);
            if (null_c)
                jw_rawz(&nc, e);
            else if (assistant_content(to, c, same, &ids, &nc))
                rc = -1;
            if (!rc)
                rc = rebuild(v, msg, n[i], (long)(c.p - msg), (long)(c.p + c.n - msg), &nc, -1, 0, 0);
        } else if (json_streq(r, "toolResult")) {
            const char *nid = 0;
            char id[1100];
            jv idv;
            jw s1;
            long ia = -1, ib = -1;
            jw_init(&s1);
            if (json_get(m, "toolCallId", &idv)) {
                id[0] = 0;
                json_str(idv, id, sizeof(id));
                nid = map_get(&ids, id);
                if (nid) {
                    jw_strz(&s1, nid);
                    ia = (long)(idv.p - msg);
                    ib = ia + idv.n;
                }
            }
            if (has_c && (null_c || (!to->image && json_type(c) == J_ARR && has_image(c)))) {
                if (null_c)
                    jw_rawz(&nc, "[]");
                else
                    downgrade(c, IMG_TOOL, &nc);
                if (nid) {
                    long ca = (long)(c.p - msg), cb = ca + c.n;
                    /* two spans: the order in the text decides which is first */
                    if (ca < ia)
                        rc = rebuild(v, msg, n[i], ca, cb, &nc, ia, ib, &s1);
                    else
                        rc = rebuild(v, msg, n[i], ia, ib, &s1, ca, cb, &nc);
                } else
                    rc = rebuild(v, msg, n[i], (long)(c.p - msg), (long)(c.p + c.n - msg), &nc, -1, 0, 0);
            } else if (nid)
                rc = rebuild(v, msg, n[i], ia, ib, &s1, -1, 0, 0);
            else
                rc = llm_view_put(v, msg, n[i]);
            jw_free(&s1);
        } else if (json_streq(r, "user") && has_c && (null_c || (!to->image && json_type(c) == J_ARR && has_image(c)))) {
            if (null_c)
                jw_rawz(&nc, "[]");
            else
                downgrade(c, IMG_USER, &nc);
            rc = rebuild(v, msg, n[i], (long)(c.p - msg), (long)(c.p + c.n - msg), &nc, -1, 0, 0);
        } else if (null_c) {
            jw_rawz(&nc, "[]");
            rc = rebuild(v, msg, n[i], (long)(c.p - msg), (long)(c.p + c.n - msg), &nc, -1, 0, 0);
        } else
            rc = llm_view_put(v, msg, n[i]);
        (void)x;
        jw_free(&nc);
    }
    map_free(&ids);
    if (rc)
        llm_view_free(v);
    return rc;
}

int llm_prepare(const llm_model *to, const char *const *msgs, const long *n, int count, llm_ts ts, llm_view *out)
{
    llm_view a, b;
    int i;
    if (first_pass(to, msgs, n, count, &a))
        return -1;
    if (llm_transform(a.m, a.n, a.count, ts, &b)) {
        llm_view_free(&a);
        llm_view_free(&b);
        return -1;
    }
    /* b's messages point into a's strings: a's strings become b's */
    for (i = 0; i < a.nown; i++)
        if (llm_view_own(&b, a.own[i])) {
            llm_view_free(&a);
            llm_view_free(&b);
            return -1;
        }
    a.nown = 0;
    llm_view_free(&a);
    *out = b;
    return 0;
}
