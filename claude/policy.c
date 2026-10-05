/* policy -- what C:Claude does around a tool call and at the session's
 * events (A4 WP3, rows 3.1 3.3 3.6 3.9): the permission rules, the hooks,
 * the checkpoints before a write, the memory of a directory a read
 * reaches -- for the conversation's calls and a subagent's alike (the
 * tools' call hook); and what the REPL hands the tools and the screen:
 * the extensions (.claude/agents, skills, commands -> ext.h), the
 * added directories, the WebSearch switch, the status line. The tools
 * themselves (tools.c) know nothing of the rules: a deny or a hook's block
 * answers the tool_use here, an allow answers tools.c's question through
 * the ask callback (r->rule_now).
 * Portable C89, host-tested through the REPL suite. */
#include <stdlib.h>
#include <string.h>
#include "repl_int.h"
#include "path.h"
#include "schema.h"
#include "util.h"

static const char *src_name(int src)
{
    switch (src) {
    case CFG_USER:
        return "user settings";
    case CFG_PROJECT:
        return "project settings";
    case CFG_LOCAL:
        return "local project settings";
    default:
        return "this session";
    }
}

static int in_str(jv in, const char *key, char *out, long cap)
{
    jv x;
    out[0] = 0;
    return json_get(in, key, &x) && json_type(x) == J_STR && json_str(x, out, cap) >= 0;
}

/* the call's path, resolved against the root: 0, -1 none */
static int call_path(cl_repl *r, jv in, char *full, long cap)
{
    char p[512];
    if (!in_str(in, "file_path", p, sizeof(p)) && !in_str(in, "path", p, sizeof(p)) &&
        !in_str(in, "notebook_path", p, sizeof(p)))
        return -1;
    return path_join(r->tools.root, p, full, cap);
}

static int is_edit(const char *cc)
{
    return !strcmp(cc, "Write") || !strcmp(cc, "Edit") || !strcmp(cc, "MultiEdit") || !strcmp(cc, "NotebookEdit");
}

static int is_read(const char *cc)
{
    return !strcmp(cc, "Read") || !strcmp(cc, "LS") || !strcmp(cc, "Grep") || !strcmp(cc, "Glob");
}

/* the call answered here: shown, and its tool_result an error */
static void answer(cl_repl *r, cl_tools *tl, jw *out, const char *id, const char *name, const char *raw, long rawn,
                   const char *what, const char *text)
{
    int tid = tools_id(name);
    long n = (long)strlen(text);
    tl->cur = tid;
    ui_tool(&r->ui, tid, name, what, raw, rawn);
    ui_result(&r->ui, tid, raw, rawn, 1, text, n);
    jw_rawz(out, "{\"type\":\"tool_result\",\"tool_use_id\":");
    jw_strz(out, id);
    jw_rawz(out, ",\"content\":");
    jw_str(out, text, n);
    jw_rawz(out, ",\"is_error\":true}");
}

/* a one-line summary of the call, as the question shows it */
static void what_of(cl_repl *r, const char *cc, jv in, char *out, long cap)
{
    char v[300];
    if (!strcmp(cc, "Bash") && in_str(in, "command", v, sizeof(v)))
        cl_copy(out, v, cap);
    else if (call_path(r, in, out, cap) == 0)
        ;
    else if (in_str(in, "url", v, sizeof(v)) || in_str(in, "pattern", v, sizeof(v)))
        cl_copy(out, v, cap);
    else
        cl_copy(out, "", cap);
    if ((long)strlen(out) > 70)
        cl_copy(out + 67, "...", 4);
}

/* cl_tools.added: is full (canonical) inside a directory added with
 * /add-dir, --add-dir or permissions.additionalDirectories? */
static int pol_added(void *u, const char *full)
{
    cl_repl *r = (cl_repl *)u;
    int i;
    for (i = 0; i < r->cfg.ndirs; i++) {
        char d[300], c[300];
        if (path_join(r->tools.root, r->cfg.dirs[i], d, sizeof(d)))
            continue;
        if (r->sys->canon(r->sys->u, d, c, sizeof(c)))
            cl_copy(c, d, sizeof(c));
        if (path_inside(c, full))
            return 1;
    }
    return 0;
}

/* a custom command's allowed-tools, for the turn it runs: "Bash(git:*),
 * Read" -- each item a rule */
static int turn_allows(cl_repl *r, const char *name, jv in)
{
    const char *p = r->turn_tools;
    while (p && *p) {
        char item[200];
        long k = 0;
        int depth = 0;
        while (*p == ' ' || *p == ',')
            p++;
        while (*p && (depth || (*p != ',' && *p != ' ')) && k < (long)sizeof(item) - 1) {
            if (*p == '(')
                depth++;
            else if (*p == ')')
                depth--;
            item[k++] = *p++;
        }
        item[k] = 0;
        if (k && cfg_rule_match(item, name, in, r->tools.root))
            return 1;
    }
    return 0;
}

static void shown(cl_repl *r, cl_hookres *h)
{
    if (h->shown.n)
        ui_line(&r->ui, h->shown.p);
}

/* Before tools_run: 1 when the call was answered here (denied by a rule,
 * blocked by a PreToolUse hook): its tool_result is in out. */
static int pol_pre(cl_repl *r, cl_tools *tl, const char *id, const char *name, int input_ok, const char *raw,
                   long rawn, jw *out)
{
    jv in;
    const char *cc = cfg_cc_tool(name);
    const cl_rule *which = 0;
    char what[320], full[512];
    int d;
    r->rule_now = RULE_NONE;
    if (!input_ok || json_parse(raw, rawn, &in) || json_type(in) != J_OBJ)
        return 0;                   /* tools_run says what is wrong with it */
    d = cfg_decide(&r->cfg, name, in, r->tools.root, &which);
    if (d == RULE_NONE && r->turn_tools && turn_allows(r, name, in))
        d = RULE_ALLOW;
    what_of(r, cc, in, what, sizeof(what));
    if (hooks_any(&r->hooks, HK_PRE_TOOL, name)) {
        cl_hookres h;
        jw ex;
        hookres_init(&h);
        jw_init(&ex);
        jw_rawz(&ex, ",\"tool_name\":");
        jw_strz(&ex, cc);
        jw_rawz(&ex, ",\"tool_input\":");
        jw_raw(&ex, raw, rawn);
        hooks_run(&r->hooks, HK_PRE_TOOL, name, ex.p, &h);
        jw_free(&ex);
        shown(r, &h);
        if (h.blocked) {
            jw t;
            jw_init(&t);
            jw_rawz(&t, "PreToolUse hook blocked this call: ");
            jw_raw(&t, h.reason.n ? h.reason.p : "(no reason given)", h.reason.n ? h.reason.n : 17);
            answer(r, tl, out, id, name, raw, rawn, what, t.p ? t.p : "blocked");
            jw_free(&t);
            hookres_free(&h);
            return 1;
        }
        if (h.decision == RULE_ALLOW && d != RULE_DENY)
            d = RULE_ALLOW;
        else if (h.decision == RULE_ASK && d != RULE_DENY)
            d = RULE_ASK;
        hookres_free(&h);
    }
    if (d == RULE_DENY) {
        char m[500];
        r->n_rule_deny++;
        repl_denied(r, name, raw, rawn);
        cl_copy(m, "Permission to use ", sizeof(m));
        cl_cat(m, cc, sizeof(m));
        cl_cat(m, " has been denied", sizeof(m));
        if (which) {
            cl_cat(m, " by the rule ", sizeof(m));
            cl_cat(m, which->text, sizeof(m));
            cl_cat(m, " (", sizeof(m));
            cl_cat(m, src_name(which->src), sizeof(m));
            cl_cat(m, ")", sizeof(m));
        }
        cl_cat(m, ".", sizeof(m));
        answer(r, tl, out, id, name, raw, rawn, what, m);
        return 1;
    }
    if (d == RULE_ASK) {
        int tid = tools_id(name);
        /* tools_run asks as usual (RULE_ASK kept: an explicit ask rule,
         * which bypassPermissions does not answer) */
        if (tid >= 0 && !perm_refused(&tl->perm, tid) && !perm_must_ask(&tl->perm, tid, 0)) {
            /* it would run without a question: the rule asks */
            int ans;
            tl->cur = tid;
            tl->cur_in = raw;
            tl->cur_inn = rawn;
            ans = repl_ask(r, tid, name, what, 0, 1);
            if (ans == ASK_NO || ans == ASK_STOP) {
                if (ans == ASK_STOP)
                    tl->stop = 1;
                answer(r, tl, out, id, name, raw, rawn, what,
                       ans == ASK_STOP ? "the user stopped this tool call and will tell you what to do differently; "
                                         "wait for their message"
                                       : "the user declined this tool call");
                return 1;
            }
            d = RULE_ALLOW;
        }
    }
    r->rule_now = d;
    /* the file as it was, for /rewind */
    if (is_edit(cc) && !perm_refused(&tl->perm, tools_id(name) >= 0 ? tools_id(name) : T_WRITE) &&
        call_path(r, in, full, sizeof(full)) == 0)
        cp_before_write(&r->cp, full);
    return 0;
}

/* After it: the result block tools_run appended (blk, n); text for
 * Claude to see after the results goes to extra. */
static void pol_post(cl_repl *r, const char *name, int input_ok, const char *raw, long rawn, const char *blk, long n,
                     jw *extra)
{
    jv in, b, x;
    const char *cc = cfg_cc_tool(name);
    int is_error;
    if (!input_ok || json_parse(raw, rawn, &in) || json_type(in) != J_OBJ || json_parse(blk, n, &b))
        return;
    is_error = json_get(b, "is_error", &x) && json_type(x) == J_TRUE;
    if (hooks_any(&r->hooks, HK_POST_TOOL, name)) {
        cl_hookres h;
        jw ex;
        hookres_init(&h);
        jw_init(&ex);
        jw_rawz(&ex, ",\"tool_name\":");
        jw_strz(&ex, cc);
        jw_rawz(&ex, ",\"tool_input\":");
        jw_raw(&ex, raw, rawn);
        jw_rawz(&ex, ",\"tool_response\":");
        jw_raw(&ex, blk, n);
        hooks_run(&r->hooks, HK_POST_TOOL, name, ex.p, &h);
        jw_free(&ex);
        shown(r, &h);
        if (h.blocked || h.context.n) {
            if (extra->n)
                jw_rawz(extra, "\n\n");
            if (h.blocked) {
                jw_rawz(extra, "PostToolUse hook feedback on ");
                jw_rawz(extra, cc);
                jw_rawz(extra, ": ");
                jw_raw(extra, h.reason.p ? h.reason.p : "", h.reason.n);
            }
            if (h.context.n) {
                if (h.blocked)
                    jw_rawz(extra, "\n");
                jw_raw(extra, h.context.p, h.context.n);
            }
        }
        hookres_free(&h);
    }
    if (!is_error && (is_read(cc) || is_edit(cc))) {
        /* a directory below the root with memory of its own: now it counts */
        char full[512];
        jw t;
        jw_init(&t);
        if (call_path(r, in, full, sizeof(full)) == 0 && mem_nested(&r->mem, r->sys, r->tools.root, full, &t) &&
            t.n) {
            if (extra->n)
                jw_rawz(extra, "\n\n");
            jw_rawz(extra, "<system-reminder>\n");
            jw_raw(extra, t.p, t.n);
            jw_rawz(extra, "\n</system-reminder>");
        }
        jw_free(&t);
    }
}

static void result_block(jw *out, const char *id, const char *text, int is_error)
{
    jw_rawz(out, "{\"type\":\"tool_result\",\"tool_use_id\":");
    jw_strz(out, id);
    jw_rawz(out, ",\"content\":");
    jw_strz(out, text);
    jw_rawz(out, is_error ? ",\"is_error\":true}" : "}");
}

/* --json-schema's StructuredOutput call: its input checked against the
 * schema, kept as the run's structured output */
static void structured(cl_repl *r, const char *id, int input_ok, const char *raw, long rawn, jw *out)
{
    jv s, v;
    char err[300];
    if (!input_ok || json_parse(raw, rawn, &v)) {
        result_block(out, id, "The input is not valid JSON. Call StructuredOutput again with the answer.", 1);
        return;
    }
    if (json_parse(r->schema, (long)strlen(r->schema), &s) == 0 && schema_check(s, v, err, sizeof(err))) {
        jw m;
        jw_init(&m);
        jw_rawz(&m, "The output does not match the required schema: ");
        jw_rawz(&m, err);
        jw_rawz(&m, ". Call StructuredOutput again with a corrected answer.");
        result_block(out, id, m.p ? m.p : err, 1);
        jw_free(&m);
        return;
    }
    free(r->structured);
    r->structured = (char *)malloc((size_t)rawn + 1);
    if (r->structured) {
        memcpy(r->structured, raw, (size_t)rawn);
        r->structured[rawn] = 0;
    }
    result_block(out, id, "Structured output provided successfully", 0);
}

void pol_call(void *u, cl_tools *tl, const char *id, const char *name, int input_ok, const char *raw, long rawn,
              jw *out, jw *extra)
{
    cl_repl *r = (cl_repl *)u;
    cl_tools *was = r->at;
    long at = out->n;
    if (r->schema && tl == &r->tools && !strcmp(name, "StructuredOutput")) {
        structured(r, id, input_ok, raw, rawn, out);
        return;
    }
    r->at = tl;                 /* the screen's callbacks show this call's tool */
    r->cur_id = id;             /* print mode's permission_denials */
    if (!pol_pre(r, tl, id, name, input_ok, raw, rawn, out))
        tools_run(tl, id, name, input_ok, raw, rawn, out);
    r->rule_now = RULE_NONE;
    pol_post(r, name, input_ok, raw, rawn, out->p + at, out->n - at, extra);
    r->at = was;
}

void pol_session(cl_repl *r, int event, const char *source)
{
    cl_hookres h;
    jw ex;
    if (!hooks_any(&r->hooks, event, source))
        return;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, event == HK_SESSION_END ? ",\"reason\":" : ",\"source\":");
    jw_strz(&ex, source);
    hooks_run(&r->hooks, event, source, ex.p, &h);
    jw_free(&ex);
    shown(r, &h);
    if (event == HK_SESSION_START && h.context.n) {
        if (r->pending.n)
            jw_rawz(&r->pending, "\n\n");
        jw_raw(&r->pending, h.context.p, h.context.n);
    }
    hookres_free(&h);
}

int pol_prompt(cl_repl *r, const char *prompt, long n)
{
    cl_hookres h;
    jw ex;
    int rc = 0;
    if (!hooks_any(&r->hooks, HK_PROMPT, ""))
        return 0;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, ",\"prompt\":");
    jw_str(&ex, prompt, n);
    hooks_run(&r->hooks, HK_PROMPT, "", ex.p, &h);
    jw_free(&ex);
    shown(r, &h);
    if (h.blocked || h.stop) {
        repl_say(r, "A UserPromptSubmit hook blocked the prompt: ", h.reason.n ? h.reason.p : "(no reason given)");
        rc = -1;
    } else if (h.context.n) {
        if (r->pending.n)
            jw_rawz(&r->pending, "\n\n");
        jw_raw(&r->pending, h.context.p, h.context.n);
    }
    hookres_free(&h);
    return rc;
}

/* Stop and SubagentStop: 1 when a hook sends Claude on */
static int stop_hook(cl_repl *r, int event, const char *name, int active, jw *reason)
{
    cl_hookres h;
    int rc = 0;
    if (!hooks_any(&r->hooks, event, name))
        return 0;
    hookres_init(&h);
    hooks_run(&r->hooks, event, name, active ? ",\"stop_hook_active\":true" : ",\"stop_hook_active\":false", &h);
    shown(r, &h);
    if (h.blocked && !h.stop) {
        jw_rawz(reason, event == HK_SUBAGENT_STOP ? "SubagentStop hook feedback: " : "Stop hook feedback: ");
        jw_raw(reason, h.reason.n ? h.reason.p : "go on", h.reason.n ? h.reason.n : 5);
        rc = 1;
    }
    hookres_free(&h);
    return rc;
}

int pol_stop(cl_repl *r, int active, jw *reason)
{
    return stop_hook(r, HK_STOP, "", active, reason);
}

/* cl_tools.agent_stop */
static int pol_agent_stop(void *u, const char *agent, int active, jw *reason)
{
    return stop_hook((cl_repl *)u, HK_SUBAGENT_STOP, agent, active, reason);
}

void pol_notify(cl_repl *r, const char *message)
{
    cl_hookres h;
    jw ex;
    if (!hooks_any(&r->hooks, HK_NOTIFICATION, ""))
        return;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, ",\"message\":");
    jw_strz(&ex, message);
    hooks_run(&r->hooks, HK_NOTIFICATION, "", ex.p, &h);
    jw_free(&ex);
    shown(r, &h);
    hookres_free(&h);
}

void pol_precompact(cl_repl *r, int automatic, const char *focus)
{
    cl_hookres h;
    jw ex;
    const char *trig = automatic ? "auto" : "manual";
    if (!hooks_any(&r->hooks, HK_PRE_COMPACT, trig))
        return;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, ",\"trigger\":");
    jw_strz(&ex, trig);
    jw_rawz(&ex, ",\"custom_instructions\":");
    jw_strz(&ex, focus ? focus : "");
    hooks_run(&r->hooks, HK_PRE_COMPACT, trig, ex.p, &h);
    jw_free(&ex);
    shown(r, &h);
    hookres_free(&h);
}

void pol_statusline(cl_repl *r)
{
    char file[300], line[600], d[24];
    char *o;
    long on = 0, rc = 0, i;
    jw ev;
    if (!r->cfg.status_cmd[0] || path_join(r->tmp, "Claude-status.json", file, sizeof(file)))
        return;
    jw_init(&ev);
    jw_rawz(&ev, "{\"hook_event_name\":\"Status\",\"session_id\":");
    jw_strz(&ev, r->sess.id);
    jw_rawz(&ev, ",\"transcript_path\":");
    jw_strz(&ev, r->sess.file);
    jw_rawz(&ev, ",\"cwd\":");
    jw_strz(&ev, r->tools.root);
    jw_rawz(&ev, ",\"model\":{\"id\":");
    jw_strz(&ev, r->model);
    jw_rawz(&ev, ",\"display_name\":");
    jw_strz(&ev, r->model);
    jw_rawz(&ev, "},\"workspace\":{\"current_dir\":");
    jw_strz(&ev, r->tools.root);
    jw_rawz(&ev, ",\"project_dir\":");
    jw_strz(&ev, r->tools.root);
    jw_rawz(&ev, ",\"added_dirs\":[");
    for (i = 0; i < r->cfg.ndirs; i++) {
        if (i)
            jw_raw(&ev, ",", 1);
        jw_strz(&ev, r->cfg.dirs[i]);
    }
    jw_rawz(&ev, "]},\"output_style\":{\"name\":");
    jw_strz(&ev, r->style[0] ? r->style : "default");
    conv_dollars(r->conv.cost_micro, d, sizeof(d));
    jw_rawz(&ev, "},\"cost\":{\"total_cost_usd\":");
    jw_rawz(&ev, d + 1);
    jw_rawz(&ev, "}}\n");
    o = (char *)malloc(2048);
    if (!o || ev.oom || r->sys->write(r->sys->u, file, ev.p, ev.n)) {
        jw_free(&ev);
        free(o);
        return;
    }
    jw_free(&ev);
    cl_copy(line, r->cfg.status_cmd, sizeof(line) - 320);
    cl_cat(line, " < ", sizeof(line));
    cl_cat(line, file, sizeof(line));
    if (r->tui && r->sys->setenv) {
        /* the script cannot ask the console its size: Claude Code's variables */
        char num[16];
        cl_ltoa(r->tui->cols, num);
        r->sys->setenv(r->sys->u, "COLUMNS", num);
        cl_ltoa(r->tui->rows, num);
        r->sys->setenv(r->sys->u, "LINES", num);
    }
    r->n_status_runs++;
    r->status_text[0] = 0;          /* a failure or no output: the row goes blank */
    if (r->sys->run(r->sys->u, line, 5, o, 2047, &on, &rc) == 0 && rc == 0) {
        long k = 0;
        /* its lines, each its own row: carriage returns out, the last newline too */
        for (i = 0; i < on && k < (long)sizeof(r->status_text) - 1; i++)
            if (o[i] != '\r')
                r->status_text[k++] = o[i];
        while (k > 0 && r->status_text[k - 1] == '\n')
            k--;
        r->status_text[k] = 0;
    }
    free(o);
    if (r->sys->remove)
        r->sys->remove(r->sys->u, file);
    r->status_ms = r->io->ms ? r->io->ms(r->io->u) : 1;
    if (!r->status_ms)
        r->status_ms = 1;
    r->status_due = 0;
    if (r->tui) {
        r->status_mode = r->tools.perm.mode;
        r->status_vim = r->tui->ed.vim;
        tui_frame(r->tui);          /* only the rows that changed go out */
    }
}

#define STATUS_DEBOUNCE_MS 300UL

void pol_status_event(cl_repl *r)
{
    unsigned long now;
    if (!r->cfg.status_cmd[0])
        return;
    now = r->io->ms ? r->io->ms(r->io->u) : 0;
    if (r->status_ms && now - r->status_ms < STATUS_DEBOUNCE_MS) {
        r->status_due = 1;          /* the next tick runs it */
        return;
    }
    pol_statusline(r);
}

void pol_status_tick(void *u)
{
    cl_repl *r = (cl_repl *)u;
    unsigned long now;
    if (!r->cfg.status_cmd[0])
        return;
    if (r->tui && (r->status_mode != r->tools.perm.mode || r->status_vim != r->tui->ed.vim))
        r->status_due = 1;          /* Shift+Tab, a vim mode change */
    now = r->io->ms ? r->io->ms(r->io->u) : 0;
    if (r->cfg.status_refresh_s > 0 && r->status_ms &&
        now - r->status_ms >= (unsigned long)r->cfg.status_refresh_s * 1000UL)
        r->status_due = 1;
    if (r->status_due && (!r->status_ms || now - r->status_ms >= STATUS_DEBOUNCE_MS))
        pol_statusline(r);
}

/* ---- the extensions the tools see (ext.h): .claude/agents, skills, commands ---- */

static int ext_agents(void *u, const cl_agent **list)
{
    cl_repl *r = (cl_repl *)u;
    *list = r->x_agents;
    return r->nx_agents;
}

static int ext_skills(void *u, const cl_skill **list)
{
    cl_repl *r = (cl_repl *)u;
    *list = r->x_skills;
    return r->nx_skills;
}

static int ext_commands(void *u, const cl_command **list)
{
    cl_repl *r = (cl_repl *)u;
    *list = r->x_cmds;
    return r->nx_cmds;
}

/* the ${CLAUDE_*} values for a definition's expansion */
static void vars_of(cl_repl *r, const cl_def *d, cl_cmd_vars *v, char *dir, long cap)
{
    if (d->type != DEF_SKILL || path_parent(d->path, dir, cap))
        dir[0] = 0;
    v->session_id = r->sess.id;
    v->effort = r->effort;
    v->skill_dir = dir;
    v->project_dir = r->tools.root;
}

int pol_expand(cl_repl *r, const cl_def *d, const char *args, jw *out, char *err, long cap)
{
    cl_cmd_vars v;
    char dir[300];
    vars_of(r, d, &v, dir, sizeof(dir));
    return cmd_expand_vars(d, args, &v, r->sys, r->tools.root, out, err, cap);
}

/* a definition's allowed-tools in force for the rest of the turn, its
 * ${CLAUDE_SKILL_DIR} / ${CLAUDE_PROJECT_DIR} put in (Claude Code
 * substitutes them in the Bash rules too) */
void pol_turn_tools(cl_repl *r, const cl_def *d)
{
    cl_cmd_vars v;
    char dir[300];
    jw w;
    if (!d->tools[0])
        return;
    vars_of(r, d, &v, dir, sizeof(dir));
    jw_init(&w);
    if (cmd_subst_vars(d->tools, &v, &w) == 0 && w.p) {
        cl_copy(r->turn_buf, w.p, sizeof(r->turn_buf));
        r->turn_tools = r->turn_buf;
    }
    jw_free(&w);
}

/* SlashCommand: the command expanded as a typed one is (slash_custom), its
 * allowed-tools in force for the rest of the turn */
static int ext_expand(void *u, const char *name, const char *args, jw *out, char *err, long cap)
{
    cl_repl *r = (cl_repl *)u;
    const cl_def *d = defs_find(&r->defs, DEF_COMMAND, name);
    if (!d || d->no_model) {
        cl_copy(err, "", cap);      /* no such command (for Claude) */
        return -1;
    }
    if (pol_expand(r, d, args, out, err, cap))
        return -1;
    r->n_cmds_run++;
    pol_turn_tools(r, d);
    return 0;
}

/* Skill: expanded as a command is (A4 gaps X1); its allowed-tools in force
 * for the rest of the turn; its model is not switched mid-turn (the
 * turn's thinking belongs to the model that started it), as SlashCommand */
static int ext_skill(void *u, const char *name, const char *args, int activate, jw *out, int *fork, char *agent,
                     long acap, char *err, long cap)
{
    cl_repl *r = (cl_repl *)u;
    const cl_def *d = defs_find(&r->defs, DEF_SKILL, name);
    *fork = 0;
    agent[0] = 0;
    if (!d) {
        cl_copy(err, "no such skill", cap);
        return -1;
    }
    if (pol_expand(r, d, args, out, err, cap))
        return -1;
    *fork = d->fork;
    cl_copy(agent, d->agent, acap);
    if (activate) {
        r->n_skills_run++;
        pol_turn_tools(r, d);
    }
    return 0;
}

/* cl_tools.agent_msg: a subagent's message to print mode's stream */
static void pol_agent_msg(void *u, const char *parent, int user, const char *json, long n, cl_stream *st)
{
    cl_repl *r = (cl_repl *)u;
    if (r->feed && r->feed->sub)
        r->feed->sub(r->feed->u, parent, user, json, n, st);
}

void pol_ext_free(cl_repl *r)
{
    free(r->x_agents);
    free(r->x_skills);
    free(r->x_cmds);
    r->x_agents = 0;
    r->x_skills = 0;
    r->x_cmds = 0;
    r->nx_agents = r->nx_skills = r->nx_cmds = 0;
}

int pol_tools(cl_repl *r)
{
    int na = defs_count(&r->defs, DEF_AGENT), ns = defs_count(&r->defs, DEF_SKILL);
    int nc = defs_count(&r->defs, DEF_COMMAND), i;
    const cl_def *d;
    pol_ext_free(r);
    r->x_agents = (cl_agent *)calloc((size_t)na + 1, sizeof(cl_agent));
    r->x_skills = (cl_skill *)calloc((size_t)ns + 1, sizeof(cl_skill));
    r->x_cmds = (cl_command *)calloc((size_t)nc + 1, sizeof(cl_command));
    if (!r->x_agents || !r->x_skills || !r->x_cmds) {
        pol_ext_free(r);
        return -1;
    }
    for (i = 0; (d = defs_nth(&r->defs, DEF_AGENT, i)) != 0; i++) {
        cl_agent *a = &r->x_agents[r->nx_agents++];
        a->name = d->name;
        a->description = d->description;
        a->tools = d->tools[0] ? d->tools : 0;
        a->model = d->model[0] ? d->model : 0;
        a->prompt = d->body;
        a->deny_tools = d->deny_tools && d->deny_tools[0] ? d->deny_tools : 0;
        a->max_turns = d->max_turns;
        a->effort = d->effort[0] ? d->effort : 0;
        a->skills = d->skills && d->skills[0] ? d->skills : 0;
        a->perm_mode = d->perm_mode[0] ? d->perm_mode : 0;
    }
    /* disable-model-invocation: only the user runs it */
    for (i = 0; (d = defs_nth(&r->defs, DEF_SKILL, i)) != 0; i++)
        if (!d->no_model) {
            cl_skill *s = &r->x_skills[r->nx_skills++];
            s->name = d->name;
            s->description = d->description;
            s->path = d->path;
        }
    for (i = 0; (d = defs_nth(&r->defs, DEF_COMMAND, i)) != 0; i++)
        if (!d->no_model) {
            cl_command *c = &r->x_cmds[r->nx_cmds++];
            c->name = d->name;
            c->description = d->description;
        }
    r->tools.web_search = cfg_web_search(&r->cfg);
    free(r->tools.json);            /* declared anew at the next request */
    r->tools.json = 0;
    return 0;
}

void pol_attach_tools(cl_repl *r)
{
    r->at = &r->tools;
    r->tools.call = pol_call;
    r->tools.added = pol_added;
    r->tools.agent_stop = pol_agent_stop;
    r->tools.agent_msg = pol_agent_msg;
    r->ext.skill = ext_skill;
    r->ext.u = r;
    r->ext.agents = ext_agents;
    r->ext.skills = ext_skills;
    r->ext.commands = ext_commands;
    r->ext.expand = ext_expand;
    r->tools.ext = &r->ext;
}

/* ---- what the screen (WP1, ui.h "A4 (WP1)") takes from WP3 ---- */

static const char *ui_setting(void *u, const char *key)
{
    cl_repl *r = (cl_repl *)u;
    if (!strcmp(key, "theme"))
        return r->cfg.theme[0] ? r->cfg.theme : 0;
    if (!strcmp(key, "editorMode"))
        return r->cfg.editor_mode[0] ? r->cfg.editor_mode : 0;
    return 0;
}

/* /theme and /vim keep their choice in the user's settings */
static void ui_set_setting(void *u, const char *key, const char *value)
{
    cl_repl *r = (cl_repl *)u;
    jw v;
    jw_init(&v);
    jw_strz(&v, value);
    if (!v.oom)
        cfg_write_key(r->sys, cfg_file(&r->cfg, CFG_USER), key, v.p);
    jw_free(&v);
    if (!strcmp(key, "theme"))
        cl_copy(r->cfg.theme, value, sizeof(r->cfg.theme));
    else if (!strcmp(key, "editorMode"))
        cl_copy(r->cfg.editor_mode, value, sizeof(r->cfg.editor_mode));
}

static int ui_memory_files(void *u, cl_memfile *out, int max)
{
    static const char *const lab[3] = { "Project memory (CLAUDE.md)", "User memory (ENVARC:Claude/CLAUDE.md)",
                                        "Local project memory (CLAUDE.local.md)" };
    static const int kind[3] = { MEM_PROJECT, MEM_USER, MEM_LOCAL };
    cl_repl *r = (cl_repl *)u;
    int i;
    for (i = 0; i < 3 && i < max; i++) {
        cl_copy(out[i].label, lab[i], sizeof(out[i].label));
        mem_file_of(kind[i], r->home, r->tools.root, out[i].path, sizeof(out[i].path));
    }
    return i;
}

static void ui_memory_changed(void *u, const char *path)
{
    (void)path;
    repl_load_memory((cl_repl *)u);
}

/* the rewind points: the prompts, oldest first */
static int rw_points(cl_repl *r, int *msg, int max)
{
    int n = repl_prompts(r, msg, max), i;
    for (i = 0; i < n / 2; i++) {
        int t = msg[i];
        msg[i] = msg[n - 1 - i];
        msg[n - 1 - i] = t;
    }
    return n;
}

static int rw_count(void *u)
{
    int msg[64];
    return rw_points((cl_repl *)u, msg, 64);
}

static int rw_label(void *u, int i, char *out, long cap)
{
    cl_repl *r = (cl_repl *)u;
    int msg[64], n = rw_points(r, msg, 64);
    jv v, b, x;
    jit it;
    long k;
    if (i < 0 || i >= n || json_parse(r->conv.m[msg[i]].json, r->conv.m[msg[i]].n, &v))
        return -1;
    json_iter(v, &it);
    if (!json_next(&it, 0, &b) || !json_get(b, "text", &x) || json_str(x, out, cap) < 0)
        return -1;
    for (k = 0; out[k]; k++)
        if (out[k] == '\n' || out[k] == '\r')
            out[k] = ' ';
    return 0;
}

static int rw_can(void *u, int i)
{
    cl_repl *r = (cl_repl *)u;
    int msg[64], n = rw_points(r, msg, 64);
    if (i < 0 || i >= n)
        return 0;
    return RW_CONV | (cp_files_since(&r->cp, msg[i]) ? RW_CODE : 0);
}

static int rw_restore(void *u, int i, int what)
{
    cl_repl *r = (cl_repl *)u;
    int msg[64], n = rw_points(r, msg, 64);
    if (i < 0 || i >= n)
        return -1;
    return repl_rewind(r, msg[i], (what & RW_CODE) != 0, (what & RW_CONV) != 0);
}

void pol_attach_ui(cl_repl *r)
{
    r->ui.setting = ui_setting;
    r->ui.set_setting = ui_set_setting;
    r->ui.su = r;
    r->ui.memory_files = ui_memory_files;
    r->ui.memory_changed = ui_memory_changed;
    r->ui.mu = r;
    r->ui.rw.u = r;
    r->ui.rw.count = rw_count;
    r->ui.rw.label = rw_label;
    r->ui.rw.can = rw_can;
    r->ui.rw.restore = rw_restore;
}
