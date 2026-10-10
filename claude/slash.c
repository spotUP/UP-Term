/* slash -- the slash command table, and the commands of A4 WP3 (rows 3.3
 * 3.5 3.7 3.8 3.9 3.10): /permissions /output-style /rename /branch
 * /rewind /autocompact /status /usage /export /config /memory /add-dir
 * /doctor /statusline /terminal-setup /login /logout /agents /skills
 * /commands /hooks /tasks /todos /cd, and the custom commands of
 * .claude/commands. The A2/A3 commands stay in repl.c.
 * Portable C89, host-tested through the REPL suite. */
#include <stdlib.h>
#include <string.h>
#include "repl_int.h"
#include "tui.h"
#include "path.h"
#include "util.h"
#include "setup.h"

const cl_cmd slash_builtin[] = {
    { "/help", "Show the commands and the keys" },
    { "/add-dir", "Let Claude work in one more directory: /add-dir DIR" },
    { "/advisor", "A stronger model Claude consults at key moments: /advisor fable|opus|sonnet|off" },
    { "/agents", "The subagents (.claude/agents)" },
    { "/autocompact", "Compact by itself: on, off, auto, or a window (500k, 1M)" },
    { "/branch", "Go on in a copy of this conversation (a new session)" },
    { "/btw", "A side question about this conversation, not added to it: /btw QUESTION" },
    { "/cd", "Change the start directory: /cd DIR" },
    { "/clear", "Start a new conversation (clears the screen): /clear [name for the old one]" },
    { "/color", "The prompt bar's color for this session: /color [red|blue|green|yellow|purple|orange|pink|cyan|default]" },
    { "/commands", "The custom commands (.claude/commands)" },
    { "/compact", "Summarise the conversation and go on from it: /compact [what to keep]" },
    { "/config", "The settings: /config [key=value ...] or /config KEY VALUE [user|project|local]" },
    { "/context", "How much of the context window is in use" },
    { "/copy", "Claude's last answer (or the Nth last: /copy N) to the clipboard" },
    { "/cost", "Tokens, cost and time so far (as /usage)" },
    { "/debug", "Debug logging on for this session; where the log is" },
    { "/diff", "The changes Claude made to files in this session" },
    { "/doctor", "Check the machine: the network, AmiSSL, the key, the settings" },
    { "/effort", "Show or set the effort: low, medium, high, xhigh, max, auto, status" },
    { "/exit", "Leave" },
    { "/export", "The conversation as text: to the clipboard, or /export FILE" },
    { "/goal", "Claude keeps working until a condition holds: /goal CONDITION, /goal clear" },
    { "/hooks", "The hooks of the settings" },
    { "/init", "Write AMIGA.md: notes on this directory for later sessions" },
    { "/keybindings", "Open the keyboard shortcuts file (ENVARC:Claude/keybindings.json)" },
    { "/login", "Store an API key in ENVARC:Claude/key" },
    { "/logout", "Remove the stored API key" },
    { "/memory", "Edit a memory file (CLAUDE.md) in the editor" },
    { "/model", "Set the model, kept for new sessions (opus, sonnet, haiku, fable, or an id)" },
    { "/output-style", "Choose the output style: Default, Explanatory, Learning, yours" },
    { "/permissions", "The permission rules: /permissions [allow|ask|deny|remove RULE]" },
    { "/plan", "Plan mode on (Claude looks, changes nothing): /plan [what to plan]" },
    { "/recap", "A one-line summary of this session" },
    { "/release-notes", "What is new in C:Claude" },
    { "/reload-skills", "Read the skills and the commands again" },
    { "/rename", "Name this conversation: /rename [NAME] (none: Claude names it)" },
    { "/resume", "Go on with an earlier conversation of this directory" },
    { "/rewind", "Go back to an earlier prompt: the files, the conversation, both, or a summary" },
    { "/setup", "The setup wizard: Claude Code on another computer, or an API key here" },
    { "/save", "Save the conversation as JSON: /save FILE" },
    { "/skills", "The skills (.claude/skills)" },
    { "/skill-doctor", "What each skill costs in context and how often it was used" },
    { "/stats", "Tokens, cost and time so far (as /usage)" },
    { "/status", "The model, the session, the account, the settings in use" },
    { "/statusline", "Set up the status line: /statusline WHAT YOU WANT, or clear" },
    { "/tasks", "The commands running in the background" },
    { "/terminal-setup", "Check the terminal: UP-Term's keys and size" },
    { "/theme", "Change the colours" },
    { "/todos", "The todo list" },
    { "/usage", "Tokens, cost per model, time, and the context window" },
    { "/vim", "Vim editing in the input box, on or off" }
};
const int slash_nbuiltin = (int)(sizeof(slash_builtin) / sizeof(slash_builtin[0]));

static void line2(cl_repl *r, const char *a, const char *b)
{
    repl_say(r, a, b);
}

static void num_line(cl_repl *r, const char *a, long v, const char *b)
{
    char m[300], num[16];
    cl_copy(m, a, sizeof(m));
    cl_ltoa(v, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, b, sizeof(m));
    ui_line(&r->ui, m);
}

static const char *src_word(int src)
{
    return src == CFG_USER ? "user" : src == CFG_PROJECT ? "project" : src == CFG_LOCAL ? "local"
           : src == DEF_BUILTIN ? "built-in" : "session";
}

/* "user" / "project" / "local" at the end of an argument: the level (and
 * the argument shortened), else def */
static int level_of(char *arg, int def)
{
    long n = (long)strlen(arg);
    static const char *const w[3] = { "user", "project", "local" };
    int i;
    for (i = 0; i < 3; i++) {
        long l = (long)strlen(w[i]);
        if (n > l && arg[n - l - 1] == ' ' && !strcmp(arg + n - l, w[i])) {
            arg[n - l - 1] = 0;
            while (n && arg[strlen(arg) - 1] == ' ')
                arg[strlen(arg) - 1] = 0;
            return i;
        }
    }
    return def;
}

/* ---- /permissions ---- */

static void permissions(cl_repl *r, const char *a)
{
    char arg[400], verb[16];
    int kind = -1, lvl, i;
    const char *rest;
    cl_copy(arg, a, sizeof(arg));
    for (rest = arg; *rest && *rest != ' '; rest++)
        ;
    cl_copy(verb, arg, (long)(rest - arg) + 1 < (long)sizeof(verb) ? (long)(rest - arg) + 1 : (long)sizeof(verb));
    while (*rest == ' ')
        rest++;
    if (!strcmp(verb, "allow"))
        kind = RULE_ALLOW;
    else if (!strcmp(verb, "ask"))
        kind = RULE_ASK;
    else if (!strcmp(verb, "deny"))
        kind = RULE_DENY;
    if (kind >= 0 || !strcmp(verb, "remove")) {
        char rule[300], t[64], p[300];
        cl_copy(rule, rest, sizeof(rule));
        lvl = level_of(rule, CFG_LOCAL);
        if (!rule[0] || cfg_rule_parse(rule, t, sizeof(t), p, sizeof(p))) {
            ui_line(&r->ui, "A rule is Tool or Tool(pattern), e.g. Bash(make *), Edit(src/**), Read.");
            return;
        }
        if (kind >= 0) {
            if (cfg_write_rule(r->sys, cfg_file(&r->cfg, lvl), kind, rule, 1)) {
                line2(r, "Cannot write the rule to ", cfg_file(&r->cfg, lvl));
                return;
            }
            cfg_add_rule(&r->cfg, kind, lvl, rule);
            pol_tools(r);           /* a deny of WebSearch takes the server tool away */
            line2(r, kind == RULE_ALLOW ? "Allow: " : kind == RULE_ASK ? "Ask first: " : "Deny: ", rule);
            line2(r, "  saved in ", cfg_file(&r->cfg, lvl));
            return;
        }
        {
            int gone = 0, k, s;
            for (s = 0; s < 3; s++)
                for (k = RULE_ALLOW; k <= RULE_DENY; k++)
                    if (cfg_write_rule(r->sys, cfg_file(&r->cfg, s), k, rule, 0) == 0)
                        gone++;
            for (i = r->cfg.nrules - 1; i >= 0; i--)
                if (!strcmp(r->cfg.rules[i].text, rule)) {
                    free(r->cfg.rules[i].text);
                    memmove(&r->cfg.rules[i], &r->cfg.rules[i + 1],
                            (size_t)(r->cfg.nrules - i - 1) * sizeof(cl_rule));
                    r->cfg.nrules--;
                    gone++;
                }
            pol_tools(r);
            line2(r, gone ? "Removed the rule " : "No such rule: ", rule);
        }
        return;
    }
    if (arg[0]) {
        ui_line(&r->ui, "Usage: /permissions [allow|ask|deny RULE [user|project|local]] or /permissions remove RULE");
        return;
    }
    ui_line(&r->ui, "Permission rules (deny wins over ask, ask over allow):");
    if (!r->cfg.nrules)
        ui_line(&r->ui, "  none. Add one: /permissions allow Bash(make *)");
    for (kind = RULE_DENY; kind >= RULE_ALLOW; kind--)
        for (i = 0; i < r->cfg.nrules; i++)
            if (r->cfg.rules[i].kind == kind) {
                char m[400];
                cl_copy(m, "  ", sizeof(m));
                cl_cat(m, cfg_kind_name(kind), sizeof(m));
                cl_cat(m, "  ", sizeof(m));
                cl_cat(m, r->cfg.rules[i].text, sizeof(m));
                cl_cat(m, "  (", sizeof(m));
                cl_cat(m, src_word(r->cfg.rules[i].src), sizeof(m));
                cl_cat(m, ")", sizeof(m));
                ui_line(&r->ui, m);
            }
    if (r->tools.perm.session) {
        char m[300];
        int t;
        cl_copy(m, "  allowed for this session by an answer:", sizeof(m));
        for (t = 0; t < T_COUNT; t++)
            if (r->tools.perm.session & (1u << t)) {
                cl_cat(m, " ", sizeof(m));
                cl_cat(m, tools_name(t), sizeof(m)); /* Claude Code's tool names (A4 WP2) */
            }
        ui_line(&r->ui, m);
    }
    for (i = 0; i < r->cfg.ndirs; i++)
        line2(r, "  additional directory: ", r->cfg.dirs[i]);
}

/* ---- /output-style ---- */

static void output_style(cl_repl *r, const char *arg)
{
    const cl_def *d;
    const char *opt[24];
    int n = 0, i, c, cur = 0;
    char v[96];
    if (*arg)
        d = defs_find(&r->defs, DEF_STYLE, arg);
    else {
        for (i = 0; i < 24 && defs_nth(&r->defs, DEF_STYLE, i); i++) {
            opt[n] = defs_nth(&r->defs, DEF_STYLE, i)->name;
            if (cl_strieq(opt[n], r->style[0] ? r->style : "Default"))
                cur = n;
            n++;
        }
        c = ui_pick(&r->ui, "Choose the output style", opt, n, cur);
        if (c < 0) {
            ui_line(&r->ui, "Output styles (/output-style NAME):");
            for (i = 0; i < n; i++) {
                char m[300];
                const cl_def *s = defs_nth(&r->defs, DEF_STYLE, i);
                cl_copy(m, i == cur ? "* " : "  ", sizeof(m));
                cl_cat(m, s->name, sizeof(m));
                cl_cat(m, "  ", sizeof(m));
                cl_cat(m, s->description, sizeof(m));
                ui_line(&r->ui, m);
            }
            return;
        }
        d = defs_nth(&r->defs, DEF_STYLE, c);
    }
    if (!d) {
        line2(r, "No output style named ", arg);
        return;
    }
    cl_copy(r->style, cl_strieq(d->name, "Default") ? "" : d->name, sizeof(r->style));
    repl_system(r);
    /* kept for this project, as Claude Code keeps it */
    cl_copy(v, "\"", sizeof(v));
    cl_cat(v, d->name, sizeof(v));
    cl_cat(v, "\"", sizeof(v));
    cfg_write_key(r->sys, cfg_file(&r->cfg, CFG_LOCAL), "outputStyle", v);
    line2(r, "Output style: ", d->name);
}

/* ---- /status /usage /doctor /terminal-setup ---- */

static void status(cl_repl *r)
{
    char m[400], num[16];
    int i;
    ui_line(&r->ui, "C:Claude for AmigaOS (A4), Claude Code's features as far as an Amiga allows.");
    line2(r, "Session: ", r->sess.id);
    if (r->sess.title[0])
        line2(r, "  named: ", r->sess.title);
    line2(r, "Start directory: ", r->tools.root);
    for (i = 0; i < r->cfg.ndirs; i++)
        line2(r, "  and: ", r->cfg.dirs[i]);
    cl_copy(m, r->model, sizeof(m));
    cl_cat(m, ", effort ", sizeof(m));
    cl_cat(m, r->effort, sizeof(m));
    if (r->fallback[0]) {
        cl_cat(m, ", fallback ", sizeof(m));
        cl_cat(m, r->fallback, sizeof(m));
    }
    line2(r, "Model: ", m);
    line2(r, "Permission mode: ", tui_mode_names[r->tools.perm.mode]);
    line2(r, "Output style: ", r->style[0] ? r->style : "Default");
    cl_copy(m, r->url.host, sizeof(m));
    cl_cat(m, r->url.tls ? " (https)" : " (plain http: no key is sent)", sizeof(m));
    line2(r, "API: ", m);
    line2(r, "API key: ", r->key && *r->key ? "set" : r->url.tls ? "none (/login)" : "not needed here");
    for (i = 0; i < 3; i++)
        if (r->cfg.found[i])
            line2(r, "Settings: ", r->cfg.path[i]);
    for (i = 0; i < r->mem.n; i++)
        line2(r, "Memory: ", r->mem.f[i].path);
    cl_copy(m, "Rules ", sizeof(m));
    cl_ltoa(r->cfg.nrules, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, ", hooks ", sizeof(m));
    cl_ltoa(r->cfg.nhooks, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, ", custom commands ", sizeof(m));
    cl_ltoa(defs_count(&r->defs, DEF_COMMAND), num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, ", agents ", sizeof(m));
    cl_ltoa(defs_count(&r->defs, DEF_AGENT), num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, ", skills ", sizeof(m));
    cl_ltoa(defs_count(&r->defs, DEF_SKILL), num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, ".", sizeof(m));
    ui_line(&r->ui, m);
    line2(r, "Auto-compact: ", r->auto_compact ? "on" : "off");
}

static void check(cl_repl *r, int ok, const char *what, const char *detail)
{
    char m[400];
    cl_copy(m, ok > 0 ? "[OK]    " : ok == 0 ? "[ERROR] " : "[INFO]  ", sizeof(m));
    cl_cat(m, what, sizeof(m));
    if (detail && *detail) {
        cl_cat(m, ": ", sizeof(m));
        cl_cat(m, detail, sizeof(m));
    }
    ui_line(&r->ui, m);
}

static void doctor(cl_repl *r)
{
    char m[200];
    int ok;
    if (r->sys->info) {
        ok = r->sys->info(r->sys->u, "os", m, sizeof(m));
        check(r, ok, "System", m);
        ok = r->sys->info(r->sys->u, "bsdsocket", m, sizeof(m));
        check(r, ok, "TCP/IP stack", m);
        if (r->url.tls) {
            ok = r->sys->info(r->sys->u, "amissl", m, sizeof(m));
            check(r, ok, "AmiSSL (https)", m);
        }
        ok = r->sys->info(r->sys->u, "vsh", m, sizeof(m));
        check(r, ok, "Shell for commands", m);
    }
    cl_copy(m, r->url.host, sizeof(m));
    check(r, 1, "API host", m);
    if (r->url.tls)
        check(r, r->key && *r->key ? 1 : 0, "API key",
              r->key && *r->key ? "set" : "none: /login, ENV:ANTHROPIC_API_KEY or ENVARC:Claude/key");
    check(r, r->cfg.err[0] ? 0 : 1, "Settings", r->cfg.err[0] ? r->cfg.err : "every file read is valid JSON");
    if (r->cfg.nwarn) {
        /* Claude Code's doctor lists the entries the settings skipped */
        num_line(r, "  Settings entries skipped: ", r->cfg.nwarn, "");
        ui_line(&r->ui, r->cfg.warn);
    }
    if (r->untrusted)
        check(r, 0, "Workspace trust", "this folder is not trusted: its hooks and allow rules wait (start Claude "
                                       "here and answer the trust question)");
    check(r, r->sys->kind(r->sys->u, r->home) == 2 ? 1 : -1, "User directory",
          r->sys->kind(r->sys->u, r->home) == 2 ? r->home : "not there yet (made at the first save)");
    check(r, r->tui ? 1 : -1, "Screen", r->tui ? "UP-Term, raw mode" : "line mode (PLAIN, or a console that is not UP-Term's)");
    check(r, -1, "Network check", "Claude PING sends one request of one token and times it");
}

static void terminal_setup(cl_repl *r)
{
    int cols = 0, rows = 0;
    char m[200], num[16];
    if (r->io->size && r->io->size(r->io->u, &cols, &rows) == 0) {
        cl_copy(m, "", sizeof(m));
        cl_ltoa(cols, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, " x ", sizeof(m));
        cl_ltoa(rows, num);
        cl_cat(m, num, sizeof(m));
        check(r, cols >= 80 ? 1 : 0, "Window size", cols >= 80 ? m : "narrower than 80 columns: widen the window");
    } else
        check(r, 0, "Window size", "the console does not say (not UP-Term?)");
    check(r, r->tui ? 1 : 0, "Raw keys", r->tui ? "UP-Term's line discipline: Shift+Enter, Esc, Shift+Tab work"
                                                : "not available: the line mode is used (start Claude in UP-Term)");
    check(r, -1, "New line in a prompt", "Shift+Enter; or \\ then Enter, or Ctrl+J, in any console");
    check(r, -1, "Colours", "UP-Term: Settings > Colours, 256 colours or true colour for the diffs");
}

/* ---- /export ---- */

static void transcript(cl_repl *r, jw *w)
{
    int i;
    for (i = 0; i < r->conv.n; i++) {
        jv v, b, x;
        jit it;
        if (json_parse(r->conv.m[i].json, r->conv.m[i].n, &v))
            continue;
        json_iter(v, &it);
        while (json_next(&it, 0, &b)) {
            long l;
            char *t;
            if (!json_get(b, "type", &x))
                continue;
            if (json_streq(x, "text") && json_get(b, "text", &x) && (t = json_strdup(x, &l)) != 0) {
                jw_rawz(w, r->conv.m[i].user ? "> " : "");
                jw_raw(w, t, l);
                jw_rawz(w, "\n\n");
                free(t);
            } else if (json_streq(x, "tool_use") && json_get(b, "name", &x) && (t = json_strdup(x, &l)) != 0) {
                jv in;
                jw_rawz(w, "* ");
                jw_rawz(w, cfg_cc_tool(t));
                if (json_get(b, "input", &in)) {
                    jw_raw(w, "(", 1);
                    jw_raw(w, in.p, in.n > 200 ? 200 : in.n);
                    jw_raw(w, ")", 1);
                }
                jw_rawz(w, "\n\n");
                free(t);
            }
        }
    }
}

static void export_(cl_repl *r, const char *arg)
{
    jw w;
    jw_init(&w);
    transcript(r, &w);
    if (!w.n) {
        jw_free(&w);
        ui_line(&r->ui, "Nothing to export yet.");
        return;
    }
    if (*arg) {
        char full[512];
        if (path_join(r->tools.root, arg, full, sizeof(full)) || r->sys->write(r->sys->u, full, w.p, w.n))
            line2(r, "Cannot write the export: ", r->sys->err(r->sys->u));
        else
            line2(r, "The conversation is in ", full);
    } else if (r->sys->clip && r->sys->clip(r->sys->u, w.p, w.n) == 0)
        ui_line(&r->ui, "The conversation is on the clipboard.");
    else
        ui_line(&r->ui, "No clipboard here: /export FILE writes it to a file.");
    jw_free(&w);
}

/* ---- /config ---- */

static const char *const cfg_keys[] = { "model", "effortLevel", "outputStyle", "autoCompactEnabled", "theme",
                                        "fallbackModel", "webSearch", "editorMode", "verbose", "autoMemoryEnabled",
                                        "autoCompactWindow" };
#define NCFG_KEYS ((int)(sizeof(cfg_keys) / sizeof(cfg_keys[0])))

static void apply_key(cl_repl *r, const char *key, const char *val)
{
    if (!strcmp(key, "model"))
        cl_copy(r->model, cfg_model(val), sizeof(r->model));
    else if (!strcmp(key, "effortLevel") || !strcmp(key, "effort"))
        cl_copy(r->effort, val, sizeof(r->effort));
    else if (!strcmp(key, "outputStyle")) {
        cl_copy(r->style, cl_strieq(val, "default") ? "" : val, sizeof(r->style));
        repl_system(r);
    } else if (!strcmp(key, "autoCompactEnabled"))
        r->auto_compact = !strcmp(val, "true");
    else if (!strcmp(key, "theme"))
        cl_copy(r->cfg.theme, val, sizeof(r->cfg.theme));
    else if (!strcmp(key, "fallbackModel"))
        cl_copy(r->fallback, cfg_model(val), sizeof(r->fallback));
    else if (!strcmp(key, "webSearch")) {
        r->cfg.web_search = strcmp(val, "false") != 0;
        pol_tools(r);
    } else if (!strcmp(key, "editorMode")) {
        cl_copy(r->cfg.editor_mode, val, sizeof(r->cfg.editor_mode));
        if (r->tui)
            ed_set_vim(&r->tui->ed, !strcmp(val, "vim"));
    } else if (!strcmp(key, "verbose")) {
        if (r->verbose != 2)
            r->verbose = !strcmp(val, "true");
    } else if (!strcmp(key, "autoMemoryEnabled")) {
        r->cfg.auto_memory = !strcmp(val, "true");
        repl_load_memory(r);
    } else if (!strcmp(key, "autoCompactWindow")) {
        long t = cfg_window_parse(val);
        r->cfg.compact_window = t > 0 ? t : 0;
    }
}

/* KEY VALUE written to the file of a level, as JSON (true, false, a
 * number or a string) */
static void config_set(cl_repl *r, const char *key, const char *val, int lvl)
{
    jw v;
    const char *p;
    int num = *val != 0;
    for (p = val; *p; p++)
        if (*p < '0' || *p > '9')
            num = 0;
    jw_init(&v);
    if (!strcmp(val, "true") || !strcmp(val, "false") || num)
        jw_rawz(&v, val);
    else
        jw_strz(&v, val);
    if (v.oom || cfg_write_key(r->sys, cfg_file(&r->cfg, lvl), key, v.p))
        line2(r, "Cannot write ", cfg_file(&r->cfg, lvl));
    else {
        char m[300];
        apply_key(r, key, val);
        cl_copy(m, key, sizeof(m));
        cl_cat(m, " = ", sizeof(m));
        cl_cat(m, v.p, sizeof(m));
        cl_cat(m, "  (", sizeof(m));
        cl_cat(m, cfg_file(&r->cfg, lvl), sizeof(m));
        cl_cat(m, ")", sizeof(m));
        ui_line(&r->ui, m);
    }
    jw_free(&v);
}

static void config(cl_repl *r, const char *a)
{
    char arg[400], *val;
    int lvl, c;
    cl_copy(arg, a, sizeof(arg));
    if (arg[0] && strchr(arg, '=')) {
        /* Claude Code's form: /config key=value [key=value ...], the user's settings */
        char *p = arg;
        while (*p) {
            char *k, *e, *v;
            while (*p == ' ')
                p++;
            if (!*p)
                break;
            k = p;
            while (*p && *p != ' ')
                p++;
            if (*p)
                *p++ = 0;
            e = strchr(k, '=');
            if (!e || e == k) {
                line2(r, "Not key=value: ", k);
                continue;
            }
            *e = 0;
            v = e + 1;
            config_set(r, k, v, CFG_USER);
        }
        return;
    }
    if (arg[0]) {
        lvl = level_of(arg, CFG_USER);
        for (val = arg; *val && *val != ' '; val++)
            ;
        if (!*val) {
            ui_line(&r->ui, "Usage: /config KEY VALUE [user|project|local]");
            return;
        }
        *val++ = 0;
        while (*val == ' ')
            val++;
        config_set(r, arg, val, lvl);
        return;
    }
    c = ui_pick(&r->ui, "Settings: choose one to change", cfg_keys, NCFG_KEYS, 0);
    if (c >= 0) {
        static const char *const onoff[] = { "true", "false" };
        static const char *const themes[] = { "dark", "light", "dark-ansi", "light-ansi" };
        static const char *const mods[] = { "opus", "sonnet", "haiku", "fable" };
        static const char *const effs[] = { "low", "medium", "high", "xhigh", "max", "auto" };
        static const char *const eds[] = { "normal", "vim" };
        static const char *const wins[] = { "200000", "500000", "1000000" };
        int yn = c == 3 || c == 6 || c == 8 || c == 9;  /* the true/false ones */
        const char *const *opt = yn ? onoff : c == 4 ? themes : c == 1 ? effs : c == 7 ? eds : c == 10 ? wins : mods;
        int n = yn ? 2 : c == 4 ? 4 : c == 1 ? 6 : c == 7 ? 2 : c == 10 ? 3 : 4, v;
        if (c == 2) {
            output_style(r, "");
            return;
        }
        v = ui_pick(&r->ui, cfg_keys[c], opt, n, 0);
        if (v >= 0)
            config_set(r, cfg_keys[c], opt[v], CFG_USER);
        return;
    }
    {
        int i;
        ui_line(&r->ui, "Settings in use (/config KEY VALUE [user|project|local] changes one):");
        for (i = 0; i < 3; i++)
            line2(r, r->cfg.found[i] ? "  read:    " : "  missing: ", r->cfg.path[i]);
        line2(r, "  model: ", r->model);
        line2(r, "  effortLevel: ", r->effort);
        line2(r, "  outputStyle: ", r->style[0] ? r->style : "Default");
        line2(r, "  autoCompactEnabled: ", r->auto_compact ? "true" : "false");
        line2(r, "  theme: ", r->cfg.theme[0] ? r->cfg.theme : "(the screen's own)");
        line2(r, "  fallbackModel: ", r->fallback[0] ? r->fallback : "(none)");
        line2(r, "  statusLine: ", r->cfg.status_cmd[0] ? r->cfg.status_cmd : "(none)");
        line2(r, "  webSearch: ", r->tools.web_search ? "true" : "false (or a deny rule for WebSearch)");
        line2(r, "  permissions.defaultMode: ", r->cfg.default_mode[0] ? r->cfg.default_mode : "default");
        line2(r, "  editorMode: ", r->tui && r->tui->ed.vim ? "vim" : "normal");
        line2(r, "  verbose: ", r->verbose ? "true" : "false");
        line2(r, "  autoMemoryEnabled: ", r->cfg.auto_memory == 0 ? "false" : "true");
        num_line(r, "  autoCompactWindow: ", repl_compact_at(r), " tokens");
    }
}

/* ---- /memory ---- */

/* a file in the user's editor: Ctrl+G's launcher with the screen (a
 * console editor works too), else $EDITOR (Ed) run as a command */
static void edit_file(cl_repl *r, const char *file)
{
    char ed[200], line[600], *o;
    long on = 0, rc = 0;
    if (r->io->edit) {
        int bad;
        line2(r, "Editing ", file);
        if (r->io->raw && r->tui)
            r->io->raw(r->io->u, 0);
        bad = r->io->edit(r->io->u, file);
        if (r->io->raw && r->tui)
            r->io->raw(r->io->u, 1);
        if (r->tui)
            tui_redraw(r->tui);
        if (bad)
            line2(r, "The editor did not run for ", file);
        return;
    }
    if (!r->sys->getenv || r->sys->getenv(r->sys->u, "EDITOR", ed, sizeof(ed)) <= 0)
        cl_copy(ed, "Ed", sizeof(ed));
    cl_copy(line, ed, sizeof(line));
    cl_cat(line, " \"", sizeof(line));
    cl_cat(line, file, sizeof(line));
    cl_cat(line, "\"", sizeof(line));
    o = (char *)malloc(1024);
    if (!o)
        return;
    line2(r, "Editing ", file);
    if (r->sys->run(r->sys->u, line, 3600, o, 1023, &on, &rc) < 0 || rc >= 10)
        line2(r, "The editor did not run: ", line);
    free(o);
}

static void memory(cl_repl *r, const char *arg)
{
    static const char *const opt[] = { "User memory (ENVARC:Claude/CLAUDE.md)", "Project memory (CLAUDE.md)",
                                       "Local project memory (CLAUDE.local.md, private)" };
    static const int kinds[] = { MEM_USER, MEM_PROJECT, MEM_LOCAL };
    char file[300];
    int c = !strcmp(arg, "user") ? 0 : !strcmp(arg, "project") ? 1 : !strcmp(arg, "local") ? 2 : -1;
    int i;
    if (!strcmp(arg, "auto on") || !strcmp(arg, "auto off")) {
        /* Claude Code's /memory switches auto memory too */
        config_set(r, "autoMemoryEnabled", arg[6] == 'n' ? "true" : "false", CFG_USER);
        line2(r, "Auto memory: ", r->mem.auto_dir[0] ? r->mem.auto_dir : "off");
        return;
    }
    if (c < 0 && !*arg)
        c = ui_pick(&r->ui, "Edit which memory file?", opt, 3, 1);
    if (c < 0) {
        ui_line(&r->ui, "Memory files in use:");
        for (i = 0; i < r->mem.n; i++)
            line2(r, "  ", r->mem.f[i].path);
        if (r->mem.auto_dir[0])
            line2(r, "  auto memory: ", r->mem.auto_dir);
        ui_line(&r->ui, "/memory user, /memory project or /memory local opens one in the editor; /memory auto on|off.");
        return;
    }
    mem_file_of(kinds[c], r->home, r->tools.root, file, sizeof(file));
    if (r->sys->kind(r->sys->u, file) == 0) {
        static const char head[] = "# Memory\n\nNotes for Claude: what to keep in mind here.\n";
        if (kinds[c] == MEM_USER && r->sys->mkdir)
            r->sys->mkdir(r->sys->u, r->home);
        r->sys->write(r->sys->u, file, head, (long)sizeof(head) - 1);
    }
    edit_file(r, file);
    repl_load_memory(r);
    num_line(r, "Memory read again: ", r->mem.n, r->mem.n == 1 ? " file." : " files.");
}

/* ---- /keybindings (A4 gaps 3) ---- */

/* Claude Code: creates the file (with the default bindings) when there is
 * none, opens it in the editor; the bindings apply when it is saved */
static void keybindings(cl_repl *r)
{
    char file[300];
    if (repl_keys_file(r, file, sizeof(file)))
        return;
    if (r->sys->kind(r->sys->u, file) == 0) {
        jw d;
        jw_init(&d);
        km_defaults_json(&d);
        if (r->sys->mkdir)
            r->sys->mkdir(r->sys->u, r->home);
        if (d.oom || r->sys->write(r->sys->u, file, d.p, d.n)) {
            jw_free(&d);
            line2(r, "Cannot write ", file);
            return;
        }
        jw_free(&d);
        line2(r, "Created with the default bindings: ", file);
    }
    edit_file(r, file);
    if (!r->tui)
        return;
    repl_keys_load(r);
    if (r->safe)
        ui_line(&r->ui, "Safe mode: the bindings in the file are not used in this session.");
    else if (r->tui->km.nwarn) {
        char m[120], num[16];
        cl_copy(m, "Keybindings read again, with ", sizeof(m));
        cl_ltoa(r->tui->km.nwarn, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, r->tui->km.nwarn == 1 ? " problem (/debug for the log)." : " problems (/debug for the log).",
               sizeof(m));
        ui_line(&r->ui, m);
    } else
        ui_line(&r->ui, "Keybindings read again.");
}

/* ---- /login /logout ---- */

static void login(cl_repl *r, const char *arg)
{
    char file[300];
    if (!*arg) {
        r->await_key = 1;
        ui_line(&r->ui, "Paste the API key (from console.anthropic.com) and press Enter. It is stored in "
                        "ENVARC:Claude/key and never shown.");
        return;
    }
    cl_copy(r->keybuf, arg, sizeof(r->keybuf));
    if (cl_key_clean(r->keybuf)) {
        memset(r->keybuf, 0, sizeof(r->keybuf));
        ui_line(&r->ui, "That is not a usable key (no spaces, at least 8 characters).");
        return;
    }
    if (path_join(r->home, "key", file, sizeof(file)) == 0) {
        if (r->sys->mkdir)
            r->sys->mkdir(r->sys->u, r->home);
        if (r->sys->write(r->sys->u, file, r->keybuf, (long)strlen(r->keybuf)))
            line2(r, "The key is used now but could not be stored: ", r->sys->err(r->sys->u));
        else
            line2(r, "The key is stored in ", file);
    }
    if (r->url.tls)
        r->key = r->keybuf;
}

static void logout(cl_repl *r)
{
    char file[300];
    if (path_join(r->home, "key", file, sizeof(file)) == 0 && r->sys->kind(r->sys->u, file) == 1 &&
        (!r->sys->remove || r->sys->remove(r->sys->u, file))) {
        line2(r, "Cannot remove ", file);
        return;
    }
    memset(r->keybuf, 0, sizeof(r->keybuf));
    r->key = 0;
    ui_line(&r->ui, "Logged out: the stored key is gone. (ENV:ANTHROPIC_API_KEY, if set, counts at the next start.)");
}

/* ---- the definition lists ---- */

static void list_defs(cl_repl *r, int type, const char *none)
{
    int i;
    const cl_def *d;
    for (i = 0; (d = defs_nth(&r->defs, type, i)) != 0; i++) {
        char m[400];
        cl_copy(m, type == DEF_COMMAND ? "  /" : "  ", sizeof(m));
        cl_cat(m, d->name, sizeof(m));
        cl_cat(m, " (", sizeof(m));
        cl_cat(m, src_word(d->src), sizeof(m));
        cl_cat(m, ")  ", sizeof(m));
        cl_cat(m, d->description, sizeof(m));
        if (d->model[0]) {
            cl_cat(m, "  [model ", sizeof(m));
            cl_cat(m, d->model, sizeof(m));
            cl_cat(m, "]", sizeof(m));
        }
        ui_line(&r->ui, m);
    }
    if (!i)
        ui_line(&r->ui, none);
}

static void hooks_list(cl_repl *r)
{
    int i;
    if (!r->cfg.nhooks) {
        ui_line(&r->ui, "No hooks. They are set in settings.json under \"hooks\", as in Claude Code.");
        return;
    }
    for (i = 0; i < r->cfg.nhooks; i++) {
        char m[500];
        const cl_hook *h = &r->cfg.hooks[i];
        cl_copy(m, "  ", sizeof(m));
        cl_cat(m, cfg_hook_events[h->event], sizeof(m));
        if (h->matcher[0]) {
            cl_cat(m, " [", sizeof(m));
            cl_cat(m, h->matcher, sizeof(m));
            cl_cat(m, "]", sizeof(m));
        }
        cl_cat(m, "  ", sizeof(m));
        cl_cat(m, h->cmd, sizeof(m));
        cl_cat(m, "  (", sizeof(m));
        cl_cat(m, src_word(h->src), sizeof(m));
        cl_cat(m, ")", sizeof(m));
        ui_line(&r->ui, m);
    }
}

static void todos(cl_repl *r)
{
    jv v, arr, e, x;
    jit it;
    if (!r->todos || json_parse(r->todos, (long)strlen(r->todos), &v) || !json_get(v, "todos", &arr)) {
        ui_line(&r->ui, "No todo list in this conversation.");
        return;
    }
    json_iter(arr, &it);
    while (json_next(&it, 0, &e)) {
        char m[300], c[240];
        c[0] = 0;
        if (json_get(e, "content", &x))
            json_str(x, c, sizeof(c));
        cl_copy(m, "  [ ] ", sizeof(m));
        if (json_get(e, "status", &x)) {
            if (json_streq(x, "completed"))
                m[3] = 'x';
            else if (json_streq(x, "in_progress"))
                m[3] = '>';
        }
        cl_cat(m, c, sizeof(m));
        ui_line(&r->ui, m);
    }
}

/* ---- /add-dir /cd ---- */

static int dir_of(cl_repl *r, const char *arg, char *out, long cap)
{
    char p[512];
    if (!*arg || path_join(r->tools.root, arg, p, sizeof(p)) || r->sys->kind(r->sys->u, p) != 2) {
        line2(r, "Not a directory: ", arg);
        return -1;
    }
    if (r->sys->canon(r->sys->u, p, out, cap))
        cl_copy(out, p, cap);
    return 0;
}

static void add_dir(cl_repl *r, const char *arg)
{
    char d[300];
    char **q;
    if (dir_of(r, arg, d, sizeof(d)))
        return;
    q = (char **)realloc(r->cfg.dirs, (size_t)(r->cfg.ndirs + 1) * sizeof(char *));
    if (!q)
        return;
    r->cfg.dirs = q;
    r->cfg.capdirs = r->cfg.ndirs + 1;
    q[r->cfg.ndirs] = (char *)malloc(strlen(d) + 1);
    if (!q[r->cfg.ndirs])
        return;
    strcpy(q[r->cfg.ndirs++], d);
    line2(r, "Claude may now read (and, in accept-edits mode, edit) in ", d);
    pol_dir_added(r, d);
}

static void cd(cl_repl *r, const char *arg)
{
    char d[300], old[256];
    if (dir_of(r, arg, d, sizeof(d)))
        return;
    cl_copy(old, r->tools.root, sizeof(old));
    pol_cwd_changed(r, old, d);     /* the hooks of the directory being left (the new one's are read next) */
    cl_copy(r->tools.root, d, sizeof(r->tools.root));
    repl_load(r);
    line2(r, "Start directory: ", r->tools.root);
}

/* ---- /rename /branch /rewind /autocompact ---- */

static void rewind_(cl_repl *r, const char *arg)
{
    int msg[32], n = repl_prompts(r, msg, 32), i, c, how;
    static const char *const hows[] = { "Restore the code and the conversation", "Restore the conversation",
                                        "Restore the code", "Summarize from here", "Summarize up to here",
                                        "Never mind" };
    char lab[32][80];
    const char *opt[32];
    if (!n) {
        ui_line(&r->ui, "No earlier prompt to go back to.");
        return;
    }
    if (*arg) {
        /* /rewind N [code|conversation|both|summarize|summarize-up]: N prompts back, 1 the last */
        long k = atol(arg);
        const char *w = strchr(arg, ' ');
        if (k < 1 || k > n) {
            num_line(r, "Usage: /rewind N [code|conversation|both|summarize|summarize-up], N from 1 to ", n, ".");
            return;
        }
        while (w && *w == ' ')
            w++;
        how = !w || !*w || !strcmp(w, "both") ? 0 : !strcmp(w, "conversation") ? 1 : !strcmp(w, "code") ? 2
              : !strcmp(w, "summarize") ? 3 : !strcmp(w, "summarize-up") ? 4 : 5;
        if (how == 5) {
            ui_line(&r->ui, "Usage: /rewind N [code|conversation|both|summarize|summarize-up]");
            return;
        }
        if (how >= 3)
            repl_summarize(r, msg[k - 1], how == 4);
        else
            repl_rewind(r, msg[k - 1], how != 1, how != 2);
        return;
    }
    for (i = 0; i < n; i++) {
        jv v, b, x;
        jit it;
        lab[i][0] = 0;
        if (json_parse(r->conv.m[msg[i]].json, r->conv.m[msg[i]].n, &v) == 0) {
            json_iter(v, &it);
            if (json_next(&it, 0, &b) && json_get(b, "text", &x)) {
                long k;
                json_str(x, lab[i], 70);
                for (k = 0; lab[i][k]; k++)
                    if (lab[i][k] == '\n')
                        lab[i][k] = ' ';
            }
        }
        if (cp_files_since(&r->cp, msg[i]))
            cl_cat(lab[i], "  (files changed since)", sizeof(lab[i]));
        opt[i] = lab[i];
    }
    c = ui_pick(&r->ui, "Rewind to before which prompt?", opt, n, 0);
    if (c < 0) {
        ui_line(&r->ui, "Prompts, the most recent first (/rewind N [code|conversation|both|summarize|summarize-up]):");
        for (i = 0; i < n; i++) {
            char m[120], num[16];
            cl_ltoa(i + 1, num);
            cl_copy(m, "  ", sizeof(m));
            cl_cat(m, num, sizeof(m));
            cl_cat(m, ". ", sizeof(m));
            cl_cat(m, lab[i], sizeof(m));
            ui_line(&r->ui, m);
        }
        return;
    }
    how = ui_pick(&r->ui, "Rewind", hows, 6, 0);
    if (how < 0 || how == 5)
        return;
    if (how >= 3)
        repl_summarize(r, msg[c], how == 4);   /* Claude Code's summaries: the rest, or what came before */
    else
        repl_rewind(r, msg[c], how != 1, how != 2);
}

/* ---- Claude Code's commands that cannot be on an Amiga: they say why ---- */

static const char why_account[] = "needs a claude.ai subscription and its login; C:Claude works with an API key "
                                  "(billing and limits: console.anthropic.com).";
static const char why_cloud[] = "runs Claude Code sessions in Anthropic's cloud or in the background beside this one; "
                                "an Amiga task runs one conversation, here, in this window.";
static const char why_git[] = "reviews a git diff or a pull request; there is no git on AmigaOS. Ask Claude to "
                              "review files by name, or see this session's changes with /diff.";
static const char why_artifact[] = "makes claude.ai artifacts and Claude Design canvases: they need a claude.ai "
                                   "account and a browser.";

static const struct na_cmd {
    const char *name, *why;
} na_cmds[] = {
    { "/mcp", "MCP servers run as Node or Python processes (stdio), which AmigaOS 3.x does not have; MCP over "
              "HTTP is not built into C:Claude. Its own tools (Read, Bash, WebFetch, ...) are in every session." },
    { "/plugin", "plugins are Node packages from Claude Code's marketplace. C:Claude takes skills, agents, "
                 "commands, hooks and output styles straight from .claude/ and ENVARC:Claude/ instead." },
    { "/reload-plugins", "there are no plugins here (see /plugin); /reload-skills reads skills and commands again." },
    { "/plugin-authoring", "there are no plugins here (see /plugin)." },
    { "/bug", "reports go to Anthropic's feedback service with a claude.ai login. To report something, save "
              "the conversation with /export FILE and send it from a modern machine." },
    { "/feedback", "feedback goes to Anthropic's service with a claude.ai login. /export FILE saves this "
                   "conversation to send from a modern machine." },
    { "/install-github-app", "installs a GitHub App through a browser and the gh CLI, neither of which runs on "
                             "AmigaOS." },
    { "/install-slack-app", "installs the Slack app through a browser OAuth flow." },
    { "/web-setup", "connects GitHub for cloud sessions through the gh CLI." },
    { "/ide", "connects to VS Code or JetBrains IDEs, which do not run on AmigaOS." },
    { "/chrome", "Claude in Chrome needs the Chrome extension." },
    { "/claude-in-chrome", "Claude in Chrome needs the Chrome extension." },
    { "/desktop", "the Claude Desktop app is for macOS and Windows." },
    { "/app", "the Claude Desktop app is for macOS and Windows." },
    { "/mobile", "shows a QR code for the Claude phone app; there is nothing to install here." },
    { "/ios", "shows a QR code for the Claude phone app; there is nothing to install here." },
    { "/android", "shows a QR code for the Claude phone app; there is nothing to install here." },
    { "/upgrade", why_account },
    { "/usage-credits", why_account },
    { "/passes", why_account },
    { "/privacy-settings", why_account },
    { "/rate-limit-options", why_account },
    { "/fast", "fast mode is a claude.ai plan feature." },
    { "/voice", "voice dictation needs a claude.ai account and streams audio from a microphone." },
    { "/remote-control", why_cloud },
    { "/rc", why_cloud },
    { "/remote-env", why_cloud },
    { "/teleport", why_cloud },
    { "/tp", why_cloud },
    { "/schedule", why_cloud },
    { "/autofix-pr", why_cloud },
    { "/ultrareview", why_cloud },
    { "/background", why_cloud },
    { "/bg", why_cloud },
    { "/stop", why_cloud },
    { "/subtask", why_cloud },
    { "/list-agents", why_cloud },
    { "/workflows", "workflows run many agents at once; this Amiga runs one conversation at a time." },
    { "/deep-research", "a workflow of many agents at once; ask Claude to research it here, step by step." },
    { "/workflow-authoring", "workflows run many agents at once; this Amiga runs one conversation at a time." },
    { "/batch", "splits work over parallel agents in git worktrees: no threads for it and no git here." },
    { "/review", why_git },
    { "/code-review", why_git },
    { "/security-review", why_git },
    { "/sandbox", "the sandbox is the operating system's (Seatbelt, bubblewrap); AmigaOS has no such isolation." },
    { "/auto-mode-setup", "auto mode needs Anthropic's action classifier of a claude.ai plan; the permission "
                          "modes here are default, acceptEdits, plan, dontAsk and bypassPermissions." },
    { "/setup-bedrock", "Amazon Bedrock and Google's Agent Platform sign requests in ways C:Claude does not; it "
                        "talks to the Anthropic API (or URL= another endpoint of the same API)." },
    { "/setup-vertex", "Amazon Bedrock and Google's Agent Platform sign requests in ways C:Claude does not; it "
                       "talks to the Anthropic API (or URL= another endpoint of the same API)." },
    { "/import", "brings configuration from OpenAI Codex, Gemini CLI or Cursor, none of which runs on an Amiga." },
    { "/tui", "C:Claude has one renderer, made for UP-Term." },
    { "/focus", "is a view of Claude Code's fullscreen renderer; C:Claude has one renderer, the classic one, "
                "made for UP-Term (Ctrl+O shows the whole transcript)." },
    { "/scroll-speed", "the mouse wheel speed is UP-Term's (its Settings)." },
    { "/artifacts", why_artifact },
    { "/design", why_artifact },
    { "/design-login", why_artifact },
    { "/design-sync", why_artifact },
    { "/slides", why_artifact },
    { "/dataviz", why_artifact },
    { "/artifact-capabilities", why_artifact },
    { "/artifact-diagramming", why_artifact },
    { "/claude-api", "loads Claude Code's bundled API reference; ask Claude to fetch the pages it needs with "
                     "WebFetch instead (platform.claude.com/docs)." },
    { "/fewer-permission-prompts", "scans Claude Code's transcripts for MCP and Bash calls; add allow rules "
                                   "here with /permissions." },
    { "/heapdump", "writes a JavaScript heap snapshot; C:Claude is not JavaScript." },
    { "/radio", "opens a radio stream in a browser." },
    { "/stickers", "orders stickers in a browser." },
    { "/powerup", "Claude Code's animated lessons; /help lists what C:Claude has." }
};
#define NNA ((int)(sizeof(na_cmds) / sizeof(na_cmds[0])))

/* 1 when word is one of Claude Code's commands that cannot be here (said why) */
static int not_here(cl_repl *r, const char *w)
{
    int i;
    for (i = 0; i < NNA; i++)
        if (!strcmp(w, na_cmds[i].name)) {
            char m[600];
            cl_copy(m, w, sizeof(m));
            cl_cat(m, " is not available on the Amiga: it ", sizeof(m));
            cl_cat(m, na_cmds[i].why, sizeof(m));
            ui_line(&r->ui, m);
            return 1;
        }
    return 0;
}

const char *slash_na_list(int i)
{
    return i >= 0 && i < NNA ? na_cmds[i].name : 0;
}

/* ---- /btw /recap: a side question ---- */

static void say_text(cl_repl *r, const char *p, long n)
{
    long a = 0;
    while (a < n) {
        long e = a, l;
        char m[600];
        while (e < n && p[e] != '\n')
            e++;
        l = e - a < (long)sizeof(m) - 1 ? e - a : (long)sizeof(m) - 1;
        memcpy(m, p + a, (size_t)l);
        m[l] = 0;
        ui_line(&r->ui, m);
        a = e + 1;
    }
}

static void btw(cl_repl *r, const char *arg)
{
    jw a;
    if (!*arg) {
        ui_line(&r->ui, r->btw[0] ? r->btw : "Usage: /btw QUESTION (a side question; the conversation does not "
                                             "keep it)");
        return;
    }
    jw_init(&a);
    if (repl_side(r, 0, r->conv.n, arg, &a) == 0) {
        line2(r, "/btw ", arg);
        say_text(r, a.p, a.n);
        cl_copy(r->btw, a.p, sizeof(r->btw));
    }
    jw_free(&a);
}

static const char recap_ask[] =
    "In one line of at most twenty words, recap this session so far: what we are doing and where it stands. "
    "Answer with that line only.";

int slash_recap(cl_repl *r, jw *out)
{
    long k;
    if (repl_side(r, 0, r->conv.n, recap_ask, out))
        return -1;
    for (k = 0; k < out->n; k++)
        if (out->p[k] == '\n')
            out->p[k] = ' ';
    if (out->n > 400) {
        out->n = 400;               /* Claude Code caps a recap at 400 characters */
        out->p[400] = 0;
    }
    return 0;
}

static void recap(cl_repl *r)
{
    jw a;
    if (!r->conv.n) {
        ui_line(&r->ui, "Nothing to recap yet.");
        return;
    }
    jw_init(&a);
    if (slash_recap(r, &a) == 0)
        line2(r, "Recap: ", a.p);
    jw_free(&a);
}

/* ---- /copy [N] ---- */

/* the text of the Nth last answer (1 the last) into out: 0, -1 none */
static int answer_text(cl_repl *r, int nth, jw *out)
{
    int i;
    for (i = r->conv.n - 1; i >= 0; i--) {
        jv v, b, x;
        jit it;
        long before = out->n;
        if (r->conv.m[i].user || json_parse(r->conv.m[i].json, r->conv.m[i].n, &v))
            continue;
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
        if (out->n == before)
            continue;               /* tool calls only */
        if (--nth == 0)
            return 0;
        jw_reset(out);
    }
    return -1;
}

static void copy_(cl_repl *r, const char *arg)
{
    jw t;
    int nth = *arg ? atoi(arg) : 1, nb = 0, c = 0, i;
    const char *opt[9];
    char lab[8][64];
    long bs[8], be[8], p;
    if (nth < 1) {
        ui_line(&r->ui, "Usage: /copy [N] (1 the last answer, 2 the one before, ...)");
        return;
    }
    jw_init(&t);
    if (answer_text(r, nth, &t)) {
        ui_line(&r->ui, "No answer of Claude's to copy yet.");
        jw_free(&t);
        return;
    }
    /* code blocks: a picker of them (Claude Code's), the whole answer first */
    for (p = 0; p < t.n && nb < 8;) {
        const char *s = strstr(t.p + p, "```"), *e;
        if (!s || (s != t.p && s[-1] != '\n'))
            break;
        e = strchr(s, '\n');
        if (!e)
            break;
        bs[nb] = (long)(e + 1 - t.p);
        s = strstr(e + 1, "```");
        if (!s)
            break;
        be[nb] = (long)(s - t.p);
        cl_copy(lab[nb], "Code block ", sizeof(lab[nb]));
        {
            char num[16];
            cl_ltoa(nb + 1, num);
            cl_cat(lab[nb], num, sizeof(lab[nb]));
        }
        nb++;
        e = strchr(s + 3, '\n');
        p = e ? (long)(e + 1 - t.p) : t.n;
    }
    if (nb) {
        opt[0] = "The whole answer";
        for (i = 0; i < nb; i++)
            opt[i + 1] = lab[i];
        c = ui_pick(&r->ui, "Copy what?", opt, nb + 1, 0);
        if (c < 0)
            c = 0;
    }
    {
        const char *s = c ? t.p + bs[c - 1] : t.p;
        long n = c ? be[c - 1] - bs[c - 1] : t.n;
        if (r->sys->clip && r->sys->clip(r->sys->u, s, n) == 0)
            ui_line(&r->ui, c ? "The code block is on the clipboard." : "Claude's answer is on the clipboard.");
        else
            ui_line(&r->ui, "No clipboard here: /export FILE writes the conversation to a file.");
        r->n_copies++;
    }
    jw_free(&t);
}

/* ---- /diff: this session's changes, from the checkpoints ---- */

/* the lines of a and b that differ (a common start and end trimmed), as
 * - and + lines (the line mode; the screen draws its own diff) */
static void plain_diff(cl_repl *r, const char *a, long an, const char *b, long bn)
{
    long p = 0, q = 0, k, ia;
    int shown = 0;
    while (p < an && p < bn && a[p] == b[p])
        p++;
    while (p > 0 && a[p - 1] != '\n')
        p--;                        /* to a line's start */
    while (q < an - p && q < bn - p && a[an - 1 - q] == b[bn - 1 - q])
        q++;
    while (q > 0 && an - q < an && a[an - q] != '\n' && an - q > p)
        q--;                        /* to a line's end */
    for (k = 0; k < 2; k++) {
        const char *s = k ? b : a;
        long e = (k ? bn : an) - q, i = p;
        while (i < e && shown < 200) {
            char m[300];
            long j = i;
            while (j < e && s[j] != '\n')
                j++;
            m[0] = k ? '+' : '-';
            m[1] = ' ';
            ia = j - i < (long)sizeof(m) - 3 ? j - i : (long)sizeof(m) - 3;
            memcpy(m + 2, s + i, (size_t)ia);
            m[ia + 2] = 0;
            ui_line(&r->ui, m);
            shown++;
            i = j + 1;
        }
    }
}

static void diff(cl_repl *r)
{
    int i, j, any = 0;
    for (i = 0; i < r->cp.n; i++) {
        const cl_cpent *e = &r->cp.e[i];
        char *before = 0, *after = 0;
        long bn = 0, an = 0;
        for (j = 0; j < i; j++)
            if (!strcmp(r->cp.e[j].path, e->path))
                break;
        if (j < i)
            continue;               /* the first snapshot of a file is its state before Claude */
        if (!e->kept && e->existed) {
            line2(r, e->path, ": changed (too large for a copy to compare)");
            any = 1;
            continue;
        }
        if (e->existed && r->sys->read(r->sys->u, e->snap, 4L * 1024 * 1024, &before, &bn))
            continue;
        if (r->sys->kind(r->sys->u, e->path) == 1)
            r->sys->read(r->sys->u, e->path, 4L * 1024 * 1024, &after, &an);
        if (bn == an && (!bn || !memcmp(before, after, (size_t)bn)) && (e->existed == (after != 0))) {
            free(before);
            free(after);
            continue;               /* back as it was */
        }
        any = 1;
        line2(r, !e->existed ? "Created: " : !after ? "Deleted: " : "Changed: ", e->path);
        if (r->tui)
            ui_preview(&r->ui, T_EDIT, e->path, before ? before : "", bn, after ? after : "", an);
        else
            plain_diff(r, before ? before : "", bn, after ? after : "", an);
        free(before);
        free(after);
    }
    if (!any)
        ui_line(&r->ui, "No file changed by Claude in this session (what commands run by Bash change is not "
                        "tracked).");
}

/* ---- /usage /cost /stats ---- */

static void usage(cl_repl *r)
{
    char m[300], num[16], d[24];
    int i;
    unsigned long now = r->io->ms ? r->io->ms(r->io->u) : 0;
    repl_cost(r);
    for (i = 0; i < r->conv.nmu; i++) {
        const cl_model_use *u = &r->conv.mu[i];
        cl_copy(m, "  ", sizeof(m));
        cl_cat(m, u->model, sizeof(m));
        cl_cat(m, ": ", sizeof(m));
        cl_ltoa(u->in, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, " in, ", sizeof(m));
        cl_ltoa(u->out, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, " out, ", sizeof(m));
        cl_ltoa(u->cache_r, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, " cache read, ", sizeof(m));
        cl_ltoa(u->cache_w, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, " cache written, ", sizeof(m));
        conv_dollars(u->cost_micro, d, sizeof(d));
        cl_cat(m, d, sizeof(m));
        ui_line(&r->ui, m);
    }
    cl_copy(m, "Time: ", sizeof(m));
    cl_ltoa((long)((now - r->t_start) / 1000), num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, " s in this session, ", sizeof(m));
    cl_ltoa((long)(r->api_ms / 1000), num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, " s of it waiting for the API.", sizeof(m));
    ui_line(&r->ui, m);
    repl_context(r);
}

/* ---- /plan /debug /release-notes /reload-skills ---- */

static void plan(cl_repl *r, const char *arg)
{
    r->tools.perm.mode = PERM_PLAN;
    ui_line(&r->ui, "Plan mode on: Claude looks around and proposes a plan; nothing is changed until you accept "
                    "it (Shift+Tab leaves it).");
    pol_status_event(r);
    if (*arg && pol_prompt(r, arg, (long)strlen(arg)) == 0)
        repl_turn(r, arg, (long)strlen(arg));
}

/* /color [color|default] (A4 gaps 3): the prompt bar's colour for this
 * session; no argument, a random one of the eight */
static void color_(cl_repl *r, const char *arg)
{
    const char *name = arg;
    if (!*arg) {
        unsigned long now = r->io->ms ? r->io->ms(r->io->u) : 0;
        int i = (int)(now / 7 % THEME_NAMED);
        if (!strcmp(r->bar_color, theme_named_names[i]))
            i = (i + 1) % THEME_NAMED;      /* a pick that changes something */
        name = theme_named_names[i];
    } else if (!strcmp(arg, "default")) {
        r->bar_color[0] = 0;
        if (r->tui)
            r->tui->bar = 0;
        ui_line(&r->ui, "Prompt bar color reset to the theme's.");
        return;
    } else if (!theme_named(arg)) {
        ui_line(&r->ui, "Usage: /color [red|blue|green|yellow|purple|orange|pink|cyan|default]");
        return;
    }
    cl_copy(r->bar_color, name, sizeof(r->bar_color));
    {
        long k;
        for (k = 0; r->bar_color[k]; k++)
            r->bar_color[k] = (char)(r->bar_color[k] | 0x20);
    }
    if (r->tui)
        r->tui->bar = theme_named(r->bar_color);
    line2(r, "Prompt bar color set to ", r->bar_color);
}

static void debug(cl_repl *r)
{
    r->debug = 1;
    line2(r, "Debug logging is on for this session: the requests' headers (key hidden), the stream's events, "
             "the tools. The log: ",
          r->log_path ? r->log_path : "the console's log (DEBUG=, --debug-file FILE)");
}

static const char *const notes[] = {
    "C:Claude, Claude Code for AmigaOS -- what is new:",
    "A4 gaps: --json-schema, --session-id, --resume FILE.jsonl, --bare, --safe-mode, --agents, "
    "--replay-user-messages, images in stream-json input, subagent messages in stream-json, "
    "--forward-subagent-text, --setting-sources, --betas, --autocompact, --permission-prompts, --verbose at the "
    "screen.",
    "A4 gaps: /btw /copy /diff /plan /recap /reload-skills /stats /debug /release-notes; skills typed as "
    "/name; /permissions as menus; /rewind can summarise; /statusline sets itself up; /config key=value.",
    "A4 gaps: hooks if, systemMessage, updatedInput and eight more events; .claude/rules; auto memory; Read "
    "shows images and PDFs to Claude; read-only commands run without a question.",
    "A4: Claude Code's tools (Read Write Edit MultiEdit Glob Grep Bash WebFetch WebSearch Task Skill ...), "
    "settings.json, CLAUDE.md, permission rules, hooks, sessions, checkpoints, -p print mode.",
    "A3: the screen of its own -- the input box, the status line, Markdown, diffs, menus.",
    "A2: the first C:Claude -- streaming chat over AmiSSL, tools, /compact /resume.",
    0
};

static void reload_skills(cl_repl *r)
{
    int s0 = defs_count(&r->defs, DEF_SKILL), c0 = defs_count(&r->defs, DEF_COMMAND), s1, c1;
    char m[200], num[16];
    if (repl_load_defs(r)) {
        ui_line(&r->ui, "Out of memory.");
        return;
    }
    s1 = defs_count(&r->defs, DEF_SKILL);
    c1 = defs_count(&r->defs, DEF_COMMAND);
    cl_copy(m, "Skills: ", sizeof(m));
    cl_ltoa(s1, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, " (", sizeof(m));
    cl_ltoa(s1 - s0, num);
    cl_cat(m, s1 >= s0 ? "+" : "", sizeof(m));
    cl_cat(m, num, sizeof(m));
    cl_cat(m, "), commands: ", sizeof(m));
    cl_ltoa(c1, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, " (", sizeof(m));
    cl_ltoa(c1 - c0, num);
    cl_cat(m, c1 >= c0 ? "+" : "", sizeof(m));
    cl_cat(m, num, sizeof(m));
    cl_cat(m, ").", sizeof(m));
    ui_line(&r->ui, m);
}

/* ---- /rename without a name: Claude names it ---- */

static const char name_ask[] = "Give this conversation a short title of three to six words that says what it is "
                               "about. Answer with the title only, no quotes.";

static void rename_(cl_repl *r, const char *arg)
{
    jw a;
    char t[96];
    if (*arg) {
        sess_rename(&r->sess, arg);
        line2(r, "This conversation is now called ", arg);
        return;
    }
    if (!r->conv.n) {
        ui_line(&r->ui, "Nothing to name yet: /rename NAME.");
        return;
    }
    jw_init(&a);
    if (repl_side(r, 0, r->conv.n, name_ask, &a) == 0) {
        long k;
        cl_copy(t, a.p, sizeof(t));
        for (k = 0; t[k]; k++)
            if (t[k] == '\n' || t[k] == '"')
                t[k] = ' ';
        while (k && t[k - 1] == ' ')
            t[--k] = 0;
        sess_rename(&r->sess, t);
        line2(r, "This conversation is now called ", t);
    }
    jw_free(&a);
}

/* ---- /statusline: Claude sets it up (the statusline-setup agent) ---- */

static void statusline(cl_repl *r, const char *arg)
{
    jw p;
    if (!strcmp(arg, "clear") || !strcmp(arg, "delete") || !strcmp(arg, "remove") || !strcmp(arg, "off")) {
        if (cfg_write_key(r->sys, cfg_file(&r->cfg, CFG_USER), "statusLine", 0) == 0 || !r->cfg.status_cmd[0]) {
            r->cfg.status_cmd[0] = 0;
            r->status_text[0] = 0;
            pol_statusline(r);
            ui_line(&r->ui, "The status line is off.");
        } else
            line2(r, "Cannot write ", cfg_file(&r->cfg, CFG_USER));
        return;
    }
    if (!strncmp(arg, "command ", 8)) {
        /* C:Claude's direct form: /statusline command CMD */
        jw v;
        jw_init(&v);
        jw_rawz(&v, "{\"type\": \"command\", \"command\": ");
        jw_strz(&v, arg + 8);
        jw_rawz(&v, "}");
        if (r->sys->mkdir)
            r->sys->mkdir(r->sys->u, r->home);
        if (cfg_write_key(r->sys, cfg_file(&r->cfg, CFG_USER), "statusLine", v.p) == 0) {
            cl_copy(r->cfg.status_cmd, arg + 8, sizeof(r->cfg.status_cmd));
            pol_statusline(r);
            line2(r, "Status line command: ", arg + 8);
        } else
            line2(r, "Cannot write ", cfg_file(&r->cfg, CFG_USER));
        jw_free(&v);
        return;
    }
    /* Claude Code: the statusline-setup agent does it from a description
     * (none: from the shell's prompt) */
    jw_init(&p);
    jw_rawz(&p, "Use the Task tool with subagent_type statusline-setup to configure my status line in ");
    jw_rawz(&p, cfg_file(&r->cfg, CFG_USER));
    jw_rawz(&p, ". ");
    if (*arg) {
        jw_rawz(&p, "What I want: ");
        jw_rawz(&p, arg);
    } else
        jw_rawz(&p, "Make it like my shell's prompt (vsh's PROMPT variable, or the AmigaShell's): the directory "
                    "and the model.");
    jw_rawz(&p, "\n\nThe command gets JSON on its standard input with: model.id, model.display_name, "
                "workspace.current_dir, workspace.project_dir, session_id, session_name, version, "
                "output_style.name, cost.total_cost_usd, cost.total_duration_ms, cost.total_api_duration_ms, "
                "context_window.context_window_size, context_window.used_percentage, "
                "context_window.remaining_percentage, effort.level, vim.mode, agent.name. Commands that read "
                "JSON on an Amiga: vsh's own tools, or a small ARexx or C program.");
    if (!p.oom && pol_prompt(r, p.p, p.n) == 0)
        repl_turn(r, p.p, p.n);
    jw_free(&p);
}

/* ---- /permissions as menus (the screen): view, add, remove, directories ---- */

static void perm_editor(cl_repl *r)
{
    static const char *const top[] = { "Allow a tool or command", "Ask before a tool or command",
                                       "Deny a tool or command", "Remove a rule", "Add a working directory",
                                       "Show the rules" };
    static const char *const lv[] = { "This project, only me (.claude/settings.local.json)",
                                      "This project, everyone (.claude/settings.json)",
                                      "All my projects (ENVARC:Claude/settings.json)" };
    static const int lvl[] = { CFG_LOCAL, CFG_PROJECT, CFG_USER };
    static const char *const ex[] = { "Bash(List *)", "Read", "Edit", "WebFetch(domain:aminet.net)" };
    char typed[300], line[400];
    unsigned picked;
    int c = ui_pick(&r->ui, "Permissions", top, 6, 5), k, i;
    if (c < 0 || c == 5) {
        permissions(r, "");
        return;
    }
    if (c == 4) {
        k = ui_choose(&r->ui, "Directory", "Which directory may Claude work in too?", ex, 0, 0, CH_OTHER, &picked,
                      typed, sizeof(typed));
        if (k == 0 && typed[0])
            add_dir(r, typed);
        return;
    }
    if (c == 3) {
        const char *opt[24];
        int n = 0;
        for (i = 0; i < r->cfg.nrules && n < 24; i++)
            opt[n++] = r->cfg.rules[i].text;
        if (!n) {
            ui_line(&r->ui, "No rules to remove.");
            return;
        }
        k = ui_pick(&r->ui, "Remove which rule?", opt, n, 0);
        if (k >= 0) {
            cl_copy(line, "remove ", sizeof(line));
            cl_cat(line, opt[k], sizeof(line));
            permissions(r, line);
        }
        return;
    }
    k = ui_choose(&r->ui, "Rule", "Which tool or command? Tool, or Tool(pattern)", ex, 0, 4, CH_OTHER, &picked,
                  typed, sizeof(typed));
    if (k < 0)
        return;
    if (k < 4)
        cl_copy(typed, ex[k], sizeof(typed));
    i = ui_pick(&r->ui, "Where is it kept?", lv, 3, 0);
    if (i < 0)
        return;
    cl_copy(line, c == 0 ? "allow " : c == 1 ? "ask " : "deny ", sizeof(line));
    cl_cat(line, typed, sizeof(line));
    cl_cat(line, lvl[i] == CFG_LOCAL ? " local" : lvl[i] == CFG_PROJECT ? " project" : " user", sizeof(line));
    permissions(r, line);
}

/* skillOverrides' states, as the /skills menu calls them (Claude Code's
 * "user-only" for user-invocable-only) */
static const char *const sk_states[] = { "on", "name-only", "user-invocable-only", "off" };
static const char *const sk_labels[] = { "on: listed to Claude, in the / menu",
                                         "name-only: Claude sees only its name",
                                         "user-only: only you call it (/name)", "off: hidden everywhere" };

/* a skill's visibility kept in .claude/settings.local.json's skillOverrides
 * (Claude Code's /skills menu saves there); the settings read again */
static void skill_state_set(cl_repl *r, const cl_def *d, const char *st)
{
    jw v;
    jw_init(&v);
    jw_strz(&v, st);
    if (!v.oom && cfg_write_sub(r->sys, cfg_file(&r->cfg, CFG_LOCAL), "skillOverrides", d->name, v.p) == 0) {
        char m[200];
        cl_copy(m, d->name, sizeof(m));
        cl_cat(m, " is now ", sizeof(m));
        cl_cat(m, st, sizeof(m));
        line2(r, m, " (skillOverrides in .claude/settings.local.json)");
        repl_load(r);
    } else
        line2(r, "The setting could not be written: ", cfg_file(&r->cfg, CFG_LOCAL));
    jw_free(&v);
}

/* /skills [text]: the skills (those whose name, description or source
 * has the text), each with its size in context and its visibility;
 * /skills NAME on|name-only|user-only|off sets one (skillOverrides); in
 * the screen, /skills alone opens the menu: a skill, then its state */
static void skills(cl_repl *r, const char *arg)
{
    int i, k = 0;
    const cl_def *d;
    {
        /* NAME STATE */
        char nm[64];
        const char *sp = strchr(arg, ' ');
        if (sp && (long)(sp - arg) < (long)sizeof(nm)) {
            const char *st = sp + 1;
            memcpy(nm, arg, (size_t)(sp - arg));
            nm[sp - arg] = 0;
            if (!strcmp(st, "user-only"))
                st = "user-invocable-only";
            d = defs_find(&r->defs, DEF_SKILL, nm);
            for (i = 0; i < 4; i++)
                if (d && !strcmp(st, sk_states[i])) {
                    skill_state_set(r, d, sk_states[i]);
                    return;
                }
        }
    }
    if (!*arg && r->tui) {
        const char *opt[32];
        char lab[32][120];
        const cl_def *ds[32];
        int n = 0, c, s;
        for (i = 0; (d = defs_nth(&r->defs, DEF_SKILL, i)) != 0 && n < 32; i++) {
            cl_copy(lab[n], d->name, sizeof(lab[0]));
            cl_cat(lab[n], "  [", sizeof(lab[0]));
            cl_cat(lab[n], cfg_skill_state(&r->cfg, d->name), sizeof(lab[0]));
            cl_cat(lab[n], "]", sizeof(lab[0]));
            opt[n] = lab[n];
            ds[n++] = d;
        }
        if (n) {
            c = ui_pick(&r->ui, "Skills: which one?", opt, n, 0);
            if (c < 0)
                return;
            for (s = 0; s < 4 && strcmp(sk_states[s], cfg_skill_state(&r->cfg, ds[c]->name)); s++)
                ;
            s = ui_pick(&r->ui, ds[c]->name, sk_labels, 4, s < 4 ? s : 0);
            if (s >= 0)
                skill_state_set(r, ds[c], sk_states[s]);
            return;
        }
    }
    for (i = 0; (d = defs_nth(&r->defs, DEF_SKILL, i)) != 0; i++) {
        char m[500], num[16];
        const char *src = src_word(d->src), *st = cfg_skill_state(&r->cfg, d->name);
        if (*arg && !strstr(d->name, arg) && !strstr(d->description, arg) && !strstr(src, arg))
            continue;
        cl_copy(m, "  ", sizeof(m));
        cl_cat(m, d->name, sizeof(m));
        cl_cat(m, " (", sizeof(m));
        cl_cat(m, src, sizeof(m));
        if (strcmp(st, "on")) {
            cl_cat(m, ", ", sizeof(m));
            cl_cat(m, !strcmp(st, "user-invocable-only") ? "user-only" : st, sizeof(m));
        }
        cl_cat(m, ")  ", sizeof(m));
        cl_cat(m, d->description, sizeof(m));
        cl_cat(m, "  [~", sizeof(m));
        cl_ltoa(((long)strlen(d->body) + 3) / 4, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, d->no_model ? " tokens; only you call it]" : " tokens when used]", sizeof(m));
        ui_line(&r->ui, m);
        k++;
    }
    if (!k)
        ui_line(&r->ui, *arg ? "No skill matches." : "No skills: put them in .claude/skills/NAME/SKILL.md (or "
                                                     "UP-Term:var/Claude/skills; ENVARC:Claude/skills without UP-Term:).");
    else if (!*arg)
        ui_line(&r->ui, "Turn one on or off: /skills NAME on|name-only|user-only|off");
}

/* /skill-doctor: what each skill costs in context (its listing, always;
 * its body, when used) */
static void skill_doctor(cl_repl *r)
{
    int i, k, n = 0, idx[64];
    long all = 0, cost[64];
    const cl_def *d, *ds[64];
    char m[300], num[16];
    /* Claude Code's report: the skills other than the bundled ones, their
     * listing's cost and how often each was used; the unused flagged,
     * the costliest first */
    for (i = 0; (d = defs_nth(&r->defs, DEF_SKILL, i)) != 0 && n < 64; i++) {
        const char *st = cfg_skill_state(&r->cfg, d->name);
        long l;
        if (d->src == DEF_BUILTIN)
            continue;
        l = d->no_model || !strcmp(st, "off") || !strcmp(st, "user-invocable-only") ? 0
            : !strcmp(st, "name-only") ? ((long)strlen(d->name) + 7) / 4
            : ((long)strlen(d->name) + (long)strlen(d->description) + (long)strlen(d->when) + 7) / 4;
        for (k = n; k > 0 && cost[idx[k - 1]] < l; k--)
            idx[k] = idx[k - 1];
        ds[n] = d;
        cost[n] = l;
        idx[k] = n++;
        all += l;
    }
    if (!n) {
        ui_line(&r->ui, "No skills of yours (the bundled ones are not counted): nothing to report.");
        return;
    }
    ui_line(&r->ui, "Your skills by what they cost in context (~tokens listed every request, used how often):");
    for (k = 0; k < n; k++) {
        long used = pol_skill_count(r, ds[idx[k]]->name);
        d = ds[idx[k]];
        cl_copy(m, "  ", sizeof(m));
        cl_cat(m, d->name, sizeof(m));
        cl_cat(m, ": ~", sizeof(m));
        cl_ltoa(cost[idx[k]], num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, " tokens, used ", sizeof(m));
        cl_ltoa(used, num);
        cl_cat(m, used ? num : "never", sizeof(m));
        cl_cat(m, used == 1 ? " time" : used ? " times" : "", sizeof(m));
        if (!used && cost[idx[k]])
            cl_cat(m, "  <- turn it off? /skills NAME off (or disable-model-invocation: true)", sizeof(m));
        ui_line(&r->ui, m);
    }
    num_line(r, "Listing in all: ~", all, " tokens a request.");
}

/* /advisor [model|off] (A4 gaps 2): Claude Code's advisor tool -- a
 * stronger model Claude consults at key moments; saved as advisorModel in
 * the user's settings */
static void advisor(cl_repl *r, const char *arg)
{
    static const char *const opt[] = { "fable", "opus", "sonnet", "off" };
    char m[200];
    const char *a = arg;
    int k;
    if (r->sys->getenv) {
        char v[8];
        if (r->sys->getenv(r->sys->u, "CLAUDE_CODE_DISABLE_ADVISOR_TOOL", v, sizeof(v)) > 0 && strcmp(v, "0")) {
            ui_line(&r->ui, "The advisor tool is turned off (CLAUDE_CODE_DISABLE_ADVISOR_TOOL).");
            return;
        }
    }
    if (!*a && r->tui) {
        k = ui_pick(&r->ui, "Advisor model", opt, 4, 0);
        if (k < 0)
            return;
        a = opt[k];
    }
    if (!*a) {
        line2(r, "Advisor: ", r->advisor_cli[0] ? r->advisor_cli : r->cfg.advisor[0] ? r->cfg.advisor : "none");
        ui_line(&r->ui, "Set one with /advisor fable|opus|sonnet|MODEL-ID, or /advisor off.");
        return;
    }
    if (!strcmp(a, "off")) {
        r->advisor_cli[0] = 0;
        r->cfg.advisor[0] = 0;
        cfg_write_key(r->sys, cfg_file(&r->cfg, CFG_USER), "advisorModel", 0);
        ui_line(&r->ui, "Advisor off.");
        return;
    }
    if (conv_advisor_ok(r->model, cfg_model(a)) < 0 && !strncmp(cfg_model(a), "claude-haiku", 12)) {
        ui_line(&r->ui, "Haiku can call an advisor but cannot be one: pick fable, opus or sonnet.");
        return;
    }
    r->advisor_cli[0] = 0;
    cl_copy(r->cfg.advisor, a, sizeof(r->cfg.advisor));
    {
        jw v;
        jw_init(&v);
        jw_strz(&v, a);
        if (!v.oom)
            cfg_write_key(r->sys, cfg_file(&r->cfg, CFG_USER), "advisorModel", v.p);
        jw_free(&v);
    }
    cl_copy(m, "Advisor set to ", sizeof(m));
    cl_cat(m, cfg_model(a), sizeof(m));
    k = conv_advisor_ok(r->model, cfg_model(a));
    if (k)
        cl_cat(m, k > 0 ? " -- it cannot advise the current model (it ranks below it): it takes effect with a "
                          "model it can advise" : " -- not attached to this model", sizeof(m));
    ui_line(&r->ui, m);
}

/* /goal [condition|clear]: Claude keeps working until the condition is met */
static void goal(cl_repl *r, const char *arg)
{
    if (!*arg) {
        line2(r, "Goal: ", r->goal[0] ? r->goal : "(none: /goal CONDITION)");
        return;
    }
    if (!strcmp(arg, "clear")) {
        r->goal[0] = 0;
        ui_line(&r->ui, "Goal cleared.");
        return;
    }
    cl_copy(r->goal, arg, sizeof(r->goal));
    line2(r, "Goal set: Claude keeps working until this holds: ", r->goal);
    if (pol_prompt(r, arg, (long)strlen(arg)) == 0)
        repl_turn(r, arg, (long)strlen(arg));
}

/* /autocompact [auto|TOKENS|on|off]: Claude Code's window (saved as
 * autoCompactWindow in the user's settings), and C:Claude's on/off */
static void autocompact(cl_repl *r, const char *arg)
{
    long t;
    if (!strcmp(arg, "on") || !strcmp(arg, "off")) {
        config_set(r, "autoCompactEnabled", !strcmp(arg, "on") ? "true" : "false", CFG_USER);
        return;
    }
    if (*arg) {
        t = cfg_window_parse(arg);
        if (!t) {
            ui_line(&r->ui, "Usage: /autocompact auto, or a window from 100k to 1M tokens (200000, 500k, 1M), "
                            "or on / off");
            return;
        }
        if (t < 0)
            cfg_write_key(r->sys, cfg_file(&r->cfg, CFG_USER), "autoCompactWindow", 0);  /* auto: the key goes */
        else {
            char num[16];
            cl_ltoa(t, num);
            if (r->sys->mkdir)
                r->sys->mkdir(r->sys->u, r->home);
            if (cfg_write_key(r->sys, cfg_file(&r->cfg, CFG_USER), "autoCompactWindow", num)) {
                line2(r, "Cannot write ", cfg_file(&r->cfg, CFG_USER));
                return;
            }
        }
        r->cfg.compact_window = t > 0 ? t : 0;
        r->compact_window = 0;          /* the session takes the saved value */
    }
    num_line(r, r->auto_compact ? "Auto-compact is on: it compacts at " : "Auto-compact is off (/autocompact on); "
                                                                          "its window would be ",
             repl_compact_at(r), " tokens of context.");
}

static void run_def(cl_repl *r, const cl_def *d, const char *arg, const char *more);

/* /loop [interval] [prompt] (A4 gaps 3; alias /proactive): Claude Code's
 * bundled skill (commands.c); with no prompt, the default loop prompt
 * after it. The model schedules: CronCreate for an interval, ScheduleWakeup
 * for a pace of its own (sched.c, tasks.c). */
static void loop_(cl_repl *r, const char *arg)
{
    const cl_def *d = defs_find(&r->defs, DEF_SKILL, "loop");
    jw more;
    if (r->tools.no_cron) {
        ui_line(&r->ui, "/loop is not available: CLAUDE_CODE_DISABLE_CRON turns the scheduler off.");
        return;
    }
    if (r->no_person) {
        ui_line(&r->ui, "/loop runs in an interactive session only: its iterations come between turns, and print "
                        "mode ends after one.");
        return;
    }
    if (!d) {
        ui_line(&r->ui, "Unknown command. Type /help for the list.");
        return;
    }
    jw_init(&more);
    if (!sched_loop_has_prompt(arg)) {
        jw_rawz(&more, "\n\n## Default loop prompt\n\n");
        sched_loop_default(r, &more);
    }
    run_def(r, d, arg, more.n ? more.p : 0);
    jw_free(&more);
}

int slash_run(cl_repl *r, const char *w, const char *arg)
{
    if (!strcmp(w, "/permissions") || !strcmp(w, "/allowed-tools")) {
        if (!*arg && r->tui)
            perm_editor(r);         /* Claude Code's dialog: menus */
        else
            permissions(r, arg);
    } else if (!strcmp(w, "/output-style"))
        output_style(r, arg);
    else if (!strcmp(w, "/status"))
        status(r);
    else if (!strcmp(w, "/usage") || !strcmp(w, "/cost") || !strcmp(w, "/stats"))
        usage(r);
    else if (!strcmp(w, "/btw"))
        btw(r, arg);
    else if (!strcmp(w, "/recap"))
        recap(r);
    else if (!strcmp(w, "/copy"))
        copy_(r, arg);
    else if (!strcmp(w, "/diff"))
        diff(r);
    else if (!strcmp(w, "/plan"))
        plan(r, arg);
    else if (!strcmp(w, "/debug"))
        debug(r);
    else if (!strcmp(w, "/color"))
        color_(r, arg);
    else if (!strcmp(w, "/keybindings"))
        keybindings(r);
    else if (!strcmp(w, "/loop") || !strcmp(w, "/proactive"))
        loop_(r, arg);
    else if (!strcmp(w, "/release-notes")) {
        int i;
        for (i = 0; notes[i]; i++)
            ui_line(&r->ui, notes[i]);
    } else if (!strcmp(w, "/reload-skills"))
        reload_skills(r);
    else if (not_here(r, w))
        ;
    else if (!strcmp(w, "/doctor"))
        doctor(r);
    else if (!strcmp(w, "/terminal-setup"))
        terminal_setup(r);
    else if (!strcmp(w, "/export"))
        export_(r, arg);
    else if (!strcmp(w, "/config"))
        config(r, arg);
    else if (!strcmp(w, "/memory"))
        memory(r, arg);
    else if (!strcmp(w, "/login"))
        login(r, arg);
    else if (!strcmp(w, "/setup"))
        setup_begin(r);
    else if (!strcmp(w, "/logout"))
        logout(r);
    else if (!strcmp(w, "/agents"))
        list_defs(r, DEF_AGENT, "No subagents: put them in .claude/agents/NAME.md (or ENVARC:Claude/agents).");
    else if (!strcmp(w, "/skills"))
        skills(r, arg);
    else if (!strcmp(w, "/skill-doctor"))
        skill_doctor(r);
    else if (!strcmp(w, "/advisor"))
        advisor(r, arg);
    else if (!strcmp(w, "/goal"))
        goal(r, arg);
    else if (!strcmp(w, "/commands"))
        list_defs(r, DEF_COMMAND, "No custom commands: put them in .claude/commands/NAME.md (or ENVARC:Claude/commands).");
    else if (!strcmp(w, "/hooks"))
        hooks_list(r);
    else if (!strcmp(w, "/tasks") || !strcmp(w, "/bashes")) {
        char m[2000];
        tools_shells(&r->tools, m, sizeof(m));     /* Bash run_in_background's shells (shells.c) */
        if (m[0])
            ui_line(&r->ui, "Background shells (BashOutput reads one, KillShell stops one):");
        ui_line(&r->ui, m[0] ? m : "No commands running in the background.");
    } else if (!strcmp(w, "/todos"))
        todos(r);
    else if (!strcmp(w, "/add-dir"))
        add_dir(r, arg);
    else if (!strcmp(w, "/cd"))
        cd(r, arg);
    else if (!strcmp(w, "/rename"))
        rename_(r, arg);
    else if (!strcmp(w, "/branch") || !strcmp(w, "/fork")) {
        if (!r->conv.n)
            ui_line(&r->ui, "Nothing to branch yet.");
        else if (sess_branch(&r->sess, &r->conv, r->io->ms ? r->io->ms(r->io->u) : 0) == 0) {
            if (*arg)
                sess_rename(&r->sess, arg);
            line2(r, "Going on in a branch of this conversation: session ", r->sess.id);
        } else
            ui_line(&r->ui, "The branch could not be written.");
    } else if (!strcmp(w, "/rewind") || !strcmp(w, "/checkpoint") || !strcmp(w, "/undo"))
        rewind_(r, arg);
    else if (!strcmp(w, "/autocompact"))
        autocompact(r, arg);
    else if (!strcmp(w, "/statusline"))
        statusline(r, arg);
    else
        return 0;
    return 1;
}

/* a custom command or a skill typed by its name, run as a turn (more:
 * text after its expansion, 0 none) */
static void run_def(cl_repl *r, const cl_def *d, const char *arg, const char *more)
{
    char err[300], keep[64];
    jw p;
    if (d->type == DEF_SKILL && !strcmp(cfg_skill_state(&r->cfg, d->name), "off")) {
        /* Claude Code: a skill skillOverrides turns off cannot be run by its name either */
        line2(r, "This skill is turned off by skillOverrides in the settings: ", d->name);
        return;
    }
    jw_init(&p);
    if (pol_expand(r, d, arg, &p, err, sizeof(err))) {
        line2(r, d->type == DEF_SKILL ? "The skill could not be expanded: " : "The command could not be expanded: ",
              err);
        jw_free(&p);
        return;
    }
    if (more)
        jw_rawz(&p, more);
    if (pol_expansion(r, d, arg, p.p ? p.p : "", p.n)) {
        jw_free(&p);
        return;                     /* a UserPromptExpansion hook said no */
    }
    if (d->type == DEF_SKILL) {
        r->n_skills_run++;
        pol_skill_used(r, d);       /* /skill-doctor's counts; its frontmatter hooks from now on */
    } else
        r->n_cmds_run++;
    if (pol_prompt(r, p.p ? p.p : "", p.n) == 0) {
        char keep_effort[16];
        cl_copy(keep, r->model, sizeof(keep));
        cl_copy(keep_effort, r->effort, sizeof(keep_effort));
        if (d->model[0])
            cl_copy(r->model, cfg_model(d->model), sizeof(r->model));
        if (d->effort[0])
            cl_copy(r->effort, d->effort, sizeof(r->effort));     /* effort: for its turn */
        r->turn_tools = 0;
        pol_turn_tools(r, d);
        repl_turn(r, p.p ? p.p : "", p.n);
        r->turn_tools = 0;
        cl_copy(r->model, keep, sizeof(r->model));
        cl_copy(r->effort, keep_effort, sizeof(r->effort));
    }
    jw_free(&p);
}

int slash_custom(cl_repl *r, const char *w, const char *arg)
{
    const cl_def *d = defs_find(&r->defs, DEF_COMMAND, w + 1);
    if (!d) {
        /* a skill typed as /name (Claude Code: skills are commands too),
         * unless user-invocable: false */
        d = defs_find(&r->defs, DEF_SKILL, w + 1);
        if (d && d->no_user)
            d = 0;
    }
    if (!d)
        return 0;
    run_def(r, d, arg, 0);
    return 1;
}
