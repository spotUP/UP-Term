/* radius -- see radius.h. */
#include <stdlib.h>
#include <string.h>
#include "radius.h"
#include "../claude/util.h"

void llm_radius_gateway(const char *value, int strip_v1, char *out, long cap)
{
    char tmp[400];
    long n;
    cl_copy(tmp, value ? value : "", sizeof(tmp));
    n = (long)strlen(tmp);
    if (strip_v1) {
        /* replace(/\/v1\/?$/u, "") */
        if (n >= 4 && !strcmp(tmp + n - 4, "/v1/"))
            tmp[n - 4] = 0;
        else if (n >= 3 && !strcmp(tmp + n - 3, "/v1"))
            tmp[n - 3] = 0;
    }
    if (!(cl_strnieq(tmp, "http://", 7) || cl_strnieq(tmp, "https://", 8))) {
        char t2[400];
        cl_copy(t2, "https://", sizeof(t2));
        cl_cat(t2, tmp, sizeof(t2));
        cl_copy(tmp, t2, sizeof(tmp));
    }
    n = (long)strlen(tmp);
    while (n > 0 && tmp[n - 1] == '/')
        tmp[--n] = 0;
    cl_copy(out, tmp, cap);
}

/* isRadiusGatewayModel */
static int valid_model(jv m)
{
    jv v;
    if (json_type(m) != J_OBJ)
        return 0;
    return json_get(m, "id", &v) && json_type(v) == J_STR && json_get(m, "name", &v) && json_type(v) == J_STR &&
           json_get(m, "reasoning", &v) && (json_type(v) == J_TRUE || json_type(v) == J_FALSE) &&
           json_get(m, "input", &v) && json_type(v) == J_ARR && json_get(m, "cost", &v) && json_type(v) == J_OBJ &&
           json_get(m, "contextWindow", &v) && json_type(v) == J_NUM && json_get(m, "maxTokens", &v) &&
           json_type(v) == J_NUM;
}

int llm_radius_models(const char *provider, const char *config, long n, jw *models)
{
    jv root, base, list, m, k, v;
    jit it, mi;
    int count = 0;
    char *burl;
    if (json_parse(config, n, &root) || json_type(root) != J_OBJ || !json_get(root, "baseUrl", &base) ||
        json_type(base) != J_STR || !json_get(root, "models", &list) || json_type(list) != J_ARR)
        return -1;
    burl = json_strdup(base, 0);
    if (!burl)
        return -1;
    jw_rawz(models, "[");
    json_iter(list, &it);
    while (json_next(&it, 0, &m)) {
        int first = 1;
        if (!valid_model(m))
            continue;
        jw_rawz(models, count++ ? ",{" : "{");
        json_iter(m, &mi);
        while (json_next(&mi, &k, &v)) {
            if (json_streq(k, "api") || json_streq(k, "provider") || json_streq(k, "baseUrl"))
                continue;
            if (!first)
                jw_rawz(models, ",");
            first = 0;
            jw_raw(models, k.p, k.n);
            jw_rawz(models, ":");
            jw_raw(models, v.p, v.n);
        }
        jw_rawz(models, ",\"api\":\"pi-messages\",\"provider\":");
        jw_strz(models, provider);
        jw_rawz(models, ",\"baseUrl\":");
        jw_strz(models, burl);
        jw_rawz(models, "}");
    }
    jw_rawz(models, "]");
    free(burl);
    return models->oom ? -1 : 0;
}

int llm_radius_apply(llm_reg *r, const char *provider, const char *models, long n, char *err, long cap)
{
    jv list, m, v;
    jit it;
    jw text;
    llm_model *save = 0;
    int save_n = r->n, count = 0, rc = 0;
    if (json_parse(models, n, &list) || json_type(list) != J_ARR) {
        cl_copy(err, "the Radius model list is not an array", cap);
        return -1;
    }
    jw_init(&text);
    jw_rawz(&text, "{\"providers\":{");
    jw_strz(&text, provider);
    jw_rawz(&text, ":{\"api\":\"pi-messages\",\"models\":[");
    json_iter(list, &it);
    while (json_next(&it, 0, &m)) {
        if (json_type(m) != J_OBJ || !json_get(m, "provider", &v) || !json_streq(v, provider))
            continue;           /* models.filter((model) => model.provider === id) */
        if (count++)
            jw_rawz(&text, ",");
        jw_raw(&text, m.p, m.n);
    }
    jw_rawz(&text, "]}}}");
    if (text.oom) {
        jw_free(&text);
        cl_copy(err, "out of memory", cap);
        return -1;
    }
    if (r->n) {
        save = (llm_model *)malloc((size_t)r->n * sizeof(*save));
        if (!save) {
            jw_free(&text);
            cl_copy(err, "out of memory", cap);
            return -1;
        }
        memcpy(save, r->m, (size_t)r->n * sizeof(*save));
    }
    llm_reg_drop_provider(r, provider);
    if (count && llm_reg_overlay(r, text.p, text.n)) {
        cl_copy(err, r->err, cap);
        if (save)
            memcpy(r->m, save, (size_t)save_n * sizeof(*save));
        r->n = save_n;
        rc = -1;
    }
    free(save);
    jw_free(&text);
    return rc;
}

/* ---- OpenRouter's per-key catalog ---- */

long llm_or_price(const char *s, long fallback)
{
    char *end = 0;
    double x;
    if (!s || !*s)
        return fallback;
    x = strtod(s, &end);
    if (end == s || !(x >= 0.0))
        return fallback;
    return (long)(x * 1000000000000.0 + 0.5);       /* $/token to micro-dollars per million tokens */
}

/* the shipped catalog's openrouter block: the overlay text {"providers":{"openrouter":{...}}} in *text, its model list in *models */
static int or_block(jw *text, jv *models)
{
    jv root, prov, blk;
    long bn;
    const char *b = llm_reg_builtin_text(&bn);
    if (json_parse(b, bn, &root) || !json_get(root, "providers", &prov) || !json_get(prov, LLM_OPENROUTER, &blk) ||
        !json_get(blk, "models", models))
        return -1;
    jw_init(text);
    jw_rawz(text, "{\"providers\":{\"" LLM_OPENROUTER "\":");
    jw_raw(text, blk.p, blk.n);
    jw_rawz(text, "}}");
    return text->oom ? -1 : 0;
}

int llm_or_reset(llm_reg *r)
{
    jw text;
    jv models;
    int rc;
    if (or_block(&text, &models))
        return -1;
    rc = llm_reg_overlay(r, text.p, text.n);
    jw_free(&text);
    return rc;
}

/* ---- the answer of GET /models/user, read as a stream: the elements of "data" one at a time ---- */
struct llm_or_user {
    int depth;                  /* { and [ open */
    int in_str, esc, expect_key;
    char key[16];
    int keyn, key_done;         /* the key of the member being read at depth 1 */
    int in_data;                /* inside the "data" array at depth 2 */
    int saw_data;
    int elem;                   /* an element object is being collected */
    long elem_cap_hit;
    jw buf;
    char (*want)[96];           /* the shipped models' ids */
    int nwant;
    llm_or_entry e[LLM_OR_MAX];
    int n;
    int bad;
};

#define OR_ELEM_MAX 131072L

llm_or_user *llm_or_user_new(void)
{
    llm_or_user *p = (llm_or_user *)calloc(1, sizeof(*p));
    jw text;
    jv models, el, v;
    jit it;
    if (!p)
        return 0;
    jw_init(&p->buf);
    p->want = (char (*)[96])calloc(LLM_OR_MAX, 96);
    if (!p->want) {
        free(p);
        return 0;
    }
    if (!or_block(&text, &models)) {
        json_iter(models, &it);
        while (json_next(&it, 0, &el))
            if (p->nwant < LLM_OR_MAX && json_get(el, "id", &v) && json_type(v) == J_STR)
                json_str(v, p->want[p->nwant++], 96);
        jw_free(&text);
    }
    return p;
}

void llm_or_user_free(llm_or_user *p)
{
    if (!p)
        return;
    jw_free(&p->buf);
    free(p->want);
    free(p);
}

static long or_num(jv o, const char *k)
{
    jv v;
    return json_get(o, k, &v) && json_type(v) == J_NUM ? json_long(v, 0) : 0;
}

static void or_price(jv pr, const char *k, long *out)
{
    jv v;
    char s[48];
    *out = 0;
    if (json_get(pr, k, &v) && json_type(v) == J_STR) {
        json_str(v, s, sizeof(s));
        *out = llm_or_price(s, 0);
    }
}

/* one element of data: kept when it is a shipped model (the first of an id) */
static void or_element(llm_or_user *p)
{
    jv el, v, tp, pr;
    char id[96];
    int i;
    llm_or_entry *e;
    long c;
    if (json_parse(p->buf.p, p->buf.n, &el) || json_type(el) != J_OBJ || !json_get(el, "id", &v) || json_type(v) != J_STR)
        return;
    json_str(v, id, sizeof(id));
    for (i = 0; i < p->nwant; i++)
        if (!strcmp(p->want[i], id))
            break;
    if (i == p->nwant)
        return;
    for (i = 0; i < p->n; i++)
        if (!strcmp(p->e[i].id, id))
            return;
    if (p->n >= LLM_OR_MAX)
        return;
    e = &p->e[p->n++];
    memset(e, 0, sizeof(*e));
    cl_copy(e->id, id, sizeof(e->id));
    /* top_provider.context_length || context_length; top_provider.max_completion_tokens (a missing limit is absent, null or 0) */
    c = 0;
    if (json_get(el, "top_provider", &tp) && json_type(tp) == J_OBJ) {
        c = or_num(tp, "context_length");
        e->maxtok = or_num(tp, "max_completion_tokens");
    }
    e->ctx = c ? c : or_num(el, "context_length");
    if (json_get(el, "pricing", &pr) && json_type(pr) == J_OBJ) {
        e->has_cost = 1;
        or_price(pr, "prompt", &e->cost[0]);
        or_price(pr, "completion", &e->cost[1]);
        or_price(pr, "input_cache_read", &e->cost[2]);
        or_price(pr, "input_cache_write", &e->cost[3]);
    }
}

int llm_or_user_feed(llm_or_user *p, const char *s, long n)
{
    long i;
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (p->elem && !p->elem_cap_hit) {
            if (p->buf.n < OR_ELEM_MAX)
                jw_raw(&p->buf, &c, 1);
            else
                p->elem_cap_hit = 1;            /* an element past 128 KB is no model of the catalog: skipped */
        }
        if (p->in_str) {
            if (p->esc)
                p->esc = 0;
            else if (c == '\\')
                p->esc = 1;
            else if (c == '"') {
                p->in_str = 0;
                if (p->depth == 1 && p->expect_key) {
                    p->key[p->keyn < 15 ? p->keyn : 15] = 0;
                    p->key_done = 1;
                    p->expect_key = 0;
                }
            } else if (p->depth == 1 && p->expect_key && !p->key_done && p->keyn < 15)
                p->key[p->keyn++] = c;
            continue;
        }
        if (c == '"') {
            p->in_str = 1;
            if (p->depth == 1 && p->expect_key) {
                p->keyn = 0;
                p->key_done = 0;
            }
        } else if (c == '{' || c == '[') {
            p->depth++;
            if (p->depth == 1) {
                p->expect_key = c == '{';
                p->key_done = 0;
                p->keyn = 0;
            } else if (p->depth == 2 && c == '[' && p->key_done && !strcmp(p->key, "data")) {
                p->in_data = 1;
                p->saw_data = 1;
            } else if (p->depth == 3 && c == '{' && p->in_data) {
                p->elem = 1;
                p->elem_cap_hit = 0;
                jw_reset(&p->buf);
                jw_raw(&p->buf, &c, 1);
            }
        } else if (c == '}' || c == ']') {
            if (p->depth == 3 && p->elem && c == '}') {
                p->elem = 0;
                if (!p->elem_cap_hit)
                    or_element(p);
                jw_reset(&p->buf);
            }
            if (p->depth == 2 && c == ']')
                p->in_data = 0;
            p->depth--;
            if (p->depth < 0)
                p->bad = 1;
        } else if (c == ',' && p->depth == 1) {
            p->expect_key = 1;
            p->key_done = 0;
            p->keyn = 0;
        }
    }
    return p->buf.oom ? -1 : 0;
}

int llm_or_user_end(llm_or_user *p)
{
    return p->saw_data && !p->bad && p->depth == 0 && !p->in_str ? 0 : -1;
}

int llm_or_user_count(const llm_or_user *p)
{
    return p->n;
}

const llm_or_entry *llm_or_user_get(const llm_or_user *p, int i)
{
    return i >= 0 && i < p->n ? &p->e[i] : 0;
}

/* ---- the registry and the store ---- */

static int or_shipped(const char *id)
{
    jw text;
    jv models, el, v;
    jit it;
    int found = 0;
    char s[96];
    if (or_block(&text, &models))
        return 0;
    json_iter(models, &it);
    while (!found && json_next(&it, 0, &el))
        if (json_get(el, "id", &v) && json_type(v) == J_STR) {
            json_str(v, s, sizeof(s));
            found = !strcmp(s, id);
        }
    jw_free(&text);
    return found;
}

int llm_or_apply(llm_reg *r, const llm_or_entry *e, int n)
{
    int i, j, keep = 0;
    for (i = 0; i < r->n; i++) {
        llm_model *m = &r->m[i];
        int hit = -1;
        if (strcmp(m->provider, LLM_OPENROUTER) || !or_shipped(m->id)) {
            r->m[keep++] = *m;
            continue;
        }
        for (j = 0; j < n; j++)
            if (!strcmp(e[j].id, m->id)) {
                hit = j;
                break;
            }
        if (hit < 0)
            continue;                           /* the key may not use it */
        if (e[hit].ctx)
            m->context_window = e[hit].ctx;
        if (e[hit].maxtok)
            m->max_tokens = e[hit].maxtok;
        if (e[hit].has_cost)
            memcpy(m->cost, e[hit].cost, sizeof(m->cost));
        r->m[keep++] = *m;
    }
    r->n = keep;
    return 0;
}

void llm_or_store(const llm_reg *r, jw *models)
{
    int i, k = 0;
    jw_rawz(models, "[");
    for (i = 0; i < r->n; i++) {
        const llm_model *m = &r->m[i];
        static const char *const nm[4] = { "input", "output", "cacheRead", "cacheWrite" };
        int c;
        if (strcmp(m->provider, LLM_OPENROUTER) || !or_shipped(m->id))
            continue;
        jw_rawz(models, k++ ? ",{\"id\":" : "{\"id\":");
        jw_strz(models, m->id);
        jw_rawz(models, ",\"name\":");
        jw_strz(models, m->name ? m->name : m->id);
        jw_rawz(models, ",\"api\":");
        jw_strz(models, m->api);
        jw_rawz(models, ",\"provider\":");
        jw_strz(models, m->provider);
        jw_rawz(models, ",\"baseUrl\":");
        jw_strz(models, m->base_url ? m->base_url : "");
        jw_rawz(models, m->reasoning ? ",\"reasoning\":true" : ",\"reasoning\":false");
        jw_rawz(models, m->image ? ",\"input\":[\"text\",\"image\"]" : ",\"input\":[\"text\"]");
        jw_rawz(models, ",\"cost\":{");
        for (c = 0; c < 4; c++) {
            jw_rawz(models, c ? ",\"" : "\"");
            jw_rawz(models, nm[c]);
            jw_rawz(models, "\":");
            llm_w_micro(models, m->cost[c]);
        }
        jw_rawz(models, "},\"contextWindow\":");
        jw_long(models, m->context_window);
        jw_rawz(models, ",\"maxTokens\":");
        jw_long(models, m->max_tokens);
        jw_rawz(models, "}");
    }
    jw_rawz(models, "]");
}

int llm_or_stored(const char *models, long n, llm_or_entry *out, int max)
{
    jv list, m, v, c;
    jit it;
    int k = 0;
    static const char *const nm[4] = { "input", "output", "cacheRead", "cacheWrite" };
    if (json_parse(models, n, &list) || json_type(list) != J_ARR)
        return 0;
    json_iter(list, &it);
    while (k < max && json_next(&it, 0, &m)) {
        llm_or_entry *e = &out[k];
        if (json_type(m) != J_OBJ || !json_get(m, "provider", &v) || !json_streq(v, LLM_OPENROUTER) || !json_get(m, "id", &v) ||
            json_type(v) != J_STR)
            continue;
        memset(e, 0, sizeof(*e));
        json_str(v, e->id, sizeof(e->id));
        e->ctx = or_num(m, "contextWindow");
        e->maxtok = or_num(m, "maxTokens");
        if (json_get(m, "cost", &c) && json_type(c) == J_OBJ) {
            int i;
            e->has_cost = 1;
            for (i = 0; i < 4; i++)
                if (json_get(c, nm[i], &v) && json_type(v) == J_NUM)
                    e->cost[i] = llm_r_micro(v);
        }
        k++;
    }
    return k;
}
