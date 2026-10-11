/* be_anthropic -- see be_anthropic.h. */
#include <stdlib.h>
#include <string.h>
#include "be_anthropic.h"
#include "../claude/http.h"
#include "../claude/util.h"

long llm_feeds_anthropic;

/* ---- the request head ---- */

long http_request_head(const http_req *r, char *out, long cap)
{
    http_hdr h[6];
    h[0].name = "User-Agent";
    h[0].value = LLM_UA_CLAUDE;
    h[1].name = "Accept";
    h[1].value = "text/event-stream";
    h[2].name = "Content-Type";
    h[2].value = "application/json";
    h[3].name = "anthropic-version";
    h[3].value = "2023-06-01";
    h[4].name = "anthropic-beta";
    h[4].value = r->beta;
    h[5].name = "x-api-key";
    h[5].value = r->key;
    return http_post_head(r->host, r->port, r->tls, r->path, h, 6, r->body_len, out, cap);
}

long http_redact(const char *head, long n, char *out, long cap)
{
    return http_redact_header(head, n, "x-api-key", out, cap);
}

/* ---- the decoder ---- */

void llm_anth_init(llm_anth_dec *d)
{
    memset(d, 0, sizeof(*d));
    d->state = AS_WAIT;
}

void llm_anth_free(llm_anth_dec *d)
{
    int i;
    for (i = 0; i < d->nb; i++)
        jw_free(&d->b[i].start);
    free(d->b);
    d->b = 0;
    d->nb = d->cap = 0;
}

static void emit(const llm_sink *k, const llm_event *e)
{
    if (k && k->ev)
        k->ev(k->u, e);
}

static void str_field(jv obj, const char *key, char *out, long cap)
{
    jv v;
    if (json_get(obj, key, &v) && json_type(v) == J_STR)
        json_str(v, out, cap);
}

static void usage(const llm_sink *k, jv u)
{
    llm_event e;
    jv v;
    llm_event_init(&e, LE_USAGE);
    if (json_get(u, "input_tokens", &v))
        e.in = json_long(v, -1);
    if (json_get(u, "output_tokens", &v))
        e.out = json_long(v, -1);
    if (json_get(u, "cache_creation_input_tokens", &v))
        e.cache_w = json_long(v, -1);
    if (json_get(u, "cache_read_input_tokens", &v))
        e.cache_r = json_long(v, -1);
    emit(k, &e);
}

static llm_anth_blk *find(llm_anth_dec *d, long index)
{
    int i;
    for (i = d->nb - 1; i >= 0; i--)
        if (d->b[i].index == index)
            return &d->b[i];
    return 0;
}

static int block_type(jv cb, char *name, long cap)
{
    jv t;
    name[0] = 0;
    if (!json_get(cb, "type", &t))
        return AK_OTHER;
    json_str(t, name, cap);
    if (json_streq(t, "text"))
        return AK_TEXT;
    if (json_streq(t, "thinking"))
        return AK_THINKING;
    if (json_streq(t, "redacted_thinking"))
        return AK_REDACTED;
    if (json_streq(t, "tool_use"))
        return AK_TOOL;
    if (json_streq(t, "fallback"))
        return AK_FALLBACK;
    if (json_streq(t, "server_tool_use"))
        return AK_SERVER;
    return AK_OTHER;
}

static int block_start(llm_anth_dec *d, jv ev, const llm_sink *k)
{
    jv idx, cb, v;
    llm_anth_blk *b;
    llm_event e;
    char tname[32], id[96], name[64];
    char *a = 0, *sig = 0;
    long an = 0, sn = 0;
    if (!json_get(ev, "index", &idx) || !json_get(ev, "content_block", &cb) || json_type(cb) != J_OBJ)
        return -1;
    if (d->nb == d->cap) {
        int c = d->cap ? d->cap * 2 : 8;
        llm_anth_blk *nb = (llm_anth_blk *)realloc(d->b, (size_t)c * sizeof(llm_anth_blk));
        if (!nb) {
            d->oom = 1;
            return -1;
        }
        d->b = nb;
        d->cap = c;
    }
    b = &d->b[d->nb++];
    memset(b, 0, sizeof(*b));
    jw_init(&b->start);
    b->index = json_long(idx, -1);
    b->type = block_type(cb, tname, sizeof(tname));
    jw_raw(&b->start, cb.p, cb.n);
    llm_event_init(&e, LE_BLOCK_START);
    e.index = b->index;
    e.raw = cb.p;
    e.rn = cb.n;
    id[0] = name[0] = 0;
    switch (b->type) {
    case AK_TEXT:
        e.kind = LB_TEXT;
        if (json_get(cb, "text", &v) && !(a = json_strdup(v, &an)))
            d->oom = 1;
        break;
    case AK_THINKING:
        e.kind = LB_THINKING;
        if (json_get(cb, "thinking", &v) && !(a = json_strdup(v, &an)))
            d->oom = 1;
        if (json_get(cb, "signature", &v) && !(sig = json_strdup(v, &sn)))
            d->oom = 1;
        break;
    case AK_REDACTED:
        /* pi-ai: a thinking block marked redacted, the data as its signature */
        e.kind = LB_THINKING;
        e.redacted = 1;
        if (json_get(cb, "data", &v) && !(sig = json_strdup(v, &sn)))
            d->oom = 1;
        break;
    case AK_TOOL:
        e.kind = LB_TOOLCALL;
        str_field(cb, "id", id, sizeof(id));
        str_field(cb, "name", name, sizeof(name));
        break;
    default:
        e.kind = LB_RAW;
        e.raw_type = tname;
        if (b->type == AK_SERVER) {
            str_field(cb, "id", id, sizeof(id));
            str_field(cb, "name", name, sizeof(name));
        }
        break;
    }
    e.p = a;
    e.n = an;
    e.sig = sig;
    e.sn = sn;
    e.id = id;
    e.name = name;
    if (!d->oom)
        emit(k, &e);
    free(a);
    free(sig);
    return d->oom ? -1 : 0;
}

static int delta(const llm_sink *k, long index, int type, jv v)
{
    llm_event e;
    long l;
    char *s = json_strdup(v, &l);
    if (!s)
        return -1;
    llm_event_init(&e, type);
    e.index = index;
    e.p = s;
    e.n = l;
    emit(k, &e);
    free(s);
    return 0;
}

static int block_delta(llm_anth_dec *d, jv ev, const llm_sink *k)
{
    jv idx, dl, t, v;
    llm_anth_blk *b;
    if (!json_get(ev, "index", &idx) || !json_get(ev, "delta", &dl))
        return -1;
    b = find(d, json_long(idx, -1));
    if (!b || b->done || !json_get(dl, "type", &t))
        return -1;
    if (json_streq(t, "text_delta") && b->type == AK_TEXT && json_get(dl, "text", &v))
        return delta(k, b->index, LE_DELTA, v);
    if (json_streq(t, "input_json_delta") && (b->type == AK_TOOL || b->type == AK_SERVER) &&
        json_get(dl, "partial_json", &v)) {
        long before = 0;
        char tmp[1];
        before = json_str(v, tmp, 1);
        if (before > 0)
            b->got += before;
        return delta(k, b->index, LE_DELTA, v);
    }
    if (json_streq(t, "thinking_delta") && b->type == AK_THINKING && json_get(dl, "thinking", &v))
        return delta(k, b->index, LE_DELTA, v);
    if (json_streq(t, "signature_delta") && b->type == AK_THINKING && json_get(dl, "signature", &v))
        return delta(k, b->index, LE_SIG_DELTA, v);
    /* other delta types (citations, ...) change nothing the client keeps */
    return 0;
}

static int block_stop(llm_anth_dec *d, jv ev, const llm_sink *k)
{
    jv idx, in, cb;
    llm_anth_blk *b;
    llm_event e;
    if (!json_get(ev, "index", &idx))
        return -1;
    b = find(d, json_long(idx, -1));
    if (!b)
        return -1;
    b->done = 1;
    if ((b->type == AK_TOOL || b->type == AK_SERVER) && !b->got &&
        json_parse(b->start.p, b->start.n, &cb) == 0 && json_get(cb, "input", &in)) {
        /* no deltas: the input of content_block_start, as it came */
        llm_event_init(&e, LE_DELTA);
        e.index = b->index;
        e.p = in.p;
        e.n = in.n;
        emit(k, &e);
    }
    llm_event_init(&e, LE_BLOCK_END);
    e.index = b->index;
    emit(k, &e);
    return 0;
}

/* pi-ai's mapStopReason (pi f10993bc anthropic-messages.ts:1613-1638): the
 * neutral reason, and for an error its message into msg (C:Claude reads
 * only the raw reason) */
static int stop_of(const char *r, const char *explanation, char *msg, long cap)
{
    msg[0] = 0;
    if (!strcmp(r, "end_turn") || !strcmp(r, "stop_sequence") || !strcmp(r, "pause_turn"))
        return LS_STOP;
    if (!strcmp(r, "max_tokens"))
        return LS_LENGTH;
    if (!strcmp(r, "tool_use"))
        return LS_TOOLUSE;
    if (!r[0])
        return LS_NONE;
    if (!strcmp(r, "refusal"))
        cl_copy(msg, explanation[0] ? explanation : "The model refused to complete the request", cap);
    else if (!strcmp(r, "sensitive"))
        cl_copy(msg, "Provider stopped with: sensitive", cap);
    else {
        cl_copy(msg, "Unhandled stop reason: ", cap);
        cl_cat(msg, r, cap);
    }
    return LS_ERROR;
}

int llm_anth_feed(llm_anth_dec *d, const char *data, long n, const llm_sink *k)
{
    jv ev, t, v, u;
    int rc = 0;
    llm_event e;
    llm_feeds_anthropic++;
    if (d->state == AS_BAD)
        return -1;
    if (json_parse(data, n, &ev) || !json_get(ev, "type", &t)) {
        d->state = AS_BAD;
        return -1;
    }
    if (json_streq(t, "ping"))
        return 0;
    if (json_streq(t, "error")) {
        jv er;
        char et[48], em[256];
        et[0] = em[0] = 0;
        if (json_get(ev, "error", &er)) {
            str_field(er, "type", et, sizeof(et));
            str_field(er, "message", em, sizeof(em));
        }
        llm_event_init(&e, LE_ERROR);
        e.stop = LS_ERROR;
        e.err_type = et[0] ? et : "error";
        e.p = em;
        e.n = (long)strlen(em);
        emit(k, &e);
        d->state = AS_ERROR;
        return 0;
    }
    if (json_streq(t, "message_start")) {
        if (d->state != AS_WAIT || !json_get(ev, "message", &v))
            rc = -1;
        else {
            char model[64], id[96];
            model[0] = id[0] = 0;
            str_field(v, "model", model, sizeof(model));
            str_field(v, "id", id, sizeof(id));
            llm_event_init(&e, LE_START);
            e.model = model;
            e.p = id;
            e.n = (long)strlen(id);
            emit(k, &e);
            if (json_get(v, "usage", &u))
                usage(k, u);
            d->state = AS_OPEN;
        }
    } else if (d->state != AS_OPEN)
        rc = -1;
    else if (json_streq(t, "content_block_start"))
        rc = block_start(d, ev, k);
    else if (json_streq(t, "content_block_delta"))
        rc = block_delta(d, ev, k);
    else if (json_streq(t, "content_block_stop"))
        rc = block_stop(d, ev, k);
    else if (json_streq(t, "message_delta")) {
        if (json_get(ev, "delta", &v)) {
            jv sd;
            char sr[32], cat[32], expl[200], msg[240];
            sr[0] = cat[0] = expl[0] = 0;
            str_field(v, "stop_reason", sr, sizeof(sr));
            if (json_get(v, "stop_details", &sd) && json_type(sd) == J_OBJ) {
                str_field(sd, "category", cat, sizeof(cat));
                str_field(sd, "explanation", expl, sizeof(expl));
            }
            llm_event_init(&e, LE_STOP);
            e.stop = stop_of(sr, expl, msg, sizeof(msg));
            e.p = msg;
            e.n = (long)strlen(msg);
            e.stop_raw = sr;
            e.stop_detail = cat;
            emit(k, &e);
        }
        if (json_get(ev, "usage", &u))
            usage(k, u);
    } else if (json_streq(t, "message_stop")) {
        llm_event_init(&e, LE_DONE);
        emit(k, &e);
        d->state = AS_DONE;
    }
    /* an event type the client does not know is passed over */
    if (rc)
        d->state = AS_BAD;
    return rc;
}

