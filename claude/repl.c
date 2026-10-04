/* repl -- see repl.h. */
#include <stdlib.h>
#include <string.h>
#include "repl.h"
#include "path.h"
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
    "Markdown, 80 columns or fewer; keep answers concise.";

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
                if (r->io->brk(r->io->u)) {
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
            if (r->io->brk(r->io->u)) {
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
    ui_tool(&((cl_repl *)u)->ui, tool, what);
}

static int tool_ask(void *u, const char *tool, const char *what, int outside)
{
    return ui_ask(&((cl_repl *)u)->ui, tool, what, outside);
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
    for (round = 0; round < 64; round++) {
        int rc, ntools, i;
        const char *stop;
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
        if (r->io->brk(r->io->u)) {
            ui_line(&r->ui, "Stopped after the tool calls. Type a message to go on.");
            break;
        }
    }
    if (!answered)
        conv_rollback(&r->conv, m0);
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

static const char help[] =
    "Commands:\n"
    "  /model [NAME]     show or set the model\n"
    "  /effort [LEVEL]   show or set the effort: low, medium, high, xhigh, max\n"
    "  /clear            start a new conversation\n"
    "  /save FILE        save the conversation as JSON\n"
    "  /cost             tokens and cost so far\n"
    "  /exit             leave\n"
    "Ctrl+C stops an answer or a command. Anything else is sent to Claude.";

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
        ui_line(&r->ui, help);
    else if (!strncmp(line, "/model", 6)) {
        if (*arg)
            cl_copy(r->model, arg, sizeof(r->model));
        show_err(r, "Model: ", r->model);
    } else if (!strncmp(line, "/effort", 7)) {
        if (*arg) {
            if (strcmp(arg, "low") && strcmp(arg, "medium") && strcmp(arg, "high") && strcmp(arg, "xhigh") &&
                strcmp(arg, "max")) {
                ui_line(&r->ui, "The effort is one of: low, medium, high, xhigh, max.");
                return 0;
            }
            cl_copy(r->effort, arg, sizeof(r->effort));
        }
        show_err(r, "Effort: ", r->effort);
    } else if (!strncmp(line, "/clear", 6)) {
        conv_clear(&r->conv);
        ui_line(&r->ui, "A new conversation.");
    } else if (!strncmp(line, "/save", 5))
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
    r->key = key;
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
    jw_init(&s);
    jw_rawz(&s, sys_a);
    jw_rawz(&s, sys_b);
    jw_rawz(&s, r->tools.root);
    jw_rawz(&s, sys_c);
    jw_rawz(&s, sys_d);
    r->system = s.p;
    return s.oom ? -1 : 0;
}

void repl_free(cl_repl *r)
{
    drop(r);
    request_free(r);
    conv_free(&r->conv);
    free(r->system);
    r->system = 0;
}
