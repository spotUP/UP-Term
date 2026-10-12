/* see transform.h */
#include <stdlib.h>
#include <string.h>
#include "transform.h"

#define TR_CALLS 64

typedef struct pending {
    char id[96], name[64];
    int answered;
} pending;

int llm_view_put(llm_view *v, const char *m, long n)
{
    if (v->count == v->cap) {
        int nc = v->cap ? v->cap * 2 : 16;
        const char **gm = (const char **)malloc((size_t)nc * sizeof(*gm));
        long *gn = (long *)malloc((size_t)nc * sizeof(*gn));
        if (!gm || !gn) {
            free(gm);
            free(gn);
            return -1;
        }
        if (v->count) {
            memcpy(gm, v->m, (size_t)v->count * sizeof(*gm));
            memcpy(gn, v->n, (size_t)v->count * sizeof(*gn));
        }
        free(v->m);
        free(v->n);
        v->m = gm;
        v->n = gn;
        v->cap = nc;
    }
    v->m[v->count] = m;
    v->n[v->count++] = n;
    return 0;
}

/* takes ownership of p (freed with the view): 0, -1 out of memory (p is not taken) */
int llm_view_own(llm_view *v, char *p)
{
    if (v->nown == v->owncap) {
        int nc = v->owncap ? v->owncap * 2 : 8;
        char **g = (char **)malloc((size_t)nc * sizeof(*g));
        if (!g)
            return -1;
        if (v->nown)
            memcpy(g, v->own, (size_t)v->nown * sizeof(*g));
        free(v->own);
        v->own = g;
        v->owncap = nc;
    }
    v->own[v->nown++] = p;
    return 0;
}

/* a "No result provided" toolResult for each unanswered call, then the held system messages */
static int close_pending(llm_view *v, pending *p, int *np, const int *held, int *nheld,
                         const char *const *msgs, const long *n, llm_ts ts)
{
    int i;
    for (i = 0; i < *np; i++) {
        jw w, c;
        if (p[i].answered)
            continue;
        jw_init(&c);
        jw_rawz(&c, "[");
        llm_b_text(&c, "No result provided", 18, 0);
        jw_rawz(&c, "]");
        jw_init(&w);
        llm_m_toolresult(&w, p[i].id, p[i].name, c.p, c.n, 0, 0, 1, -1, ts);
        jw_free(&c);
        if (w.oom || !w.p) {
            jw_free(&w);
            return -1;
        }
        if (llm_view_own(v, w.p)) {
            jw_free(&w);
            return -1;
        }
        if (llm_view_put(v, w.p, w.n))
            return -1;
        v->synth++;
    }
    *np = 0;
    for (i = 0; i < *nheld; i++)
        if (llm_view_put(v, msgs[held[i]], n[held[i]]))
            return -1;
    *nheld = 0;
    return 0;
}

int llm_transform(const char *const *msgs, const long *n, int count, llm_ts ts, llm_view *v)
{
    static pending p[TR_CALLS];
    int held[TR_CALLS];
    int np = 0, nheld = 0, i;
    memset(v, 0, sizeof(*v));
    for (i = 0; i < count; i++) {
        jv m, r, c, b, t, x;
        jit it;
        if (json_parse(msgs[i], n[i], &m) || !json_get(m, "role", &r))
            return -1;
        if (json_streq(r, "assistant")) {
            if (close_pending(v, p, &np, held, &nheld, msgs, n, ts))
                return -1;
            if (json_get(m, "stopReason", &x) && (json_streq(x, "error") || json_streq(x, "aborted"))) {
                v->dropped++;
                continue;
            }
            if (json_get(m, "content", &c) && json_type(c) == J_ARR) {
                json_iter(c, &it);
                while (json_next(&it, 0, &b))
                    if (json_get(b, "type", &t) && json_streq(t, "toolCall") && np < TR_CALLS) {
                        p[np].id[0] = p[np].name[0] = 0;
                        if (json_get(b, "id", &x))
                            json_str(x, p[np].id, sizeof(p[np].id));
                        if (json_get(b, "name", &x))
                            json_str(x, p[np].name, sizeof(p[np].name));
                        p[np++].answered = 0;
                    }
            }
            if (llm_view_put(v, msgs[i], n[i]))
                return -1;
        } else if (json_streq(r, "toolResult")) {
            char id[96];
            int k;
            id[0] = 0;
            if (json_get(m, "toolCallId", &x))
                json_str(x, id, sizeof(id));
            for (k = 0; k < np; k++)
                if (!strcmp(p[k].id, id))
                    p[k].answered = 1;
            if (llm_view_put(v, msgs[i], n[i]))
                return -1;
        } else if (json_streq(r, "system") && np > 0 && nheld < TR_CALLS) {
            held[nheld++] = i;
        } else {
            if (json_streq(r, "user") && close_pending(v, p, &np, held, &nheld, msgs, n, ts))
                return -1;
            if (llm_view_put(v, msgs[i], n[i]))
                return -1;
        }
    }
    return close_pending(v, p, &np, held, &nheld, msgs, n, ts);
}

void llm_view_free(llm_view *v)
{
    int i;
    for (i = 0; i < v->nown; i++)
        free(v->own[i]);
    free(v->own);
    free(v->m);
    free(v->n);
    memset(v, 0, sizeof(*v));
}
