/* see transcript.h */
#include <stdlib.h>
#include <string.h>
#include "transcript.h"

/* the inner text of a JSON string value (between the quotes), appended raw */
void llm_tx_inner(jw *w, jv s)
{
    if (json_type(s) == J_STR && s.n >= 2)
        jw_raw(w, s.p + 1, s.n - 2);
}

/* pi's contentText: a string, or the text blocks joined by "\n" (inner form, no quotes); the number of parts */
int llm_tx_content_text(jw *w, jv c)
{
    jit it;
    jv b, t, v;
    int k = 0;
    if (json_type(c) == J_STR) {
        llm_tx_inner(w, c);
        return 1;
    }
    if (json_type(c) != J_ARR)
        return 0;
    json_iter(c, &it);
    while (json_next(&it, 0, &b))
        if (json_get(b, "type", &t) && json_streq(t, "text") && json_get(b, "text", &v)) {
            if (k++)
                jw_rawz(w, "\\n");
            llm_tx_inner(w, v);
        }
    return k;
}

/* pi's getSystemMessageText: the content, then the sections' texts, the non-empty ones joined by a blank line; the
 * quoted JSON string appended */
void llm_tx_sys_text(jw *w, jv m)
{
    jw part;
    jv c, sec, k, v;
    jit it;
    int n = 0;
    jw_raw(w, "\"", 1);
    jw_init(&part);
    if (json_get(m, "content", &c))
        llm_tx_content_text(&part, c);
    if (part.n) {
        jw_raw(w, part.p, part.n);
        n++;
    }
    jw_free(&part);
    if (json_get(m, "sections", &sec) && json_type(sec) == J_OBJ) {
        json_iter(sec, &it);
        while (json_next(&it, &k, &v))
            if (json_type(v) == J_STR && v.n > 2) {
                if (n++)
                    jw_rawz(w, "\\n\\n");
                llm_tx_inner(w, v);
            }
    }
    jw_raw(w, "\"", 1);
}

/* pi's renderSystemMessageUpdate as a quoted string; 0 (nothing appended) when it has nothing to say */
long llm_tx_sys_update(jw *w, jv m)
{
    jw part;
    jv c, sec, k, v;
    jit it;
    int n = 0;
    long start = w->n;
    jw_raw(w, "\"", 1);
    jw_init(&part);
    if (json_get(m, "content", &c))
        llm_tx_content_text(&part, c);
    if (part.n) {
        jw_raw(w, part.p, part.n);
        n++;
    }
    jw_free(&part);
    if (json_get(m, "sections", &sec) && json_type(sec) == J_OBJ) {
        json_iter(sec, &it);
        while (json_next(&it, &k, &v)) {
            if (n++)
                jw_rawz(w, "\\n\\n");
            if (json_type(v) == J_NULL) {
                jw_rawz(w, "Removed system prompt section \\\"");
                llm_tx_inner(w, k);
                jw_rawz(w, "\\\".");
            } else {
                jw_rawz(w, "Updated system prompt section \\\"");
                llm_tx_inner(w, k);
                jw_rawz(w, "\\\":\\n\\n");
                llm_tx_inner(w, v);
            }
        }
    }
    jw_raw(w, "\"", 1);
    if (w->n == start + 2) {
        w->n = start;
        if (w->p)
            w->p[w->n] = 0;
        return 0;
    }
    return w->n - start;
}

static int tool_find(const jv *tl, int n, jv name)
{
    int i;
    jv v;
    for (i = 0; i < n; i++)
        if (json_get(tl[i], "name", &v) && v.n == name.n && !memcmp(v.p, name.p, (size_t)v.n))
            return i;
    return -1;
}

/* the tools of a toolsAdded array put into tl (a name set again keeps its place) */
static int tools_set(jv arr, jv *tl, int *n, int *twice)
{
    jit it;
    jv t, nm;
    if (json_type(arr) != J_ARR)
        return 0;
    json_iter(arr, &it);
    while (json_next(&it, 0, &t)) {
        int i;
        if (!json_get(t, "name", &nm))
            return -1;
        i = tool_find(tl, *n, nm);
        if (i < 0)
            i = (*n)++;
        else
            *twice = 1;
        tl[i] = t;
    }
    return 0;
}

static void tools_del(jv arr, jv *tl, int *n)
{
    jit it;
    jv t, nm;
    if (json_type(arr) != J_ARR)
        return;
    json_iter(arr, &it);
    while (json_next(&it, 0, &t)) {
        int i;
        if (!json_get(t, "name", &nm))
            continue;
        i = tool_find(tl, *n, nm);
        if (i >= 0) {
            memmove(tl + i, tl + i + 1, (size_t)(*n - i - 1) * sizeof(*tl));
            (*n)--;
        }
    }
}


/* how many tools an array declares */
static int tools_in(jv arr)
{
    jit it;
    jv t;
    int n = 0;
    if (json_type(arr) != J_ARR)
        return 0;
    json_iter(arr, &it);
    while (json_next(&it, 0, &t))
        n++;
    return n;
}

void llm_tx_free(llm_tx *t)
{
    free(t->itl);
    free(t->tl);
    memset(t, 0, sizeof(*t));
}

int llm_tx_plan(const llm_ctx *c, int mid_sys, llm_tx *t)
{
    int i, cap = 0, twice = 0, removed = 0;
    jv m, r, v, ts;
    memset(t, 0, sizeof(*t));
    if (mid_sys && c->nmsg > 0 && !json_parse(c->msgs[0], c->msg_n[0], &m) && json_get(m, "role", &r) &&
        json_streq(r, "system")) {
        t->has_init = 1;
        t->init = m;
    }
    /* room: every tool any system message or c->tools declares */
    if (t->has_init) {
        if (json_get(t->init, "toolsAdded", &v))
            cap += tools_in(v);
    } else if (c->tools && c->tools_n > 2 && !json_parse(c->tools, c->tools_n, &ts))
        cap += tools_in(ts);
    if (mid_sys)
        for (i = t->has_init ? 1 : 0; i < c->nmsg; i++)
            if (!json_parse(c->msgs[i], c->msg_n[i], &m) && json_get(m, "role", &r) && json_streq(r, "system") &&
                json_get(m, "toolsAdded", &v))
                cap += tools_in(v);
    t->itl = (jv *)malloc((size_t)(cap + 1) * sizeof(jv));
    t->tl = (jv *)malloc((size_t)(cap + 1) * sizeof(jv));
    if (!t->itl || !t->tl)
        return -1;
    if (t->has_init) {
        if (json_get(t->init, "toolsAdded", &v) && tools_set(v, t->itl, &t->nitl, &twice))
            return -1;
    } else if (c->tools && c->tools_n > 2) {
        if (json_parse(c->tools, c->tools_n, &ts) || json_type(ts) != J_ARR || tools_set(ts, t->itl, &t->nitl, &twice))
            return -1;
    }
    memcpy(t->tl, t->itl, (size_t)t->nitl * sizeof(jv));
    t->ntl = t->nitl;
    if (mid_sys) {
        for (i = t->has_init ? 1 : 0; i < c->nmsg; i++) {
            if (json_parse(c->msgs[i], c->msg_n[i], &m) || !json_get(m, "role", &r) || !json_streq(r, "system"))
                continue;
            if (json_get(m, "toolsRemoved", &v)) {
                if (tools_in(v) > 0)
                    removed = 1;
                tools_del(v, t->tl, &t->ntl);
            }
            if (json_get(m, "toolsAdded", &v) && tools_set(v, t->tl, &t->ntl, &twice))
                return -1;
        }
    }
    t->additive = !twice && !removed;
    return 0;
}

/* JSON.stringify of a parsed value: no whitespace, strings re-escaped the way JSON.stringify writes them; drop_empty_keys:
 * members whose key is "" left out (bedrock-converse-stream's sanitizeBedrockDocument) */
void llm_json_compact(jw *w, jv v, int drop_empty_keys)
{
    jit it;
    jv k, x;
    int n = 0;
    switch (json_type(v)) {
    case J_OBJ:
        jw_rawz(w, "{");
        json_iter(v, &it);
        while (json_next(&it, &k, &x)) {
            char *d;
            long l;
            d = json_strdup(k, &l);
            if (drop_empty_keys && (!d || l == 0)) {
                free(d);
                continue;
            }
            jw_rawz(w, n++ ? "," : "");
            jw_str(w, d ? d : "", d ? l : 0);
            free(d);
            jw_rawz(w, ":");
            llm_json_compact(w, x, drop_empty_keys);
        }
        jw_rawz(w, "}");
        break;
    case J_ARR:
        jw_rawz(w, "[");
        json_iter(v, &it);
        while (json_next(&it, 0, &x)) {
            jw_rawz(w, n++ ? "," : "");
            llm_json_compact(w, x, drop_empty_keys);
        }
        jw_rawz(w, "]");
        break;
    case J_STR:
        {
            char *d;
            long l;
            d = json_strdup(v, &l);
            jw_str(w, d ? d : "", d ? l : 0);
            free(d);
        }
        break;
    default:
        jw_raw(w, v.p, v.n);
        break;
    }
}

/* a JSON string value that is empty after JS String.prototype.trim() (ASCII white space; pi's trim also takes the Unicode spaces) */
int llm_tx_blank(jv s)
{
    char *d;
    long l, i;
    int b = 1;
    d = json_strdup(s, &l);
    if (!d)
        return 1;
    for (i = 0; i < l; i++)
        if (!(d[i] == ' ' || d[i] == '\t' || d[i] == '\n' || d[i] == '\r' || d[i] == '\v' || d[i] == '\f')) {
            b = 0;
            break;
        }
    free(d);
    return b;
}
