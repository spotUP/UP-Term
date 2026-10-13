/* be_bedrock -- the bedrock-converse-stream backend (Phase M6): POST {endpoint}/model/<id>/converse-stream, signed with AWS
 * Signature Version 4 (llm/sigv4.c) or sent with a bearer token, the body from pi-ai messages, and the decoder from the
 * AWS event-stream answer (llm/evstream.c) to the neutral events.
 *
 * pi 1.1.0 api/bedrock-converse-stream.ts on @aws-sdk/client-bedrock-runtime 3.1127.0 (package-lock.json of the pinned
 * source): stream() :127-340 (credentials :183-250, region :196-206, the loop over the stream's events :298-336),
 * formatBedrockError :393, streamSimple :531-580 (the thinking budget), handleContentBlock* / handleMetadata
 * :582-755, the model rules :758-960, convertMessages :976-1146, convertToolConfig :1148-1183, mapStopReason :1185,
 * the region and endpoint helpers :1202-1260, buildAdditionalModelRequestFields :1262-1340; env-api-keys.ts :174-190
 * (the variables that make the provider available). The SDK's restJson1 serializer gives the member order (messages,
 * system, inferenceConfig, toolConfig, additionalModelRequestFields; image {format, source}; toolSpec {name,
 * inputSchema, description, strict}); the goldens (tests/pi/bedrock, tools/gen_pi_bedrock.mjs) were made by pi itself
 * against a recording HTTP server.
 *
 * Not ported (COMPATIBILITY.md): AWS profiles, SSO, container and web identity credentials (AWS_PROFILE, ~/.aws, IMDS),
 * proxies, toolChoice, temperature, requestMetadata, thinkingDisplay "omitted", custom thinking budgets,
 * PI_CACHE_RETENTION=long and AWS_BEDROCK_FORCE_CACHE, the SDK's retries (pi's agent retries a failed answer), the
 * bedrock_response_failure diagnostic on the message, sanitizeSurrogates, the SDK's wording of a non-JSON error body.
 * Portable C89, host-tested (tests/test_pi_llm.c, goldens from pi). */
#include <stdlib.h>
#include <string.h>
#include "be_bedrock.h"
#include "registry.h"
#include "constrain.h"
#include "estimate.h"
#include "sigv4.h"
#include "transcript.h"
#include "../claude/http.h"
#include "../claude/util.h"

long llm_feeds_bedrock;

/* ---- the model rules (bedrock-converse-stream.ts:760-950) ---- */

/* getModelMatchCandidates: the id and the name, each lower case and with runs of white space, "_", "." and ":" as "-" */
typedef struct cands {
    char s[4][260];
    int n;
} cands;

static void cands_of(const llm_model *m, cands *c)
{
    const char *v[2];
    int i, k;
    long j;
    v[0] = m->id ? m->id : "";
    v[1] = m->name && *m->name ? m->name : 0;
    c->n = 0;
    for (i = 0; i < 2; i++) {
        char *lo, *nm;
        long l;
        if (!v[i])
            continue;
        lo = c->s[c->n++];
        nm = c->s[c->n++];
        l = 0;
        for (j = 0; v[i][j] && l < 258; j++)
            lo[l++] = v[i][j] >= 'A' && v[i][j] <= 'Z' ? (char)(v[i][j] + 32) : v[i][j];
        lo[l] = 0;
        k = 0;
        for (j = 0; lo[j] && k < 258;) {
            if (lo[j] == ' ' || lo[j] == '\t' || lo[j] == '\n' || lo[j] == '_' || lo[j] == '.' || lo[j] == ':') {
                while (lo[j] == ' ' || lo[j] == '\t' || lo[j] == '\n' || lo[j] == '_' || lo[j] == '.' || lo[j] == ':')
                    j++;
                nm[k++] = '-';
            } else
                nm[k++] = lo[j++];
        }
        nm[k] = 0;
    }
}

static int has(const cands *c, const char *needle)
{
    int i;
    for (i = 0; i < c->n; i++)
        if (strstr(c->s[i], needle))
            return 1;
    return 0;
}

/* isAnthropicClaudeModel */
static int is_claude(const llm_model *m)
{
    char id[260], nm[260];
    long i;
    for (i = 0; m->id && m->id[i] && i < 258; i++)
        id[i] = m->id[i] >= 'A' && m->id[i] <= 'Z' ? (char)(m->id[i] + 32) : m->id[i];
    id[i < 259 ? i : 259] = 0;
    for (i = 0; m->name && m->name[i] && i < 258; i++)
        nm[i] = m->name[i] >= 'A' && m->name[i] <= 'Z' ? (char)(m->name[i] + 32) : m->name[i];
    nm[i < 259 ? i : 259] = 0;
    if (!m->name)
        nm[0] = 0;
    return strstr(id, "anthropic.claude") || strstr(id, "anthropic/claude") || strstr(nm, "anthropic.claude") ||
           strstr(nm, "anthropic/claude") || strstr(nm, "claude");
}

/* supportsAdaptiveThinking */
static int adaptive(const llm_model *m)
{
    cands c;
    cands_of(m, &c);
    return has(&c, "opus-4-6") || has(&c, "opus-4-7") || has(&c, "opus-4-8") || has(&c, "opus-5") ||
           has(&c, "sonnet-4-6") || has(&c, "sonnet-5") || has(&c, "haiku-5") || has(&c, "fable-5");
}

/* supportsNativeXhighEffort and supportsThinkingBlockBinding (the same list) */
static int native_xhigh(const llm_model *m)
{
    cands c;
    cands_of(m, &c);
    return has(&c, "opus-4-7") || has(&c, "opus-4-8") || has(&c, "opus-5") || has(&c, "sonnet-5") ||
           has(&c, "haiku-5") || has(&c, "fable-5");
}

/* supportsPromptCaching (without AWS_BEDROCK_FORCE_CACHE) */
static int caching(const llm_model *m)
{
    cands c;
    cands_of(m, &c);
    if (!has(&c, "claude"))
        return 0;
    if (has(&c, "fable-5") || has(&c, "opus-5") || has(&c, "sonnet-5") || has(&c, "haiku-5"))
        return 1;
    return has(&c, "-4-") || has(&c, "claude-3-7-sonnet") || has(&c, "claude-3-5-haiku");
}

/* mapThinkingLevelToEffort */
static const char *effort(const llm_model *m, int lv)
{
    static const char *const names[] = { "off", "minimal", "low", "medium", "high", "xhigh", "max" };
    if (lv == LT_XHIGH && native_xhigh(m))
        return "xhigh";
    if (m->thinking_map[lv].kind == LTM_STR)
        return m->thinking_map[lv].val;
    (void)names;
    switch (lv) {
    case LT_MINIMAL:
    case LT_LOW:
        return "low";
    case LT_MEDIUM:
        return "medium";
    default:
        return "high";
    }
}

/* ---- the endpoint, region and credentials (stream() :160-230) ---- */

static const char *nz(const char *s)
{
    return s && *s ? s : 0;
}

/* getStandardBedrockEndpointRegion: the region of ^bedrock-runtime(-fips)?.<region>.amazonaws.com(.cn)?$, "" none */
static void standard_region(const char *url, char *out, long cap)
{
    const char *h;
    char host[160];
    long i, n = 0;
    const char *p, *q;
    out[0] = 0;
    if (!url)
        return;
    if (!strncmp(url, "https://", 8))
        h = url + 8;
    else if (!strncmp(url, "http://", 7))
        h = url + 7;
    else
        return;
    while (h[n] && h[n] != '/' && h[n] != '?' && h[n] != '#' && h[n] != ':' && h[n] != '@' && n < (long)sizeof(host) - 1) {
        host[n] = h[n] >= 'A' && h[n] <= 'Z' ? (char)(h[n] + 32) : h[n];
        n++;
    }
    host[n] = 0;
    if (strncmp(host, "bedrock-runtime", 15))
        return;
    p = host + 15;
    if (!strncmp(p, "-fips", 5))
        p += 5;
    if (*p != '.')
        return;
    p++;
    q = strstr(p, ".amazonaws.com");
    if (!q || q == p)
        return;
    if (strcmp(q, ".amazonaws.com") && strcmp(q, ".amazonaws.com.cn"))
        return;
    for (i = 0; p + i < q; i++)
        if (!((p[i] >= 'a' && p[i] <= 'z') || (p[i] >= '0' && p[i] <= '9') || p[i] == '-'))
            return;
    if ((long)(q - p) >= cap)
        return;
    memcpy(out, p, (size_t)(q - p));
    out[q - p] = 0;
}

/* the region of ^arn:aws(-[a-z0-9-]+)?:bedrock:<region>: */
static void arn_region(const char *id, char *out, long cap)
{
    const char *p = id;
    long n = 0;
    out[0] = 0;
    if (strncmp(p, "arn:aws", 7))
        return;
    p += 7;
    if (*p == '-') {
        p++;
        while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-')
            p++;
    }
    if (strncmp(p, ":bedrock:", 9))
        return;
    p += 9;
    while (((p[n] >= 'a' && p[n] <= 'z') || (p[n] >= '0' && p[n] <= '9') || p[n] == '-') && n < cap - 1)
        n++;
    if (n == 0 || p[n] != ':')
        return;
    memcpy(out, p, (size_t)n);
    out[n] = 0;
}

int llm_bedrock_resolve(const llm_model *m, const llm_bedrock_env *e, llm_aws *aws, char *endpoint, long epcap,
                        char *err, long errcap)
{
    char std[64], arn[64];
    const char *conf, *region = "";
    int explicit_ep, profile = nz(e->profile) != 0;
    memset(aws, 0, sizeof(*aws));
    conf = nz(e->region) ? e->region : nz(e->default_region) ? e->default_region : 0;
    if (conf)
        cl_copy(aws->cfg_region, conf, sizeof(aws->cfg_region));
    standard_region(m->base_url, std, sizeof(std));
    arn_region(m->id ? m->id : "", arn, sizeof(arn));
    explicit_ep = std[0] ? (!conf && !profile) : 1;
    if (arn[0])
        region = arn;
    else if (conf)
        region = conf;
    else if (std[0] && explicit_ep)
        region = std;
    else if (!profile)
        region = "us-east-1";
    cl_copy(aws->region, region, sizeof(aws->region));
    if (explicit_ep)
        cl_copy(endpoint, m->base_url ? m->base_url : "", epcap);
    else {
        cl_copy(endpoint, "https://bedrock-runtime.", epcap);
        cl_cat(endpoint, region, epcap);
        cl_cat(endpoint, !strncmp(region, "cn-", 3) ? ".amazonaws.com.cn" : ".amazonaws.com", epcap);
    }
    if (!region[0]) {
        /* AWS_PROFILE was set and no region: the SDK would read the profile's, amiga-pi does not read ~/.aws */
        cl_copy(err, "AWS_PROFILE is set without AWS_REGION: amiga-pi does not read AWS profiles; set AWS_REGION and AWS_ACCESS_KEY_ID "
                     "with AWS_SECRET_ACCESS_KEY, or AWS_BEARER_TOKEN_BEDROCK.", errcap);
        return -1;
    }
    aws->skip_auth = nz(e->skip_auth) && !strcmp(e->skip_auth, "1");
    if (aws->skip_auth) {
        cl_copy(aws->access, "dummy-access-key", sizeof(aws->access));
        cl_copy(aws->secret, "dummy-secret-key", sizeof(aws->secret));
    } else if (nz(e->access) && nz(e->secret)) {
        if (strlen(e->access) >= sizeof(aws->access) || strlen(e->secret) >= sizeof(aws->secret) ||
            (nz(e->token) && strlen(e->token) >= sizeof(aws->token))) {
            cl_copy(err, "the AWS credentials in the environment are longer than amiga-pi takes", errcap);
            return -1;
        }
        cl_copy(aws->access, e->access, sizeof(aws->access));
        cl_copy(aws->secret, e->secret, sizeof(aws->secret));
        if (nz(e->token))
            cl_copy(aws->token, e->token, sizeof(aws->token));
    }
    if (!aws->access[0] && !e->bearer) {
        cl_copy(err, "No Amazon Bedrock credentials amiga-pi can use: set AWS_BEARER_TOKEN_BEDROCK, or AWS_ACCESS_KEY_ID and "
                     "AWS_SECRET_ACCESS_KEY (with AWS_SESSION_TOKEN for temporary keys). AWS profiles (AWS_PROFILE), SSO and "
                     "container or web identity credentials are not supported.", errcap);
        return -1;
    }
    return 0;
}

/* ---- head, route, sign ---- */

static long b_head(const char *host, int port, int tls, const char *path, const char *key,
                   const char *user_agent, long body_len, char *out, long cap)
{
    http_hdr h[3];
    h[0].name = "User-Agent";
    h[0].value = user_agent;
    h[1].name = "Content-Type";
    h[1].value = "application/json";
    h[2].name = "Authorization";
    if (key && *key) {
        static char bearer[1100];
        cl_copy(bearer, "Bearer ", sizeof(bearer));
        cl_cat(bearer, key, sizeof(bearer));
        h[2].value = bearer;
    } else
        h[2].value = "";
    return http_post_head(host, port, tls, path, h, 3, body_len, out, cap);
}

/* the path below the endpoint: /model/<id as the SDK encodes a path segment>/converse-stream */
static long b_route(const llm_ctx *c, char *out, long cap)
{
    jw w;
    long n;
    jw_init(&w);
    jw_rawz(&w, "/model/");
    llm_uri_encode(&w, c->model->id, (long)strlen(c->model->id), 0);
    jw_rawz(&w, "/converse-stream");
    n = w.oom || w.n >= cap ? -1 : w.n;
    if (n >= 0)
        memcpy(out, w.p, (size_t)w.n + 1);
    jw_free(&w);
    return n;
}

/* the SDK's SignatureV4 for service "bedrock": Host, Content-Type, x-amz-date and the session token signed */
static long b_sign(const llm_ctx *c, const char *host, int port, int tls, const char *path, const char *body, long bn,
                   int bearer, char *out, long cap, char *err, long errcap)
{
    const llm_aws *aw = c->model->aws;
    llm_sigv4 in;
    llm_sigout o;
    llm_sig_hdr h[2];
    char hostv[200], date[20];
    jw w;
    long n;
    if (bearer || !aw || !aw->access[0]) {
        if (!bearer && !aw) {
            cl_copy(err, "No Amazon Bedrock credentials", errcap);
            return -1;
        }
        return 0;
    }
    cl_copy(hostv, host, sizeof(hostv));
    if (port != (tls ? 443 : 80)) {
        char num[16];
        cl_ltoa(port, num);
        cl_cat(hostv, ":", sizeof(hostv));
        cl_cat(hostv, num, sizeof(hostv));
    }
    h[0].name = "host";
    h[0].value = hostv;
    h[1].name = "content-type";
    h[1].value = "application/json";
    llm_sigv4_date(llm_wall(), date);
    memset(&in, 0, sizeof(in));
    in.method = "POST";
    in.path = path;
    in.hdr = h;
    in.nhdr = 2;
    in.body = body;
    in.bn = bn;
    in.access = aw->access;
    in.secret = aw->secret;
    in.token = aw->token[0] ? aw->token : 0;
    in.sign_token = 1;
    in.region = aw->region;
    in.service = "bedrock";
    in.amzdate = date;
    in.normalize = 1;
    llm_sigout_init(&o);
    if (llm_sigv4_sign(&in, &o)) {
        llm_sigout_free(&o);
        cl_copy(err, "out of memory", errcap);
        return -1;
    }
    jw_init(&w);
    jw_rawz(&w, "x-amz-date: ");
    jw_rawz(&w, date);
    jw_rawz(&w, "\r\n");
    if (in.token) {
        jw_rawz(&w, "x-amz-security-token: ");
        jw_rawz(&w, in.token);
        jw_rawz(&w, "\r\n");
    }
    jw_rawz(&w, "Authorization: ");
    jw_raw(&w, o.auth.p, o.auth.n);
    jw_rawz(&w, "\r\n");
    n = w.oom || w.n >= cap ? -1 : w.n;
    if (n >= 0)
        memcpy(out, w.p, (size_t)w.n + 1);
    else
        cl_copy(err, "the signed request headers are too long", errcap);
    jw_free(&w);
    llm_sigout_free(&o);
    return n;
}

/* ---- the body ---- */

static int b64_val(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;
    return -1;
}

/* atob on s[0..n) (the forgiving-base64 decode, without the white space it skips): the bytes (malloc'd, *out_n of them),
 * 0 when atob throws (a length of 1 mod 4 after the padding is cut, a character outside the alphabet) */
static unsigned char *b64_decode(const char *s, long n, long *out_n)
{
    unsigned char *o;
    long i, m = 0, acc = 0, bits = 0, len = n;
    if (len % 4 == 0 && len > 0 && s[len - 1] == '=') {
        len--;
        if (len > 0 && s[len - 1] == '=')
            len--;
    }
    if (len % 4 == 1)
        return 0;
    o = (unsigned char *)malloc((size_t)(len / 4 * 3 + 3));
    if (!o)
        return 0;
    for (i = 0; i < len; i++) {
        int v = b64_val((unsigned char)s[i]);
        if (v < 0) {
            free(o);
            return 0;
        }
        acc = ((acc << 6) | v) & 0xffff;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            o[m++] = (unsigned char)((acc >> bits) & 0xff);
        }
    }
    *out_n = m;
    return o;
}

/* a message's text block {"text": ...} unless the text is blank */
static int put_text(jw *w, jv text, int k)
{
    if (text.n == 2 || llm_tx_blank(text))
        return k;
    jw_rawz(w, k ? ",{\"text\":" : "{\"text\":");
    jw_raw(w, text.p, text.n);
    jw_rawz(w, "}");
    return k + 1;
}

static const char *image_format(jv mime)
{
    if (json_streq(mime, "image/jpeg") || json_streq(mime, "image/jpg"))
        return "jpeg";
    if (json_streq(mime, "image/png"))
        return "png";
    if (json_streq(mime, "image/gif"))
        return "gif";
    if (json_streq(mime, "image/webp"))
        return "webp";
    return 0;
}

/* createImageBlock: {"image":{"format":..,"source":{"bytes":<base64>}}}; -1 for a type pi throws "Unknown image type" on */
static int put_image(jw *w, jv b, int k)
{
    jv d, m;
    const char *f;
    if (!json_get(b, "data", &d) || !json_get(b, "mimeType", &m))
        return -2;
    f = image_format(m);
    if (!f)
        return -1;
    jw_rawz(w, k ? ",{\"image\":{\"format\":\"" : "{\"image\":{\"format\":\"");
    jw_rawz(w, f);
    jw_rawz(w, "\",\"source\":{\"bytes\":");
    jw_raw(w, d.p, d.n);
    jw_rawz(w, "}}}");
    return k + 1;
}

/* one user message of the request */
static int w_user(jw *w, int *n, jv c, long *close_at)
{
    jit it;
    jv b, t, v;
    int k = 0;
    jw_rawz(w, *n ? ",{\"role\":\"user\",\"content\":[" : "{\"role\":\"user\",\"content\":[");
    if (json_type(c) == J_STR) {
        if (c.n == 2 || llm_tx_blank(c)) {
            jw_rawz(w, "{\"text\":\"<empty>\"}");
            k = 1;
        } else
            k = put_text(w, c, 0);
    } else if (json_type(c) == J_ARR) {
        json_iter(c, &it);
        while (json_next(&it, 0, &b)) {
            if (!json_get(b, "type", &t))
                continue;
            if (json_streq(t, "text") && json_get(b, "text", &v))
                k = put_text(w, v, k);
            else if (json_streq(t, "image")) {
                int r = put_image(w, b, k);
                if (r < 0)
                    return -1;
                k = r;
            }
        }
        if (!k) {
            jw_rawz(w, "{\"text\":\"<empty>\"}");
            k = 1;
        }
    }
    *close_at = w->n;
    jw_rawz(w, "]}");
    (*n)++;
    return 0;
}

static int w_assistant(jw *w, int *n, const llm_model *md, jv m)
{
    jv c, b, t, v, s, id, nm, args;
    jit it;
    int k = 0, claude = is_claude(md);
    long mark = w->n;
    if (!json_get(m, "content", &c) || json_type(c) != J_ARR || json_count(c) == 0)
        return 0;
    jw_rawz(w, *n ? ",{\"role\":\"assistant\",\"content\":[" : "{\"role\":\"assistant\",\"content\":[");
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        int red = 0;
        jv rv;
        if (!json_get(b, "type", &t))
            continue;
        if (json_streq(t, "text")) {
            if (json_get(b, "text", &v))
                k = put_text(w, v, k);
        } else if (json_streq(t, "toolCall")) {
            if (!json_get(b, "id", &id) || !json_get(b, "name", &nm))
                continue;
            jw_rawz(w, k ? ",{\"toolUse\":{\"toolUseId\":" : "{\"toolUse\":{\"toolUseId\":");
            jw_raw(w, id.p, id.n);
            jw_rawz(w, ",\"name\":");
            jw_raw(w, nm.p, nm.n);
            jw_rawz(w, ",\"input\":");
            if (json_get(b, "arguments", &args) && json_type(args) != J_NULL)
                llm_json_compact(w, args, 1);
            else
                jw_rawz(w, "{}");
            jw_rawz(w, "}}");
            k++;
        } else if (json_streq(t, "thinking")) {
            red = json_get(b, "redacted", &rv) && json_type(rv) == J_TRUE;
            if (red) {
                /* encrypted reasoning is opaque: the stored payload goes back as redactedContent */
                if (json_get(b, "thinkingSignature", &s) && json_type(s) == J_STR && s.n > 2) {
                    char *sig;
                    unsigned char *bytes;
                    long sl, bl = 0;
                    sig = json_strdup(s, &sl);
                    bytes = sig ? b64_decode(sig, sl, &bl) : 0;
                    free(sig);
                    if (bytes && bl > 0) {
                        jw tmp;
                        jw_init(&tmp);
                        jw_base64(&tmp, bytes, bl);
                        jw_rawz(w, k ? ",{\"reasoningContent\":{\"redactedContent\":\"" : "{\"reasoningContent\":{\"redactedContent\":\"");
                        jw_raw(w, tmp.p, tmp.n);
                        jw_rawz(w, "\"}}");
                        jw_free(&tmp);
                        k++;
                    }
                    free(bytes);
                }
                continue;
            }
            if (!json_get(b, "thinking", &v) || v.n == 2 || llm_tx_blank(v))
                continue;
            if (claude) {
                /* a signature arrives after the thinking deltas: without one the block goes back as plain text */
                if (!json_get(b, "thinkingSignature", &s) || json_type(s) != J_STR || s.n == 2 || llm_tx_blank(s)) {
                    jw_rawz(w, k ? ",{\"text\":" : "{\"text\":");
                    jw_raw(w, v.p, v.n);
                    jw_rawz(w, "}");
                } else {
                    jw_rawz(w, k ? ",{\"reasoningContent\":{\"reasoningText\":{\"text\":" : "{\"reasoningContent\":{\"reasoningText\":{\"text\":");
                    jw_raw(w, v.p, v.n);
                    jw_rawz(w, ",\"signature\":");
                    jw_raw(w, s.p, s.n);
                    jw_rawz(w, "}}}");
                }
            } else {
                jw_rawz(w, k ? ",{\"reasoningContent\":{\"reasoningText\":{\"text\":" : "{\"reasoningContent\":{\"reasoningText\":{\"text\":");
                jw_raw(w, v.p, v.n);
                jw_rawz(w, "}}}");
            }
            k++;
        }
    }
    if (!k) {
        w->n = mark;            /* every block was filtered out: pi skips the message */
        if (w->p)
            w->p[w->n] = 0;
        return 0;
    }
    jw_rawz(w, "]}");
    (*n)++;
    return 0;
}

/* a toolResult message as the toolResult content block */
static int w_result(jw *w, jv m)
{
    jv c, b, t, v, id, e;
    jit it;
    int k = 0;
    jw_rawz(w, "{\"toolResult\":{\"toolUseId\":");
    if (!json_get(m, "toolCallId", &id))
        return -1;
    jw_raw(w, id.p, id.n);
    jw_rawz(w, ",\"content\":[");
    if (json_get(m, "content", &c) && json_type(c) == J_ARR) {
        json_iter(c, &it);
        while (json_next(&it, 0, &b)) {
            if (!json_get(b, "type", &t))
                continue;
            if (json_streq(t, "image")) {
                int r = put_image(w, b, k);
                if (r < 0)
                    return -1;
                k = r;
            } else if (json_streq(t, "text") && json_get(b, "text", &v))
                k = put_text(w, v, k);
        }
    }
    if (!k)
        jw_rawz(w, "{\"text\":\"<empty>\"}");
    jw_rawz(w, "],\"status\":");
    jw_rawz(w, json_get(m, "isError", &e) && json_type(e) == J_TRUE ? "\"error\"}}" : "\"success\"}}");
    return 0;
}

static int govcloud(const llm_model *md)
{
    const llm_aws *a = md->aws;
    char low[40];
    long i;
    if (a && !strncmp(a->cfg_region, "us-gov-", 7))
        return 1;
    for (i = 0; md->id[i] && i < 39; i++)
        low[i] = md->id[i] >= 'A' && md->id[i] <= 'Z' ? (char)(md->id[i] + 32) : md->id[i];
    low[i] = 0;
    return !strncmp(low, "us-gov.", 7) || !strncmp(low, "arn:aws-us-gov:", 15);
}

/* buildAdditionalModelRequestFields: -1 nothing, else the object written to w */
static int w_fields(jw *w, const llm_model *md, int lv, long budget)
{
    const char *eff;
    if (lv == LT_OFF || !md->reasoning)
        return 0;
    if (is_claude(md)) {
        int gov = govcloud(md), ad = adaptive(md), bind = !gov && native_xhigh(md);
        jw_rawz(w, ",\"additionalModelRequestFields\":{\"thinking\":{\"type\":");
        if (ad) {
            jw_rawz(w, "\"adaptive\"");
            if (!gov)
                jw_rawz(w, ",\"display\":\"summarized\"");
            if (bind)
                jw_rawz(w, ",\"block_binding\":{\"prefix_mismatch_behavior\":\"drop_block\"}");
            jw_rawz(w, "},\"output_config\":{\"effort\":");
            jw_strz(w, effort(md, lv));
            jw_rawz(w, "}");
            if (bind)
                jw_rawz(w, ",\"anthropic_beta\":[\"thinking-binding-controls-2026-08-01\"]");
        } else {
            jw_rawz(w, "\"enabled\",\"budget_tokens\":");
            jw_long(w, budget);
            if (!gov)
                jw_rawz(w, ",\"display\":\"summarized\"");
            jw_rawz(w, "},\"anthropic_beta\":[\"interleaved-thinking-2025-05-14\"]");
        }
        jw_rawz(w, "}");
        return 1;
    }
    {
        cands c;
        cands_of(md, &c);
        if (has(&c, "gpt-oss")) {
            jw_rawz(w, ",\"additionalModelRequestFields\":{\"reasoning_effort\":");
            jw_strz(w, lv >= LT_HIGH ? "high" : lv == LT_MEDIUM ? "medium" : "low");
            jw_rawz(w, "}");
            return 1;
        }
        if (has(&c, "gpt-")) {
            static const char *const gpt[] = { "low", "low", "low", "medium", "high", "xhigh", "max" };
            eff = md->thinking_map[lv].kind == LTM_STR ? md->thinking_map[lv].val : gpt[lv];
            jw_rawz(w, ",\"additionalModelRequestFields\":{\"reasoning\":{\"effort\":");
            jw_strz(w, eff);
            jw_rawz(w, "}}");
            return 1;
        }
    }
    return 0;
}

static int b_body(const llm_ctx *x, jw *w)
{
    const llm_model *md = x->model;
    int i, rc = -1, n = 0, lv, claude = is_claude(md), last_user = 0, cache = 0;
    long maxt = llm_ctx_max_tokens(x), budget = 0, close_at = 0;
    llm_tx tx;
    if (llm_tx_plan(x, 0, &tx)) {
        llm_tx_free(&tx);
        return -1;
    }
    cache = caching(md);
    /* streamSimple: no level is no reasoning; a budget-based Claude shares its cap with the thinking budget */
    lv = x->thinking == LT_OFF ? LT_OFF : llm_think_clamp(md, x->thinking);
    if (lv != LT_OFF && claude && !adaptive(md))
        llm_think_budget(md, x->est_tokens, llm_ctx_max_tokens(x), lv, &maxt, &budget);
    jw_rawz(w, "{\"messages\":[");
    for (i = 0; i < x->nmsg; i++) {
        jv m, r, c;
        if (json_parse(x->msgs[i], x->msg_n[i], &m) || !json_get(m, "role", &r))
            goto out;
        if (json_streq(r, "user")) {
            if (!json_get(m, "content", &c) || w_user(w, &n, c, &close_at))
                goto out;
            last_user = 1;
        } else if (json_streq(r, "assistant")) {
            int before = n;
            if (w_assistant(w, &n, md, m))
                goto out;
            if (n != before)
                last_user = 0;
        } else if (json_streq(r, "toolResult")) {
            /* the results that follow join this one in one user message (Bedrock wants them together) */
            jw_rawz(w, n ? ",{\"role\":\"user\",\"content\":[" : "{\"role\":\"user\",\"content\":[");
            for (;;) {
                if (w_result(w, m))
                    goto out;
                if (i + 1 < x->nmsg && !json_parse(x->msgs[i + 1], x->msg_n[i + 1], &m) && json_get(m, "role", &r) &&
                    json_streq(r, "toolResult")) {
                    jw_rawz(w, ",");
                    i++;
                } else
                    break;
            }
            close_at = w->n;
            jw_rawz(w, "]}");
            n++;
            last_user = 1;
        }                       /* a system message: collapsed into x->system / x->tools by the caller */
    }
    if (cache && n > 0 && last_user) {
        /* the last message, a user one, ends with a cache point */
        w->n = close_at;
        if (w->p)
            w->p[w->n] = 0;
        jw_rawz(w, ",{\"cachePoint\":{\"type\":\"default\"}}]}");
    }
    jw_rawz(w, "]");
    if (x->system && *x->system) {
        jw_rawz(w, ",\"system\":[{\"text\":");
        jw_strz(w, x->system);
        jw_rawz(w, "}");
        if (cache)
            jw_rawz(w, ",{\"cachePoint\":{\"type\":\"default\"}}");
        jw_rawz(w, "]");
    }
    jw_rawz(w, ",\"inferenceConfig\":{\"maxTokens\":");
    jw_long(w, maxt);
    jw_rawz(w, "}");
    if (tx.ntl > 0) {
        jw sch;
        jv v;
        jw_init(&sch);
        jw_rawz(w, ",\"toolConfig\":{\"tools\":[");
        for (i = 0; i < tx.ntl; i++) {
            jv t = tx.tl[i];
            int st;
            if (!json_get(t, "name", &v)) {
                jw_free(&sch);
                goto out;
            }
            jw_rawz(w, i ? ",{\"toolSpec\":{\"name\":" : "{\"toolSpec\":{\"name\":");
            jw_raw(w, v.p, v.n);
            st = llm_tool_strict(t, md->compat.strict_tools == 1, LCS_NONE, &sch, 0, 0);
            if (st < 0) {
                jw_free(&sch);
                goto out;
            }
            jw_rawz(w, ",\"inputSchema\":{\"json\":");
            if (st == 1)
                jw_raw(w, sch.p, sch.n);
            else if (json_get(t, "parameters", &v))
                jw_raw(w, v.p, v.n);
            else
                jw_rawz(w, "{\"type\":\"object\"}");
            jw_rawz(w, "}");
            if (json_get(t, "description", &v)) {
                jw_rawz(w, ",\"description\":");
                jw_raw(w, v.p, v.n);
            }
            if (st == 1)
                jw_rawz(w, ",\"strict\":true");
            jw_rawz(w, "}}");
        }
        jw_rawz(w, "]}");
        jw_free(&sch);
    }
    w_fields(w, md, lv, budget);
    jw_rawz(w, "}");
    rc = w->oom ? -1 : 0;
out:
    llm_tx_free(&tx);
    return rc;
}

/* ---- the decoder ---- */

typedef struct bd_blk {
    long bidx;                  /* the API's contentBlockIndex; -1 once the block stopped */
    long cidx;                  /* our contentIndex */
    int kind;                   /* LB_TEXT, LB_THINKING, LB_TOOLCALL */
    int redacted;
    unsigned char *rb;          /* the encrypted reasoning so far (a thinking block's redactedContent chunks) */
    long rn, rcap;
    int ended;
} bd_blk;

typedef struct bd_dec {
    int started, done;
    int stop;                   /* LS_PENDING until a messageStop */
    char raw[48];
    char err[200];              /* the failing stop reason's text */
    bd_blk *b;
    int nb, cap;
    long next;
} bd_dec;

static void *b_new(void)
{
    bd_dec *d = (bd_dec *)malloc(sizeof(bd_dec));
    if (d) {
        memset(d, 0, sizeof(*d));
        d->stop = LS_PENDING;
    }
    return d;
}

static void b_free(void *dv)
{
    bd_dec *d = (bd_dec *)dv;
    int i;
    if (!d)
        return;
    for (i = 0; i < d->nb; i++)
        free(d->b[i].rb);
    free(d->b);
    free(d);
}

static void emit(const llm_sink *k, const llm_event *e)
{
    if (k && k->ev)
        k->ev(k->u, e);
}

static void bfail(bd_dec *d, const llm_sink *k, const char *msg, const char *raw)
{
    llm_event e;
    llm_event_init(&e, LE_ERROR);
    e.stop = LS_ERROR;
    e.err_type = "error";
    e.p = msg;
    e.n = (long)strlen(msg);
    e.stop_raw = raw && *raw ? raw : 0;
    emit(k, &e);
    d->done = 1;
}

/* the first block still carrying contentBlockIndex idx (blocks.findIndex(b => b.index === idx)) */
static bd_blk *find(bd_dec *d, long idx)
{
    int i;
    for (i = 0; i < d->nb; i++)
        if (d->b[i].bidx == idx)
            return &d->b[i];
    return 0;
}

static bd_blk *add(bd_dec *d, long bidx, int kind)
{
    bd_blk *b;
    if (d->nb == d->cap) {
        int nc = d->cap ? d->cap * 2 : 8;
        bd_blk *g = (bd_blk *)realloc(d->b, (size_t)nc * sizeof(*g));
        if (!g)
            return 0;
        d->b = g;
        d->cap = nc;
    }
    b = &d->b[d->nb++];
    memset(b, 0, sizeof(*b));
    b->bidx = bidx;
    b->cidx = d->next++;
    b->kind = kind;
    return b;
}

/* flushRedactedContent: the buffered encrypted reasoning becomes the block's signature (base64 of all the bytes) */
static void flush_redacted(const llm_sink *k, bd_blk *b)
{
    llm_event e;
    jw t;
    if (b->kind != LB_THINKING || !b->redacted || !b->rb)
        return;
    jw_init(&t);
    jw_base64(&t, b->rb, b->rn);
    llm_event_init(&e, LE_SIG_DELTA);
    e.index = b->cidx;
    e.p = t.p ? t.p : "";
    e.n = t.n;
    e.replace = 1;
    emit(k, &e);
    jw_free(&t);
    free(b->rb);
    b->rb = 0;
    b->rn = b->rcap = 0;
}

static void end_block(const llm_sink *k, bd_blk *b)
{
    llm_event e;
    flush_redacted(k, b);
    if (b->ended)
        return;
    llm_event_init(&e, LE_BLOCK_END);
    e.index = b->cidx;
    emit(k, &e);
    b->ended = 1;
}

static void start(bd_dec *d, const llm_sink *k)
{
    llm_event e;
    llm_event_init(&e, LE_START);
    e.p = "";
    e.n = 0;
    emit(k, &e);
    d->started = 1;
}

static void delta(const llm_sink *k, long idx, const char *p, long n)
{
    llm_event e;
    if (n <= 0)
        return;
    llm_event_init(&e, LE_DELTA);
    e.index = idx;
    e.p = p;
    e.n = n;
    emit(k, &e);
}

static void block_start(const llm_sink *k, bd_blk *b, const char *id, const char *name)
{
    llm_event e;
    llm_event_init(&e, LE_BLOCK_START);
    e.kind = b->kind;
    e.index = b->cidx;
    e.redacted = b->redacted;
    e.id = id;
    e.name = name;
    emit(k, &e);
}

static void on_delta(bd_dec *d, const llm_sink *k, jv root)
{
    jv v, dl, tx, tu, rc, x;
    long bidx = 0;
    bd_blk *b;
    if (json_get(root, "contentBlockIndex", &v))
        bidx = json_long(v, 0);
    if (!json_get(root, "delta", &dl) || json_type(dl) != J_OBJ)
        return;
    b = find(d, bidx);
    if (json_get(dl, "text", &tx) && json_type(tx) == J_STR) {
        char *s;
        long l;
        if (!b) {
            b = add(d, bidx, LB_TEXT);
            if (!b)
                return;
            block_start(k, b, 0, 0);
        }
        if (b->kind == LB_TEXT) {
            s = json_strdup(tx, &l);
            if (s) {
                delta(k, b->cidx, s, l);
                free(s);
            }
        }
    } else if (json_get(dl, "toolUse", &tu) && json_type(tu) == J_OBJ && b && b->kind == LB_TOOLCALL) {
        char *s;
        long l;
        if (json_get(tu, "input", &x) && json_type(x) == J_STR) {
            s = json_strdup(x, &l);
            if (s) {
                delta(k, b->cidx, s, l);
                free(s);
            }
        }
    } else if (json_get(dl, "reasoningContent", &rc) && json_type(rc) == J_OBJ) {
        jv red;
        int has_red = json_get(rc, "redactedContent", &red) && json_type(red) == J_STR && red.n > 2;
        if (!b) {
            b = add(d, bidx, LB_THINKING);
            if (!b)
                return;
            block_start(k, b, 0, 0);
        }
        if (b->kind == LB_THINKING) {
            char *s;
            long l;
            if (json_get(rc, "text", &x) && json_type(x) == J_STR && x.n > 2) {
                s = json_strdup(x, &l);
                if (s) {
                    delta(k, b->cidx, s, l);
                    free(s);
                }
            }
            if (json_get(rc, "signature", &x) && json_type(x) == J_STR && x.n > 2 && !b->redacted) {
                llm_event e;
                llm_event_init(&e, LE_SIG_DELTA);
                e.index = b->cidx;
                e.p = x.p + 1;          /* a signature is base64: nothing to unescape */
                e.n = x.n - 2;
                emit(k, &e);
            }
            if (has_red) {
                unsigned char *bytes;
                long bl = 0, sl;
                char *s2;
                if (!b->redacted) {
                    /* the first chunk of encrypted reasoning: the block is redacted, its signature starts over and a
                     * placeholder joins the text */
                    static const char ph[] = "[Reasoning redacted]";
                    llm_event e;
                    b->redacted = 1;
                    llm_event_init(&e, LE_SIG_DELTA);
                    e.index = b->cidx;
                    e.p = "";
                    e.n = 0;
                    e.replace = 1;
                    e.redacted = 1;
                    emit(k, &e);
                    delta(k, b->cidx, ph, (long)sizeof(ph) - 1);
                }
                s2 = json_strdup(red, &sl);
                bytes = s2 ? b64_decode(s2, sl, &bl) : 0;
                free(s2);
                if (bytes && bl > 0) {
                    if (b->rn + bl > b->rcap) {
                        long nc = (b->rn + bl) * 2;
                        unsigned char *g = (unsigned char *)realloc(b->rb, (size_t)nc);
                        if (!g) {
                            free(bytes);
                            return;
                        }
                        b->rb = g;
                        b->rcap = nc;
                    }
                    memcpy(b->rb + b->rn, bytes, (size_t)bl);
                    b->rn += bl;
                }
                free(bytes);
            }
        }
    }
}

/* the text of a failed stop reason (mapStopReason) */
static int map_stop(const char *r, char *msg, long cap)
{
    msg[0] = 0;
    if (!strcmp(r, "end_turn") || !strcmp(r, "stop_sequence"))
        return LS_STOP;
    if (!strcmp(r, "max_tokens") || !strcmp(r, "model_context_window_exceeded"))
        return LS_LENGTH;
    if (!strcmp(r, "tool_use"))
        return LS_TOOLUSE;
    if (r[0]) {
        cl_copy(msg, "Provider stopped with: ", cap);
        cl_cat(msg, r, cap);
    }
    return LS_ERROR;
}

/* an exception of the stream, as formatBedrockError words it: the modeled ones with their prefix, any other as the
 * JSON object the SDK throws */
static void exception_text(const char *type, const char *data, long n, char *out, long cap)
{
    static const struct { const char *event, *prefix; } map[] = {
        { "internalServerException", "Internal server error" },
        { "modelStreamErrorException", "Model stream error" },
        { "validationException", "Validation error" },
        { "throttlingException", "Throttling error" },
        { "serviceUnavailableException", "Service unavailable" },
    };
    jv root, v;
    char msg[400];
    int i;
    if (json_parse(data, n, &root) || json_type(root) != J_OBJ) {
        cl_copy(out, type, cap);
        return;
    }
    for (i = 0; i < 5; i++)
        if (!strcmp(type, map[i].event)) {
            if (json_get(root, "message", &v) && json_type(v) == J_STR)
                json_str(v, msg, sizeof(msg));
            else
                cl_copy(msg, "Unknown", sizeof(msg));
            cl_copy(out, map[i].prefix, cap);
            cl_cat(out, ": ", cap);
            cl_cat(out, msg, cap);
            return;
        }
    {
        jw w;
        jw_init(&w);
        llm_json_compact(&w, root, 0);
        cl_copy(out, w.p ? w.p : "", cap);
        jw_free(&w);
    }
}

/* one event of the stream: event is ":event-type" of an event, "!" and the ":exception-type" of an exception */
static int b_feed(void *dv, const char *event, const char *data, long n, const llm_sink *k)
{
    bd_dec *d = (bd_dec *)dv;
    jv root, v, x, st, tu;
    llm_feeds_bedrock++;
    if (!d || d->done)
        return 0;
    if (json_parse(data, n, &root) || json_type(root) != J_OBJ)
        return -1;
    if (event[0] == '!') {
        char msg[500];
        int i;
        exception_text(event + 1, data, n, msg, sizeof(msg));
        for (i = 0; i < d->nb; i++)
            end_block(k, &d->b[i]);
        if (!d->started)
            start(d, k);
        bfail(d, k, msg, 0);
        return 0;
    }
    if (!strcmp(event, "messageStart")) {
        if (!json_get(root, "role", &v) || !json_streq(v, "assistant")) {
            bfail(d, k, "Unexpected assistant message start but got user message start instead", 0);
            return 0;
        }
        if (!d->started)
            start(d, k);
    } else if (!strcmp(event, "contentBlockStart")) {
        long bidx = json_get(root, "contentBlockIndex", &v) ? json_long(v, 0) : 0;
        if (json_get(root, "start", &st) && json_type(st) == J_OBJ && json_get(st, "toolUse", &tu) &&
            json_type(tu) == J_OBJ) {
            char id[200], name[120];
            bd_blk *b = add(d, bidx, LB_TOOLCALL);
            id[0] = name[0] = 0;
            if (json_get(tu, "toolUseId", &x) && json_type(x) == J_STR)
                json_str(x, id, sizeof(id));
            if (json_get(tu, "name", &x) && json_type(x) == J_STR)
                json_str(x, name, sizeof(name));
            if (b)
                block_start(k, b, id, name);
        }
    } else if (!strcmp(event, "contentBlockDelta")) {
        on_delta(d, k, root);
    } else if (!strcmp(event, "contentBlockStop")) {
        bd_blk *b = json_get(root, "contentBlockIndex", &v) ? find(d, json_long(v, 0)) : find(d, 0);
        if (b) {
            b->bidx = -1;
            end_block(k, b);
        }
    } else if (!strcmp(event, "messageStop")) {
        char r[48];
        r[0] = 0;
        if (json_get(root, "stopReason", &v) && json_type(v) == J_STR)
            json_str(v, r, sizeof(r));
        cl_copy(d->raw, r, sizeof(d->raw));
        d->stop = map_stop(r, d->err, sizeof(d->err));
    } else if (!strcmp(event, "metadata")) {
        jv u, cd;
        if (json_get(root, "usage", &u) && json_type(u) == J_OBJ) {
            llm_event e;
            llm_event_init(&e, LE_USAGE);
            e.in = json_get(u, "inputTokens", &v) ? json_long(v, 0) : 0;
            e.out = json_get(u, "outputTokens", &v) ? json_long(v, 0) : 0;
            e.cache_r = json_get(u, "cacheReadInputTokens", &v) ? json_long(v, 0) : 0;
            e.cache_w = json_get(u, "cacheWriteInputTokens", &v) ? json_long(v, 0) : 0;
            /* usage.totalTokens = event.usage.totalTokens || input + output */
            e.total = json_get(u, "totalTokens", &v) ? json_long(v, 0) : 0;
            if (!e.total)
                e.total = e.in + e.out;
            e.has_total = 1;
            if (json_get(u, "cacheDetails", &cd) && json_type(cd) == J_ARR) {
                jit it;
                jv det;
                long one = 0;
                json_iter(cd, &it);
                while (json_next(&it, 0, &det))
                    if (json_get(det, "ttl", &x) && json_streq(x, "1h") && json_get(det, "inputTokens", &v))
                        one += json_long(v, 0);
                e.cache_w1h = one;
                e.has_w1h = 1;
            }
            emit(k, &e);
        }
    }                           /* any other event: the SDK's $unknown, ignored */
    return 0;
}

/* the body ended (the loop over the SDK's stream ran out): a stop reason is needed, an error one fails */
static void b_fin(void *dv, const llm_sink *k)
{
    bd_dec *d = (bd_dec *)dv;
    llm_event e;
    int i;
    if (!d || d->done)
        return;
    if (d->stop == LS_PENDING) {
        for (i = 0; i < d->nb; i++)
            end_block(k, &d->b[i]);
        bfail(d, k, "Bedrock stream ended without a stop reason", 0);
        return;
    }
    if (d->stop == LS_ERROR) {
        for (i = 0; i < d->nb; i++)
            end_block(k, &d->b[i]);
        bfail(d, k, d->err[0] ? d->err : "An unknown error occurred", d->raw);
        return;
    }
    /* a stream can settle without stopping every block: finalize here too */
    for (i = 0; i < d->nb; i++)
        end_block(k, &d->b[i]);
    llm_event_init(&e, LE_STOP);
    e.stop = d->stop;
    e.stop_raw = d->raw[0] ? d->raw : 0;
    emit(k, &e);
    llm_event_init(&e, LE_DONE);
    e.stop = d->stop;
    emit(k, &e);
    d->done = 1;
}

/* ---- the text of a failed HTTP answer (the SDK's error for the status, formatBedrockError) ---- */

static void b_http_err(int status, const char *errtype, const char *body, long bn, char *out, long cap)
{
    static const struct { const char *name, *prefix; } map[] = {
        { "InternalServerException", "Internal server error" },
        { "ModelStreamErrorException", "Model stream error" },
        { "ValidationException", "Validation error" },
        { "ThrottlingException", "Throttling error" },
        { "ServiceUnavailableException", "Service unavailable" },
    };
    jv root, v;
    char name[100], msg[400], num[16];
    int i, have = 0;
    name[0] = msg[0] = 0;
    /* the code: x-amzn-errortype up to ":" or "#", else the body's code / __type after "#" */
    if (errtype && *errtype) {
        cl_copy(name, errtype, sizeof(name));
        for (i = 0; name[i]; i++)
            if (name[i] == ':' || name[i] == '#') {
                if (name[i] == '#') {
                    memmove(name, name + i + 1, strlen(name + i));
                    i = -1;
                    continue;
                }
                name[i] = 0;
                break;
            }
    }
    if (body && bn > 0 && json_parse(body, bn, &root) == 0 && json_type(root) == J_OBJ) {
        have = 1;
        if (!name[0]) {
            if ((json_get(root, "code", &v) || json_get(root, "__type", &v)) && json_type(v) == J_STR) {
                char *t;
                json_str(v, name, sizeof(name));
                t = strrchr(name, '#');
                if (t)
                    memmove(name, t + 1, strlen(t));
                t = strchr(name, ':');
                if (t)
                    *t = 0;
            }
        }
        if ((json_get(root, "message", &v) || json_get(root, "Message", &v)) && json_type(v) == J_STR)
            json_str(v, msg, sizeof(msg));
    } else if (body && bn > 0) {
        /* not JSON (a gateway's page): the status and what it said */
        long n = bn < 300 ? bn : 300;
        cl_ltoa(status, num);
        cl_copy(out, "HTTP ", cap);
        cl_cat(out, num, cap);
        cl_cat(out, ": ", cap);
        {
            long len = (long)strlen(out);
            if (len + n >= cap)
                n = cap - len - 1;
            memcpy(out + len, body, (size_t)n);
            out[len + n] = 0;
        }
        return;
    }
    (void)have;
    if (!msg[0])
        cl_copy(msg, "UnknownError", sizeof(msg));
    if (!name[0])
        cl_copy(name, "Unknown", sizeof(name));
    for (i = 0; i < 5; i++)
        if (!strcmp(name, map[i].name)) {
            cl_copy(out, map[i].prefix, cap);
            goto joined;
        }
    cl_copy(out, name, cap);
joined:
    cl_cat(out, ": ", cap);
    cl_cat(out, msg, cap);
    if (strstr(msg, "data retention mode") || strstr(msg, "Data retention mode") || strstr(msg, "DATA RETENTION MODE"))
        cl_cat(out, " See https://docs.aws.amazon.com/bedrock/latest/userguide/data-retention.html for supported data retention modes.", cap);
}

const llm_backend llm_be_bedrock = {
    "bedrock-converse-stream", "Authorization", "/model", b_head, b_body, 0, b_new, b_feed, b_free,
    "Bedrock stream ended without a stop reason", b_route, b_fin, b_sign, b_http_err
};
