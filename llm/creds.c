/* see creds.h */
#include <stdlib.h>
#include <string.h>
#include "creds.h"

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

/* an environment variable appended at out[n]: its length; -1 when it is unset, empty (pi: `env[name] || undefined`) or
 * does not fit. getenv cuts a long value to the room it is given, so a value that fills the room is taken as cut:
 * an error, never half a key */
static long env_into(llm_cfg *c, const char *name, char *out, long cap, long n)
{
    long v;
    if (!c->getenv || cap - n < 2)
        return -1;
    v = c->getenv(c->u, name, out + n, cap - n);
    if (v <= 0 || v >= cap - n - 1)
        return -1;
    return v;
}

long llm_cfg_value(llm_cfg *c, const char *cfg, char *out, long cap)
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
            if ((v = env_into(c, nm, out, cap, n)) < 0)
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
            if ((v = env_into(c, nm, out, cap, n)) < 0)
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

void llm_key_env_name(const char *provider, char *out, long cap)
{
    long i, n = 0;
    for (i = 0; provider[i] && n < cap - 9; i++) {
        char ch = provider[i];
        out[n++] = ch == '-' ? '_' : ch >= 'a' && ch <= 'z' ? (char)(ch - 32) : ch;
    }
    out[n] = 0;
    strcat(out, "_API_KEY");
}

/* auth.json: {"<provider>":{"type":"api_key","key":"<config value>"}}.
 * A stored credential owns the provider (pi-ai auth/resolve.js): -2 nothing is stored for it, else the key's length,
 * or -1 when the entry cannot give one (a command that fails, another credential type, an empty key); the caller
 * does not fall back to models.json or the environment after a -1. */
static long auth_key(llm_cfg *c, const char *provider, char *out, long cap)
{
    jv root, e, t, k;
    char *raw;
    long n, r;
    if (!c->auth || json_parse(c->auth, c->auth_n, &root) || json_type(root) != J_OBJ)
        return -2;
    {
        jit it;
        jv key;
        int found = 0;
        json_iter(root, &it);
        while (json_next(&it, &key, &e)) {
            char nm[64];
            if (json_str(key, nm, sizeof(nm)) >= 0 && !strcmp(nm, provider)) {
                found = 1;
                break;
            }
        }
        if (!found)
            return -2;
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

long llm_cfg_key(llm_cfg *c, const llm_model *m, char *out, long cap, const char **src)
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
