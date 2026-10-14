/* be_azure -- azure-openai-responses (Phase M3): the Responses API on an Azure OpenAI resource. pi 1.1.0
 * api/azure-openai-responses.ts: the same body and the same stream as openai-responses (buildParams differs only in the
 * model, which is the deployment's name, in sending no session headers and in strict tools being on unless the model
 * says off), POST {baseUrl}/responses?api-version=<v> with the key in an `api-key` header (the AzureOpenAI client of the
 * openai SDK: authHeaders). The endpoint rules are api/azure-openai-config.ts; llm_cfg_endpoint (llm/creds.c) feeds
 * them the environment.
 * Portable C89, host-tested (tests/test_pi_llm.c, goldens from pi). */
#include <stdlib.h>
#include <string.h>
#include "be_azure.h"
#include "registry.h"
#include "httperr.h"
#include "../claude/http.h"
#include "../claude/util.h"

long llm_feeds_azure;

static long a_head(const char *host, int port, int tls, const char *path, const char *key,
                   const char *user_agent, long body_len, char *out, long cap)
{
    http_hdr h[4];
    h[0].name = "User-Agent";
    h[0].value = user_agent;
    h[1].name = "Accept";
    h[1].value = "text/event-stream";
    h[2].name = "Content-Type";
    h[2].value = "application/json";
    h[3].name = "api-key";
    h[3].value = key ? key : "";
    return http_post_head(host, port, tls, path, h, 4, body_len, out, cap);
}

static long a_route(const llm_ctx *c, char *out, long cap)
{
    cl_copy(out, "/responses?api-version=", cap);
    cl_cat(out, c->model->api_version && *c->model->api_version ? c->model->api_version : "v1", cap);
    return (long)strlen(out) < cap - 1 ? (long)strlen(out) : -1;
}

static int a_body(const llm_ctx *x, jw *w)
{
    const llm_model *md = x->model;
    return llm_responses_body(x, w, md->deployment && *md->deployment ? md->deployment : md->id,
                              md->compat.strict_tools != 0);
}

static int a_feed(void *dv, const char *event, const char *data, long n, const llm_sink *k)
{
    llm_feeds_azure++;
    return llm_responses_feed(dv, event, data, n, k);
}

/* ---- the endpoint ---- */

static void trim(const char *s, char *out, long cap)
{
    long a = 0, b = (long)strlen(s);
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r'))
        a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' || s[b - 1] == '\r'))
        b--;
    if (b - a >= cap)
        b = a + cap - 1;
    memmove(out, s + a, (size_t)(b - a));
    out[b - a] = 0;
}

static int ends_with(const char *s, long n, const char *suf)
{
    long m = (long)strlen(suf);
    long i;
    if (n < m)
        return 0;
    for (i = 0; i < m; i++) {
        char c = s[n - m + i];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c + 32);
        if (c != suf[i])
            return 0;
    }
    return 1;
}

/* normalizeAzureBaseUrl: an Azure host with no path, /openai or /openai/v1/responses is the resource's /openai/v1 */
static int normalize(const char *in, char *out, long cap, char *err, long errcap)
{
    char t[300], tmp[300];
    const char *auth, *rest;
    long alen, hlen, i, plen;
    int azure;
    trim(in, t, sizeof(t));
    while (t[0] && t[strlen(t) - 1] == '/')
        t[strlen(t) - 1] = 0;
    if (!strncmp(t, "https://", 8))
        auth = t + 8;
    else if (!strncmp(t, "http://", 7))
        auth = t + 7;
    else {
        cl_copy(err, "Invalid Azure OpenAI base URL: ", errcap);
        cl_cat(err, in, errcap);
        return -1;
    }
    alen = 0;
    while (auth[alen] && auth[alen] != '/' && auth[alen] != '?' && auth[alen] != '#')
        alen++;
    if (!alen) {
        cl_copy(err, "Invalid Azure OpenAI base URL: ", errcap);
        cl_cat(err, in, errcap);
        return -1;
    }
    rest = auth + alen;
    hlen = alen;
    for (i = 0; i < alen; i++)
        if (auth[i] == '@')
            hlen = alen - i - 1;
    {
        const char *h = auth + alen - hlen;
        long j;
        for (j = 0; j < hlen; j++)
            if (h[j] == ':') {
                hlen = j;
                break;
            }
        azure = ends_with(h, hlen, ".openai.azure.com") || ends_with(h, hlen, ".cognitiveservices.azure.com") ||
                ends_with(h, hlen, ".ai.azure.com");
    }
    plen = 0;
    while (rest[plen] && rest[plen] != '?' && rest[plen] != '#')
        plen++;
    while (plen > 0 && rest[plen - 1] == '/')
        plen--;
    if (azure && (plen == 0 || (plen == 7 && !strncmp(rest, "/openai", 7)) ||
                  (plen == 20 && !strncmp(rest, "/openai/v1/responses", 20)))) {
        long pre = (long)(rest - t);
        memcpy(tmp, t, (size_t)pre);
        tmp[pre] = 0;
        cl_cat(tmp, "/openai/v1", sizeof(tmp));
        cl_copy(out, tmp, cap);
    } else {
        cl_copy(out, t, cap);
    }
    return 0;
}

static const char *nz(const char *s)
{
    return s && *s ? s : 0;
}

/* AZURE_OPENAI_DEPLOYMENT_NAME_MAP "model=deployment,model=deployment": the deployment of id, "" none */
static void deployment_of(const char *map, const char *id, char *out, long cap)
{
    const char *p = map;
    out[0] = 0;
    while (p && *p) {
        const char *e = strchr(p, ',');
        char ent[260], a[130], b[130];
        char *eq;
        long n = e ? (long)(e - p) : (long)strlen(p);
        if (n >= (long)sizeof(ent))
            n = (long)sizeof(ent) - 1;
        memcpy(ent, p, (size_t)n);
        ent[n] = 0;
        trim(ent, ent, sizeof(ent));
        eq = strchr(ent, '=');
        if (ent[0] && eq) {
            char *eq2;
            *eq = 0;
            eq2 = strchr(eq + 1, '=');
            if (eq2)
                *eq2 = 0;           /* split("=", 2) drops the rest */
            trim(ent, a, sizeof(a));
            trim(eq + 1, b, sizeof(b));
            if (a[0] && b[0] && !strcmp(a, id)) {
                cl_copy(out, b, cap);     /* the last entry for an id wins (Map.set) */
            }
        }
        if (!e)
            break;
        p = e + 1;
    }
}

int llm_azure_resolve(const llm_model *m, const char *e_base, const char *e_res, const char *e_ver, const char *e_map,
                      llm_azure_ep *out, char *err, long errcap)
{
    char raw[300], tb[300];
    const char *base = 0;
    memset(out, 0, sizeof(*out));
    if (nz(e_base)) {
        trim(e_base, tb, sizeof(tb));
        if (tb[0])
            base = tb;
    }
    if (!base && nz(e_res)) {
        cl_copy(raw, "https://", sizeof(raw));
        cl_cat(raw, e_res, sizeof(raw));
        cl_cat(raw, ".openai.azure.com/openai/v1", sizeof(raw));
        base = raw;
    }
    if (!base && nz(m->base_url))
        base = m->base_url;
    if (!base) {
        cl_copy(err, "Azure OpenAI base URL is required. Set AZURE_OPENAI_BASE_URL or AZURE_OPENAI_RESOURCE_NAME, or pass azureBaseUrl, azureResourceName, or model.baseUrl.", errcap);
        return -1;
    }
    if (normalize(base, out->base, sizeof(out->base), err, errcap))
        return -1;
    cl_copy(out->version, nz(e_ver) ? e_ver : "v1", sizeof(out->version));
    deployment_of(e_map, m->id, out->deployment, sizeof(out->deployment));
    if (!out->deployment[0])
        cl_copy(out->deployment, m->id, sizeof(out->deployment));
    return 0;
}

static void a_http_err(const llm_http_fail *f, char *out, long cap)
{
    llm_httperr_openai(f, "Azure OpenAI API error", 0, out, cap);
}

const llm_backend llm_be_azure = {
    "azure-openai-responses", "api-key", "/responses", a_head, a_body, 0, llm_responses_new, a_feed,
    llm_responses_free, "OpenAI Responses stream ended before a terminal response event", a_route, 0, 0, a_http_err, 0, 0
};
