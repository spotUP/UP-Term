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
    llm_anth_init(&s->dec);
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
    llm_anth_free(&s->dec);
}

static sblock *find(cl_stream *s, long index)
{
    int i;
    for (i = s->nb - 1; i >= 0; i--)
        if (s->b[i].index == index)
            return &s->b[i];
    return 0;
}

/* the neutral block's kind back to C:Claude's block type */
static int block_type(const llm_event *e)
{
    if (e->kind == LB_TEXT)
        return B_TEXT;
    if (e->kind == LB_THINKING)
        return e->redacted ? B_REDACTED : B_THINKING;
    if (e->kind == LB_TOOLCALL)
        return B_TOOL;
    if (e->raw_type && !strcmp(e->raw_type, "fallback"))
        return B_FALLBACK;
    if (e->raw_type && !strcmp(e->raw_type, "server_tool_use"))
        return B_SERVER;
    return B_OTHER;
}

static void block_start(cl_stream *s, const llm_event *e)
{
    sblock *b;
    if (s->nb == s->cap) {
        int c = s->cap ? s->cap * 2 : 8;
        sblock *nb = (sblock *)realloc(s->b, (size_t)c * sizeof(sblock));
        if (!nb) {
            s->oom = 1;
            s->fail = 1;
            return;
        }
        s->b = nb;
        s->cap = c;
    }
    b = &s->b[s->nb++];
    memset(b, 0, sizeof(*b));
    jw_init(&b->a);
    jw_init(&b->sig);
    jw_init(&b->start);
    b->index = e->index;
    b->type = block_type(e);
    jw_raw(&b->start, e->raw, e->rn);
    switch (b->type) {
    case B_TEXT:
        jw_raw(&b->a, e->p, e->n);
        break;
    case B_THINKING:
        jw_raw(&b->a, e->p, e->n);
        jw_raw(&b->sig, e->sig, e->sn);
        break;
    case B_TOOL:
    case B_SERVER:
        cl_copy(b->id, e->id ? e->id : "", sizeof(b->id));
        cl_copy(b->name, e->name ? e->name : "", sizeof(b->name));
        break;
    }
    if (s->ui.block)
        s->ui.block(s->ui.u, b->type, b->name);
    if (b->type == B_TEXT && b->a.n && s->ui.text)
        s->ui.text(s->ui.u, b->a.p, b->a.n);
}

static void on_event(void *u, const llm_event *e)
{
    cl_stream *s = (cl_stream *)u;
    sblock *b;
    switch (e->type) {
    case LE_START:
        cl_copy(s->model, e->model ? e->model : "", sizeof(s->model));
        cl_copy(s->id, e->p ? e->p : "", sizeof(s->id));
        s->state = ST_OPEN;
        break;
    case LE_USAGE:
        if (e->in >= 0)
            s->in_tok = e->in;
        if (e->out >= 0)
            s->out_tok = e->out;
        if (e->cache_w >= 0)
            s->cache_w = e->cache_w;
        if (e->cache_r >= 0)
            s->cache_r = e->cache_r;
        break;
    case LE_BLOCK_START:
        block_start(s, e);
        break;
    case LE_DELTA:
        b = find(s, e->index);
        if (!b)
            break;
        jw_raw(&b->a, e->p, e->n);
        if (b->type == B_TEXT && s->ui.text && e->n)
            s->ui.text(s->ui.u, e->p, e->n);
        else if (b->type == B_THINKING && s->ui.thinking && e->n)
            s->ui.thinking(s->ui.u, e->p, e->n);
        break;
    case LE_SIG_DELTA:
        b = find(s, e->index);
        if (b)
            jw_raw(&b->sig, e->p, e->n);
        break;
    case LE_BLOCK_END:
        b = find(s, e->index);
        if (!b)
            break;
        b->done = 1;
        if (b->type == B_TOOL || b->type == B_SERVER) {
            jv in;
            b->input_ok = b->a.n && json_parse(b->a.p, b->a.n, &in) == 0 && json_type(in) == J_OBJ;
        }
        if (s->ui.stop)
            s->ui.stop(s->ui.u, b);
        break;
    case LE_STOP:
        if (e->stop_raw && *e->stop_raw)
            cl_copy(s->stop_reason, e->stop_raw, sizeof(s->stop_reason));
        if (e->stop_detail && *e->stop_detail)
            cl_copy(s->stop_category, e->stop_detail, sizeof(s->stop_category));
        break;
    case LE_DONE:
        s->state = ST_DONE;
        break;
    case LE_ERROR:
        cl_copy(s->err_type, e->err_type ? e->err_type : "error", sizeof(s->err_type));
        if (e->p && e->n) {
            long n = e->n < (long)sizeof(s->err_msg) - 1 ? e->n : (long)sizeof(s->err_msg) - 1;
            memcpy(s->err_msg, e->p, (size_t)n);
            s->err_msg[n] = 0;
        }
        s->state = ST_ERROR;
        break;
    }
}

int stream_event(cl_stream *s, const char *event, const char *data, long n)
{
    llm_sink k;
    int rc;
    (void)event;                    /* the data's own "type" decides */
    if (s->state == ST_BAD)
        return -1;
    k.u = s;
    k.ev = on_event;
    rc = llm_anth_feed(&s->dec, data, n, &k);
    if (s->dec.oom)
        s->oom = 1;
    if (rc || s->fail) {
        s->state = ST_BAD;
        return -1;
    }
    return 0;
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
        case B_SERVER:
            jw_rawz(out, b->type == B_TOOL ? "{\"type\":\"tool_use\",\"id\":" : "{\"type\":\"server_tool_use\",\"id\":");
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
