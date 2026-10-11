/* see registry.h */
#include <stdlib.h>
#include <string.h>
#include "registry.h"
#include "models_builtin.h"

static const char *const tnames[LT_N] = { "off", "minimal", "low", "medium", "high", "xhigh", "max" };

void llm_model_init(llm_model *m)
{
    memset(m, 0, sizeof(*m));
    m->compat.max_tokens_field = m->compat.usage_in_streaming = m->compat.developer_role =
        m->compat.tool_result_name = m->compat.reasoning_effort = m->compat.thinking_as_text = LC_UNSET;
}

void llm_reg_init(llm_reg *r)
{
    memset(r, 0, sizeof(*r));
}

void llm_reg_free(llm_reg *r)
{
    int i;
    for (i = 0; i < r->nown; i++)
        free(r->own[i]);
    free(r->own);
    free(r->m);
    memset(r, 0, sizeof(*r));
}

char *llm_reg_dup(llm_reg *r, const char *s, long n)
{
    char *d;
    if (r->nown == r->owncap) {
        int nc = r->owncap ? r->owncap * 2 : 64;
        char **g = (char **)realloc(r->own, (size_t)nc * sizeof(*g));
        if (!g) {
            r->oom = 1;
            return 0;
        }
        r->own = g;
        r->owncap = nc;
    }
    d = (char *)malloc((size_t)n + 1);
    if (!d) {
        r->oom = 1;
        return 0;
    }
    memcpy(d, s, (size_t)n);
    d[n] = 0;
    r->own[r->nown++] = d;
    return d;
}

int llm_reg_builtin(llm_reg *r)
{
    return llm_reg_overlay(r, llm_models_builtin, (long)strlen(llm_models_builtin));
}

const llm_model *llm_reg_find(const llm_reg *r, const char *provider, const char *id)
{
    int i;
    for (i = 0; i < r->n; i++)
        if (!strcmp(r->m[i].provider, provider) && !strcmp(r->m[i].id, id))
            return &r->m[i];
    return 0;
}

int llm_reg_has_provider(const llm_reg *r, const char *provider)
{
    int i;
    for (i = 0; i < r->n; i++)
        if (!strcmp(r->m[i].provider, provider))
            return 1;
    return 0;
}

/* ---- thinking levels ---- */

const char *llm_think_name(int level)
{
    return level >= 0 && level < LT_N ? tnames[level] : "";
}

int llm_think_of(const char *name)
{
    int i;
    for (i = 0; i < LT_N; i++)
        if (!strcmp(name, tnames[i]))
            return i;
    return -1;
}

unsigned llm_think_levels(const llm_model *m)
{
    unsigned mask = 1u << LT_OFF;
    int i;
    if (!m->reasoning)
        return mask;
    for (i = LT_MINIMAL; i < LT_N; i++) {
        int k = m->thinking_map[i].kind;
        if (k == LTM_NULL)
            continue;
        if ((i == LT_XHIGH || i == LT_MAX) && k == LTM_UNSET)
            continue;
        mask |= 1u << i;
    }
    if (m->thinking_map[LT_OFF].kind == LTM_NULL)
        mask &= ~(1u << LT_OFF);
    return mask;
}

int llm_think_clamp(const llm_model *m, int level)
{
    unsigned av = llm_think_levels(m);
    int i;
    if (level < 0 || level >= LT_N) {
        for (i = 0; i < LT_N; i++)
            if (av & (1u << i))
                return i;
        return LT_OFF;
    }
    if (av & (1u << level))
        return level;
    for (i = level; i < LT_N; i++)
        if (av & (1u << i))
            return i;
    for (i = level - 1; i >= 0; i--)
        if (av & (1u << i))
            return i;
    for (i = 0; i < LT_N; i++)
        if (av & (1u << i))
            return i;
    return LT_OFF;
}

/* ---- resolveCliModel ---- */

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static int ieq(const char *a, const char *b)
{
    while (*a && *b && lower((unsigned char)*a) == lower((unsigned char)*b)) {
        a++;
        b++;
    }
    return !*a && !*b;
}

static int ihas(const char *h, const char *needle)
{
    size_t n = strlen(needle), i;
    if (!n)
        return 1;
    for (i = 0; h[i]; i++) {
        size_t k = 0;
        while (k < n && h[i + k] && lower((unsigned char)h[i + k]) == lower((unsigned char)needle[k]))
            k++;
        if (k == n)
            return 1;
    }
    return 0;
}

static int ref_is(const llm_model *m, const char *ref)
{
    size_t pl = strlen(m->provider);
    char buf[256];
    if (pl + 1 + strlen(m->id) >= sizeof(buf))
        return 0;
    memcpy(buf, m->provider, pl);
    buf[pl] = '/';
    strcpy(buf + pl + 1, m->id);
    return ieq(buf, ref);
}

/* findExactModelReferenceMatch over the models of cand[0..nc): the model, or -1 */
static int exact_ref(const llm_model *all, const int *cand, int nc, const char *ref)
{
    int i, found = -1, cnt = 0;
    const char *sl;
    for (i = 0; i < nc; i++)
        if (ref_is(&all[cand[i]], ref)) {
            found = cand[i];
            cnt++;
        }
    if (cnt == 1)
        return found;
    if (cnt > 1)
        return -1;
    sl = strchr(ref, '/');
    if (sl && sl != ref && sl[1]) {
        char prov[64];
        size_t pl = (size_t)(sl - ref);
        if (pl < sizeof(prov)) {
            memcpy(prov, ref, pl);
            prov[pl] = 0;
            cnt = 0;
            for (i = 0; i < nc; i++)
                if (ieq(all[cand[i]].provider, prov) && ieq(all[cand[i]].id, sl + 1)) {
                    found = cand[i];
                    cnt++;
                }
            if (cnt == 1)
                return found;
            if (cnt > 1)
                return -1;
        }
    }
    cnt = 0;
    for (i = 0; i < nc; i++)
        if (ieq(all[cand[i]].id, ref)) {
            found = cand[i];
            cnt++;
        }
    return cnt == 1 ? found : -1;
}

static int is_alias(const char *id)
{
    size_t n = strlen(id), i;
    if (n >= 7 && !strcmp(id + n - 7, "-latest"))
        return 1;
    if (n >= 9 && id[n - 9] == '-') {
        for (i = n - 8; i < n; i++)
            if (id[i] < '0' || id[i] > '9')
                return 1;
        return 0;
    }
    return 1;
}

/* tryMatchModel */
static int try_match(const llm_model *all, const int *cand, int nc, const char *pat)
{
    int i, best_a = -1, best_d = -1;
    int ex = exact_ref(all, cand, nc, pat);
    if (ex >= 0)
        return ex;
    for (i = 0; i < nc; i++) {
        const llm_model *m = &all[cand[i]];
        if (!(ihas(m->id, pat) || (m->name && ihas(m->name, pat))))
            continue;
        if (is_alias(m->id)) {
            if (best_a < 0 || strcmp(m->id, all[best_a].id) > 0)
                best_a = cand[i];
        } else if (best_d < 0 || strcmp(m->id, all[best_d].id) > 0)
            best_d = cand[i];
    }
    return best_a >= 0 ? best_a : best_d;
}

/* parseModelPattern with allowInvalidThinkingLevelFallback false */
static int parse_pattern(const llm_model *all, const int *cand, int nc, const char *pat, int *level)
{
    const char *colon;
    char prefix[256];
    int lv, m = try_match(all, cand, nc, pat);
    if (m >= 0) {
        *level = -1;
        return m;
    }
    colon = strrchr(pat, ':');
    if (!colon || (size_t)(colon - pat) >= sizeof(prefix))
        return -1;
    lv = llm_think_of(colon + 1);
    if (lv < 0)
        return -1;
    memcpy(prefix, pat, (size_t)(colon - pat));
    prefix[colon - pat] = 0;
    m = parse_pattern(all, cand, nc, prefix, level);
    if (m >= 0)
        *level = lv;
    return m;
}

int llm_reg_resolve(llm_reg *r, const char *cli_provider, const char *pattern, llm_model *out, int *level,
                    char *err, long errcap)
{
    int *cand = 0, nc = 0, i, m = -1, lv = -1, inferred = 0;
    char prov[64], pat[256];
    const char *p = 0;
    err[0] = 0;
    *level = -1;
    if (r->n == 0) {
        strncpy(err, "No models available. Check your installation or add models to models.json.", (size_t)errcap - 1);
        err[errcap - 1] = 0;
        return -1;
    }
    cand = (int *)malloc((size_t)r->n * sizeof(int));
    if (!cand)
        return -1;
    pat[0] = 0;
    strncpy(pat, pattern, sizeof(pat) - 1);
    pat[sizeof(pat) - 1] = 0;
    if (cli_provider) {
        for (i = 0; i < r->n; i++)
            if (ieq(r->m[i].provider, cli_provider)) {
                p = r->m[i].provider;
                break;
            }
        if (!p) {
            strncpy(err, "Unknown provider \"", (size_t)errcap - 1);
            strncat(err, cli_provider, (size_t)errcap - strlen(err) - 1);
            strncat(err, "\". Use --list-models to see available providers/models.", (size_t)errcap - strlen(err) - 1);
            free(cand);
            return -1;
        }
    }
    if (!p) {
        const char *sl = strchr(pattern, '/');
        if (sl && (size_t)(sl - pattern) < sizeof(prov)) {
            memcpy(prov, pattern, (size_t)(sl - pattern));
            prov[sl - pattern] = 0;
            for (i = 0; i < r->n; i++)
                if (ieq(r->m[i].provider, prov)) {
                    p = r->m[i].provider;
                    strncpy(pat, sl + 1, sizeof(pat) - 1);
                    pat[sizeof(pat) - 1] = 0;
                    inferred = 1;
                    break;
                }
        }
    }
    if (!p) {
        /* a bare exact id or provider/id across all models */
        int hits = 0, hit = -1;
        for (i = 0; i < r->n; i++)
            if (ieq(r->m[i].id, pattern) || ref_is(&r->m[i], pattern)) {
                hits++;
                hit = i;
            }
        if (hits == 1) {
            *out = r->m[hit];
            free(cand);
            return 0;
        }
        if (hits > 1) {
            strncpy(err, "Model \"", (size_t)errcap - 1);
            strncat(err, pattern, (size_t)errcap - strlen(err) - 1);
            strncat(err, "\" is ambiguous across providers. Use --provider or provider/model.",
                    (size_t)errcap - strlen(err) - 1);
            free(cand);
            return -1;
        }
    }
    for (i = 0; i < r->n; i++)
        if (!p || !strcmp(r->m[i].provider, p))
            cand[nc++] = i;
    m = parse_pattern(r->m, cand, nc, pat, &lv);
    if (m < 0 && inferred) {
        /* the whole input as a raw id on any provider (an id with a slash in it) */
        for (nc = 0, i = 0; i < r->n; i++)
            cand[nc++] = i;
        m = try_match(r->m, cand, nc, pattern);
        lv = -1;
        if (m < 0)
            m = parse_pattern(r->m, cand, nc, pattern, &lv);
    }
    if (m >= 0) {
        *out = r->m[m];
        *level = lv;
        free(cand);
        return 0;
    }
    if (p) {
        /* buildFallbackModel: a known provider, an id the catalog lacks: the provider's first model under that id */
        char *id;
        for (i = 0; i < r->n; i++)
            if (!strcmp(r->m[i].provider, p))
                break;
        if (i < r->n) {
            const char *colon = strrchr(pat, ':');
            long idl = (long)strlen(pat);
            lv = -1;
            if (colon && llm_think_of(colon + 1) >= 0) {
                lv = llm_think_of(colon + 1);
                idl = (long)(colon - pat);
            }
            id = llm_reg_dup(r, pat, idl);
            if (id) {
                *out = r->m[i];
                out->id = id;
                out->name = id;
                *level = lv;
                free(cand);
                return 0;
            }
        }
    }
    strncpy(err, "Model \"", (size_t)errcap - 1);
    strncat(err, pattern, (size_t)errcap - strlen(err) - 1);
    strncat(err, "\" not found. Use --list-models to see available models.", (size_t)errcap - strlen(err) - 1);
    free(cand);
    return -1;
}
