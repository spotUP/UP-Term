/* models.json laid over the registry (plan F2): pi's model-config.ts schema
 * and provider-composer.ts merge rules (pi f10993bc). See registry.h.
 *
 * Read: comments (// and slash-star) and a BOM are dropped first (pi's
 * stripJsonComments), the text must be {"providers": {...}}. A provider's
 * baseUrl / apiKey / headers / authHeader / compat are written into all its
 * models, `models` adds a model or replaces the one with that id,
 * `modelOverrides` merges into an existing one (unknown ids are ignored).
 * Not carried over (accepted and ignored): inputLimits, promptCache,
 * samplingParams*, oauth, the compat flags this client has no use for.
 * Portable C89. */
#include <stdlib.h>
#include <string.h>
#include "registry.h"
#include "../claude/util.h"

typedef struct ctx {
    llm_reg *r;
    char path[160];
} ctx;

static int fail(ctx *c, const char *what)
{
    cl_copy(c->r->err, "Invalid models.json schema: ", sizeof(c->r->err));
    cl_cat(c->r->err, c->path, sizeof(c->r->err));
    cl_cat(c->r->err, ": ", sizeof(c->r->err));
    cl_cat(c->r->err, what, sizeof(c->r->err));
    return -1;
}

static char *sdup(ctx *c, jv v)
{
    long n;
    char *s = json_strdup(v, &n), *d;
    if (!s)
        return 0;
    d = llm_reg_dup(c->r, s, n);
    free(s);
    return d;
}

/* ---- the text: no BOM, no comments ---- */
static char *clean(const char *t, long n, long *out)
{
    char *b = (char *)malloc((size_t)n + 1);
    long i = 0, k = 0;
    int in = 0;
    if (!b)
        return 0;
    if (n >= 3 && !memcmp(t, "\xEF\xBB\xBF", 3))
        i = 3;
    while (i < n) {
        char ch = t[i];
        if (in) {
            b[k++] = ch;
            if (ch == '\\' && i + 1 < n)
                b[k++] = t[++i];
            else if (ch == '"')
                in = 0;
            i++;
        } else if (ch == '"') {
            in = 1;
            b[k++] = ch;
            i++;
        } else if (ch == '/' && i + 1 < n && t[i + 1] == '/') {
            while (i < n && t[i] != '\n')
                i++;
        } else if (ch == '/' && i + 1 < n && t[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(t[i] == '*' && t[i + 1] == '/'))
                i++;
            i += 2;
        } else {
            b[k++] = ch;
            i++;
        }
    }
    b[k] = 0;
    *out = k;
    return b;
}

/* ---- pieces ---- */

static int rd_compat(ctx *c, jv cv, llm_model *m)
{
    jv v;
    if (json_type(cv) != J_OBJ)
        return fail(c, "compat must be an object");
    if (json_get(cv, "supportsDeveloperRole", &v))
        m->compat.developer_role = json_type(v) == J_TRUE;
    if (json_get(cv, "supportsReasoningEffort", &v))
        m->compat.reasoning_effort = json_type(v) == J_TRUE;
    if (json_get(cv, "supportsUsageInStreaming", &v))
        m->compat.usage_in_streaming = json_type(v) == J_TRUE;
    if (json_get(cv, "requiresToolResultName", &v))
        m->compat.tool_result_name = json_type(v) == J_TRUE;
    if (json_get(cv, "requiresThinkingAsText", &v))
        m->compat.thinking_as_text = json_type(v) == J_TRUE;
    if (json_get(cv, "maxTokensField", &v))
        m->compat.max_tokens_field = json_streq(v, "max_tokens");
    if (json_get(cv, "forceAdaptiveThinking", &v))
        m->adaptive = json_type(v) == J_TRUE;
    return 0;
}

static int rd_tmap(ctx *c, jv tv, llm_model *m)
{
    int i;
    jv v;
    if (json_type(tv) != J_OBJ)
        return fail(c, "thinkingLevelMap must be an object");
    for (i = 0; i < LT_N; i++)
        if (json_get(tv, llm_think_name(i), &v)) {
            if (json_type(v) == J_NULL)
                m->thinking_map[i].kind = LTM_NULL;
            else if (json_type(v) == J_STR) {
                m->thinking_map[i].kind = LTM_STR;
                m->thinking_map[i].val = sdup(c, v);
            } else
                return fail(c, "thinkingLevelMap values are strings or null");
        }
    return c->r->oom ? -1 : 0;
}

static int rd_cost(ctx *c, jv cv, long cost[4], int full)
{
    static const char *const k[4] = { "input", "output", "cacheRead", "cacheWrite" };
    int i;
    jv v;
    if (json_type(cv) != J_OBJ)
        return fail(c, "cost must be an object");
    for (i = 0; i < 4; i++) {
        if (json_get(cv, k[i], &v)) {
            if (json_type(v) != J_NUM)
                return fail(c, "cost values are numbers");
            cost[i] = llm_r_micro(v);
        } else if (full)
            return fail(c, "cost needs input, output, cacheRead and cacheWrite");
    }
    return 0;
}

static int rd_input(ctx *c, jv iv, llm_model *m)
{
    jv e;
    jit it;
    if (json_type(iv) != J_ARR)
        return fail(c, "input must be an array");
    m->image = 0;
    json_iter(iv, &it);
    while (json_next(&it, 0, &e)) {
        if (json_streq(e, "image"))
            m->image = 1;
        else if (!json_streq(e, "text"))
            return fail(c, "input holds \"text\" or \"image\"");
    }
    return 0;
}

/* the fields model definitions and overrides share */
static int rd_common(ctx *c, jv d, llm_model *m, int full)
{
    jv v;
    if (json_get(d, "name", &v)) {
        if (json_type(v) != J_STR)
            return fail(c, "name must be a string");
        m->name = sdup(c, v);
    }
    if (json_get(d, "reasoning", &v)) {
        if (json_type(v) != J_TRUE && json_type(v) != J_FALSE)
            return fail(c, "reasoning must be true or false");
        m->reasoning = json_type(v) == J_TRUE;
    }
    if (json_get(d, "thinkingLevelMap", &v) && rd_tmap(c, v, m))
        return -1;
    if (json_get(d, "input", &v) && rd_input(c, v, m))
        return -1;
    if (json_get(d, "cost", &v) && rd_cost(c, v, m->cost, full))
        return -1;
    if (json_get(d, "contextWindow", &v)) {
        if (json_type(v) != J_NUM || json_long(v, 0) <= 0)
            return fail(c, "invalid contextWindow");
        m->context_window = json_long(v, 0);
    }
    if (json_get(d, "maxTokens", &v)) {
        if (json_type(v) != J_NUM || json_long(v, 0) <= 0)
            return fail(c, "invalid maxTokens");
        m->max_tokens = json_long(v, 0);
    }
    if (json_get(d, "compat", &v) && rd_compat(c, v, m))
        return -1;
    return c->r->oom ? -1 : 0;
}

/* {"Name":"value"} text of a headers object */
static const char *rd_headers(ctx *c, jv h)
{
    jw w;
    jit it;
    jv k, v;
    int n = 0;
    const char *d;
    if (json_type(h) != J_OBJ) {
        fail(c, "headers must be an object");
        return 0;
    }
    jw_init(&w);
    jw_raw(&w, "{", 1);
    json_iter(h, &it);
    while (json_next(&it, &k, &v)) {
        if (json_type(v) != J_STR) {
            jw_free(&w);
            fail(c, "header values are strings");
            return 0;
        }
        jw_rawz(&w, n++ ? "," : "");
        jw_raw(&w, k.p, k.n);
        jw_raw(&w, ":", 1);
        jw_raw(&w, v.p, v.n);
    }
    jw_raw(&w, "}", 1);
    d = w.oom ? 0 : llm_reg_dup(c->r, w.p, w.n);
    jw_free(&w);
    return d;
}

static int add_model(llm_reg *r, const llm_model *m)
{
    if (r->n == r->cap) {
        int nc = r->cap ? r->cap * 2 : 16;
        llm_model *g = (llm_model *)realloc(r->m, (size_t)nc * sizeof(*g));
        if (!g) {
            r->oom = 1;
            return -1;
        }
        r->m = g;
        r->cap = nc;
    }
    r->m[r->n++] = *m;
    return 0;
}

static int provider(ctx *c, const char *pid, jv cfg)
{
    llm_reg *r = c->r;
    jv v, defs, ovr, e;
    jit it;
    const char *base = 0, *api = 0, *key = 0, *hdr = 0;
    int i, has_auth = 0, auth = 0, any = 0;
    if (json_type(cfg) != J_OBJ)
        return fail(c, "a provider must be an object");
    if (json_get(cfg, "baseUrl", &v)) {
        if (json_type(v) != J_STR)
            return fail(c, "baseUrl must be a string");
        base = sdup(c, v);
        any = 1;
    }
    if (json_get(cfg, "api", &v)) {
        if (json_type(v) != J_STR)
            return fail(c, "api must be a string");
        api = sdup(c, v);
    }
    if (json_get(cfg, "apiKey", &v)) {
        if (json_type(v) != J_STR)
            return fail(c, "apiKey must be a string");
        key = sdup(c, v);
        any = 1;
    }
    if (json_get(cfg, "authHeader", &v)) {
        has_auth = 1;
        auth = json_type(v) == J_TRUE;
        any = 1;
    }
    if (json_get(cfg, "headers", &v)) {
        hdr = rd_headers(c, v);
        if (!hdr)
            return -1;
        any = 1;
    }
    if (json_get(cfg, "compat", &v) || json_get(cfg, "modelOverrides", &v))
        any = 1;
    if (json_get(cfg, "models", &v) && json_type(v) == J_ARR && json_count(v) > 0)
        any = 1;
    if (!any) {
        cl_copy(r->err, "Provider ", sizeof(r->err));
        cl_cat(r->err, pid, sizeof(r->err));
        cl_cat(r->err, ": must specify \"baseUrl\", \"headers\", \"compat\", \"modelOverrides\", or \"models\".", sizeof(r->err));
        return -1;
    }
    /* the provider's existing models take the provider-level settings */
    for (i = 0; i < r->n; i++) {
        llm_model *m = &r->m[i];
        if (strcmp(m->provider, pid))
            continue;
        if (base)
            m->base_url = base;
        if (key)
            m->key = key;
        if (hdr)
            m->headers = hdr;
        if (has_auth)
            m->auth_header = auth;
        if (json_get(cfg, "compat", &v) && rd_compat(c, v, m))
            return -1;
    }
    if (json_get(cfg, "models", &defs)) {
        if (json_type(defs) != J_ARR)
            return fail(c, "models must be an array");
        json_iter(defs, &it);
        while (json_next(&it, 0, &e)) {
            llm_model m, *dflt = 0;
            const char *id;
            int at = -1, k;
            if (json_type(e) != J_OBJ || !json_get(e, "id", &v) || json_type(v) != J_STR || v.n <= 2)
                return fail(c, "a model needs a non-empty id");
            id = sdup(c, v);
            if (!id)
                return -1;
            llm_model_init(&m);
            m.id = id;
            m.provider = 0;
            for (k = 0; k < r->n; k++)
                if (!strcmp(r->m[k].provider, pid)) {
                    m.provider = r->m[k].provider;
                    if (!strcmp(r->m[k].id, id))
                        at = k;
                }
            if (!m.provider)
                m.provider = llm_reg_dup(r, pid, (long)strlen(pid));
            /* findModelDefaults: the same id, else a model of the same api, else openai-completions, else any */
            {
                const char *want = 0;
                jv av;
                if (json_get(e, "api", &av) && json_type(av) == J_STR)
                    want = sdup(c, av);
                else
                    want = api;
                for (k = 0; k < r->n && !dflt; k++)
                    if (!strcmp(r->m[k].provider, pid) && !strcmp(r->m[k].id, id))
                        dflt = &r->m[k];
                for (k = 0; k < r->n && !dflt && want; k++)
                    if (!strcmp(r->m[k].provider, pid) && !strcmp(r->m[k].api, want))
                        dflt = &r->m[k];
                for (k = 0; k < r->n && !dflt; k++)
                    if (!strcmp(r->m[k].provider, pid) && !strcmp(r->m[k].api, "openai-completions"))
                        dflt = &r->m[k];
                for (k = 0; k < r->n && !dflt; k++)
                    if (!strcmp(r->m[k].provider, pid))
                        dflt = &r->m[k];
                m.api = want ? want : dflt ? dflt->api : 0;
            }
            if (json_get(e, "baseUrl", &v) && json_type(v) == J_STR)
                m.base_url = sdup(c, v);
            else
                m.base_url = base ? base : dflt ? dflt->base_url : 0;
            if (!m.api) {
                cl_copy(r->err, "Provider ", sizeof(r->err));
                cl_cat(r->err, pid, sizeof(r->err));
                cl_cat(r->err, ", model ", sizeof(r->err));
                cl_cat(r->err, id, sizeof(r->err));
                cl_cat(r->err, ": no \"api\" specified. Set at provider or model level.", sizeof(r->err));
                return -1;
            }
            if (!m.base_url) {
                cl_copy(r->err, "Provider ", sizeof(r->err));
                cl_cat(r->err, pid, sizeof(r->err));
                cl_cat(r->err, ": \"baseUrl\" is required when defining custom models.", sizeof(r->err));
                return -1;
            }
            m.name = id;
            m.context_window = 128000;
            m.max_tokens = 16384;
            m.image = 0;
            /* the provider's settings: this file's, else those the provider already has */
            m.key = key ? key : dflt ? dflt->key : 0;
            m.headers = hdr ? hdr : dflt ? dflt->headers : 0;
            m.auth_header = has_auth ? auth : dflt ? dflt->auth_header : 0;
            if (json_get(cfg, "compat", &v) && rd_compat(c, v, &m))
                return -1;
            if (rd_common(c, e, &m, 1))
                return -1;
            if (at >= 0)
                r->m[at] = m;
            else if (add_model(r, &m))
                return -1;
        }
    }
    if (json_get(cfg, "modelOverrides", &ovr)) {
        jv ok;
        if (json_type(ovr) != J_OBJ)
            return fail(c, "modelOverrides must be an object");
        json_iter(ovr, &it);
        while (json_next(&it, &ok, &e)) {
            char id[160];
            if (json_str(ok, id, sizeof(id)) < 0 || json_type(e) != J_OBJ)
                return fail(c, "an override is an object");
            for (i = 0; i < r->n; i++)
                if (!strcmp(r->m[i].provider, pid) && !strcmp(r->m[i].id, id))
                    if (rd_common(c, e, &r->m[i], 0))
                        return -1;
        }
    }
    return r->oom ? -1 : 0;
}

int llm_reg_overlay(llm_reg *r, const char *text, long n)
{
    ctx c;
    char *b;
    long bn;
    jv root, provs, key, cfg;
    jit it;
    llm_model *save = 0;
    int save_n = r->n, rc = 0;
    r->err[0] = 0;
    c.r = r;
    cl_copy(c.path, "root", sizeof(c.path));
    b = clean(text, n, &bn);
    if (!b) {
        strcpy(r->err, "out of memory");
        return -1;
    }
    if (json_parse(b, bn, &root) || json_type(root) != J_OBJ) {
        strcpy(r->err, "Failed to parse models.json");
        free(b);
        return -1;
    }
    if (!json_get(root, "providers", &provs) || json_type(provs) != J_OBJ) {
        strcpy(r->err, "Invalid models.json schema: providers: required object");
        free(b);
        return -1;
    }
    if (r->n) {
        save = (llm_model *)malloc((size_t)r->n * sizeof(*save));
        if (!save) {
            free(b);
            strcpy(r->err, "out of memory");
            return -1;
        }
        memcpy(save, r->m, (size_t)r->n * sizeof(*save));
    }
    json_iter(provs, &it);
    while (rc == 0 && json_next(&it, &key, &cfg)) {
        char pid[64];
        if (json_str(key, pid, sizeof(pid)) < 0) {
            rc = -1;
            break;
        }
        cl_copy(c.path, "providers.", sizeof(c.path));
        cl_cat(c.path, pid, sizeof(c.path));
        rc = provider(&c, pid, cfg);
    }
    if (rc) {
        if (r->n > save_n || save) {
            /* put the model table back as it was */
            r->n = save_n;
            if (save)
                memcpy(r->m, save, (size_t)save_n * sizeof(*save));
        }
        if (!r->err[0])
            strcpy(r->err, "out of memory");
    }
    free(save);
    free(b);
    return rc;
}
