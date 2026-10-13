/* httperr -- see httperr.h. */
#include <stdlib.h>
#include <string.h>
#include "httperr.h"
#include "transcript.h"
#include "../claude/util.h"

#define BODY_MAX 4000           /* MAX_PROVIDER_ERROR_BODY_CHARS (utils/error-body.ts) */

/* JavaScript truthiness of a parsed value */
static int truthy(jv v)
{
    long i;
    switch (json_type(v)) {
    case J_NULL:
    case J_FALSE:
        return 0;
    case J_STR:
        return v.n > 2;
    case J_NUM:
        for (i = 0; i < v.n; i++) {
            char c = v.p[i];
            if (c == 'e' || c == 'E')
                break;
            if (c >= '1' && c <= '9')
                return 1;
        }
        return 0;
    default:
        return 1;
    }
}

/* JSON.stringify of a value into w */
static void stringify(jw *w, jv v)
{
    llm_json_compact(w, v, 0);
}

static void put(char *out, long cap, const jw *w)
{
    long n = w->n < cap - 1 ? w->n : cap - 1;
    if (n < 0)
        n = 0;
    if (w->p && n)
        memcpy(out, w->p, (size_t)n);
    out[n] = 0;
}

/* the text of response.text() as a string value of a message: the body bytes */
static void body_text(const llm_http_fail *f, jw *w)
{
    jw_raw(w, f->body, f->bn);
}

/* APIError.makeMessage(status, error, message) of both SDKs: error is a parsed value (have_err) or none; message the
 * body text when it was no (truthy) JSON, else none */
static void make_message(jw *o, int status, int have_err, jv err, int have_msg, const jw *msg)
{
    jw m;
    int has = 0;
    char num[16];
    jv x;
    jw_init(&m);
    if (have_err && json_type(err) == J_OBJ && json_get(err, "message", &x) && truthy(x)) {
        if (json_type(x) == J_STR) {
            char *d;
            long l;
            d = json_strdup(x, &l);
            jw_raw(&m, d ? d : "", d ? l : 0);
            free(d);
        } else
            stringify(&m, x);
        has = 1;
    } else if (have_err && truthy(err)) {
        stringify(&m, err);
        has = 1;
    } else if (have_msg && msg->n > 0) {
        jw_raw(&m, msg->p, msg->n);
        has = 1;
    }
    cl_ltoa(status, num);
    jw_rawz(o, num);
    if (has) {
        jw_rawz(o, " ");
        jw_raw(o, m.p, m.n);
    } else
        jw_rawz(o, " status code (no body)");
    jw_free(&m);
}

void llm_httperr_openai(const llm_http_fail *f, const char *prefix, int openrouter_raw, char *out, long cap)
{
    jv root, err, x;
    int parsed, have_err = 0, have_msg = 0, carries;
    jw text, message, body, res;
    char num[16];
    err.p = "";
    err.n = 0;
    jw_init(&text);
    jw_init(&message);
    jw_init(&body);
    jw_init(&res);
    body_text(f, &text);
    parsed = f->bn > 0 && json_parse(f->body, f->bn, &root) == 0 && truthy(root);
    if (parsed) {
        /* makeStatusError: an object without a non-null error member is wrapped as {error: it}; generate reads ['error'] */
        int obj = json_type(root) == J_OBJ, arr = json_type(root) == J_ARR;
        if (obj && json_get(root, "error", &x) && json_type(x) != J_NULL) {
            err = x;
            have_err = 1;
        } else if (obj || arr) {
            err = root;
            have_err = 1;
        }
    } else
        have_msg = 1;
    make_message(&message, f->status, have_err, err, have_msg, &text);
    /* normalizeProviderError: the body is error.error when it is a plain non-empty object */
    if (have_err && json_type(err) == J_OBJ && json_count(err) > 0) {
        jw s;
        jw_init(&s);
        stringify(&s, err);
        if (s.n > BODY_MAX) {
            long over = s.n - BODY_MAX;
            jw_raw(&body, s.p, BODY_MAX);
            jw_rawz(&body, "... [truncated ");
            cl_ltoa(over, num);
            jw_rawz(&body, num);
            jw_rawz(&body, " chars]");
        } else
            jw_raw(&body, s.p, s.n);
        jw_free(&s);
    }
    carries = body.n == 0 || (message.p && body.p && strstr(message.p, body.p) != 0);
    cl_ltoa(f->status, num);
    if (carries) {
        if (prefix) {
            jw_rawz(&res, prefix);
            jw_rawz(&res, " (");
            jw_rawz(&res, num);
            jw_rawz(&res, "): ");
        }
        jw_raw(&res, message.p, message.n);
    } else {
        if (prefix) {
            jw_rawz(&res, prefix);
            jw_rawz(&res, " (");
            jw_rawz(&res, num);
            jw_rawz(&res, "): ");
        } else {
            jw_rawz(&res, num);
            jw_rawz(&res, ": ");
        }
        jw_raw(&res, body.p, body.n);
    }
    if (openrouter_raw && have_err && json_type(err) == J_OBJ) {
        jv md, raw;
        if (json_get(err, "metadata", &md) && json_type(md) == J_OBJ && json_get(md, "raw", &raw) && truthy(raw)) {
            jw rs;
            jw_init(&rs);
            if (json_type(raw) == J_STR) {
                char *d;
                long l;
                d = json_strdup(raw, &l);
                jw_raw(&rs, d ? d : "", d ? l : 0);
                free(d);
            } else if (json_type(raw) == J_OBJ)
                jw_rawz(&rs, "[object Object]");
            else
                stringify(&rs, raw);
            if (!rs.p || !strstr(res.p, rs.p)) {
                jw_rawz(&res, "\n");
                jw_raw(&res, rs.p, rs.n);
            }
            jw_free(&rs);
        }
    }
    put(out, cap, &res);
    jw_free(&text);
    jw_free(&message);
    jw_free(&body);
    jw_free(&res);
}

void llm_httperr_anthropic(const llm_http_fail *f, char *out, long cap)
{
    jv root;
    int parsed, have_msg = 0;
    jw text, message;
    jw_init(&text);
    jw_init(&message);
    body_text(f, &text);
    parsed = f->bn > 0 && json_parse(f->body, f->bn, &root) == 0 && truthy(root);
    if (!parsed)
        have_msg = 1;
    if (!parsed) {
        root.p = "";
        root.n = 0;
    }
    make_message(&message, f->status, parsed, root, have_msg, &text);
    put(out, cap, &message);
    jw_free(&text);
    jw_free(&message);
}

/* V8's message for JSON.parse of a text: only the cases that matter for an error page are exact (nothing but white
 * space; a text that cannot begin a value); every other failure reads "Unexpected end of JSON input" */
static void json_syntax(const llm_http_fail *f, jw *o)
{
    long i = 0, len = f->bn, start, end;
    char c;
    while (i < len && (f->body[i] == ' ' || f->body[i] == '\t' || f->body[i] == '\n' || f->body[i] == '\r'))
        i++;
    c = i < len ? f->body[i] : 0;
    if (!c || c == '{' || c == '[' || c == '"' || c == '-' || (c >= '0' && c <= '9') || c == 't' || c == 'f' || c == 'n') {
        jw_rawz(o, "Unexpected end of JSON input");
        return;
    }
    jw_rawz(o, "Unexpected token '");
    {
        long cl = 1;
        if (((unsigned char)c & 0xE0) == 0xC0)
            cl = 2;
        else if (((unsigned char)c & 0xF0) == 0xE0)
            cl = 3;
        else if (((unsigned char)c & 0xF8) == 0xF0)
            cl = 4;
        if (i + cl > len)
            cl = len - i;
        jw_raw(o, f->body + i, cl);
    }
    jw_rawz(o, "', ");
    if (len <= 21) {
        start = 0;
        end = len;
    } else {
        start = i - 10 > 0 ? i - 10 : 0;
        end = i + 10 < len ? i + 10 : len;
    }
    jw_rawz(o, "\"");
    if (start > 0)
        jw_rawz(o, "...");
    {
        jw s;
        jw_init(&s);
        jw_str(&s, f->body + start, end - start);
        jw_raw(o, s.p + 1, s.n - 2);        /* the escaped text without its quotes */
        jw_free(&s);
    }
    jw_rawz(o, "\"");
    if (end < len)
        jw_rawz(o, "...");
    jw_rawz(o, " is not valid JSON");
}

void llm_httperr_google(const llm_http_fail *f, char *out, long cap)
{
    jw res;
    jv root;
    char num[16];
    jw_init(&res);
    if (f->ctype && strstr(f->ctype, "application/json")) {
        if (f->bn > 0 && json_parse(f->body, f->bn, &root) == 0)
            stringify(&res, root);
        else
            json_syntax(f, &res);
    } else {
        /* { error: { message: <text>, code: <status>, status: <reason phrase> } } */
        jw_rawz(&res, "{\"error\":{\"message\":");
        jw_str(&res, f->body, f->bn);
        jw_rawz(&res, ",\"code\":");
        cl_ltoa(f->status, num);
        jw_rawz(&res, num);
        jw_rawz(&res, ",\"status\":");
        jw_strz(&res, f->reason ? f->reason : "");
        jw_rawz(&res, "}}");
    }
    put(out, cap, &res);
    jw_free(&res);
}
