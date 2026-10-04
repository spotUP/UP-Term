/* repl -- see repl.h. */
#include <stdlib.h>
#include <string.h>
#include "repl.h"
#include "path.h"
#include "tui.h"
#include "show.h"
#include "util.h"

enum { R_OK, R_RETRY, R_FAIL, R_CANCEL };

#define IDLE_LIMIT_MS 180000L   /* no byte for this long: the connection is dead */

static const char sys_a[] =
    "You are Claude, running as a native client on an Amiga computer (AmigaOS 3.x, a 68020 or "
    "faster processor) in the UP-Term terminal. ";
static const char sys_b[] =
    "Paths are AmigaOS paths: a volume or an assign ends with a colon (SYS:, RAM:, Work:), "
    "directories are separated by /, and a leading / or an empty part means the parent directory. "
    "Relative paths start from the start directory: ";
static const char sys_c[] =
    ". Commands run through vsh, a Unix-like shell for AmigaOS; AmigaDOS commands work, and return "
    "codes 5, 10 and 20 mean warning, error and failure. The machine is slow and has little "
    "memory: prefer small, targeted reads and searches. ";
static const char sys_d[] =
    "Every tool call is shown to the user and may need their permission. The terminal shows "
    "Markdown, 80 columns or fewer; keep answers concise. For a task of several steps keep a "
    "todo list with todo_write.";

int cl_key_clean(char *key)
{
    char *s = key, *e;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'))
        e--;
    *e = 0;
    memmove(key, s, (size_t)(e - s) + 1);
    for (s = key; *s; s++)
        if ((unsigned char)*s <= 0x20 || (unsigned char)*s >= 0x7f)
            return -1;
    return strlen(key) >= 8 ? 0 : -1;
}

/* ---- the stream's screen ---- */

static void st_text(void *u, const char *s, long n)
{
    cl_repl *r = (cl_repl *)u;
    r->shown = 1;
    r->chars += n;
    ui_tokens(&r->ui, r->turn_out + r->chars / 4);
    r->render.text(r->render.u, s, n);
}

static void st_block(void *u, int type, const char *name)
{
    cl_repl *r = (cl_repl *)u;
    char what[96];
    if (type == B_THINKING)
        ui_status(&r->ui, "Thinking");
    else if (type == B_TOOL) {
        if (r->shown)
            r->render.end(r->render.u);
        cl_copy(what, "Preparing ", sizeof(what));
        cl_cat(what, name, sizeof(what));
        ui_status(&r->ui, what);
    }
}

static void log_s(cl_repl *r, const char *a, const char *b, long bn)
{
    if (!r->debug || !r->io->log)
        return;
    r->io->log(r->io->u, a, (long)strlen(a));
    if (b)
        r->io->log(r->io->u, b, bn);
    r->io->log(r->io->u, "\n", 1);
}

static void on_event(void *u, const char *ev, const char *data, long n)
{
    cl_repl *r = (cl_repl *)u;
    log_s(r, ev, data, n);
    stream_event(&r->st, ev, data, n);
}

static void on_body(void *u, const char *s, long n)
{
    cl_repl *r = (cl_repl *)u;
    if (r->resp.status == 200 && r->resp.event_stream)
        sse_feed(&r->sse, s, n);
    else if (r->errbody.n < 16384)
        jw_raw(&r->errbody, s, n);
}

/* ---- the transport ---- */

static void drop(cl_repl *r)
{
    if (r->connected)
        r->net->close(r->net->u);
    r->connected = 0;
}

static void show_err(cl_repl *r, const char *a, const char *b)
{
    char m[400];
    cl_copy(m, a, sizeof(m));
    if (b)
        cl_cat(m, b, sizeof(m));
    ui_line(&r->ui, m);
}

static void api_error(cl_repl *r)
{
    char m[600], num[16], t[64], msg[400];
    jv v, e, x;
    t[0] = msg[0] = 0;
    if (json_parse(r->errbody.p ? r->errbody.p : "", r->errbody.n, &v) == 0 && json_get(v, "error", &e)) {
        if (json_get(e, "type", &x))
            json_str(x, t, sizeof(t));
        if (json_get(e, "message", &x))
            json_str(x, msg, sizeof(msg));
    }
    cl_copy(m, "The API refused the request (HTTP ", sizeof(m));
    cl_ltoa(r->resp.status, num);
    cl_cat(m, num, sizeof(m));
    if (t[0]) {
        cl_cat(m, ", ", sizeof(m));
        cl_cat(m, t, sizeof(m));
    }
    cl_cat(m, ")", sizeof(m));
    if (msg[0]) {
        cl_cat(m, ": ", sizeof(m));
        cl_cat(m, msg, sizeof(m));
    }
    if (r->resp.request_id[0]) {
        cl_cat(m, " Request id ", sizeof(m));
        cl_cat(m, r->resp.request_id, sizeof(m));
        cl_cat(m, ".", sizeof(m));
    }
    ui_line(&r->ui, m);
}

static void request_free(cl_repl *r)
{
    stream_free(&r->st);
    sse_free(&r->sse);
    jw_free(&r->errbody);
}

/* One POST and its whole answer. *retry_s: the wait the server asked for. */
static int post(cl_repl *r, const char *body, long bn, long *retry_s)
{
    http_req q;
    cl_stream_ui sui;
    long hn, idle = 0;
    int reused, again = 1, got;
    *retry_s = -1;
    request_free(r);
    sui.u = r;
    sui.text = st_text;
    sui.block = st_block;
top:
    reused = r->connected;
    if (!r->connected) {
        int rc;
        unsigned long t0 = r->io->ms ? r->io->ms(r->io->u) : 0;
        ui_status(&r->ui, "Connecting");
        rc = r->net->open(r->net->u, r->url.host, r->url.port, r->url.tls);
        if (rc == NET_BREAK)
            return R_CANCEL;
        if (rc) {
            show_err(r, "Cannot connect: ", r->net->err(r->net->u));
            return R_RETRY;
        }
        r->connected = 1;
        r->t_open = r->io->ms ? r->io->ms(r->io->u) - t0 : 0;
    }
    memset(&q, 0, sizeof(q));
    q.host = r->url.host;
    q.port = r->url.port;
    q.tls = r->url.tls;
    q.path = r->url.path;
    q.key = r->key;
    q.beta = conv_beta(r->model);
    q.body_len = bn;
    hn = http_request_head(&q, r->head, sizeof(r->head));
    if (hn < 0)
        return R_FAIL;
    if (r->debug && r->io->log) {
        char red[1100];
        long rn = http_redact(r->head, hn, red, sizeof(red));
        if (rn > 0)
            r->io->log(r->io->u, red, rn);
    }
    {
        unsigned long t0 = r->io->ms ? r->io->ms(r->io->u) : 0;
        long s1 = r->net->send(r->net->u, r->head, hn);
        long s2 = s1 == hn ? r->net->send(r->net->u, body, bn) : s1;
        if (s1 == NET_BREAK || s2 == NET_BREAK) {
            drop(r);
            return R_CANCEL;
        }
        if (s1 != hn || s2 != bn) {
            drop(r);
            if (reused && again--)
                goto top;           /* the server closed the kept connection */
            show_err(r, "Sending failed: ", r->net->err(r->net->u));
            return R_RETRY;
        }
        http_resp_init(&r->resp, on_body, r);
        stream_init(&r->st, &sui);
        sse_init(&r->sse, on_event, r);
        jw_init(&r->errbody);
        got = 0;
        for (;;) {
            long n = r->net->recv(r->net->u, r->buf, sizeof(r->buf), 250);
            if (n == NET_TIMEOUT) {
                if (ui_poll(&r->ui)) {
                    drop(r);
                    return R_CANCEL;
                }
                if (!r->shown)
                    ui_status(&r->ui, r->st.state == ST_OPEN ? "Thinking" : "Waiting for Claude");
                idle += 250;
                if (idle < IDLE_LIMIT_MS)
                    continue;
                drop(r);
                show_err(r, "No answer for three minutes; the connection is given up.", 0);
                return R_RETRY;
            }
            if (n == NET_BREAK) {
                drop(r);
                return R_CANCEL;
            }
            if (n < 0 || (n == 0 && http_resp_eof(&r->resp))) {
                drop(r);
                if (!got && reused && again--) {
                    request_free(r);
                    goto top;
                }
                show_err(r, n < 0 ? "The connection failed: " : "The connection closed early.",
                         n < 0 ? r->net->err(r->net->u) : 0);
                return R_RETRY;
            }
            if (n == 0)
                break;              /* the body ran to the close, complete */
            if (!got) {
                got = 1;
                r->t_first = r->io->ms ? r->io->ms(r->io->u) - t0 : 0;
            }
            idle = 0;
            if (http_resp_feed(&r->resp, r->buf, n)) {
                drop(r);
                show_err(r, "The server's answer was not valid HTTP.", 0);
                return R_FAIL;
            }
            if (ui_poll(&r->ui)) {
                drop(r);
                return R_CANCEL;
            }
            if (r->resp.state == HR_DONE)
                break;
        }
    }
    if (r->resp.close)
        drop(r);
    ui_status_clear(&r->ui);
    log_s(r, "status ", r->resp.request_id, (long)strlen(r->resp.request_id));
    if (r->resp.status == 200 && r->resp.event_stream) {
        if (r->sse.oom || r->st.oom) {
            show_err(r, "Out of memory while reading the answer.", 0);
            return R_FAIL;
        }
        if (r->st.state == ST_DONE)
            return R_OK;
        if (r->st.state == ST_ERROR) {
            int busy = !strcmp(r->st.err_type, "overloaded_error") || !strcmp(r->st.err_type, "api_error");
            show_err(r, busy && !r->st.nb ? "Claude is busy (" : "The answer stopped with an error (", r->st.err_type);
            if (r->st.err_msg[0])
                show_err(r, "  ", r->st.err_msg);
            /* nothing of the answer yet: as good as a 529, try again */
            return busy && !r->st.nb ? R_RETRY : R_FAIL;
        }
        if (r->st.state == ST_BAD) {
            show_err(r, "The answer's stream was malformed.", 0);
            return R_FAIL;
        }
        show_err(r, "The answer was cut off.", 0);
        return R_RETRY;
    }
    *retry_s = r->resp.retry_after;
    if (r->resp.status == 429 || r->resp.status == 529 || r->resp.status == 408 ||
        (r->resp.status >= 500 && r->resp.status <= 599)) {
        api_error(r);
        return R_RETRY;
    }
    api_error(r);
    return R_FAIL;
}

/* post() with retries and backoff */
static int request(cl_repl *r, const char *body, long bn)
{
    long delay = 2, wait;
    int attempt;
    for (attempt = 1;; attempt++) {
        long ra;
        int rc;
        r->shown = 0;
        r->chars = 0;
        rc = post(r, body, bn, &ra);
        if (rc != R_RETRY)
            return rc;
        if (attempt >= CL_TRIES) {
            ui_line(&r->ui, "Giving up after 5 attempts.");
            return R_FAIL;
        }
        wait = ra > 0 ? ra : delay;
        if (wait > 120)
            wait = 120;
        {
            char m[120], num[16];
            if (r->shown)
                r->render.end(r->render.u);
            cl_copy(m, "Trying again in ", sizeof(m));
            cl_ltoa(wait, num);
            cl_cat(m, num, sizeof(m));
            cl_cat(m, " seconds (attempt ", sizeof(m));
            cl_ltoa(attempt + 1, num);
            cl_cat(m, num, sizeof(m));
            cl_cat(m, " of 5). Ctrl+C stops.", sizeof(m));
            ui_line(&r->ui, m);
        }
        if (r->io->sleep(r->io->u, wait * 1000))
            return R_CANCEL;
        delay *= 2;
    }
}

/* ---- a turn ---- */

static void tool_show(void *u, const char *tool, const char *what)
{
    cl_repl *r = (cl_repl *)u;
    ui_tool(&r->ui, r->tools.cur, tool, what, r->tools.cur_in, r->tools.cur_inn);
}

static int tool_ask(void *u, const char *tool, const char *what, int outside)
{
    cl_repl *r = (cl_repl *)u;
    return ui_ask(&r->ui, r->tools.cur, tool, what, outside);
}

static void tool_preview(void *u, int tool, const char *path, const char *before, long bn, const char *after,
                         long an)
{
    ui_preview(&((cl_repl *)u)->ui, tool, path, before, bn, after, an);
}

static void tool_result(void *u, int tool, const char *in, long inn, int is_error, const char *text, long n)
{
    ui_result(&((cl_repl *)u)->ui, tool, in, inn, is_error, text, n);
}

long repl_window(const char *model)
{
    return !strncmp(model, "claude-haiku", 12) ? 200000L : 1000000L;
}

/* the status line's "ctx: N% left" */
static void ctx_show(cl_repl *r)
{
    long w = repl_window(r->model), used = r->ctx_used;
    if (!r->tui)
        return;
    if (used > w)
        used = w;
    /* (w - used) * 100 / w without overflow in 32 bits */
    r->tui->ctx_left = (int)(100 - (used / (w / 100)));
    if (r->tui->ctx_left < 0)
        r->tui->ctx_left = 0;
}

/* the conversation kept for /resume (quietly: a failed save loses nothing yet) */
static void session_save(cl_repl *r)
{
    jw w;
    if (!r->session[0] || !r->conv.n)
        return;
    jw_init(&w);
    if (!conv_messages(&r->conv, &w))
        r->sys->write(r->sys->u, r->session, w.p, w.n);
    jw_free(&w);
}

static void turn(cl_repl *r, const char *prompt, long pn)
{
    cl_mark m0 = conv_mark(&r->conv);
    cl_opts o;
    jw body, content;
    int answered = 0, round;
    if (conv_add_user_text(&r->conv, prompt, pn)) {
        ui_line(&r->ui, "Out of memory.");
        return;
    }
    memset(&o, 0, sizeof(o));
    o.model = r->model;
    o.effort = r->effort;
    o.max_tokens = r->max_tokens;
    o.system = r->system;
    o.tools = tools_json();
    jw_init(&body);
    jw_init(&content);
    r->io->brk(r->io->u);           /* a Ctrl+C from before the turn does not count */
    r->turn_out = 0;
    ui_busy(&r->ui, 1);
    for (round = 0; round < 64; round++) {
        int rc, ntools, i;
        const char *stop;
        r->tools.stop = 0;
        jw_reset(&body);
        if (conv_body(&r->conv, &o, &body)) {
            ui_line(&r->ui, "Out of memory.");
            break;
        }
        rc = request(r, body.p, body.n);
        if (r->shown && rc != R_OK)
            r->render.end(r->render.u);
        if (rc == R_CANCEL) {
            ui_line(&r->ui, "Stopped. The unfinished answer is not kept.");
            break;
        }
        if (rc != R_OK)
            break;
        conv_usage(&r->conv, r->st.model[0] ? r->st.model : r->model, r->st.in_tok, r->st.out_tok,
                   r->st.cache_w, r->st.cache_r);
        r->ctx_used = r->st.in_tok + r->st.cache_r + r->st.cache_w + r->st.out_tok;
        r->turn_out += r->st.out_tok;
        ctx_show(r);
        stop = r->st.stop_reason;
        ntools = stream_tools(&r->st);
        if (r->shown)
            r->render.end(r->render.u);
        if (!strcmp(stop, "refusal")) {
            char m[160];
            cl_copy(m, "Claude declined this request", sizeof(m));
            if (r->st.stop_category[0]) {
                cl_cat(m, " (", sizeof(m));
                cl_cat(m, r->st.stop_category, sizeof(m));
                cl_cat(m, ")", sizeof(m));
            }
            cl_cat(m, ". Nothing was added to the conversation.", sizeof(m));
            ui_line(&r->ui, m);
            break;
        }
        if (!strcmp(stop, "max_tokens") && ntools) {
            ui_line(&r->ui, "The answer reached the output limit inside a tool call; nothing was run or kept.");
            break;
        }
        jw_reset(&content);
        if (stream_content(&r->st, &content) || conv_add(&r->conv, 0, content.p, content.n)) {
            ui_line(&r->ui, "Out of memory.");
            break;
        }
        answered = 1;
        if (!strcmp(stop, "max_tokens"))
            ui_line(&r->ui, "(The answer reached the output limit.)");
        if (!ntools)
            break;
        /* every tool_use answered in one user message, in order */
        jw_reset(&content);
        jw_raw(&content, "[", 1);
        for (i = 0; i < ntools; i++) {
            sblock *t = stream_tool(&r->st, i);
            if (i)
                jw_raw(&content, ",", 1);
            tools_run(&r->tools, t->id, t->name, t->input_ok, t->a.p, t->a.n, &content);
        }
        jw_raw(&content, "]", 1);
        if (content.oom || conv_add(&r->conv, 1, content.p, content.n)) {
            ui_line(&r->ui, "Out of memory.");
            /* the tool_use turn is in; without its results the history is
             * invalid, so take the turn back out */
            answered = 0;
            break;
        }
        if (r->tools.stop)
            break;                  /* the user says what to do instead */
        if (ui_poll(&r->ui)) {
            ui_line(&r->ui, "Stopped after the tool calls. Type a message to go on.");
            break;
        }
    }
    ui_busy(&r->ui, 0);
    if (!answered)
        conv_rollback(&r->conv, m0);
    else
        session_save(r);
    jw_free(&body);
    jw_free(&content);
    request_free(r);
}

/* ---- commands ---- */

static void cost(cl_repl *r)
{
    char m[300], num[16], d[24];
    cl_copy(m, "Requests ", sizeof(m));
    cl_ltoa(r->conv.requests, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, ". Tokens: input ", sizeof(m));
    cl_ltoa(r->conv.in_tok, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, ", output ", sizeof(m));
    cl_ltoa(r->conv.out_tok, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, ", cache write ", sizeof(m));
    cl_ltoa(r->conv.cache_w, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, ", cache read ", sizeof(m));
    cl_ltoa(r->conv.cache_r, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, ". Cost ", sizeof(m));
    conv_dollars(r->conv.cost_micro, d, sizeof(d));
    cl_cat(m, d, sizeof(m));
    if (r->conv.unpriced)
        cl_cat(m, " (plus requests on a model without a price here)", sizeof(m));
    cl_cat(m, ".", sizeof(m));
    ui_line(&r->ui, m);
}

static void save(cl_repl *r, const char *arg)
{
    char full[512];
    jw w;
    if (!*arg) {
        ui_line(&r->ui, "Usage: /save FILE");
        return;
    }
    if (path_join(r->tools.root, arg, full, sizeof(full))) {
        ui_line(&r->ui, "Not a usable file name.");
        return;
    }
    jw_init(&w);
    if (conv_messages(&r->conv, &w) || r->sys->write(r->sys->u, full, w.p, w.n))
        show_err(r, "Cannot save: ", r->sys->err(r->sys->u));
    else
        show_err(r, "Saved the conversation to ", full);
    jw_free(&w);
}

/* the slash commands: the menu's list and /help's */
static const cl_cmd cmds[] = {
    { "/help", "Show the commands and the keys" },
    { "/clear", "Start a new conversation (clears the screen)" },
    { "/compact", "Summarise the conversation and go on from the summary" },
    { "/context", "How much of the context window is in use" },
    { "/cost", "Tokens and cost so far" },
    { "/effort", "Show or set the effort: low, medium, high, xhigh, max" },
    { "/exit", "Leave" },
    { "/init", "Write AMIGA.md: notes on this directory for later sessions" },
    { "/model", "Show or set the model" },
    { "/resume", "Load a saved conversation (the last one by default)" },
    { "/save", "Save the conversation as JSON: /save FILE" }
};
#define NCMDS ((int)(sizeof(cmds) / sizeof(cmds[0])))

static const char help_keys[] =
    "Keys: Enter sends, Shift+Enter (or \\ then Enter, or Ctrl+J) starts a new line; "
    "Up/Down: earlier lines; Ctrl+A/E start/end, Ctrl+K/U/W cut, Ctrl+Y puts it back; "
    "Shift+Tab: the permission mode (default, accept edits, plan); Esc stops Claude; "
    "Ctrl+O shows results in full; Ctrl+C clears the line, twice leaves.";

static const char *const models[] = { "claude-opus-5-5", "claude-opus-5", "claude-sonnet-5-5", "claude-fable-5-1",
                                      "claude-haiku-4-5" };
static const char *const efforts[] = { "low", "medium", "high", "xhigh", "max" };

static void context(cl_repl *r)
{
    char m[400], num[16];
    long w = repl_window(r->model), used = r->ctx_used, pct;
    int i, cells = 30, full;
    if (used > w)
        used = w;
    pct = used / (w / 100);
    full = (int)(used * cells / w);
    if (used && !full)
        full = 1;
    cl_copy(m, "Context ", sizeof(m));
    for (i = 0; i < cells; i++)
        cl_cat(m, i < full ? "\342\226\210" : "\342\226\221", sizeof(m));   /* U+2588, U+2591 */
    cl_cat(m, "  ", sizeof(m));
    cl_ltoa(used, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, " of ", sizeof(m));
    cl_ltoa(w, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, " tokens (", sizeof(m));
    cl_ltoa(pct, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, "%), ", sizeof(m));
    cl_ltoa(100 - pct, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, "% left. Messages: ", sizeof(m));
    cl_ltoa(r->conv.n, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, used ? ". (As of the last request; /compact makes room.)" : ". (Nothing sent yet.)", sizeof(m));
    ui_line(&r->ui, m);
}

static void capture_text(void *u, const char *s, long n)
{
    jw_raw((jw *)u, s, n);
}

static void capture_end(void *u)
{
    (void)u;
}

static const char compact_prompt[] =
    "Summarise this conversation for a fresh session that will continue it: the user's goals, "
    "the decisions made, the files and paths involved, what was changed, what is still open, and "
    "anything the user asked to keep in mind. Be complete but concise. Do not call any tool.";

static const char compact_intro[] =
    "This session continues an earlier conversation that was compacted. Its summary:\n\n";

/* /compact: one request for a summary, then a NEW conversation seeded with
 * it -- nothing earlier is edited (the history stays append-only) */
static void compact(cl_repl *r)
{
    cl_mark m0 = conv_mark(&r->conv);
    cl_opts o;
    cl_render keep = r->render;
    jw body, sum;
    int rc;
    if (!r->conv.n) {
        ui_line(&r->ui, "Nothing to compact yet.");
        return;
    }
    if (conv_add_user_text(&r->conv, compact_prompt, (long)sizeof(compact_prompt) - 1)) {
        ui_line(&r->ui, "Out of memory.");
        return;
    }
    memset(&o, 0, sizeof(o));
    o.model = r->model;
    o.effort = r->effort;
    o.max_tokens = r->max_tokens;
    o.system = r->system;
    o.tools = tools_json();
    o.no_tools = 1;
    jw_init(&body);
    jw_init(&sum);
    r->render.u = &sum;
    r->render.text = capture_text;
    r->render.end = capture_end;
    r->io->brk(r->io->u);
    ui_busy(&r->ui, 1);
    rc = conv_body(&r->conv, &o, &body) ? R_FAIL : request(r, body.p, body.n);
    ui_busy(&r->ui, 0);
    r->render = keep;
    if (rc == R_OK && !strcmp(r->st.stop_reason, "end_turn") && sum.n > 0) {
        jw seed;
        conv_usage(&r->conv, r->st.model[0] ? r->st.model : r->model, r->st.in_tok, r->st.out_tok,
                   r->st.cache_w, r->st.cache_r);
        jw_init(&seed);
        jw_rawz(&seed, compact_intro);
        jw_raw(&seed, sum.p, sum.n);
        conv_clear(&r->conv);
        if (seed.oom || conv_add_user_text(&r->conv, seed.p, seed.n)) {
            conv_clear(&r->conv);
            ui_line(&r->ui, "Out of memory: the conversation starts anew.");
        } else {
            r->ctx_used = sum.n / 4;
            ctx_show(r);
            ui_line(&r->ui, "Compacted. The conversation goes on from its summary (/context for the size).");
            session_save(r);
        }
        jw_free(&seed);
    } else {
        conv_rollback(&r->conv, m0);
        if (rc == R_CANCEL)
            ui_line(&r->ui, "Stopped. The conversation is as it was.");
        else if (rc == R_OK)
            ui_line(&r->ui, "No summary came back; the conversation is as it was.");
    }
    jw_free(&body);
    jw_free(&sum);
    request_free(r);
}

static const char init_prompt[] =
    "Look at the files in the start directory (list_dir, read_file, grep; a few targeted reads, "
    "the machine is slow) and write AMIGA.md there with write_file: notes for future sessions "
    "on this directory -- what it is, how it is laid out, how to build, run and test it on the "
    "Amiga, and conventions you notice. Under 60 lines. If AMIGA.md exists, improve it.";

/* the text blocks of a saved message, shown again (/resume) */
static void replay(cl_repl *r, jv content, int user)
{
    jit it;
    jv b, x;
    if (json_type(content) == J_STR) {
        long l;
        char *t = json_strdup(content, &l);
        if (t && user)
            ui_user(&r->ui, t);
        free(t);
        return;
    }
    json_iter(content, &it);
    while (json_next(&it, 0, &b)) {
        long l;
        char *t;
        if (!json_get(b, "type", &x) || !json_streq(x, "text") || !json_get(b, "text", &x))
            continue;
        t = json_strdup(x, &l);
        if (!t)
            continue;
        if (user)
            ui_user(&r->ui, t);
        else {
            r->render.text(r->render.u, t, l);
            r->render.end(r->render.u);
        }
        free(t);
    }
}

static void resume(cl_repl *r, const char *arg)
{
    char full[512], num[16], m[120];
    char *b = 0;
    long n = 0;
    jv v, e, role, content;
    jit it;
    cl_conv c;
    const char *f = *arg ? arg : r->session;
    if (!*f) {
        ui_line(&r->ui, "Usage: /resume FILE");
        return;
    }
    if (*arg ? path_join(r->tools.root, arg, full, sizeof(full)) : (cl_copy(full, f, sizeof(full)), 0)) {
        ui_line(&r->ui, "Not a usable file name.");
        return;
    }
    if (r->sys->read(r->sys->u, full, 8L * 1024 * 1024, &b, &n)) {
        show_err(r, "Cannot read the saved conversation: ", r->sys->err(r->sys->u));
        return;
    }
    if (json_parse(b, n, &v) || json_type(v) != J_ARR || !json_count(v)) {
        free(b);
        ui_line(&r->ui, "That is not a saved conversation (a JSON array of messages).");
        return;
    }
    conv_init(&c);
    json_iter(v, &it);
    while (json_next(&it, 0, &e)) {
        if (!json_get(e, "role", &role) || !json_get(e, "content", &content) ||
            (!json_streq(role, "user") && !json_streq(role, "assistant"))) {
            conv_free(&c);
            free(b);
            ui_line(&r->ui, "That is not a saved conversation (a message without role or content).");
            return;
        }
        if (json_type(content) == J_STR) {
            jw w;
            jw_init(&w);
            jw_rawz(&w, "[{\"type\":\"text\",\"text\":");
            jw_raw(&w, content.p, content.n);
            jw_rawz(&w, "}]");
            conv_add(&c, json_streq(role, "user"), w.p, w.n);
            jw_free(&w);
        } else {
            conv_add(&c, json_streq(role, "user"), content.p, content.n);
        }
    }
    conv_clear(&r->conv);
    free(r->conv.m);
    r->conv.m = c.m;
    r->conv.n = c.n;
    r->conv.cap = c.cap;
    json_iter(v, &it);
    while (json_next(&it, 0, &e)) {
        json_get(e, "role", &role);
        json_get(e, "content", &content);
        replay(r, content, json_streq(role, "user"));
    }
    free(b);
    cl_copy(m, "Resumed a conversation of ", sizeof(m));
    cl_ltoa(r->conv.n, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, " messages.", sizeof(m));
    ui_line(&r->ui, m);
}

/* /help: the command table (the slash menu's too) and the keys */
static void show_help(cl_repl *r)
{
    int i;
    ui_line(&r->ui, "Commands:");
    for (i = 0; i < NCMDS; i++) {
        char m[160];
        long k;
        cl_copy(m, "  ", sizeof(m));
        cl_cat(m, cmds[i].name, sizeof(m));
        for (k = (long)strlen(m); k < 14; k++)
            m[k] = ' ';
        m[k] = 0;
        cl_cat(m, cmds[i].help, sizeof(m));
        ui_line(&r->ui, m);
    }
    ui_line(&r->ui, r->tui ? help_keys : "Ctrl+C stops an answer or a command. Anything else is sent to Claude.");
}

static int pick(cl_repl *r, const char *title, const char *const *opt, int n, const char *cur)
{
    int i, sel = 0;
    for (i = 0; i < n; i++)
        if (!strcmp(opt[i], cur))
            sel = i;
    return ui_pick(&r->ui, title, opt, n, sel);
}

int repl_line(cl_repl *r, const char *line)
{
    const char *arg;
    long n = (long)strlen(line);
    while (n && (line[n - 1] == ' ' || line[n - 1] == '\t'))
        n--;
    if (!n)
        return 0;
    if (line[0] != '/') {
        turn(r, line, n);
        return 0;
    }
    for (arg = line; *arg && *arg != ' '; arg++)
        ;
    while (*arg == ' ')
        arg++;
    if (!strncmp(line, "/exit", 5) || !strncmp(line, "/quit", 5))
        return 1;
    if (!strncmp(line, "/help", 5))
        show_help(r);
    else if (!strncmp(line, "/model", 6)) {
        if (*arg)
            cl_copy(r->model, arg, sizeof(r->model));
        else {
            int c = pick(r, "Select a model", models, (int)(sizeof(models) / sizeof(models[0])), r->model);
            if (c >= 0)
                cl_copy(r->model, models[c], sizeof(r->model));
        }
        ctx_show(r);
        show_err(r, "Model: ", r->model);
    } else if (!strncmp(line, "/effort", 7)) {
        if (*arg) {
            if (strcmp(arg, "low") && strcmp(arg, "medium") && strcmp(arg, "high") && strcmp(arg, "xhigh") &&
                strcmp(arg, "max")) {
                ui_line(&r->ui, "The effort is one of: low, medium, high, xhigh, max.");
                return 0;
            }
            cl_copy(r->effort, arg, sizeof(r->effort));
        } else {
            int c = pick(r, "Select the effort", efforts, 5, r->effort);
            if (c >= 0)
                cl_copy(r->effort, efforts[c], sizeof(r->effort));
        }
        show_err(r, "Effort: ", r->effort);
    } else if (!strncmp(line, "/clear", 6)) {
        conv_clear(&r->conv);
        r->ctx_used = 0;
        ctx_show(r);
        if (r->tui)
            tui_clear(r->tui);
        ui_line(&r->ui, "A new conversation.");
    } else if (!strncmp(line, "/compact", 8))
        compact(r);
    else if (!strncmp(line, "/context", 8))
        context(r);
    else if (!strncmp(line, "/init", 5))
        turn(r, init_prompt, (long)sizeof(init_prompt) - 1);
    else if (!strncmp(line, "/resume", 7))
        resume(r, arg);
    else if (!strncmp(line, "/save", 5))
        save(r, arg);
    else if (!strncmp(line, "/cost", 5))
        cost(r);
    else
        ui_line(&r->ui, "Unknown command. Type /help for the list.");
    return 0;
}

void repl_run(cl_repl *r)
{
    char *line = (char *)malloc(8192);
    char m[200];
    if (!line)
        return;
    if (r->tui) {
        show_welcome(r->show, r->model, r->tools.root);
        for (;;) {
            long n = tui_read(r->tui, line, 8192);
            if (n < 0)
                break;
            ui_user(&r->ui, line);
            if (repl_line(r, line))
                break;
        }
        free(line);
        return;
    }
    cl_copy(m, "Claude in UP-Term. Model ", sizeof(m));
    cl_cat(m, r->model, sizeof(m));
    cl_cat(m, ", effort ", sizeof(m));
    cl_cat(m, r->effort, sizeof(m));
    cl_cat(m, ". Type /help for commands, /exit to leave.", sizeof(m));
    ui_line(&r->ui, m);
    for (;;) {
        long n;
        ui_puts(&r->ui, "\n\033[1m>\033[0m ");
        n = r->io->read_line(r->io->u, line, 8192);
        r->ui.col0 = 1;
        if (n < 0 || repl_line(r, line))
            break;
    }
    free(line);
}

int repl_screen(cl_repl *r)
{
    cl_tui *t;
    cl_show *s;
    if (!r->io->read || r->tui)
        return -1;
    t = (cl_tui *)malloc(sizeof(cl_tui));
    s = (cl_show *)malloc(sizeof(cl_show));
    if (!t || !s || tui_init(t, r->io)) {
        if (t)
            tui_free(t);
        free(t);
        free(s);
        return -1;
    }
    t->model = r->model;
    t->effort = r->effort;
    t->root = r->tools.root;
    t->mode = &r->tools.perm.mode;
    t->cmds = cmds;
    t->ncmds = NCMDS;
    show_init(s, t);
    if (tui_start(t)) {
        show_free(s);
        tui_free(t);
        free(t);
        free(s);
        return -1;
    }
    r->tui = t;
    r->show = s;
    r->ui.tui = t;
    r->ui.show = s;
    show_render(s, &r->render);
    ctx_show(r);
    tui_frame(t);
    return 0;
}

int repl_ping(cl_repl *r)
{
    static const char body[] =
        "{\"max_tokens\":1,\"stream\":true,\"messages\":[{\"role\":\"user\",\"content\":\"ping\"}],\"model\":";
    jw b;
    long ra;
    int rc;
    char m[200], num[16];
    jw_init(&b);
    jw_rawz(&b, body);
    jw_strz(&b, r->model);
    jw_raw(&b, "}", 1);
    rc = post(r, b.p, b.n, &ra);
    jw_free(&b);
    if (rc == R_CANCEL)
        return -1;
    cl_copy(m, "HTTP status ", sizeof(m));
    cl_ltoa(r->resp.status, num);
    cl_cat(m, r->resp.status ? num : "none", sizeof(m));
    cl_cat(m, ". Connect (with the TLS handshake) ", sizeof(m));
    cl_ltoa((long)r->t_open, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, " ms, first byte after ", sizeof(m));
    cl_ltoa((long)r->t_first, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, " ms.", sizeof(m));
    ui_line(&r->ui, m);
    if (r->resp.status != 200 && r->resp.status)
        api_error(r);
    request_free(r);
    drop(r);
    return rc == R_OK ? 0 : -1;
}

int repl_init(cl_repl *r, cl_io *io, cl_net *net, cl_sys *sys, const char *url, const char *key,
              const char *root)
{
    jw s;
    memset(r, 0, sizeof(*r));
    r->io = io;
    r->net = net;
    r->sys = sys;
    ui_init(&r->ui, io);
    ui_plain(&r->ui, &r->render);
    conv_init(&r->conv);
    jw_init(&r->errbody);
    sse_init(&r->sse, on_event, r);
    stream_init(&r->st, 0);
    if (http_parse_url(url, &r->url)) {
        show_err(r, "Not a usable URL: ", url);
        return -1;
    }
    if (r->url.tls && (!key || !*key)) {
        ui_line(&r->ui, "No API key: set ENV:ANTHROPIC_API_KEY, or put the key in ENVARC:Claude/key.");
        return -1;
    }
    /* the key goes only over TLS: a plain http URL (the fixture) never sees it */
    r->key = r->url.tls ? key : 0;
    cl_copy(r->model, CL_DEFAULT_MODEL, sizeof(r->model));
    cl_copy(r->effort, CL_DEFAULT_EFFORT, sizeof(r->effort));
    r->max_tokens = CL_MAX_TOKENS;
    r->tools.sys = sys;
    if (sys->canon(sys->u, root, r->tools.root, sizeof(r->tools.root)))
        cl_copy(r->tools.root, root, sizeof(r->tools.root));
    r->tools.timeout_s = 60;
    r->tools.u = r;
    r->tools.show = tool_show;
    r->tools.ask = tool_ask;
    r->tools.preview = tool_preview;
    r->tools.result = tool_result;
    jw_init(&s);
    jw_rawz(&s, sys_a);
    jw_rawz(&s, sys_b);
    jw_rawz(&s, r->tools.root);
    jw_rawz(&s, sys_c);
    jw_rawz(&s, sys_d);
    /* AMIGA.md in the start directory: the project's notes (/init writes it) */
    {
        char p[512], *notes = 0;
        long nn = 0;
        if (path_join(r->tools.root, "AMIGA.md", p, sizeof(p)) == 0 && sys->kind(sys->u, p) == 1 &&
            sys->read(sys->u, p, 16384, &notes, &nn) == 0) {
            jw_rawz(&s, "\n\nThe project's notes (AMIGA.md in the start directory):\n\n");
            jw_raw(&s, notes, nn);
        }
        free(notes);
    }
    r->system = s.p;
    return s.oom ? -1 : 0;
}

void repl_free(cl_repl *r)
{
    if (r->tui) {
        tui_stop(r->tui);
        show_free(r->show);
        tui_free(r->tui);
        free(r->tui);
        free(r->show);
        r->tui = 0;
        r->show = 0;
        r->ui.tui = 0;
        r->ui.show = 0;
    }
    drop(r);
    request_free(r);
    conv_free(&r->conv);
    free(r->system);
    r->system = 0;
}
