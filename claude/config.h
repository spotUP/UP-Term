/* config -- Claude Code's settings for C:Claude (A4 WP3, rows 3.2 3.3 3.11).
 *
 * settings.json at three levels, read in this order and merged: the user's
 * (ENVARC:Claude/settings.json, Claude Code's ~/.claude), the project's
 * (<root>/.claude/settings.json) and the project's local one
 * (<root>/.claude/settings.local.json). The keys are Claude Code's: a scalar
 * from a later file wins (model, effort/effortLevel, outputStyle, theme,
 * statusLine {command, padding, refreshInterval}, autoCompactEnabled,
 * fallbackModel, permissions.defaultMode, and C:Claude's webSearch);
 * lists add up (permissions allow / deny / ask, permissions
 * .additionalDirectories, hooks, env).
 *
 * Permission rules have Claude Code's form, Tool or Tool(pattern):
 *   Bash(make test)      that command exactly; Bash(make:*) or Bash(make *)
 *                        anything that starts with "make"; a compound line
 *                        (&& || ; |) is allowed only when every part is
 *   Read(src)            Read, LS, Grep and Glob in the root.s src and below;
 *   Edit(*.c)            Write, Edit, MultiEdit; a pattern without a / is a
 *                        name at any depth; "Work:x/#?" style is absolute
 *   WebFetch(domain:aminet.net)
 * The deny list wins over ask, ask over allow. Our own A2 tool names
 * (read_file, run_command, ...) are the same rules under Claude Code's
 * names (cfg_cc_tool).
 * Portable C89 over sys.h, host-tested (tests/test_claude_config.c). */
#ifndef CL_CONFIG_H
#define CL_CONFIG_H

#include "json.h"
#include "sys.h"

enum { CFG_USER, CFG_PROJECT, CFG_LOCAL, CFG_SESSION, CFG_NSRC };
enum { RULE_NONE, RULE_ALLOW, RULE_ASK, RULE_DENY };

/* hook events, Claude Code's names (hooks.h runs them) */
enum {
    HK_PRE_TOOL, HK_POST_TOOL, HK_PROMPT, HK_STOP, HK_SUBAGENT_STOP, HK_SESSION_START, HK_SESSION_END,
    HK_PRE_COMPACT, HK_NOTIFICATION,
    /* A4 gaps: Claude Code's other events that exist here */
    HK_PERMISSION_REQUEST, HK_POST_TOOL_FAILURE, HK_SUBAGENT_START, HK_POST_COMPACT, HK_STOP_FAILURE,
    HK_PROMPT_EXPANSION, HK_CWD_CHANGED, HK_DIR_ADDED, HK_PRE_MODEL_SWITCH, HK_POST_MODEL_SWITCH,
    HK_INSTRUCTIONS_LOADED, HK_POST_TOOL_BATCH, HK_CONFIG_CHANGE, HK_SETUP, HK_COUNT
};
extern const char *const cfg_hook_events[HK_COUNT];

typedef struct cl_rule {
    int kind;                   /* RULE_ALLOW / ASK / DENY */
    int src;                    /* CFG_* */
    char *text;                 /* as written, "Bash(make *)" */
} cl_rule;

typedef struct cl_hook {
    int event;                  /* HK_* */
    int src;
    char *matcher;              /* "" all; "Bash", "Edit|Write", ".*" */
    char *cmd;
    int timeout_s;
    char *cond;                 /* "if": a permission rule the call must match, 0 none */
    int kind;                   /* HOOK_COMMAND, HOOK_PROMPT (cmd holds the prompt) */
    char *model;                /* a prompt hook's model, 0 the small one */
    char *status;               /* statusMessage: shown while it runs, 0 none */
    int once;                   /* "once": true -- runs once a session */
} cl_hook;

enum { HOOK_COMMAND, HOOK_PROMPT };

typedef struct cl_kv {
    char *k, *v;
} cl_kv;

typedef struct cl_settings {
    char model[64];
    int model_src;              /* the level that set model (CFG_*), -1 none */
    char effort[16];
    char output_style[64];
    char theme[32];
    char editor_mode[16];       /* editorMode: "vim" or "normal" */
    /* vimInsertModeRemaps: the two-key INSERT sequences mapped to Esc, two
     * characters each ("jjkj"); read from the user's settings and
     * --settings only (a project's cannot remap keys), as Claude Code's */
    char vim_remaps[17];
    char status_cmd[256];       /* statusLine.command */
    int status_pad;             /* statusLine.padding: columns before its text */
    int status_refresh_s;       /* statusLine.refreshInterval: seconds, 0 only on events */
    int web_search;             /* "webSearch" (C:Claude's switch; Claude Code's way is the
                                 * deny rule "WebSearch"): -1 not set, 0 off, 1 on */
    char default_mode[24];      /* default / acceptEdits / plan */
    char fallback_model[64];
    int auto_compact;           /* -1 not set, 0 off, 1 on */
    cl_rule *rules;
    int nrules, caprules;
    cl_hook *hooks;
    int nhooks, caphooks;
    cl_kv *env;
    int nenv, capenv;
    char **dirs;                /* additionalDirectories */
    int ndirs, capdirs;
    /* A4 gaps */
    int no_hooks;               /* disableAllHooks: true */
    int verbose;                /* "verbose": -1 not set, 0, 1 */
    char agent[64];             /* "agent": the main thread runs as that agent */
    long compact_window;        /* autoCompactWindow: tokens, 0 not set */
    int auto_memory;            /* autoMemoryEnabled: -1 not set, 0, 1 */
    int hide_vim;               /* statusLine.hideVimModeIndicator */
    char **md_excludes;         /* claudeMdExcludes: globs of memory files not loaded */
    int nmdx, capmdx;
    unsigned skip;              /* bit per CFG_USER/PROJECT/LOCAL not read (--setting-sources) */
    char key_helper[256];       /* apiKeyHelper: a command whose output is the API key */
    char avail_models[256];     /* availableModels, comma-separated ("" all) */
    long bash_max_chars;        /* bashOutputMaxChars, 0 the default */
    int cleanup_days;           /* cleanupPeriodDays, -1 not set */
    char path[CFG_NSRC][300];   /* the files (user, project, local) */
    int found[CFG_NSRC];        /* read and valid */
    char err[300];              /* the last file that did not parse, and why */
} cl_settings;

void cfg_init(cl_settings *s);
void cfg_free(cl_settings *s);
/* One settings text merged in (src: CFG_*): 0, -1 not a JSON object
 * (err says why; nothing taken from it). */
int cfg_merge(cl_settings *s, int src, const char *json, long n, const char *name);
/* The three files: home is the user's directory (ENVARC:Claude), root the
 * project. A missing file is fine; one that does not parse is skipped
 * with s->err set. Returns how many were read. */
int cfg_load(cl_settings *s, cl_sys *sys, const char *home, const char *root);
/* the file of a level, "" for CFG_SESSION */
const char *cfg_file(const cl_settings *s, int src);

/* One top-level key of a settings file set to a JSON value (raw, already
 * valid), or removed (value 0); the file is created when missing, its
 * other keys kept as they were: 0, -1. */
int cfg_write_key(cl_sys *sys, const char *file, const char *key, const char *value);
/* A rule added to (add 1) or removed from (add 0) permissions.<allow|ask|
 * deny> of a settings file: 0, -1 (also -1: not there to remove). */
int cfg_write_rule(cl_sys *sys, const char *file, int kind, const char *rule, int add);
/* a rule in memory only (this session) */
int cfg_add_rule(cl_settings *s, int kind, int src, const char *text);
const char *cfg_kind_name(int kind);    /* "allow" "ask" "deny" */

/* A rule's parts: the tool and the pattern ("" none). 0, -1 malformed. */
int cfg_rule_parse(const char *rule, char *tool, long tcap, char *pat, long pcap);
/* Claude Code's name of a tool ("read_file" -> "Read"); others as they are */
const char *cfg_cc_tool(const char *name);
/* Does the rule cover this call? root resolves relative paths. */
int cfg_rule_match(const char *rule, const char *tool, jv input, const char *root);
/* The rules' answer for a call: RULE_NONE (ask as usual), ALLOW, ASK,
 * DENY; *which the deciding rule (or 0). */
int cfg_decide(const cl_settings *s, const char *tool, jv input, const char *root, const cl_rule **which);

/* Is the web_search server tool on? Off by "webSearch": false or by a
 * deny rule naming WebSearch alone (Claude Code's way to turn it off). The
 * API runs it, so there is no call to ask about: an ask rule does not
 * turn it off. */
int cfg_web_search(const cl_settings *s);

/* An auto-compact window as Claude Code takes it: 200000, 500k, 1M, or a
 * bare 100..1000 meaning thousands; 100K to 1M. The tokens, -1 for
 * "auto", 0 when it is none of these. */
long cfg_window_parse(const char *v);

/* the user's home for ~/ in path rules (HOME, else SYS:) */
void cfg_set_home(const char *dir);

/* the hooks dropped (disableAllHooks, --bare, --safe-mode) */
void cfg_drop_hooks(cl_settings *s);

/* opus / sonnet / haiku / fable (also opusplan's model, default) -> the
 * current id; anything else as it is */
const char *cfg_model(const char *name);

/* the glob match the rules use: * ? and ** (across /), [..]; AmigaDOS
 * #? and ? too; icase for paths */
int cfg_glob(const char *pat, const char *s, int icase);

#endif
