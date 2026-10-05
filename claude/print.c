/* print -- see print.h. */
#include <stdlib.h>
#include <string.h>
#include "print.h"
#include "repl_int.h"
#include "tui.h"
#include "path.h"
#include "util.h"

#define NOTES_MAX 4096L

typedef struct pst {
    cl_repl *r;
    cl_cli *c;
    cl_pout *p;
    cl_io *orig;                /* main's console */
    cl_io io;                   /* the REPL's while the run lasts */
    cl_feed feed;
    jw notes;                   /* the transcript's last lines (the error text of a failed turn) */
    jw text;                    /* the last answer's text */
    jw denials;                 /* permission_denials' items */
    jw line;                    /* one output line being built */
    unsigned long seed, t0, api0;
    long resp0;
    cl_conv base;               /* the usage when the run started (only the counters) */
    char sub_parent[64];        /* the Task call whose subagent's prompt went out last */
} pst;

/* ---- the REPL's console, wrapped: the transcript kept aside ---- */

static void w_write(void *u, const char *s, long n)
{
    pst *st = (pst *)u;
    if (n <= 0 || s[0] == '\r')
        return;                 /* the spinner's line */
    if (st->c->verbose && st->p->err)
        st->p->err(st->p->u, s, n);
    if (st->notes.n && st->notes.n + n > NOTES_MAX) {
        long keep = st->notes.n > NOTES_MAX / 2 ? NOTES_MAX / 2 : st->notes.n;
        memmove(st->notes.p, st->notes.p + st->notes.n - keep, (size_t)keep);
        st->notes.n = keep;
        st->notes.p[keep] = 0;
    }
    jw_raw(&st->notes, s, n > NOTES_MAX ? NOTES_MAX : n);
}

static long w_read_line(void *u, char *buf, long cap)
{
    (void)u;
    (void)buf;
    (void)cap;
    return -1;                  /* nobody types in print mode */
}

static int w_brk(void *u)
{
    cl_io *o = ((pst *)u)->orig;
    return o->brk ? o->brk(o->u) : 0;
}

static int w_sleep(void *u, long ms)
{
    cl_io *o = ((pst *)u)->orig;
    return o->sleep ? o->sleep(o->u, ms) : 0;
}

static unsigned long w_ms(void *u)
{
    cl_io *o = ((pst *)u)->orig;
    return o->ms ? o->ms(o->u) : 0;
}

static void w_log(void *u, const char *s, long n)
{
    cl_io *o = ((pst *)u)->orig;
    if (o->log)
        o->log(o->u, s, n);
}

/* ---- JSON bits ---- */

static void put_uuid(pst *st, jw *w)
{
    static const char hex[] = "0123456789abcdef";
    char u[37];
    int i, k = 0;
    for (i = 0; i < 32; i++) {
        unsigned d;
        if (!(i & 7))
            st->seed = st->seed * 1103515245UL + 12345UL;
        d = (unsigned)(st->seed >> (4 * (i & 7))) & 15;
        if (i == 12)
            d = 4;              /* version 4 */
        else if (i == 16)
            d = 8 | (d & 3);    /* the variant */
        if (i == 8 || i == 12 || i == 16 || i == 20)
            u[k++] = '-';
        u[k++] = hex[d];
    }
    u[k] = 0;
    jw_strz(w, u);
}

/* micro-dollars as a JSON number: 0.012345 */
static void put_usd(jw *w, unsigned long micro)
{
    char b[24], f[8];
    unsigned long frac = micro % 1000000UL;
    int i;
    cl_ltoa((long)(micro / 1000000UL), b);
    jw_rawz(w, b);
    if (!frac)
        return;
    for (i = 5; i >= 0; i--) {
        f[i] = (char)('0' + frac % 10);
        frac /= 10;
    }
    f[6] = 0;
    for (i = 5; i > 0 && f[i] == '0'; i--)
        f[i] = 0;
    jw_raw(w, ".", 1);
    jw_rawz(w, f);
}

static void key_long(jw *w, const char *key, long v, int comma)
{
    if (comma)
        jw_raw(w, ",", 1);
    jw_strz(w, key);
    jw_raw(w, ":", 1);
    jw_long(w, v);
}

static void tail(pst *st, jw *w)
{
    jw_rawz(w, ",\"session_id\":");
    jw_strz(w, st->r->sess.id);
    jw_rawz(w, ",\"uuid\":");
    put_uuid(st, w);
    jw_raw(w, "}", 1);
}

static void emit(pst *st, jw *w)
{
    if (w->oom)
        return;
    jw_raw(w, "\n", 1);
    st->p->out(st->p->u, w->p, w->n);
}

/* the text blocks of a content array, joined */
static void content_text(const char *json, long n, jw *out)
{
    jv v, b, x;
    jit it;
    jw_reset(out);
    if (json_parse(json, n, &v) || json_type(v) != J_ARR)
        return;
    json_iter(v, &it);
    while (json_next(&it, 0, &b)) {
        long l;
        char *t;
        if (!json_get(b, "type", &x) || !json_streq(x, "text") || !json_get(b, "text", &x))
            continue;
        t = json_strdup(x, &l);
        if (!t)
            continue;
        if (out->n)
            jw_rawz(out, "\n\n");
        jw_raw(out, t, l);
        free(t);
    }
}

/* ---- the feed: the turn's messages as they come ---- */

static void f_message(void *u, int user, const char *json, long n)
{
    pst *st = (pst *)u;
    cl_stream *s = &st->r->st;
    jw *w = &st->line;
    if (!user)
        content_text(json, n, &st->text);
    if (st->c->out != CLI_STREAM)
        return;
    jw_reset(w);
    if (user) {
        jw_rawz(w, "{\"type\":\"user\",\"message\":{\"role\":\"user\",\"content\":");
        jw_raw(w, json, n);
        jw_rawz(w, "},\"parent_tool_use_id\":null");
    } else {
        jw_rawz(w, "{\"type\":\"assistant\",\"message\":{\"id\":");
        jw_strz(w, s->id);
        jw_rawz(w, ",\"type\":\"message\",\"role\":\"assistant\",\"model\":");
        jw_strz(w, s->model[0] ? s->model : st->r->model);
        jw_rawz(w, ",\"content\":");
        jw_raw(w, json, n);
        jw_rawz(w, ",\"stop_reason\":");
        if (s->stop_reason[0])
            jw_strz(w, s->stop_reason);
        else
            jw_rawz(w, "null");
        jw_rawz(w, ",\"stop_sequence\":null,\"usage\":{");
        key_long(w, "input_tokens", s->in_tok, 0);
        key_long(w, "cache_creation_input_tokens", s->cache_w, 1);
        key_long(w, "cache_read_input_tokens", s->cache_r, 1);
        key_long(w, "output_tokens", s->out_tok, 1);
        jw_rawz(w, "}},\"parent_tool_use_id\":null");
    }
    tail(st, w);
    emit(st, w);
}

static void f_event(void *u, const char *ev, const char *data, long n)
{
    pst *st = (pst *)u;
    jw *w = &st->line;
    jv v;
    if (!st->c->partial || !strcmp(ev, "ping") || json_parse(data, n, &v))
        return;
    jw_reset(w);
    jw_rawz(w, "{\"type\":\"stream_event\",\"event\":");
    jw_raw(w, data, n);
    jw_rawz(w, ",\"parent_tool_use_id\":null");
    tail(st, w);
    emit(st, w);
}

static void f_denied(void *u, const char *tool, const char *id, const char *input, long n)
{
    pst *st = (pst *)u;
    jv v;
    if (st->denials.n)
        jw_raw(&st->denials, ",", 1);
    jw_rawz(&st->denials, "{\"tool_name\":");
    jw_strz(&st->denials, cfg_cc_tool(tool));
    jw_rawz(&st->denials, ",\"tool_use_id\":");
    jw_strz(&st->denials, id);
    jw_rawz(&st->denials, ",\"tool_input\":");
    if (input && n > 0 && json_parse(input, n, &v) == 0 && json_type(v) == J_OBJ)
        jw_raw(&st->denials, input, n);
    else
        jw_rawz(&st->denials, "{}");
    jw_raw(&st->denials, "}", 1);
}

/* a subagent's message (Claude Code: assistant and user messages with
 * parent_tool_use_id the Task call's id; the prompt that drives it first;
 * then by default only its tool_use and tool_result blocks, with
 * --forward-subagent-text its text and thinking too) */
static void f_sub(void *u, const char *parent, int user, const char *json, long n, const cl_stream *s)
{
    pst *st = (pst *)u;
    jw *w = &st->line, blocks;
    jv v, b, x;
    jit it;
    int first = 1, prompt = user && strcmp(st->sub_parent, parent) != 0;
    if (st->c->out != CLI_STREAM || json_parse(json, n, &v) || json_type(v) != J_ARR)
        return;
    if (prompt)
        cl_copy(st->sub_parent, parent, sizeof(st->sub_parent));
    jw_init(&blocks);
    jw_raw(&blocks, "[", 1);
    json_iter(v, &it);
    while (json_next(&it, 0, &b)) {
        if (!prompt && !st->c->fwd_sub && json_get(b, "type", &x) &&
            (json_streq(x, "text") || json_streq(x, "thinking") || json_streq(x, "redacted_thinking")))
            continue;
        if (!first)
            jw_raw(&blocks, ",", 1);
        first = 0;
        jw_raw(&blocks, b.p, b.n);
    }
    jw_raw(&blocks, "]", 1);
    if (first || blocks.oom) {
        jw_free(&blocks);
        return;                     /* nothing of it is forwarded */
    }
    jw_reset(w);
    if (user) {
        jw_rawz(w, "{\"type\":\"user\",\"message\":{\"role\":\"user\",\"content\":");
        jw_raw(w, blocks.p, blocks.n);
        jw_rawz(w, "},\"parent_tool_use_id\":");
    } else {
        jw_rawz(w, "{\"type\":\"assistant\",\"message\":{\"id\":");
        jw_strz(w, s && s->id[0] ? s->id : "");
        jw_rawz(w, ",\"type\":\"message\",\"role\":\"assistant\",\"model\":");
        jw_strz(w, s && s->model[0] ? s->model : st->r->model);
        jw_rawz(w, ",\"content\":");
        jw_raw(w, blocks.p, blocks.n);
        jw_rawz(w, ",\"stop_reason\":");
        if (s && s->stop_reason[0])
            jw_strz(w, s->stop_reason);
        else
            jw_rawz(w, "null");
        jw_rawz(w, ",\"stop_sequence\":null,\"usage\":{");
        key_long(w, "input_tokens", s ? s->in_tok : 0, 0);
        key_long(w, "cache_creation_input_tokens", s ? s->cache_w : 0, 1);
        key_long(w, "cache_read_input_tokens", s ? s->cache_r : 0, 1);
        key_long(w, "output_tokens", s ? s->out_tok : 0, 1);
        jw_rawz(w, "}},\"parent_tool_use_id\":");
    }
    jw_strz(w, parent);
    tail(st, w);
    emit(st, w);
    jw_free(&blocks);
}

/* system/api_retry (Claude Code's headless stream) */
static void f_retry(void *u, int attempt, int max, long delay_ms, int status, const char *error)
{
    pst *st = (pst *)u;
    jw *w = &st->line;
    if (st->c->out != CLI_STREAM)
        return;
    jw_reset(w);
    jw_rawz(w, "{\"type\":\"system\",\"subtype\":\"api_retry\"");
    key_long(w, "attempt", attempt, 1);
    key_long(w, "max_retries", max, 1);
    key_long(w, "retry_delay_ms", delay_ms, 1);
    jw_rawz(w, ",\"error_status\":");
    if (status)
        jw_long(w, status);
    else
        jw_rawz(w, "null");
    jw_rawz(w, ",\"error\":");
    jw_strz(w, error);
    tail(st, w);
    emit(st, w);
}

/* --include-hook-events: system/hook_started and system/hook_response */
static void f_hook(void *u, const char *event, const char *cmd, int done, long rc, const char *out, long n)
{
    pst *st = (pst *)u;
    jw *w = &st->line;
    if (st->c->out != CLI_STREAM || !st->c->hook_events)
        return;
    jw_reset(w);
    jw_rawz(w, done ? "{\"type\":\"system\",\"subtype\":\"hook_response\",\"hook_name\":"
                    : "{\"type\":\"system\",\"subtype\":\"hook_started\",\"hook_name\":");
    jw_strz(w, cmd);
    jw_rawz(w, ",\"hook_event\":");
    jw_strz(w, event);
    if (done) {
        jw_rawz(w, ",\"output\":");
        jw_str(w, out ? out : "", n);
        jw_rawz(w, ",\"stdout\":");
        jw_str(w, out ? out : "", n);
        jw_rawz(w, ",\"stderr\":\"\"");    /* AmigaDOS: one stream */
        key_long(w, "exit_code", rc, 1);
        jw_rawz(w, ",\"outcome\":");
        jw_strz(w, rc == 0 ? "success" : rc == 2 ? "blocked" : "error");
    }
    tail(st, w);
    emit(st, w);
}

static const char suggest_ask[] =
    "Predict what the user is most likely to type next in this conversation, as they would type it: one short "
    "prompt, no quotes, nothing else. If there is no likely next prompt, answer NONE.";

/* --prompt-suggestions: a prompt_suggestion after a turn */
static void suggestion(pst *st)
{
    cl_repl *r = st->r;
    jw a, *w = &st->line;
    long k;
    if (!st->c->suggestions || st->c->out != CLI_STREAM || r->turn_rc != TURN_OK || r->conv.n < 2)
        return;
    jw_init(&a);
    if (repl_side(r, 0, r->conv.n, suggest_ask, &a) == 0 && a.n && strncmp(a.p, "NONE", 4)) {
        for (k = 0; k < a.n; k++)
            if (a.p[k] == '\n')
                a.p[k] = ' ';
        jw_reset(w);
        jw_rawz(w, "{\"type\":\"prompt_suggestion\",\"suggestion\":");
        jw_str(w, a.p, a.n);
        tail(st, w);
        emit(st, w);
    }
    jw_free(&a);
}

/* ---- system/init ---- */

static const char *perm_mode(const cl_repl *r)
{
    if (r->ask_policy == ASKP_BYPASS)
        return "bypassPermissions";
    if (r->ask_policy == ASKP_DENY)
        return "dontAsk";
    return r->tools.perm.mode == PERM_ACCEPT ? "acceptEdits" : r->tools.perm.mode == PERM_PLAN ? "plan" : "default";
}

static void init_msg(pst *st)
{
    cl_repl *r = st->r;
    jw *w = &st->line;
    const char *tj = tools_json(&r->tools, r->model);
    jv tools, t, x;
    jit it;
    int i, first = 1;
    const cl_def *d;
    jw_reset(w);
    jw_rawz(w, "{\"type\":\"system\",\"subtype\":\"init\",\"cwd\":");
    jw_strz(w, r->tools.root);
    /* the tools as the request declares them (the one source of truth) */
    jw_rawz(w, ",\"tools\":[");
    if (tj && json_parse(tj, (long)strlen(tj), &tools) == 0 && json_type(tools) == J_ARR) {
        json_iter(tools, &it);
        while (json_next(&it, 0, &t)) {
            if (!json_get(t, "name", &x))
                continue;
            if (!first)
                jw_raw(w, ",", 1);
            first = 0;
            if (json_streq(x, "web_search"))
                jw_strz(w, "WebSearch");
            else
                jw_raw(w, x.p, x.n);
        }
    }
    jw_rawz(w, "],\"mcp_servers\":[],\"model\":");
    jw_strz(w, r->model);
    jw_rawz(w, ",\"permissionMode\":");
    jw_strz(w, perm_mode(r));
    jw_rawz(w, ",\"slash_commands\":[");
    for (i = 0; i < r->nmenu; i++) {
        if (i)
            jw_raw(w, ",", 1);
        jw_strz(w, r->menu[i].name[0] == '/' ? r->menu[i].name + 1 : r->menu[i].name);
    }
    jw_rawz(w, "],\"apiKeySource\":");
    jw_strz(w, st->c->key_source[0] ? st->c->key_source : r->key ? "user" : "none");
    jw_rawz(w, ",\"claude_code_version\":\"1.0.0\"");     /* C:Claude's own version, semver as the field's */
    jw_rawz(w, ",\"output_style\":");
    jw_strz(w, r->style[0] ? r->style : "default");
    jw_rawz(w, ",\"agents\":[");
    for (i = 0; (d = defs_nth(&r->defs, DEF_AGENT, i)) != 0; i++) {
        if (i)
            jw_raw(w, ",", 1);
        jw_strz(w, d->name);
    }
    jw_rawz(w, "],\"skills\":[");
    for (i = 0; (d = defs_nth(&r->defs, DEF_SKILL, i)) != 0; i++) {
        if (i)
            jw_raw(w, ",", 1);
        jw_strz(w, d->name);
    }
    jw_rawz(w, "],\"plugins\":[]");
    tail(st, w);
    emit(st, w);
}

/* ---- the result ---- */

/* the transcript's last line: a failed turn's reason */
static void last_note(pst *st, jw *out)
{
    long e = st->notes.n, b;
    jw_reset(out);
    if (!e)
        return;
    while (e > 0 && (st->notes.p[e - 1] == '\n' || st->notes.p[e - 1] == ' '))
        e--;
    for (b = e; b > 0 && st->notes.p[b - 1] != '\n'; b--)
        ;
    jw_raw(out, st->notes.p + b, e - b);
}

static void usage_obj(jw *w, long in, long cw, long cr, long out)
{
    jw_raw(w, "{", 1);
    key_long(w, "input_tokens", in, 0);
    key_long(w, "cache_creation_input_tokens", cw, 1);
    key_long(w, "cache_read_input_tokens", cr, 1);
    key_long(w, "output_tokens", out, 1);
    jw_rawz(w, ",\"service_tier\":\"standard\"}");
}

static void model_usage(pst *st, jw *w)
{
    const cl_conv *c = &st->r->conv;
    int i, k, first = 1;
    jw_raw(w, "{", 1);
    for (i = 0; i < c->nmu; i++) {
        const cl_model_use *u = &c->mu[i];
        cl_model_use b;
        memset(&b, 0, sizeof(b));
        for (k = 0; k < st->base.nmu; k++)
            if (!strcmp(st->base.mu[k].model, u->model))
                b = st->base.mu[k];
        if (u->in == b.in && u->out == b.out && u->cache_w == b.cache_w && u->cache_r == b.cache_r)
            continue;           /* not used in this run */
        if (!first)
            jw_raw(w, ",", 1);
        first = 0;
        jw_strz(w, u->model);
        jw_raw(w, ":{", 2);
        key_long(w, "inputTokens", u->in - b.in, 0);
        key_long(w, "outputTokens", u->out - b.out, 1);
        key_long(w, "cacheReadInputTokens", u->cache_r - b.cache_r, 1);
        key_long(w, "cacheCreationInputTokens", u->cache_w - b.cache_w, 1);
        key_long(w, "webSearchRequests", 0, 1);
        jw_rawz(w, ",\"costUSD\":");
        put_usd(w, u->cost_micro - b.cost_micro);
        key_long(w, "contextWindow", repl_window(u->model), 1);
        key_long(w, "maxOutputTokens", st->r->max_tokens, 1);
        jw_raw(w, "}", 1);
    }
    jw_raw(w, "}", 1);
}

/* One prompt answered and its result written: 0, or 10 when the result
 * is an error. */
#define SCHEMA_TRIES 3              /* reminders to call StructuredOutput before giving up */

static const char schema_nudge[] =
    "You have not called the StructuredOutput tool. Call it now with your final answer in the required format.";

static int answer(pst *st, const char *prompt, long pn, const char *blocks, long bn)
{
    cl_repl *r = st->r;
    unsigned long ms0 = w_ms(st), api0 = r->api_ms;
    long resp0 = r->n_responses;
    const char *sub = "success";
    int is_error = 0, rc, tries = 0;
    char err[160], num[16];
    jw res, *w = &st->line;
    char *z = (char *)malloc((size_t)pn + 1);
    jw_init(&res);
    jw_reset(&st->text);
    jw_reset(&st->notes);
    err[0] = 0;
    if (!z)
        return 10;
    memcpy(z, prompt, (size_t)pn);
    z[pn] = 0;
    r->turn_rc = TURN_OK;
    free(r->structured);
    r->structured = 0;
    if (blocks)
        repl_blocks(r, z, pn, blocks, bn);   /* images or documents: no command parsing */
    else
        repl_line(r, z);
    free(z);
    /* --json-schema: Claude is reminded until it calls StructuredOutput */
    while (r->schema && !r->structured && r->turn_rc == TURN_OK && tries < SCHEMA_TRIES) {
        tries++;
        repl_turn(r, schema_nudge, (long)sizeof(schema_nudge) - 1);
    }
    if (r->schema && !r->structured && r->turn_rc == TURN_OK) {
        sub = "error_max_structured_output_retries";
        is_error = 1;
        cl_copy(err, "Claude did not provide the structured output after ", sizeof(err));
        cl_ltoa(SCHEMA_TRIES, num);
        cl_cat(err, num, sizeof(err));
        cl_cat(err, " reminders", sizeof(err));
    }
    switch (r->turn_rc) {
    case TURN_MAX_TURNS:
        sub = "error_max_turns";
        is_error = 1;
        cl_copy(err, "Reached max turns (", sizeof(err));
        cl_ltoa(r->max_turns, num);
        cl_cat(err, num, sizeof(err));
        cl_cat(err, ")", sizeof(err));
        break;
    case TURN_BUDGET:
        sub = "error_max_budget_usd";
        is_error = 1;
        cl_copy(err, "Reached maximum budget ($", sizeof(err));
        {
            jw b;
            jw_init(&b);
            put_usd(&b, r->budget_micro);
            cl_cat(err, b.p ? b.p : "", sizeof(err));
            jw_free(&b);
        }
        cl_cat(err, ")", sizeof(err));
        break;
    case TURN_FAIL:
    case TURN_CANCEL:
        is_error = 1;
        last_note(st, &res);
        break;
    default:
        if (st->text.n || r->n_responses != resp0)
            jw_raw(&res, st->text.p ? st->text.p : "", st->text.n);
        else
            last_note(st, &res);    /* a command: its output */
        break;
    }
    if (st->c->out == CLI_TEXT) {
        if (err[0]) {
            jw_reset(&res);
            jw_rawz(&res, "Error: ");
            jw_rawz(&res, err);
        }
        jw_raw(&res, "\n", 1);
        st->p->out(st->p->u, res.p, res.n);
    } else {
        jw_reset(w);
        jw_rawz(w, "{\"type\":\"result\",\"subtype\":");
        jw_strz(w, sub);
        jw_rawz(w, is_error ? ",\"is_error\":true" : ",\"is_error\":false");
        key_long(w, "duration_ms", (long)(w_ms(st) - ms0), 1);
        key_long(w, "duration_api_ms", (long)(r->api_ms - api0), 1);
        key_long(w, "num_turns", r->n_responses - resp0, 1);
        if (!err[0]) {
            jw_rawz(w, ",\"result\":");
            jw_str(w, res.p ? res.p : "", res.n);
        }
        jw_rawz(w, ",\"stop_reason\":");
        if (r->n_responses != resp0 && r->st.stop_reason[0])
            jw_strz(w, r->st.stop_reason);
        else
            jw_rawz(w, "null");
        jw_rawz(w, ",\"session_id\":");
        jw_strz(w, r->sess.id);
        jw_rawz(w, ",\"total_cost_usd\":");
        put_usd(w, r->conv.cost_micro - st->base.cost_micro);
        jw_rawz(w, ",\"usage\":");
        usage_obj(w, r->conv.in_tok - st->base.in_tok, r->conv.cache_w - st->base.cache_w,
                  r->conv.cache_r - st->base.cache_r, r->conv.out_tok - st->base.out_tok);
        jw_rawz(w, ",\"modelUsage\":");
        model_usage(st, w);
        jw_rawz(w, ",\"permission_denials\":[");
        jw_raw(w, st->denials.p ? st->denials.p : "", st->denials.n);
        jw_raw(w, "]", 1);
        if (r->structured) {
            jw_rawz(w, ",\"structured_output\":");    /* --json-schema */
            jw_rawz(w, r->structured);
        }
        if (err[0]) {
            jw_rawz(w, ",\"errors\":[");
            jw_strz(w, err);
            jw_raw(w, "]", 1);
        }
        jw_rawz(w, ",\"uuid\":");
        put_uuid(st, w);
        jw_raw(w, "}", 1);
        emit(st, w);
    }
    rc = is_error ? 10 : 0;
    jw_free(&res);
    jw_reset(&st->denials);
    suggestion(st);
    return rc;
}

/* a failure before any turn (no key, nothing to resume): as a result */
static int early(pst *st, const char *msg)
{
    jw *w = &st->line;
    if (st->c->out == CLI_TEXT) {
        st->p->out(st->p->u, msg, (long)strlen(msg));
        st->p->out(st->p->u, "\n", 1);
        return 10;
    }
    jw_reset(w);
    jw_rawz(w, "{\"type\":\"result\",\"subtype\":\"success\",\"is_error\":true");
    key_long(w, "duration_ms", (long)(w_ms(st) - st->t0), 1);
    jw_rawz(w, ",\"duration_api_ms\":0,\"num_turns\":0,\"result\":");
    jw_strz(w, msg);
    jw_rawz(w, ",\"stop_reason\":null,\"session_id\":");
    jw_strz(w, st->r->sess.id);
    jw_rawz(w, ",\"total_cost_usd\":0,\"usage\":");
    usage_obj(w, 0, 0, 0, 0);
    jw_rawz(w, ",\"modelUsage\":{},\"permission_denials\":[],\"uuid\":");
    put_uuid(st, w);
    jw_raw(w, "}", 1);
    emit(st, w);
    return 10;
}

/* ---- the input ---- */

/* everything piped in: 0 (also when nothing is), -1 too big or failed */
static int read_all(pst *st, jw *out)
{
    char buf[2048];
    long n;
    if (!st->p->in)
        return 0;
    while ((n = st->p->in(st->p->u, buf, sizeof(buf))) > 0) {
        if (out->n + n > PRINT_IN_MAX)
            return -1;
        jw_raw(out, buf, n);
        if (out->oom)
            return -1;
    }
    return n < 0 ? -1 : 0;
}

/* One stream-json input line: the user message's text into out; when it
 * has an image or a document block, its blocks (text, image, document;
 * comma-separated, as sent: base64 data passes through untouched) into
 * blocks, else blocks stays empty. 0, -1 not one. */
static int user_line(const char *s, long n, jw *out, jw *blocks)
{
    jv v, x, m, c, b;
    jit it;
    int media = 0;
    jw_reset(out);
    jw_reset(blocks);
    if (json_parse(s, n, &v) || json_type(v) != J_OBJ || !json_get(v, "type", &x) || !json_streq(x, "user") ||
        !json_get(v, "message", &m) || !json_get(m, "content", &c))
        return -1;
    if (json_type(c) == J_STR) {
        long l;
        char *t = json_strdup(c, &l);
        if (!t)
            return -1;
        jw_raw(out, t, l);
        free(t);
        return out->n ? 0 : -1;
    }
    content_text(c.p, c.n, out);
    if (json_type(c) == J_ARR) {
        json_iter(c, &it);
        while (json_next(&it, 0, &b))
            if (json_get(b, "type", &x) && (json_streq(x, "image") || json_streq(x, "document")))
                media = 1;
    }
    if (media) {
        json_iter(c, &it);
        while (json_next(&it, 0, &b)) {
            if (!json_get(b, "type", &x) ||
                !(json_streq(x, "text") || json_streq(x, "image") || json_streq(x, "document")))
                continue;
            if (blocks->n)
                jw_raw(blocks, ",", 1);
            jw_raw(blocks, b.p, b.n);
        }
        return blocks->oom ? -1 : 0;
    }
    return out->n ? 0 : -1;
}

/* --replay-user-messages: the input's user message echoed (SDK's
 * SDKUserMessageReplay) */
static void replay(pst *st, const char *s, long n)
{
    jv v, m;
    jw *w = &st->line;
    if (json_parse(s, n, &v) || !json_get(v, "message", &m))
        return;
    jw_reset(w);
    jw_rawz(w, "{\"type\":\"user\",\"message\":");
    jw_raw(w, m.p, m.n);
    jw_rawz(w, ",\"parent_tool_use_id\":null,\"isReplay\":true");
    tail(st, w);
    emit(st, w);
}

static int stream_in(pst *st)
{
    char buf[1024];
    jw acc, msg, blocks;
    long n, i;
    int rc = 0, any = 0;
    jw_init(&acc);
    jw_init(&msg);
    jw_init(&blocks);
    for (;;) {
        n = st->p->in ? st->p->in(st->p->u, buf, sizeof(buf)) : 0;
        if (n > 0)
            jw_raw(&acc, buf, n);
        /* every complete line (and the last one at the end) */
        for (;;) {
            long e = -1;
            for (i = 0; i < acc.n; i++)
                if (acc.p[i] == '\n') {
                    e = i;
                    break;
                }
            if (e < 0 && (n > 0 || !acc.n))
                break;
            if (e < 0)
                e = acc.n;
            if (user_line(acc.p, e, &msg, &blocks) == 0) {
                if (st->c->replay)
                    replay(st, acc.p, e);
                rc = answer(st, msg.p ? msg.p : "", msg.n, blocks.n ? blocks.p : 0, blocks.n);
                any = 1;
            }
            memmove(acc.p, acc.p + (e < acc.n ? e + 1 : e), (size_t)(acc.n - (e < acc.n ? e + 1 : e)));
            acc.n -= e < acc.n ? e + 1 : e;
            acc.p[acc.n] = 0;
        }
        if (n <= 0 || acc.oom)
            break;
    }
    jw_free(&acc);
    jw_free(&msg);
    jw_free(&blocks);
    return any ? rc : 20;
}

/* the REPL's notes straight to the output (a subcommand's) */
static void s_write(void *u, const char *s, long n)
{
    pst *st = (pst *)u;
    if (n > 0 && s[0] != '\r')
        st->p->out(st->p->u, s, n);
}

static long s_read_line(void *u, char *buf, long cap)
{
    cl_io *o = ((pst *)u)->orig;
    return o->read_line ? o->read_line(o->u, buf, cap) : -1;
}

/* the files of a project's directory (sessions, memory/) removed */
typedef struct purge_ls {
    cl_dirent e[128];
    int n;
} purge_ls;

static int purge_one(void *c, const cl_dirent *e)
{
    purge_ls *l = (purge_ls *)c;
    if (l->n < 128)
        l->e[l->n++] = *e;
    return 0;
}

static int purge_dir(cl_sys *sys, const char *dir)
{
    purge_ls *l = (purge_ls *)malloc(sizeof(purge_ls));
    int i, gone = 0;
    if (!l || sys->kind(sys->u, dir) != 2) {
        free(l);
        return 0;
    }
    l->n = 0;
    sys->list(sys->u, dir, purge_one, l);
    for (i = 0; i < l->n; i++) {
        char p[400];
        if (path_join(dir, l->e[i].name, p, sizeof(p)))
            continue;
        if (l->e[i].dir)
            gone += purge_dir(sys, p);
        if (sys->remove && sys->remove(sys->u, p) == 0)
            gone++;
    }
    free(l);
    return gone;
}

/* the history's lines of a project dropped (the rest kept as it was) */
static int purge_history(cl_sys *sys, const char *file, const char *project)
{
    char *b = 0;
    long n = 0, i = 0;
    int gone = 0;
    jw keep;
    if (!file || !*file || sys->kind(sys->u, file) != 1 || sys->read(sys->u, file, 4L * 1024 * 1024, &b, &n))
        return 0;
    jw_init(&keep);
    while (i < n) {
        long e = i;
        jv v, x;
        char pr[300];
        while (e < n && b[e] != '\n')
            e++;
        pr[0] = 0;
        if (json_parse(b + i, e - i, &v) == 0 && json_get(v, "project", &x))
            json_str(x, pr, sizeof(pr));
        if (pr[0] && cl_strieq(pr, project))
            gone++;
        else {
            jw_raw(&keep, b + i, e - i);
            jw_raw(&keep, "\n", 1);
        }
        i = e + 1;
    }
    if (gone && !keep.oom)
        sys->write(sys->u, file, keep.p ? keep.p : "", keep.n);
    jw_free(&keep);
    free(b);
    return gone;
}

int print_subcommand(cl_repl *r, cl_cli *c, cl_pout *p)
{
    pst st;
    cl_io *orig = r->io;
    int rc = 0;
    memset(&st, 0, sizeof(st));
    st.r = r;
    st.c = c;
    st.p = p;
    st.orig = orig;
    st.io = *orig;
    st.io.u = &st;
    st.io.write = s_write;
    st.io.read_line = s_read_line;
    st.io.read = 0;
    st.io.raw = 0;
    r->io = &st.io;
    r->ui.io = &st.io;
    if (c->sub == SUB_DOCTOR)
        repl_line(r, "/doctor");
    else if (c->sub == SUB_AUTH_STATUS) {
        int in = r->key && *r->key;
        jw w;
        jw_init(&w);
        if (c->text) {
            jw_rawz(&w, in ? "Logged in with an API key" : "Not logged in (Claude auth login, or ENV:ANTHROPIC_API_KEY)");
            jw_rawz(&w, ".\nSettings directory: ");
            jw_rawz(&w, r->home);
            jw_rawz(&w, "\n");
        } else {
            jw_rawz(&w, in ? "{\"loggedIn\":true,\"authMethod\":\"api_key\",\"apiKeySource\":"
                           : "{\"loggedIn\":false,\"authMethod\":\"none\",\"apiKeySource\":");
            jw_strz(&w, c->key_source[0] ? c->key_source : in ? "user" : "none");
            jw_rawz(&w, ",\"configDirectory\":");
            jw_strz(&w, r->home);
            jw_rawz(&w, "}\n");
        }
        if (!w.oom)
            p->out(p->u, w.p, w.n);
        jw_free(&w);
        rc = in ? 0 : 10;
    } else if (c->sub == SUB_AUTH_LOGIN) {
        char key[512];
        repl_line(r, "/login");
        if (s_read_line(&st, key, sizeof(key)) > 0)
            repl_line(r, key);          /* the line after /login is the key */
        memset(key, 0, sizeof(key));
        rc = r->key && *r->key ? 0 : 10;
    } else if (c->sub == SUB_AUTH_LOGOUT)
        repl_line(r, "/logout");
    else if (c->sub == SUB_PURGE) {
        char dir[300], ans[16], num[16];
        cl_session s;
        jw m;
        if (c->sub_arg[0] && (path_join(r->tools.root, c->sub_arg, dir, sizeof(dir)) || r->sys->kind(r->sys->u, dir) != 2)) {
            static const char no[] = "Not a directory.\n";
            p->out(p->u, no, (long)sizeof(no) - 1);
            rc = 20;
        } else {
            if (!c->sub_arg[0] || r->sys->canon(r->sys->u, dir, dir, sizeof(dir)))
                cl_copy(dir, c->sub_arg[0] ? dir : r->tools.root, sizeof(dir));
            sess_init(&s, r->sys, r->home, dir);
            jw_init(&m);
            jw_rawz(&m, "This removes C:Claude's sessions, auto memory and prompt history of ");
            jw_rawz(&m, dir);
            jw_rawz(&m, " (");
            jw_rawz(&m, s.dir);
            jw_rawz(&m, "). Go on? (y/n) ");
            p->out(p->u, m.p ? m.p : "", m.n);
            jw_free(&m);
            if (s_read_line(&st, ans, sizeof(ans)) > 0 && (ans[0] == 'y' || ans[0] == 'Y')) {
                int gone = purge_dir(r->sys, s.dir);
                if (r->sys->remove)
                    r->sys->remove(r->sys->u, s.dir);
                gone += purge_history(r->sys, r->ui.histfile, dir);
                jw_init(&m);
                jw_rawz(&m, "Removed ");
                cl_ltoa(gone, num);
                jw_rawz(&m, num);
                jw_rawz(&m, gone == 1 ? " file or line.\n" : " files and lines.\n");
                p->out(p->u, m.p ? m.p : "", m.n);
                jw_free(&m);
            } else {
                static const char kept[] = "Nothing removed.\n";
                p->out(p->u, kept, (long)sizeof(kept) - 1);
            }
        }
    }
    r->io = orig;
    r->ui.io = orig;
    return rc;
}

int print_run(cl_repl *r, cl_cli *c, cl_pout *p)
{
    pst st;
    cl_io *orig = r->io;
    int rc;
    memset(&st, 0, sizeof(st));
    st.r = r;
    st.c = c;
    st.p = p;
    st.orig = orig;
    st.io = *orig;
    st.io.u = &st;
    st.io.read_line = w_read_line;
    st.io.write = w_write;
    st.io.brk = w_brk;
    st.io.sleep = w_sleep;
    st.io.ms = orig->ms ? w_ms : 0;
    st.io.log = orig->log ? w_log : 0;
    st.io.read = 0;
    st.io.size = 0;
    st.io.raw = 0;
    st.io.edit = 0;
    jw_init(&st.notes);
    jw_init(&st.text);
    jw_init(&st.denials);
    jw_init(&st.line);
    st.feed.u = &st;
    st.feed.message = f_message;
    st.feed.event = f_event;
    st.feed.denied = f_denied;
    st.feed.sub = f_sub;
    st.feed.retry = f_retry;
    st.feed.hook = f_hook;
    r->io = &st.io;
    r->ui.io = &st.io;
    r->feed = &st.feed;
    r->no_person = 1;
    st.t0 = w_ms(&st);
    st.seed = st.t0 ^ 0x5eedUL;
    st.base = r->conv;
    if (repl_need_key(r))
        rc = early(&st, "Not logged in: no API key. Set ENV:ANTHROPIC_API_KEY, or start Claude and type /login.");
    else if (cli_session(c, r)) {
        if (p->err) {
            p->err(p->u, c->err, (long)strlen(c->err));
            p->err(p->u, "\n", 1);
        }
        rc = early(&st, c->err);
    } else if (c->init_only) {
        /* --init-only: Setup and SessionStart hooks, no conversation */
        repl_setup(r, "init", 1);
        rc = 0;
    } else {
        st.base = r->conv;      /* a resumed conversation's own cost is not this run's */
        if (c->init || c->maintenance)
            repl_setup(r, c->maintenance ? "maintenance" : "init", 0);     /* Setup hooks first */
        if (c->out == CLI_STREAM)
            init_msg(&st);
        if (c->in == CLI_STREAM)
            rc = stream_in(&st);
        else {
            jw in;
            jw_init(&in);
            if (c->prompt)
                jw_rawz(&in, c->prompt);
            {
                jw piped;
                jw_init(&piped);
                if (read_all(&st, &piped)) {
                    static const char big[] = "Error: the piped input is larger than 10 MB, or could not be read.\n";
                    if (p->err)
                        p->err(p->u, big, (long)sizeof(big) - 1);
                    rc = 20;
                    jw_free(&piped);
                    jw_free(&in);
                    goto out;
                }
                if (piped.n) {
                    if (in.n)
                        jw_raw(&in, "\n", 1);
                    jw_raw(&in, piped.p, piped.n);
                }
                jw_free(&piped);
            }
            repl_setup(r, 0, 1);    /* SessionStart now: its initialUserMessage comes first */
            if (r->first_msg) {
                char *f = r->first_msg;
                r->first_msg = 0;
                rc = answer(&st, f, (long)strlen(f), 0, 0);
                free(f);
                if (!in.n) {
                    jw_free(&in);
                    goto out;
                }
            }
            if (!in.n) {
                static const char none[] =
                    "Error: Input must be provided either through stdin or as a prompt argument when using --print\n";
                if (p->err)
                    p->err(p->u, none, (long)sizeof(none) - 1);
                rc = 20;
            } else if (in.oom)
                rc = early(&st, "Out of memory.");
            else
                rc = answer(&st, in.p, in.n, 0, 0);
            jw_free(&in);
        }
    }
out:
    r->feed = 0;
    r->io = orig;
    r->ui.io = orig;
    jw_free(&st.notes);
    jw_free(&st.text);
    jw_free(&st.denials);
    jw_free(&st.line);
    return rc;
}
