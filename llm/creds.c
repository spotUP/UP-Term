/* see creds.h */
#include <stdlib.h>
#include <string.h>
#include "creds.h"
#include "../claude/util.h"

void llm_cfg_init(llm_cfg *c)
{
    memset(c, 0, sizeof(*c));
}

void llm_cfg_free(llm_cfg *c)
{
    int i;
    for (i = 0; i < c->ncache; i++) {
        free(c->cache[i].cmd);
        free(c->cache[i].val);
    }
    memset(c->runtime_key, 0, sizeof(c->runtime_key));
    c->ncache = 0;
}

static int name_start(int ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

static int name_char(int ch)
{
    return name_start(ch) || (ch >= '0' && ch <= '9');
}

static long put(char *out, long cap, long n, const char *s, long sn)
{
    if (n + sn >= cap)
        return -1;
    memcpy(out + n, s, (size_t)sn);
    return n + sn;
}

static long run_cached(llm_cfg *c, const char *cmd, char *out, long cap)
{
    int i;
    char tmp[1024];
    long n;
    for (i = 0; i < c->ncache; i++)
        if (!strcmp(c->cache[i].cmd, cmd)) {
            long l;
            if (!c->cache[i].val)
                return -1;
            l = (long)strlen(c->cache[i].val);
            if (l >= cap)
                return -1;
            memcpy(out, c->cache[i].val, (size_t)l + 1);
            return l;
        }
    n = c->run ? c->run(c->u, cmd, tmp, sizeof(tmp)) : -1;
    c->runs++;
    if (c->ncache < LLM_CMDS) {
        c->cache[c->ncache].cmd = (char *)malloc(strlen(cmd) + 1);
        if (c->cache[c->ncache].cmd) {
            strcpy(c->cache[c->ncache].cmd, cmd);
            c->cache[c->ncache].val = 0;
            if (n >= 0) {
                c->cache[c->ncache].val = (char *)malloc((size_t)n + 1);
                if (c->cache[c->ncache].val) {
                    memcpy(c->cache[c->ncache].val, tmp, (size_t)n);
                    c->cache[c->ncache].val[n] = 0;
                }
            }
            c->ncache++;
        }
    }
    if (n < 0 || n >= cap)
        return -1;
    memcpy(out, tmp, (size_t)n);
    out[n] = 0;
    return n;
}

/* an environment variable appended at out[n]: its length; -1 when it is unset or empty (pi: `env[name] || undefined`),
 * -2 when it does not fit. getenv cuts a long value to the room it is given, so a value that fills the room is taken
 * as cut: an error, never half a key */
static long env_into(llm_cfg *c, const char *name, char *out, long cap, long n)
{
    long v;
    if (!c->getenv)
        return -1;
    if (cap - n < 2)
        return -2;
    v = c->getenv(c->u, name, out + n, cap - n);
    if (v <= 0)
        return -1;
    return v >= cap - n - 1 ? -2 : v;
}

/* the env variables a value names that are unset or empty, each once, in order (pi: getMissingConfigValueEnvVarNames) */
typedef struct miss {
    char name[8][96];
    int n, extra;
} miss;

static void miss_note(miss *m, const char *nm)
{
    int i;
    for (i = 0; i < m->n; i++)
        if (!strcmp(m->name[i], nm))
            return;
    if (m->n < 8)
        strcpy(m->name[m->n++], nm);
    else
        m->extra = 1;
}

/* the value expanded; with miss, an unset variable is noted and counts as empty instead of failing the value */
static long expand(llm_cfg *c, const char *cfg, char *out, long cap, miss *ms)
{
    long n = 0;
    const char *p = cfg;
    if (cap < 1)
        return -1;
    if (cfg[0] == '!')
        return run_cached(c, cfg + 1, out, cap);
    while (*p) {
        if (*p != '$') {
            if ((n = put(out, cap, n, p, 1)) < 0)
                return -1;
            p++;
            continue;
        }
        if (p[1] == '$' || p[1] == '!') {
            if ((n = put(out, cap, n, p + 1, 1)) < 0)
                return -1;
            p += 2;
        } else if (p[1] == '{') {
            const char *e = strchr(p + 2, '}');
            char nm[96];
            long l, v;
            int ok = 1;
            const char *q;
            if (!e) {
                if ((n = put(out, cap, n, "$", 1)) < 0)
                    return -1;
                p++;
                continue;
            }
            l = (long)(e - (p + 2));
            if (l < 1 || l >= (long)sizeof(nm) || !name_start((unsigned char)p[2]))
                ok = 0;
            for (q = p + 2; ok && q < e; q++)
                if (!name_char((unsigned char)*q))
                    ok = 0;
            if (!ok) {
                if ((n = put(out, cap, n, p, (long)(e + 1 - p))) < 0)
                    return -1;
                p = e + 1;
                continue;
            }
            memcpy(nm, p + 2, (size_t)l);
            nm[l] = 0;
            v = env_into(c, nm, out, cap, n);
            if (v == -1 && ms)
                miss_note(ms, nm);
            if (v == -1 && ms)
                v = 0;
            if (v < 0)
                return -1;
            n += v;
            p = e + 1;
        } else if (name_start((unsigned char)p[1])) {
            char nm[96];
            long l = 0, v;
            const char *q = p + 1;
            while (name_char((unsigned char)*q) && l < (long)sizeof(nm) - 1)
                nm[l++] = *q++;
            nm[l] = 0;
            v = env_into(c, nm, out, cap, n);
            if (v == -1 && ms)
                miss_note(ms, nm);
            if (v == -1 && ms)
                v = 0;
            if (v < 0)
                return -1;
            n += v;
            p = q;
        } else {
            if ((n = put(out, cap, n, "$", 1)) < 0)
                return -1;
            p++;
        }
    }
    out[n] = 0;
    return n;
}

long llm_cfg_value(llm_cfg *c, const char *cfg, char *out, long cap)
{
    return expand(c, cfg, out, cap, 0);
}

long llm_cfg_value_strict(llm_cfg *c, const char *cfg, const char *what, char *out, long cap, char *err, long errcap)
{
    miss ms;
    long n;
    int i;
    memset(&ms, 0, sizeof(ms));
    err[0] = 0;
    if (cfg[0] == '!') {
        /* pi's resolveConfigValueUncached: the command runs every time; no output, a failure or a timeout is none */
        n = c->run && cap > 0 ? c->run(c->u, cfg + 1, out, cap) : -1;
        c->runs++;
        if (n > 0) {
            out[n] = 0;
            return n;
        }
        cl_copy(err, "Failed to resolve ", errcap);
        cl_cat(err, what, errcap);
        cl_cat(err, " from shell command: ", errcap);
        cl_cat(err, cfg + 1, errcap);
        return -1;
    }
    n = expand(c, cfg, out, cap, &ms);
    if (n >= 0 && !ms.n)
        return n;
    cl_copy(err, "Failed to resolve ", errcap);
    cl_cat(err, what, errcap);
    if (ms.n) {
        cl_cat(err, ms.n == 1 && !ms.extra ? " from environment variable: " : " from environment variables: ", errcap);
        for (i = 0; i < ms.n; i++) {
            if (i)
                cl_cat(err, ", ", errcap);
            cl_cat(err, ms.name[i], errcap);
        }
    }
    return -1;
}

void llm_key_env_name(const char *provider, char *out, long cap)
{
    long i, n = 0;
    /* env-api-keys.ts: the two providers whose variable is not <PROVIDER>_API_KEY */
    if (!strcmp(provider, "azure")) {
        cl_copy(out, "AZURE_OPENAI_API_KEY", cap);
        return;
    }
    if (!strcmp(provider, "google")) {
        cl_copy(out, "GEMINI_API_KEY", cap);
        return;
    }
    if (!strcmp(provider, "amazon-bedrock")) {
        cl_copy(out, "AWS_BEARER_TOKEN_BEDROCK", cap);
        return;
    }
    for (i = 0; provider[i] && n < cap - 9; i++) {
        char ch = provider[i];
        out[n++] = ch == '-' ? '_' : ch >= 'a' && ch <= 'z' ? (char)(ch - 32) : ch;
    }
    out[n] = 0;
    strcat(out, "_API_KEY");
}

/* the six providers pi 1.1.0 lets a subscription log into besides Anthropic (ai/src/providers/<provider>.ts oauth entries).
 * Anthropic is not among them on purpose: a stored Anthropic OAuth entry gives no credential (owner decision 2026-10-07) */
int llm_oauth_provider(const char *provider)
{
    static const char *const ids[] = { "openai", "openai-codex", "github-copilot", "kimi-coding", "xai", "meta" };
    unsigned i;
    for (i = 0; i < sizeof(ids) / sizeof(ids[0]); i++)
        if (provider && !strcmp(provider, ids[i]))
            return 1;
    return 0;
}

/* the provider's entry of auth.json: 1 with *e, 0 none */
static int auth_entry(llm_cfg *c, const char *provider, jv *e)
{
    jv root;
    jit it;
    jv key;
    if (!c->auth || json_parse(c->auth, c->auth_n, &root) || json_type(root) != J_OBJ)
        return 0;
    json_iter(root, &it);
    while (json_next(&it, &key, e)) {
        char nm[64];
        if (json_str(key, nm, sizeof(nm)) >= 0 && !strcmp(nm, provider))
            return 1;
    }
    return 0;
}

/* auth.json: {"<provider>":{"type":"api_key","key":"<config value>"}} or pi's {"type":"oauth","access":...}.
 * A stored credential owns the provider (pi-ai auth/resolve.js): -2 nothing is stored for it, else the key's length,
 * or -1 when the entry cannot give one (a command that fails, another credential type, an empty key, an OAuth entry of
 * a provider with no OAuth here); the caller does not fall back to models.json or the environment after a -1. An OAuth
 * entry's access token is the credential as it is: not a config value, no $ or ! in it is read. */
static long auth_key(llm_cfg *c, const char *provider, char *out, long cap)
{
    jv e, t, k;
    char *raw;
    long n, r;
    if (!auth_entry(c, provider, &e))
        return -2;
    if (json_type(e) == J_OBJ && json_get(e, "type", &t) && json_streq(t, "oauth")) {
        if (!llm_oauth_provider(provider) || !json_get(e, "access", &k) || json_type(k) != J_STR)
            return -1;
        n = json_str(k, out, cap);
        return n > 0 && n < cap ? n : -1;
    }
    if (json_type(e) != J_OBJ || !json_get(e, "type", &t) || !json_streq(t, "api_key") || !json_get(e, "key", &k))
        return -1;
    raw = json_strdup(k, &n);
    if (!raw)
        return -1;
    r = n ? llm_cfg_value(c, raw, out, cap) : -1;
    free(raw);
    return r > 0 ? r : -1;
}

static int b64val(int ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A';
    if (ch >= 'a' && ch <= 'z')
        return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9')
        return ch - '0' + 52;
    if (ch == '-' || ch == '+')
        return 62;
    if (ch == '_' || ch == '/')
        return 63;
    return -1;
}

long llm_b64url_decode(const char *s, long n, unsigned char *out, long cap)
{
    long i, o = 0;
    unsigned long acc = 0;
    int bits = 0;
    for (i = 0; i < n && s[i] != '='; i++) {
        int v = b64val((unsigned char)s[i]);
        if (v < 0)
            return -1;
        acc = (acc << 6) | (unsigned long)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (o >= cap)
                return -1;
            out[o++] = (unsigned char)((acc >> bits) & 0xff);
        }
    }
    return o;
}

/* a JWT's payload (its second part) decoded into out, terminated: its length, -1 not a JWT */
static long jwt_payload(const char *tok, char *out, long cap)
{
    const char *a = strchr(tok, '.'), *b;
    long n;
    if (!a)
        return -1;
    a++;
    b = strchr(a, '.');
    if (!b || strchr(b + 1, '.'))
        return -1;
    n = llm_b64url_decode(a, (long)(b - a), (unsigned char *)out, cap - 1);
    if (n < 0)
        return -1;
    out[n] = 0;
    return n;
}

int llm_jwt_account_id(const char *token, char *out, long cap)
{
    char pl[2048];
    jv v, a, id;
    long n = jwt_payload(token, pl, sizeof(pl));
    if (n < 0 || json_parse(pl, n, &v) || json_type(v) != J_OBJ || !json_get(v, "https://api.openai.com/auth", &a) ||
        json_type(a) != J_OBJ || !json_get(a, "chatgpt_account_id", &id) || json_type(id) != J_STR)
        return -1;
    n = json_str(id, out, cap);
    return n > 0 && n < cap ? 0 : -1;
}

/* getBaseUrlFromToken / getGitHubCopilotBaseUrl (auth/oauth/github-copilot.ts :69-87) */
static void copilot_base(jv access, const char *enterprise, char *out, long cap)
{
    const char *p = 0;
    long i;
    for (i = 0; i + 9 <= access.n; i++)
        if (!memcmp(access.p + i, "proxy-ep=", 9)) {
            p = access.p + i + 9;
            break;
        }
    if (p) {
        long n = 0;
        char host[200];
        while (p < access.p + access.n && *p != ';' && *p != '"' && n < (long)sizeof(host) - 1) {
            host[n] = *p++;
            n++;
        }
        host[n] = 0;
        if (n) {
            cl_copy(out, "https://", cap);
            cl_cat(out, !strncmp(host, "proxy.", 6) ? "api." : "", cap);
            cl_cat(out, !strncmp(host, "proxy.", 6) ? host + 6 : host, cap);
            return;
        }
    }
    if (enterprise && *enterprise) {
        cl_copy(out, "https://copilot-api.", cap);
        cl_cat(out, enterprise, cap);
        return;
    }
    cl_copy(out, "https://api.individual.githubcopilot.com", cap);
}

int llm_cfg_oauth(llm_cfg *c, const llm_model *m, llm_oauth_use *u)
{
    jv e, t, k, access;
    char ent[160];
    memset(u, 0, sizeof(*u));
    if (!llm_oauth_provider(m->provider) || !auth_entry(c, m->provider, &e) || json_type(e) != J_OBJ ||
        !json_get(e, "type", &t) || !json_streq(t, "oauth") || !json_get(e, "access", &access) ||
        json_type(access) != J_STR || access.n <= 2)
        return 0;
    ent[0] = 0;
    if (json_get(e, "enterpriseUrl", &k) && json_type(k) == J_STR)
        json_str(k, ent, sizeof(ent));
    if (!strcmp(m->provider, "github-copilot")) {
        /* toAuth: the base URL comes with the Copilot token; Anthropic-messages through Copilot is Bearer auth */
        char dom[160];
        long i, j = 0;
        /* copilotEnterpriseDomain: normalizeDomain(enterpriseUrl), the host of the URL */
        {
            const char *h = strstr(ent, "://");
            h = h ? h + 3 : ent;
            for (i = 0; h[i] && h[i] != '/' && h[i] != ':' && h[i] != '?' && h[i] != '#' && j < (long)sizeof(dom) - 1; i++)
                dom[j++] = (char)(h[i] >= 'A' && h[i] <= 'Z' ? h[i] + 32 : h[i]);
            dom[j] = 0;
        }
        copilot_base(access, dom, u->base, sizeof(u->base));
        u->copilot = 1;
        u->bearer = m->api && !strcmp(m->api, "anthropic-messages");
    } else if (!strcmp(m->provider, "kimi-coding"))
        u->bearer = 1;              /* toAuth: headers {Authorization: Bearer ...}, no apiKey */
    return 1;
}

/* copilot's filterModels: an OAuth credential with availableModelIds lists the models the account has */
int llm_cfg_model_ok(llm_cfg *c, const llm_model *m)
{
    jv e, t, a, it_v;
    jit it;
    int all = 1, any = 0;
    if (strcmp(m->provider, "github-copilot") || !auth_entry(c, m->provider, &e) || json_type(e) != J_OBJ ||
        !json_get(e, "type", &t) || !json_streq(t, "oauth") || !json_get(e, "availableModelIds", &a) ||
        json_type(a) != J_ARR)
        return 1;
    json_iter(a, &it);
    while (json_next(&it, 0, &it_v))
        if (json_type(it_v) != J_STR)
            all = 0;
    if (!all)
        return 1;
    json_iter(a, &it);
    while (json_next(&it, 0, &it_v))
        if (json_streq(it_v, m->id))
            any = 1;
    return any;
}

static long cfg_key(llm_cfg *c, const llm_model *m, char *out, long cap, const char **src)
{
    long n;
    char nm[96];
    if (src)
        *src = 0;
    if (c->runtime_key[0] && (!c->runtime_provider[0] || !strcmp(c->runtime_provider, m->provider))) {
        n = (long)strlen(c->runtime_key);
        if (n < cap) {
            memcpy(out, c->runtime_key, (size_t)n + 1);
            if (src)
                *src = "--api-key";
            return n;
        }
    }
    n = auth_key(c, m->provider, out, cap);
    if (n > 0) {
        if (src)
            *src = "auth.json";
        return n;
    }
    if (n == -1) {              /* a stored credential owns the provider: no fallback */
        out[0] = 0;
        return 0;
    }
    if (m->key && (n = llm_cfg_value(c, m->key, out, cap)) > 0) {
        if (src)
            *src = "models.json";
        return n;
    }
    llm_key_env_name(m->provider, nm, sizeof(nm));
    if ((n = env_into(c, nm, out, cap, 0)) > 0) {
        if (src)
            *src = "environment";
        return n;
    }
    out[0] = 0;
    return 0;
}

long llm_cfg_key(llm_cfg *c, const llm_model *m, char *out, long cap, const char **src)
{
    const char *where = 0;
    long n = cfg_key(c, m, out, cap, &where);
    /* amazon-bedrock with AWS_BEDROCK_SKIP_AUTH=1: pi sends no stored key (the SDK still reads the environment's) */
    if (n > 0 && m->aws && m->aws->skip_auth && (!where || strcmp(where, "environment"))) {
        out[0] = 0;
        n = 0;
        where = 0;
    }
    if (src)
        *src = where;
    return n;
}

int llm_cfg_headers(llm_cfg *c, const llm_model *m, const char *key, jw *w)
{
    jw t;
    int rc = 0;
    jw_init(&t);
    if (m->headers) {
        jv root, k, v;
        jit it;
        if (json_parse(m->headers, (long)strlen(m->headers), &root) == 0) {
            json_iter(root, &it);
            while (rc == 0 && json_next(&it, &k, &v)) {
                char nm[128], raw[512], val[512];
                long l;
                if (json_str(k, nm, sizeof(nm)) < 0 || json_str(v, raw, sizeof(raw)) < 0) {
                    rc = -1;
                    break;
                }
                l = llm_cfg_value(c, raw, val, sizeof(val));
                if (l < 0) {
                    rc = -1;
                    break;
                }
                jw_rawz(&t, nm);
                jw_rawz(&t, ": ");
                jw_raw(&t, val, l);
                jw_rawz(&t, "\r\n");
            }
        }
    }
    if (rc == 0 && m->auth_header && key && key[0]) {
        jw_rawz(&t, "Authorization: Bearer ");
        jw_rawz(&t, key);
        jw_rawz(&t, "\r\n");
    }
    if (rc == 0 && !t.oom)
        jw_raw(w, t.p, t.n);
    jw_free(&t);
    return rc;
}

static int nz_skip(const char *skip)
{
    return skip && !strcmp(skip, "1");
}

/* bedrock-converse-stream: the region, endpoint and credentials from the AWS_* variables and the bearer token */
static int bedrock_endpoint(llm_cfg *c, llm_model *m, char *err, long errcap)
{
    char region[48], dregion[48], profile[100], access[130], secret[130], skip[8];
    static char token[4100];
    llm_bedrock_env e;
    static char key[4100];             /* an OAuth access token is a JWT of a few KB */
    const char *src;
    region[0] = dregion[0] = profile[0] = access[0] = secret[0] = skip[0] = token[0] = 0;
    env_into(c, "AWS_REGION", region, sizeof(region), 0);
    env_into(c, "AWS_DEFAULT_REGION", dregion, sizeof(dregion), 0);
    env_into(c, "AWS_PROFILE", profile, sizeof(profile), 0);
    env_into(c, "AWS_ACCESS_KEY_ID", access, sizeof(access), 0);
    env_into(c, "AWS_SECRET_ACCESS_KEY", secret, sizeof(secret), 0);
    env_into(c, "AWS_SESSION_TOKEN", token, sizeof(token), 0);
    env_into(c, "AWS_BEDROCK_SKIP_AUTH", skip, sizeof(skip), 0);
    if (m->base_url != c->bd_ep)
        cl_copy(c->bd_orig, m->base_url ? m->base_url : "", sizeof(c->bd_orig));    /* the catalog's, kept across requests */
    {
        llm_model t = *m;
        t.base_url = c->bd_orig;
        e.region = region;
        e.default_region = dregion;
        e.profile = profile;
        e.access = access;
        e.secret = secret;
        e.token = token;
        e.skip_auth = skip;
        e.bearer = llm_cfg_key(c, m, key, sizeof(key), &src) > 0;
        if (e.bearer && nz_skip(skip) && (!src || strcmp(src, "environment")))
            e.bearer = 0;               /* AWS_BEDROCK_SKIP_AUTH drops a stored key; the SDK still reads the variable itself */
        if (llm_bedrock_resolve(&t, &e, &c->aws, c->bd_ep, sizeof(c->bd_ep), err, errcap))
            return -1;
    }
    m->base_url = c->bd_ep;
    m->aws = &c->aws;
    return 0;
}

int llm_cfg_authed(llm_cfg *c, const llm_model *m)
{
    char v[300];
    static const char *const more[] = { "AWS_PROFILE", "AWS_CONTAINER_CREDENTIALS_RELATIVE_URI",
                                        "AWS_CONTAINER_CREDENTIALS_FULL_URI", "AWS_WEB_IDENTITY_TOKEN_FILE" };
    static char key[4100];             /* an OAuth access token is a JWT of a few KB */
    int i;
    if (llm_cfg_key(c, m, key, sizeof(key), 0) > 0)
        return 1;
    if (!m->api || strcmp(m->api, "bedrock-converse-stream") || !m->provider || strcmp(m->provider, "amazon-bedrock"))
        return 0;
    if (env_into(c, "AWS_ACCESS_KEY_ID", v, sizeof(v), 0) > 0 && env_into(c, "AWS_SECRET_ACCESS_KEY", v, sizeof(v), 0) > 0)
        return 1;
    for (i = 0; i < 4; i++)
        if (env_into(c, more[i], v, sizeof(v), 0) > 0)
            return 1;
    return 0;
}

int llm_cfg_endpoint(llm_cfg *c, llm_model *m, char *err, long errcap)
{
    char eb[300], er[100], ev[48], em[600];
    llm_model t;
    if (m->api && !strcmp(m->api, "bedrock-converse-stream"))
        return bedrock_endpoint(c, m, err, errcap);
    if (!m->api || strcmp(m->api, "azure-openai-responses"))
        return 0;
    eb[0] = er[0] = ev[0] = em[0] = 0;
    env_into(c, "AZURE_OPENAI_BASE_URL", eb, sizeof(eb), 0);
    env_into(c, "AZURE_OPENAI_RESOURCE_NAME", er, sizeof(er), 0);
    env_into(c, "AZURE_OPENAI_API_VERSION", ev, sizeof(ev), 0);
    env_into(c, "AZURE_OPENAI_DEPLOYMENT_NAME_MAP", em, sizeof(em), 0);
    if (m->base_url != c->az.base)
        cl_copy(c->az_orig, m->base_url ? m->base_url : "", sizeof(c->az_orig));   /* the catalog's, kept across requests */
    t = *m;
    t.base_url = c->az_orig;
    if (llm_azure_resolve(&t, eb, er, ev, em, &c->az, err, errcap))
        return -1;
    m->base_url = c->az.base;
    m->api_version = c->az.version;
    m->deployment = c->az.deployment;
    return 0;
}
