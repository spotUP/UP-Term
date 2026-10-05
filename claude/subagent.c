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
#include "util.h"

#define AGENT_ROUNDS 60
#define QUERY_MAX_TOKENS 8192L

static const cl_agent builtins[] = {
    { "general-purpose",
      "General-purpose agent for researching complex questions, searching for code, and executing "
      "multi-step tasks. When a search for a keyword or file may take several tries, let this agent do it.",
      0, 0,
      "You are an agent for C:Claude, Claude Code on an Amiga. Given the user's message, use the tools "
      "available to complete the task. Do what has been asked; nothing more, nothing less. When you have "
      "completed the task, respond with a concise report of what was done and any key findings: the "
      "caller relays it to the user, so it needs only the essentials, with full AmigaOS paths." },
    { "Explore",
      "Fast agent for exploring a code base: finding files by patterns, searching code for keywords, "
      "answering questions about the code. Say how thorough it should be: quick, medium or very thorough.",
      "Glob, Grep, Read, Bash", 0,
      "You are a file search specialist for C:Claude on an Amiga. You search and read; you never create, "
      "change or delete files, and run only commands that change nothing. Use Glob for names, Grep for "
      "contents, Read for a known file. Search broadly, then narrow down. Report what you found, with "
      "full AmigaOS paths, concisely: the caller sees only your final message." },
    { "Plan",
      "Software architect agent for designing implementation plans: returns a step-by-step plan, the "
      "critical files and the trade-offs.",
      "Glob, Grep, Read, Bash", 0,
      "You are a software architect for C:Claude on an Amiga. Explore the code (Glob, Grep, Read; "
      "commands that change nothing), understand the requirement, and design an implementation plan: "
      "the steps in order, the files to change, the risks and the trade-offs. You change no file. "
      "Your final message is the plan." }
};
#define NBUILTIN ((int)(sizeof(builtins) / sizeof(builtins[0])))

static int provided(const cl_tools *t, const cl_agent **list)
{
    *list = 0;
    return t->ext && t->ext->agents ? t->ext->agents(t->ext->u, list) : 0;
}

int agent_count(const cl_tools *t)
{
    const cl_agent *l;
    return NBUILTIN + provided(t, &l);
}

const cl_agent *agent_get(const cl_tools *t, int i)
{
    const cl_agent *l;
    int n = provided(t, &l);
    if (i < NBUILTIN)
        return &builtins[i];
    i -= NBUILTIN;
    return i < n ? &l[i] : 0;
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

/* the tools a definition names ("Read, Grep" or "Bash(git:*) Read"): a bit set */
static unsigned long tool_mask(const char *list)
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
        if (id >= 0)
            m |= 1ul << id;
        else if (!strcmp(name, "WebSearch"))
            m |= 1ul << T_COUNT;    /* the server tool's bit */
    }
    return m;
}

static const char env_note[] =
    "\n\nThe machine is an Amiga (AmigaOS 3.x): paths are AmigaOS paths (a volume or assign ends with a "
    "colon; / separates directories). Relative paths start from the start directory: ";
static const char env_note2[] =
    ". The machine is slow and has little memory: prefer targeted searches and reads. Your final message "
    "is all the caller sees: make it a complete, concise report.";

void agent_task(cl_tools *t, jw *out, const char *id, jv in)
{
    char *desc = tl_prop(in, "description", 0), *prompt = tl_prop(in, "prompt", 0);
    char *type = tl_prop(in, "subagent_type", 0), *alias = tl_prop(in, "model", 0);
    const cl_agent *a = 0;
    cl_tools child;
    cl_conv c;
    cl_opts o;
    jw body, content, sys, final;
    int i, n, round, uses = 0, ok = 0;
    long pn = 0;
    unsigned long mask;
    const char *model;
    char num[16];
    jw_init(&body);
    jw_init(&content);
    jw_init(&sys);
    jw_init(&final);
    conv_init(&c);
    memset(&child, 0, sizeof(child));
    if (!desc || !prompt || !type || !alias) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    pn = (long)strlen(prompt);
    n = agent_count(t);
    for (i = 0; i < n; i++) {
        const cl_agent *x = agent_get(t, i);
        if (x && cl_strieq(x->name, type)) {
            a = x;
            break;
        }
    }
    if (t->show)
        t->show(t->u, "Task", desc);
    if (!a) {
        jw m;
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
        goto done;
    }
    if (!t->api.send) {
        tl_error(t, out, id, "subagents are not available here", 0);
        goto done;
    }
    /* the child: the same machine, its own tools list, no Task, no questions */
    child = *t;
    child.depth = 1;
    child.json = 0;
    child.stop = 0;
    mask = tool_mask(a->tools);
    child.allowed = t->allowed & mask & ~((1ul << T_TASK) | (1ul << T_ASK_USER) | (1ul << T_EXIT_PLAN) |
                                          (1ul << T_ENTER_PLAN));
    child.web_search = t->web_search && (mask & (1ul << T_COUNT)) != 0;
    model = agent_model_id(*alias ? alias : 0);
    if (!model)
        model = agent_model_id(a->model);
    if (!model)
        model = t->model ? t->model : "claude-opus-5-5";
    child.model = model;
    jw_rawz(&sys, a->prompt ? a->prompt : "");
    jw_rawz(&sys, env_note);
    jw_rawz(&sys, t->root);
    jw_rawz(&sys, env_note2);
    memset(&o, 0, sizeof(o));
    o.model = model;
    o.effort = "";
    o.max_tokens = 32000L;
    o.system = sys.p;
    if (sys.oom || conv_add_user_text(&c, prompt, pn)) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    for (round = 0; round < AGENT_ROUNDS; round++) {
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
        ntools = stream_tools(&st);
        if (!ntools && !strcmp(st.stop_reason, "pause_turn")) {
            stream_free(&st);
            continue;               /* the server's own tool loop goes on */
        }
        if (!ntools) {
            answer_text(&st, &final);
            stream_free(&st);
            ok = 1;
            break;
        }
        jw_reset(&content);
        jw_raw(&content, "[", 1);
        for (k = 0; k < ntools; k++) {
            sblock *b = stream_tool(&st, k);
            if (k)
                jw_raw(&content, ",", 1);
            tools_run(&child, b->id, b->name, b->input_ok, b->a.p, b->a.n, &content);
            uses++;
        }
        jw_raw(&content, "]", 1);
        stream_free(&st);
        if (content.oom || conv_add(&c, 1, content.p, content.n)) {
            tl_error(t, out, id, "out of memory", 0);
            goto done;
        }
        if (child.stop) {
            t->stop = 1;            /* the parent's round ends too: the user has the word */
            tl_error(t, out, id, "The user stopped one of the agent's tool calls and will say what to do "
                                 "instead; the agent did not finish.", 0);
            goto done;
        }
    }
    if (!ok) {
        tl_error(t, out, id, "The agent did not finish within 60 rounds.", 0);
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
        t->perm = child.perm;   /* the answers given count for the session */
    free(child.json);
    conv_free(&c);
    jw_free(&body);
    jw_free(&content);
    jw_free(&sys);
    jw_free(&final);
    free(desc);
    free(prompt);
    free(type);
    free(alias);
}
