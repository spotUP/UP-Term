/* conv -- see conv.h. */
#include <stdlib.h>
#include <string.h>
#include "conv.h"
#include "util.h"

void conv_init(cl_conv *c)
{
    memset(c, 0, sizeof(*c));
}

void conv_clear(cl_conv *c)
{
    int i;
    for (i = 0; i < c->n; i++)
        free(c->m[i].json);
    c->n = 0;
}

void conv_usage_reset(cl_conv *c)
{
    c->requests = 0;
    c->in_tok = c->out_tok = c->cache_w = c->cache_r = 0;
    c->cost_micro = 0;
    c->unpriced = 0;
    c->nmu = 0;
    memset(c->mu, 0, sizeof(c->mu));
}

void conv_free(cl_conv *c)
{
    conv_clear(c);
    free(c->m);
    conv_init(c);
}

int conv_add(cl_conv *c, int user, const char *json, long n)
{
    char *j;
    if (c->n == c->cap) {
        int nc = c->cap ? c->cap * 2 : 16;
        cl_msg *m = (cl_msg *)realloc(c->m, (size_t)nc * sizeof(cl_msg));
        if (!m)
            return -1;
        c->m = m;
        c->cap = nc;
    }
    j = (char *)malloc((size_t)n + 1);
    if (!j)
        return -1;
    memcpy(j, json, (size_t)n);
    j[n] = 0;
    c->m[c->n].user = user;
    c->m[c->n].json = j;
    c->m[c->n].n = n;
    c->n++;
    return 0;
}

int conv_add_user_blocks(cl_conv *c, const char *b, long bn)
{
    int rc;
    if (c->n && c->m[c->n - 1].user) {
        cl_msg *m = &c->m[c->n - 1];
        char *j = (char *)realloc(m->json, (size_t)(m->n + bn + 2));
        if (!j)
            return -1;
        m->json = j;
        /* "[...]" -> "[..., new]" */
        j[m->n - 1] = ',';
        memcpy(j + m->n, b, (size_t)bn);
        m->n += bn;
        j[m->n++] = ']';
        j[m->n] = 0;
        return 0;
    }
    {
        jw a;
        jw_init(&a);
        jw_raw(&a, "[", 1);
        jw_raw(&a, b, bn);
        jw_raw(&a, "]", 1);
        rc = a.oom ? -1 : conv_add(c, 1, a.p, a.n);
        jw_free(&a);
    }
    return rc;
}

int conv_add_user_text(cl_conv *c, const char *s, long n)
{
    jw w;
    int rc;
    jw_init(&w);
    jw_rawz(&w, "{\"type\":\"text\",\"text\":");
    jw_str(&w, s, n);
    jw_raw(&w, "}", 1);
    rc = w.oom ? -1 : conv_add_user_blocks(c, w.p, w.n);
    jw_free(&w);
    return rc;
}

cl_mark conv_mark(const cl_conv *c)
{
    cl_mark m;
    m.n = c->n;
    m.last = c->n ? c->m[c->n - 1].n : 0;
    return m;
}

void conv_rollback(cl_conv *c, cl_mark m)
{
    while (c->n > m.n)
        free(c->m[--c->n].json);
    if (c->n && c->m[c->n - 1].n > m.last) {
        cl_msg *x = &c->m[c->n - 1];
        x->n = m.last;
        x->json[x->n - 1] = ']';
        x->json[x->n] = 0;
    }
}

static int fam_ver(const char *id, int *ver);

int conv_caps(const char *model)
{
    int ver, fam;
    /* Haiku 4.5 (and the 3.x models) take neither an effort nor adaptive
     * thinking (theirs is budget_tokens); every current 4.6+ / 5.x model
     * takes both (claude-api skill, model notes 2026-09-25) */
    if (!strncmp(model, "claude-haiku", 12) || !strncmp(model, "claude-3", 8))
        return 0;
    /* Claude Code's model-config: "You can't turn thinking off on Opus 5.5,
     * Sonnet 5.5, or the Fable models" */
    fam = fam_ver(model, &ver);
    if (fam == 'f' || ((fam == 'o' || fam == 's') && ver >= 55))
        return CAP_EFFORT | CAP_ADAPTIVE | CAP_THINK_ALWAYS;
    return CAP_EFFORT | CAP_ADAPTIVE;
}

/* a model id's family and version (maj*10+min): "claude-opus-4-7" ->
 * 'o', 47; a dated id's date is not a version; 0 not a Claude id */
static int fam_ver(const char *id, int *ver)
{
    static const char *const fam[] = { "haiku", "sonnet", "opus", "fable", 0 };
    int i;
    *ver = 0;
    if (strncmp(id, "claude-", 7))
        return 0;
    for (i = 0; fam[i]; i++) {
        long l = (long)strlen(fam[i]);
        const char *v = id + 7 + l;
        if (strncmp(id + 7, fam[i], (size_t)l) || v[0] != '-' || v[1] < '0' || v[1] > '9')
            continue;
        *ver = (v[1] - '0') * 10;
        if (v[2] == '-' && v[3] >= '0' && v[3] <= '9' && (v[4] == 0 || v[4] == '-' || v[4] == '['))
            *ver += v[3] - '0';
        return fam[i][0];
    }
    return 0;
}

int conv_advisor_ok(const char *main, const char *adv)
{
    int mv, av, mf = fam_ver(main, &mv), af = fam_ver(adv, &av);
    if (!mf || !af)
        return -1;                  /* a model Claude Code does not know: not attached */
    /* the main models that take an advisor: Fable, Opus 4.6+, Sonnet 4.6+, Haiku 4.5 */
    if ((mf == 'o' && mv < 46) || (mf == 's' && mv < 46) || (mf == 'h' && mv < 45))
        return -1;
    if (af == 'h')
        return -1;                  /* Haiku can call an advisor, not be one */
    /* Claude Code's pairing table (advisor.md "Choose an advisor model") */
    if (mf == 'f')
        return af == 'f' && (mv >= 51 ? av >= 51 : av >= 50) ? 0 : 1;
    if (af == 'f')
        return 0;
    if (mf == 'h' || (mf == 's' && mv < 50))
        return 0;                   /* Haiku 4.5, Sonnet 4.6: any Opus or Sonnet */
    if (mf == 'o' && mv < 47)
        return af == 'o' || av >= 50 ? 0 : 1;                       /* Opus 4.6 */
    if (mf == 's' && mv < 55)
        return (af == 'o' && av >= 47) || (af == 's' && av >= 50) ? 0 : 1;    /* Sonnet 5 */
    if (mf == 'o' && mv < 50)
        return (af == 'o' && av >= 47) || (af == 's' && av >= 55) ? 0 : 1;    /* Opus 4.7, 4.8 */
    if (mf == 's')
        return (af == 'o' && av >= 50) || (af == 's' && av >= 55) ? 0 : 1;    /* Sonnet 5.5 */
    return af == 'o' && av >= 50 ? 0 : 1;                           /* Opus 5, 5.5 */
}

const char *conv_beta(const char *model)
{
    static const char *const fb[] = {
        "claude-opus-5-5", "claude-opus-5", "claude-fable-5-1", "claude-fable-5", "claude-sonnet-5-5", 0
    };
    int i;
    for (i = 0; fb[i]; i++)
        if (!strcmp(model, fb[i]))
            return "server-side-fallback-2026-07-01";
    return "";
}

static void messages(const cl_conv *c, jw *out)
{
    int i;
    jw_raw(out, "[", 1);
    for (i = 0; i < c->n; i++) {
        if (i)
            jw_raw(out, ",", 1);
        jw_rawz(out, c->m[i].user ? "{\"role\":\"user\",\"content\":" : "{\"role\":\"assistant\",\"content\":");
        jw_raw(out, c->m[i].json, c->m[i].n);
        jw_raw(out, "}", 1);
    }
    jw_raw(out, "]", 1);
}

int conv_body(const cl_conv *c, const cl_opts *o, jw *out)
{
    jw_rawz(out, "{\"model\":");
    jw_strz(out, o->model);
    jw_rawz(out, ",\"max_tokens\":");
    jw_long(out, o->max_tokens);
    jw_rawz(out, ",\"stream\":true");
    /* adaptive thinking with its text summarised (the thinking view shows
     * it; the 5.x models leave it empty by default) */
    {
        int caps = conv_caps(o->model);
        /* Alt+T's thinking off (A4 gaps 3): disabled where the model allows
         * it, and then an effort above high goes as high (Claude Code: some
         * models refuse xhigh / max without thinking) */
        int off = o->think_off && (caps & CAP_ADAPTIVE) && !(caps & CAP_THINK_ALWAYS);
        if (off && !o->no_thinking)
            jw_rawz(out, ",\"thinking\":{\"type\":\"disabled\"}");
        else if ((caps & CAP_ADAPTIVE) && !o->no_thinking)  /* CLAUDE_CODE_DISABLE_THINKING */
            jw_rawz(out, ",\"thinking\":{\"type\":\"adaptive\",\"display\":\"summarized\"}");
        if (o->effort && *o->effort && (caps & CAP_EFFORT)) {
            jw_rawz(out, ",\"output_config\":{\"effort\":");
            jw_strz(out, off && (!strcmp(o->effort, "xhigh") || !strcmp(o->effort, "max")) ? "high" : o->effort);
            jw_raw(out, "}", 1);
        }
    }
    if (*conv_beta(o->model))
        jw_rawz(out, ",\"fallbacks\":\"default\"");
    jw_rawz(out, ",\"cache_control\":{\"type\":\"ephemeral\"}");
    if (o->system && *o->system) {
        jw_rawz(out, ",\"system\":[{\"type\":\"text\",\"text\":");
        jw_strz(out, o->system);
        jw_rawz(out, ",\"cache_control\":{\"type\":\"ephemeral\"}}]");
    }
    if (o->tools && *o->tools && strcmp(o->tools, "[]")) {
        jw_rawz(out, ",\"tools\":");
        jw_rawz(out, o->tools);
        jw_rawz(out, o->no_tools ? ",\"tool_choice\":{\"type\":\"none\"}" : ",\"tool_choice\":{\"type\":\"auto\"}");
    }
    jw_rawz(out, ",\"messages\":");
    messages(c, out);
    if (o->extra && *o->extra) {
        /* CLAUDE_CODE_EXTRA_BODY: its members merged into the top level */
        jv x, k, v;
        jit it;
        if (json_parse(o->extra, (long)strlen(o->extra), &x) == 0 && json_type(x) == J_OBJ) {
            json_iter(x, &it);
            while (json_next(&it, &k, &v)) {
                jw_raw(out, ",", 1);
                jw_raw(out, k.p, k.n);
                jw_raw(out, ":", 1);
                jw_raw(out, v.p, v.n);
            }
        }
    }
    jw_raw(out, "}", 1);
    return out->oom ? -1 : 0;
}

int conv_messages(const cl_conv *c, jw *out)
{
    messages(c, out);
    return out->oom ? -1 : 0;
}

int conv_price(const char *model, cl_price *p)
{
    static const struct {
        const char *m;
        long in, out, cread, cwrite;
    } t[] = {
        { "claude-opus-5-5", 400, 2000, 20, 500 },
        { "claude-opus-5", 500, 2500, 50, 625 },
        { "claude-sonnet-5-5", 200, 1000, 20, 250 },
        { "claude-haiku-4-5", 100, 500, 10, 125 },
        { 0, 0, 0, 0, 0 }
    };
    int i;
    for (i = 0; t[i].m; i++) {
        size_t l = strlen(t[i].m);
        /* the id itself, or its dated snapshot ("claude-haiku-4-5-20251001") */
        if (strncmp(model, t[i].m, l) || (model[l] && (model[l] != '-' || strlen(model + l + 1) != 8 ||
                                                       model[l + 1] < '0' || model[l + 1] > '9')))
            continue;
        p->in = t[i].in;
        p->out = t[i].out;
        p->cread = t[i].cread;
        p->cwrite = t[i].cwrite;
        return 1;
    }
    memset(p, 0, sizeof(*p));
    return 0;
}

/* tokens * price (dollars per million * 100) / 100 = micro-dollars */
static unsigned long micro(long tok, long price)
{
    unsigned long t = tok > 0 ? (unsigned long)tok : 0;
    /* split so the product stays in 32 bits for any token count */
    return (t / 100) * (unsigned long)price + (t % 100) * (unsigned long)price / 100;
}

void conv_usage(cl_conv *c, const char *model, long in, long out, long cache_w, long cache_r)
{
    cl_price p;
    cl_model_use *u;
    unsigned long cost = 0;
    int i;
    c->requests++;
    c->in_tok += in;
    c->out_tok += out;
    c->cache_w += cache_w;
    c->cache_r += cache_r;
    if (conv_price(model, &p)) {
        cost = micro(in, p.in) + micro(out, p.out) + micro(cache_w, p.cwrite) + micro(cache_r, p.cread);
        c->cost_micro += cost;
    } else
        c->unpriced++;
    for (i = 0; i < c->nmu && strcmp(c->mu[i].model, model); i++)
        ;
    if (i == c->nmu && c->nmu < CONV_MODELS)
        cl_copy(c->mu[c->nmu++].model, model, sizeof(c->mu[0].model));
    u = &c->mu[i < c->nmu ? i : c->nmu - 1];
    u->in += in;
    u->out += out;
    u->cache_w += cache_w;
    u->cache_r += cache_r;
    u->cost_micro += cost;
}

void conv_dollars(unsigned long m, char *out, long cap)
{
    char b[16], f[8];
    unsigned long frac = (m % 1000000UL + 50) / 100, whole = m / 1000000UL;
    int i, l;
    if (frac >= 10000) {
        frac -= 10000;
        whole++;
    }
    cl_copy(out, "$", cap);
    cl_ltoa((long)whole, b);
    cl_cat(out, b, cap);
    cl_cat(out, ".", cap);
    l = cl_ltoa((long)frac, b);
    for (i = 0; i < 4 - l; i++)
        f[i] = '0';
    f[i] = 0;
    cl_cat(out, f, cap);
    cl_cat(out, b, cap);
}
