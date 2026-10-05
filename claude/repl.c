/* repl -- see repl.h. */
#include <stdlib.h>
#include <string.h>
#include "repl_int.h"
#include "path.h"
#include "tui.h"
#include "show.h"
#include "util.h"

enum { R_OK, R_RETRY, R_FAIL, R_CANCEL };

static void started(cl_repl *r);

#define IDLE_LIMIT_MS 180000L   /* no byte for this long: the connection is dead */

static const char sys_a[] =
    "You are Claude, running as a native client on an Amiga computer (AmigaOS 3.x, a 68020 or "
    "faster processor) in the UP-Term terminal. ";
static const char sys_b[] =
    "Paths are AmigaOS paths: a volume or an assign ends with a colon (SYS:, RAM:, Work:), "
    "directories are separated by /, and a leading / or an empty part means the parent directory. "
    "Relative paths start from the start directory: ";
static const char sys_c[] =
    ". Bash runs commands through vsh, a Unix-like shell for AmigaOS (or the AmigaShell when vsh is "
    "not installed); AmigaDOS commands work, and return codes 5, 10 and 20 mean warning, error and "
    "failure. The machine is slow and has little memory: prefer Glob, Grep and targeted Reads. ";
static const char sys_d[] =
    "Every tool call is shown to the user and may need their permission. The terminal shows "
    "Markdown, 80 columns or fewer; keep answers concise. For a task of several steps keep a "
    "todo list with TodoWrite.";

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

/* A4 1.9: the thinking text, for the transcript viewer */
static void st_thinking(void *u, const char *s, long n)
{
    ui_thinking(&((cl_repl *)u)->ui, s, n);
}

static void st_block(void *u, int type, const char *name)
{
    cl_repl *r = (cl_repl *)u;
    char what[96];
    if (type == B_THINKING)
        ui_status(&r->ui, "Thinking");
    else if (type == B_SERVER)
        ui_status(&r->ui, "Searching the web");
    else if (type == B_TOOL) {
        if (r->shown)
            r->render.end(r->render.u);
        cl_copy(what, "Preparing ", sizeof(what));
        cl_cat(what, name, sizeof(what));
        ui_status(&r->ui, what);
    }
}

/* a server tool's call or result (web_search), shown when it is complete */
static void st_stop(void *u, const sblock *b)
{
    cl_repl *r = (cl_repl *)u;
    if (b->type == B_SERVER || (b->type == B_OTHER && b->start.n))
        ui_server(&r->ui, b->type == B_SERVER, b->start.p, b->start.n, b->a.p, b->a.n);
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
    if (r->feed && r->feed->event && !r->quiet_req)
        r->feed->event(r->feed->u, ev, data, n);    /* A4 WP4: --include-partial-messages */
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
    char beta[320];
    *retry_s = -1;
    request_free(r);
    r->resp.status = 0;             /* no answer yet (system/api_retry's error_status) */
    sui.u = r;
    sui.text = st_text;
    sui.block = st_block;
    sui.thinking = st_thinking;
    sui.stop = st_stop;
    if (repl_need_key(r)) {
        /* A4 WP4: a keyless start; nothing goes out without the key */
        show_err(r, "Not logged in: no API key. Type /login, or set ENV:ANTHROPIC_API_KEY.", 0);
        return R_FAIL;
    }
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
    if (r->betas[0]) {
        /* --betas: added to the model's own (anthropic-beta takes a comma list) */
        cl_copy(beta, q.beta, sizeof(beta));
        if (beta[0])
            cl_cat(beta, ",", sizeof(beta));
        cl_cat(beta, r->betas, sizeof(beta));
        q.beta = beta;
    }
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
            r->busy_fail = busy && !r->st.nb;
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
    r->busy_fail = r->resp.status == 529;
    if (r->resp.status == 429 || r->resp.status == 529 || r->resp.status == 408 ||
        (r->resp.status >= 500 && r->resp.status <= 599)) {
        api_error(r);
        return R_RETRY;
    }
    api_error(r);
    return R_FAIL;
}

/* post() with retries and backoff */
static int request_retry(cl_repl *r, const char *body, long bn)
{
    long delay = 2, wait;
    int attempt;
    r->busy_fail = 0;
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
        if (r->feed && r->feed->retry && !r->quiet_req) {
            /* stream-json's system/api_retry (Claude Code's error kinds) */
            int st = r->resp.status == 200 ? 0 : r->resp.status;
            r->feed->retry(r->feed->u, attempt, CL_TRIES - 1, wait * 1000, st,
                           r->busy_fail ? "overloaded" : st == 429 ? "rate_limit" : st >= 500 ? "server_error"
                                                                                       : "unknown");
        }
        if (r->io->sleep(r->io->u, wait * 1000))
            return R_CANCEL;
        delay *= 2;
    }
}

/* ... and the time it took (print mode's duration_api_ms) */
static int request(cl_repl *r, const char *body, long bn)
{
    unsigned long t0 = r->io->ms ? r->io->ms(r->io->u) : 0;
    int rc = request_retry(r, body, bn);
    if (r->io->ms)
        r->api_ms += r->io->ms(r->io->u) - t0;
    return rc;
}

/* ---- a turn ---- */

/* the callbacks below serve the conversation's tools and a subagent's:
 * r->at is the one whose call runs (pol_call sets it) */
static void tool_show(void *u, const char *tool, const char *what)
{
    cl_repl *r = (cl_repl *)u;
    ui_tool(&r->ui, r->at->cur, tool, what, r->at->cur_in, r->at->cur_inn);
}

void repl_denied(cl_repl *r, const char *tool, const char *input, long n)
{
    if (r->feed && r->feed->denied)
        r->feed->denied(r->feed->u, tool, r->cur_id ? r->cur_id : "", input, n);
}

int repl_ask(cl_repl *r, int tid, const char *tool, const char *what, int outside, int rule)
{
    char m[200];
    /* a subagent's permissionMode (dontAsk, bypassPermissions) is its own */
    int pol = r->at && r->at->ask_policy ? r->at->ask_policy - 1 : r->ask_policy;
    /* A4 WP4: nobody to ask (print mode, dontAsk): denied, but a read in
     * the start directory runs (Claude Code: no approval needed there);
     * bypassPermissions: yes, except to an explicit ask rule */
    if (pol == ASKP_BYPASS && !rule)
        return ASK_ONCE;
    if (r->no_person || pol == ASKP_DENY) {
        if (!rule && !outside && tid >= 0 && perm_read_only(tid))
            return ASK_ONCE;
        repl_denied(r, tool, r->at->cur_in, r->at->cur_inn);
        return ASK_NO;
    }
    cl_copy(m, "Claude needs your permission to use ", sizeof(m));
    cl_cat(m, cfg_cc_tool(tool), sizeof(m));
    pol_notify(r, m);
    return ui_ask(&r->ui, tid, tool, what, outside);
}

static int tool_ask(void *u, const char *tool, const char *what, int outside)
{
    cl_repl *r = (cl_repl *)u;
    if (r->rule_now == RULE_ALLOW) {
        /* a permission rule (or a hook) allowed it: no question */
        r->n_rule_allow++;
        return ASK_ONCE;
    }
    return repl_ask(r, r->at->cur, tool, what, outside, r->rule_now == RULE_ASK);
}

static void tool_preview(void *u, int tool, const char *path, const char *before, long bn, const char *after,
                         long an)
{
    ui_preview(&((cl_repl *)u)->ui, tool, path, before, bn, after, an);
}

static void tool_result(void *u, int tool, const char *in, long inn, int is_error, const char *text, long n)
{
    cl_repl *r = (cl_repl *)u;
    if (tool == T_TODO_WRITE && !is_error) {
        /* the list as it stands, for /todos */
        char *t = (char *)malloc((size_t)inn + 1);
        if (t) {
            memcpy(t, in, (size_t)inn);
            t[inn] = 0;
            free(r->todos);
            r->todos = t;
        }
    }
    if (r->show)
        r->show->brief = r->at->brief;
    ui_result(&r->ui, tool, in, inn, is_error, text, n);
    if (r->show)
        r->show->brief = 0;
}

static int tool_choose(void *u, const char *header, const char *question, const char *const *labels,
                       const char *const *descs, int n, int flags, unsigned *picked, char *other, long cap)
{
    if (((cl_repl *)u)->no_person)
        return -1;                  /* print mode: no one to answer */
    return ui_choose(&((cl_repl *)u)->ui, header, question, labels, descs, n, flags, picked, other, cap);
}

/* ExitPlanMode's plan, drawn as an answer is (Markdown) */
static void tool_plan(void *u, const char *text, long n)
{
    cl_repl *r = (cl_repl *)u;
    r->render.text(r->render.u, text, n);
    r->render.end(r->render.u);
}

static void quiet_text(void *u, const char *s, long n)
{
    (void)u;
    (void)s;
    (void)n;
}

static void quiet_end(void *u)
{
    (void)u;
}

static int request(cl_repl *r, const char *body, long bn);

/* One request for a tool (WebFetch's small model, a Task subagent) while
 * the turn's own answer is still in use: that stream is kept aside, the
 * new one handed over whole, nothing of it drawn. */
static int api_send(void *u, const char *body, long bn, cl_stream *st)
{
    cl_repl *r = (cl_repl *)u;
    cl_stream keep = r->st;
    cl_render rk = r->render;
    int shown = r->shown, rc;
    long chars = r->chars;
    stream_init(&r->st, 0);
    r->render.u = 0;
    r->render.text = quiet_text;
    r->render.end = quiet_end;
    r->quiet_req++;
    rc = request(r, body, bn);
    r->quiet_req--;
    r->render = rk;
    *st = r->st;
    r->st = keep;
    r->shown = shown;
    r->chars = chars;
    if (rc == R_OK) {
        conv_usage(&r->conv, st->model[0] ? st->model : r->model, st->in_tok, st->out_tok, st->cache_w,
                   st->cache_r);
        return 0;
    }
    return rc == R_CANCEL ? -2 : -1;
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

/* the conversation appended to its session file (quietly: a failed save
 * loses nothing yet, the next turn writes what is missing) */
static void session_save(cl_repl *r)
{
    if (r->conv.n)
        sess_save(&r->sess, &r->conv);
}

void repl_saved(cl_repl *r)
{
    session_save(r);
}

/* the tools' results of one round: every tool_use answered in one user
 * message, in order, then the text the policy adds (hook feedback, the
 * memory of a directory a read reached) */
static int run_tools(cl_repl *r, int ntools, jw *content)
{
    jw extra;
    int i;
    jw_init(&extra);
    jw_reset(content);
    jw_raw(content, "[", 1);
    for (i = 0; i < ntools; i++) {
        sblock *t = stream_tool(&r->st, i);
        if (i)
            jw_raw(content, ",", 1);
        pol_call(r, &r->tools, t->id, t->name, t->input_ok, t->a.p, t->a.n, content, &extra);
    }
    if (extra.n) {
        jw_rawz(content, ",{\"type\":\"text\",\"text\":");
        jw_str(content, extra.p, extra.n);
        jw_raw(content, "}", 1);
    }
    jw_raw(content, "]", 1);
    jw_free(&extra);
    return content->oom ? -1 : 0;
}

long repl_compact_at(cl_repl *r)
{
    long w = repl_window(r->model), set = 0;
    char v[24];
    /* Claude Code's order: CLAUDE_CODE_AUTO_COMPACT_WINDOW, --autocompact, autoCompactWindow */
    if (r->sys->getenv && r->sys->getenv(r->sys->u, "CLAUDE_CODE_AUTO_COMPACT_WINDOW", v, sizeof(v)) > 0)
        set = cfg_window_parse(v);
    if (!set && r->compact_window)
        set = r->compact_window;
    if (!set)
        set = r->cfg.compact_window;
    if (set > 0)
        return set < w ? set : w;   /* capped at the model's window */
    return (w / 100) * CL_COMPACT_PCT;
}

/* the auto-compact threshold passed? */
static int too_full(cl_repl *r)
{
    return r->auto_compact && r->conv.n > 1 && r->ctx_used > repl_compact_at(r);
}

static void turn(cl_repl *r, const char *prompt, long pn)
{
    cl_mark m0 = conv_mark(&r->conv);
    cl_opts o;
    jw body, content, xtools;
    int answered = 0, round, stops = 0;
    const char *turn_tools = r->turn_tools;     /* a SlashCommand's allowed-tools last this turn */
    const char *blocks = r->blocks;     /* a prompt given as content blocks (images: stream-json input) */
    long bn = r->blocks_n;
    r->blocks = 0;
    r->turn_rc = TURN_FAIL;
    cp_turn(&r->cp, r->conv.n);
    if ((blocks ? conv_add_user_blocks(&r->conv, blocks, bn) : conv_add_user_text(&r->conv, prompt, pn)) ||
        (r->pending.n && conv_add_user_text(&r->conv, r->pending.p, r->pending.n))) {
        conv_rollback(&r->conv, m0);
        ui_line(&r->ui, "Out of memory.");
        return;
    }
    jw_reset(&r->pending);
    memset(&o, 0, sizeof(o));
    o.model = r->model;
    o.effort = r->effort;
    o.max_tokens = r->max_tokens;
    o.system = r->system;
    o.tools = tools_json(&r->tools, r->model);
    jw_init(&xtools);
    if (r->schema && o.tools) {
        /* --json-schema: Claude Code's StructuredOutput tool, the schema as its input */
        long tn = (long)strlen(o.tools);
        if (tn >= 2 && o.tools[tn - 1] == ']') {
            jw_raw(&xtools, o.tools, tn - 1);
            if (tn > 2)
                jw_raw(&xtools, ",", 1);
        } else
            jw_raw(&xtools, "[", 1);
        jw_rawz(&xtools, "{\"name\":\"StructuredOutput\",\"description\":\"Use this tool to return your final "
                         "response in the requested structured format. You MUST call this tool exactly once at "
                         "the end of your response to provide the structured output.\",\"input_schema\":");
        jw_rawz(&xtools, r->schema);
        jw_rawz(&xtools, "}]");
        if (!xtools.oom)
            o.tools = xtools.p;
    }
    jw_init(&body);
    jw_init(&content);
    r->io->brk(r->io->u);           /* a Ctrl+C from before the turn does not count */
    r->turn_out = 0;
    ui_busy(&r->ui, 1);
    for (round = 0; round < 64; round++) {
        int rc, ntools;
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
            r->turn_rc = TURN_CANCEL;
            break;
        }
        if (rc == R_FAIL && r->busy_fail && r->fallback[0] && strcmp(r->model, r->fallback)) {
            /* --fallback-model: the model is overloaded, the turn goes on with the other */
            cl_copy(r->model, r->fallback, sizeof(r->model));
            repl_say(r, "The model is overloaded; going on with the fallback model ", r->model);
            ctx_show(r);
            continue;
        }
        if (rc != R_OK)
            break;
        r->n_responses++;
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
        r->turn_rc = TURN_OK;
        if (r->feed && r->feed->message)
            r->feed->message(r->feed->u, 0, content.p, content.n);
        pol_status_event(r);        /* Claude Code: a new assistant message */
        if (!strcmp(stop, "max_tokens"))
            ui_line(&r->ui, "(The answer reached the output limit.)");
        if (!ntools && !strcmp(stop, "pause_turn"))
            continue;               /* a server tool's loop paused: the API goes on from the turn */
        if (!ntools) {
            /* a Stop hook may send Claude on (Claude Code's exit 2) */
            jw why;
            int again;
            jw_init(&why);
            again = stops < 3 && pol_stop(r, stops > 0, &why);
            if (again && conv_add_user_text(&r->conv, why.p, why.n) == 0) {
                stops++;
                jw_free(&why);
                continue;
            }
            jw_free(&why);
            break;
        }
        if (run_tools(r, ntools, &content) || conv_add(&r->conv, 1, content.p, content.n)) {
            ui_line(&r->ui, "Out of memory.");
            /* the tool_use turn is in; without its results the history is
             * invalid, so take the turn back out */
            answered = 0;
            r->turn_rc = TURN_FAIL;
            break;
        }
        if (r->feed && r->feed->message)
            r->feed->message(r->feed->u, 1, content.p, content.n);
        /* A4 WP4: --max-turns, --max-budget-usd (print mode) end the turn
         * after a tool round, before the next request */
        if (r->max_turns && round + 1 >= r->max_turns) {
            r->turn_rc = TURN_MAX_TURNS;
            break;
        }
        if (r->budget_micro && r->conv.cost_micro - r->budget_base >= r->budget_micro) {
            r->turn_rc = TURN_BUDGET;
            break;
        }
        if (r->tools.stop)
            break;                  /* the user says what to do instead */
        if (ui_poll(&r->ui)) {
            ui_line(&r->ui, "Stopped after the tool calls. Type a message to go on.");
            break;
        }
        {
            /* A4 1.5: prompts typed ahead during the round go in now, with
             * the tool results, as Claude Code's queue does */
            jw q;
            jw_init(&q);
            if (ui_take_queued(&r->ui, &q) && !q.oom)
                conv_add_user_text(&r->conv, q.p, q.n);
            jw_free(&q);
        }
    }
    ui_busy(&r->ui, 0);
    r->turn_tools = turn_tools;
    if (!answered)
        conv_rollback(&r->conv, m0);
    else
        session_save(r);
    jw_free(&body);
    jw_free(&content);
    jw_free(&xtools);
    request_free(r);
    if (answered && too_full(r)) {
        ui_line(&r->ui, "The context window is nearly full: compacting the conversation (/autocompact turns this off).");
        repl_compact(r, "", 1);
    }
}

void repl_turn(cl_repl *r, const char *prompt, long n)
{
    turn(r, prompt, n);
}

int repl_blocks(cl_repl *r, const char *text, long tn, const char *blocks, long bn)
{
    started(r);
    if (pol_prompt(r, text, tn))
        return 0;
    r->blocks = blocks;
    r->blocks_n = bn;
    turn(r, text, tn);
    r->blocks = 0;
    return 0;
}

/* ---- commands ---- */

void repl_say(cl_repl *r, const char *a, const char *b)
{
    show_err(r, a, b);
}

void repl_ctx(cl_repl *r)
{
    ctx_show(r);
}

void repl_cost(cl_repl *r)
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

static const char help_keys[] =
    "Keys: Enter sends, Shift+Enter (or \\ then Enter, or Ctrl+J) starts a new line; "
    "Up/Down: earlier prompts, Ctrl+R searches them; Ctrl+A/E start/end, Ctrl+K/U/W cut, "
    "Ctrl+Y puts it back, Ctrl+_ undoes; Shift+Tab: the permission mode (default, accept edits, "
    "plan); Esc stops Claude, Esc Esc clears the box or rewinds.";
/* A4 (WP1)'s keys and prefixes */
static const char help_keys2[] =
    "Enter while Claude works queues the prompt; Ctrl+O: the transcript in full; Ctrl+T: the "
    "todo list; Ctrl+L redraws; Ctrl+G edits the prompt in your editor; Ctrl+C clears the line, "
    "twice leaves. A line starting with ! runs a command, # saves a memory; @path attaches a "
    "file (Tab completes it).";

static const char *const models[] = { "claude-opus-5-5", "claude-opus-5", "claude-sonnet-5-5", "claude-fable-5-1",
                                      "claude-haiku-4-5" };
static const char *const efforts[] = { "low", "medium", "high", "xhigh", "max" };

void repl_context(cl_repl *r)
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

/* /compact [focus]: one request for a summary, then a NEW conversation
 * seeded with it -- nothing earlier is edited (the history stays
 * append-only; the session file gets the new start as one more line) */
void repl_compact(cl_repl *r, const char *focus, int automatic)
{
    cl_mark m0 = conv_mark(&r->conv);
    cl_opts o;
    cl_render keep = r->render;
    jw body, sum, ask;
    int rc;
    if (!r->conv.n) {
        ui_line(&r->ui, "Nothing to compact yet.");
        return;
    }
    pol_precompact(r, automatic, focus);
    jw_init(&ask);
    jw_rawz(&ask, compact_prompt);
    if (focus && *focus) {
        jw_rawz(&ask, " Focus the summary on: ");
        jw_rawz(&ask, focus);
    }
    if (ask.oom || conv_add_user_text(&r->conv, ask.p, ask.n)) {
        jw_free(&ask);
        ui_line(&r->ui, "Out of memory.");
        return;
    }
    jw_free(&ask);
    memset(&o, 0, sizeof(o));
    o.model = r->model;
    o.effort = r->effort;
    o.max_tokens = r->max_tokens;
    o.system = r->system;
    o.tools = tools_json(&r->tools, r->model);
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
        sess_truncate(&r->sess, 0);
        cp_turn(&r->cp, 0);
        if (seed.oom || conv_add_user_text(&r->conv, seed.p, seed.n)) {
            conv_clear(&r->conv);
            ui_line(&r->ui, "Out of memory: the conversation starts anew.");
        } else {
            r->ctx_used = sum.n / 4;
            ctx_show(r);
            ui_line(&r->ui, "Compacted. The conversation goes on from its summary (/context for the size).");
            session_save(r);
            pol_status_event(r);    /* Claude Code: /compact finished */
            pol_session(r, HK_SESSION_START, "compact");
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

static void start(cl_repl *r);
static void started(cl_repl *r);

static const char init_prompt[] =
    "Look at the files in the start directory (Glob, Grep, a few targeted Reads; the machine is "
    "slow) and write AMIGA.md there with Write: notes for future sessions "
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

void repl_replay(cl_repl *r)
{
    int i;
    for (i = 0; i < r->conv.n; i++) {
        jv v;
        if (json_parse(r->conv.m[i].json, r->conv.m[i].n, &v) == 0)
            replay(r, v, r->conv.m[i].user);
    }
}

/* the A2 format: a JSON array of messages (/save writes it) */
int repl_load_json(cl_repl *r, const char *full)
{
    char *b = 0;
    long n = 0;
    jv v, e, role, content;
    jit it;
    cl_conv c;
    if (r->sys->read(r->sys->u, full, 8L * 1024 * 1024, &b, &n)) {
        show_err(r, "Cannot read the saved conversation: ", r->sys->err(r->sys->u));
        return -1;
    }
    if (json_parse(b, n, &v) || json_type(v) != J_ARR || !json_count(v)) {
        free(b);
        ui_line(&r->ui, "That is not a saved conversation (a JSON array of messages).");
        return -1;
    }
    conv_init(&c);
    json_iter(v, &it);
    while (json_next(&it, 0, &e)) {
        if (!json_get(e, "role", &role) || !json_get(e, "content", &content) ||
            (!json_streq(role, "user") && !json_streq(role, "assistant"))) {
            conv_free(&c);
            free(b);
            ui_line(&r->ui, "That is not a saved conversation (a message without role or content).");
            return -1;
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
    free(b);
    conv_clear(&r->conv);
    free(r->conv.m);
    r->conv.m = c.m;
    r->conv.n = c.n;
    r->conv.cap = c.cap;
    /* it goes on in a new session of its own */
    sess_new(&r->sess, r->io->ms ? r->io->ms(r->io->u) : 0);
    session_save(r);
    return 0;
}

static void resumed(cl_repl *r)
{
    char m[160], num[16];
    repl_replay(r);
    cp_turn(&r->cp, r->conv.n);
    cl_copy(m, "Resumed a conversation of ", sizeof(m));
    cl_ltoa(r->conv.n, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, " messages", sizeof(m));
    if (r->sess.title[0]) {
        cl_cat(m, " (", sizeof(m));
        cl_cat(m, r->sess.title, sizeof(m));
        cl_cat(m, ")", sizeof(m));
    }
    cl_cat(m, ".", sizeof(m));
    ui_line(&r->ui, m);
    r->start_due = 0;               /* Claude Code: a resumed session's source is "resume" */
    pol_session(r, HK_SESSION_START, "resume");
}

int repl_resume_session(cl_repl *r, const char *name)
{
    char id[40];
    int rc;
    long nl = (long)strlen(name);
    if (nl > 6 && cl_strieq(name + nl - 6, ".jsonl")) {
        /* Claude Code: the path of a session's .jsonl transcript in place of an id */
        char full[512];
        if (r->sys->kind(r->sys->u, name) == 1)
            cl_copy(full, name, sizeof(full));      /* as given (an absolute path) */
        else if (path_join(r->tools.root, name, full, sizeof(full)))
            full[0] = 0;
        if (!full[0] || sess_load_file(&r->sess, full, &r->conv)) {
            show_err(r, "No session file here: ", name);
            return -1;
        }
        resumed(r);
        return 0;
    }
    rc = sess_find(&r->sess, name, id, sizeof(id));
    if (rc == -2) {
        show_err(r, "More than one session matches ", name);
        return -1;
    }
    if (rc || sess_load(&r->sess, id, &r->conv)) {
        show_err(r, "No session here by that name or id: ", name);
        return -1;
    }
    resumed(r);
    return 0;
}

int repl_continue(cl_repl *r)
{
    cl_sess_info one;
    if (sess_list(&r->sess, &one, 1) < 1 || sess_load(&r->sess, one.id, &r->conv)) {
        ui_line(&r->ui, "No earlier conversation in this directory to continue.");
        return -1;
    }
    resumed(r);
    return 0;
}

/* /resume: a picker over this directory's sessions; /resume NAME|ID; and
 * A2's JSON files (/resume FILE.json, and ENVARC:Claude/session.json
 * when there is no session yet) */
static void resume(cl_repl *r, const char *arg)
{
    cl_sess_info *l;
    int n, i, c;
    long al = (long)strlen(arg);
    if (*arg) {
        char full[512];
        if (al > 5 && cl_strieq(arg + al - 5, ".json")) {
            if (path_join(r->tools.root, arg, full, sizeof(full))) {
                ui_line(&r->ui, "Not a usable file name.");
                return;
            }
            if (repl_load_json(r, full) == 0)
                resumed(r);
            return;
        }
        repl_resume_session(r, arg);
        return;
    }
    l = (cl_sess_info *)malloc(SESS_LIST * sizeof(cl_sess_info));
    if (!l) {
        ui_line(&r->ui, "Out of memory.");
        return;
    }
    n = sess_list(&r->sess, l, SESS_LIST);
    if (!n) {
        free(l);
        if (r->session[0] && r->sys->kind(r->sys->u, r->session) == 1) {
            /* A2's one saved conversation: taken over as a session */
            if (repl_load_json(r, r->session) == 0)
                resumed(r);
            return;
        }
        ui_line(&r->ui, "No saved conversation in this directory yet.");
        return;
    }
    {
        char (*lab)[120] = (char (*)[120])malloc((size_t)n * 120);
        const char *opt[SESS_LIST];
        if (!lab) {
            free(l);
            ui_line(&r->ui, "Out of memory.");
            return;
        }
        for (i = 0; i < n; i++) {
            cl_copy(lab[i], l[i].title[0] ? l[i].title : l[i].first[0] ? l[i].first : "(no prompt)", 90);
            cl_cat(lab[i], "  [", 120);
            cl_cat(lab[i], l[i].id, 120);
            cl_cat(lab[i], "]", 120);
            opt[i] = lab[i];
        }
        c = ui_pick(&r->ui, "Resume a conversation", opt, n, 0);
        if (c < 0) {
            /* the line mode: the list, and how to pick */
            ui_line(&r->ui, "Conversations in this directory, the most recent first:");
            for (i = 0; i < n; i++)
                show_err(r, "  ", lab[i]);
            ui_line(&r->ui, "Type /resume ID or /resume NAME to go on with one.");
        } else if (sess_load(&r->sess, l[c].id, &r->conv) == 0)
            resumed(r);
        else
            ui_line(&r->ui, "That conversation could not be read.");
        free(lab);
    }
    free(l);
}

/* /help: the command table (the slash menu's too) and the keys */
static void show_help(cl_repl *r)
{
    int i;
    ui_line(&r->ui, "Commands:");
    for (i = 0; i < r->nmenu; i++) {
        char m[200];
        long k;
        cl_copy(m, "  ", sizeof(m));
        cl_cat(m, r->menu[i].name, sizeof(m));
        for (k = (long)strlen(m); k < 18; k++)
            m[k] = ' ';
        m[k] = 0;
        cl_cat(m, r->menu[i].help, sizeof(m));
        ui_line(&r->ui, m);
    }
    ui_line(&r->ui, r->tui ? help_keys : "Ctrl+C stops an answer or a command. Anything else is sent to Claude.");
    if (r->tui)
        ui_line(&r->ui, help_keys2);
}

int repl_pick(cl_repl *r, const char *title, const char *const *opt, int n, const char *cur)
{
    int i, sel = 0;
    for (i = 0; i < n; i++)
        if (!strcmp(opt[i], cur))
            sel = i;
    return ui_pick(&r->ui, title, opt, n, sel);
}

/* the word "/name" of a command line, exactly */
static int is_cmd(const char *word, const char *name)
{
    return !strcmp(word, name);
}

int repl_line(cl_repl *r, const char *line)
{
    const char *arg;
    char word[64];
    long n = (long)strlen(line), wl;
    while (n && (line[n - 1] == ' ' || line[n - 1] == '\t'))
        n--;
    if (!n)
        return 0;
    started(r);
    if (r->await_key) {
        /* the line after /login is the key, never shown or kept */
        r->await_key = 0;
        slash_run(r, "/login", line);
        if (r->first && !repl_need_key(r))
            start(r);                   /* the prompt given at the start goes now */
        return 0;
    }
    {
        /* A4: ! commands, # memories, @ mentions, /theme, /vim (input.c) */
        jw x;
        int in;
        jw_init(&x);
        in = ui_input(&r->ui, line, &x);
        if (in == IN_SEND && !x.oom && pol_prompt(r, x.p, x.n) == 0)
            turn(r, x.p, x.n);
        jw_free(&x);
        if (in != IN_PASS)
            return 0;
    }
    if (line[0] != '/') {
        if (pol_prompt(r, line, n))
            return 0;
        turn(r, line, n);
        return 0;
    }
    for (arg = line; *arg && *arg != ' '; arg++)
        ;
    wl = (long)(arg - line);
    if (wl >= (long)sizeof(word))
        wl = (long)sizeof(word) - 1;
    memcpy(word, line, (size_t)wl);
    word[wl] = 0;
    while (*arg == ' ')
        arg++;
    if (is_cmd(word, "/exit") || is_cmd(word, "/quit"))
        return 1;
    if (is_cmd(word, "/help"))
        show_help(r);
    else if (is_cmd(word, "/model")) {
        if (*arg)
            cl_copy(r->model, cfg_model(arg), sizeof(r->model));
        else {
            int c = repl_pick(r, "Select a model", models, (int)(sizeof(models) / sizeof(models[0])), r->model);
            if (c >= 0)
                cl_copy(r->model, models[c], sizeof(r->model));
        }
        ctx_show(r);
        show_err(r, "Model: ", r->model);
    } else if (is_cmd(word, "/effort")) {
        if (*arg) {
            if (strcmp(arg, "low") && strcmp(arg, "medium") && strcmp(arg, "high") && strcmp(arg, "xhigh") &&
                strcmp(arg, "max")) {
                ui_line(&r->ui, "The effort is one of: low, medium, high, xhigh, max.");
                return 0;
            }
            cl_copy(r->effort, arg, sizeof(r->effort));
        } else {
            int c = repl_pick(r, "Select the effort", efforts, 5, r->effort);
            if (c >= 0)
                cl_copy(r->effort, efforts[c], sizeof(r->effort));
        }
        show_err(r, "Effort: ", r->effort);
    } else if (is_cmd(word, "/clear")) {
        pol_session(r, HK_SESSION_END, "clear");
        conv_clear(&r->conv);
        sess_new(&r->sess, r->io->ms ? r->io->ms(r->io->u) : 0);
        cp_turn(&r->cp, 0);
        r->ctx_used = 0;
        ctx_show(r);
        if (r->tui)
            tui_clear(r->tui);
        ui_line(&r->ui, "A new conversation.");
        pol_session(r, HK_SESSION_START, "clear");
    } else if (is_cmd(word, "/compact"))
        repl_compact(r, arg, 0);
    else if (is_cmd(word, "/context"))
        repl_context(r);
    else if (is_cmd(word, "/init"))
        turn(r, init_prompt, (long)sizeof(init_prompt) - 1);
    else if (is_cmd(word, "/resume"))
        resume(r, arg);
    else if (is_cmd(word, "/save"))
        save(r, arg);
    else if (is_cmd(word, "/cost"))
        repl_cost(r);
    else if (!slash_run(r, word, arg) && !slash_custom(r, word, arg))
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
        start(r);
        for (;;) {
            long n = tui_read(r->tui, line, 8192);
            if (n < 0)
                break;
            if (!r->await_key)
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
    start(r);
    for (;;) {
        long n;
        ui_puts(&r->ui, r->await_key ? "\n\033[1mKey:\033[0m " : "\n\033[1m>\033[0m ");
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
    t->cmds = r->menu;
    t->ncmds = r->nmenu;
    t->status = r->status_text;     /* the statusLine command's row(s) */
    t->status_pad = r->cfg.status_pad;
    t->idle = pol_status_tick;
    t->iu = r;
    show_init(s, t);
    s->verbose = &r->verbose;    /* --verbose: results unfolded in place */
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
    ui_attach(&r->ui, r->sys, r->tools.root, &r->conv);    /* A4: history, @, rewind, settings */
    ctx_show(r);
    tui_frame(t);
    pol_status_event(r);            /* Claude Code: the session started */
    return 0;
}

/* ---- rewind ---- */

/* does message i start with a prompt (a text block, no tool results)? */
static int is_prompt(const cl_repl *r, int i)
{
    jv v, b, x;
    jit it;
    if (i < 0 || i >= r->conv.n || !r->conv.m[i].user || json_parse(r->conv.m[i].json, r->conv.m[i].n, &v))
        return 0;
    json_iter(v, &it);
    return json_next(&it, 0, &b) && json_get(b, "type", &x) && json_streq(x, "text");
}

int repl_prompts(const cl_repl *r, int *msg, int max)
{
    int i, k = 0;
    for (i = r->conv.n - 1; i >= 0 && k < max; i--)
        if (is_prompt(r, i))
            msg[k++] = i;
    return k;
}

int repl_rewind(cl_repl *r, int msg, int code, int conv)
{
    char rep[600];
    if (!is_prompt(r, msg))
        return -1;
    if (code) {
        int lost = 0, n = cp_restore(&r->cp, msg, &lost, rep, sizeof(rep));
        char m[120], num[16];
        cl_copy(m, "Files put back: ", sizeof(m));
        cl_ltoa(n, num);
        cl_cat(m, num, sizeof(m));
        if (lost) {
            cl_cat(m, "; could not put back: ", sizeof(m));
            cl_ltoa(lost, num);
            cl_cat(m, num, sizeof(m));
        }
        cl_cat(m, ".", sizeof(m));
        ui_line(&r->ui, m);
        if (rep[0])
            ui_line(&r->ui, rep);
    }
    if (conv) {
        cl_mark mk;
        mk.n = msg;
        mk.last = msg ? r->conv.m[msg - 1].n : 0;
        conv_rollback(&r->conv, mk);
        sess_truncate(&r->sess, msg);
        cp_turn(&r->cp, r->conv.n);
        {
            /* the context left: estimated (4 bytes a token) until the next request says */
            long b = r->system ? (long)strlen(r->system) : 0;
            int i;
            for (i = 0; i < r->conv.n; i++)
                b += r->conv.m[i].n;
            r->ctx_used = r->conv.n ? b / 4 : 0;
            ctx_show(r);
        }
        if (r->conv.n)
            session_save(r);
        ui_line(&r->ui, "The conversation is back to before that prompt.");
    }
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

static void menu_free(cl_repl *r)
{
    int i;
    for (i = slash_nbuiltin; i < r->nmenu; i++)
        free((char *)r->menu[i].name);
    free(r->menu);
    r->menu = 0;
    r->nmenu = 0;
}

/* the slash menu: the built-in commands, then the custom ones */
static int menu_build(cl_repl *r)
{
    int nc = defs_count(&r->defs, DEF_COMMAND), i, k;
    menu_free(r);
    r->menu = (cl_cmd *)malloc((size_t)(slash_nbuiltin + nc) * sizeof(cl_cmd));
    if (!r->menu)
        return -1;
    for (i = 0; i < slash_nbuiltin; i++)
        r->menu[i] = slash_builtin[i];
    k = slash_nbuiltin;
    for (i = 0; i < nc; i++) {
        const cl_def *d = defs_nth(&r->defs, DEF_COMMAND, i);
        char *nm = (char *)malloc(strlen(d->name) + 2);
        if (!nm)
            break;
        nm[0] = '/';
        strcpy(nm + 1, d->name);
        r->menu[k].name = nm;
        r->menu[k].help = d->description[0] ? d->description
                                             : d->src == CFG_PROJECT ? "(a project command)" : "(a user command)";
        k++;
    }
    r->nmenu = k;
    if (r->tui) {
        r->tui->cmds = r->menu;
        r->tui->ncmds = r->nmenu;
    }
    return 0;
}

static const char mem_intro[] =
    "\n\nCodebase and user instructions are shown below. Be sure to adhere to these instructions. "
    "IMPORTANT: These instructions OVERRIDE any default behavior and you MUST follow them exactly as written.\n\n";

int repl_system(cl_repl *r)
{
    jw s;
    const cl_def *st = r->style[0] ? defs_find(&r->defs, DEF_STYLE, r->style) : 0;
    jw_init(&s);
    if (r->sys_replace)
        jw_rawz(&s, r->sys_replace);    /* --system-prompt (A4 WP4) */
    else {
        jw_rawz(&s, sys_a);
        jw_rawz(&s, sys_b);
        jw_rawz(&s, r->tools.root);
        jw_rawz(&s, sys_c);
        jw_rawz(&s, sys_d);
    }
    if (st && st->body[0]) {
        jw_rawz(&s, "\n\n# Output style: ");
        jw_rawz(&s, st->name);
        jw_rawz(&s, "\n\n");
        jw_rawz(&s, st->body);
    }
    if (r->mem.text.n) {
        jw_rawz(&s, mem_intro);
        jw_raw(&s, r->mem.text.p, r->mem.text.n);
    }
    if (r->sys_append) {
        jw_rawz(&s, "\n\n");
        jw_rawz(&s, r->sys_append);     /* --append-system-prompt */
    }
    if (s.oom) {
        jw_free(&s);
        return -1;
    }
    free(r->system);
    r->system = s.p;
    r->tools.memory = r->mem.text.n ? r->mem.text.p : 0;     /* subagents get CLAUDE.md too */
    return 0;
}

int repl_load_memory(cl_repl *r)
{
    mem_free(&r->mem);
    if (!r->bare && !r->safe)       /* --bare / --safe-mode: no CLAUDE.md */
        mem_load(&r->mem, r->sys, r->home, r->tools.root);
    return repl_system(r);
}

int repl_load(cl_repl *r)
{
    int i;
    char env[64];
    cfg_free(&r->cfg);
    r->cfg.skip = r->sources ? ~r->sources & 7u : 0;    /* --setting-sources */
    cfg_load(&r->cfg, r->sys, r->home, r->tools.root);
    for (i = 0; i < 2; i++)
        if (r->layer[i])            /* the command line's layer (A4 WP4): after the files, wins */
            cfg_merge(&r->cfg, CFG_SESSION, r->layer[i], (long)strlen(r->layer[i]),
                      i ? "the command line" : "--settings");
    if (r->bare || r->safe || r->cfg.no_hooks)
        cfg_drop_hooks(&r->cfg);    /* --bare, --safe-mode, disableAllHooks */
    if (r->safe)
        r->cfg.status_cmd[0] = 0;
    defs_free(&r->defs);
    defs_load(&r->defs, r->sys, r->home, r->tools.root);
    if (r->bare || r->safe)
        defs_drop(&r->defs, -1);    /* only the built-in output styles stay */
    if (r->no_slash) {
        defs_drop(&r->defs, DEF_COMMAND);
        defs_drop(&r->defs, DEF_SKILL);
    }
    if (r->agents_json) {
        char err[200];
        if (defs_add_agents_json(&r->defs, r->agents_json, err, sizeof(err)))
            repl_say(r, "", err);
    }
    if (r->cfg.verbose >= 0 && r->verbose != 2)
        r->verbose = r->cfg.verbose;
    /* the settings: the command line (main) comes after and wins */
    if (r->cfg.model[0])
        cl_copy(r->model, cfg_model(r->cfg.model), sizeof(r->model));
    if (r->cfg.model_src != CFG_SESSION && r->sys->getenv && r->sys->getenv(r->sys->u, "ANTHROPIC_MODEL", env, sizeof(env)) > 0)
        cl_copy(r->model, cfg_model(env), sizeof(r->model));   /* above the files, below --model */
    if (r->cfg.effort[0])
        cl_copy(r->effort, r->cfg.effort, sizeof(r->effort));
    if (r->cfg.fallback_model[0])
        cl_copy(r->fallback, cfg_model(r->cfg.fallback_model), sizeof(r->fallback));
    if (r->cfg.output_style[0])
        cl_copy(r->style, cl_strieq(r->cfg.output_style, "default") ? "" : r->cfg.output_style, sizeof(r->style));
    if (r->cfg.auto_compact >= 0)
        r->auto_compact = r->cfg.auto_compact;
    if (r->cfg.default_mode[0])
        r->ask_policy = ASKP_ASK;
    if (!strcmp(r->cfg.default_mode, "acceptEdits"))
        r->tools.perm.mode = PERM_ACCEPT;
    else if (!strcmp(r->cfg.default_mode, "plan"))
        r->tools.perm.mode = PERM_PLAN;
    else if (!strcmp(r->cfg.default_mode, "default") || !strcmp(r->cfg.default_mode, "manual"))
        r->tools.perm.mode = PERM_DEFAULT;
    else if (!strcmp(r->cfg.default_mode, "dontAsk") || !strcmp(r->cfg.default_mode, "bypassPermissions")) {
        r->tools.perm.mode = PERM_DEFAULT;
        r->ask_policy = r->cfg.default_mode[0] == 'd' ? ASKP_DENY : ASKP_BYPASS;
    }
    for (i = 0; i < r->cfg.nenv; i++)
        if (r->sys->setenv)
            r->sys->setenv(r->sys->u, r->cfg.env[i].k, r->cfg.env[i].v);
    if (r->cfg.err[0])
        repl_say(r, "Settings: ", r->cfg.err);
    if (r->tui)
        r->tui->status_pad = r->cfg.status_pad;
    if (menu_build(r) || pol_tools(r))
        return -1;
    return repl_load_memory(r);
}

/* a variable or the default */
static void var_or(cl_sys *sys, const char *name, const char *def, char *out, long cap)
{
    if (!sys->getenv || sys->getenv(sys->u, name, out, cap) <= 0)
        cl_copy(out, def, cap);
}

int repl_init(cl_repl *r, cl_io *io, cl_net *net, cl_sys *sys, const char *url, const char *key,
              const char *root)
{
    char cpdir[300];
    memset(r, 0, sizeof(*r));
    r->io = io;
    r->net = net;
    r->sys = sys;
    ui_init(&r->ui, io);
    ui_plain(&r->ui, &r->render);
    conv_init(&r->conv);
    jw_init(&r->errbody);
    jw_init(&r->pending);
    sse_init(&r->sse, on_event, r);
    stream_init(&r->st, 0);
    cfg_init(&r->cfg);
    mem_init(&r->mem);
    defs_init(&r->defs);
    if (http_parse_url(url, &r->url)) {
        show_err(r, "Not a usable URL: ", url);
        return -1;
    }
    /* no key for https: the start goes on to /login (A4 WP4, repl_need_key) */
    /* the key goes only over TLS: a plain http URL (the fixture) never sees it */
    r->key = r->url.tls ? key : 0;
    cl_copy(r->model, CL_DEFAULT_MODEL, sizeof(r->model));
    cl_copy(r->effort, CL_DEFAULT_EFFORT, sizeof(r->effort));
    r->max_tokens = CL_MAX_TOKENS;
    r->auto_compact = 1;
    r->tools.sys = sys;
    if (sys->canon(sys->u, root, r->tools.root, sizeof(r->tools.root)))
        cl_copy(r->tools.root, root, sizeof(r->tools.root));
    r->tools.timeout_s = 60;
    r->tools.u = r;
    r->tools.show = tool_show;
    r->tools.ask = tool_ask;
    r->tools.preview = tool_preview;
    r->tools.result = tool_result;
    r->tools.choose = tool_choose;
    r->tools.plan = tool_plan;
    r->tools.api.u = r;
    r->tools.api.send = api_send;
    r->tools.model = r->model;
    if (tools_init(&r->tools)) {
        ui_line(&r->ui, "Out of memory.");
        return -1;
    }
    var_or(sys, "CLAUDE_CONFIG_DIR", CL_HOME, r->home, sizeof(r->home));
    var_or(sys, "CLAUDE_CODE_TMPDIR", CL_TMP, r->tmp, sizeof(r->tmp));
    sess_init(&r->sess, sys, r->home, r->tools.root);
    sess_new(&r->sess, io->ms ? io->ms(io->u) : 0);
    if (path_join(r->tmp, "Claude-cp", cpdir, sizeof(cpdir)))
        cl_copy(cpdir, "T:Claude-cp", sizeof(cpdir));
    cp_init(&r->cp, sys, cpdir);
    checkpoint_use(&r->cp);
    r->hooks.cfg = &r->cfg;
    r->hooks.sys = sys;
    r->hooks.session_id = r->sess.id;
    r->hooks.transcript = r->sess.file;
    r->hooks.cwd = r->tools.root;
    r->hooks.tmp = r->tmp;
    pol_attach_ui(r);
    pol_attach_tools(r);
    if (repl_load(r))
        return -1;
    r->start_due = 1;            /* SessionStart runs once the command line is applied (--bare, ...) */
    return 0;
}

void repl_free(cl_repl *r)
{
    if (r->hooks.cfg)
        pol_session(r, HK_SESSION_END, "exit");
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
    tools_free(&r->tools);
    conv_free(&r->conv);
    free(r->system);
    r->system = 0;
    cfg_free(&r->cfg);
    mem_free(&r->mem);
    pol_ext_free(r);
    defs_free(&r->defs);
    cp_free(&r->cp);
    menu_free(r);
    jw_free(&r->pending);
    free(r->todos);
    r->todos = 0;
    r->hooks.cfg = 0;
    memset(r->keybuf, 0, sizeof(r->keybuf));
    free(r->sys_replace);
    free(r->sys_append);
    free(r->layer[0]);
    free(r->layer[1]);
    r->sys_replace = r->sys_append = r->layer[0] = r->layer[1] = 0;
    free(r->agents_json);
    free(r->schema);
    free(r->structured);
    r->agents_json = r->schema = r->structured = 0;
}

int repl_need_key(const cl_repl *r)
{
    return r->url.tls && !(r->key && *r->key);
}

/* A4 WP4: the start -- /login first when there is no key, else the
 * prompt the session starts with */
/* SessionStart "startup", once, after the command line was applied */
static void started(cl_repl *r)
{
    if (r->start_due) {
        r->start_due = 0;
        pol_session(r, HK_SESSION_START, "startup");
    }
}

static void start(cl_repl *r)
{
    started(r);
    if (repl_need_key(r))
        repl_line(r, "/login");
    else if (r->first) {
        const char *f = r->first;
        r->first = 0;
        ui_user(&r->ui, f);
        repl_line(r, f);
    }
}
