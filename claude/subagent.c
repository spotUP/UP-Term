/* subagent -- the Task tool (ledger A4 WP2): a nested conversation with
 * its own system prompt and its own subset of the tools, run to its end
 * before the parent goes on (one at a time: no threads on this machine);
 * the parent gets the subagent's final text. Its tool calls are shown and
 * asked like any other; the session's permission answers are shared. The
 * agents are the built-in ones below and the provider's (ext.h,
 * .claude/agents). Also the one quiet model call WebFetch uses. */
#include <stdlib.h>
#include <string.h>
#include "tools_int.h"
#include "conv.h"
#include "stream.h"
#include "path.h"
#include "util.h"

#define AGENT_ROUNDS 60
#define QUERY_MAX_TOKENS 8192L

static int runs;                /* subagent runs so far (each its own run_id) */

static const cl_agent builtins[] = {
    { "general-purpose",
      "General-purpose agent for researching complex questions, searching for code, and executing "
      "multi-step tasks. When a search for a keyword or file may take several tries, let this agent do it.",
      0, 0,
      "You are an agent for C:Claude, Claude Code on an Amiga. Given the user's message, use the tools "
      "available to complete the task. Do what has been asked; nothing more, nothing less. When you have "
      "completed the task, respond with a concise report of what was done and any key findings: the "
      "caller relays it to the user, so it needs only the essentials, with full AmigaOS paths.", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { "Explore",
      "Fast agent for exploring a code base: finding files by patterns, searching code for keywords, "
      "answering questions about the code. Say how thorough it should be: quick, medium or very thorough.",
      "Glob, Grep, Read, Bash", 0,
      "You are a file search specialist for C:Claude on an Amiga. You search and read; you never create, "
      "change or delete files, and run only commands that change nothing. Use Glob for names, Grep for "
      "contents, Read for a known file. Search broadly, then narrow down. Report what you found, with "
      "full AmigaOS paths, concisely: the caller sees only your final message.", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { "Plan",
      "Software architect agent for designing implementation plans: returns a step-by-step plan, the "
      "critical files and the trade-offs.",
      "Glob, Grep, Read, Bash", 0,
      "You are a software architect for C:Claude on an Amiga. Explore the code (Glob, Grep, Read; "
      "commands that change nothing), understand the requirement, and design an implementation plan: "
      "the steps in order, the files to change, the risks and the trade-offs. You change no file. "
      "Your final message is the plan.", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { "statusline-setup",
      "Use this agent to configure the user's C:Claude status line setting.",
      "Read, Edit, Write", "sonnet",
      "You configure C:Claude's status line: the statusLine key of the user's settings file, "
      "{\"type\": \"command\", \"command\": \"...\"}. The command is an AmigaDOS command line; it gets "
      "a JSON object on its standard input (the request lists its fields) and what it prints is the "
      "status line. Read the settings file first (it may not exist yet), keep every other key, write "
      "valid JSON. For more than one command line, write a small script beside the settings and point "
      "the command at it. Report the command you set.", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { "claude-code-guide",
      "Use this agent for questions about Claude Code or C:Claude: features, hooks, slash commands, settings, "
      "skills, subagents, the command line.",
      "Glob, Grep, Read, WebFetch, WebSearch", "haiku",
      "You answer questions about Claude Code and C:Claude, its AmigaOS port. Claude Code's documentation is at "
      "https://code.claude.com/docs/en/ (a page's Markdown: add .md, for example "
      "https://code.claude.com/docs/en/hooks.md); fetch the pages that answer the question with WebFetch. "
      "Say where C:Claude differs when you know (paths ENVARC:Claude for ~/.claude, AmigaDOS commands). Answer "
      "concisely with the page you used.", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { "claude",
      "Catch-all for any task that does not fit a more specific agent.", 0, 0,
      "You are an agent for C:Claude, Claude Code on an Amiga. Do the task you are given with the tools you "
      "have, completely, and report what you did and found concisely, with full AmigaOS paths.", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
#define NBUILTIN ((int)(sizeof(builtins) / sizeof(builtins[0])))

static int provided(const cl_tools *t, const cl_agent **list)
{
    *list = 0;
    return t->ext && t->ext->agents ? t->ext->agents(t->ext->u, list) : 0;
}

/* a built-in hidden by a provided agent of its name (Claude Code: the
 * built-ins come last) */
static int hidden(const cl_tools *t, const cl_agent *l, int n, int b)
{
    int i;
    if (t->no_explore_plan && (!strcmp(builtins[b].name, "Explore") || !strcmp(builtins[b].name, "Plan")))
        return 1;                   /* CLAUDE_CODE_DISABLE_EXPLORE_PLAN_AGENTS */
    for (i = 0; i < n; i++)
        if (cl_strieq(l[i].name, builtins[b].name))
            return 1;
    return 0;
}

int agent_count(const cl_tools *t)
{
    const cl_agent *l;
    int n = provided(t, &l), b, k = n;
    for (b = 0; b < NBUILTIN; b++)
        k += !hidden(t, l, n, b);
    return k;
}

/* the built-ins (those not hidden) first, then the provided ones */
const cl_agent *agent_get(const cl_tools *t, int i)
{
    const cl_agent *l;
    int n = provided(t, &l), b;
    for (b = 0; b < NBUILTIN; b++)
        if (!hidden(t, l, n, b) && i-- == 0)
            return &builtins[b];
    return i >= 0 && i < n ? &l[i] : 0;
}

const char *agent_model_id(const char *a)
{
    if (!a || !*a || !strcmp(a, "inherit"))
        return 0;
    if (!strcmp(a, "sonnet"))
        return "claude-sonnet-5-5";
    if (!strcmp(a, "opus"))
        return "claude-opus-5-5";
    if (!strcmp(a, "haiku"))
        return "claude-haiku-4-5";
    if (!strcmp(a, "fable"))
        return "claude-fable-5-1";
    return a;
}

const cl_agent *tools_agent(const cl_tools *t, const char *name)
{
    int i, n = agent_count(t);
    for (i = 0; i < n; i++) {
        const cl_agent *x = agent_get(t, i);
        if (x && cl_strieq(x->name, name))
            return x;
    }
    return 0;
}

/* the text blocks of an answer, appended to w */
static void answer_text(cl_stream *st, jw *w)
{
    int i;
    for (i = 0; i < st->nb; i++)
        if (st->b[i].type == B_TEXT && st->b[i].done && st->b[i].a.n) {
            if (w->n)
                jw_raw(w, "\n\n", 2);
            jw_raw(w, st->b[i].a.p, st->b[i].a.n);
        }
}

int agent_query(cl_tools *t, const char *model, const char *system, const char *prompt, long pn, jw *answer,
                char *err, long cap)
{
    cl_conv c;
    cl_opts o;
    cl_stream st;
    jw body;
    int rc;
    if (!t->api.send) {
        cl_copy(err, "no transport for a model call", cap);
        return -1;
    }
    conv_init(&c);
    jw_init(&body);
    memset(&o, 0, sizeof(o));
    o.model = model;
    o.effort = "";
    o.max_tokens = QUERY_MAX_TOKENS;
    o.system = system;
    o.tools = "";
    if (conv_add_user_text(&c, prompt, pn) || conv_body(&c, &o, &body)) {
        conv_free(&c);
        jw_free(&body);
        cl_copy(err, "out of memory", cap);
        return -1;
    }
    memset(&st, 0, sizeof(st));
    rc = t->api.send(t->api.u, body.p, body.n, &st);
    jw_free(&body);
    conv_free(&c);
    if (rc) {
        cl_copy(err, rc == -2 ? "stopped" : "the request failed", cap);
        stream_free(&st);
        return rc;
    }
    if (!strcmp(st.stop_reason, "refusal")) {
        cl_copy(err, "the model declined", cap);
        stream_free(&st);
        return -1;
    }
    answer_text(&st, answer);
    stream_free(&st);
    return 0;
}

unsigned long tools_mask(const char *list)
{
    unsigned long m = 0;
    const char *p = list;
    if (!list || !*list || !strcmp(list, "*"))
        return (1ul << T_COUNT) - 1;
    while (*p) {
        char name[40];
        int k = 0, id;
        while (*p == ' ' || *p == ',' || *p == '\t')
            p++;
        while (*p && *p != ' ' && *p != ',' && *p != '(' && *p != '\t' && k < (int)sizeof(name) - 1)
            name[k++] = *p++;
        name[k] = 0;
        if (*p == '(') {
            while (*p && *p != ')')
                p++;
            if (*p)
                p++;
        }
        id = tools_id(name);
        if (id < 0 && !strcmp(name, "Agent"))
            id = T_TASK;            /* Claude Code's newer name of Task */
        if (id >= 0)
            m |= 1ul << id;
        if (id == T_TODO_WRITE || id == T_TASK_CREATE || id == T_TASK_GET || id == T_TASK_LIST ||
            id == T_TASK_UPDATE)
            m |= (1ul << T_TODO_WRITE) | (1ul << T_TASK_CREATE) | (1ul << T_TASK_GET) | (1ul << T_TASK_LIST) |
                 (1ul << T_TASK_UPDATE);    /* the task tools come as a set */
        if (id == T_KILL_SHELL || id == T_TASK_STOP)
            m |= (1ul << T_KILL_SHELL) | (1ul << T_TASK_STOP);
    }
    return m;
}

static const char env_note[] =
    "\n\nThe machine is an Amiga (AmigaOS 3.x): paths are AmigaOS paths (a volume or assign ends with a "
    "colon; / separates directories). Relative paths start from the start directory: ";
static const char env_note2[] =
    ". The machine is slow and has little memory: prefer targeted searches and reads. Your final message "
    "is all the caller sees: make it a complete, concise report.";

/* the memory files' text goes to every agent but the read-only search
 * ones (Claude Code: Explore and Plan skip CLAUDE.md) */
static int gets_memory(const cl_agent *a)
{
    return strcmp(a->name, "Explore") && strcmp(a->name, "Plan");
}

/* a subagent's message to the observer (stream-json) */
static void forward(cl_tools *t, const char *id, int user, const char *json, long n, cl_stream *st)
{
    if (t->agent_msg)
        t->agent_msg(t->u, id, user, json, n, st);
}

/* the user message that starts a subagent: its prompt as a text block */
static void forward_prompt(cl_tools *t, const char *id, const char *prompt, long pn)
{
    jw w;
    if (!t->agent_msg)
        return;
    jw_init(&w);
    jw_rawz(&w, "[{\"type\":\"text\",\"text\":");
    jw_str(&w, prompt, pn);
    jw_rawz(&w, "}]");
    if (!w.oom)
        forward(t, id, 1, w.p, w.n, 0);
    jw_free(&w);
}

/* The agent's skills (frontmatter "skills"): each one's text appended to
 * its system prompt, as Claude Code preloads them. */
static void preload(cl_tools *t, const cl_agent *a, jw *sys)
{
    const char *p = a->skills;
    while (p && *p) {
        char name[64], err[160], agent[64];
        int k = 0, fork = 0;
        jw body;
        while (*p == ' ' || *p == ',')
            p++;
        while (*p && *p != ',' && *p != ' ' && k < (int)sizeof(name) - 1)
            name[k++] = *p++;
        name[k] = 0;
        if (!k || !t->ext || !t->ext->skill)
            continue;
        jw_init(&body);
        if (t->ext->skill(t->ext->u, name, "", 0, &body, &fork, agent, sizeof(agent), err, sizeof(err)) == 0 &&
            body.n) {
            jw_rawz(sys, "\n\n# Skill: ");
            jw_rawz(sys, name);
            jw_rawz(sys, "\n\n");
            jw_raw(sys, body.p, body.n);
        }
        jw_free(&body);
    }
}

/* An agent's persistent memory (frontmatter memory: user, project, local;
 * Claude Code's): its directory -- ENVARC:Claude/agent-memory/<name>,
 * <root>/.claude/agent-memory/<name>, <root>/.claude/agent-memory-local/
 * <name> -- named in its system prompt with MEMORY.md's first 200 lines
 * (25 KB at most); Read, Write and Edit are its tools there, free. */
static void agent_memory(cl_tools *t, cl_tools *child, const cl_agent *a, jw *sys)
{
    char dir[300], f[340];
    char *b = 0;
    long n = 0, k = 0, lines = 0;
    int ok;
    if (!strcmp(a->memory, "user"))
        ok = t->home && path_join(t->home, "agent-memory", dir, sizeof(dir)) == 0;
    else if (!strcmp(a->memory, "project"))
        ok = path_join(t->root, ".claude/agent-memory", dir, sizeof(dir)) == 0;
    else if (!strcmp(a->memory, "local"))
        ok = path_join(t->root, ".claude/agent-memory-local", dir, sizeof(dir)) == 0;
    else
        return;
    if (!ok || path_join(dir, a->name, child->mem_dir, sizeof(child->mem_dir)))
        return;
    child->allowed |= (1ul << T_READ) | (1ul << T_WRITE) | (1ul << T_EDIT);
    jw_rawz(sys, "\n\n# Your memory\n\nYou have a persistent memory directory: ");
    jw_rawz(sys, child->mem_dir);
    jw_rawz(sys, " (it may not exist yet; Write makes it). Before you start, consider what you noted there in "
                 "earlier sessions; as you work, save what will help next time -- patterns, conventions, decisions, "
                 "what was learned the hard way -- one topic a file, MEMORY.md the index. Keep MEMORY.md under 200 "
                 "lines: curate it when it grows past that.");
    if (path_join(child->mem_dir, "MEMORY.md", f, sizeof(f)) == 0 && t->sys->kind(t->sys->u, f) == 1 &&
        t->sys->read(t->sys->u, f, 256L * 1024, &b, &n) == 0) {
        while (k < n && k < 25L * 1024 && lines < 200) {
            if (b[k] == '\n')
                lines++;
            k++;
        }
        jw_rawz(sys, "\n\nContents of MEMORY.md:\n\n");
        jw_raw(sys, b, k);
        if (k < n)
            jw_rawz(sys, "\n(MEMORY.md is longer: it was cut here; curate it)");
        free(b);
    }
}

/* One subagent run to its end: agent a on the prompt, its final text the
 * tool_result of the call id. alias: the call's model ("" the agent's). */
static void agent_run(cl_tools *t, jw *out, const char *id, const cl_agent *a, const char *prompt, long pn,
                      const char *alias)
{
    cl_tools child;
    cl_conv c;
    cl_opts o;
    jw body, content, sys, final, extra;
    int round, uses = 0, ok = 0, stops = 0, rounds;
    unsigned long mask;
    const char *model;
    char num[16];
    jw_init(&body);
    jw_init(&content);
    jw_init(&sys);
    jw_init(&final);
    jw_init(&extra);
    conv_init(&c);
    memset(&child, 0, sizeof(child));
    if (!t->api.send) {
        tl_error(t, out, id, "subagents are not available here", 0);
        goto done;
    }
    /* the child: the same machine, its own tools list, no questions; Task
     * while the depth limit allows (Claude Code: nested subagents, three
     * layers by default) */
    child = *t;
    child.depth = t->depth + 1;
    child.run_id = ++runs;
    child.json = 0;
    child.stop = 0;
    child.sub_append = 0;           /* the parent owns it */
    child.parent_id = id;
    mask = tools_mask(a->tools);
    if (a->deny_tools && a->deny_tools[0])
        mask &= ~tools_mask(a->deny_tools);     /* disallowedTools */
    child.allowed = t->allowed & mask & ~((1ul << T_ASK_USER) | (1ul << T_EXIT_PLAN) | (1ul << T_ENTER_PLAN));
    child.web_search = t->web_search && (mask & (1ul << T_WEB_SEARCH)) != 0;
    /* permissionMode: the edits and plan modes are the tools' own; nobody
     * asked / yes to all are the REPL's ask policies */
    if (a->perm_mode) {
        if (!strcmp(a->perm_mode, "acceptEdits"))
            child.perm.mode = PERM_ACCEPT;
        else if (!strcmp(a->perm_mode, "plan"))
            child.perm.mode = PERM_PLAN;
        else if (!strcmp(a->perm_mode, "default"))
            child.perm.mode = PERM_DEFAULT;
        else if (!strcmp(a->perm_mode, "dontAsk"))
            child.ask_policy = 2;
        else if (!strcmp(a->perm_mode, "bypassPermissions"))
            child.ask_policy = 3;
    }
    /* Claude Code's order: the call's model, the agent's, CLAUDE_CODE_SUBAGENT_MODEL,
     * the conversation's; CLAUDE_CODE_SUBAGENT_MODEL_FORCE puts the variable first */
    model = t->sub_force && t->sub_model[0] ? t->sub_model : agent_model_id(alias && *alias ? alias : 0);
    if (!model)
        model = a->model && !strcmp(a->model, "inherit") ? t->model : agent_model_id(a->model);
    if (!model && t->sub_model[0])
        model = t->sub_model;
    if (!model)
        model = t->model ? t->model : "claude-opus-5-5";
    child.model = model;
    rounds = a->max_turns > 0 ? a->max_turns : AGENT_ROUNDS;
    jw_rawz(&sys, a->prompt ? a->prompt : "");
    jw_rawz(&sys, env_note);
    jw_rawz(&sys, t->root);
    jw_rawz(&sys, env_note2);
    preload(t, a, &sys);
    child.mem_dir[0] = 0;
    if (a->memory && t->auto_memory)
        agent_memory(t, &child, a, &sys);
    if (t->memory && *t->memory && gets_memory(a)) {
        jw_rawz(&sys, "\n\nCodebase and user instructions (CLAUDE.md):\n\n");
        jw_rawz(&sys, t->memory);
    }
    if (t->sub_append && *t->sub_append) {
        jw_rawz(&sys, "\n\n");
        jw_rawz(&sys, t->sub_append);   /* --append-subagent-system-prompt */
    }
    if (t->agent_hooks && a->hooks)
        t->agent_hooks(t->u, a, child.run_id, 1);  /* its frontmatter's hooks, while it runs */
    if (t->agent_start) {
        /* SubagentStart hooks: their additionalContext is the agent's */
        jw ctx;
        jw_init(&ctx);
        t->agent_start(t->u, a->name, id, &ctx);
        if (ctx.n) {
            jw_rawz(&sys, "\n\n");
            jw_raw(&sys, ctx.p, ctx.n);
        }
        jw_free(&ctx);
    }
    memset(&o, 0, sizeof(o));
    o.model = model;
    o.effort = a->effort ? a->effort : "";
    o.max_tokens = 32000L;
    o.system = sys.p;
    if (sys.oom || conv_add_user_text(&c, prompt, pn)) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    forward_prompt(t, id, prompt, pn);
    for (round = 0; round < rounds; round++) {
        cl_stream st;
        int rc, ntools, k;
        o.tools = tools_json(&child, model);
        jw_reset(&body);
        if (conv_body(&c, &o, &body)) {
            tl_error(t, out, id, "out of memory", 0);
            goto done;
        }
        memset(&st, 0, sizeof(st));
        rc = t->api.send(t->api.u, body.p, body.n, &st);
        if (rc) {
            stream_free(&st);
            tl_error(t, out, id, rc == -2 ? "The user stopped the agent before it finished."
                                          : "The agent's request to the API failed; it did not finish.", 0);
            goto done;
        }
        if (!strcmp(st.stop_reason, "refusal") || (!strcmp(st.stop_reason, "max_tokens") && stream_tools(&st))) {
            stream_free(&st);
            tl_error(t, out, id, "The agent's answer was declined or cut off; it did not finish.", 0);
            goto done;
        }
        jw_reset(&content);
        if (stream_content(&st, &content) || conv_add(&c, 0, content.p, content.n)) {
            stream_free(&st);
            tl_error(t, out, id, "out of memory", 0);
            goto done;
        }
        forward(t, id, 0, content.p, content.n, &st);
        ntools = stream_tools(&st);
        if (!ntools && !strcmp(st.stop_reason, "pause_turn")) {
            stream_free(&st);
            continue;               /* the server's own tool loop goes on */
        }
        if (!ntools) {
            jw why, last;
            int again;
            jw_init(&why);
            jw_init(&last);
            answer_text(&st, &last);
            /* a SubagentStop hook may send it on (Claude Code's exit 2) */
            again = stops < 3 && t->agent_stop &&
                    t->agent_stop(t->u, a->name, id, stops > 0, last.p ? last.p : "", last.n, &why);
            jw_free(&last);
            if (again && !why.oom && conv_add_user_text(&c, why.p, why.n) == 0) {
                stops++;
                jw_free(&why);
                stream_free(&st);
                continue;
            }
            jw_free(&why);
            answer_text(&st, &final);
            stream_free(&st);
            ok = 1;
            break;
        }
        jw_reset(&content);
        jw_reset(&extra);
        jw_raw(&content, "[", 1);
        for (k = 0; k < ntools; k++) {
            sblock *b = stream_tool(&st, k);
            if (k)
                jw_raw(&content, ",", 1);
            /* the policy (rules, hooks, checkpoints) as for the conversation's calls */
            if (t->call)
                t->call(t->u, &child, b->id, b->name, b->input_ok, b->a.p, b->a.n, &content, &extra);
            else
                tools_run(&child, b->id, b->name, b->input_ok, b->a.p, b->a.n, &content);
            uses++;
        }
        if (extra.n) {
            jw_rawz(&content, ",{\"type\":\"text\",\"text\":");
            jw_str(&content, extra.p, extra.n);
            jw_raw(&content, "}", 1);
        }
        jw_raw(&content, "]", 1);
        stream_free(&st);
        if (content.oom || conv_add(&c, 1, content.p, content.n)) {
            tl_error(t, out, id, "out of memory", 0);
            goto done;
        }
        forward(t, id, 1, content.p, content.n, 0);
        if (child.stop) {
            t->stop = 1;            /* the parent's round ends too: the user has the word */
            tl_error(t, out, id, "The user stopped one of the agent's tool calls and will say what to do "
                                 "instead; the agent did not finish.", 0);
            goto done;
        }
    }
    if (!ok) {
        cl_ltoa(rounds, num);
        jw_reset(&final);
        jw_rawz(&final, "The agent did not finish within ");
        jw_rawz(&final, num);
        jw_rawz(&final, " rounds.");
        tl_error(t, out, id, final.p ? final.p : "The agent did not finish.", 0);
        goto done;
    }
    if (!final.n)
        jw_rawz(&final, "(The agent finished without a report.)");
    jw_rawz(&final, "\n\n(Agent ");
    jw_rawz(&final, a->name);
    jw_rawz(&final, ": ");
    cl_ltoa(uses, num);
    jw_rawz(&final, num);
    jw_rawz(&final, uses == 1 ? " tool use.)" : " tool uses.)");
    tl_result(t, out, id, final.p, final.n, 0);
done:
    if (child.depth)
        shells_end_owner(t, child.run_id);     /* its background commands stop with it (Claude Code) */
    if (child.depth && t->agent_hooks && a->hooks)
        t->agent_hooks(t->u, a, child.run_id, 0);
    if (child.depth && !t->fetch_cache)
        t->fetch_cache = child.fetch_cache;    /* a cache the agent's WebFetch made is the session's */
    if (child.depth) {
        int mode = t->perm.mode;
        t->perm = child.perm;   /* the answers given count for the session ... */
        t->perm.mode = mode;    /* ... not the agent's own permission mode */
    }
    free(child.json);
    conv_free(&c);
    jw_free(&body);
    jw_free(&content);
    jw_free(&sys);
    jw_free(&final);
    jw_free(&extra);
}

/* the "not found" error, with the agents there are */
static void no_agent(cl_tools *t, jw *out, const char *id, const char *type)
{
    jw m;
    int i, n = agent_count(t);
    jw_init(&m);
    jw_rawz(&m, "Agent type '");
    jw_rawz(&m, type);
    jw_rawz(&m, "' not found. Available agents: ");
    for (i = 0; i < n; i++) {
        if (i)
            jw_rawz(&m, ", ");
        jw_rawz(&m, agent_get(t, i)->name);
    }
    tl_error(t, out, id, m.p ? m.p : "out of memory", 0);
    jw_free(&m);
}

void agent_task(cl_tools *t, jw *out, const char *id, jv in)
{
    char *desc = tl_prop(in, "description", 0), *prompt = tl_prop(in, "prompt", 0);
    char *type = tl_prop(in, "subagent_type", 0), *alias = tl_prop(in, "model", 0);
    const cl_agent *a;
    if (!desc || !prompt || !type || !alias)
        tl_error(t, out, id, "out of memory", 0);
    else {
        a = tools_agent(t, type);
        if (t->show)
            t->show(t->u, "Task", desc);
        if (!a)
            no_agent(t, out, id, type);
        else
            agent_run(t, out, id, a, prompt, (long)strlen(prompt), alias);
    }
    free(desc);
    free(prompt);
    free(type);
    free(alias);
}

int agent_answer(cl_tools *t, const cl_agent *a, const char *prompt, long pn, jw *answer)
{
    jw out;
    jv v, x;
    int rc = -1;
    cl_tools q = *t;
    jw_init(&out);
    q.result = 0;                   /* nothing of it on the screen as a tool result */
    q.show = 0;
    agent_run(&q, &out, "hook-agent", a, prompt, pn, "");
    if (json_parse(out.p ? out.p : "", out.n, &v) == 0 && !(json_get(v, "is_error", &x) && json_type(x) == J_TRUE) &&
        json_get(v, "content", &x) && json_type(x) == J_STR) {
        long l;
        char *s = json_strdup(x, &l);
        if (s) {
            jw_raw(answer, s, l);
            rc = 0;
        }
        free(s);
    }
    t->perm = q.perm;
    jw_free(&out);
    return rc;
}

void agent_fork(cl_tools *t, jw *out, const char *id, const char *type, const char *prompt, long pn)
{
    const cl_agent *a = tools_agent(t, type && *type ? type : "general-purpose");
    if (!a)
        no_agent(t, out, id, type);
    else
        agent_run(t, out, id, a, prompt, pn, "");
}
