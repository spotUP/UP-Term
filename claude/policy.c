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
#include "tools_int.h"
#include "tasks.h"
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

/* cl_tools.can_read: would Read run on full without a question -- no deny or
 * ask rule for it (Edit's relaxed check) */
int pol_can_read(void *u, const char *full)
{
    cl_repl *r = (cl_repl *)u;
    jw w;
    jv in;
    int d = RULE_NONE;
    jw_init(&w);
    jw_rawz(&w, "{\"file_path\":");
    jw_strz(&w, full);
    jw_raw(&w, "}", 1);
    if (!w.oom && json_parse(w.p, w.n, &in) == 0)
        d = cfg_decide(&r->cfg, "Read", in, r->tools.root, 0);
    jw_free(&w);
    return d != RULE_DENY && d != RULE_ASK;
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
/* the permission mode's name, Claude Code's (the hooks' permission_mode) */
static const char *mode_name(const cl_repl *r, const cl_tools *tl)
{
    int pol = tl && tl->ask_policy ? tl->ask_policy - 1 : r->ask_policy;
    if (pol == ASKP_BYPASS)
        return "bypassPermissions";
    if (pol == ASKP_DENY)
        return "dontAsk";
    return (tl ? tl : &r->tools)->perm.mode == PERM_ACCEPT ? "acceptEdits"
           : (tl ? tl : &r->tools)->perm.mode == PERM_PLAN ? "plan" : "default";
}

/* a tool event's common members: the tool, its input, its id, the mode */
static void tool_event(cl_repl *r, cl_tools *tl, jw *ex, const char *cc, const char *id, const char *raw, long rawn)
{
    jw_rawz(ex, ",\"tool_name\":");
    jw_strz(ex, cc);
    jw_rawz(ex, ",\"tool_input\":");
    jw_raw(ex, raw, rawn);
    jw_rawz(ex, ",\"tool_use_id\":");
    jw_strz(ex, id ? id : "");
    jw_rawz(ex, ",\"permission_mode\":");
    jw_strz(ex, mode_name(r, tl));
}

static int pol_pre(cl_repl *r, cl_tools *tl, const char *id, const char *name, int input_ok, const char *raw,
                   long rawn, jw *out, jw *extra, jw *upd)
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
    if (d == RULE_NONE && r->mem.auto_dir[0] && (is_edit(cc) || is_read(cc)) &&
        call_path(r, in, full, sizeof(full)) == 0) {
        /* auto memory: Claude's own directory (as written, or canonical when it exists) */
        char ad[300];
        if (path_inside(r->mem.auto_dir, full) ||
            (r->sys->canon(r->sys->u, r->mem.auto_dir, ad, sizeof(ad)) == 0 && path_inside(ad, full)))
            d = RULE_ALLOW;
    }
    if (d == RULE_NONE && tl->mem_dir[0] && (is_edit(cc) || is_read(cc)) && call_path(r, in, full, sizeof(full)) == 0) {
        /* an agent's own memory directory (memory: in its frontmatter) */
        char ad[300];
        if (path_inside(tl->mem_dir, full) ||
            (r->sys->canon(r->sys->u, tl->mem_dir, ad, sizeof(ad)) == 0 && path_inside(ad, full)))
            d = RULE_ALLOW;
    }
    what_of(r, cc, in, what, sizeof(what));
    if (hooks_any(&r->hooks, HK_PRE_TOOL, name)) {
        cl_hookres h;
        jw ex;
        hookres_init(&h);
        jw_init(&ex);
        tool_event(r, tl, &ex, cc, id, raw, rawn);
        r->hooks.tool = name;
        r->hooks.input = raw;
        r->hooks.input_n = rawn;
        hooks_run(&r->hooks, HK_PRE_TOOL, name, ex.p, &h);
        r->hooks.tool = r->hooks.input = 0;
        jw_free(&ex);
        shown(r, &h);
        if (h.defer && !h.blocked) {
            /* "defer" (Claude Code): print mode, one call in the round -- the
             * run stops here, the call kept for a --resume; elsewhere ignored */
            if (r->no_person && tl == &r->tools && r->round_ntools == 1) {
                cl_copy(r->defer_id, id ? id : "", sizeof(r->defer_id));
                cl_copy(r->defer_name, cfg_cc_tool(name), sizeof(r->defer_name));
                free(r->defer_input);
                r->defer_input = (char *)malloc((size_t)rawn + 1);
                if (r->defer_input) {
                    memcpy(r->defer_input, raw, (size_t)rawn);
                    r->defer_input[rawn] = 0;
                }
                hookres_free(&h);
                return 2;
            }
            if (r->debug && r->io->log) {
                static const char m[] = "PreToolUse defer ignored: only print mode with one tool call defers\n";
                r->io->log(r->io->u, m, (long)sizeof(m) - 1);
            }
        }
        if (h.stop)
            tl->stop = 1;           /* "continue": false -- Claude stops after this round */
        if (h.context.n) {
            /* additionalContext: for Claude, beside the round's results */
            if (extra->n)
                jw_rawz(extra, "\n\n");
            jw_raw(extra, h.context.p, h.context.n);
        }
        if (!h.blocked && h.updated.n && json_parse(h.updated.p, h.updated.n, &in) == 0 && json_type(in) == J_OBJ) {
            /* updatedInput: the call runs with it, the rules decide again on it */
            jw_reset(upd);
            jw_raw(upd, h.updated.p, h.updated.n);
            raw = upd->p;
            rawn = upd->n;
            d = cfg_decide(&r->cfg, name, in, r->tools.root, &which);
            if (d == RULE_NONE && r->turn_tools && turn_allows(r, name, in))
                d = RULE_ALLOW;
            what_of(r, cc, in, what, sizeof(what));
        }
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
            char note[200];
            tl->cur = tid;
            tl->cur_in = raw;
            tl->cur_inn = rawn;
            note[0] = 0;
            ans = repl_ask(r, tid, name, what, 0, 1, note, sizeof(note));
            if (ans == ASK_RERUN && r->perm_upd.n && json_parse(r->perm_upd.p, r->perm_upd.n, &in) == 0) {
                /* a PermissionRequest hook's updatedInput: the call goes on with it,
                 * a deny rule still decides */
                jw_reset(upd);
                jw_raw(upd, r->perm_upd.p, r->perm_upd.n);
                jw_reset(&r->perm_upd);
                json_parse(upd->p, upd->n, &in);
                raw = upd->p;
                rawn = upd->n;
                r->n_perm_rerun++;
                if (cfg_decide(&r->cfg, name, in, r->tools.root, &which) == RULE_DENY) {
                    answer(r, tl, out, id, name, raw, rawn, what, "Permission to use this tool has been denied.");
                    return 1;
                }
                ans = ASK_ONCE;
            }
            if (ans == ASK_NO && note[0]) {
                /* No with a comment: Claude is told why and goes on (tl_gate's way) */
                jw t;
                jw_init(&t);
                jw_rawz(&t, "the user declined this tool call and said: ");
                jw_rawz(&t, note);
                answer(r, tl, out, id, name, raw, rawn, what, t.p ? t.p : "the user declined this tool call");
                jw_free(&t);
                return 1;
            }
            if (ans != ASK_NO && ans != ASK_STOP && ans != ASK_RERUN)
                cl_copy(tl->note, note, sizeof(tl->note));  /* Yes with a comment: with the result */
            if (ans == ASK_NO || ans == ASK_STOP || ans == ASK_RERUN) {
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
        if (!r->no_checkpoints)
            cp_before_write(&r->cp, full);     /* CLAUDE_CODE_DISABLE_FILE_CHECKPOINTING turns this off */
    return 0;
}

/* A file was read or edited: a .claude/skills between the root and it
 * loads (Claude Code's nested skills), a skill whose paths match becomes
 * available. 1 when the list changed. */
static int skills_for(cl_repl *r, const char *full)
{
    char p[300], d[300];
    int changed = 0, i, k;
    long rl = (long)strlen(r->tools.root);
    const char *rel = full;
    if (path_inside(r->tools.root, full) && (long)strlen(full) > rl) {
        rel = full + rl;
        if (*rel == '/')
            rel++;
    }
    /* the directories from the file's up to (not including) the root */
    if (path_parent(full, p, sizeof(p)) == 0)
        for (k = 0; k < 8 && path_inside(r->tools.root, p) && !cl_strieq(p, r->tools.root); k++) {
            char sk[340];
            int seen = 0;
            for (i = 0; i < r->n_nested; i++)
                seen |= cl_strieq(r->nested[i], p);
            if (!seen && path_join(p, ".claude/skills", sk, sizeof(sk)) == 0 &&
                r->sys->kind(r->sys->u, sk) == 2 && r->n_nested < 8) {
                cl_copy(r->nested[r->n_nested++], p, sizeof(r->nested[0]));
                if (defs_load_dir(&r->defs, r->sys, DEF_SKILL, CFG_PROJECT, sk))
                    changed = 1;
            }
            cl_copy(d, p, sizeof(d));
            if (path_parent(d, p, sizeof(p)))
                break;
        }
    for (i = 0; i < r->defs.n; i++) {
        cl_def *s = &r->defs.d[i];
        const char *g;
        if (s->type != DEF_SKILL || s->active || !s->paths || !s->paths[0])
            continue;
        for (g = s->paths; *g;) {
            char one[200];
            int n = 0;
            while (*g == ' ' || *g == ',')
                g++;
            while (*g && *g != ',' && n < (int)sizeof(one) - 1)
                one[n++] = *g++;
            while (n && one[n - 1] == ' ')
                n--;
            one[n] = 0;
            if (n && (cfg_glob(one, rel, 1) || cfg_glob(one, full, 1))) {
                s->active = 1;
                changed = 1;
                break;
            }
        }
    }
    if (changed)
        repl_load_menu(r);
    return changed;
}

/* After it: the result block tools_run appended (blk, n); text for
 * Claude to see after the results goes to extra. */
static void pol_post(cl_repl *r, cl_tools *tl, const char *id, const char *name, int input_ok, const char *raw,
                     long rawn, jw *out, long at, jw *extra)
{
    const char *blk = out->p + at;
    long n = out->n - at;
    jv in, b, x;
    const char *cc = cfg_cc_tool(name);
    int is_error, ev;
    if (!input_ok || json_parse(raw, rawn, &in) || json_type(in) != J_OBJ || json_parse(blk, n, &b))
        return;
    is_error = json_get(b, "is_error", &x) && json_type(x) == J_TRUE;
    ev = is_error ? HK_POST_TOOL_FAILURE : HK_POST_TOOL;     /* Claude Code: a failed call has its own event */
    if (hooks_any(&r->hooks, ev, name)) {
        cl_hookres h;
        jw ex;
        hookres_init(&h);
        jw_init(&ex);
        tool_event(r, tl, &ex, cc, id, raw, rawn);
        if (is_error) {
            jw_rawz(&ex, ",\"error\":");
            if (json_get(b, "content", &x) && json_type(x) == J_STR)
                jw_raw(&ex, x.p, x.n);
            else
                jw_rawz(&ex, "\"\"");
            jw_rawz(&ex, ",\"is_interrupt\":false");
        } else {
            jw_rawz(&ex, ",\"tool_response\":");
            jw_raw(&ex, blk, n);
        }
        r->hooks.tool = name;
        r->hooks.input = raw;
        r->hooks.input_n = rawn;
        hooks_run(&r->hooks, ev, name, ex.p, &h);
        r->hooks.tool = r->hooks.input = 0;
        jw_free(&ex);
        shown(r, &h);
        if (h.stop)
            tl->stop = 1;
        if (h.output.n && !is_error) {
            /* updatedToolOutput: the tool_result Claude gets is the hook's */
            jv ov;
            out->n = at;
            jw_rawz(out, "{\"type\":\"tool_result\",\"tool_use_id\":");
            jw_strz(out, id);
            jw_rawz(out, ",\"content\":");
            if (json_parse(h.output.p, h.output.n, &ov) == 0 && (json_type(ov) == J_STR || json_type(ov) == J_ARR))
                jw_raw(out, h.output.p, h.output.n);
            else
                jw_str(out, h.output.p, h.output.n);
            jw_raw(out, "}", 1);
        }
        if (h.blocked || h.context.n) {
            if (extra->n)
                jw_rawz(extra, "\n\n");
            if (h.blocked) {
                jw_rawz(extra, is_error ? "PostToolUseFailure hook feedback on " : "PostToolUse hook feedback on ");
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
    if (!is_error && (is_read(cc) || is_edit(cc)) && !r->bare) {
        /* a directory below the root with memory of its own, a rule whose
         * paths: match the file: now they count */
        char full[512];
        jw t;
        int k0 = r->mem.n, k1 = 0, got = 0;
        jw_init(&t);
        if (call_path(r, in, full, sizeof(full)) == 0) {
            got = mem_nested(&r->mem, r->sys, r->tools.root, full, &t);
            k1 = r->mem.n;
            got += mem_rules(&r->mem, r->sys, r->tools.root, full, &t);
            if (got) {
                /* InstructionsLoaded for each (no decision) */
                int k2 = r->mem.n;
                r->mem.n = k1;
                pol_instructions(r, k0, "nested_traversal");
                r->mem.n = k2;
                pol_instructions(r, k1, "path_glob_match");
            }
        }
        if (skills_for(r, full))
            pol_tools(r);               /* a nested .claude/skills, a skill's paths: now offered */
        if (got && t.n) {
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
    jw upd;
    int pre;
    if (r->schema && tl == &r->tools && !strcmp(name, "StructuredOutput")) {
        structured(r, id, input_ok, raw, rawn, out);
        return;
    }
    r->at = tl;                 /* the screen's callbacks show this call's tool */
    r->cur_id = id;             /* print mode's permission_denials */
    jw_init(&upd);
    pre = pol_pre(r, tl, id, name, input_ok, raw, rawn, out, extra, &upd);
    if (pre == 2) {
        /* deferred: no result; the turn stops at this call */
        jw_free(&upd);
        r->at = was;
        r->rule_now = RULE_NONE;
        return;
    }
    if (!pre) {
        if (upd.n) {
            raw = upd.p;        /* a PreToolUse hook's updatedInput */
            rawn = upd.n;
        }
        tl->rule_ask = r->rule_now == RULE_ASK;
        tl->rerun = 0;
        tools_run(tl, id, name, input_ok, raw, rawn, out);
        tl->rule_ask = 0;
        if (tl->rerun && r->perm_upd.n) {
            /* a PermissionRequest hook allowed it with updatedInput: the call
             * again with that input, the rules deciding anew on it (Claude Code) */
            tl->rerun = 0;
            out->n = at;
            jw_reset(&upd);
            jw_raw(&upd, r->perm_upd.p, r->perm_upd.n);
            jw_reset(&r->perm_upd);
            raw = upd.p;
            rawn = upd.n;
            r->perm_rerun = 1;
            {
                jw upd2;
                jw_init(&upd2);
                if (!pol_pre(r, tl, id, name, 1, raw, rawn, out, extra, &upd2)) {
                    tl->rule_ask = r->rule_now == RULE_ASK;
                    if (r->rule_now == RULE_NONE)
                        r->rule_now = RULE_ALLOW;   /* the hook's allow stands for the new input */
                    tools_run(tl, id, name, 1, raw, rawn, out);
                    tl->rule_ask = 0;
                }
                jw_free(&upd2);
            }
            r->perm_rerun = 0;
            r->n_perm_rerun++;
        }
        tl->rerun = 0;
    }
    r->rule_now = RULE_NONE;
    pol_post(r, tl, id, name, input_ok, raw, rawn, out, at, extra);
    jw_free(&upd);
    r->at = was;
}

/* CLAUDE_ENV_FILE's lines ("export NAME=value" or "NAME=value") set
 * for the rest of the session (the commands Bash runs see them) */
static void env_file(cl_repl *r, const char *f)
{
    char *b = 0;
    long n = 0, i = 0;
    if (r->sys->kind(r->sys->u, f) != 1 || r->sys->read(r->sys->u, f, 16384, &b, &n))
        return;
    while (i < n) {
        long e = i, eq;
        char name[64], val[512];
        while (e < n && b[e] != '\n')
            e++;
        if (e - i > 7 && !strncmp(b + i, "export ", 7))
            i += 7;
        for (eq = i; eq < e && b[eq] != '='; eq++)
            ;
        if (eq < e && eq - i > 0 && eq - i < (long)sizeof(name)) {
            long vl = e - eq - 1;
            const char *v = b + eq + 1;
            if (vl >= 2 && (v[0] == '"' || v[0] == '\'') && v[vl - 1] == v[0]) {
                v++;
                vl -= 2;
            }
            if (vl >= 0 && vl < (long)sizeof(val)) {
                memcpy(name, b + i, (size_t)(eq - i));
                name[eq - i] = 0;
                memcpy(val, v, (size_t)vl);
                val[vl] = 0;
                if (val[0] && val[strlen(val) - 1] == '\r')
                    val[strlen(val) - 1] = 0;
                r->sys->setenv(r->sys->u, name, val);
            }
        }
        i = e + 1;
    }
    free(b);
}

void pol_env_prepare(cl_repl *r)
{
    char envf[300];
    if (r->sys->setenv && path_join(r->tmp, "Claude-env.sh", envf, sizeof(envf)) == 0)
        r->sys->setenv(r->sys->u, "CLAUDE_ENV_FILE", envf);
}

void pol_env_apply(cl_repl *r)
{
    char envf[300];
    if (r->sys->setenv && path_join(r->tmp, "Claude-env.sh", envf, sizeof(envf)) == 0)
        env_file(r, envf);
}

void pol_session(cl_repl *r, int event, const char *source)
{
    cl_hookres h;
    jw ex;
    char envf[300];
    if (!hooks_any(&r->hooks, event, source))
        return;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, event == HK_SESSION_END ? ",\"reason\":" : ",\"source\":");
    jw_strz(&ex, source);
    if (event == HK_SESSION_START && r->sys->setenv && path_join(r->tmp, "Claude-env.sh", envf, sizeof(envf)) == 0) {
        if (r->sys->remove)
            r->sys->remove(r->sys->u, envf);
        r->sys->setenv(r->sys->u, "CLAUDE_ENV_FILE", envf);
    } else
        envf[0] = 0;
    hooks_run(&r->hooks, event, source, ex.p, &h);
    jw_free(&ex);
    shown(r, &h);
    if (envf[0])
        env_file(r, envf);
    if (event == HK_SESSION_START && h.has_watch)
        watch_set(r, h.watch.p ? h.watch.p : "[]", h.watch.n ? h.watch.n : 2);     /* watchPaths: FileChanged */
    if (event == HK_SESSION_START) {
        if (h.title[0] && strcmp(source, "clear") && strcmp(source, "compact"))
            sess_rename(&r->sess, h.title);     /* sessionTitle: as /rename */
        if (h.first.n && !r->first_msg) {
            /* initialUserMessage: the session's first turn (print mode) */
            r->first_msg = (char *)malloc((size_t)h.first.n + 1);
            if (r->first_msg) {
                memcpy(r->first_msg, h.first.p, (size_t)h.first.n);
                r->first_msg[h.first.n] = 0;
            }
        }
        if (h.reload)
            repl_load_defs(r);              /* reloadSkills: what the hook installed is there now */
    }
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

/* the text blocks of a message's content array, joined */
static void text_of(const char *json, long n, jw *out)
{
    jv v, b, x;
    jit it;
    if (json_parse(json, n, &v) || json_type(v) != J_ARR)
        return;
    json_iter(v, &it);
    while (json_next(&it, 0, &b))
        if (json_get(b, "type", &x) && json_streq(x, "text") && json_get(b, "text", &x)) {
            long l;
            char *t = json_strdup(x, &l);
            if (t) {
                if (out->n)
                    jw_rawz(out, "\n\n");
                jw_raw(out, t, l);
            }
            free(t);
        }
}

/* Stop's and SubagentStop's members: stop_hook_active, the agent's id and
 * type (SubagentStop), last_assistant_message, background_tasks,
 * session_crons (Claude Code's) */
static void stop_members(cl_repl *r, jw *ex, int active, const char *agent, const char *id, const char *last,
                         long ln)
{
    jw_rawz(ex, active ? ",\"stop_hook_active\":true" : ",\"stop_hook_active\":false");
    if (agent) {
        jw_rawz(ex, ",\"agent_id\":");
        jw_strz(ex, id ? id : "");
        jw_rawz(ex, ",\"agent_type\":");
        jw_strz(ex, agent);
    }
    jw_rawz(ex, ",\"last_assistant_message\":");
    jw_str(ex, last ? last : "", last ? ln : 0);
    jw_rawz(ex, ",\"background_tasks\":");
    shells_json(&r->tools, ex);
    jw_rawz(ex, ",\"session_crons\":");
    tasks_crons_json(r->tools.tasks, ex, 0, 1);
}

/* Stop and SubagentStop: 1 when a hook sends Claude on */
static int stop_hook(cl_repl *r, int event, const char *name, const char *ex, jw *reason)
{
    cl_hookres h;
    int rc = 0;
    if (!hooks_any(&r->hooks, event, name))
        return 0;
    hookres_init(&h);
    hooks_run(&r->hooks, event, name, ex, &h);
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
    jw ex, last;
    int i, rc;
    if (!hooks_any(&r->hooks, HK_STOP, ""))
        return 0;
    jw_init(&ex);
    jw_init(&last);
    for (i = r->conv.n - 1; i >= 0; i--)
        if (!r->conv.m[i].user) {
            text_of(r->conv.m[i].json, r->conv.m[i].n, &last);
            break;
        }
    stop_members(r, &ex, active, 0, 0, last.p, last.n);
    rc = ex.oom ? 0 : stop_hook(r, HK_STOP, "", ex.p, reason);
    jw_free(&ex);
    jw_free(&last);
    return rc;
}

/* cl_tools.agent_start: SubagentStart (matcher the agent type) */
static void pol_agent_start(void *u, const char *agent, const char *id, jw *context)
{
    cl_repl *r = (cl_repl *)u;
    cl_hookres h;
    jw ex;
    if (!hooks_any(&r->hooks, HK_SUBAGENT_START, agent))
        return;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, ",\"agent_id\":");
    jw_strz(&ex, id);
    jw_rawz(&ex, ",\"agent_type\":");
    jw_strz(&ex, agent);
    hooks_run(&r->hooks, HK_SUBAGENT_START, agent, ex.p, &h);
    jw_free(&ex);
    shown(r, &h);
    if (h.context.n)
        jw_raw(context, h.context.p, h.context.n);
    hookres_free(&h);
}

/* cl_tools.agent_stop */
static int pol_agent_stop(void *u, const char *agent, const char *id, int active, const char *last, long ln,
                          jw *reason)
{
    cl_repl *r = (cl_repl *)u;
    jw ex;
    int rc;
    if (!hooks_any(&r->hooks, HK_SUBAGENT_STOP, agent))
        return 0;
    jw_init(&ex);
    stop_members(r, &ex, active, agent, id, last, ln);
    rc = ex.oom ? 0 : stop_hook(r, HK_SUBAGENT_STOP, agent, ex.p, reason);
    jw_free(&ex);
    return rc;
}

void pol_notify(cl_repl *r, const char *type, const char *message)
{
    cl_hookres h;
    jw ex;
    if (!hooks_any(&r->hooks, HK_NOTIFICATION, type))
        return;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, ",\"message\":");
    jw_strz(&ex, message);
    jw_rawz(&ex, ",\"title\":\"Claude\",\"notification_type\":");
    jw_strz(&ex, type);
    hooks_run(&r->hooks, HK_NOTIFICATION, type, ex.p, &h);
    jw_free(&ex);
    shown(r, &h);
    hookres_free(&h);
}

int pol_precompact(cl_repl *r, int automatic, const char *focus)
{
    cl_hookres h;
    jw ex;
    int rc = 0;
    const char *trig = automatic ? "auto" : "manual";
    if (!hooks_any(&r->hooks, HK_PRE_COMPACT, trig))
        return 0;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, ",\"trigger\":");
    jw_strz(&ex, trig);
    jw_rawz(&ex, ",\"custom_instructions\":");
    jw_strz(&ex, focus ? focus : "");
    hooks_run(&r->hooks, HK_PRE_COMPACT, trig, ex.p, &h);
    jw_free(&ex);
    shown(r, &h);
    if (h.blocked || h.stop) {
        repl_say(r, "A PreCompact hook blocked the compaction: ", h.reason.n ? h.reason.p : "(no reason given)");
        rc = -1;
    }
    hookres_free(&h);
    return rc;
}

/* an event whose hooks only run (and show what they say): name the
 * matcher's value, extra its members */
static void just_run(cl_repl *r, int event, const char *name, const char *extra)
{
    cl_hookres h;
    if (!hooks_any(&r->hooks, event, name))
        return;
    hookres_init(&h);
    hooks_run(&r->hooks, event, name, extra, &h);
    shown(r, &h);
    hookres_free(&h);
}

void pol_postcompact(cl_repl *r, int automatic, const char *summary, long n)
{
    jw ex;
    jw_init(&ex);
    jw_rawz(&ex, ",\"trigger\":");
    jw_strz(&ex, automatic ? "auto" : "manual");
    jw_rawz(&ex, ",\"compact_summary\":");
    jw_str(&ex, summary, n);
    if (!ex.oom)
        just_run(r, HK_POST_COMPACT, automatic ? "auto" : "manual", ex.p);
    jw_free(&ex);
}

void pol_stop_failure(cl_repl *r, const char *error)
{
    jw ex;
    jw_init(&ex);
    jw_rawz(&ex, ",\"error\":");
    jw_strz(&ex, error);
    if (!ex.oom)
        just_run(r, HK_STOP_FAILURE, error, ex.p);
    jw_free(&ex);
}

void pol_cwd_changed(cl_repl *r, const char *old_cwd, const char *new_cwd)
{
    jw ex;
    cl_hookres h;
    char envf[300];
    if (!hooks_any(&r->hooks, HK_CWD_CHANGED, ""))
        return;
    jw_init(&ex);
    jw_rawz(&ex, ",\"old_cwd\":");
    jw_strz(&ex, old_cwd);
    jw_rawz(&ex, ",\"new_cwd\":");
    jw_strz(&ex, new_cwd);
    hookres_init(&h);
    /* Claude Code: CwdChanged clears CLAUDE_ENV_FILE's variables, its hooks may write new ones */
    if (r->sys->remove && path_join(r->tmp, "Claude-env.sh", envf, sizeof(envf)) == 0)
        r->sys->remove(r->sys->u, envf);
    pol_env_prepare(r);
    if (!ex.oom)
        hooks_run(&r->hooks, HK_CWD_CHANGED, "", ex.p, &h);
    pol_env_apply(r);
    shown(r, &h);
    if (h.has_watch)
        watch_set(r, h.watch.p ? h.watch.p : "[]", h.watch.n ? h.watch.n : 2);
    hookres_free(&h);
    jw_free(&ex);
}

void pol_dir_added(cl_repl *r, const char *dir)
{
    jw ex;
    jw_init(&ex);
    jw_rawz(&ex, ",\"directory\":");
    jw_strz(&ex, dir);
    jw_rawz(&ex, ",\"source\":\"slash_command\"");
    if (!ex.oom)
        just_run(r, HK_DIR_ADDED, "slash_command", ex.p);
    jw_free(&ex);
}

int pol_expansion(cl_repl *r, const cl_def *d, const char *args, const char *prompt, long n)
{
    cl_hookres h;
    jw ex;
    int rc = 0;
    if (!hooks_any(&r->hooks, HK_PROMPT_EXPANSION, d->name))
        return 0;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, ",\"expansion_type\":\"slash_command\",\"command_name\":");
    jw_strz(&ex, d->name);
    jw_rawz(&ex, ",\"command_args\":");
    jw_strz(&ex, args);
    jw_rawz(&ex, ",\"command_source\":");
    jw_strz(&ex, d->src == CFG_PROJECT ? "project" : d->src == CFG_USER ? "user" : "session");
    jw_rawz(&ex, ",\"prompt\":");
    jw_str(&ex, prompt, n);
    jw_rawz(&ex, ",\"permission_mode\":");
    jw_strz(&ex, mode_name(r, 0));
    hooks_run(&r->hooks, HK_PROMPT_EXPANSION, d->name, ex.p, &h);
    jw_free(&ex);
    shown(r, &h);
    if (h.blocked || h.stop) {
        repl_say(r, "A UserPromptExpansion hook blocked /", d->name);
        if (h.reason.n)
            repl_say(r, "  ", h.reason.p);
        rc = -1;
    } else if (h.context.n) {
        if (r->pending.n)
            jw_rawz(&r->pending, "\n\n");
        jw_raw(&r->pending, h.context.p, h.context.n);
    }
    hookres_free(&h);
    return rc;
}

int pol_permission_request(cl_repl *r, const char *tool, const char *input, long n)
{
    cl_hookres h;
    jw ex;
    int rc = RULE_NONE;
    const char *cc = cfg_cc_tool(tool);
    if (!hooks_any(&r->hooks, HK_PERMISSION_REQUEST, tool))
        return RULE_NONE;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, ",\"tool_name\":");
    jw_strz(&ex, cc);
    jw_rawz(&ex, ",\"tool_input\":");
    if (input && n > 0)
        jw_raw(&ex, input, n);
    else
        jw_rawz(&ex, "{}");
    jw_rawz(&ex, ",\"permission_mode\":");
    jw_strz(&ex, mode_name(r, r->at));
    r->hooks.tool = tool;
    r->hooks.input = input;
    r->hooks.input_n = n;
    hooks_run(&r->hooks, HK_PERMISSION_REQUEST, tool, ex.p, &h);
    r->hooks.tool = r->hooks.input = 0;
    jw_free(&ex);
    shown(r, &h);
    rc = h.behavior;
    jw_reset(&r->perm_upd);
    if (rc == RULE_ALLOW && h.updated.n && !r->perm_rerun) {
        /* "allow" with updatedInput: the call runs again with it (pol_call) */
        jv v;
        if (json_parse(h.updated.p, h.updated.n, &v) == 0 && json_type(v) == J_OBJ)
            jw_raw(&r->perm_upd, h.updated.p, h.updated.n);
    }
    if (h.stop && r->at)
        r->at->stop = 1;            /* "interrupt": true */
    hookres_free(&h);
    return rc;
}

void pol_keep_rule(cl_repl *r, const char *tool, const char *input, long n)
{
    const char *cc = cfg_cc_tool(tool);
    char rule[300], word[64];
    jv in, x;
    cl_copy(rule, cc, sizeof(rule));
    if (input && n > 0 && json_parse(input, n, &in) == 0) {
        if (!strcmp(cc, "Bash") && json_get(in, "command", &x)) {
            /* Claude Code's prefix rule: the command's first word */
            char cmd[200];
            int k = 0;
            json_str(x, cmd, sizeof(cmd));
            while (cmd[k] && cmd[k] != ' ' && cmd[k] != ';' && cmd[k] != '|' && cmd[k] != '&' &&
                   k < (int)sizeof(word) - 1) {
                word[k] = cmd[k];
                k++;
            }
            word[k] = 0;
            if (k) {
                cl_copy(rule, "Bash(", sizeof(rule));
                cl_cat(rule, word, sizeof(rule));
                cl_cat(rule, " *)", sizeof(rule));
            }
        } else if (!strcmp(cc, "WebFetch") && json_get(in, "url", &x)) {
            char url[300], *h, *e;
            json_str(x, url, sizeof(url));
            h = strstr(url, "://");
            if (h) {
                h += 3;
                for (e = h; *e && *e != '/' && *e != ':'; e++)
                    ;
                *e = 0;
                cl_copy(rule, "WebFetch(domain:", sizeof(rule));
                cl_cat(rule, h, sizeof(rule));
                cl_cat(rule, ")", sizeof(rule));
            }
        }
    }
    if (cfg_write_rule(r->sys, cfg_file(&r->cfg, CFG_LOCAL), RULE_ALLOW, rule, 1) == 0) {
        cfg_add_rule(&r->cfg, RULE_ALLOW, CFG_LOCAL, rule);
        repl_say(r, "Allowed from now on in this project: ", rule);
    } else
        repl_say(r, "The rule could not be kept: ", cfg_file(&r->cfg, CFG_LOCAL));
}

/* PreModelSwitch: -1 a hook cancelled the switch (shown); PostModelSwitch after it */
int pol_model_switch(cl_repl *r, const char *from, const char *to, const char *requested, int after)
{
    cl_hookres h;
    jw ex;
    int ev = after ? HK_POST_MODEL_SWITCH : HK_PRE_MODEL_SWITCH, rc = 0;
    if (!hooks_any(&r->hooks, ev, to))
        return 0;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, ",\"from_model\":");
    jw_strz(&ex, from);
    jw_rawz(&ex, ",\"to_model\":");
    jw_strz(&ex, to);
    jw_rawz(&ex, ",\"requested_model\":");
    jw_strz(&ex, requested);
    jw_rawz(&ex, ",\"source\":\"user\"");
    hooks_run(&r->hooks, ev, to, ex.p, &h);
    jw_free(&ex);
    shown(r, &h);
    if (!after && (h.blocked || h.stop)) {
        repl_say(r, "A PreModelSwitch hook blocked the switch: ", h.reason.n ? h.reason.p : "(no reason given)");
        rc = -1;
    }
    hookres_free(&h);
    return rc;
}

/* InstructionsLoaded for the memory files from index k on (no decision) */
void pol_instructions(cl_repl *r, int k, const char *reason)
{
    for (; k < r->mem.n; k++) {
        const cl_memsrc *f = &r->mem.f[k];
        jw ex;
        if (!hooks_any(&r->hooks, HK_INSTRUCTIONS_LOADED, reason))
            return;
        jw_init(&ex);
        jw_rawz(&ex, ",\"file_path\":");
        jw_strz(&ex, f->path);
        jw_rawz(&ex, ",\"memory_type\":");
        jw_strz(&ex, f->kind == MEM_USER ? "User" : f->kind == MEM_LOCAL ? "Local" : "Project");
        jw_rawz(&ex, ",\"load_reason\":");
        jw_strz(&ex, f->kind == MEM_IMPORT ? "include" : reason);
        if (!ex.oom)
            just_run(r, HK_INSTRUCTIONS_LOADED, f->kind == MEM_IMPORT ? "include" : reason, ex.p);
        jw_free(&ex);
    }
}

/* PostToolBatch after a round: 1 when a hook stops the loop (its reason
 * kept for Claude in extra) */
int pol_batch(cl_repl *r, const char *calls, long n, jw *extra)
{
    cl_hookres h;
    jw ex;
    int rc = 0;
    if (!hooks_any(&r->hooks, HK_POST_TOOL_BATCH, ""))
        return 0;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, ",\"tool_calls\":");
    jw_raw(&ex, calls, n);
    jw_rawz(&ex, ",\"permission_mode\":");
    jw_strz(&ex, mode_name(r, 0));
    hooks_run(&r->hooks, HK_POST_TOOL_BATCH, "", ex.p, &h);
    jw_free(&ex);
    shown(r, &h);
    if (h.blocked || h.stop) {
        repl_say(r, "A PostToolBatch hook stopped the turn: ", h.reason.n ? h.reason.p : "(no reason given)");
        if (h.reason.n) {
            if (extra->n)
                jw_rawz(extra, "\n\n");
            jw_rawz(extra, "PostToolBatch hook: ");
            jw_raw(extra, h.reason.p, h.reason.n);
        }
        rc = 1;
    }
    hookres_free(&h);
    return rc;
}

/* ConfigChange: -1 a hook blocks the new settings (nothing is said) */
int pol_config_change(cl_repl *r, int src, const char *file)
{
    cl_hookres h;
    jw ex;
    int rc = 0;
    const char *source = src == CFG_USER ? "user_settings" : src == CFG_PROJECT ? "project_settings" : "local_settings";
    if (!hooks_any(&r->hooks, HK_CONFIG_CHANGE, source))
        return 0;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, ",\"source\":");
    jw_strz(&ex, source);
    jw_rawz(&ex, ",\"file_path\":");
    jw_strz(&ex, file);
    hooks_run(&r->hooks, HK_CONFIG_CHANGE, source, ex.p, &h);
    jw_free(&ex);
    if (h.blocked)
        rc = -1;
    hookres_free(&h);
    return rc;
}

/* Setup (--init, --init-only, --maintenance): runs, cannot block */
void pol_setup(cl_repl *r, const char *trigger)
{
    jw ex;
    jw_init(&ex);
    jw_rawz(&ex, ",\"trigger\":");
    jw_strz(&ex, trigger);
    if (!ex.oom)
        just_run(r, HK_SETUP, trigger, ex.p);
    jw_free(&ex);
}

/* a prompt hook's question: the small model, no tools */
static int pol_ask_model(void *u, const char *model, const char *prompt, jw *answer)
{
    cl_repl *r = (cl_repl *)u;
    char err[120];
    return agent_query(&r->tools, model && *model ? cfg_model(model) : "claude-haiku-4-5",
                       "You evaluate a condition for a hook of C:Claude (Claude Code on an Amiga). Answer with the "
                       "JSON object asked for, nothing else.",
                       prompt, (long)strlen(prompt), answer, err, sizeof(err))
               ? -1
               : 0;
}

/* an http hook's POST: over the WebFetch connection (bsdsocket, AmiSSL) */
static int pol_hook_post(void *u, const char *url, const char *headers, const char *body, long n, int timeout_s,
                         int *status, jw *resp, char *err, long cap)
{
    cl_repl *r = (cl_repl *)u;
    if (!r->tools.web) {
        cl_copy(err, "there is no network connection here", cap);
        return -1;
    }
    return web_post(r->tools.web, url, headers, body, n, timeout_s, status, resp, err, cap);
}

/* an agent hook: a subagent with Read, Grep and Glob, 50 turns (Claude Code) */
static int pol_ask_agent(void *u, const char *model, const char *prompt, jw *answer)
{
    cl_repl *r = (cl_repl *)u;
    cl_agent a;
    memset(&a, 0, sizeof(a));
    a.name = "hook";
    a.description = "verifies a hook's condition";
    a.tools = "Read, Grep, Glob";
    a.model = model && *model ? model : "haiku";
    a.prompt = "You verify a condition for a hook of C:Claude (Claude Code on an Amiga). Look at the files you "
               "need with Read, Grep and Glob, then answer with the JSON object asked for, nothing else.";
    a.max_turns = 50;
    return agent_answer(&r->tools, &a, prompt, (long)strlen(prompt), answer);
}

/* ---- hooks in a skill's or an agent's frontmatter (A4 gaps 2) ---- */

static cl_settings *extra_hooks(cl_repl *r)
{
    if (!r->hooks.extra) {
        r->hooks.extra = (cl_settings *)malloc(sizeof(cl_settings));
        if (r->hooks.extra)
            cfg_init(r->hooks.extra);
    }
    return r->hooks.extra;
}

void pol_hooks_free(cl_repl *r)
{
    if (r->hooks.extra) {
        cfg_free(r->hooks.extra);
        free(r->hooks.extra);
        r->hooks.extra = 0;
    }
}

/* a definition's frontmatter hooks added with an owner tag; Stop as
 * SubagentStop for an agent (Claude Code) */
static void add_front_hooks(cl_repl *r, const char *json, int owner, int agent, const char *where)
{
    cl_settings *x = extra_hooks(r);
    jv o, v, k, val;
    jit it;
    jw j;
    if (!x || r->bare || r->safe || r->cfg.no_hooks || json_parse(json, (long)strlen(json), &o) ||
        json_type(o) != J_OBJ)
        return;
    /* "Stop" in an agent's frontmatter is its SubagentStop */
    jw_init(&j);
    jw_raw(&j, "{", 1);
    json_iter(o, &it);
    while (json_next(&it, &k, &val)) {
        if (j.n > 1)
            jw_raw(&j, ",", 1);
        if (agent && json_streq(k, "Stop"))
            jw_rawz(&j, "\"SubagentStop\"");
        else
            jw_raw(&j, k.p, k.n);
        jw_raw(&j, ":", 1);
        jw_raw(&j, val.p, val.n);
    }
    jw_raw(&j, "}", 1);
    if (!j.oom && json_parse(j.p, j.n, &v) == 0)
        cfg_add_hooks(x, v, CFG_SESSION, owner, where);
    jw_free(&j);
}

/* a skill's hooks: from its first use on, for the rest of the session */
void pol_skill_hooks(cl_repl *r, const cl_def *d)
{
    int i, owner;
    if (!d->hooks)
        return;
    /* the skill's tag: its place in the definitions, + 1 */
    for (i = 0; i < r->defs.n && &r->defs.d[i] != d; i++)
        ;
    owner = i + 1;
    if (r->hooks.extra)
        for (i = 0; i < r->hooks.extra->nhooks; i++)
            if (r->hooks.extra->hooks[i].owner == owner)
                return;             /* registered at an earlier use */
    if (d->src == CFG_PROJECT && r->untrusted)
        return;                     /* a project skill's hooks follow the settings' trust rule */
    add_front_hooks(r, d->hooks, owner, 0, d->path);
}

/* cl_tools.agent_hooks: an agent's hooks while it runs (a project agent's
 * only in a trusted folder: a print-mode run does not count) */
static void pol_agent_hooks(void *u, const cl_agent *a, int run, int on)
{
    cl_repl *r = (cl_repl *)u;
    if (!on) {
        if (r->hooks.extra)
            cfg_drop_owner(r->hooks.extra, -run);
        return;
    }
    if (a->project && !r->trusted_dir) {
        if (r->debug && r->io->log) {
            static const char m[] = "a project agent's frontmatter hooks were skipped: trust the folder first\n";
            r->io->log(r->io->u, m, (long)sizeof(m) - 1);
        }
        return;
    }
    add_front_hooks(r, a->hooks, -run, 1, a->name);
}

static void pol_hook_status(void *u, const char *msg)
{
    cl_repl *r = (cl_repl *)u;
    if (msg)
        ui_status(&r->ui, msg);
    else
        ui_status_clear(&r->ui);
}

static void pol_hook_seen(void *u, int event, const char *cmd, int done, long rc, const char *out, long n)
{
    cl_repl *r = (cl_repl *)u;
    if (r->feed && r->feed->hook)
        r->feed->hook(r->feed->u, cfg_hook_events[event], cmd, done, rc, out, n);
}

/* terminalSequence: the allowed escapes (title, notification, bell) to the console */
static void pol_hook_term(void *u, const char *seq, long n)
{
    cl_repl *r = (cl_repl *)u;
    if (r->io->write && !r->no_person)
        r->io->write(r->io->u, seq, n);
}

void pol_attach_hooks(cl_repl *r)
{
    r->hooks.u = r;
    r->hooks.ask_model = pol_ask_model;
    r->hooks.status = pol_hook_status;
    r->hooks.seen = pol_hook_seen;
    r->hooks.term = pol_hook_term;
    r->hooks.post = pol_hook_post;
    r->hooks.ask_agent = pol_ask_agent;
    r->tools.agent_hooks = pol_agent_hooks;
}

/* "claude-opus-5-5" -> "Opus 5.5" (Claude Code's display name); others as they are */
static const char *model_name(const char *id)
{
    static char b[48];
    static const char *const fam[] = { "opus", "Opus", "sonnet", "Sonnet", "haiku", "Haiku", "fable", "Fable", 0 };
    int i;
    if (strncmp(id, "claude-", 7))
        return id;
    for (i = 0; fam[i]; i += 2) {
        long l = (long)strlen(fam[i]);
        const char *v = id + 7 + l;
        if (!strncmp(id + 7, fam[i], (size_t)l) && v[0] == '-' && v[1] >= '0' && v[1] <= '9' && v[2] == '-' &&
            v[3] >= '0' && v[3] <= '9') {
            cl_copy(b, fam[i + 1], sizeof(b));
            {
                long k = (long)strlen(b);
                b[k] = ' ';
                b[k + 1] = v[1];
                b[k + 2] = '.';
                b[k + 3] = v[3];
                b[k + 4] = 0;
            }
            return b;
        }
    }
    return id;
}

static void key_num(jw *w, const char *key, long v)
{
    jw_raw(w, ",", 1);
    jw_strz(w, key);
    jw_raw(w, ":", 1);
    jw_long(w, v);
}

/* the rest of the status line's JSON after cost.total_cost_usd: the
 * times and lines, the context window, effort, thinking, vim, the agent,
 * the session's name, the version */
static void status_rest(cl_repl *r, jw *ev)
{
    long win = repl_window(r->model), used = r->ctx_used, pct;
    unsigned long now = r->io->ms ? r->io->ms(r->io->u) : 0;
    key_num(ev, "total_duration_ms", (long)(now - r->t_start));
    key_num(ev, "total_api_duration_ms", (long)r->api_ms);
    key_num(ev, "total_lines_added", r->lines_added);
    key_num(ev, "total_lines_removed", r->lines_removed);
    if (used > win)
        used = win;
    pct = (long)(used / (win / 100));
    jw_rawz(ev, "},\"context_window\":{\"context_window_size\":");
    jw_long(ev, win);
    key_num(ev, "total_input_tokens", r->conv.in_tok + r->conv.cache_r + r->conv.cache_w);
    key_num(ev, "total_output_tokens", r->conv.out_tok);
    key_num(ev, "used_percentage", pct);
    key_num(ev, "remaining_percentage", 100 - pct);
    jw_rawz(ev, ",\"current_usage\":{\"input_tokens\":");
    jw_long(ev, r->st.in_tok);
    key_num(ev, "output_tokens", r->st.out_tok);
    key_num(ev, "cache_creation_input_tokens", r->st.cache_w);
    key_num(ev, "cache_read_input_tokens", r->st.cache_r);
    jw_rawz(ev, "}},\"exceeds_200k_tokens\":");
    jw_rawz(ev, r->ctx_used > 200000L ? "true" : "false");
    jw_rawz(ev, ",\"effort\":{\"level\":");
    jw_strz(ev, r->effort);
    jw_rawz(ev, "},\"thinking\":{\"enabled\":");
    jw_rawz(ev, conv_caps(r->model) & CAP_ADAPTIVE ? "true" : "false");
    jw_raw(ev, "}", 1);
    if (r->tui && r->tui->ed.vim) {
        jw_rawz(ev, ",\"vim\":{\"mode\":");
        jw_strz(ev, r->tui->ed.vim == VIM_INSERT ? "INSERT" : "NORMAL");
        jw_raw(ev, "}", 1);
    }
    if (r->agent_name[0]) {
        jw_rawz(ev, ",\"agent\":{\"name\":");
        jw_strz(ev, r->agent_name);
        jw_raw(ev, "}", 1);
    }
    if (r->sess.title[0]) {
        jw_rawz(ev, ",\"session_name\":");
        jw_strz(ev, r->sess.title);
    }
    jw_rawz(ev, ",\"version\":\"1.0.0\"");
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
    jw_strz(&ev, model_name(r->model));
    jw_rawz(&ev, "},\"workspace\":{\"current_dir\":");
    jw_strz(&ev, r->tools.root);
    jw_rawz(&ev, ",\"project_dir\":");
    jw_strz(&ev, r->launch_root[0] ? r->launch_root : r->tools.root);
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
    status_rest(r, &ev);
    jw_rawz(&ev, "}\n");
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
#define IDLE_PROMPT_MS 60000UL   /* Claude Code: idle_prompt after a minute of waiting */

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
    now = r->io->ms ? r->io->ms(r->io->u) : 0;
    if (r->idle_from && !r->idle_told && now - r->idle_from >= IDLE_PROMPT_MS) {
        /* Claude Code's idle_prompt: Claude has waited a minute for the user */
        r->idle_told = 1;
        pol_notify(r, "idle_prompt", "Claude is waiting for your input");
    }
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
    v->no_shell = r->cfg.no_skill_shell;    /* disableSkillShellExecution (bundled skills are not touched) */
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
    const char *st;
    *fork = 0;
    agent[0] = 0;
    if (!d) {
        cl_copy(err, "no such skill", cap);
        return -1;
    }
    st = cfg_skill_state(&r->cfg, d->name);
    if (activate && (!strcmp(st, "off") || !strcmp(st, "user-invocable-only"))) {
        /* Claude Code: skillOverrides keeps it from Claude */
        cl_copy(err, "the skill is not available to Claude: skillOverrides sets it to ", cap);
        cl_cat(err, st, cap);
        return -1;
    }
    if (pol_expand(r, d, args, out, err, cap))
        return -1;
    *fork = d->fork;
    cl_copy(agent, d->agent, acap);
    if (activate) {
        r->n_skills_run++;
        pol_turn_tools(r, d);
        pol_skill_used(r, d);
    }
    return 0;
}

/* ---- /skill-doctor's use counts (A4 gaps 2): <home>/skill-usage.json,
 * {"name": count, ...}, kept across sessions ---- */

static int usage_file(cl_repl *r, char *out, long cap)
{
    return path_join(r->home, "skill-usage.json", out, cap);
}

long pol_skill_count(cl_repl *r, const char *name)
{
    char f[300];
    char *b = 0;
    long n = 0, c = 0;
    jv o, x;
    if (usage_file(r, f, sizeof(f)) || r->sys->kind(r->sys->u, f) != 1 || r->sys->read(r->sys->u, f, 64L * 1024, &b, &n))
        return 0;
    if (json_parse(b, n, &o) == 0 && json_get(o, name, &x))
        c = json_long(x, 0);
    free(b);
    return c;
}

void pol_skill_used(cl_repl *r, const cl_def *d)
{
    char f[300], num[16];
    long c = pol_skill_count(r, d->name) + 1;
    pol_skill_hooks(r, d);          /* its frontmatter's hooks, for the rest of the session */
    if (d->src == DEF_BUILTIN || usage_file(r, f, sizeof(f)))
        return;                     /* Claude Code's report leaves the bundled skills out */
    cl_ltoa(c, num);
    if (r->sys->mkdir)
        r->sys->mkdir(r->sys->u, r->home);
    cfg_write_key(r->sys, f, d->name, num);
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
        a->initial = d->initial && d->initial[0] ? d->initial : 0;
        a->hooks = d->hooks;
        a->memory = d->memory[0] ? d->memory : 0;
        a->color = d->color[0] ? d->color : 0;
        a->project = d->src == CFG_PROJECT;
    }
    /* disable-model-invocation: only the user runs it; skillOverrides: off and
     * user-invocable-only are not listed to Claude, name-only without its text */
    for (i = 0; (d = defs_nth(&r->defs, DEF_SKILL, i)) != 0; i++) {
        const char *st = cfg_skill_state(&r->cfg, d->name);
        if (!d->no_model && (!d->paths || !d->paths[0] || d->active) && strcmp(st, "off") &&
            strcmp(st, "user-invocable-only")) {
            cl_skill *s = &r->x_skills[r->nx_skills++];
            int bare = !strcmp(st, "name-only");
            s->name = d->name;
            s->description = bare ? "" : d->description;
            s->path = d->path;
            s->when = !bare && d->when && d->when[0] ? d->when : 0;
        }
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
    r->tools.agent_start = pol_agent_start;
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
    if (!strcmp(key, "vimInsertModeRemaps"))
        return r->cfg.vim_remaps[0] ? r->cfg.vim_remaps : 0;
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
    if (what & (RW_SUM | RW_SUM_UP))
        return repl_summarize(r, msg[i], (what & RW_SUM_UP) != 0);
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
