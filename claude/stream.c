/* stream -- see stream.h. */
#include <stdlib.h>
#include <string.h>
#include "stream.h"
#include "util.h"

void stream_init(cl_stream *s, const cl_stream_ui *ui)
{
    memset(s, 0, sizeof(*s));
    if (ui)
        s->ui = *ui;
    s->state = ST_WAIT;
}

void stream_free(cl_stream *s)
{
    int i;
    for (i = 0; i < s->nb; i++) {
        jw_free(&s->b[i].a);
        jw_free(&s->b[i].sig);
        jw_free(&s->b[i].start);
    }
    free(s->b);
    s->b = 0;
    s->nb = s->cap = 0;
}

static void str_field(jv obj, const char *key, char *out, long cap)
{
    jv v;
    if (json_get(obj, key, &v) && json_type(v) == J_STR)
        json_str(v, out, cap);
}

static void usage(cl_stream *s, jv u)
{
    jv v;
    if (json_get(u, "input_tokens", &v))
        s->in_tok = json_long(v, s->in_tok);
    if (json_get(u, "output_tokens", &v))
        s->out_tok = json_long(v, s->out_tok);
    if (json_get(u, "cache_creation_input_tokens", &v))
        s->cache_w = json_long(v, s->cache_w);
    if (json_get(u, "cache_read_input_tokens", &v))
        s->cache_r = json_long(v, s->cache_r);
}

static sblock *find(cl_stream *s, long index)
{
    int i;
    for (i = s->nb - 1; i >= 0; i--)
        if (s->b[i].index == index)
            return &s->b[i];
    return 0;
}

static int block_type(jv cb)
{
    jv t;
    if (!json_get(cb, "type", &t))
        return B_OTHER;
    if (json_streq(t, "text"))
        return B_TEXT;
    if (json_streq(t, "thinking"))
        return B_THINKING;
    if (json_streq(t, "redacted_thinking"))
        return B_REDACTED;
    if (json_streq(t, "tool_use"))
        return B_TOOL;
    if (json_streq(t, "fallback"))
        return B_FALLBACK;
    return B_OTHER;
}

static void append_str(jw *w, jv v)
{
    long l;
    char *d = json_strdup(v, &l);
    if (!d) {
        w->oom = 1;
        return;
    }
    jw_raw(w, d, l);
    free(d);
}

static int block_start(cl_stream *s, jv ev)
{
    jv idx, cb, v;
    sblock *b;
    if (!json_get(ev, "index", &idx) || !json_get(ev, "content_block", &cb) || json_type(cb) != J_OBJ)
        return -1;
    if (s->nb == s->cap) {
        int c = s->cap ? s->cap * 2 : 8;
        sblock *nb = (sblock *)realloc(s->b, (size_t)c * sizeof(sblock));
        if (!nb) {
            s->oom = 1;
            return -1;
        }
        s->b = nb;
        s->cap = c;
    }
    b = &s->b[s->nb++];
    memset(b, 0, sizeof(*b));
    jw_init(&b->a);
    jw_init(&b->sig);
    jw_init(&b->start);
    b->index = json_long(idx, -1);
    b->type = block_type(cb);
    jw_raw(&b->start, cb.p, cb.n);
    switch (b->type) {
    case B_TEXT:
        if (json_get(cb, "text", &v))
            append_str(&b->a, v);
        break;
    case B_THINKING:
        if (json_get(cb, "thinking", &v))
            append_str(&b->a, v);
        if (json_get(cb, "signature", &v))
            append_str(&b->sig, v);
        break;
    case B_TOOL:
        str_field(cb, "id", b->id, sizeof(b->id));
        str_field(cb, "name", b->name, sizeof(b->name));
        break;
    }
    if (s->ui.block)
        s->ui.block(s->ui.u, b->type, b->name);
    if (b->type == B_TEXT && b->a.n && s->ui.text)
        s->ui.text(s->ui.u, b->a.p, b->a.n);
    return 0;
}

static int block_delta(cl_stream *s, jv ev)
{
    jv idx, d, t, v;
    sblock *b;
    if (!json_get(ev, "index", &idx) || !json_get(ev, "delta", &d))
        return -1;
    b = find(s, json_long(idx, -1));
    if (!b || b->done || !json_get(d, "type", &t))
        return -1;
    if (json_streq(t, "text_delta") && b->type == B_TEXT && json_get(d, "text", &v)) {
        long l;
        char *txt = json_strdup(v, &l);
        if (!txt)
            return -1;
        jw_raw(&b->a, txt, l);
        if (s->ui.text && l)
            s->ui.text(s->ui.u, txt, l);
        free(txt);
    } else if (json_streq(t, "input_json_delta") && b->type == B_TOOL && json_get(d, "partial_json", &v))
        append_str(&b->a, v);
    else if (json_streq(t, "thinking_delta") && b->type == B_THINKING && json_get(d, "thinking", &v))
        append_str(&b->a, v);
    else if (json_streq(t, "signature_delta") && b->type == B_THINKING && json_get(d, "signature", &v))
        append_str(&b->sig, v);
    /* other delta types (citations, ...) change nothing the client keeps */
    return 0;
}

static int block_stop(cl_stream *s, jv ev)
{
    jv idx, in;
    sblock *b;
    if (!json_get(ev, "index", &idx))
        return -1;
    b = find(s, json_long(idx, -1));
    if (!b)
        return -1;
    b->done = 1;
    if (b->type == B_TOOL) {
        if (!b->a.n) {
            /* no deltas: the input of content_block_start */
            jv cb;
            if (json_parse(b->start.p, b->start.n, &cb) == 0 && json_get(cb, "input", &in))
                jw_raw(&b->a, in.p, in.n);
        }
        b->input_ok = b->a.n && json_parse(b->a.p, b->a.n, &in) == 0 && json_type(in) == J_OBJ;
    }
    return 0;
}

int stream_event(cl_stream *s, const char *event, const char *data, long n)
{
    jv ev, t, v, u;
    int rc = 0;
    (void)event;                    /* the data's own "type" decides */
    if (s->state == ST_BAD)
        return -1;
    if (json_parse(data, n, &ev) || !json_get(ev, "type", &t)) {
        s->state = ST_BAD;
        return -1;
    }
    if (json_streq(t, "ping"))
        return 0;
    if (json_streq(t, "error")) {
        jv e;
        if (json_get(ev, "error", &e)) {
            str_field(e, "type", s->err_type, sizeof(s->err_type));
            str_field(e, "message", s->err_msg, sizeof(s->err_msg));
        }
        if (!s->err_type[0])
            cl_copy(s->err_type, "error", sizeof(s->err_type));
        s->state = ST_ERROR;
        return 0;
    }
    if (json_streq(t, "message_start")) {
        if (s->state != ST_WAIT || !json_get(ev, "message", &v))
            rc = -1;
        else {
            str_field(v, "model", s->model, sizeof(s->model));
            str_field(v, "id", s->id, sizeof(s->id));
            if (json_get(v, "usage", &u))
                usage(s, u);
            s->state = ST_OPEN;
        }
    } else if (s->state != ST_OPEN)
        rc = -1;
    else if (json_streq(t, "content_block_start"))
        rc = block_start(s, ev);
    else if (json_streq(t, "content_block_delta"))
        rc = block_delta(s, ev);
    else if (json_streq(t, "content_block_stop"))
        rc = block_stop(s, ev);
    else if (json_streq(t, "message_delta")) {
        if (json_get(ev, "delta", &v)) {
            jv sd;
            str_field(v, "stop_reason", s->stop_reason, sizeof(s->stop_reason));
            if (json_get(v, "stop_details", &sd) && json_type(sd) == J_OBJ)
                str_field(sd, "category", s->stop_category, sizeof(s->stop_category));
        }
        if (json_get(ev, "usage", &u))
            usage(s, u);
    } else if (json_streq(t, "message_stop"))
        s->state = ST_DONE;
    /* an event type the client does not know is passed over */
    if (rc)
        s->state = ST_BAD;
    return rc;
}

int stream_content(cl_stream *s, jw *out)
{
    int i, last_fb = -1, first = 1;
    for (i = 0; i < s->nb; i++)
        if (s->b[i].type == B_FALLBACK)
            last_fb = i;
    jw_raw(out, "[", 1);
    for (i = 0; i < s->nb; i++) {
        sblock *b = &s->b[i];
        if (!b->done)
            continue;
        if (i < last_fb && b->type != B_TEXT && b->type != B_FALLBACK)
            continue;
        if (b->type == B_TEXT && !b->a.n)
            continue;               /* the API refuses empty text blocks */
        if (!first)
            jw_raw(out, ",", 1);
        first = 0;
        switch (b->type) {
        case B_TEXT:
            jw_rawz(out, "{\"type\":\"text\",\"text\":");
            jw_str(out, b->a.p, b->a.n);
            jw_raw(out, "}", 1);
            break;
        case B_THINKING:
            jw_rawz(out, "{\"type\":\"thinking\",\"thinking\":");
            jw_str(out, b->a.p ? b->a.p : "", b->a.n);
            jw_rawz(out, ",\"signature\":");
            jw_str(out, b->sig.p ? b->sig.p : "", b->sig.n);
            jw_raw(out, "}", 1);
            break;
        case B_TOOL:
            jw_rawz(out, "{\"type\":\"tool_use\",\"id\":");
            jw_strz(out, b->id);
            jw_rawz(out, ",\"name\":");
            jw_strz(out, b->name);
            jw_rawz(out, ",\"input\":");
            if (b->input_ok)
                jw_raw(out, b->a.p, b->a.n);
            else
                jw_rawz(out, "{}");
            jw_raw(out, "}", 1);
            break;
        default:
            jw_raw(out, b->start.p, b->start.n);
            break;
        }
    }
    jw_raw(out, "]", 1);
    return out->oom ? -1 : 0;
}

int stream_tools(cl_stream *s)
{
    int i, c = 0, last_fb = -1;
    for (i = 0; i < s->nb; i++)
        if (s->b[i].type == B_FALLBACK)
            last_fb = i;
    for (i = last_fb + 1; i < s->nb; i++)
        if (s->b[i].type == B_TOOL && s->b[i].done)
            c++;
    return c;
}

sblock *stream_tool(cl_stream *s, int k)
{
    int i, last_fb = -1;
    for (i = 0; i < s->nb; i++)
        if (s->b[i].type == B_FALLBACK)
            last_fb = i;
    for (i = last_fb + 1; i < s->nb; i++)
        if (s->b[i].type == B_TOOL && s->b[i].done && !k--)
            return &s->b[i];
    return 0;
}
