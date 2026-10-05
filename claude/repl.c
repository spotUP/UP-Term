/* repl -- see repl.h. */
#include <stdlib.h>
#include <string.h>
#include "repl_int.h"
#include "path.h"
#include "tui.h"
#include "show.h"
#include "tools_int.h"
#include "trust.h"
#include "util.h"

enum { R_OK, R_RETRY, R_FAIL, R_CANCEL };

static void started(cl_repl *r);
static void turn(cl_repl *r, const char *prompt, long pn);
static void settings_changed(cl_repl *r);
static void cfg_times(cl_repl *r);

const char *repl_effort(const cl_repl *r)
{
    /* CLAUDE_CODE_EFFORT_LEVEL wins over --effort, /effort and the settings (Claude Code) */
    static char env[16];
    if (r->sys && r->sys->getenv && r->sys->getenv(r->sys->u, "CLAUDE_CODE_EFFORT_LEVEL", env, sizeof(env)) > 0 &&
        (!strcmp(env, "low") || !strcmp(env, "medium") || !strcmp(env, "high") || !strcmp(env, "xhigh") ||
         !strcmp(env, "max") || !strcmp(env, "auto")))
        return strcmp(env, "auto") ? env : "";
    return strcmp(r->effort, "auto") ? r->effort : "";     /* auto: none sent, the model's own */
}

/* CLAUDE_CODE_MAX_CONTEXT_TOKENS, CLAUDE_CODE_DISABLE_1M_CONTEXT (repl_env sets them) */
static long win_override;
static int win_no1m;

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
    "task list with the task tools when you have them (TaskCreate and TaskUpdate, or TodoWrite).";

static const char auto_a[] =
    "\n\n# Auto memory\n\nYou have a memory directory of your own for this project: ";
static const char auto_b[] =
    " (it may not exist yet; Write makes it). Keep in it what is worth knowing in a later session -- the "
    "user's preferences, facts about this project and this machine, what was learned the hard way -- one "
    "topic a file, and MEMORY.md as the index (its first 200 lines are read at every start). Update or remove "
    "what turns out wrong. Do not save what the code or the conversation already makes plain.";

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

/* an id of the turn or the message for MessageDisplay (uuid-shaped) */
static void new_id(cl_repl *r, char *out)
{
    static const char hex[] = "0123456789abcdef";
    unsigned long s = r->id_seed = r->id_seed * 1103515245UL + 12345UL + (r->io->ms ? r->io->ms(r->io->u) : 0);
    int i, k = 0;
    for (i = 0; i < 32; i++) {
        if (!(i & 7))
            s = s * 1103515245UL + 12345UL;
        if (i == 8 || i == 12 || i == 16 || i == 20)
            out[k++] = '-';
        out[k++] = hex[i == 12 ? 4 : (s >> (4 * (i & 7))) & 15];
    }
    out[k] = 0;
    r->id_seed = s;
}

/* MessageDisplay (A4 gaps 2, Claude Code's): the hook gets a batch of the
 * answer's text -- completed lines; the last batch final -- and what it
 * returns as displayContent is drawn in its place (only drawn: the
 * conversation keeps the original). render 0: print mode, nothing drawn. */
static void md_batch(cl_repl *r, const char *s, long n, int final, int render)
{
    cl_hookres h;
    jw ex;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, ",\"turn_id\":");
    jw_strz(&ex, r->turn_id);
    jw_rawz(&ex, ",\"message_id\":");
    jw_strz(&ex, r->msg_id);
    jw_rawz(&ex, ",\"index\":");
    jw_long(&ex, r->md_index++);
    jw_rawz(&ex, final ? ",\"final\":true,\"delta\":" : ",\"final\":false,\"delta\":");
    jw_str(&ex, s, n);
    if (!ex.oom)
        hooks_run(&r->hooks, HK_MESSAGE_DISPLAY, "", ex.p, &h);
    r->n_md++;
    if (render) {
        if (h.has_display)
            r->render.text(r->render.u, h.display.p ? h.display.p : "", h.display.n);
        else if (n)
            r->render.text(r->render.u, s, n);
    }
    jw_free(&ex);
    hookres_free(&h);
}

/* the text held for MessageDisplay drawn: the whole lines (final 0), or
 * all of it as the message's last batch (final 1) */
static void md_flush(cl_repl *r, int final)
{
    long k = r->md.n;
    if (!r->md_on)
        return;
    if (!final)
        while (k > 0 && r->md.p[k - 1] != '\n')
            k--;
    if (k || final)
        md_batch(r, r->md.p ? r->md.p : "", k, final, 1);
    if (k) {
        memmove(r->md.p, r->md.p + k, (size_t)(r->md.n - k));
        r->md.n -= k;
        r->md.p[r->md.n] = 0;
    }
    if (final)
        r->md_on = 0;
}

static void st_text(void *u, const char *s, long n)
{
    cl_repl *r = (cl_repl *)u;
    r->shown = 1;
    r->chars += n;
    ui_tokens(&r->ui, r->turn_out + r->chars / 4);
    if (r->md_on) {
        jw_raw(&r->md, s, n);       /* MessageDisplay: drawn a batch of whole lines at a time */
        if (memchr(s, '\n', (size_t)n))
            md_flush(r, 0);
        return;
    }
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
    else if (type == B_SERVER && !strcmp(name, "advisor")) {
        /* Claude Code's "Advising" line, with the advisor model's name */
        cl_copy(what, "Advising (", sizeof(what));
        cl_cat(what, r->tools.advisor, sizeof(what));
        cl_cat(what, ")", sizeof(what));
        ui_status(&r->ui, what);
    } else if (type == B_SERVER)
        ui_status(&r->ui, "Searching the web");
    else if (type == B_TOOL) {
        if (r->md_on && r->md.n) {
            /* the text before a tool call: its lines so far, the rest as a batch */
            md_flush(r, 0);
            if (r->md.n) {
                md_batch(r, r->md.p, r->md.n, 0, 1);
                jw_reset(&r->md);
            }
        }
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
    jw_reset(&r->md);
    r->md_on = !r->no_person && !r->quiet_req && hooks_has(&r->hooks, HK_MESSAGE_DISPLAY);
    if (r->md_on) {
        new_id(r, r->msg_id);
        r->md_index = 0;
    }
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
    if (r->betas[0] || (r->tools.advisor[0] && strstr(body, "\"advisor_20260301\""))) {
        /* --betas: added to the model's own (anthropic-beta takes a comma list);
         * the advisor tool's beta when the request declares it (/advisor) */
        cl_copy(beta, q.beta, sizeof(beta));
        if (r->tools.advisor[0] && strstr(body, "\"advisor_20260301\"")) {
            if (beta[0])
                cl_cat(beta, ",", sizeof(beta));
            cl_cat(beta, "advisor-tool-2026-03-01", sizeof(beta));
        }
        if (r->betas[0]) {
            if (beta[0])
                cl_cat(beta, ",", sizeof(beta));
            cl_cat(beta, r->betas, sizeof(beta));
        }
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
        if (attempt >= (r->tries > 0 ? r->tries : CL_TRIES)) {
            char m[80], num[16];
            cl_copy(m, "Giving up after ", sizeof(m));
            cl_ltoa(attempt, num);
            cl_cat(m, num, sizeof(m));
            cl_cat(m, attempt == 1 ? " attempt." : " attempts.", sizeof(m));
            ui_line(&r->ui, m);
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
            cl_cat(m, " of ", sizeof(m));
            cl_ltoa(r->tries > 0 ? r->tries : CL_TRIES, num);
            cl_cat(m, num, sizeof(m));
            cl_cat(m, "). Ctrl+C stops.", sizeof(m));
            ui_line(&r->ui, m);
        }
        if (r->feed && r->feed->retry && !r->quiet_req) {
            /* stream-json's system/api_retry (Claude Code's error kinds) */
            int st = r->resp.status == 200 ? 0 : r->resp.status;
            r->feed->retry(r->feed->u, attempt, (r->tries > 0 ? r->tries : CL_TRIES) - 1, wait * 1000, st,
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
static unsigned long tool_clock(void *u)
{
    cl_repl *r = (cl_repl *)u;
    return r->io->ms ? r->io->ms(r->io->u) : 0;
}

static void tool_show(void *u, const char *tool, const char *what)
{
    cl_repl *r = (cl_repl *)u;
    if (r->at->cur == T_TASK) {
        /* the subagent's color in its call's header (Claude Code) */
        jv in, x;
        char type[64];
        const cl_agent *a;
        type[0] = 0;
        if (json_parse(r->at->cur_in, r->at->cur_inn, &in) == 0 && json_get(in, "subagent_type", &x))
            json_str(x, type, sizeof(type));
        a = type[0] ? tools_agent(r->at, type) : 0;
        r->ui.name_sgr = a ? theme_named(a->color) : 0;
    }
    ui_tool(&r->ui, r->at->cur, tool, what, r->at->cur_in, r->at->cur_inn);
}

void repl_denied(cl_repl *r, const char *tool, const char *input, long n)
{
    if (r->feed && r->feed->denied)
        r->feed->denied(r->feed->u, tool, r->cur_id ? r->cur_id : "", input, n);
}

int repl_ask(cl_repl *r, int tid, const char *tool, const char *what, int outside, int rule, char *note,
             long cap)
{
    char m[200];
    /* a subagent's permissionMode (dontAsk, bypassPermissions) is its own */
    int pol = r->at && r->at->ask_policy ? r->at->ask_policy - 1 : r->ask_policy;
    /* A4 WP4: nobody to ask (print mode, dontAsk): denied, but a read in
     * the start directory runs (Claude Code: no approval needed there);
     * bypassPermissions: yes, except to an explicit ask rule */
    if ((r->at ? r->at : &r->tools)->perm.mode == PERM_BYPASS && !rule)
        return ASK_ONCE;
    {
        /* Claude Code: PermissionRequest hooks answer before the dialog
         * (in print mode, before the "no" nobody would say) */
        int hk = pol_permission_request(r, tool, r->at ? r->at->cur_in : 0, r->at ? r->at->cur_inn : 0);
        if (hk == RULE_ALLOW)
            return r->perm_upd.n ? ASK_RERUN : ASK_ONCE;    /* updatedInput: the call again with it */
        if (hk == RULE_DENY) {
            repl_denied(r, tool, r->at ? r->at->cur_in : 0, r->at ? r->at->cur_inn : 0);
            return ASK_NO;
        }
    }
    if (r->no_person || pol == ASKP_DENY) {
        if (!rule && !outside && tid >= 0 && perm_read_only(tid))
            return ASK_ONCE;
        repl_denied(r, tool, r->at->cur_in, r->at->cur_inn);
        return ASK_NO;
    }
    cl_copy(m, "Claude needs your permission to use ", sizeof(m));
    cl_cat(m, cfg_cc_tool(tool), sizeof(m));
    pol_notify(r, "permission_prompt", m);
    {
        int ans = ui_ask(&r->ui, tid, tool, what, outside);
        if (note && cap)
            cl_copy(note, r->ui.ask_note, cap);     /* Tab's comment on Yes / No */
        if (ans == ASK_PROJECT) {
            pol_keep_rule(r, tool, r->at ? r->at->cur_in : 0, r->at ? r->at->cur_inn : 0);
            ans = ASK_ONCE;
        }
        return ans;
    }
}

static int tool_ask(void *u, const char *tool, const char *what, int outside, char *note, long cap)
{
    cl_repl *r = (cl_repl *)u;
    if (r->rule_now == RULE_ALLOW) {
        /* a permission rule (or a hook) allowed it: no question */
        r->n_rule_allow++;
        return ASK_ONCE;
    }
    return repl_ask(r, r->at->cur, tool, what, outside, r->rule_now == RULE_ASK, note, cap);
}

static int tool_wait(void *u, long ms)
{
    return ui_wait(&((cl_repl *)u)->ui, ms);
}

/* the lines a change takes out and puts in (a common start and end
 * trimmed, as the diff on the screen counts them) */
static void count_lines(const char *a, long an, const char *b, long bn, long *del, long *add)
{
    long p = 0, q = 0, i;
    while (p < an && p < bn && a[p] == b[p])
        p++;
    while (p > 0 && a[p - 1] != '\n')
        p--;
    while (q < an - p && q < bn - p && a[an - 1 - q] == b[bn - 1 - q])
        q++;
    while (q > 0 && a[an - q] != '\n' && an - q > p)
        q--;
    *del = *add = 0;
    for (i = p; i < an - q; i++)
        *del += a[i] == '\n';
    for (i = p; i < bn - q; i++)
        *add += b[i] == '\n';
    if (an - q > p && a[an - q - 1] != '\n')
        (*del)++;
    if (bn - q > p && b[bn - q - 1] != '\n')
        (*add)++;
}

static void tool_preview(void *u, int tool, const char *path, const char *before, long bn, const char *after,
                         long an)
{
    cl_repl *r = (cl_repl *)u;
    long d, a;
    count_lines(before ? before : "", before ? bn : 0, after ? after : "", after ? an : 0, &d, &a);
    r->pend_del = d;            /* counted when the change is made (tool_result) */
    r->pend_add = a;
    ui_preview(&r->ui, tool, path, before, bn, after, an);
}

static void tool_result(void *u, int tool, const char *in, long inn, int is_error, const char *text, long n)
{
    cl_repl *r = (cl_repl *)u;
    if ((tool == T_WRITE || tool == T_EDIT || tool == T_MULTIEDIT) && !is_error) {
        r->lines_removed += r->pend_del;    /* the status line's cost.total_lines_* */
        r->lines_added += r->pend_add;
    }
    r->pend_del = r->pend_add = 0;
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
    if ((tool == T_TASK_CREATE || tool == T_TASK_UPDATE) && !is_error) {
        /* the task list as TodoWrite's: /todos and the screen's list (Ctrl+T) show it */
        char *td = tasks_todos(r->at->tasks);
        free(r->todos);
        r->todos = td;
        ui_result(&r->ui, T_TODO_WRITE, td ? td : "{\"todos\":[]}", td ? (long)strlen(td) : 12, 0, text, n);
        return;
    }
    if (r->show)
        r->show->brief = r->at->brief;
    ui_result(&r->ui, tool, in, inn, is_error, text, n);
    if (r->show)
        r->show->brief = 0;
}

/* Claude Code's task tool availability: the model's default, or asked for
 * (CLAUDE_CODE_ENABLE_TODO_TOOLS=1, --allowedTools / --tools naming one);
 * CLAUDE_CODE_ENABLE_TASKS=0 gives TodoWrite in their place */
static int model_allowed(const cl_repl *r, const char *m);
static void snap_record(cl_repl *r);
static void snap_load(cl_repl *r);
static int env_on(cl_repl *r, const char *name);
static void env_str(cl_repl *r, const char *name, char *out, long cap);

/* the advisor in force: --advisor, else advisorModel; attached only where
 * Claude Code would (the pairing table, availableModels, not turned off) */
static void advisor_now(cl_repl *r)
{
    const char *a = r->advisor_cli[0] ? r->advisor_cli : r->cfg.advisor;
    char v[8];
    r->tools.advisor[0] = 0;
    if (!a[0] || (r->sys->getenv && r->sys->getenv(r->sys->u, "CLAUDE_CODE_DISABLE_ADVISOR_TOOL", v, sizeof(v)) > 0 &&
                  strcmp(v, "0")))
        return;
    if (conv_advisor_ok(r->model, cfg_model(a)) == 0 && model_allowed(r, cfg_model(a)))
        cl_copy(r->tools.advisor, cfg_model(a), sizeof(r->tools.advisor));
}

int repl_todo_mode(cl_repl *r)
{
    char v[8];
    int avail = tools_todo_default(r->model) == TODO_TASKS || r->todo_optin ||
                (r->sys->getenv && r->sys->getenv(r->sys->u, "CLAUDE_CODE_ENABLE_TODO_TOOLS", v, sizeof(v)) > 0 &&
                 strcmp(v, "0"));
    if (!avail)
        return TODO_NONE;
    if (r->sys->getenv && r->sys->getenv(r->sys->u, "CLAUDE_CODE_ENABLE_TASKS", v, sizeof(v)) > 0 && !strcmp(v, "0"))
        return TODO_WRITE;
    return TODO_TASKS;
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
    if (win_override > 0)
        return win_override;        /* CLAUDE_CODE_MAX_CONTEXT_TOKENS */
    return !strncmp(model, "claude-haiku", 12) || win_no1m ? 200000L : 1000000L;
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
    sched_save(r);                  /* the cron jobs, when they changed */
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
    sched_news(r, &extra);          /* background tasks, async hooks: news beside the results */
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
    return (w / 100) * (r->compact_pct > 0 ? r->compact_pct : CL_COMPACT_PCT);
}

/* the auto-compact threshold passed? */
#define GOAL_ROUNDS 10

/* /goal: has the turn met the condition? A small model reads the last
 * answer; when not, Claude goes on (at most GOAL_ROUNDS turns a goal) */
static void goal_check(cl_repl *r)
{
    jw q, a, last;
    char err[120];
    int i;
    if (r->goal_rounds >= GOAL_ROUNDS) {
        repl_say(r, "The goal is still open after ten more turns; it stays set (/goal clear ends it): ", r->goal);
        r->goal_rounds = 0;
        return;
    }
    jw_init(&q);
    jw_init(&a);
    jw_init(&last);
    for (i = r->conv.n - 1; i >= 0 && !last.n; i--)
        if (!r->conv.m[i].user)
            jw_raw(&last, r->conv.m[i].json, r->conv.m[i].n > 8000 ? 8000 : r->conv.m[i].n);
    jw_rawz(&q, "The goal: ");
    jw_rawz(&q, r->goal);
    jw_rawz(&q, "\n\nThe assistant's last answer (content blocks):\n");
    jw_raw(&q, last.p ? last.p : "", last.n);
    jw_rawz(&q, "\n\nIs the goal met? Answer MET, or NOT MET and in one sentence what is missing.");
    if (!q.oom && agent_query(&r->tools, "claude-haiku-4-5", "You judge whether a goal is reached.", q.p, q.n, &a, err,
                              sizeof(err)) == 0) {
        if (a.n >= 3 && !strncmp(a.p, "MET", 3)) {
            repl_say(r, "Goal met: ", r->goal);
            r->goal[0] = 0;
            r->goal_rounds = 0;
        } else {
            jw go;
            jw_init(&go);
            jw_rawz(&go, "Keep working toward the goal: ");
            jw_rawz(&go, r->goal);
            jw_rawz(&go, "\nNot met yet: ");
            jw_raw(&go, a.p ? a.p : "", a.n);
            r->goal_rounds++;
            if (!go.oom)
                turn(r, go.p, go.n);
            jw_free(&go);
        }
    }
    jw_free(&q);
    jw_free(&a);
    jw_free(&last);
}

/* Stop hooks may send Claude on this often a turn (Claude Code: 8,
 * CLAUDE_CODE_STOP_HOOK_BLOCK_CAP) */
static int stop_cap(cl_repl *r)
{
    char v[16];
    if (r->sys->getenv && r->sys->getenv(r->sys->u, "CLAUDE_CODE_STOP_HOOK_BLOCK_CAP", v, sizeof(v)) > 0 &&
        atoi(v) > 0)
        return atoi(v);
    return 8;
}

static int too_full(cl_repl *r)
{
    return r->auto_compact && r->conv.n > 1 && r->ctx_used > repl_compact_at(r);
}

/* the turn's tools JSON (read anew each round: a skill may have been
 * loaded on the way), with --json-schema's StructuredOutput tool */
static const char *turn_tools_json(cl_repl *r, jw *xtools)
{
    const char *t = tools_json(&r->tools, r->model);
    jw_reset(xtools);
    if (r->schema && t) {
        /* --json-schema: Claude Code's StructuredOutput tool, the schema as its input */
        long tn = (long)strlen(t);
        if (tn >= 2 && t[tn - 1] == ']') {
            jw_raw(xtools, t, tn - 1);
            if (tn > 2)
                jw_raw(xtools, ",", 1);
        } else
            jw_raw(xtools, "[", 1);
        jw_rawz(xtools, "{\"name\":\"StructuredOutput\",\"description\":\"Use this tool to return your final "
                        "response in the requested structured format. You MUST call this tool exactly once at "
                        "the end of your response to provide the structured output.\",\"input_schema\":");
        jw_rawz(xtools, r->schema);
        jw_rawz(xtools, "}]");
        if (!xtools->oom)
            return xtools->p;
    }
    return t;
}

/* Does the conversation end with an assistant message whose tool calls have
 * no results (a run that stopped at a deferred call)? */
int repl_has_pending(const cl_repl *r)
{
    const cl_msg *m;
    if (!r->conv.n)
        return 0;
    m = &r->conv.m[r->conv.n - 1];
    return !m->user && m->json && strstr(m->json, "\"type\":\"tool_use\"") != 0;
}

/* those calls run (PreToolUse fires again: Claude Code's resume of a
 * defer), their results the next user message: 0, -1 failed; defer_id set
 * when a hook deferred again (nothing added then) */
static int run_pending(cl_repl *r)
{
    const cl_msg *m = &r->conv.m[r->conv.n - 1];
    jw content, extra;
    jv v, b, x, in;
    jit it;
    int k = 0;
    if (!repl_has_pending(r) || json_parse(m->json, m->n, &v))
        return -1;
    jw_init(&content);
    jw_init(&extra);
    jw_raw(&content, "[", 1);
    r->round_ntools = 0;
    json_iter(v, &it);
    while (json_next(&it, 0, &b))
        r->round_ntools += json_get(b, "type", &x) && json_streq(x, "tool_use");
    json_iter(v, &it);
    while (json_next(&it, 0, &b)) {
        char id[96], name[64];
        if (!json_get(b, "type", &x) || !json_streq(x, "tool_use") || !json_get(b, "input", &in))
            continue;
        id[0] = name[0] = 0;
        if (json_get(b, "id", &x))
            json_str(x, id, sizeof(id));
        if (json_get(b, "name", &x))
            json_str(x, name, sizeof(name));
        if (k++)
            jw_raw(&content, ",", 1);
        pol_call(r, &r->tools, id, name, 1, in.p, in.n, &content, &extra);
    }
    if (r->defer_id[0]) {
        jw_free(&content);
        jw_free(&extra);
        return 0;
    }
    {
        /* Claude Code: the deferred call's result goes with a continuation
         * message (CLAUDE_CODE_RESUME_PROMPT) */
        char cont[300];
        env_str(r, "CLAUDE_CODE_RESUME_PROMPT", cont, sizeof(cont));
        if (extra.n)
            jw_rawz(&extra, "\n\n");
        jw_rawz(&extra, cont[0] ? cont : "Continue from where you left off.");
    }
    if (extra.n) {
        jw_rawz(&content, ",{\"type\":\"text\",\"text\":");
        jw_str(&content, extra.p, extra.n);
        jw_raw(&content, "}", 1);
    }
    jw_raw(&content, "]", 1);
    k = content.oom || conv_add(&r->conv, 1, content.p, content.n) ? -1 : 0;
    if (!k && r->feed && r->feed->message)
        r->feed->message(r->feed->u, 1, content.p, content.n);
    jw_free(&content);
    jw_free(&extra);
    return k;
}

void repl_pending(cl_repl *r)
{
    turn(r, 0, 0);
}

static void turn(cl_repl *r, const char *prompt, long pn)
{
    cl_mark m0 = conv_mark(&r->conv);
    cl_opts o;
    jw body, content, xtools;
    int answered = 0, round, stops = 0, rt;
    const char *turn_tools = r->turn_tools;     /* a SlashCommand's allowed-tools last this turn */
    const char *blocks = r->blocks;     /* a prompt given as content blocks (images: stream-json input) */
    long bn = r->blocks_n;
    r->blocks = 0;
    r->turn_rc = TURN_FAIL;
    if (!r->conv.n) {
        snap_record(r);             /* --system-prompt-snapshot: this conversation's flags' text */
        repl_system(r);
    }
    if (!r->conv.n && r->no_dynamic && r->mem.auto_dir[0]) {
        /* --exclude-dynamic-system-prompt-sections: the per-user part goes with the first prompt */
        if (r->pending.n)
            jw_rawz(&r->pending, "\n\n");
        jw_rawz(&r->pending, auto_a + 2);
        jw_rawz(&r->pending, r->mem.auto_dir);
        jw_rawz(&r->pending, auto_b);
    }
    cp_turn(&r->cp, r->conv.n);
    if (!prompt) {
        /* a resumed run's pending tool calls (a PreToolUse defer): run now, the
         * turn goes on from their results */
        int k = run_pending(r);
        if (k < 0) {
            conv_rollback(&r->conv, m0);
            return;
        }
        if (r->defer_id[0]) {
            r->turn_rc = TURN_DEFERRED;     /* deferred again */
            return;
        }
    } else if ((blocks ? conv_add_user_blocks(&r->conv, blocks, bn) : conv_add_user_text(&r->conv, prompt, pn)) ||
               (r->pending.n && conv_add_user_text(&r->conv, r->pending.p, r->pending.n))) {
        conv_rollback(&r->conv, m0);
        ui_line(&r->ui, "Out of memory.");
        return;
    }
    jw_reset(&r->pending);
    memset(&o, 0, sizeof(o));
    o.model = r->model;
    o.effort = repl_effort(r);
    o.max_tokens = r->max_tokens;
    o.system = r->system;
    o.no_thinking = r->no_thinking;
    o.think_off = r->think_off;
    o.extra = r->extra_body;
    jw_init(&xtools);
    jw_init(&body);
    jw_init(&content);
    r->io->brk(r->io->u);           /* a Ctrl+C from before the turn does not count */
    r->turn_out = 0;
    r->in_turn = 1;
    new_id(r, r->turn_id);          /* MessageDisplay's turn_id */
    ui_busy(&r->ui, 1);
    for (round = 0; round < 64; round++) {
        int rc, ntools;
        const char *stop;
        r->tools.stop = 0;
        r->tools.todo_mode = repl_todo_mode(r);
        advisor_now(r);             /* /advisor: attached when it may advise this model */
        o.tools = turn_tools_json(r, &xtools);
        jw_reset(&body);
        if (conv_body(&r->conv, &o, &body)) {
            ui_line(&r->ui, "Out of memory.");
            break;
        }
        rc = request(r, body.p, body.n);
        if (rc != R_OK)
            md_flush(r, 1);         /* what was held for MessageDisplay still drawn */
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
        if (rc != R_OK) {
            if (rc == R_FAIL) {
                int st = r->resp.status;
                r->api_failed = st == 401 || st == 403 ? "authentication_failed" : st == 429 ? "rate_limit"
                                : st == 400 ? "invalid_request" : st == 402 ? "billing_error"
                                : st >= 500 || r->busy_fail ? "server_error" : "unknown";
            }
            break;
        }
        r->n_responses++;
        conv_usage(&r->conv, r->st.model[0] ? r->st.model : r->model, r->st.in_tok, r->st.out_tok,
                   r->st.cache_w, r->st.cache_r);
        r->ctx_used = r->st.in_tok + r->st.cache_r + r->st.cache_w + r->st.out_tok;
        r->turn_out += r->st.out_tok;
        ctx_show(r);
        stop = r->st.stop_reason;
        ntools = stream_tools(&r->st);
        md_flush(r, 1);             /* MessageDisplay: the message's last batch */
        if (r->no_person && hooks_has(&r->hooks, HK_MESSAGE_DISPLAY)) {
            /* print mode: once per message, its whole text (Claude Code) */
            jw all;
            int k;
            jw_init(&all);
            for (k = 0; k < r->st.nb; k++)
                if (r->st.b[k].type == B_TEXT && r->st.b[k].a.n)
                    jw_raw(&all, r->st.b[k].a.p, r->st.b[k].a.n);
            if (all.n) {
                new_id(r, r->msg_id);
                r->md_index = 0;
                md_batch(r, all.p, all.n, 1, 0);
            }
            jw_free(&all);
        }
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
            again = stops < stop_cap(r) && pol_stop(r, stops > 0, &why);
            if (again && conv_add_user_text(&r->conv, why.p, why.n) == 0) {
                stops++;
                jw_free(&why);
                continue;
            }
            jw_free(&why);
            break;
        }
        r->round_ntools = ntools;
        rt = run_tools(r, ntools, &content);
        if (rt == 0 && r->defer_id[0]) {
            /* PreToolUse "defer" (print mode, one call): the run stops at the
             * call, kept unanswered in the session for a --resume */
            r->turn_rc = TURN_DEFERRED;
            break;
        }
        if (rt || conv_add(&r->conv, 1, content.p, content.n)) {
            ui_line(&r->ui, "Out of memory.");
            /* the tool_use turn is in; without its results the history is
             * invalid, so take the turn back out */
            answered = 0;
            r->turn_rc = TURN_FAIL;
            break;
        }
        if (r->feed && r->feed->message)
            r->feed->message(r->feed->u, 1, content.p, content.n);
        {
            /* PostToolBatch: once with the whole round (a block ends the turn) */
            jw calls, why;
            int k, stop;
            jw_init(&calls);
            jw_init(&why);
            jw_raw(&calls, "[", 1);
            for (k = 0; k < ntools; k++) {
                sblock *tb = stream_tool(&r->st, k);
                if (k)
                    jw_raw(&calls, ",", 1);
                jw_rawz(&calls, "{\"tool_name\":");
                jw_strz(&calls, cfg_cc_tool(tb->name));
                jw_rawz(&calls, ",\"tool_input\":");
                if (tb->input_ok && tb->a.n)
                    jw_raw(&calls, tb->a.p, tb->a.n);
                else
                    jw_rawz(&calls, "{}");
                jw_rawz(&calls, ",\"tool_use_id\":");
                jw_strz(&calls, tb->id);
                jw_raw(&calls, "}", 1);
            }
            jw_raw(&calls, "]", 1);
            stop = !calls.oom && pol_batch(r, calls.p, calls.n, &why);
            if (stop && why.n)
                conv_add_user_text(&r->conv, why.p, why.n);     /* Claude sees why, next time */
            jw_free(&calls);
            jw_free(&why);
            if (stop)
                break;
        }
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
    r->in_turn = 0;
    r->turn_tools = turn_tools;
    if (r->turn_rc == TURN_FAIL && r->api_failed)
        pol_stop_failure(r, r->api_failed);    /* the turn ended on an API error */
    r->api_failed = 0;
    if (!answered)
        conv_rollback(&r->conv, m0);
    else
        session_save(r);
    jw_free(&body);
    jw_free(&content);
    jw_free(&xtools);
    request_free(r);
    r->idle_from = r->io->ms ? r->io->ms(r->io->u) : 0;     /* Notification idle_prompt counts from here */
    r->idle_told = 0;
    if (answered && r->turn_rc == TURN_OK && r->goal[0] && !r->no_person)
        goal_check(r);
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
    if (r->no_compact) {
        ui_line(&r->ui, "Compaction is turned off (DISABLE_COMPACT).");
        return;
    }
    if (pol_precompact(r, automatic, focus))
        return;                     /* a PreCompact hook said no */
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
    o.effort = repl_effort(r);
    o.max_tokens = r->max_tokens;
    o.system = r->system;
    o.no_thinking = r->no_thinking;
    o.think_off = r->think_off;
    o.extra = r->extra_body;
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
            snap_record(r);         /* Claude Code: after a compaction this launch's flags count */
            repl_system(r);
            session_save(r);
            pol_postcompact(r, automatic, sum.p, sum.n);
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

int repl_side(cl_repl *r, int from, int to, const char *ask, jw *answer)
{
    cl_conv c;
    cl_opts o;
    cl_render keep = r->render;
    jw body;
    int rc, i;
    conv_init(&c);
    if (to > r->conv.n)
        to = r->conv.n;
    for (i = from; i < to; i++)
        if (conv_add(&c, r->conv.m[i].user, r->conv.m[i].json, r->conv.m[i].n)) {
            conv_free(&c);
            return -1;
        }
    /* a range that ends inside a tool round: its tool_use answered by nothing
     * would be refused, so a range always ends after an answer or a prompt */
    if (conv_add_user_text(&c, ask, (long)strlen(ask))) {
        conv_free(&c);
        return -1;
    }
    memset(&o, 0, sizeof(o));
    o.model = r->model;
    o.effort = repl_effort(r);
    o.max_tokens = r->max_tokens;
    o.system = r->system;
    o.no_thinking = r->no_thinking;
    o.think_off = r->think_off;
    o.extra = r->extra_body;
    o.tools = tools_json(&r->tools, r->model);
    o.no_tools = 1;                 /* the tools stay listed (the cache), none is called */
    jw_init(&body);
    r->render.u = answer;
    r->render.text = capture_text;
    r->render.end = capture_end;
    r->io->brk(r->io->u);
    ui_busy(&r->ui, 1);
    rc = conv_body(&c, &o, &body) ? R_FAIL : request(r, body.p, body.n);
    ui_busy(&r->ui, 0);
    r->render = keep;
    if (rc == R_OK)
        conv_usage(&r->conv, r->st.model[0] ? r->st.model : r->model, r->st.in_tok, r->st.out_tok, r->st.cache_w,
                   r->st.cache_r);
    jw_free(&body);
    conv_free(&c);
    request_free(r);
    return rc == R_OK && answer->n ? 0 : rc == R_CANCEL ? -2 : -1;
}

static const char sum_ask[] =
    "Summarise the conversation above for continuing it: the goals, the decisions, the files and paths, "
    "what was changed and what is still open. Be complete but concise. Do not call any tool.";

int repl_summarize(cl_repl *r, int msg, int up_to)
{
    jw sum, seed;
    int rc, i;
    cl_conv c;
    if (msg < 0 || msg >= r->conv.n || !r->conv.m[msg].user)
        return -1;
    jw_init(&sum);
    rc = up_to ? repl_side(r, 0, msg, sum_ask, &sum) : repl_side(r, msg, r->conv.n, sum_ask, &sum);
    if (rc) {
        jw_free(&sum);
        if (rc == -1)
            ui_line(&r->ui, "No summary came back; the conversation is as it was.");
        return rc;
    }
    jw_init(&seed);
    jw_rawz(&seed, up_to ? "Summary of the conversation before this point:\n\n"
                         : "Summary of the conversation from here on (it was summarised):\n\n");
    jw_raw(&seed, sum.p, sum.n);
    jw_free(&sum);
    conv_init(&c);
    if (seed.oom)
        rc = -1;
    else if (up_to) {
        /* the summary, then the chosen prompt and everything after it */
        rc = conv_add_user_text(&c, seed.p, seed.n);
        for (i = msg; i < r->conv.n && !rc; i++) {
            const cl_msg *m = &r->conv.m[i];
            if (i == msg && m->n > 2)
                rc = conv_add_user_blocks(&c, m->json + 1, m->n - 2);     /* its blocks join the summary's message */
            else
                rc = conv_add(&c, m->user, m->json, m->n);
        }
    } else {
        /* everything before the chosen prompt, then the summary of the rest */
        for (i = 0; i < msg && !rc; i++)
            rc = conv_add(&c, r->conv.m[i].user, r->conv.m[i].json, r->conv.m[i].n);
        if (!rc)
            rc = conv_add_user_text(&c, seed.p, seed.n);
    }
    jw_free(&seed);
    if (rc) {
        conv_free(&c);
        ui_line(&r->ui, "Out of memory: the conversation is as it was.");
        return -1;
    }
    /* the counters stay; the messages are the new ones */
    for (i = 0; i < r->conv.n; i++)
        free(r->conv.m[i].json);
    free(r->conv.m);
    r->conv.m = c.m;
    r->conv.n = c.n;
    r->conv.cap = c.cap;
    sess_truncate(&r->sess, up_to ? 0 : msg);
    cp_turn(&r->cp, r->conv.n);
    r->ctx_used = 0;
    ctx_show(r);
    session_save(r);
    ui_line(&r->ui, up_to ? "Summarised the conversation up to that prompt; it goes on from there."
                          : "Summarised the conversation from that prompt on.");
    return 0;
}

static int start(cl_repl *r);
static void started(cl_repl *r);
static void turn(cl_repl *r, const char *prompt, long pn);
static void settings_changed(cl_repl *r);
static void cfg_times(cl_repl *r);

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
    cp_session(&r->cp, r->sess.file);     /* its snapshots: /rewind reaches back before the resume */
    pol_session(r, HK_SESSION_START, "resume");
    sched_load(r, 1);               /* Claude Code: a resume restores the session's cron jobs */
    watch_start(r);
    snap_load(r);                   /* the flags' text it started with, until a compaction */
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
        if (m[k - 1] != ' ')
            cl_cat(m, "  ", sizeof(m)); /* a name wider than the column: still a gap */
        cl_cat(m, r->menu[i].help, sizeof(m));
        ui_line(&r->ui, m);
    }
    ui_line(&r->ui, r->tui ? help_keys : "Ctrl+C stops an answer or a command. Anything else is sent to Claude.");
    if (r->tui)
        ui_line(&r->ui, help_keys2);
    {
        /* Claude Code's commands that cannot be here: typed, each says why */
        char m[900];
        const char *w;
        cl_copy(m, "Not on the Amiga (type one to see why):", sizeof(m));
        for (i = 0; (w = slash_na_list(i)) != 0; i++) {
            cl_cat(m, " ", sizeof(m));
            cl_cat(m, w, sizeof(m));
        }
        ui_line(&r->ui, m);
    }
}

int repl_pick(cl_repl *r, const char *title, const char *const *opt, int n, const char *cur)
{
    int i, sel = 0;
    for (i = 0; i < n; i++)
        if (!strcmp(opt[i], cur))
            sel = i;
    return ui_pick(&r->ui, title, opt, n, sel);
}

/* one /context line: what, ~tokens, the share of the window */
static void part_line(cl_repl *r, const char *what, long bytes)
{
    char m[200], num[16];
    long tok = (bytes + 3) / 4, w = repl_window(r->model);
    cl_copy(m, "  ", sizeof(m));
    cl_cat(m, what, sizeof(m));
    while ((long)strlen(m) < 22)
        cl_cat(m, " ", sizeof(m));
    cl_ltoa(tok, num);
    cl_cat(m, "~", sizeof(m));
    cl_cat(m, num, sizeof(m));
    cl_cat(m, " tokens (", sizeof(m));
    cl_ltoa(tok * 1000 / w / 10, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, ".", sizeof(m));
    cl_ltoa(tok * 1000 / w % 10, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, "%)", sizeof(m));
    ui_line(&r->ui, m);
}

/* /context's categories, estimated from the text sizes (4 bytes a token) */
static void context_parts(cl_repl *r)
{
    const char *tj = tools_json(&r->tools, r->model);
    const cl_def *st = r->style[0] ? defs_find(&r->defs, DEF_STYLE, r->style) : 0;
    long sys = r->system ? (long)strlen(r->system) : 0, mem = r->mem.text.n, sty = st ? (long)strlen(st->body) : 0;
    long msgs = 0, skills = 0;
    int i;
    const cl_def *d;
    for (i = 0; i < r->conv.n; i++)
        msgs += r->conv.m[i].n;
    for (i = 0; (d = defs_nth(&r->defs, DEF_SKILL, i)) != 0; i++)
        skills += (long)strlen(d->name) + (long)strlen(d->description) + 4;
    ui_line(&r->ui, "By category (estimated):");
    part_line(r, "System prompt", sys - mem - sty > 0 ? sys - mem - sty : 0);
    part_line(r, "Memory files", mem);
    if (sty)
        part_line(r, "Output style", sty);
    part_line(r, "Tools", tj ? (long)strlen(tj) - skills : 0);
    part_line(r, "Skills (listed)", skills);
    part_line(r, "Messages", msgs);
    if (mem > 40000L)
        ui_line(&r->ui, "  The memory files are large: /memory shows them; claudeMdExcludes leaves some out.");
}

/* availableModels: is this model one of them (by alias or id)? */
static int model_allowed(const cl_repl *r, const char *m)
{
    const char *p = r->cfg.avail_models;
    if (!*p)
        return 1;
    while (*p) {
        char one[64];
        int k = 0;
        while (*p == ',' || *p == ' ')
            p++;
        while (*p && *p != ',' && k < (int)sizeof(one) - 1)
            one[k++] = *p++;
        one[k] = 0;
        if (k && (cl_strieq(one, m) || !strcmp(cfg_model(one), cfg_model(m))))
            return 1;
    }
    return 0;
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
    r->idle_from = 0;
    settings_changed(r);            /* ConfigChange: a settings file edited meanwhile */
    if (r->await_key) {
        /* the line after /login is the key, never shown or kept */
        r->await_key = 0;
        slash_run(r, "/login", line);
        if (r->first && !repl_need_key(r))
            (void)start(r);             /* the prompt given at the start goes now */
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
    if (is_cmd(word, "/exit") || is_cmd(word, "/quit")) {
        r->end_reason = "prompt_input_exit";    /* SessionEnd's reason (Claude Code's values) */
        return 1;
    }
    if (is_cmd(word, "/help"))
        show_help(r);
    else if (is_cmd(word, "/model")) {
        const char *chosen = 0;
        if (*arg)
            chosen = arg;
        else {
            int c = repl_pick(r, "Select a model", models, (int)(sizeof(models) / sizeof(models[0])), r->model);
            if (c >= 0)
                chosen = models[c];
        }
        if (chosen && !model_allowed(r, chosen)) {
            show_err(r, "Not in availableModels (the settings): ", chosen);
            chosen = 0;
        }
        if (chosen && pol_model_switch(r, r->model, cfg_model(chosen), chosen, 0))
            chosen = 0;             /* a PreModelSwitch hook said no */
        if (chosen) {
            /* Claude Code: kept as the default for new sessions (the user's settings) */
            jw v;
            char from[64];
            cl_copy(from, r->model, sizeof(from));
            cl_copy(r->model, cfg_model(chosen), sizeof(r->model));
            pol_model_switch(r, from, r->model, chosen, 1);
            jw_init(&v);
            jw_strz(&v, chosen);
            if (r->sys->mkdir)
                r->sys->mkdir(r->sys->u, r->home);
            if (!v.oom)
                cfg_write_key(r->sys, cfg_file(&r->cfg, CFG_USER), "model", v.p);
            jw_free(&v);
        }
        ctx_show(r);
        show_err(r, "Model: ", r->model);
    } else if (is_cmd(word, "/effort")) {
        if (!strcmp(arg, "status")) {
            show_err(r, "Effort: ", r->effort);
            return 0;
        }
        if (*arg) {
            if (strcmp(arg, "low") && strcmp(arg, "medium") && strcmp(arg, "high") && strcmp(arg, "xhigh") &&
                strcmp(arg, "max") && strcmp(arg, "auto")) {
                ui_line(&r->ui, "The effort is one of: low, medium, high, xhigh, max, auto (the model's own); "
                                "/effort status shows it.");
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
        if (*arg && r->conv.n) {
            sess_save(&r->sess, &r->conv);
            sess_rename(&r->sess, arg);     /* Claude Code: a name for the previous conversation */
        }
        conv_clear(&r->conv);
        conv_usage_reset(&r->conv);         /* a new session's totals */
        r->cp.keep = r->sess.started && !r->sess.off;
        r->api_ms = 0;
        r->t_start = r->io->ms ? r->io->ms(r->io->u) : 0;
        sess_new(&r->sess, r->io->ms ? r->io->ms(r->io->u) : 0);
        cp_session(&r->cp, r->sess.file);
        cp_turn(&r->cp, 0);
        r->ctx_used = 0;
        ctx_show(r);
        if (r->tui)
            tui_clear(r->tui);
        ui_line(&r->ui, "A new conversation.");
        pol_session(r, HK_SESSION_START, "clear");
    } else if (is_cmd(word, "/compact"))
        repl_compact(r, arg, 0);
    else if (is_cmd(word, "/context")) {
        repl_context(r);
        context_parts(r);           /* Claude Code's breakdown by category */
    }
    else if (is_cmd(word, "/init"))
        turn(r, init_prompt, (long)sizeof(init_prompt) - 1);
    else if (is_cmd(word, "/resume"))
        resume(r, arg);
    else if (is_cmd(word, "/save"))
        save(r, arg);
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
        show_welcome(r->show, r->model, env_on(r, "CLAUDE_CODE_HIDE_CWD") ? "" : r->tools.root);
        if (start(r)) {
            free(line);
            return;
        }
        for (;;) {
            long n = tui_read(r->tui, line, 8192);
            int woke = r->woke;
            if (n < 0)
                break;
            r->woke = 0;
            /* not echoed: a scheduled turn (sched_tui_wake announced it), a
             * key's command (Alt+P) */
            if (!r->await_key && !woke && !r->tui->keycmd)
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
    if (start(r)) {
        free(line);
        return;
    }
    for (;;) {
        long n;
        if (!r->await_key)
            sched_line_mode(r);     /* due cron jobs, background news: before the wait for a line */
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
    t->perm = &r->tools.perm;
    t->think_off = &r->think_off;   /* Alt+T */
    t->cmds = r->menu;
    t->ncmds = r->nmenu;
    t->status = r->status_text;     /* the statusLine command's row(s) */
    t->status_pad = r->cfg.status_pad;
    t->hide_vim = r->cfg.hide_vim;
    t->idle = pol_status_tick;
    t->iu = r;
    t->wake = sched_tui_wake;       /* A4 gaps 2: a scheduled turn while the screen waits */
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
    if (env_on(r, "CLAUDE_CODE_SKIP_PROMPT_HISTORY"))
        r->ui.histfile[0] = 0;      /* Claude Code: no prompt history on disk */
    ui_attach(&r->ui, r->sys, r->tools.root, &r->conv);    /* A4: history, @, rewind, settings */
    r->tools.wait = tool_wait;      /* Bash and ! lines: Esc, Ctrl+B while they run */
    r->ui.tools = &r->tools;
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
    for (i = slash_nbuiltin; i < r->nmenu; i++) {
        free((char *)r->menu[i].name);
        free((char *)r->menu[i].help);
    }
    free(r->menu);
    r->menu = 0;
    r->nmenu = 0;
}

/* one custom entry: "/name", its description and argument hint */
static int menu_add(cl_repl *r, int k, const cl_def *d, const char *none)
{
    char *nm = (char *)malloc(strlen(d->name) + 2);
    jw h;
    if (!nm)
        return -1;
    nm[0] = '/';
    strcpy(nm + 1, d->name);
    jw_init(&h);
    jw_rawz(&h, d->description[0] ? d->description : none);
    if (d->hint && d->hint[0]) {
        jw_rawz(&h, "  ");
        jw_rawz(&h, d->hint);      /* argument-hint */
    }
    if (h.oom || !h.p) {
        free(nm);
        jw_free(&h);
        return -1;
    }
    r->menu[k].name = nm;
    r->menu[k].help = h.p;
    return 0;
}

/* the slash menu: the built-in commands, then the custom ones, then the
 * skills a user may type (Claude Code: a skill is a command too) */
static int menu_build(cl_repl *r)
{
    int nc = defs_count(&r->defs, DEF_COMMAND), ns = defs_count(&r->defs, DEF_SKILL), i, k;
    const cl_def *d;
    menu_free(r);
    r->menu = (cl_cmd *)malloc((size_t)(slash_nbuiltin + nc + ns) * sizeof(cl_cmd));
    if (!r->menu)
        return -1;
    for (i = 0; i < slash_nbuiltin; i++)
        r->menu[i] = slash_builtin[i];
    k = slash_nbuiltin;
    for (i = 0; (d = defs_nth(&r->defs, DEF_COMMAND, i)) != 0; i++)
        if (menu_add(r, k, d, d->src == CFG_PROJECT ? "(a project command)" : "(a user command)") == 0)
            k++;
    for (i = 0; (d = defs_nth(&r->defs, DEF_SKILL, i)) != 0; i++)
        if (!d->no_user && !defs_find(&r->defs, DEF_COMMAND, d->name) &&
            strcmp(cfg_skill_state(&r->cfg, d->name), "off") &&     /* skillOverrides off: not in the menu */
            menu_add(r, k, d, "(a skill)") == 0)
            k++;
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

/* ---- --system-prompt-snapshot (A4 gaps 2): the system prompt flags'
 * text recorded on a conversation's first request (<session>.sys beside
 * its file) and used for it until a compaction -- a --continue or
 * --resume with other flags changes nothing before then (Claude Code);
 * "off" takes the flags of each launch; --bare records only with "on" ---- */

static int snap_on(const cl_repl *r)
{
    return r->snapshot == 1 || (r->snapshot < 0 && !r->bare);
}

static int snap_file(cl_repl *r, char *out, long cap)
{
    char name[64];
    cl_copy(name, r->sess.id, 9);
    cl_cat(name, ".sys", sizeof(name));
    return r->sess.dir[0] ? path_join(r->sess.dir, name, out, cap) : -1;
}

/* the first request of a conversation: the flags' text recorded */
static void snap_record(cl_repl *r)
{
    char f[400];
    jw w;
    free(r->sys_snap);
    r->sys_snap = 0;
    if (!snap_on(r))
        return;
    jw_init(&w);
    jw_rawz(&w, "{\"replace\":");
    if (r->sys_replace)
        jw_strz(&w, r->sys_replace);
    else
        jw_rawz(&w, "null");
    jw_rawz(&w, ",\"append\":");
    if (r->sys_append)
        jw_strz(&w, r->sys_append);
    else
        jw_rawz(&w, "null");
    jw_raw(&w, "}", 1);
    if (!w.oom) {
        r->sys_snap = w.p;
        w.p = 0;
        if (!r->sess.off && snap_file(r, f, sizeof(f)) == 0) {
            if (r->sys->mkdir)
                r->sys->mkdir(r->sys->u, r->sess.dir);
            r->sys->write(r->sys->u, f, r->sys_snap, (long)strlen(r->sys_snap));
        }
    }
    jw_free(&w);
}

/* a resumed conversation: its recorded flags' text, when it has one */
static void snap_load(cl_repl *r)
{
    char f[400];
    char *b = 0;
    long n = 0;
    jv v;
    free(r->sys_snap);
    r->sys_snap = 0;
    if (!snap_on(r) || snap_file(r, f, sizeof(f)) || r->sys->kind(r->sys->u, f) != 1 ||
        r->sys->read(r->sys->u, f, 256L * 1024, &b, &n))
        return;
    if (json_parse(b, n, &v) == 0 && json_type(v) == J_OBJ)
        r->sys_snap = b;
    else
        free(b);
    repl_system(r);
}

/* the flags' text in force: the recorded one, else this launch's */
static char *snap_text(cl_repl *r, const char *key, char *flag)
{
    jv v, x;
    if (!r->sys_snap || json_parse(r->sys_snap, (long)strlen(r->sys_snap), &v) || !json_get(v, key, &x))
        return flag;
    if (json_type(x) != J_STR)
        return 0;
    {
        long l;
        char *s = json_strdup(x, &l);
        free(key[0] == 'r' ? r->snap_rep : r->snap_app);
        if (key[0] == 'r')
            r->snap_rep = s;
        else
            r->snap_app = s;
        return s;
    }
}

int repl_system(cl_repl *r)
{
    jw s;
    const cl_def *st = r->style[0] ? defs_find(&r->defs, DEF_STYLE, r->style) : 0;
    char *sys_replace = snap_text(r, "replace", r->sys_replace), *sys_append = snap_text(r, "append", r->sys_append);
    jw_init(&s);
    if (sys_replace)
        jw_rawz(&s, sys_replace);       /* --system-prompt (A4 WP4) */
    else {
        jw_rawz(&s, sys_a);
        jw_rawz(&s, sys_b);
        jw_rawz(&s, r->tools.root);
        jw_rawz(&s, sys_c);
        /* a custom output style without keep-coding-instructions: the
         * machine's facts stay, the way of working goes (Claude Code) */
        if (!st || st->src == DEF_BUILTIN || st->keep_coding)
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
    if (r->mem.auto_dir[0] && !r->no_dynamic) {
        jw_rawz(&s, auto_a);
        jw_rawz(&s, r->mem.auto_dir);
        jw_rawz(&s, auto_b);
    }
    if (sys_append) {
        jw_rawz(&s, "\n\n");
        jw_rawz(&s, sys_append);        /* --append-system-prompt */
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

/* auto memory on? (autoMemoryEnabled, CLAUDE_CODE_DISABLE_AUTO_MEMORY; on by default) */
static int auto_memory(cl_repl *r)
{
    char v[8];
    if (r->bare || r->safe || r->cfg.auto_memory == 0)
        return 0;
    if (r->sys->getenv && r->sys->getenv(r->sys->u, "CLAUDE_CODE_DISABLE_AUTO_MEMORY", v, sizeof(v)) > 0 &&
        strcmp(v, "0"))
        return 0;
    return 1;
}

int repl_load_memory(cl_repl *r)
{
    mem_free(&r->mem);
    r->mem.excl = r->cfg.md_excludes;           /* claudeMdExcludes */
    r->mem.nexcl = r->cfg.nmdx;
    {
        /* external imports: approved (1), declined (2) or not asked (0) for this project */
        int k = trust_get(r->sys, r->home, r->tools.root, TRUST_IMPORTS, 0);
        r->mem.ext_ok = k == 1 ? 1 : k == 0 ? 2 : 0;
    }
    if (env_on(r, "CLAUDE_CODE_DISABLE_CLAUDE_MDS"))
        return repl_system(r);      /* Claude Code: no memory file at all, auto memory included */
    if (!r->bare && !r->safe)       /* --bare / --safe-mode: no CLAUDE.md */
        mem_load(&r->mem, r->sys, r->home, r->tools.root);
    r->tools.auto_memory = auto_memory(r);      /* an agent's memory: needs auto memory on */
    if (auto_memory(r)) {
        /* Claude Code's auto memory: <home>/projects/<project>/memory/ */
        char d[340];
        if (path_join(r->sess.dir, "memory", d, sizeof(d)) == 0)
            mem_auto(&r->mem, r->sys, d);
    }
    return repl_system(r);
}

/* the definitions (commands, agents, skills, styles) with the command
 * line's say: --bare / --safe-mode none of the files', --disable-slash-
 * commands no commands or skills, --agents first */
static void repl_defs(cl_repl *r)
{
    defs_free(&r->defs);
    defs_load(&r->defs, r->sys, r->home, r->tools.root);
    if (r->bare || r->safe)
        defs_drop(&r->defs, -1);    /* only the built-in output styles stay */
    if (env_on(r, "CLAUDE_CODE_DISABLE_BUNDLED_SKILLS")) {
        /* Claude Code: the bundled skills removed, the user's and the project's kept */
        int i, k = 0;
        for (i = 0; i < r->defs.n; i++) {
            if (r->defs.d[i].type == DEF_SKILL && r->defs.d[i].src == DEF_BUILTIN) {
                def_free(&r->defs.d[i]);
                continue;
            }
            r->defs.d[k++] = r->defs.d[i];
        }
        r->defs.n = k;
    }
    if (r->no_slash) {
        defs_drop(&r->defs, DEF_COMMAND);
        defs_drop(&r->defs, DEF_SKILL);
    }
    if (r->agents_json) {
        char err[200];
        if (defs_add_agents_json(&r->defs, r->agents_json, err, sizeof(err)))
            repl_say(r, "", err);
    }
}

int repl_load_menu(cl_repl *r)
{
    return menu_build(r);
}

int repl_load_defs(cl_repl *r)
{
    r->n_nested = 0;
    repl_defs(r);
    return menu_build(r) || pol_tools(r) ? -1 : 0;
}

/* ---- the environment variables (A4 gaps 2): Claude Code's CLAUDE_CODE_*
 * that mean something on the Amiga, read in one place -- at each load, so
 * a settings file's "env" block counts too ---- */

static long env_num(cl_repl *r, const char *name, long def)
{
    char v[32];
    if (r->sys->getenv && r->sys->getenv(r->sys->u, name, v, sizeof(v)) > 0 && v[0] >= '0' && v[0] <= '9')
        return atol(v);
    return def;
}

/* a switch variable: set to something other than 0 / false / "" */
static int env_on(cl_repl *r, const char *name)
{
    char v[16];
    return r->sys->getenv && r->sys->getenv(r->sys->u, name, v, sizeof(v)) > 0 && strcmp(v, "0") &&
           !cl_strieq(v, "false") && !cl_strieq(v, "no");
}

/* the variable's text ("" unset) */
static void env_str(cl_repl *r, const char *name, char *out, long cap)
{
    if (!r->sys->getenv || r->sys->getenv(r->sys->u, name, out, cap) < 0)
        out[0] = 0;
}

/* The variables, Claude Code's meanings (env-vars.md). Read here, at each
 * load: BASH_DEFAULT_TIMEOUT_MS BASH_MAX_TIMEOUT_MS
 * CLAUDE_CODE_MAX_SUBAGENT_SPAWN_DEPTH CLAUDE_CODE_DISABLE_BACKGROUND_TASKS
 * CLAUDE_CODE_DISABLE_CRON CLAUDE_CODE_MAX_WEB_SEARCHES_PER_SESSION
 * CLAUDE_CODE_MAX_OUTPUT_TOKENS CLAUDE_CODE_MAX_RETRIES
 * CLAUDE_CODE_SUBAGENT_MODEL(_FORCE) CLAUDE_CODE_DISABLE_FILE_CHECKPOINTING
 * CLAUDE_CODE_DISABLE_WEB_FETCH CLAUDE_CODE_WEBFETCH_CACHE_TTL_MS
 * CLAUDE_CODE_WEBFETCH_DEADLINE_MS CLAUDE_BASH_MAINTAIN_PROJECT_WORKING_DIR
 * CLAUDE_CODE_FILE_READ_MAX_OUTPUT_TOKENS CLAUDE_CODE_MAX_CONTEXT_TOKENS
 * CLAUDE_CODE_DISABLE_1M_CONTEXT CLAUDE_CODE_DISABLE_THINKING
 * CLAUDE_CODE_EXTRA_BODY CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC
 * CLAUDE_CODE_DISABLE_EXPLORE_PLAN_AGENTS DISABLE_AUTO_COMPACT
 * DISABLE_COMPACT CLAUDE_AUTOCOMPACT_PCT_OVERRIDE CLAUDE_CODE_MAX_TURNS
 * CLAUDE_AFK_TIMEOUT_MS CLAUDE_AFK_COUNTDOWN_MS.
 * Read where they are used: CLAUDE_CODE_EFFORT_LEVEL (repl_effort),
 * CLAUDE_CODE_ENABLE_TASKS / _TODO_TOOLS (repl_todo_mode),
 * CLAUDE_CODE_DISABLE_ADVISOR_TOOL, CLAUDE_CODE_STOP_HOOK_BLOCK_CAP,
 * CLAUDE_CODE_AUTO_COMPACT_WINDOW, CLAUDE_CODE_DISABLE_AUTO_MEMORY,
 * CLAUDE_CODE_DISABLE_CLAUDE_MDS, CLAUDE_CODE_DISABLE_BUNDLED_SKILLS,
 * CLAUDE_CODE_SIMPLE, CLAUDE_CODE_SAFE_MODE, CLAUDE_CODE_SKIP_PROMPT_HISTORY,
 * CLAUDE_CODE_PROJECT_DIR_NAME, CLAUDE_CODE_HIDE_CWD,
 * CLAUDE_CODE_FORWARD_SUBAGENT_TEXT, CLAUDE_CODE_RESUME_PROMPT,
 * CLAUDE_CODE_DEBUG_LOGS_DIR (main), CLAUDE_CODE_TMPDIR, CLAUDE_CONFIG_DIR.
 * Set for the commands C:Claude runs: CLAUDECODE=1, CLAUDE_CODE_SESSION_ID. */
static void repl_env(cl_repl *r)
{
    long v;
    char s[64];
    /* BASH_DEFAULT_TIMEOUT_MS, BASH_MAX_TIMEOUT_MS: Bash's limits */
    v = env_num(r, "BASH_DEFAULT_TIMEOUT_MS", 0);
    r->tools.timeout_s = v > 0 ? (int)((v + 999) / 1000) : 120;
    r->tools.max_timeout_ms = env_num(r, "BASH_MAX_TIMEOUT_MS", 0);
    /* nested subagents: layers below the conversation (1 turns nesting off) */
    r->tools.max_depth = (int)env_num(r, "CLAUDE_CODE_MAX_SUBAGENT_SPAWN_DEPTH", 0);
    /* no background tasks (--bare too): a command stops at its time limit */
    r->tools.no_background = r->bare || env_on(r, "CLAUDE_CODE_DISABLE_BACKGROUND_TASKS");
    r->tools.no_cron = env_on(r, "CLAUDE_CODE_DISABLE_CRON");
    r->tools.max_searches = env_num(r, "CLAUDE_CODE_MAX_WEB_SEARCHES_PER_SESSION", 0);
    /* the requests: output tokens (capped at 128000), retries (at most 15) */
    v = env_num(r, "CLAUDE_CODE_MAX_OUTPUT_TOKENS", 0);
    r->max_tokens = v > 0 ? (v > 128000L ? 128000L : v) : CL_MAX_TOKENS;
    v = env_num(r, "CLAUDE_CODE_MAX_RETRIES", -1);
    r->tries = v >= 0 ? (int)(v > 15 ? 15 : v) + 1 : CL_TRIES;
    /* subagents' model: under the call's and the definition's, unless forced */
    env_str(r, "CLAUDE_CODE_SUBAGENT_MODEL", s, sizeof(s));
    cl_copy(r->tools.sub_model, s[0] ? cfg_model(s) : "", sizeof(r->tools.sub_model));
    r->tools.sub_force = s[0] && env_on(r, "CLAUDE_CODE_SUBAGENT_MODEL_FORCE");
    r->no_checkpoints = env_on(r, "CLAUDE_CODE_DISABLE_FILE_CHECKPOINTING");
    r->tools.no_fetch = env_on(r, "CLAUDE_CODE_DISABLE_WEB_FETCH");
    r->tools.fetch_ttl_ms = env_num(r, "CLAUDE_CODE_WEBFETCH_CACHE_TTL_MS", 0);
    r->tools.fetch_deadline_ms = env_num(r, "CLAUDE_CODE_WEBFETCH_DEADLINE_MS", 300000L);
    r->tools.no_cd_keep = env_on(r, "CLAUDE_BASH_MAINTAIN_PROJECT_WORKING_DIR");
    if (r->tools.no_cd_keep)
        r->tools.cwd[0] = 0;
    v = env_num(r, "CLAUDE_CODE_FILE_READ_MAX_OUTPUT_TOKENS", 0);
    r->tools.read_max = v > 0 ? v * 4 : 0;      /* tokens, about four characters each */
    r->max_ctx = env_num(r, "CLAUDE_CODE_MAX_CONTEXT_TOKENS", 0);
    r->no_1m = env_on(r, "CLAUDE_CODE_DISABLE_1M_CONTEXT");
    win_override = r->max_ctx;
    win_no1m = r->no_1m;
    r->no_thinking = env_on(r, "CLAUDE_CODE_DISABLE_THINKING");
    /* A4 gaps 3: an unanswered AskUserQuestion goes on without the user after
     * askUserQuestionTimeout; CLAUDE_AFK_TIMEOUT_MS wins (0: at once), the
     * countdown shows for the last CLAUDE_AFK_COUNTDOWN_MS (20 s) of it */
    v = env_num(r, "CLAUDE_AFK_TIMEOUT_MS", -1);
    r->ui.afk_ms = v >= 0 ? v : r->cfg.ask_timeout_ms > 0 ? r->cfg.ask_timeout_ms : -1;
    v = env_num(r, "CLAUDE_AFK_COUNTDOWN_MS", 20000L);
    r->ui.afk_count_ms = r->ui.afk_ms >= 0 && v > r->ui.afk_ms ? r->ui.afk_ms : v;
    free(r->extra_body);
    r->extra_body = 0;
    {
        char b[2048];
        jv o;
        env_str(r, "CLAUDE_CODE_EXTRA_BODY", b, sizeof(b));
        if (b[0] && json_parse(b, (long)strlen(b), &o) == 0 && json_type(o) == J_OBJ) {
            r->extra_body = (char *)malloc(strlen(b) + 1);
            if (r->extra_body)
                strcpy(r->extra_body, b);
        }
    }
    /* Claude Code: Monitor is not there without nonessential traffic */
    r->tools.no_monitor = env_on(r, "CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC") || env_on(r, "DISABLE_TELEMETRY");
    r->tools.no_explore_plan = env_on(r, "CLAUDE_CODE_DISABLE_EXPLORE_PLAN_AGENTS");
    if (env_on(r, "DISABLE_AUTO_COMPACT") || env_on(r, "DISABLE_COMPACT"))
        r->auto_compact = 0;
    r->no_compact = env_on(r, "DISABLE_COMPACT");
    v = env_num(r, "CLAUDE_AUTOCOMPACT_PCT_OVERRIDE", 0);
    r->compact_pct = v > 0 && v < CL_COMPACT_PCT ? (int)v : CL_COMPACT_PCT;    /* it can lower, not raise */
    if (r->sys->setenv) {
        /* the commands C:Claude runs can tell (Claude Code's) */
        r->sys->setenv(r->sys->u, "CLAUDECODE", "1");
        r->sys->setenv(r->sys->u, "CLAUDE_CODE_SESSION_ID", r->sess.id);
    }
}

int repl_load(cl_repl *r)
{
    int i;
    char env[64];
    cfg_free(&r->cfg);
    r->cfg.skip = r->sources ? ~r->sources & 7u : 0;    /* --setting-sources */
    r->cfg.untrusted = r->untrusted;    /* workspace trust: the project's allow rules wait for it */
    r->hooks.held = r->untrusted && !r->no_person;      /* interactive: hooks wait for it too */
    cfg_load(&r->cfg, r->sys, r->home, r->tools.root);
    for (i = 0; i < 2; i++)
        if (r->layer[i])            /* the command line's layer (A4 WP4): after the files, wins */
            cfg_merge(&r->cfg, CFG_SESSION, r->layer[i], (long)strlen(r->layer[i]),
                      i ? "the command line" : "--settings");
    if (r->bare || r->safe || r->cfg.no_hooks)
        cfg_drop_hooks(&r->cfg);    /* --bare, --safe-mode, disableAllHooks */
    if (r->safe)
        r->cfg.status_cmd[0] = 0;
    repl_defs(r);
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
    if (r->cfg.output_style[0]) {
        /* Claude Code: the setting is case-sensitive; a name that is no style's is Default */
        const cl_def *os = defs_find(&r->defs, DEF_STYLE, r->cfg.output_style);
        cl_copy(r->style, os && !strcmp(os->name, r->cfg.output_style) && strcmp(os->name, "Default") ? os->name : "",
                sizeof(r->style));
    }
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
    else if (!strcmp(r->cfg.default_mode, "dontAsk")) {
        r->tools.perm.mode = PERM_DEFAULT;
        r->ask_policy = ASKP_DENY;
    } else if (!strcmp(r->cfg.default_mode, "bypassPermissions"))
        r->tools.perm.mode = PERM_BYPASS;
    /* Shift+Tab reaches bypass when the session may use it (Claude Code:
     * --dangerously-skip-permissions or --allow-dangerously-skip-permissions) */
    r->tools.perm.can_bypass = r->allow_bypass || r->tools.perm.mode == PERM_BYPASS;
    for (i = 0; i < r->cfg.nenv; i++)
        if (r->sys->setenv)
            r->sys->setenv(r->sys->u, r->cfg.env[i].k, r->cfg.env[i].v);
    if (r->cfg.key_helper[0] && r->url.tls) {
        /* apiKeyHelper: its output is the key (it wins over a stored one) */
        char *o = (char *)malloc(1024);
        long on = 0, rc = 0;
        if (o && r->sys->run(r->sys->u, r->cfg.key_helper, 30, o, 1023, &on, &rc) == 0 && rc == 0) {
            o[on] = 0;
            cl_copy(r->keybuf, o, sizeof(r->keybuf));
            if (cl_key_clean(r->keybuf) == 0)
                r->key = r->keybuf;
            else
                repl_say(r, "apiKeyHelper gave no usable key: ", r->cfg.key_helper);
        } else
            repl_say(r, "apiKeyHelper did not run: ", r->cfg.key_helper);
        if (o)
            memset(o, 0, 1024);
        free(o);
    }
    {
        /* Claude Code: bashOutputMaxChars sizes the inline ceiling and the
         * read-back window together (to 128000) and BASH_MAX_OUTPUT_LENGTH is
         * then ignored; alone, the variable sets the window (to 150000) */
        char v[24];
        r->tools.out_max = r->tools.out_inline = 0;
        if (r->cfg.bash_max_chars > 0)
            r->tools.out_max = r->tools.out_inline = r->cfg.bash_max_chars > 128000L ? 128000L : r->cfg.bash_max_chars;
        else if (r->sys->getenv && r->sys->getenv(r->sys->u, "BASH_MAX_OUTPUT_LENGTH", v, sizeof(v)) > 0 &&
                 atol(v) > 0)
            r->tools.out_max = atol(v) > 150000L ? 150000L : atol(v);
    }
    repl_env(r);                    /* the CLAUDE_CODE_* variables (the env block of the settings included) */
    if (r->cfg.err[0])
        repl_say(r, "Settings: ", r->cfg.err);
    if (r->tui) {
        r->tui->status_pad = r->cfg.status_pad;
        r->tui->hide_vim = r->cfg.hide_vim;
    }
    if (menu_build(r) || pol_tools(r))
        return -1;
    cfg_times(r);
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
    jw_init(&r->md);
    jw_init(&r->perm_upd);
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
    r->snapshot = -1;               /* --system-prompt-snapshot: the default */
    r->tools.sys = sys;
    if (sys->canon(sys->u, root, r->tools.root, sizeof(r->tools.root)))
        cl_copy(r->tools.root, root, sizeof(r->tools.root));
    r->tools.timeout_s = 120;       /* Claude Code: 2 minutes (repl_env: BASH_DEFAULT_TIMEOUT_MS) */
    r->tools.u = r;
    r->tools.can_read = pol_can_read;
    r->tools.clock = tool_clock;
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
    r->tools.home = r->home;
    {
        char hd[256];
        var_or(sys, "HOME", "SYS:", hd, sizeof(hd));
        cfg_set_home(hd);           /* ~/ in permission rules */
    }
    var_or(sys, "CLAUDE_CODE_TMPDIR", CL_TMP, r->tmp, sizeof(r->tmp));
    sess_init(&r->sess, sys, r->home, r->tools.root);
    {
        /* CLAUDE_CODE_PROJECT_DIR_NAME (with CLAUDE_CONFIG_DIR): the projects/ directory's name */
        char pn[64], cd[8];
        env_str(r, "CLAUDE_CODE_PROJECT_DIR_NAME", pn, sizeof(pn));
        if (pn[0] && !strpbrk(pn, "/:") && sys->getenv && sys->getenv(sys->u, "CLAUDE_CONFIG_DIR", cd, sizeof(cd)) >= 0) {
            char d[300];
            if (path_join(r->home, "projects", d, sizeof(d)) == 0 && path_join(d, pn, r->sess.dir, sizeof(r->sess.dir)))
                cl_copy(r->sess.dir, d, sizeof(r->sess.dir));
        }
    }
    if (env_on(r, "CLAUDE_CODE_SKIP_PROMPT_HISTORY"))
        r->sess.off = 1;            /* no session file (and no history: repl_screen) */
    sess_new(&r->sess, io->ms ? io->ms(io->u) : 0);
    if (path_join(r->tmp, "Claude-cp", cpdir, sizeof(cpdir)))
        cl_copy(cpdir, "T:Claude-cp", sizeof(cpdir));
    cp_init(&r->cp, sys, cpdir);
    cp_session(&r->cp, r->sess.file);     /* this session's own directory of snapshots */
    checkpoint_use(&r->cp);
    r->hooks.cfg = &r->cfg;
    r->hooks.sys = sys;
    r->hooks.session_id = r->sess.id;
    r->hooks.transcript = r->sess.file;
    r->hooks.cwd = r->tools.root;
    r->hooks.tmp = r->tmp;
    r->hooks.project_dir = r->launch_root;      /* CLAUDE_PROJECT_DIR */
    pol_attach_ui(r);
    pol_attach_tools(r);
    pol_attach_hooks(r);
    if (repl_load(r))
        return -1;
    r->start_due = 1;
    r->began = 0;            /* SessionStart runs once the command line is applied (--bare, ...) */
    r->t_start = io->ms ? io->ms(io->u) : 0;
    cl_copy(r->launch_root, r->tools.root, sizeof(r->launch_root));
    return 0;
}

void repl_free(cl_repl *r)
{
    if (r->hooks.cfg)
        pol_session(r, HK_SESSION_END, r->end_reason ? r->end_reason : "other");
    if (r->tools.tasks)
        sched_save(r);
    hooks_async_stop(&r->hooks);    /* Claude Code: async hooks still running at the end are cancelled */
    watch_free(r);
    pol_hooks_free(r);
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
    r->cp.keep = r->sess.started && !r->sess.off;  /* a saved session keeps its snapshots */
    cp_free(&r->cp);
    menu_free(r);
    jw_free(&r->pending);
    jw_free(&r->md);
    jw_free(&r->perm_upd);
    free(r->defer_input);
    r->defer_input = 0;
    free(r->sys_snap);
    free(r->snap_rep);
    free(r->snap_app);
    r->sys_snap = r->snap_rep = r->snap_app = 0;
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
    free(r->first_msg);
    free(r->extra_body);
    r->extra_body = 0;
    r->agents_json = r->schema = r->structured = r->first_msg = 0;
}

int repl_need_key(const cl_repl *r)
{
    return r->url.tls && !(r->key && *r->key);
}

/* A4 WP4: the start -- /login first when there is no key, else the
 * prompt the session starts with */
/* SessionStart "startup", once, after the command line was applied;
 * InstructionsLoaded for the memory read at the start */
static void started(cl_repl *r)
{
    if (r->start_due) {
        r->start_due = 0;
        r->began = 1;
        if (r->cfg.cleanup_days > 0)
            sess_cleanup(&r->sess, r->tmp, (long)r->cfg.cleanup_days * 86400L);
        pol_session(r, HK_SESSION_START, "startup");
        pol_instructions(r, 0, "session_start");
        sched_load(r, 0);           /* the project's durable cron jobs */
        watch_start(r);             /* FileChanged: the watched files as they are now */
    }
}

/* the settings files' times, as read */
static void cfg_times(cl_repl *r)
{
    int i;
    for (i = 0; i < 3; i++)
        r->cfg_mtime[i] = r->sys->mtime && r->cfg.path[i][0] ? r->sys->mtime(r->sys->u, r->cfg.path[i]) : 0;
}

/* A settings file changed on disk since it was read: its ConfigChange
 * hooks, then the settings read again (unless a hook blocks it) */
static void settings_changed(cl_repl *r)
{
    int i;
    if (!r->sys->mtime)
        return;
    for (i = 0; i < 3; i++) {
        long t = r->cfg.path[i][0] ? r->sys->mtime(r->sys->u, r->cfg.path[i]) : 0;
        if (t == r->cfg_mtime[i])
            continue;
        r->cfg_mtime[i] = t;
        if (pol_config_change(r, i, r->cfg.path[i]) == 0) {
            repl_load(r);
            repl_say(r, "Settings changed on disk, read again: ", r->cfg.path[i]);
        }
        return;
    }
}

/* ---- workspace trust (A4 gaps 2): Claude Code's dialog, per directory ---- */

/* what the folder's own settings would bring in: allow rules, hooks, directories */
static void trust_what(cl_repl *r, char *out, long cap)
{
    cl_settings s;
    int i, n = 0, allow = 0;
    char num[16];
    cfg_init(&s);
    for (i = CFG_PROJECT; i <= CFG_LOCAL; i++) {
        char *b = 0;
        long bn = 0;
        if (r->cfg.path[i][0] && r->sys->kind(r->sys->u, r->cfg.path[i]) == 1 &&
            r->sys->read(r->sys->u, r->cfg.path[i], 256L * 1024, &b, &bn) == 0)
            cfg_merge(&s, i, b, bn, r->cfg.path[i]);
        free(b);
    }
    for (i = 0; i < s.nrules; i++)
        allow += s.rules[i].kind == RULE_ALLOW;
    out[0] = 0;
    if (allow) {
        cl_ltoa(allow, num);
        cl_cat(out, num, cap);
        cl_cat(out, allow == 1 ? " allow rule" : " allow rules", cap);
        n++;
    }
    if (s.nhooks) {
        cl_cat(out, n++ ? ", " : "", cap);
        cl_ltoa(s.nhooks, num);
        cl_cat(out, num, cap);
        cl_cat(out, s.nhooks == 1 ? " hook" : " hooks", cap);
    }
    if (s.ndirs) {
        cl_cat(out, n++ ? ", " : "", cap);
        cl_ltoa(s.ndirs, num);
        cl_cat(out, num, cap);
        cl_cat(out, s.ndirs == 1 ? " additional directory" : " additional directories", cap);
    }
    if (s.key_helper[0])
        cl_cat(out, n++ ? ", an apiKeyHelper command" : "an apiKeyHelper command", cap);
    cfg_free(&s);
}

/* Claude Code's dialog for external imports: a project's memory file
 * imports files outside the start directory; asked once per project (a
 * no is kept too), the memory read again on a yes */
void repl_ext_imports(cl_repl *r)
{
    static const char *const opt[] = { "Yes, allow external imports", "No, disable external imports" };
    int ans;
    ui_line(&r->ui, "This project's memory (CLAUDE.md) imports files from outside the start directory:");
    ui_line(&r->ui, r->mem.ext_list);
    ans = ui_pick(&r->ui, "Allow external CLAUDE.md file imports?", opt, 2, 1);
    if (ans < 0 && !r->tui && r->io->read_line) {
        char line[32];
        ui_puts(&r->ui, "Allow external CLAUDE.md file imports? (y/n) ");
        ans = r->io->read_line(r->io->u, line, sizeof(line)) >= 0 && (line[0] == 'y' || line[0] == 'Y') ? 0 : 1;
        r->ui.col0 = 1;
    }
    trust_set(r->sys, r->home, r->tools.root, TRUST_IMPORTS, ans == 0);
    trust_set(r->sys, r->home, r->tools.root, TRUST_IMPORTS_ASKED, 1);
    if (ans == 0)
        repl_load_memory(r);
}

int repl_trust(cl_repl *r)
{
    char home[256], m[400], what[200];
    int ans;
    if (r->trusted_dir || trust_get(r->sys, r->home, r->tools.root, TRUST_ACCEPTED, 1) == 1) {
        r->trusted_dir = 1;
        return 0;
    }
    /* not trusted: the project's allow rules and directories wait (print mode
     * too, where no one is asked); interactive, every settings file's hooks */
    r->untrusted = 1;
    repl_load(r);
    trust_what(r, what, sizeof(what));
    if (r->no_person) {
        /* print mode: never asked; the project's allow rules and directories
         * are not used (print.c says so on the error stream) */
        r->trust_warn = strstr(what, "allow rule") || strstr(what, "director");
        return 0;
    }
    ui_line(&r->ui, "Do you trust the files in this folder?");
    ui_line(&r->ui, r->tools.root);
    ui_line(&r->ui, "C:Claude may read, change and run files here. Hooks and the folder's own permission "
                    "settings take effect only once you trust it.");
    if (what[0]) {
        cl_copy(m, "This folder's settings would add: ", sizeof(m));
        cl_cat(m, what, sizeof(m));
        ui_line(&r->ui, m);
    }
    {
        static const char *const opt[] = { "Yes, proceed", "No, exit" };
        ans = ui_pick(&r->ui, "Workspace trust", opt, 2, 0);
        if (ans < 0 && !r->tui && r->io->read_line) {
            /* the line mode: y or n */
            char line[32];
            ui_puts(&r->ui, "Trust this folder? (y/n) ");
            ans = r->io->read_line(r->io->u, line, sizeof(line)) >= 0 && (line[0] == 'y' || line[0] == 'Y') ? 0 : 1;
            r->ui.col0 = 1;
        }
    }
    if (ans != 0)
        return -1;
    r->trusted_dir = 1;
    r->untrusted = 0;
    var_or(r->sys, "HOME", "SYS:", home, sizeof(home));
    if (!cl_strieq(home, r->tools.root))
        trust_set(r->sys, r->home, r->tools.root, TRUST_ACCEPTED, 1);  /* the home directory: this session only */
    repl_load(r);
    return 0;
}

void repl_setup(cl_repl *r, const char *trigger, int start)
{
    if (trigger)
        pol_setup(r, trigger);      /* Setup hooks: --init, --maintenance */
    if (start)
        started(r);                 /* --init-only: SessionStart too, then nothing */
}

static int start(cl_repl *r)
{
    if (repl_trust(r)) {
        ui_line(&r->ui, "The folder is not trusted: C:Claude ends here.");
        return -1;                  /* Claude Code: no to the trust question exits */
    }
    if (r->cfg.nwarn) {
        /* Claude Code's Settings Warning: the entries skipped, the rest in effect */
        ui_line(&r->ui, "Settings Warning (these entries were skipped; the rest of the files is in effect):");
        ui_line(&r->ui, r->cfg.warn);
    }
    if (r->mem.next && r->mem.ext_ok == 0)
        repl_ext_imports(r);
    started(r);
    if (repl_need_key(r))
        repl_line(r, "/login");
    else if (r->first) {
        const char *f = r->first;
        r->first = 0;
        ui_user(&r->ui, f);
        repl_line(r, f);
    }
    return 0;
}
