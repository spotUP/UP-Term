/* tools -- see tools.h. The definitions (names, descriptions, schemas),
 * the checks, the permission rules, the file tools (Read, Write, Edit,
 * MultiEdit), the questions (AskUserQuestion, the plan-mode tools), Skill,
 * SlashCommand, and what the screen shows of each. Glob and Grep are in
 * search.c, the background shells in shells.c, WebFetch in webfetch.c,
 * Task in subagent.c. */
#include <stdlib.h>
#include <string.h>
#include "tools_int.h"
#include "schema.h"
#include "path.h"
#include "stream.h"
#include "util.h"

#define READ_WHOLE (256L * 1024)     /* Read without offset/limit */
#define READ_PART  (2048L * 1024)    /* Read with them */
#define READ_LINES 2000
#define LINE_MAX_CH 2000
#define EDIT_MAX   (1024L * 1024)

/* ---- the definitions ----
 * Descriptions and schemas in pieces under C89's 509-byte literal limit. */

static const char *const d_read[] = {
    "Reads a file. file_path is an AmigaOS path (SYS:S/Startup-Sequence, RAM:x, Work:src/main.c) or "
    "relative to the start directory. By default up to 2000 lines from the start; offset (the first "
    "line, 1-based) and limit read a part: use them for long files. Lines longer than 2000 characters "
    "are cut. ",
    "The result is in cat -n format: the line number, a tab, the line. Files up to 256 KB are read "
    "whole, larger ones (to 2 MB) only with offset and limit. Binary files are refused. Read a file "
    "before you Edit or Write it. Read several files in one answer when they may all be useful.",
    0
};
static const char s_read[] =
    "{\"type\":\"object\",\"properties\":{\"file_path\":{\"type\":\"string\",\"description\":\"The file to "
    "read\"},\"offset\":{\"type\":\"integer\",\"description\":\"The line to start at (1 = the first)\"},"
    "\"limit\":{\"type\":\"integer\",\"description\":\"How many lines to read\"}},\"required\":[\"file_path\"],"
    "\"additionalProperties\":false}";

static const char *const d_write[] = {
    "Writes a file, replacing it when it exists. An existing file must have been Read in this "
    "conversation first, or the write fails. Prefer Edit for a change to an existing file. Do not "
    "create documentation files unless the user asks for them.",
    0
};
static const char s_write[] =
    "{\"type\":\"object\",\"properties\":{\"file_path\":{\"type\":\"string\",\"description\":\"The file to "
    "write\"},\"content\":{\"type\":\"string\",\"description\":\"The whole new content\"}},"
    "\"required\":[\"file_path\",\"content\"],\"additionalProperties\":false}";

static const char *const d_edit[] = {
    "Performs an exact string replacement in a file. Read the file first: an Edit of a file not Read in "
    "this conversation fails. Keep the indentation exactly as it follows the tab in Read's output; "
    "never put the line-number prefix into old_string or new_string. ",
    "The edit fails when old_string is not unique in the file: give more surrounding context, or set "
    "replace_all to change every occurrence (a rename). An empty old_string on a file that does not "
    "exist creates it. The file keeps its character set (Latin-1 or UTF-8).",
    0
};
static const char s_edit[] =
    "{\"type\":\"object\",\"properties\":{\"file_path\":{\"type\":\"string\",\"description\":\"The file to "
    "change\"},\"old_string\":{\"type\":\"string\",\"description\":\"The text to replace\"},\"new_string\":"
    "{\"type\":\"string\",\"description\":\"The text to put in its place (different from old_string)\"},"
    "\"replace_all\":{\"type\":\"boolean\",\"description\":\"Replace every occurrence (default false)\"}},"
    "\"required\":[\"file_path\",\"old_string\",\"new_string\"],\"additionalProperties\":false}";

static const char *const d_multiedit[] = {
    "Several exact string replacements in one file, made in order, each on the result of the ones before. "
    "All of them succeed or none is made (the file stays as it was). Each follows Edit's rules: Read "
    "the file first, old_string unique unless replace_all, never the line-number prefix. Prefer it to "
    "several Edit calls on one file.",
    0
};
static const char s_multiedit[] =
    "{\"type\":\"object\",\"properties\":{\"file_path\":{\"type\":\"string\",\"description\":\"The file to "
    "change\"},\"edits\":{\"type\":\"array\",\"description\":\"The edits, in order\",\"items\":{\"type\":"
    "\"object\",\"properties\":{\"old_string\":{\"type\":\"string\"},\"new_string\":{\"type\":\"string\"},"
    "\"replace_all\":{\"type\":\"boolean\"}},\"required\":[\"old_string\",\"new_string\"],"
    "\"additionalProperties\":false}}},\"required\":[\"file_path\",\"edits\"],\"additionalProperties\":false}";

static const char *const d_glob[] = {
    "Fast file name pattern matching in a directory tree. Patterns: * (within a name), ** (any "
    "directories), ?, [abc], {c,h}; the AmigaDOS forms #? and (a|b) work too. Names match without "
    "regard to case. Returns the matching paths (a directory with a trailing /), newest first, at most "
    "100. path: the directory to search (default: the start directory).",
    0
};
static const char s_glob[] =
    "{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\",\"description\":\"The pattern, "
    "e.g. **/*.c\"},\"path\":{\"type\":\"string\",\"description\":\"The directory to search in\"}},"
    "\"required\":[\"pattern\"],\"additionalProperties\":false}";

static const char *const d_grep[] = {
    "Searches file contents with a regular expression (ripgrep's syntax: ERE plus \\d \\w \\s \\b and "
    "(?i); no look-around, no back references). Use Grep for searches, never grep or Search through "
    "Bash. path: a file or a directory (default: the start directory). glob filters file names "
    "(*.c, *.{c,h}); type by kind (c, cpp, py, js, ts, md, asm, rexx, guide, json, html, txt, sh). ",
    "output_mode: files_with_matches (the default: the files, newest first), content (the matching "
    "lines; -n line numbers, -A/-B/-C lines of context), count (matches per file). -i ignores case; "
    "multiline lets a pattern cross lines (. matches a newline). head_limit keeps the first N lines or "
    "entries, offset skips the first N.",
    0
};
static const char *const s_grep[] = {
    "{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\",\"description\":\"The regular "
    "expression\"},\"path\":{\"type\":\"string\",\"description\":\"A file or directory to search\"},"
    "\"glob\":{\"type\":\"string\",\"description\":\"Only files whose names match\"},\"output_mode\":"
    "{\"type\":\"string\",\"enum\":[\"content\",\"files_with_matches\",\"count\"]},",
    "\"-B\":{\"type\":\"integer\",\"description\":\"Lines before each match (content)\"},\"-A\":{\"type\":"
    "\"integer\",\"description\":\"Lines after each match (content)\"},\"-C\":{\"type\":\"integer\","
    "\"description\":\"Lines before and after (content)\"},\"-n\":{\"type\":\"boolean\",\"description\":"
    "\"Line numbers (content)\"},\"-i\":{\"type\":\"boolean\",\"description\":\"Ignore case\"},",
    "\"type\":{\"type\":\"string\",\"description\":\"A file type: c, py, md, ...\"},\"head_limit\":"
    "{\"type\":\"integer\",\"description\":\"The first N lines or entries only\"},\"offset\":{\"type\":"
    "\"integer\",\"description\":\"Skip the first N lines or entries\"},\"multiline\":{\"type\":"
    "\"boolean\",\"description\":\"Patterns may span lines\"}},\"required\":[\"pattern\"],"
    "\"additionalProperties\":false}",
    0
};

static const char *const d_bash[] = {
    "Runs a command line in the start directory through vsh (a Unix-like shell for AmigaOS: pipes, "
    "redirection, AmigaDOS commands and programs) when C:vsh is installed, else through the "
    "AmigaShell. Output and errors are returned, up to 30000 characters, with the return code (5 "
    "warn, 10 error, 20 failure; 10 or more is a failure). Commands get no input. ",
    "timeout in milliseconds (default 120000, at most 600000). run_in_background starts it and "
    "returns at once with a shell id: read its output with BashOutput, stop it with KillShell. Use "
    "Read, Glob and Grep rather than Type, List and Search. description: what the command does, in "
    "5 to 10 words.",
    0
};
static const char s_bash[] =
    "{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\",\"description\":\"The command "
    "line\"},\"timeout\":{\"type\":\"integer\",\"description\":\"Milliseconds, at most 600000\"},"
    "\"description\":{\"type\":\"string\",\"description\":\"What it does, 5-10 words\"},"
    "\"run_in_background\":{\"type\":\"boolean\",\"description\":\"Run it in the background\"}},"
    "\"required\":[\"command\"],\"additionalProperties\":false}";

static const char *const d_bash_output[] = {
    "The new output of a background shell (Bash with run_in_background) since the last call, and "
    "whether it still runs or its return code. filter: a regular expression; only matching lines are "
    "returned (the others are dropped for good).",
    0
};
static const char s_bash_output[] =
    "{\"type\":\"object\",\"properties\":{\"bash_id\":{\"type\":\"string\",\"description\":\"The shell's "
    "id\"},\"filter\":{\"type\":\"string\",\"description\":\"Only lines matching this regular "
    "expression\"}},\"required\":[\"bash_id\"],\"additionalProperties\":false}";

static const char *const d_kill_shell[] = { "Stops a background shell by its id (a break, Ctrl+C).", 0 };
static const char s_kill_shell[] =
    "{\"type\":\"object\",\"properties\":{\"shell_id\":{\"type\":\"string\",\"description\":\"The shell's "
    "id\"}},\"required\":[\"shell_id\"],\"additionalProperties\":false}";

static const char *const d_web_fetch[] = {
    "Fetches a web page (http or https) and answers prompt about it: the page is turned into Markdown "
    "and a small, fast model answers the prompt from it; you get that answer. url: a full URL. Pages "
    "are cut at 100000 characters. A redirect to another host is not followed: you are told the new "
    "URL, to fetch with a new call. Read-only.",
    0
};
static const char s_web_fetch[] =
    "{\"type\":\"object\",\"properties\":{\"url\":{\"type\":\"string\",\"description\":\"The URL to "
    "fetch\"},\"prompt\":{\"type\":\"string\",\"description\":\"What to find out from the page\"}},"
    "\"required\":[\"url\",\"prompt\"],\"additionalProperties\":false}";

static const char *const d_todo[] = {
    "Keeps a structured todo list for the current task and shows it to the user as a checklist. Use "
    "it for tasks of three steps or more and when the user gives several tasks; skip it for a single, "
    "simple step. Send the whole list each time. Each item: content (imperative: \"Run the tests\"), ",
    "activeForm (present continuous: \"Running the tests\"), status pending, in_progress or completed. "
    "Exactly one item in_progress while you work; mark each completed as soon as it is done, never "
    "ahead of time.",
    0
};
static const char s_todo[] =
    "{\"type\":\"object\",\"properties\":{\"todos\":{\"type\":\"array\",\"items\":{\"type\":\"object\","
    "\"properties\":{\"content\":{\"type\":\"string\"},\"status\":{\"type\":\"string\",\"enum\":"
    "[\"pending\",\"in_progress\",\"completed\"]},\"activeForm\":{\"type\":\"string\"}},\"required\":"
    "[\"content\",\"status\",\"activeForm\"],\"additionalProperties\":false}}},\"required\":[\"todos\"],"
    "\"additionalProperties\":false}";

static const char *const d_ask[] = {
    "Asks the user one to four questions while you work, each with two to four options (the user may "
    "also type an answer of their own): to clarify an instruction, to learn a preference, to offer "
    "directions. header: a short label, 12 characters at most. multiSelect: several options may be "
    "chosen. ",
    "Put a recommended option first and end its label with \" (Recommended)\". In plan mode ask about "
    "requirements here; ExitPlanMode asks for the plan's approval.",
    0
};
static const char *const s_ask[] = {
    "{\"type\":\"object\",\"properties\":{\"questions\":{\"type\":\"array\",\"description\":\"1 to 4 "
    "questions\",\"items\":{\"type\":\"object\",\"properties\":{\"question\":{\"type\":\"string\"},"
    "\"header\":{\"type\":\"string\"},\"options\":{\"type\":\"array\",\"description\":\"2 to 4 options\",",
    "\"items\":{\"type\":\"object\",\"properties\":{\"label\":{\"type\":\"string\"},\"description\":"
    "{\"type\":\"string\"}},\"required\":[\"label\",\"description\"],\"additionalProperties\":false}},"
    "\"multiSelect\":{\"type\":\"boolean\"}},\"required\":[\"question\",\"header\",\"options\","
    "\"multiSelect\"],\"additionalProperties\":false}}},\"required\":[\"questions\"],"
    "\"additionalProperties\":false}",
    0
};

static const char *const d_exit_plan[] = {
    "In plan mode, when the plan for an implementation task is ready: shows the plan (Markdown, "
    "concise) to the user and asks to leave plan mode and carry it out. Only for tasks that need code "
    "written, not for research. Settle open questions with AskUserQuestion first.",
    0
};
static const char s_exit_plan[] =
    "{\"type\":\"object\",\"properties\":{\"plan\":{\"type\":\"string\",\"description\":\"The plan, "
    "Markdown\"}},\"required\":[\"plan\"],\"additionalProperties\":false}";

static const char *const d_enter_plan[] = {
    "Asks the user to switch to plan mode: for a non-trivial implementation task whose approach should "
    "be agreed before code is written (several files, a design choice, unclear requirements). In plan "
    "mode only read-only tools run: explore, then present the plan with ExitPlanMode. Skip it for small, "
    "clear changes.",
    0
};
static const char s_enter_plan[] = "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}";

static const char *const d_task[] = {
    "Launches a subagent for a complex, multi-step task: it works alone with its own tools and returns "
    "one final report (you see only that, not its steps). Give a short description (3-5 words) and a "
    "complete prompt: what to do and what to return. The agent runs to its end before you go on (one "
    "at a time on this machine). ",
    "Use it for open-ended searches over many files or for work whose details would fill your "
    "context; for a known file or symbol use Read or Grep directly. subagent_type is one of:\n",
    0
};
static const char s_task[] =
    "{\"type\":\"object\",\"properties\":{\"description\":{\"type\":\"string\",\"description\":\"3-5 "
    "words\"},\"prompt\":{\"type\":\"string\",\"description\":\"The task\"},\"subagent_type\":{\"type\":"
    "\"string\",\"description\":\"The agent\"},\"model\":{\"type\":\"string\",\"enum\":[\"sonnet\","
    "\"opus\",\"haiku\"],\"description\":\"Another model for it (default: the agent's)\"}},"
    "\"required\":[\"description\",\"prompt\",\"subagent_type\"],\"additionalProperties\":false}";

static const char *const d_skill[] = {
    "Runs a skill: a packaged set of instructions for a kind of task. When a request matches a skill, "
    "call this first; its instructions come back as the result, to follow. args: optional arguments "
    "for it. Skills:\n",
    0
};
static const char s_skill[] =
    "{\"type\":\"object\",\"properties\":{\"skill\":{\"type\":\"string\",\"description\":\"The skill's "
    "name\"},\"args\":{\"type\":\"string\",\"description\":\"Arguments for it\"}},\"required\":[\"skill\"],"
    "\"additionalProperties\":false}";

static const char *const d_slash[] = {
    "Runs one of the user's custom slash commands: its prompt, with the arguments put in, comes back as "
    "the result, to carry out. command: the whole line, \"/name arguments\". Commands:\n",
    0
};
static const char s_slash[] =
    "{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\",\"description\":\"/name and "
    "its arguments\"}},\"required\":[\"command\"],\"additionalProperties\":false}";

typedef struct tdef {
    const char *name;
    const char *title;          /* what the screen calls it */
    const char *const *desc;
    const char *schema;         /* one piece ... */
    const char *const *schema_parts;    /* ... or several */
} tdef;

static const tdef defs[T_COUNT] = {
    { "Read", "Read", d_read, s_read, 0 },
    { "Write", "Write", d_write, s_write, 0 },
    { "Edit", "Update", d_edit, s_edit, 0 },
    { "MultiEdit", "Update", d_multiedit, s_multiedit, 0 },
    { "Glob", "Search", d_glob, s_glob, 0 },
    { "Grep", "Search", d_grep, 0, s_grep },
    { "Bash", "Bash", d_bash, s_bash, 0 },
    { "BashOutput", "BashOutput", d_bash_output, s_bash_output, 0 },
    { "KillShell", "Kill Shell", d_kill_shell, s_kill_shell, 0 },
    { "WebFetch", "Fetch", d_web_fetch, s_web_fetch, 0 },
    { "TodoWrite", "Update Todos", d_todo, s_todo, 0 },
    { "AskUserQuestion", "Question", d_ask, 0, s_ask },
    { "ExitPlanMode", "Plan", d_exit_plan, s_exit_plan, 0 },
    { "EnterPlanMode", "Plan Mode", d_enter_plan, s_enter_plan, 0 },
    { "Task", "Task", d_task, s_task, 0 },
    { "Skill", "Skill", d_skill, s_skill, 0 },
    { "SlashCommand", "SlashCommand", d_slash, s_slash, 0 },
};

/* the A2 names, for a model that still calls them (a resumed session) */
static const char *const old_names[][2] = {
    { "read_file", "Read" }, { "list_dir", "Glob" }, { "grep", "Grep" }, { "write_file", "Write" },
    { "edit_file", "Edit" }, { "run_command", "Bash" }, { "todo_write", "TodoWrite" }, { 0, 0 }
};

int tools_id(const char *name)
{
    int i;
    for (i = 0; i < T_COUNT; i++)
        if (!strcmp(name, defs[i].name))
            return i;
    return -1;
}

const char *tools_name(int tool)
{
    return tool >= 0 && tool < T_COUNT ? defs[tool].name : tool == T_WEB_SEARCH ? "web_search" : "?";
}

const char *tools_title(int tool)
{
    return tool >= 0 && tool < T_COUNT ? defs[tool].title : tool == T_WEB_SEARCH ? "Web Search" : "Tool";
}

static void put_schema(jw *w, int tool)
{
    int i;
    if (defs[tool].schema)
        jw_rawz(w, defs[tool].schema);
    else
        for (i = 0; defs[tool].schema_parts[i]; i++)
            jw_rawz(w, defs[tool].schema_parts[i]);
}

/* a list line for a description: "- name: text" */
static void list_line(jw *d, const char *name, const char *text, const char *extra)
{
    jw_rawz(d, "- ");
    jw_rawz(d, name);
    if (text && *text) {
        jw_rawz(d, ": ");
        jw_rawz(d, text);
    }
    if (extra && *extra) {
        jw_rawz(d, " (Tools: ");
        jw_rawz(d, extra);
        jw_rawz(d, ")");
    }
    jw_rawz(d, "\n");
}

/* the description of a tool, with the lists the provider fills in */
static int describe(cl_tools *t, int tool, jw *d)
{
    int i, n;
    for (i = 0; defs[tool].desc[i]; i++)
        jw_rawz(d, defs[tool].desc[i]);
    if (tool == T_TASK) {
        n = agent_count(t);
        for (i = 0; i < n; i++) {
            const cl_agent *a = agent_get(t, i);
            list_line(d, a->name, a->description, a->tools && *a->tools ? a->tools : "all");
        }
        return n;
    }
    if (tool == T_SKILL) {
        const cl_skill *s = 0;
        n = t->ext && t->ext->skills ? t->ext->skills(t->ext->u, &s) : 0;
        for (i = 0; i < n; i++)
            list_line(d, s[i].name, s[i].description, 0);
        return n;
    }
    if (tool == T_SLASH) {
        const cl_command *c = 0;
        int k = 0;
        n = t->ext && t->ext->commands ? t->ext->commands(t->ext->u, &c) : 0;
        for (i = 0; i < n; i++)
            if (c[i].description && *c[i].description) {
                char nm[80];
                cl_copy(nm, "/", sizeof(nm));
                cl_cat(nm, c[i].name, sizeof(nm));
                list_line(d, nm, c[i].description, 0);
                k++;
            }
        return k;
    }
    return 1;
}

static int is_haiku(const char *model)
{
    return model && !strncmp(model, "claude-haiku", 12);
}

const char *tools_json(cl_tools *t, const char *model)
{
    jw w, d;
    int i, first = 1;
    if (t->json && t->json_haiku == is_haiku(model))
        return t->json;
    free(t->json);
    t->json = 0;
    jw_init(&w);
    jw_init(&d);
    jw_raw(&w, "[", 1);
    for (i = 0; i < T_COUNT; i++) {
        if (!(t->allowed & (1ul << i)))
            continue;
        if (i == T_TASK && (t->depth || !t->api.send))
            continue;               /* no Task inside a subagent, none without a transport */
        if (i == T_WEB_FETCH && (!t->web || !t->api.send))
            continue;
        jw_reset(&d);
        if (!describe(t, i, &d))
            continue;               /* no skills, no commands: the tool is left out */
        if (!first)
            jw_raw(&w, ",", 1);
        first = 0;
        jw_rawz(&w, "{\"name\":");
        jw_strz(&w, defs[i].name);
        jw_rawz(&w, ",\"description\":");
        jw_str(&w, d.p, d.n);
        jw_rawz(&w, ",\"strict\":true,\"eager_input_streaming\":true,\"input_schema\":");
        put_schema(&w, i);
        jw_raw(&w, "}", 1);
    }
    if (t->web_search) {
        /* the server tool: the dynamic-filtering version where the model has it */
        if (!first)
            jw_raw(&w, ",", 1);
        jw_rawz(&w, is_haiku(model) ? "{\"type\":\"web_search_20250305\",\"name\":\"web_search\",\"max_uses\":5}"
                                    : "{\"type\":\"web_search_20260209\",\"name\":\"web_search\",\"max_uses\":5}");
    }
    jw_raw(&w, "]", 1);
    jw_free(&d);
    if (w.oom) {
        jw_free(&w);
        return "[]";
    }
    t->json = w.p;
    t->json_haiku = is_haiku(model);
    return t->json;
}

int tools_validate(int tool, jv in, char *err, long cap)
{
    jw s;
    jv schema;
    int rc;
    if (tool < 0 || tool >= T_COUNT) {
        cl_copy(err, "unknown tool", cap);
        return -1;
    }
    if (json_type(in) != J_OBJ) {
        cl_copy(err, "The input must be a JSON object", cap);
        return -1;
    }
    jw_init(&s);
    put_schema(&s, tool);
    if (s.oom || json_parse(s.p, s.n, &schema)) {
        jw_free(&s);
        cl_copy(err, "out of memory", cap);
        return -1;
    }
    rc = schema_check(schema, in, err, cap);
    jw_free(&s);
    if (rc)
        return -1;
    /* what a schema cannot say */
    if (tool == T_ASK_USER) {
        jv q, e, o;
        jit it;
        json_get(in, "questions", &q);
        if (json_count(q) < 1 || json_count(q) > 4) {
            cl_copy(err, "questions must hold 1 to 4 questions", cap);
            return -1;
        }
        json_iter(q, &it);
        while (json_next(&it, 0, &e)) {
            json_get(e, "options", &o);
            if (json_count(o) < 2 || json_count(o) > 4) {
                cl_copy(err, "each question needs 2 to 4 options", cap);
                return -1;
            }
        }
    }
    if (tool == T_MULTIEDIT) {
        jv e;
        json_get(in, "edits", &e);
        if (json_count(e) < 1) {
            cl_copy(err, "edits must hold at least one edit", cap);
            return -1;
        }
    }
    if ((tool == T_READ && (tl_num(in, "offset", 1) < 0 || tl_num(in, "limit", 1) < 1)) ||
        (tool == T_BASH && (tl_num(in, "timeout", 1) < 1 || tl_num(in, "timeout", 1) > 600000L)) ||
        (tool == T_GREP && (tl_num(in, "-A", 0) < 0 || tl_num(in, "-B", 0) < 0 || tl_num(in, "-C", 0) < 0 ||
                            tl_num(in, "head_limit", 0) < 0 || tl_num(in, "offset", 0) < 0))) {
        cl_copy(err, "a number is out of range (Read: offset >= 0, limit >= 1; Bash: timeout 1..600000; "
                     "Grep: no negative numbers)", cap);
        return -1;
    }
    return 0;
}

/* ---- permissions ---- */

int perm_read_only(int tool)
{
    return tool == T_READ || tool == T_GLOB || tool == T_GREP;
}

/* tools that never ask: they change nothing, or are questions themselves */
static int never_asks(int tool)
{
    return tool == T_TODO_WRITE || tool == T_BASH_OUTPUT || tool == T_KILL_SHELL || tool == T_TASK ||
           tool == T_ASK_USER || tool == T_EXIT_PLAN || tool == T_ENTER_PLAN;
}

static int is_edit(int tool)
{
    return tool == T_WRITE || tool == T_EDIT || tool == T_MULTIEDIT;
}

int perm_must_ask(const cl_perm *p, int tool, int outside)
{
    if (never_asks(tool))
        return 0;
    if (outside)
        return 1;
    if (p->mode == PERM_ACCEPT && is_edit(tool))
        return 0;
    return !(p->session & (1ul << tool));
}

int perm_refused(const cl_perm *p, int tool)
{
    return p->mode == PERM_PLAN && (is_edit(tool) || tool == T_BASH || tool == T_KILL_SHELL);
}

void perm_grant(cl_perm *p, int tool)
{
    if (perm_read_only(tool))
        p->session |= (1ul << T_READ) | (1ul << T_GLOB) | (1ul << T_GREP);
    else
        p->session |= 1ul << tool;
}

/* ---- the read set: Write and Edit need a Read first ---- */

typedef struct cl_readset {
    char (*path)[256];
    long *mtime;
    int n, cap;
} cl_readset;

static int rs_find(const cl_readset *rs, const char *p)
{
    int i;
    for (i = 0; rs && i < rs->n; i++)
        if (cl_strieq(rs->path[i], p))
            return i;
    return -1;
}

static void rs_note(cl_tools *t, const char *p)
{
    cl_readset *rs = t->rs;
    int i;
    long m = t->sys->mtime ? t->sys->mtime(t->sys->u, p) : -1;
    if (!rs)
        return;
    i = rs_find(rs, p);
    if (i < 0) {
        if (rs->n == rs->cap) {
            int nc = rs->cap ? rs->cap * 2 : 32;
            char (*np)[256] = (char (*)[256])realloc(rs->path, (size_t)nc * 256);
            long *nm;
            if (!np)
                return;
            rs->path = np;
            nm = (long *)realloc(rs->mtime, (size_t)nc * sizeof(long));
            if (!nm)
                return;
            rs->mtime = nm;
            rs->cap = nc;
        }
        i = rs->n++;
        cl_copy(rs->path[i], p, 256);
    }
    rs->mtime[i] = m;
}

/* 0 may be written; -1 with the reason written as the result */
static int rs_check(cl_tools *t, jw *out, const char *id, const char *p)
{
    int i;
    if (!t->rs)
        return 0;
    i = rs_find(t->rs, p);
    if (i < 0) {
        tl_error(t, out, id, "File has not been read yet. Read it first before writing to it.", 0);
        return -1;
    }
    if (t->sys->mtime && t->rs->mtime[i] >= 0 && t->sys->mtime(t->sys->u, p) != t->rs->mtime[i]) {
        tl_error(t, out, id,
                 "File has been modified since read, either by the user or by another program. Read it "
                 "again before attempting to write it.", 0);
        return -1;
    }
    return 0;
}

int tools_init(cl_tools *t)
{
    t->rs = (cl_readset *)calloc(1, sizeof(cl_readset));
    t->sh = shells_new();
    if (!t->allowed)
        t->allowed = (1ul << T_COUNT) - 1;
    t->web_search = 1;
    return t->rs && t->sh ? 0 : -1;
}

void tools_free(cl_tools *t)
{
    if (t->sh)
        shells_free(t->sys, t->sh);
    t->sh = 0;
    if (t->rs) {
        free(t->rs->path);
        free(t->rs->mtime);
        free(t->rs);
    }
    t->rs = 0;
    free(t->json);
    t->json = 0;
}

/* ---- helpers ---- */

char *tl_prop(jv in, const char *key, long *len)
{
    jv v;
    char *s;
    if (json_get(in, key, &v) && json_type(v) == J_STR)
        return json_strdup(v, len);
    s = (char *)malloc(1);
    if (s)
        s[0] = 0;
    if (len)
        *len = 0;
    return s;
}

long tl_num(jv in, const char *key, long def)
{
    jv v;
    return json_get(in, key, &v) ? json_long(v, def) : def;
}

int tl_bool(jv in, const char *key)
{
    jv v;
    return json_get(in, key, &v) && json_type(v) == J_TRUE;
}

void tl_summary(char *out, long cap, const char *a, const char *b)
{
    long o = 0;
    const char *src[2];
    int k;
    src[0] = a;
    src[1] = b;
    for (k = 0; k < 2 && src[k]; k++) {
        const char *s = src[k];
        if (k && o < cap - 3) {
            out[o++] = ' ';
            out[o++] = ' ';
        }
        for (; *s && o < cap - 1; s++)
            out[o++] = ((unsigned char)*s < 0x20 || *s == 0x7f) ? '?' : *s;
    }
    out[o] = 0;
}

int tl_is_binary(const char *s, long n)
{
    long i;
    if (n > 4096)
        n = 4096;
    for (i = 0; i < n; i++)
        if (!s[i])
            return 1;
    return 0;
}

static int valid_utf8(const char *s, long n)
{
    long i = 0;
    unsigned long cp;
    while (i < n) {
        int k = json_utf8(s + i, n - i, &cp);
        if (!k)
            return 0;
        i += k;
    }
    return 1;
}

/* Latin-1 to UTF-8, malloc'ed */
static char *latin1_to_utf8(const char *s, long n, long *on)
{
    char *o = (char *)malloc((size_t)n * 2 + 1);
    long i, k = 0;
    if (!o)
        return 0;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80)
            o[k++] = (char)c;
        else {
            o[k++] = (char)(0xc0 | (c >> 6));
            o[k++] = (char)(0x80 | (c & 0x3f));
        }
    }
    o[k] = 0;
    *on = k;
    return o;
}

/* UTF-8 to Latin-1 in place ('?' for what Latin-1 lacks): the new length */
static long utf8_to_latin1(char *s, long n)
{
    long i = 0, k = 0;
    unsigned long cp;
    while (i < n) {
        int l = json_utf8(s + i, n - i, &cp);
        if (!l) {
            l = 1;
            cp = (unsigned char)s[i];
        }
        s[k++] = cp < 0x100 ? (char)cp : '?';
        i += l;
    }
    return k;
}

int tl_resolve(cl_tools *t, const char *arg, char *full, long cap, int *outside)
{
    char canon[512], parent[512];
    const char *name;
    if (path_join(t->root, *arg ? arg : "", full, cap))
        return -1;
    if (t->sys->canon(t->sys->u, full, canon, sizeof(canon)) == 0) {
        *outside = !path_inside(t->root, canon);
        return 0;
    }
    /* not there yet (a new file): its directory's canonical name */
    if (path_parent(full, parent, sizeof(parent)) == 0 &&
        t->sys->canon(t->sys->u, parent[0] ? parent : t->root, canon, sizeof(canon)) == 0) {
        name = full + strlen(parent);
        while (*name == '/')
            name++;
        cl_cat(canon, canon[0] && canon[strlen(canon) - 1] != ':' && canon[strlen(canon) - 1] != '/' ? "/" : "",
               sizeof(canon));
        cl_cat(canon, name, sizeof(canon));
        *outside = !path_inside(t->root, canon);
        return 0;
    }
    *outside = !path_inside(t->root, full);
    return 0;
}

void tl_result(cl_tools *t, jw *out, const char *id, const char *text, long n, int is_error)
{
    if (t->result)
        t->result(t->u, t->cur, t->cur_in, t->cur_inn, is_error, text, n);
    jw_rawz(out, "{\"type\":\"tool_result\",\"tool_use_id\":");
    jw_strz(out, id);
    jw_rawz(out, ",\"content\":");
    jw_str(out, text, n);
    if (is_error)
        jw_rawz(out, ",\"is_error\":true");
    jw_raw(out, "}", 1);
}

void tl_error(cl_tools *t, jw *out, const char *id, const char *a, const char *b)
{
    jw m;
    jw_init(&m);
    jw_rawz(&m, a);
    if (b)
        jw_rawz(&m, b);
    tl_result(t, out, id, m.p ? m.p : a, m.p ? m.n : (long)strlen(a), 1);
    jw_free(&m);
}

int tl_gate(cl_tools *t, jw *out, const char *id, int tool, const char *what, int outside, int show)
{
    int ans;
    if (perm_refused(&t->perm, tool)) {
        tl_error(t, out, id, "plan mode is on: only tools that change nothing run now (Read, Glob, Grep, "
                             "WebFetch, Task, the questions). Present your plan with ExitPlanMode; the user "
                             "leaves plan mode to let you carry it out", 0);
        return -1;
    }
    if (show && t->show)
        t->show(t->u, defs[tool].name, what);
    if (!perm_must_ask(&t->perm, tool, outside))
        return 0;
    ans = t->ask ? t->ask(t->u, defs[tool].name, what, outside) : ASK_NO;
    if (ans == ASK_STOP) {
        t->stop = 1;
        tl_error(t, out, id, "the user stopped this tool call and will tell you what to do differently; "
                             "wait for their message", 0);
        return -1;
    }
    if (ans == ASK_NO) {
        tl_error(t, out, id, "the user declined this tool call", 0);
        return -1;
    }
    if (ans == ASK_SESSION && !outside)
        perm_grant(&t->perm, tool);
    return 0;
}

/* ---- Read ---- */

/* the number right-aligned in 6 columns, a tab (cat -n) */
static void line_no(jw *r, long no)
{
    char num[16];
    int l = cl_ltoa(no, num);
    while (l++ < 6)
        jw_raw(r, " ", 1);
    jw_rawz(r, num);
    jw_raw(r, "\t", 1);
}

/* the length of s[0..n) cut to at most max bytes, not inside a UTF-8 character */
static long cut_at(const char *s, long n, long max)
{
    if (n <= max)
        return n;
    while (max > 0 && ((unsigned char)s[max] & 0xc0) == 0x80)
        max--;
    return max;
}

static void run_read(cl_tools *t, jw *out, const char *id, jv in)
{
    char full[512], what[300], num[16];
    char *arg = tl_prop(in, "file_path", 0), *b = 0;
    long n = 0, offset = tl_num(in, "offset", 0), limit = tl_num(in, "limit", 0), i, line = 1, shown = 0;
    int outside = 0, rc, part = offset > 0 || limit > 0, kind;
    jw r;
    if (!arg) {
        tl_error(t, out, id, "out of memory", 0);
        return;
    }
    if (tl_resolve(t, arg, full, sizeof(full), &outside)) {
        tl_error(t, out, id, "not a usable path (it climbs above a volume's root): ", arg);
        free(arg);
        return;
    }
    free(arg);
    tl_summary(what, sizeof(what), full, 0);
    if (tl_gate(t, out, id, T_READ, what, outside, 1))
        return;
    kind = t->sys->kind(t->sys->u, full);
    if (kind == 2) {
        tl_error(t, out, id, "this is a directory, not a file (Glob lists what is in it): ", full);
        return;
    }
    if (!kind) {
        tl_error(t, out, id, "File does not exist: ", full);
        return;
    }
    rc = t->sys->read(t->sys->u, full, part ? READ_PART : READ_WHOLE, &b, &n);
    if (rc == SYS_TOO_BIG) {
        tl_error(t, out, id, part ? "the file is larger than 2 MB: Grep it, or read it with Bash (Type with a range)"
                                  : "the file is larger than 256 KB: read it in parts with offset and limit, "
                                    "or Grep for what you need",
                 0);
        return;
    }
    if (rc) {
        tl_error(t, out, id, "cannot read the file: ", t->sys->err(t->sys->u));
        return;
    }
    if (tl_is_binary(b, n)) {
        free(b);
        tl_error(t, out, id, "this is a binary file, not text: ", full);
        return;
    }
    rs_note(t, full);
    jw_init(&r);
    if (!n) {
        jw_rawz(&r, "<system-reminder>Warning: the file exists but its contents are empty.</system-reminder>");
    } else {
        if (offset < 1)
            offset = 1;
        if (limit < 1)
            limit = READ_LINES;
        i = 0;
        while (i < n && shown < limit && r.n < 4L * READ_WHOLE) {
            long e = i, len;
            while (e < n && b[e] != '\n')
                e++;
            if (line >= offset) {
                len = e - i;
                if (len && b[i + len - 1] == '\r')
                    len--;
                line_no(&r, line);
                jw_raw(&r, b + i, cut_at(b + i, len, LINE_MAX_CH));
                jw_raw(&r, "\n", 1);
                shown++;
            }
            line++;
            i = e + 1;
        }
        if (!shown) {
            long total = 0;
            for (i = 0; i < n; i++)
                total += b[i] == '\n';
            total += b[n - 1] != '\n';
            jw_rawz(&r, "<system-reminder>Warning: the file exists but is shorter than the provided offset (");
            cl_ltoa(offset, num);
            jw_rawz(&r, num);
            jw_rawz(&r, "). The file has ");
            cl_ltoa(total, num);
            jw_rawz(&r, num);
            jw_rawz(&r, " lines.</system-reminder>");
        } else if (i < n) {
            long rest = 0, k;
            for (k = i; k < n; k++)
                rest += b[k] == '\n';
            rest += b[n - 1] != '\n';
            jw_rawz(&r, "(");
            cl_ltoa(rest, num);
            jw_rawz(&r, num);
            jw_rawz(&r, " more lines: read on with offset ");
            cl_ltoa(line, num);
            jw_rawz(&r, num);
            jw_rawz(&r, ")\n");
        }
    }
    free(b);
    tl_result(t, out, id, r.p ? r.p : "", r.n, 0);
    jw_free(&r);
}

/* ---- Write ---- */

static void run_write(cl_tools *t, jw *out, const char *id, jv in)
{
    char full[512], what[300];
    long cn = 0, on = 0;
    char *arg = tl_prop(in, "file_path", 0), *content = tl_prop(in, "content", &cn), *old = 0;
    int outside = 0, kind, latin = 0;
    jw m;
    if (!arg || !content) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    if (tl_resolve(t, arg, full, sizeof(full), &outside)) {
        tl_error(t, out, id, "not a usable path (it climbs above a volume's root): ", arg);
        goto done;
    }
    tl_summary(what, sizeof(what), full, 0);
    if (t->show)
        t->show(t->u, defs[T_WRITE].name, what);
    kind = t->sys->kind(t->sys->u, full);
    if (kind == 2) {
        tl_error(t, out, id, "that is a directory: ", full);
        goto done;
    }
    if (kind == 1 && rs_check(t, out, id, full))
        goto done;
    if (perm_refused(&t->perm, T_WRITE)) {
        tl_gate(t, out, id, T_WRITE, what, outside, 0);
        goto done;
    }
    if (kind == 1 && t->sys->read(t->sys->u, full, EDIT_MAX, &old, &on))
        old = 0;
    latin = old && !valid_utf8(old, on) && !tl_is_binary(old, on);
    if (t->preview) {
        long un = on;
        char *u8 = latin ? latin1_to_utf8(old, on, &un) : 0;
        t->preview(t->u, T_WRITE, full, u8 ? u8 : old, old ? un : 0, content, cn);
        free(u8);
    }
    if (tl_gate(t, out, id, T_WRITE, what, outside, 0))
        goto done;
    if (latin)
        cn = utf8_to_latin1(content, cn);
    if (t->sys->write(t->sys->u, full, content, cn)) {
        tl_error(t, out, id, "cannot write the file: ", t->sys->err(t->sys->u));
        goto done;
    }
    rs_note(t, full);
    jw_init(&m);
    jw_rawz(&m, kind == 1 ? "The file " : "File created successfully at: ");
    jw_rawz(&m, full);
    if (kind == 1)
        jw_rawz(&m, latin ? " has been updated (kept as Latin-1)." : " has been updated.");
    tl_result(t, out, id, m.p, m.n, 0);
    jw_free(&m);
done:
    free(arg);
    free(content);
    free(old);
}

/* ---- Edit and MultiEdit ---- */

typedef struct pedit {
    char *before, *after;
    long bn, an;
    int latin, created;
    long first;                 /* where the first change starts in after */
} pedit;

static long count_of(const char *h, long hn, const char *nd, long nn, long *first)
{
    long i, c = 0;
    *first = -1;
    for (i = 0; i + nn <= hn; i++)
        if (!memcmp(h + i, nd, (size_t)nn)) {
            if (*first < 0)
                *first = i;
            c++;
            i += nn - 1;
        }
    return c;
}

/* one replacement on e->after (in place of it): 0, or -1 with the
 * reason in err (Claude Code's wording) */
static int apply_one(pedit *e, const char *olds, long on, const char *news, long nn, int all, jw *err)
{
    long c, at, i, k, outn;
    char *o;
    if (on == nn && !memcmp(olds, news, (size_t)on)) {
        jw_rawz(err, "No changes to make: old_string and new_string are exactly the same.");
        return -1;
    }
    c = count_of(e->after, e->an, olds, on, &at);
    if (!c) {
        jw_rawz(err, "String to replace not found in file.\nString: ");
        jw_raw(err, olds, on);
        return -1;
    }
    if (c > 1 && !all) {
        char num[16];
        cl_ltoa(c, num);
        jw_rawz(err, "Found ");
        jw_rawz(err, num);
        jw_rawz(err, " matches of the string to replace, but replace_all is false. To replace all occurrences, "
                     "set replace_all to true. To replace only one occurrence, please provide more context to "
                     "uniquely identify the instance.\nString: ");
        jw_raw(err, olds, on);
        return -1;
    }
    outn = e->an + (all ? c : 1) * (nn - on);
    o = (char *)malloc((size_t)outn + 1);
    if (!o) {
        jw_rawz(err, "out of memory");
        return -1;
    }
    for (i = 0, k = 0; i < e->an;) {
        if (i + on <= e->an && !memcmp(e->after + i, olds, (size_t)on) && (all || i == at)) {
            memcpy(o + k, news, (size_t)nn);
            k += nn;
            i += on;
        } else
            o[k++] = e->after[i++];
    }
    o[k] = 0;
    if (e->first < 0 || at < e->first)
        e->first = at;
    free(e->after);
    e->after = o;
    e->an = k;
    return 0;
}

static void edit_free(pedit *e)
{
    free(e->before);
    free(e->after);
    memset(e, 0, sizeof(*e));
}

/* the file read into e (before and a copy as after), as UTF-8 */
static int edit_load(cl_tools *t, jw *out, const char *id, const char *path, pedit *e)
{
    char *b = 0;
    long n = 0;
    int rc = t->sys->read(t->sys->u, path, EDIT_MAX, &b, &n);
    if (rc) {
        tl_error(t, out, id, rc == SYS_TOO_BIG ? "the file is larger than 1 MB: " : "cannot read the file: ",
                 rc == SYS_TOO_BIG ? path : t->sys->err(t->sys->u));
        return -1;
    }
    if (tl_is_binary(b, n)) {
        free(b);
        tl_error(t, out, id, "this is a binary file, not text: ", path);
        return -1;
    }
    e->latin = !valid_utf8(b, n);
    if (e->latin) {
        e->before = latin1_to_utf8(b, n, &e->bn);
        free(b);
    } else {
        e->before = b;
        e->bn = n;
    }
    e->after = e->before ? (char *)malloc((size_t)e->bn + 1) : 0;
    if (!e->after) {
        tl_error(t, out, id, "out of memory", 0);
        return -1;
    }
    memcpy(e->after, e->before, (size_t)e->bn + 1);
    e->an = e->bn;
    e->first = -1;
    return 0;
}

/* "Here's the result of running `cat -n` on a snippet": lines around the
 * first change */
static void snippet(jw *m, const char *s, long n, long at)
{
    long line = 1, i, from, k;
    for (i = 0; i < at && i < n; i++)
        line += s[i] == '\n';
    from = line > 4 ? line - 4 : 1;
    for (i = 0, k = 1; i < n && k < from; i++)
        k += s[i] == '\n';
    for (; i < n && k < line + 5; k++) {
        long e = i;
        while (e < n && s[e] != '\n')
            e++;
        line_no(m, k);
        jw_raw(m, s + i, cut_at(s + i, e - i, LINE_MAX_CH));
        jw_raw(m, "\n", 1);
        i = e + 1;
    }
}

static void run_edit(cl_tools *t, jw *out, const char *id, jv in, int tool)
{
    char full[512], what[300], num[16];
    char *arg = tl_prop(in, "file_path", 0);
    int outside = 0, kind, k = 0;
    pedit e;
    jw err, m;
    jv edits, ed;
    jit it;
    memset(&e, 0, sizeof(e));
    jw_init(&err);
    if (!arg) {
        tl_error(t, out, id, "out of memory", 0);
        return;
    }
    if (tl_resolve(t, arg, full, sizeof(full), &outside)) {
        tl_error(t, out, id, "not a usable path (it climbs above a volume's root): ", arg);
        free(arg);
        return;
    }
    free(arg);
    tl_summary(what, sizeof(what), full, 0);
    if (t->show)
        t->show(t->u, defs[tool].name, what);
    kind = t->sys->kind(t->sys->u, full);
    if (kind == 2) {
        tl_error(t, out, id, "that is a directory: ", full);
        return;
    }
    /* the edits, worked out before anything is asked */
    if (tool == T_EDIT) {
        long on = 0;
        char *olds = tl_prop(in, "old_string", &on);
        if (!kind && olds && !on) {
            /* an empty old_string on a new file: create it */
            e.created = 1;
            e.after = tl_prop(in, "new_string", &e.an);
            e.first = 0;
            free(olds);
            if (!e.after) {
                tl_error(t, out, id, "out of memory", 0);
                return;
            }
        } else {
            free(olds);
            if (!kind) {
                tl_error(t, out, id, "File does not exist: ", full);
                return;
            }
            if (rs_check(t, out, id, full) || edit_load(t, out, id, full, &e))
                goto done;
        }
    } else {
        if (!kind) {
            tl_error(t, out, id, "File does not exist: ", full);
            return;
        }
        if (rs_check(t, out, id, full) || edit_load(t, out, id, full, &e))
            goto done;
    }
    if (!e.created) {
        if (tool == T_EDIT)
            edits.p = 0;
        else
            json_get(in, "edits", &edits);
        if (tool == T_EDIT) {
            long on = 0, nn = 0;
            char *olds = tl_prop(in, "old_string", &on), *news = tl_prop(in, "new_string", &nn);
            int bad = !olds || !news;
            if (!bad && !on)
                jw_rawz(&err, "old_string is empty: the file exists; give the text to replace, or use Write");
            bad = bad || !on || apply_one(&e, olds, on, news, nn, tl_bool(in, "replace_all"), &err);
            free(olds);
            free(news);
            if (bad) {
                tl_error(t, out, id, err.p ? err.p : "out of memory", 0);
                goto done;
            }
        } else {
            json_iter(edits, &it);
            while (json_next(&it, 0, &ed)) {
                long on = 0, nn = 0;
                char *olds = tl_prop(ed, "old_string", &on), *news = tl_prop(ed, "new_string", &nn);
                int bad = !olds || !news;
                k++;
                cl_ltoa(k, num);
                if (!bad && !on) {
                    jw_rawz(&err, "old_string is empty");
                    bad = 1;
                } else if (!bad)
                    bad = apply_one(&e, olds, on, news, nn, tl_bool(ed, "replace_all"), &err);
                free(olds);
                free(news);
                if (bad) {
                    jw m2;
                    jw_init(&m2);
                    jw_rawz(&m2, "Edit ");
                    jw_rawz(&m2, num);
                    jw_rawz(&m2, " of ");
                    cl_ltoa(json_count(edits), num);
                    jw_rawz(&m2, num);
                    jw_rawz(&m2, " failed, so none was made: ");
                    jw_raw(&m2, err.p ? err.p : "", err.n);
                    tl_error(t, out, id, m2.p ? m2.p : "out of memory", 0);
                    jw_free(&m2);
                    goto done;
                }
            }
        }
    }
    if (perm_refused(&t->perm, tool)) {
        tl_gate(t, out, id, tool, what, outside, 0);
        goto done;
    }
    if (t->preview)
        t->preview(t->u, tool, full, e.before, e.bn, e.after, e.an);
    if (tl_gate(t, out, id, tool, what, outside, 0))
        goto done;
    {
        long rn = e.an;
        if (e.latin)
            rn = utf8_to_latin1(e.after, rn);
        if (t->sys->write(t->sys->u, full, e.after, rn)) {
            tl_error(t, out, id, "cannot write the file: ", t->sys->err(t->sys->u));
            goto done;
        }
        if (e.latin) {
            /* back to UTF-8 for the snippet */
            char *u = latin1_to_utf8(e.after, rn, &e.an);
            free(e.after);
            e.after = u;
        }
    }
    rs_note(t, full);
    jw_init(&m);
    if (e.created) {
        jw_rawz(&m, "File created successfully at: ");
        jw_rawz(&m, full);
    } else if (tool == T_MULTIEDIT) {
        cl_ltoa(k, num);
        jw_rawz(&m, "Applied ");
        jw_rawz(&m, num);
        jw_rawz(&m, k == 1 ? " edit to " : " edits to ");
        jw_rawz(&m, full);
        jw_rawz(&m, e.latin ? " (kept as Latin-1)." : ".");
    } else if (tl_bool(in, "replace_all")) {
        jw_rawz(&m, "The file ");
        jw_rawz(&m, full);
        jw_rawz(&m, " has been updated. All occurrences were replaced");
        jw_rawz(&m, e.latin ? " (kept as Latin-1)." : ".");
    } else {
        jw_rawz(&m, "The file ");
        jw_rawz(&m, full);
        jw_rawz(&m, e.latin ? " has been updated (kept as Latin-1)." : " has been updated.");
        if (e.after) {
            jw_rawz(&m, " Here's the result of running `cat -n` on a snippet of the edited file:\n");
            snippet(&m, e.after, e.an, e.first);
        }
    }
    tl_result(t, out, id, m.p ? m.p : "", m.n, 0);
    jw_free(&m);
done:
    jw_free(&err);
    edit_free(&e);
}

/* ---- Bash (in the foreground) ---- */

static void run_bash(cl_tools *t, jw *out, const char *id, jv in)
{
    char what[300], num[16];
    char *cmd = tl_prop(in, "command", 0), *buf;
    long n = 0, rc = 0, ms = tl_num(in, "timeout", 0);
    int r, secs;
    jw res;
    if (!cmd) {
        tl_error(t, out, id, "out of memory", 0);
        return;
    }
    tl_summary(what, sizeof(what), cmd, 0);
    if (tl_gate(t, out, id, T_BASH, what, 0, 1)) {
        free(cmd);
        return;
    }
    if (tl_bool(in, "run_in_background")) {
        shells_start(t, out, id, cmd);
        free(cmd);
        return;
    }
    secs = ms > 0 ? (int)((ms + 999) / 1000) : t->timeout_s > 0 ? t->timeout_s : 120;
    buf = (char *)malloc(TL_OUT_MAX + 1);
    if (!buf) {
        free(cmd);
        tl_error(t, out, id, "out of memory", 0);
        return;
    }
    r = t->sys->run(t->sys->u, cmd, secs, buf, TL_OUT_MAX, &n, &rc);
    free(cmd);
    if (r == -1) {
        free(buf);
        tl_error(t, out, id, "the command did not start: ", t->sys->err(t->sys->u));
        return;
    }
    jw_init(&res);
    if (r == SYS_TIMEOUT) {
        jw_rawz(&res, "The command ran out of time (");
        cl_ltoa(secs, num);
        jw_rawz(&res, num);
        jw_rawz(&res, " s) and was sent a break (Ctrl+C).\n");
    } else if (r == SYS_BREAK)
        jw_rawz(&res, "The user stopped the command (Ctrl+C).\n");
    jw_rawz(&res, "Return code ");
    cl_ltoa(rc, num);
    jw_rawz(&res, num);
    jw_rawz(&res, ".\n");
    jw_raw(&res, buf, n);
    if (n >= TL_OUT_MAX)
        jw_rawz(&res, "\n(output cut at 30000 characters)");
    tl_result(t, out, id, res.p, res.n, r != 0 || rc >= 10);
    jw_free(&res);
    free(buf);
}

/* ---- TodoWrite ---- */

static void run_todo(cl_tools *t, jw *out, const char *id)
{
    static const char ok[] = "Todos have been modified successfully. Ensure that you continue to use the todo "
                             "list to track your progress. Please proceed with the current tasks if applicable";
    tl_result(t, out, id, ok, (long)sizeof(ok) - 1, 0);
}

/* ---- the questions ---- */

static void run_ask(cl_tools *t, jw *out, const char *id, jv in)
{
    jv qs, q, opts, o, x;
    jit it, oi;
    jw ans;
    int first = 1;
    json_get(in, "questions", &qs);
    if (t->show)
        t->show(t->u, defs[T_ASK_USER].name, "");
    if (!t->choose) {
        tl_error(t, out, id, "there is nobody to ask here (no screen); go on with your best judgement", 0);
        return;
    }
    jw_init(&ans);
    jw_rawz(&ans, "User has answered your questions: ");
    json_iter(qs, &it);
    while (json_next(&it, 0, &q)) {
        char *labels[4], *descs[4], *qt, *hd, other[400];
        long l;
        int n = 0, i, c, multi = tl_bool(q, "multiSelect");
        unsigned picked = 0;
        qt = tl_prop(q, "question", &l);
        hd = tl_prop(q, "header", &l);
        json_get(q, "options", &opts);
        json_iter(opts, &oi);
        while (n < 4 && json_next(&oi, 0, &o)) {
            labels[n] = json_get(o, "label", &x) ? json_strdup(x, &l) : 0;
            descs[n] = json_get(o, "description", &x) ? json_strdup(x, &l) : 0;
            n++;
        }
        other[0] = 0;
        c = t->choose(t->u, hd ? hd : "", qt ? qt : "", (const char *const *)labels, (const char *const *)descs,
                      n, CH_OTHER | (multi ? CH_MULTI : 0), &picked, other, sizeof(other));
        if (c >= 0) {
            if (!first)
                jw_rawz(&ans, ", ");
            first = 0;
            jw_raw(&ans, "\"", 1);
            jw_rawz(&ans, qt ? qt : "");
            jw_rawz(&ans, "\"=\"");
            if (c == n)
                jw_rawz(&ans, other);
            else if (multi) {
                int k = 0;
                for (i = 0; i < n; i++)
                    if (picked & (1u << i)) {
                        if (k++)
                            jw_rawz(&ans, ", ");
                        jw_rawz(&ans, labels[i] ? labels[i] : "");
                    }
                if (other[0]) {
                    if (k)
                        jw_rawz(&ans, ", ");
                    jw_rawz(&ans, other);
                }
            } else
                jw_rawz(&ans, labels[c] ? labels[c] : "");
            jw_raw(&ans, "\"", 1);
        }
        for (i = 0; i < n; i++) {
            free(labels[i]);
            free(descs[i]);
        }
        free(qt);
        free(hd);
        if (c < 0) {
            jw_free(&ans);
            tl_error(t, out, id, "The user declined to answer your questions. Ask what they would like instead, "
                                 "or go on without the answers if you can.", 0);
            return;
        }
    }
    jw_rawz(&ans, ". You can now continue with the user's answers in mind.");
    tl_result(t, out, id, ans.p, ans.n, 0);
    jw_free(&ans);
}

static void run_exit_plan(cl_tools *t, jw *out, const char *id, jv in)
{
    static const char *const opts[] = { "Yes, and auto-accept edits", "Yes, and manually approve edits",
                                        "No, keep planning" };
    long pn = 0;
    char *plan = tl_prop(in, "plan", &pn);
    unsigned picked = 0;
    char other[300];
    int c;
    if (!plan) {
        tl_error(t, out, id, "out of memory", 0);
        return;
    }
    if (t->show)
        t->show(t->u, defs[T_EXIT_PLAN].name, "");
    if (t->perm.mode != PERM_PLAN) {
        free(plan);
        tl_error(t, out, id, "plan mode is not on: go ahead with the task", 0);
        return;
    }
    if (t->plan)
        t->plan(t->u, plan, pn);
    free(plan);
    other[0] = 0;
    c = t->choose ? t->choose(t->u, "Plan", "Would you like to proceed?", opts, 0, 3, CH_OTHER, &picked, other,
                              sizeof(other))
                  : -1;
    if (c == 0 || c == 1) {
        static const char ok[] = "User has approved your plan. You can now start coding. Start with updating "
                                 "your todo list if applicable";
        t->perm.mode = c == 0 ? PERM_ACCEPT : PERM_DEFAULT;
        tl_result(t, out, id, ok, (long)sizeof(ok) - 1, 0);
        return;
    }
    if (c == 3 && other[0]) {
        tl_error(t, out, id, "The user wants changes to the plan before it is carried out; plan mode stays on. "
                             "Their words: ", other);
        return;
    }
    tl_error(t, out, id, "The user doesn't want to proceed with this plan yet; plan mode stays on. Ask what to "
                         "change, or refine the plan.", 0);
}

static void run_enter_plan(cl_tools *t, jw *out, const char *id)
{
    static const char *const opts[] = { "Yes, enter plan mode", "No, start implementing now" };
    unsigned picked = 0;
    char other[16];
    int c;
    if (t->show)
        t->show(t->u, defs[T_ENTER_PLAN].name, "");
    if (t->perm.mode == PERM_PLAN) {
        tl_error(t, out, id, "plan mode is already on", 0);
        return;
    }
    c = t->choose ? t->choose(t->u, "Plan mode", "Claude wants to enter plan mode to explore and design an "
                                                 "approach first. Enter plan mode?",
                              opts, 0, 2, 0, &picked, other, sizeof(other))
                  : -1;
    if (c == 0) {
        static const char ok[] =
            "Entered plan mode. You should now focus on exploring the codebase and designing an implementation "
            "approach. In plan mode only tools that change nothing run (Read, Glob, Grep, WebFetch, Task, "
            "AskUserQuestion). When the plan is ready, present it with ExitPlanMode.";
        t->perm.mode = PERM_PLAN;
        tl_result(t, out, id, ok, (long)sizeof(ok) - 1, 0);
        return;
    }
    tl_error(t, out, id, "The user declined plan mode: go ahead with the task directly.", 0);
}

/* ---- Skill and SlashCommand ---- */

static void run_skill(cl_tools *t, jw *out, const char *id, jv in)
{
    const cl_skill *s = 0;
    int n = t->ext && t->ext->skills ? t->ext->skills(t->ext->u, &s) : 0, i;
    char *name = tl_prop(in, "skill", 0), *args = tl_prop(in, "args", 0), *b = 0, dir[512], what[200];
    long bn = 0;
    const char *body;
    jw m;
    if (!name || !args) {
        free(name);
        free(args);
        tl_error(t, out, id, "out of memory", 0);
        return;
    }
    for (i = 0; i < n; i++)
        if (cl_strieq(s[i].name, name))
            break;
    if (i == n) {
        tl_error(t, out, id, "Unknown skill: ", name);
        goto done;
    }
    tl_summary(what, sizeof(what), s[i].name, 0);
    if (tl_gate(t, out, id, T_SKILL, what, 0, 1))
        goto done;
    if (t->sys->read(t->sys->u, s[i].path, 256L * 1024, &b, &bn)) {
        tl_error(t, out, id, "cannot read the skill: ", t->sys->err(t->sys->u));
        goto done;
    }
    /* the frontmatter (--- ... ---) is the loader's; the body is the instructions */
    body = b;
    if (!strncmp(b, "---", 3)) {
        const char *e = strstr(b + 3, "\n---");
        if (e) {
            body = e + 4;
            while (*body == '\r' || *body == '\n')
                body++;
        }
    }
    if (path_parent(s[i].path, dir, sizeof(dir)))
        cl_copy(dir, s[i].path, sizeof(dir));
    jw_init(&m);
    jw_rawz(&m, "Launching skill: ");
    jw_rawz(&m, s[i].name);
    jw_rawz(&m, "\nBase directory for this skill: ");
    jw_rawz(&m, dir);
    jw_rawz(&m, "\n\n");
    {
        long l = bn - (long)(body - b);
        while (l > 0 && (body[l - 1] == '\n' || body[l - 1] == '\r' || body[l - 1] == ' '))
            l--;
        jw_raw(&m, body, l);
    }
    if (*args) {
        jw_rawz(&m, "\n\nARGUMENTS: ");
        jw_rawz(&m, args);
    }
    tl_result(t, out, id, m.p, m.n, 0);
    jw_free(&m);
done:
    free(b);
    free(name);
    free(args);
}

static void run_slash(cl_tools *t, jw *out, const char *id, jv in)
{
    char *line = tl_prop(in, "command", 0), name[64], err[300], what[300];
    const char *p, *args;
    long k = 0;
    jw m;
    if (!line) {
        tl_error(t, out, id, "out of memory", 0);
        return;
    }
    p = line;
    while (*p == ' ')
        p++;
    if (*p == '/')
        p++;
    while (p[k] && p[k] != ' ' && k < (long)sizeof(name) - 1) {
        name[k] = p[k];
        k++;
    }
    name[k] = 0;
    args = p + k;
    while (*args == ' ')
        args++;
    if (!t->ext || !t->ext->expand) {
        tl_error(t, out, id, "there are no custom slash commands here", 0);
        free(line);
        return;
    }
    tl_summary(what, sizeof(what), line, 0);
    if (tl_gate(t, out, id, T_SLASH, what, 0, 1)) {
        free(line);
        return;
    }
    jw_init(&m);
    jw_rawz(&m, "Launching command /");
    jw_rawz(&m, name);
    jw_rawz(&m, ". Carry out these instructions:\n\n");
    err[0] = 0;
    if (t->ext->expand(t->ext->u, name, args, &m, err, sizeof(err)))
        tl_error(t, out, id, "Unknown slash command: /", err[0] ? err : name);
    else
        tl_result(t, out, id, m.p, m.n, 0);
    jw_free(&m);
    free(line);
}

/* ---- the call ---- */

void tools_run(cl_tools *t, const char *id, const char *name, int input_ok,
               const char *raw, long rawn, jw *out)
{
    int tool = tools_id(name);
    jv in;
    char err[300];
    t->cur = tool;
    t->cur_in = raw ? raw : "";
    t->cur_inn = raw ? rawn : 0;
    if (tool < 0) {
        int i;
        for (i = 0; old_names[i][0]; i++)
            if (!strcmp(name, old_names[i][0])) {
                char m[160];
                cl_copy(m, old_names[i][0], sizeof(m));
                cl_cat(m, " is no longer a tool; use ", sizeof(m));
                cl_cat(m, old_names[i][1], sizeof(m));
                cl_cat(m, " instead", sizeof(m));
                tl_error(t, out, id, m, 0);
                return;
            }
        tl_error(t, out, id, "No such tool available: ", name);
        return;
    }
    if (!(t->allowed & (1ul << tool)) || (tool == T_TASK && t->depth)) {
        tl_error(t, out, id, "No such tool available here: ", name);
        return;
    }
    if (t->stop) {
        /* the user stopped this round at an earlier call of it */
        tl_error(t, out, id, "not run: the user stopped at an earlier tool call and will say what to do instead", 0);
        return;
    }
    if (!input_ok || json_parse(raw, rawn, &in)) {
        char cut[300];
        long k = rawn < (long)sizeof(cut) - 1 ? rawn : (long)sizeof(cut) - 1;
        memcpy(cut, raw ? raw : "", (size_t)(raw ? k : 0));
        cut[raw ? k : 0] = 0;
        tl_error(t, out, id, "the tool input was not valid JSON; send the call again. It began: ", cut);
        return;
    }
    if (tools_validate(tool, in, err, sizeof(err))) {
        char m[400];
        cl_copy(m, "InputValidationError: ", sizeof(m));
        cl_cat(m, defs[tool].name, sizeof(m));
        cl_cat(m, " failed due to the following issue:\n", sizeof(m));
        cl_cat(m, err, sizeof(m));
        tl_error(t, out, id, m, 0);
        return;
    }
    switch (tool) {
    case T_READ:
        run_read(t, out, id, in);
        break;
    case T_WRITE:
        run_write(t, out, id, in);
        break;
    case T_EDIT:
    case T_MULTIEDIT:
        run_edit(t, out, id, in, tool);
        break;
    case T_GLOB:
        search_glob(t, out, id, in);
        break;
    case T_GREP:
        search_grep(t, out, id, in);
        break;
    case T_BASH:
        run_bash(t, out, id, in);
        break;
    case T_BASH_OUTPUT:
        shells_output(t, out, id, in);
        break;
    case T_KILL_SHELL:
        shells_kill(t, out, id, in);
        break;
    case T_WEB_FETCH:
        webfetch_run(t, out, id, in);
        break;
    case T_TODO_WRITE:
        run_todo(t, out, id);
        break;
    case T_ASK_USER:
        run_ask(t, out, id, in);
        break;
    case T_EXIT_PLAN:
        run_exit_plan(t, out, id, in);
        break;
    case T_ENTER_PLAN:
        run_enter_plan(t, out, id);
        break;
    case T_TASK:
        agent_task(t, out, id, in);
        break;
    case T_SKILL:
        run_skill(t, out, id, in);
        break;
    case T_SLASH:
        run_slash(t, out, id, in);
        break;
    }
}

/* ---- what the screen shows ---- */

static void arg_kv(jw *w, jv in, const char *key, const char *label)
{
    jv v;
    long l;
    char *s;
    if (!json_get(in, key, &v) || json_type(v) != J_STR || !(s = json_strdup(v, &l)))
        return;
    if (w->n)
        jw_rawz(w, ", ");
    if (label) {
        jw_rawz(w, label);
        jw_rawz(w, ": \"");
    }
    jw_raw(w, s, l);
    if (label)
        jw_rawz(w, "\"");
    free(s);
}

char *tools_args(int tool, const char *in, long inn)
{
    jv v;
    jw w;
    char *r;
    jw_init(&w);
    if (json_parse(in ? in : "", inn, &v) == 0 && json_type(v) == J_OBJ) {
        switch (tool) {
        case T_READ:
        case T_WRITE:
        case T_EDIT:
        case T_MULTIEDIT:
            arg_kv(&w, v, "file_path", 0);
            break;
        case T_GLOB:
        case T_GREP:
            arg_kv(&w, v, "pattern", "pattern");
            arg_kv(&w, v, "path", "path");
            arg_kv(&w, v, "glob", "glob");
            break;
        case T_BASH:
            arg_kv(&w, v, "command", 0);
            break;
        case T_BASH_OUTPUT:
            arg_kv(&w, v, "bash_id", 0);
            break;
        case T_KILL_SHELL:
            arg_kv(&w, v, "shell_id", 0);
            break;
        case T_WEB_FETCH:
            arg_kv(&w, v, "url", 0);
            break;
        case T_TASK:
            arg_kv(&w, v, "description", 0);
            break;
        case T_SKILL:
            arg_kv(&w, v, "skill", 0);
            break;
        case T_SLASH:
            arg_kv(&w, v, "command", 0);
            break;
        }
    }
    r = (char *)malloc((size_t)w.n + 1);
    if (r) {
        memcpy(r, w.p ? w.p : "", (size_t)w.n);
        r[w.n] = 0;
    }
    jw_free(&w);
    return r;
}

static long lines_of(const char *p, long n)
{
    long i, c = 0;
    for (i = 0; i < n; i++)
        c += p[i] == '\n';
    return c + (n && p[n - 1] != '\n');
}

/* the result's first line, when it starts with prefix */
static int first_line(const char *p, long n, const char *prefix, char *out, long cap)
{
    long pl = (long)strlen(prefix), e = 0;
    if (n < pl || memcmp(p, prefix, (size_t)pl))
        return 0;
    while (e < n && p[e] != '\n' && e < cap - 1)
        e++;
    memcpy(out, p, (size_t)e);
    out[e] = 0;
    return 1;
}

int tools_summary(int tool, const char *in, long inn, const char *p, long n, char *out, long cap)
{
    char num[16];
    long k, c;
    (void)in;
    (void)inn;
    switch (tool) {
    case T_READ:
        /* the cat -n lines: those that start with the number */
        for (k = 0, c = 0; k < n;) {
            long e = k;
            while (e < n && p[e] != '\n')
                e++;
            c += e - k > 7 && p[k + 6] == '\t';
            k = e + 1;
        }
        cl_copy(out, "Read ", cap);
        cl_ltoa(c, num);
        cl_cat(out, num, cap);
        cl_cat(out, c == 1 ? " line" : " lines", cap);
        return 1;
    case T_GLOB:
        if (first_line(p, n, "No files found", out, cap))
            return 1;
        c = 0;
        for (k = 0; k < n; k++)
            c += p[k] == '\n';
        c += n && p[n - 1] != '\n';
        if (n && strstr(p, "(Results are truncated"))
            c--;
        cl_copy(out, "Found ", cap);
        cl_ltoa(c, num);
        cl_cat(out, num, cap);
        cl_cat(out, c == 1 ? " file" : " files", cap);
        return 1;
    case T_GREP:
        if (first_line(p, n, "Found ", out, cap) || first_line(p, n, "No files found", out, cap) ||
            first_line(p, n, "No matches found", out, cap))
            return 1;
        cl_copy(out, "Found ", cap);
        cl_ltoa(lines_of(p, n), num);
        cl_cat(out, num, cap);
        cl_cat(out, lines_of(p, n) == 1 ? " line" : " lines", cap);
        return 1;
    case T_KILL_SHELL:
    case T_SKILL:
        return first_line(p, n, "", out, cap);
    case T_EXIT_PLAN:
        cl_copy(out, "User approved Claude's plan", cap);
        return 1;
    case T_ENTER_PLAN:
        cl_copy(out, "Entered plan mode", cap);
        return 1;
    case T_SLASH:
        return first_line(p, n, "Launching command", out, cap);
    }
    return 0;
}

int tools_server_line(const char *block, long n, char *out, long cap)
{
    jv b, x, q;
    if (json_parse(block, n, &b) || !json_get(b, "type", &x))
        return -1;
    if (json_streq(x, "server_tool_use")) {
        char query[200];
        query[0] = 0;
        if (json_get(b, "input", &x) && json_get(x, "query", &q))
            json_str(q, query, sizeof(query));
        cl_copy(out, "Web Search(\"", cap);
        cl_cat(out, query, cap);
        cl_cat(out, "\")", cap);
        return 0;
    }
    if (json_streq(x, "web_search_tool_result")) {
        char num[16];
        if (!json_get(b, "content", &x))
            return -1;
        if (json_type(x) != J_ARR) {
            char code[64];
            code[0] = 0;
            if (json_get(x, "error_code", &q))
                json_str(q, code, sizeof(code));
            cl_copy(out, "Search failed: ", cap);
            cl_cat(out, code[0] ? code : "error", cap);
            return 0;
        }
        cl_copy(out, "Did 1 search: ", cap);
        cl_ltoa(json_count(x), num);
        cl_cat(out, num, cap);
        cl_cat(out, json_count(x) == 1 ? " result" : " results", cap);
        return 0;
    }
    return -1;
}
