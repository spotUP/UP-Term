/* policy -- what C:Claude does around a tool call and at the session's
 * events (A4 WP3, rows 3.1 3.3 3.6 3.9): the permission rules, the hooks,
 * the checkpoints before a write, the memory of a directory a read
 * reaches. Called from repl.c's turn (see repl_int.h); the tools
 * themselves (tools.c) know nothing of it: a deny or a hook's block
 * answers the tool_use here, an allow answers tools.c's question through
 * the ask callback (r->rule_now).
 * Portable C89, host-tested through the REPL suite. */
#include <stdlib.h>
#include <string.h>
#include "repl_int.h"
#include "path.h"
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
static void answer(cl_repl *r, jw *out, const char *id, const char *name, const char *raw, long rawn,
                   const char *what, const char *text)
{
    int tid = tools_id(name);
    long n = (long)strlen(text);
    r->tools.cur = tid;
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

/* is the call's path inside a directory added with /add-dir or
 * permissions.additionalDirectories? */
static int in_added(cl_repl *r, jv in)
{
    char full[512];
    int i;
    if (call_path(r, in, full, sizeof(full)))
        return 0;
    for (i = 0; i < r->cfg.ndirs; i++) {
        char d[300];
        if (path_join(r->tools.root, r->cfg.dirs[i], d, sizeof(d)) == 0 && path_inside(d, full))
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

int pol_pre(cl_repl *r, const char *id, const char *name, int input_ok, const char *raw, long rawn, jw *out)
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
    if (d == RULE_NONE && in_added(r, in) && (is_read(cc) || (is_edit(cc) && r->tools.perm.mode == PERM_ACCEPT)))
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
            answer(r, out, id, name, raw, rawn, what, t.p ? t.p : "blocked");
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
        answer(r, out, id, name, raw, rawn, what, m);
        return 1;
    }
    if (d == RULE_ASK) {
        int tid = tools_id(name);
        d = RULE_NONE;              /* tools_run asks as usual */
        if (tid >= 0 && !perm_refused(&r->tools.perm, tid) && !perm_must_ask(&r->tools.perm, tid, 0)) {
            /* it would run without a question: the rule asks */
            int ans;
            char m[200];
            r->tools.cur = tid;
            r->tools.cur_in = raw;
            r->tools.cur_inn = rawn;
            cl_copy(m, "Claude needs your permission to use ", sizeof(m));
            cl_cat(m, cc, sizeof(m));
            pol_notify(r, m);
            ans = ui_ask(&r->ui, tid, name, what, 0);
            if (ans == ASK_NO || ans == ASK_STOP) {
                if (ans == ASK_STOP)
                    r->tools.stop = 1;
                answer(r, out, id, name, raw, rawn, what,
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
    if (is_edit(cc) && !perm_refused(&r->tools.perm, tools_id(name) >= 0 ? tools_id(name) : T_WRITE_FILE) &&
        call_path(r, in, full, sizeof(full)) == 0)
        cp_before_write(&r->cp, full);
    return 0;
}

void pol_post(cl_repl *r, const char *name, int input_ok, const char *raw, long rawn, const char *blk, long n,
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

int pol_stop(cl_repl *r, int active, jw *reason)
{
    cl_hookres h;
    int rc = 0;
    if (!hooks_any(&r->hooks, HK_STOP, ""))
        return 0;
    hookres_init(&h);
    hooks_run(&r->hooks, HK_STOP, "", active ? ",\"stop_hook_active\":true" : ",\"stop_hook_active\":false", &h);
    shown(r, &h);
    if (h.blocked && !h.stop) {
        jw_rawz(reason, "Stop hook feedback: ");
        jw_raw(reason, h.reason.n ? h.reason.p : "go on", h.reason.n ? h.reason.n : 5);
        rc = 1;
    }
    hookres_free(&h);
    return rc;
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
    jw_rawz(&ev, "},\"output_style\":{\"name\":");
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
    if (r->sys->run(r->sys->u, line, 5, o, 2047, &on, &rc) == 0 && rc == 0) {
        for (i = 0; i < on && o[i] != '\n' && o[i] != '\r'; i++)
            ;
        o[i] = 0;
        cl_copy(r->status_text, o, sizeof(r->status_text));
    }
    free(o);
    if (r->sys->remove)
        r->sys->remove(r->sys->u, file);
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
