/* The Claude client's tools (claude/tools.c and the modules behind it,
 * ledger A2 and A4 WP2): AmigaOS path names, the permission rules, the
 * schema checks, and every tool against a temporary tree on the host
 * (claude/sys_posix.c): Read Write Edit MultiEdit Glob Grep Bash
 * BashOutput KillShell TodoWrite AskUserQuestion EnterPlanMode
 * ExitPlanMode Skill SlashCommand, and Task and WebFetch over stubs of
 * the API (recorded answers in tests/claude/) and of the web. */
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include "harness.h"
#include "claude_load.h"
#include "../claude/path.h"
#include "../claude/tools.h"
#include "../claude/stream.h"
#include "../claude/sse.h"
#include "../claude/util.h"
#include "../claude/sys_posix.h"

static void test_path(void)
{
    char o[256];
    CHECK_INT(path_join("Work:Projects", "foo", o, sizeof(o)), 0);
    CHECK_STR(o, "Work:Projects/foo");
    CHECK_INT(path_join("RAM:", "x", o, sizeof(o)), 0);
    CHECK_STR(o, "RAM:x");
    CHECK_INT(path_join("Work:a/b", "/c", o, sizeof(o)), 0);
    CHECK_STR(o, "Work:a/c");
    CHECK_INT(path_join("Work:a/b", "//c", o, sizeof(o)), 0);
    CHECK_STR(o, "Work:c");
    CHECK_INT(path_join("Work:a/b", "x//y", o, sizeof(o)), 0);
    CHECK_STR(o, "Work:a/b/y");
    CHECK_INT(path_join("Work:a", "SYS:S/Startup-Sequence", o, sizeof(o)), 0);
    CHECK_STR(o, "SYS:S/Startup-Sequence");
    CHECK_INT(path_join("Work:a", "S:", o, sizeof(o)), 0);
    CHECK_STR(o, "S:");
    CHECK_INT(path_join("Work:a", "../b", o, sizeof(o)), 0);
    CHECK_STR(o, "Work:b");
    CHECK_INT(path_join("Work:a", "./b/.", o, sizeof(o)), 0);
    CHECK_STR(o, "Work:a/b");
    CHECK_INT(path_join("Work:a", "b/", o, sizeof(o)), 0);
    CHECK_STR(o, "Work:a/b");
    CHECK_INT(path_join("Work:a", "", o, sizeof(o)), 0);
    CHECK_STR(o, "Work:a");
    /* above a volume's root */
    CHECK_INT(path_join("Work:a", "//c", o, sizeof(o)), -1);
    CHECK_INT(path_join("RAM:", "/x", o, sizeof(o)), -1);
    CHECK_INT(path_join("Work:", "SYS:/x", o, sizeof(o)), -1);
    CHECK_INT(path_join("Work:a", "b", o, 8), -1);
    CHECK_INT(path_parent("Work:a/b", o, sizeof(o)), 0);
    CHECK_STR(o, "Work:a");
    CHECK_INT(path_parent("Work:a", o, sizeof(o)), 0);
    CHECK_STR(o, "Work:");
    CHECK_INT(path_parent("Work:", o, sizeof(o)), -1);

    CHECK(path_inside("Work:a", "WORK:A/b"));
    CHECK(path_inside("Work:a", "Work:a"));
    CHECK(!path_inside("Work:a", "Work:ab"));
    CHECK(!path_inside("Work:a", "Work:"));
    CHECK(path_inside("RAM:", "RAM:x/y"));
    CHECK(!path_inside("RAM:", "SYS:x"));
}

static void test_perm(void)
{
    cl_perm p;
    int i;
    memset(&p, 0, sizeof(p));
    /* what asks: the writing tools, Bash, WebFetch, Skill, SlashCommand; the
     * read-only tools inside the working directories never (Claude Code) */
    for (i = 0; i < T_COUNT; i++) {
        int asks = i == T_WRITE || i == T_EDIT || i == T_MULTIEDIT || i == T_BASH || i == T_WEB_FETCH ||
                   i == T_SKILL || i == T_SLASH || i == T_WEB_SEARCH || i == T_MONITOR;
        CHECK_INT(perm_must_ask(&p, i, 0), asks);
    }
    CHECK(!perm_must_ask(&p, T_READ, 0));
    CHECK(!perm_must_ask(&p, T_GLOB, 0));
    CHECK(!perm_must_ask(&p, T_GREP, 0));
    /* outside them: always, whatever was granted */
    CHECK(perm_must_ask(&p, T_READ, 1));
    CHECK(perm_must_ask(&p, T_GLOB, 1));
    perm_grant(&p, T_GREP);
    CHECK(perm_must_ask(&p, T_GREP, 1));
    CHECK(perm_must_ask(&p, T_WRITE, 0));
    CHECK(perm_must_ask(&p, T_EDIT, 0));
    CHECK(perm_must_ask(&p, T_BASH, 0));
    CHECK(perm_must_ask(&p, T_WEB_FETCH, 0));
    /* outside the start directory: always */
    CHECK(perm_must_ask(&p, T_READ, 1));
    /* a writing tool allowed for the session: that tool only */
    perm_grant(&p, T_WRITE);
    CHECK(!perm_must_ask(&p, T_WRITE, 0));
    CHECK(perm_must_ask(&p, T_EDIT, 0));
    CHECK(perm_must_ask(&p, T_WRITE, 1));
    /* accept edits: Write, Edit, MultiEdit inside the start directory run */
    memset(&p, 0, sizeof(p));
    p.mode = PERM_ACCEPT;
    CHECK(!perm_must_ask(&p, T_WRITE, 0));
    CHECK(!perm_must_ask(&p, T_EDIT, 0));
    CHECK(!perm_must_ask(&p, T_MULTIEDIT, 0));
    CHECK(perm_must_ask(&p, T_EDIT, 1));
    CHECK(perm_must_ask(&p, T_BASH, 0));
    CHECK(!perm_must_ask(&p, T_READ, 0));
    CHECK(!perm_refused(&p, T_BASH));
    /* plan: what changes something is refused */
    p.mode = PERM_PLAN;
    CHECK(perm_refused(&p, T_WRITE));
    CHECK(perm_refused(&p, T_EDIT));
    CHECK(perm_refused(&p, T_MULTIEDIT));
    CHECK(perm_refused(&p, T_BASH));
    CHECK(perm_refused(&p, T_KILL_SHELL));
    CHECK(!perm_refused(&p, T_GREP));
    CHECK(!perm_refused(&p, T_WEB_FETCH));
    CHECK(!perm_refused(&p, T_TASK));
    CHECK(!perm_refused(&p, T_TODO_WRITE));
    CHECK(!perm_refused(&p, T_EXIT_PLAN));
}

static int bad(int tool, const char *in)
{
    jv v;
    char err[300];
    if (json_parse(in, (long)strlen(in), &v))
        return -2;
    return tools_validate(tool, v, err, sizeof(err));
}

static void test_validate(void)
{
    jv v, e, x;
    jit it;
    cl_tools t;
    char err[300];
    int n = 0, server = 0;
    memset(&t, 0, sizeof(t));
    CHECK_INT(tools_init(&t), 0);
    /* every tool declared: no transport here, so no Task, WebFetch or
     * WebSearch; no provider, so no Skill and no SlashCommand; no machine,
     * so no TaskStop, Monitor, Cron; the older BashOutput and KillShell
     * never; TodoWrite (the default task tool) */
    CHECK_INT(json_parse(tools_json(&t, "claude-opus-5-5"), (long)strlen(tools_json(&t, "claude-opus-5-5")), &v), 0);
    json_iter(v, &it);
    while (json_next(&it, 0, &e)) {
        n++;
        if (json_get(e, "type", &x))
            server++;
        else
            CHECK(json_get(e, "strict", &x) && json_type(x) == J_TRUE && json_get(e, "input_schema", &x));
    }
    CHECK_INT(n, 11);
    CHECK_INT(server, 0);
    CHECK(strstr(tools_json(&t, "claude-opus-5-5"), "\"TodoWrite\"") != 0);
    CHECK(strstr(tools_json(&t, "claude-opus-5-5"), "BashOutput") == 0);
    /* the task tools instead of TodoWrite; none */
    t.todo_mode = TODO_TASKS;
    CHECK(strstr(tools_json(&t, "claude-opus-5-5"), "\"TaskCreate\"") != 0 &&
          strstr(tools_json(&t, "claude-opus-5-5"), "\"TodoWrite\"") == 0);
    t.todo_mode = TODO_NONE;
    CHECK(strstr(tools_json(&t, "claude-opus-5-5"), "\"TaskCreate\"") == 0 &&
          strstr(tools_json(&t, "claude-opus-5-5"), "\"TodoWrite\"") == 0);
    /* Claude Code's task tool availability by model */
    CHECK_INT(tools_todo_default("claude-opus-4-7"), TODO_TASKS);
    CHECK_INT(tools_todo_default("claude-sonnet-4-6"), TODO_TASKS);
    CHECK_INT(tools_todo_default("claude-haiku-4-5"), TODO_TASKS);
    CHECK_INT(tools_todo_default("claude-opus-4-8"), TODO_NONE);
    CHECK_INT(tools_todo_default("claude-opus-5-5"), TODO_NONE);
    CHECK_INT(tools_todo_default("claude-fable-5-1"), TODO_NONE);
    /* the advisor server tool when /advisor set one */
    cl_copy(t.advisor, "claude-opus-5-5", sizeof(t.advisor));
    CHECK(strstr(tools_json(&t, "claude-sonnet-5-5"),
                 "{\"type\":\"advisor_20260301\",\"name\":\"advisor\",\"model\":\"claude-opus-5-5\"}") != 0);
    t.advisor[0] = 0;
    CHECK(strstr(tools_json(&t, "claude-sonnet-5-5"), "advisor") == 0);
    tools_free(&t);
    {
        /* WebSearch's request: the server tool by model, with the domain lists */
        jw w;
        jw_init(&w);
        tools_search_tool(&w, "claude-haiku-4-5", "[\"aminet.net\"]", 0);
        CHECK_STR(w.p, "{\"type\":\"web_search_20250305\",\"name\":\"web_search\",\"max_uses\":8,\"allowed_domains\":"
                       "[\"aminet.net\"]}");
        jw_reset(&w);
        tools_search_tool(&w, "claude-opus-5-5", 0, "[\"x.com\"]");
        CHECK_STR(w.p, "{\"type\":\"web_search_20260209\",\"name\":\"web_search\",\"max_uses\":8,\"blocked_domains\":"
                       "[\"x.com\"]}");
        jw_free(&w);
    }

    CHECK_INT(bad(T_READ, "{\"file_path\":\"S:x\"}"), 0);
    CHECK_INT(bad(T_READ, "{\"file_path\":\"S:x\",\"offset\":10,\"limit\":5}"), 0);
    CHECK_INT(bad(T_READ, "{}"), -1);
    CHECK_INT(bad(T_READ, "[\"S:x\"]"), -1);
    CHECK_INT(bad(T_READ, "{\"file_path\":7}"), -1);
    CHECK_INT(bad(T_READ, "{\"file_path\":\"x\",\"extra\":1}"), -1);
    CHECK_INT(bad(T_READ, "{\"file_path\":\"x\",\"offset\":1.5}"), -1);
    CHECK_INT(bad(T_READ, "{\"file_path\":\"x\",\"limit\":0}"), -1);
    CHECK_INT(bad(T_GREP, "{\"pattern\":\"x\"}"), 0);
    CHECK_INT(bad(T_GREP, "{\"pattern\":\"x\",\"-i\":\"yes\"}"), -1);
    CHECK_INT(bad(T_GREP, "{\"pattern\":\"x\",\"output_mode\":\"lines\"}"), -1);
    CHECK_INT(bad(T_GREP, "{\"pattern\":\"x\",\"-C\":-1}"), -1);
    CHECK_INT(bad(T_GREP, "{\"pattern\":\"x\",\"-i\":true,\"-n\":true,\"-A\":2,\"path\":\"S\",\"glob\":\"*.c\","
                          "\"output_mode\":\"content\",\"head_limit\":5,\"offset\":1,\"multiline\":false,\"type\":\"c\"}"), 0);
    CHECK_INT(bad(T_EDIT, "{\"file_path\":\"a\",\"old_string\":\"b\"}"), -1);
    CHECK_INT(bad(T_EDIT, "{\"file_path\":\"a\",\"old_string\":\"b\",\"new_string\":\"c\",\"replace_all\":true}"), 0);
    CHECK_INT(bad(T_MULTIEDIT, "{\"file_path\":\"a\",\"edits\":[]}"), -1);
    CHECK_INT(bad(T_MULTIEDIT, "{\"file_path\":\"a\",\"edits\":[{\"old_string\":\"b\"}]}"), -1);
    CHECK_INT(bad(T_MULTIEDIT, "{\"file_path\":\"a\",\"edits\":[{\"old_string\":\"b\",\"new_string\":\"c\"}]}"), 0);
    CHECK_INT(bad(T_BASH, "{\"command\":\"Version\"}"), 0);
    CHECK_INT(bad(T_BASH, "{\"command\":\"Version\",\"timeout\":700000}"), -1);
    CHECK_INT(bad(T_BASH, "{\"command\":\"Version\",\"run_in_background\":true,\"description\":\"v\"}"), 0);
    CHECK_INT(bad(T_TODO_WRITE, "{\"todos\":[{\"content\":\"a\",\"status\":\"pending\",\"activeForm\":\"A\"}]}"), 0);
    CHECK_INT(bad(T_TODO_WRITE, "{\"todos\":[]}"), 0);
    CHECK_INT(bad(T_TODO_WRITE, "{\"todos\":[{\"content\":\"a\",\"status\":\"maybe\",\"activeForm\":\"A\"}]}"), -1);
    CHECK_INT(bad(T_TODO_WRITE, "{\"todos\":[{\"content\":\"a\",\"status\":\"pending\"}]}"), -1);
    CHECK_INT(bad(T_TODO_WRITE, "{\"todos\":[\"a\"]}"), -1);
    CHECK_INT(bad(T_TODO_WRITE, "{\"todos\":\"a\"}"), -1);
    CHECK_INT(bad(T_ASK_USER, "{\"questions\":[{\"question\":\"q\",\"header\":\"h\",\"multiSelect\":false,"
                              "\"options\":[{\"label\":\"a\",\"description\":\"\"},{\"label\":\"b\",\"description\":\"\"}]}]}"), 0);
    CHECK_INT(bad(T_ASK_USER, "{\"questions\":[{\"question\":\"q\",\"header\":\"h\",\"multiSelect\":false,"
                              "\"options\":[{\"label\":\"a\",\"description\":\"\"}]}]}"), -1);
    CHECK_INT(bad(T_ASK_USER, "{\"questions\":[]}"), -1);
    CHECK_INT(bad(T_ENTER_PLAN, "{}"), 0);
    CHECK_INT(bad(T_ENTER_PLAN, "{\"x\":1}"), -1);
    CHECK_INT(bad(T_TASK, "{\"description\":\"d\",\"prompt\":\"p\",\"subagent_type\":\"Explore\",\"model\":\"haiku\"}"), 0);
    CHECK_INT(bad(T_TASK, "{\"description\":\"d\",\"prompt\":\"p\",\"subagent_type\":\"Explore\",\"model\":\"gpt\"}"), -1);
    CHECK_INT(bad(T_WEB_FETCH, "{\"url\":\"http://x/\"}"), -1);
    /* the messages say which parameter, as Claude Code's do */
    CHECK_INT(json_parse("{\"command\":1}", 13, &v), 0);
    CHECK_INT(tools_validate(T_BASH, v, err, sizeof(err)), -1);
    CHECK_STR(err, "The parameter `command` must be a string, not a number");
    CHECK_INT(json_parse("{}", 2, &v), 0);
    CHECK_INT(tools_validate(T_READ, v, err, sizeof(err)), -1);
    CHECK_STR(err, "The required parameter `file_path` is missing");
    {
        static const char me[] = "{\"file_path\":\"a\",\"edits\":[{\"old_string\":\"b\",\"new_string\":\"c\",\"x\":1}]}";
        CHECK_INT(json_parse(me, (long)strlen(me), &v), 0);
    }
    CHECK_INT(tools_validate(T_MULTIEDIT, v, err, sizeof(err)), -1);
    CHECK_STR(err, "An unexpected parameter `edits[0].x` was provided");
    CHECK_INT(tools_id("TodoWrite"), T_TODO_WRITE);
    CHECK_INT(tools_id("Read"), T_READ);
    CHECK_INT(tools_id("read_file"), -1);
    CHECK_STR(tools_name(T_ASK_USER), "AskUserQuestion");
}

/* ---- the tools on a temporary tree ---- */

typedef struct asker {
    int asked, shown;
    int answer;
    char note[64];              /* the comment given with the answer (Tab on Yes / No) */
    int outside;
    char last[300], last_tool[40];
    /* the preview: the file before and after (an edit's, a write's) */
    int previews;
    char before[200], after[200];
    /* the result hook */
    int results, last_error;
    /* the questions */
    int chooses, choose_ret;
    unsigned choose_picked;
    char choose_other[100], choose_q[200], choose_opts[400];
    int plans;
    char plan[200];
} asker;

static void on_preview(void *u, int tool, const char *path, const char *b, long bn, const char *a, long an)
{
    asker *k = (asker *)u;
    (void)tool;
    (void)path;
    k->previews++;
    k->before[0] = k->after[0] = 0;
    if (b && bn < 199) {
        memcpy(k->before, b, (size_t)bn);
        k->before[bn] = 0;
    }
    if (an < 199) {
        memcpy(k->after, a, (size_t)an);
        k->after[an] = 0;
    }
}

static void on_result(void *u, int tool, const char *in, long inn, int is_error, const char *text, long n)
{
    asker *k = (asker *)u;
    (void)tool;
    (void)in;
    (void)inn;
    (void)text;
    (void)n;
    k->results++;
    k->last_error = is_error;
}

static void on_show(void *u, const char *tool, const char *what)
{
    asker *a = (asker *)u;
    a->shown++;
    cl_copy(a->last, what, sizeof(a->last));
    cl_copy(a->last_tool, tool, sizeof(a->last_tool));
}

static int on_ask(void *u, const char *tool, const char *what, int outside, char *note, long cap)
{
    asker *a = (asker *)u;
    (void)tool;
    (void)what;
    a->asked++;
    a->outside = outside;
    cl_copy(note, a->note, cap);
    return a->answer;
}

static int on_choose(void *u, const char *header, const char *question, const char *const *labels,
                     const char *const *descs, int n, int flags, unsigned *picked, char *other, long cap)
{
    asker *a = (asker *)u;
    int i;
    (void)header;
    (void)descs;
    (void)flags;
    a->chooses++;
    cl_copy(a->choose_q, question, sizeof(a->choose_q));
    a->choose_opts[0] = 0;
    for (i = 0; i < n; i++) {
        cl_cat(a->choose_opts, labels[i], sizeof(a->choose_opts));
        cl_cat(a->choose_opts, "|", sizeof(a->choose_opts));
    }
    *picked = a->choose_picked;
    cl_copy(other, a->choose_other, cap);
    return a->choose_ret;
}

static void on_plan(void *u, const char *text, long n)
{
    asker *a = (asker *)u;
    a->plans++;
    if (n < (long)sizeof(a->plan)) {
        memcpy(a->plan, text, (size_t)n);
        a->plan[n] = 0;
    }
}

static char dir[512];

static void put(const char *rel, const char *s, long n)
{
    char p[600];
    FILE *f;
    strcpy(p, dir);
    strcat(p, "/");
    strcat(p, rel);
    if (n < 0)
        n = (long)strlen(s);
    f = fopen(p, "wb");
    if (f) {
        fwrite(s, 1, (size_t)n, f);
        fclose(f);
    }
}

static long get(const char *rel, char *b, long cap)
{
    char p[600];
    FILE *f;
    long n;
    strcpy(p, dir);
    strcat(p, "/");
    strcat(p, rel);
    f = fopen(p, "rb");
    if (!f)
        return -1;
    n = (long)fread(b, 1, (size_t)cap - 1, f);
    fclose(f);
    b[n] = 0;
    return n;
}

static void sub(const char *rel)
{
    char p[600];
    strcpy(p, dir);
    strcat(p, "/");
    strcat(p, rel);
    mkdir(p, 0700);
}

/* a file's time set (seconds ago), for the newest-first orders */
static void age(const char *rel, long ago)
{
    char p[600];
    struct utimbuf ut;
    strcpy(p, dir);
    strcat(p, "/");
    strcat(p, rel);
    ut.actime = ut.modtime = time(0) - ago;
    utime(p, &ut);
}

/* run one call; the tool_result's content into text, its is_error back */
static int call(cl_tools *t, const char *name, const char *in, char *text, long cap)
{
    jw out;
    jv r, v;
    int err = -1;
    jw_init(&out);
    tools_run(t, "toolu_test", name, 1, in, (long)strlen(in), &out);
    text[0] = 0;
    if (json_parse(out.p, out.n, &r) == 0) {
        if (json_get(r, "content", &v))
            json_str(v, text, cap);
        err = json_get(r, "is_error", &v) && json_type(v) == J_TRUE;
        if (!json_get(r, "tool_use_id", &v) || !json_streq(v, "toolu_test"))
            err = -1;
    }
    jw_free(&out);
    return err;
}

/* ---- stubs: the API (recorded answers), the web (canned HTTP), the provider ---- */

typedef struct api_stub {
    const char *queue[8];
    int n, next;
    char *body[8];
    int nbody;
    int fail_with;              /* 0, or the send's return */
} api_stub;

static api_stub api;

static void to_stream(void *u, const char *e, const char *d, long n)
{
    stream_event((cl_stream *)u, e, d, n);
}

static int api_send(void *u, const char *body, long bn, cl_stream *st)
{
    long n;
    char *sse_text;
    sse s;
    (void)u;
    if (api.nbody < 8) {
        api.body[api.nbody] = (char *)malloc((size_t)bn + 1);
        memcpy(api.body[api.nbody], body, (size_t)bn);
        api.body[api.nbody++][bn] = 0;
    }
    stream_init(st, 0);
    if (api.fail_with)
        return api.fail_with;
    if (api.next >= api.n)
        return -1;
    sse_text = claude_load(api.queue[api.next++], &n);
    if (!sse_text)
        return -1;
    sse_init(&s, to_stream, st);
    sse_feed(&s, sse_text, n);
    sse_free(&s);
    free(sse_text);
    return st->state == ST_DONE ? 0 : -1;
}

static void api_reset(void)
{
    int i;
    for (i = 0; i < api.nbody; i++)
        free(api.body[i]);
    memset(&api, 0, sizeof(api));
}

typedef struct web_stub {
    const char *resp[4];
    int n, next, cur;
    long pos;
    char host[128];
    int port, tls;
    jw req;
} web_stub;

static web_stub web;

static int w_open(void *u, const char *host, int port, int tls)
{
    (void)u;
    cl_copy(web.host, host, sizeof(web.host));
    web.port = port;
    web.tls = tls;
    web.cur = web.next < web.n ? web.next++ : -1;
    web.pos = 0;
    return web.cur < 0 ? NET_ERROR : 0;
}

static long w_send(void *u, const char *b, long n)
{
    (void)u;
    jw_raw(&web.req, b, n);
    return n;
}

static long w_recv(void *u, char *b, long cap, int timeout_ms)
{
    long n;
    (void)u;
    (void)timeout_ms;
    if (web.cur < 0)
        return 0;
    n = (long)strlen(web.resp[web.cur]) - web.pos;
    if (n > 50)
        n = 50;
    if (n > cap)
        n = cap;
    memcpy(b, web.resp[web.cur] + web.pos, (size_t)n);
    web.pos += n;
    return n;
}

static void w_close(void *u)
{
    (void)u;
    web.cur = -1;
}

static const char *w_err(void *u)
{
    (void)u;
    return "stub";
}

static const cl_agent my_agents[] = {
    { "reviewer", "Reviews a change", "Read, Grep", "haiku", "You review code.", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
static cl_skill my_skills[1];
static const cl_command my_cmds[] = { { "hello", "Greets someone" }, { "secret", 0 } };

static int x_agents(void *u, const cl_agent **l)
{
    (void)u;
    *l = my_agents;
    return 1;
}

static int x_skills(void *u, const cl_skill **l)
{
    (void)u;
    *l = my_skills;
    return 1;
}

static int x_commands(void *u, const cl_command **l)
{
    (void)u;
    *l = my_cmds;
    return 2;
}

static int x_expand(void *u, const char *name, const char *args, jw *out, char *err, long cap)
{
    (void)u;
    if (strcmp(name, "hello")) {
        cl_copy(err, name, cap);
        return -1;
    }
    jw_rawz(out, "Say hello to ");
    jw_rawz(out, args);
    jw_rawz(out, " in German.");
    return 0;
}

static const cl_ext my_ext = { 0, x_agents, x_skills, x_commands, x_expand, 0 };

/* a project agent with a built-in's name hides the built-in */
static const cl_agent plan_agent[] = { { "Plan", "OUR-OWN-PLANNER", "Read", 0, "Plan our way.", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };
static int x_plan(void *u, const cl_agent **l)
{
    (void)u;
    *l = plan_agent;
    return 1;
}
static const cl_ext plan_ext = { 0, x_plan, 0, 0, 0, 0 };

/* an added working directory that holds only outside.txt (in the start
 * directory's parent: "/" is AmigaDOS's parent) */
static int added_root_file(void *u, const char *full)
{
    const char *e = full + strlen(full) - 12;
    (void)u;
    return e >= full && !strcmp(e, "/outside.txt");
}

static sys_posix sp;
static cl_sys sys;
static cl_tools t;
static asker a;

static void tools_setup(void)
{
    sys_posix_init(&sp, &sys);
    memset(&t, 0, sizeof(t));
    memset(&a, 0, sizeof(a));
    t.sys = &sys;
    CHECK_INT(sys.canon(sys.u, dir, t.root, sizeof(t.root)), 0);
    t.timeout_s = 60;
    t.u = &a;
    t.show = on_show;
    t.ask = on_ask;
    t.choose = on_choose;
    t.plan = on_plan;
    CHECK_INT(tools_init(&t), 0);
}

static void test_files(void)
{
    char text[4096], buf[256];
    jw out;
    tools_setup();
    /* Read: inside the start directory it does not ask (Claude Code); cat -n */
    a.answer = ASK_SESSION;
    CHECK_INT(call(&t, "Read", "{\"file_path\":\"S/Startup-Sequence\"}", text, sizeof(text)), 0);
    CHECK_STR(text, "     1\tSetPatch QUIET\n     2\tC:Version >NIL:\n");
    CHECK_INT(a.asked, 0);
    CHECK_INT(a.shown, 1);
    CHECK_STR(a.last_tool, "Read");
    /* offset and limit; the rest announced */
    CHECK_INT(call(&t, "Read", "{\"file_path\":\"long.txt\",\"offset\":3,\"limit\":2}", text, sizeof(text)), 0);
    CHECK_STR(text, "     3\tline 3\n     4\tline 4\n(6 more lines: read on with offset 5)\n");
    CHECK_INT(call(&t, "Read", "{\"file_path\":\"long.txt\",\"offset\":40}", text, sizeof(text)), 0);
    CHECK(strstr(text, "shorter than the provided offset (40). The file has 10 lines.") != 0);
    CHECK_INT(a.asked, 0);
    CHECK_INT(call(&t, "Read", "{\"file_path\":\"bin.dat\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "binary") != 0);
    CHECK_INT(call(&t, "Read", "{\"file_path\":\"nothere\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "File does not exist") != 0);
    CHECK_INT(call(&t, "Read", "{\"file_path\":\"sub\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "directory") != 0);
    CHECK_INT(call(&t, "Read", "{\"file_path\":\"empty.txt\"}", text, sizeof(text)), 0);
    CHECK(strstr(text, "contents are empty") != 0);
    /* a line longer than 2000 characters is cut */
    CHECK_INT(call(&t, "Read", "{\"file_path\":\"wide.txt\"}", text, sizeof(text)), 0);
    CHECK_INT((long)strlen(text), 7 + 2000 + 1);
    /* a Latin-1 file reaches Claude as UTF-8 */
    CHECK_INT(call(&t, "Read", "{\"file_path\":\"notes.txt\"}", text, sizeof(text)), 0);
    CHECK(!strncmp(text, "     1\tGr\xc3\xbc\xc3\x9f" "e Welt\n", 20));

    /* Write: an existing file not read yet is refused, unasked */
    a.asked = 0;
    CHECK_INT(call(&t, "Write", "{\"file_path\":\"other.txt\",\"content\":\"x\"}", text, sizeof(text)), 1);
    CHECK_STR(text, "File has not been read yet. Read it first before writing to it.");
    CHECK_INT(a.asked, 0);
    /* a new file: asks every time; "no" writes nothing */
    a.answer = ASK_NO;
    CHECK_INT(call(&t, "Write", "{\"file_path\":\"new.txt\",\"content\":\"hello\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "declined") != 0);
    CHECK_INT(get("new.txt", buf, sizeof(buf)), -1);
    CHECK_INT(a.asked, 1);
    /* No with a comment: Claude is told it, the turn goes on (no stop) */
    strcpy(a.note, "call it hello.txt");
    t.stop = 0;
    CHECK_INT(call(&t, "Write", "{\"file_path\":\"new.txt\",\"content\":\"hello\"}", text, sizeof(text)), 1);
    CHECK_STR(text, "the user declined this tool call and said: call it hello.txt");
    CHECK_INT(t.stop, 0);
    /* Yes with a comment: the call runs, the comment follows its result */
    a.answer = ASK_ONCE;
    strcpy(a.note, "keep it short");
    CHECK_INT(call(&t, "Write", "{\"file_path\":\"note.txt\",\"content\":\"x\"}", text, sizeof(text)), 0);
    CHECK(strstr(text, "File created successfully at: ") == text);
    CHECK(strstr(text, "\n\nThe user allowed this call with a comment: keep it short") != 0);
    CHECK_STR(t.note, "");
    a.note[0] = 0;
    a.asked = 1;
    a.answer = ASK_ONCE;
    CHECK_INT(call(&t, "Write", "{\"file_path\":\"new.txt\",\"content\":\"hello \\u00fc\"}", text, sizeof(text)), 0);
    CHECK(!strncmp(text, "File created successfully at: ", 30));
    CHECK_INT(get("new.txt", buf, sizeof(buf)), 8);
    CHECK_STR(buf, "hello \xc3\xbc");
    CHECK_INT(a.asked, 2);
    /* written by Claude: it may be written again without a Read */
    CHECK_INT(call(&t, "Write", "{\"file_path\":\"new.txt\",\"content\":\"hello \\u00fc\"}", text, sizeof(text)), 0);
    CHECK(strstr(text, "has been updated.") != 0);
    /* changed behind Claude's back since the Read: refused */
    age("new.txt", 100);
    CHECK_INT(call(&t, "Write", "{\"file_path\":\"new.txt\",\"content\":\"x\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "modified since read") != 0);
    CHECK_INT(call(&t, "Read", "{\"file_path\":\"new.txt\"}", text, sizeof(text)), 0);

    /* Edit: unique match; a Latin-1 file stays Latin-1; the snippet */
    CHECK_INT(call(&t, "Edit", "{\"file_path\":\"notes.txt\",\"old_string\":\"Gr\\u00fc\\u00dfe Welt\","
                               "\"new_string\":\"Gr\\u00fc\\u00dfe World\"}", text, sizeof(text)), 0);
    CHECK(strstr(text, "has been updated (kept as Latin-1). Here's the result of running `cat -n` on a snippet") != 0);
    CHECK(strstr(text, "     1\tGr\xc3\xbc\xc3\x9f" "e World\n     2\tline two\n") != 0);
    get("notes.txt", buf, sizeof(buf));
    CHECK_STR(buf, "Gr\xfc\xdf" "e World\nline two\nline two\n");
    CHECK_INT(call(&t, "Edit", "{\"file_path\":\"notes.txt\",\"old_string\":\"line two\",\"new_string\":\"x\"}",
                   text, sizeof(text)), 1);
    CHECK(!strncmp(text, "Found 2 matches of the string to replace, but replace_all is false.", 67));
    CHECK_INT(call(&t, "Edit", "{\"file_path\":\"notes.txt\",\"old_string\":\"absent\",\"new_string\":\"x\"}",
                   text, sizeof(text)), 1);
    CHECK_STR(text, "String to replace not found in file.\nString: absent");
    CHECK_INT(call(&t, "Edit", "{\"file_path\":\"notes.txt\",\"old_string\":\"two\",\"new_string\":\"two\"}",
                   text, sizeof(text)), 1);
    CHECK(strstr(text, "No changes to make") != 0);
    get("notes.txt", buf, sizeof(buf));
    CHECK_STR(buf, "Gr\xfc\xdf" "e World\nline two\nline two\n");
    /* replace_all */
    CHECK_INT(call(&t, "Edit", "{\"file_path\":\"notes.txt\",\"old_string\":\"line two\",\"new_string\":\"zwei\","
                               "\"replace_all\":true}", text, sizeof(text)), 0);
    get("notes.txt", buf, sizeof(buf));
    CHECK_STR(buf, "Gr\xfc\xdf" "e World\nzwei\nzwei\n");
    /* an empty old_string on a new file creates it */
    CHECK_INT(call(&t, "Edit", "{\"file_path\":\"made.txt\",\"old_string\":\"\",\"new_string\":\"made\\n\"}",
                   text, sizeof(text)), 0);
    CHECK_INT(get("made.txt", buf, sizeof(buf)), 5);
    /* Edit of a file never read: refused */
    CHECK_INT(call(&t, "Edit", "{\"file_path\":\"other.txt\",\"old_string\":\"o\",\"new_string\":\"x\"}", text,
                   sizeof(text)), 1);
    CHECK(strstr(text, "has not been read yet") != 0);

    /* MultiEdit: in order, each on the result of the ones before ... */
    CHECK_INT(call(&t, "Read", "{\"file_path\":\"other.txt\"}", text, sizeof(text)), 0);
    CHECK_INT(call(&t, "MultiEdit", "{\"file_path\":\"other.txt\",\"edits\":[{\"old_string\":\"alpha\",\"new_string\":"
                                    "\"ALPHA\"},{\"old_string\":\"ALPHA beta\",\"new_string\":\"ab\"},{\"old_string\":"
                                    "\"g\",\"new_string\":\"G\",\"replace_all\":true}]}", text, sizeof(text)), 0);
    CHECK(strstr(text, "Applied 3 edits to ") != 0);
    get("other.txt", buf, sizeof(buf));
    CHECK_STR(buf, "ab Gamma\nGG\n");
    /* ... and atomic: one failing edit leaves the file as it was */
    CHECK_INT(call(&t, "MultiEdit", "{\"file_path\":\"other.txt\",\"edits\":[{\"old_string\":\"ab\",\"new_string\":"
                                    "\"x\"},{\"old_string\":\"nope\",\"new_string\":\"y\"}]}", text, sizeof(text)), 1);
    CHECK(!strncmp(text, "Edit 2 of 2 failed, so none was made: String to replace not found", 65));
    get("other.txt", buf, sizeof(buf));
    CHECK_STR(buf, "ab Gamma\nGG\n");

    /* outside the start directory: asked even though reads are allowed */
    a.asked = 0;
    a.answer = ASK_NO;
    CHECK_INT(call(&t, "Read", "{\"file_path\":\"/outside.txt\"}", text, sizeof(text)), 1);
    CHECK_INT(a.asked, 1);
    CHECK_INT(a.outside, 1);
    /* ... unless it lies in an added working directory (--add-dir, /add-dir) */
    a.asked = 0;
    t.added = added_root_file;
    CHECK_INT(call(&t, "Read", "{\"file_path\":\"/outside.txt\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "File does not exist") != 0);
    CHECK_INT(a.asked, 0);
    t.added = 0;

    /* invalid input: an error result naming the parameter, nothing run, nothing asked */
    a.asked = 0;
    CHECK_INT(call(&t, "Write", "{\"file_path\":\"x\"}", text, sizeof(text)), 1);
    CHECK_STR(text, "InputValidationError: Write failed due to the following issue:\n"
                    "The required parameter `content` is missing");
    CHECK_INT(call(&t, "no_such_tool", "{}", text, sizeof(text)), 1);
    CHECK_STR(text, "No such tool available: no_such_tool");
    /* an A2 name (a resumed session's model may still call it) */
    CHECK_INT(call(&t, "read_file", "{\"path\":\"x\"}", text, sizeof(text)), 1);
    CHECK_STR(text, "read_file is no longer a tool; use Read instead");
    jw_init(&out);
    {
        const char cut[] = "{\"file_path\": \"RAM:x\", \"content\": \"cut";
        tools_run(&t, "toolu_bad", "Write", 0, cut, (long)strlen(cut), &out);
    }
    CHECK(strstr(out.p, "\"is_error\":true") != 0);
    CHECK(strstr(out.p, "not valid JSON") != 0);
    jw_free(&out);
    CHECK_INT(a.asked, 0);

    /* the preview: an edit shows the file before and after it, before the
     * question; the answer "no, and tell Claude" stops the round */
    t.preview = on_preview;
    t.result = on_result;
    a.asked = a.previews = a.results = 0;
    a.answer = ASK_STOP;
    CHECK_INT(call(&t, "Edit", "{\"file_path\":\"new.txt\",\"old_string\":\"hello\",\"new_string\":\"bye\"}",
                   text, sizeof(text)), 1);
    CHECK_INT(a.previews, 1);
    CHECK_STR(a.before, "hello \xc3\xbc");
    CHECK_STR(a.after, "bye \xc3\xbc");
    CHECK(strstr(text, "tell you what to do differently") != 0);
    CHECK_INT(t.stop, 1);
    CHECK_INT(get("new.txt", buf, sizeof(buf)), 8);
    /* the rest of that round is not run, not even asked */
    CHECK_INT(call(&t, "Read", "{\"file_path\":\"new.txt\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "not run") != 0);
    CHECK_INT(a.asked, 1);
    CHECK_INT(a.results, 2);
    CHECK_INT(a.last_error, 1);
    t.stop = 0;
    /* a write of a new file previews no "before" */
    a.answer = ASK_ONCE;
    CHECK_INT(call(&t, "Write", "{\"file_path\":\"fresh.txt\",\"content\":\"one\\ntwo\\n\"}", text, sizeof(text)), 0);
    CHECK_INT(a.previews, 2);
    CHECK_STR(a.before, "");
    CHECK_STR(a.after, "one\ntwo\n");
    /* accept edits: the edit runs without a question */
    t.perm.mode = PERM_ACCEPT;
    a.asked = 0;
    CHECK_INT(call(&t, "Edit", "{\"file_path\":\"fresh.txt\",\"old_string\":\"two\",\"new_string\":\"2\"}",
                   text, sizeof(text)), 0);
    CHECK_INT(a.asked, 0);
    CHECK_INT(get("fresh.txt", buf, sizeof(buf)), 6);
    /* plan mode: Bash is refused with a result that says why, unasked --
     * but a command that only looks runs (Claude Code's read-only set) */
    t.perm.mode = PERM_PLAN;
    CHECK_INT(call(&t, "Bash", "{\"command\":\"rm no\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "plan mode is on") != 0);
    CHECK_INT(call(&t, "Bash", "{\"command\":\"echo looks\"}", text, sizeof(text)), 0);
    CHECK(strstr(text, "looks") != 0);
    CHECK_INT(bash_read_only("List S: ; type s:startup-sequence | grep x"), 1);
    CHECK_INT(bash_read_only("echo x >RAM:f"), 0);         /* writes a file */
    CHECK_INT(bash_read_only("ls && rm x"), 0);            /* one part changes things */
    CHECK_INT(bash_read_only("C:List S:"), 0);             /* a path is not the read-only List */
    CHECK_INT(bash_read_only("echo `delete x`"), 0);
    CHECK_INT(call(&t, "Write", "{\"file_path\":\"fresh.txt\",\"content\":\"x\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "plan mode is on") != 0);
    CHECK_INT(a.asked, 0);
    CHECK_INT(call(&t, "TodoWrite", "{\"todos\":[{\"content\":\"x\",\"status\":\"pending\",\"activeForm\":\"X\"}]}",
                   text, sizeof(text)), 0);
    CHECK(strstr(text, "Todos have been modified successfully") != 0);
    t.perm.mode = PERM_DEFAULT;
    t.preview = 0;
    t.result = 0;
    tools_free(&t);
}

static void test_search(void)
{
    char text[4096], s[600];
    tools_setup();
    a.answer = ASK_SESSION;
    /* Glob: newest first, full paths, directories marked */
    CHECK_INT(call(&t, "Glob", "{\"pattern\":\"**/*.c\"}", text, sizeof(text)), 0);
    cl_copy(s, t.root, sizeof(s));
    cl_cat(s, "/src/b.c\n", sizeof(s));
    CHECK(!strncmp(text, s, strlen(s)));        /* b.c is the newest */
    CHECK(strstr(text, "/src/deep/a.c\n") != 0);
    CHECK(strstr(text, ".h") == 0);
    CHECK_INT(call(&t, "Glob", "{\"pattern\":\"src/*.{c,h}\"}", text, sizeof(text)), 0);
    CHECK(strstr(text, "/src/b.c\n") != 0 && strstr(text, "/src/b.h\n") != 0 && strstr(text, "deep/a.c") == 0);
    /* the AmigaDOS form, case ignored, a directory found too */
    CHECK_INT(call(&t, "Glob", "{\"pattern\":\"#?.C\",\"path\":\"src\"}", text, sizeof(text)), 0);
    CHECK(strstr(text, "/src/b.c\n") != 0);
    CHECK_INT(call(&t, "Glob", "{\"pattern\":\"de#?\",\"path\":\"src\"}", text, sizeof(text)), 0);
    CHECK(strstr(text, "/src/deep/\n") != 0);
    CHECK_INT(call(&t, "Glob", "{\"pattern\":\"*.nothing\"}", text, sizeof(text)), 0);
    CHECK_STR(text, "No files found");
    CHECK_INT(call(&t, "Glob", "{\"pattern\":\"*\",\"path\":\"nodir\"}", text, sizeof(text)), 1);

    /* Grep: files_with_matches by default, newest first */
    CHECK_INT(call(&t, "Grep", "{\"pattern\":\"needle\"}", text, sizeof(text)), 0);
    CHECK(!strncmp(text, "Found 2 files\n", 14));
    CHECK(strstr(text, "/src/b.c\n") < strstr(text, "/src/deep/a.c\n"));
    /* content with line numbers and context, one file: no file name */
    CHECK_INT(call(&t, "Grep", "{\"pattern\":\"ne+dle\",\"path\":\"src/deep/a.c\",\"output_mode\":\"content\","
                               "\"-n\":true,\"-B\":1}", text, sizeof(text)), 0);
    CHECK_STR(text, "1-int a;\n2:/* a needle here */\n");
    /* -C, a separator between groups apart */
    CHECK_INT(call(&t, "Grep", "{\"pattern\":\"^x[0-9]$\",\"path\":\"ctx.txt\",\"output_mode\":\"content\","
                               "\"-n\":true,\"-C\":1}", text, sizeof(text)), 0);
    CHECK_STR(text, "1-a\n2:x1\n3-b\n--\n5-e\n6:x2\n7-f\n");
    /* -i, a glob filter, a type filter, count */
    CHECK_INT(call(&t, "Grep", "{\"pattern\":\"NEEDLE\",\"-i\":true,\"glob\":\"*.h\"}", text, sizeof(text)), 0);
    CHECK(!strncmp(text, "Found 1 file\n", 13) && strstr(text, "/src/b.h") != 0);
    CHECK_INT(call(&t, "Grep", "{\"pattern\":\"NEEDLE\",\"-i\":true,\"type\":\"c\",\"output_mode\":\"count\"}",
                   text, sizeof(text)), 0);
    CHECK(strstr(text, "/src/b.h:1\n") != 0);
    CHECK(strstr(text, "/src/b.c:2\n") != 0);
    CHECK(strstr(text, "Found 4 total occurrences across 3 files.") != 0);
    CHECK_INT(call(&t, "Grep", "{\"pattern\":\"needle\",\"type\":\"pascal\"}", text, sizeof(text)), 0);
    CHECK_STR(text, "No files found");
    CHECK_INT(call(&t, "Grep", "{\"pattern\":\"x\",\"type\":\"cobol\"}", text, sizeof(text)), 1);
    /* head_limit and offset over the output lines */
    CHECK_INT(call(&t, "Grep", "{\"pattern\":\"[a-z]\",\"path\":\"ctx.txt\",\"output_mode\":\"content\","
                               "\"head_limit\":2,\"offset\":1}", text, sizeof(text)), 0);
    CHECK_STR(text, "x1\nb\n");
    /* multiline: a match across lines */
    CHECK_INT(call(&t, "Grep", "{\"pattern\":\"x1.b\",\"path\":\"ctx.txt\",\"output_mode\":\"content\","
                               "\"multiline\":true,\"-n\":true}", text, sizeof(text)), 0);
    CHECK_STR(text, "2:x1\n3:b\n");
    CHECK_INT(call(&t, "Grep", "{\"pattern\":\"x1.b\",\"path\":\"ctx.txt\",\"output_mode\":\"content\"}",
                   text, sizeof(text)), 0);
    CHECK_STR(text, "No matches found");
    /* a pattern the engine refuses: a reason, not a guess */
    CHECK_INT(call(&t, "Grep", "{\"pattern\":\"(?=x)\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "look-around") != 0);
    CHECK_INT(a.asked, 0);
    tools_free(&t);
}

static void test_bash(void)
{
    char text[4096], seen[400];
    int k;
    tools_setup();
    /* a command: its output and return code; 10 and more is a failure */
    a.answer = ASK_ONCE;
    CHECK_INT(call(&t, "Bash", "{\"command\":\"echo hi; exit 5\",\"description\":\"say hi\"}", text, sizeof(text)), 0);
    CHECK_STR(text, "Return code 5.\nhi\n");
    CHECK_INT(call(&t, "Bash", "{\"command\":\"echo bad; exit 10\"}", text, sizeof(text)), 1);
    CHECK_STR(text, "Return code 10.\nbad\n");
    /* control characters from the model never reach the screen as such */
    CHECK_INT(call(&t, "Bash", "{\"command\":\"echo \\u001b[2J\"}", text, sizeof(text)), 0);
    CHECK(strchr(a.last, 0x1b) == 0);
    /* in the background: an id at once, the output read in pieces */
    CHECK_INT(call(&t, "Bash", "{\"command\":\"echo one; sleep 1; echo two\",\"run_in_background\":true}", text,
                   sizeof(text)), 0);
    CHECK(strstr(text, "Command running in background with ID: bash_1. Output is being written to: ") == text);
    for (k = 0; k < 400; k++) {
        struct timespec ts;
        CHECK_INT(call(&t, "BashOutput", "{\"bash_id\":\"bash_1\"}", text, sizeof(text)), 0);
        if (strstr(text, "one\n"))
            break;
        ts.tv_sec = 0;
        ts.tv_nsec = 5000000L;
        nanosleep(&ts, 0);
    }
    CHECK(strstr(text, "<status>running</status>") != 0);
    CHECK_STR(strstr(text, "<stdout>") ? strstr(text, "<stdout>") : text, "<stdout>\none\n</stdout>\n");
    seen[0] = 0;
    for (k = 0; k < 400; k++) {
        CHECK_INT(call(&t, "BashOutput", "{\"bash_id\":\"bash_1\",\"filter\":\"t.o\"}", text, sizeof(text)), 0);
        if (strstr(text, "<stdout>"))
            cl_cat(seen, strstr(text, "<stdout>"), sizeof(seen));
        if (strstr(text, "completed"))
            break;
        {
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 10000000L;
            nanosleep(&ts, 0);
        }
    }
    CHECK(strstr(text, "<status>completed</status>") != 0);
    CHECK(strstr(text, "<exit_code>0</exit_code>") != 0);
    /* the new output only, filtered */
    CHECK_STR(seen, "<stdout>\ntwo\n</stdout>\n");
    /* KillShell: a long one stopped */
    CHECK_INT(call(&t, "Bash", "{\"command\":\"sleep 30\",\"run_in_background\":true}", text, sizeof(text)), 0);
    CHECK(!strncmp(text, "Command running in background with ID: bash_2. Output", 53));
    CHECK_INT(call(&t, "KillShell", "{\"shell_id\":\"bash_2\"}", text, sizeof(text)), 0);
    CHECK_STR(text, "Successfully killed shell: bash_2 (sleep 30)");
    CHECK_INT(call(&t, "BashOutput", "{\"bash_id\":\"bash_2\"}", text, sizeof(text)), 0);
    CHECK(strstr(text, "<status>killed</status>") != 0);
    CHECK_INT(call(&t, "KillShell", "{\"shell_id\":\"bash_2\"}", text, sizeof(text)), 1);
    CHECK_INT(call(&t, "BashOutput", "{\"bash_id\":\"bash_9\"}", text, sizeof(text)), 1);
    CHECK_STR(text, "No shell found with ID: bash_9");
    /* still running at the end: stopped by tools_free */
    CHECK_INT(call(&t, "Bash", "{\"command\":\"sleep 30\",\"run_in_background\":true}", text, sizeof(text)), 0);
    tools_free(&t);
    CHECK(!sp.jobs[2].used || sp.jobs[2].ended);
}

static void test_questions(void)
{
    char text[2048];
    static const char q[] = "{\"questions\":[{\"question\":\"Which size?\",\"header\":\"Size\",\"multiSelect\":false,"
                            "\"options\":[{\"label\":\"80x24\",\"description\":\"classic\"},{\"label\":\"Full\","
                            "\"description\":\"whole screen\"}]},{\"question\":\"Which fonts?\",\"header\":\"Fonts\","
                            "\"multiSelect\":true,\"options\":[{\"label\":\"Topaz\",\"description\":\"\"},"
                            "{\"label\":\"Unifont\",\"description\":\"\"},{\"label\":\"Fixed\",\"description\":\"\"}]}]}";
    tools_setup();
    /* AskUserQuestion: both questions asked; a pick, then several */
    a.choose_ret = 1;
    a.choose_picked = 5;
    CHECK_INT(call(&t, "AskUserQuestion", q, text, sizeof(text)), 0);
    CHECK_INT(a.chooses, 2);
    CHECK_STR(text, "User has answered your questions: \"Which size?\"=\"Full\", \"Which fonts?\"=\"Topaz, Fixed\". You can "
                    "now continue with the user's answers in mind.");
    /* (the multi-select's answer comes from the bits: 1 and 4, Topaz and Fixed) */
    a.chooses = 0;
    a.choose_ret = 2;           /* n: the user's own answer */
    cl_copy(a.choose_other, "132x50", sizeof(a.choose_other));
    {
        static const char one[] = "{\"questions\":[{\"question\":\"Which size?\",\"header\":\"Size\",\"multiSelect\":"
                                  "false,\"options\":[{\"label\":\"80x24\",\"description\":\"\"},{\"label\":\"Full\","
                                  "\"description\":\"\"}]}]}";
        CHECK_INT(call(&t, "AskUserQuestion", one, text, sizeof(text)), 0);
        CHECK(strstr(text, "\"Which size?\"=\"132x50\"") != 0);
        a.choose_ret = -1;
        CHECK_INT(call(&t, "AskUserQuestion", one, text, sizeof(text)), 1);
        CHECK(strstr(text, "declined") != 0);
    }
    CHECK_STR(a.choose_opts, "80x24|Full|");
    /* EnterPlanMode: asked; yes switches the mode */
    a.choose_ret = 0;
    CHECK_INT(call(&t, "EnterPlanMode", "{}", text, sizeof(text)), 0);
    CHECK_INT(t.perm.mode, PERM_PLAN);
    CHECK(strstr(text, "Entered plan mode") != 0);
    CHECK_INT(call(&t, "EnterPlanMode", "{}", text, sizeof(text)), 1);
    /* ExitPlanMode: the plan shown, "no" keeps planning, "yes, auto-accept" leaves */
    a.choose_ret = 2;
    CHECK_INT(call(&t, "ExitPlanMode", "{\"plan\":\"1. Read\\n2. Edit\"}", text, sizeof(text)), 1);
    CHECK_INT(t.perm.mode, PERM_PLAN);
    CHECK_INT(a.plans, 1);
    CHECK_STR(a.plan, "1. Read\n2. Edit");
    CHECK(strstr(a.choose_opts, "Yes, and auto-accept edits|") != 0);
    a.choose_ret = 0;
    CHECK_INT(call(&t, "ExitPlanMode", "{\"plan\":\"1. Read\"}", text, sizeof(text)), 0);
    CHECK_INT(t.perm.mode, PERM_ACCEPT);
    CHECK(!strncmp(text, "User has approved your plan.", 28));
    CHECK_INT(call(&t, "ExitPlanMode", "{\"plan\":\"x\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "not on") != 0);
    t.perm.mode = PERM_DEFAULT;
    /* no screen to ask on: an error result, nothing guessed */
    t.choose = 0;
    CHECK_INT(call(&t, "AskUserQuestion", q, text, sizeof(text)), 1);
    tools_free(&t);
}

static void test_ext(void)
{
    char text[2048], p[600];
    const char *js;
    tools_setup();
    strcpy(p, dir);
    strcat(p, "/greet/SKILL.md");
    my_skills[0].name = "greet";
    my_skills[0].description = "Greets in style";
    my_skills[0].path = p;
    t.ext = &my_ext;
    /* the lists go into the descriptions; a command without one is not offered */
    js = tools_json(&t, "claude-opus-5-5");
    CHECK(strstr(js, "- greet: Greets in style\\n") != 0);
    CHECK(strstr(js, "- /hello: Greets someone\\n") != 0);
    CHECK(strstr(js, "secret") == 0);
    /* Skill: asked, the body after the frontmatter, the arguments */
    a.answer = ASK_ONCE;
    CHECK_INT(call(&t, "Skill", "{\"skill\":\"greet\",\"args\":\"Bob\"}", text, sizeof(text)), 0);
    CHECK(!strncmp(text, "Launching skill: greet\nBase directory for this skill: ", 54));
    CHECK_STR(strstr(text, "/greet\n") ? strstr(text, "/greet\n") : text, "/greet\n\nSay Hallo.\n\nARGUMENTS: Bob");
    CHECK(strstr(text, "name: greet") == 0);
    CHECK_INT(a.asked, 1);
    CHECK_INT(call(&t, "Skill", "{\"skill\":\"nope\"}", text, sizeof(text)), 1);
    CHECK_STR(text, "Unknown skill: nope");
    /* SlashCommand: the provider's expansion */
    CHECK_INT(call(&t, "SlashCommand", "{\"command\":\"/hello Amiga fans\"}", text, sizeof(text)), 0);
    CHECK_STR(text, "Launching command /hello. Carry out these instructions:\n\nSay hello to Amiga fans in German.");
    CHECK_INT(call(&t, "SlashCommand", "{\"command\":\"/nope\"}", text, sizeof(text)), 1);
    CHECK_STR(text, "Unknown slash command: /nope");
    tools_free(&t);
}

static void test_task(void)
{
    char text[4096];
    jv b, x;
    tools_setup();
    api_reset();
    t.api.send = api_send;
    t.model = "claude-opus-5-5";
    t.ext = &my_ext;
    a.answer = ASK_SESSION;
    /* the agents in Task's description: the built-ins and the provider's */
    CHECK(strstr(tools_json(&t, "claude-opus-5-5"), "- Explore: Fast agent") != 0);
    CHECK(strstr(tools_json(&t, "claude-opus-5-5"), "- reviewer: Reviews a change (Tools: Read, Grep)") != 0);
    {
        /* a provided "Plan" takes the built-in's place, once */
        const char *js;
        const char *keep_json = t.json;
        t.json = 0;
        t.ext = &plan_ext;
        js = tools_json(&t, "claude-opus-5-5");
        CHECK(strstr(js, "- Plan: OUR-OWN-PLANNER (Tools: Read)") != 0);
        CHECK(strstr(js, "- Plan: Software architect") == 0);
        CHECK(strstr(js, "- Explore: Fast agent") != 0);
        free(t.json);
        t.json = (char *)keep_json;
        t.ext = &my_ext;
    }
    /* Explore: its own system prompt and tools, a Grep, its report */
    api.queue[0] = "agent_tool.sse";
    api.queue[1] = "agent_final.sse";
    api.n = 2;
    CHECK_INT(call(&t, "Task", "{\"description\":\"Find startup\",\"prompt\":\"What runs first?\","
                               "\"subagent_type\":\"Explore\"}", text, sizeof(text)), 0);
    CHECK_STR(text, "S/Startup-Sequence runs SetPatch first (line 1: SetPatch QUIET).\n\n(Agent Explore: 1 tool use.)");
    CHECK_INT(api.nbody, 2);
    if (api.nbody == 2) {
        CHECK_INT(json_parse(api.body[0], (long)strlen(api.body[0]), &b), 0);
        CHECK(json_get(b, "model", &x) && json_streq(x, "claude-opus-5-5"));
        CHECK(strstr(api.body[0], "file search specialist") != 0);
        CHECK(strstr(api.body[0], "{\"name\":\"Grep\",") != 0);
        CHECK(strstr(api.body[0], "{\"name\":\"Edit\",") == 0);
        CHECK(strstr(api.body[0], "{\"name\":\"Task\",") == 0);
        CHECK(strstr(api.body[0], "web_search") == 0);
        /* the second request carries the Grep's result */
        CHECK(strstr(api.body[1], "1:SetPatch QUIET") != 0);
    }
    /* the subagent's tool calls were shown; a read inside the start directory asks nothing */
    CHECK_STR(a.last_tool, "Grep");
    CHECK_INT(a.asked, 0);
    /* the provider's agent with its model alias */
    api_reset();
    t.api.send = api_send;
    api.queue[0] = "agent_final.sse";
    api.n = 1;
    CHECK_INT(call(&t, "Task", "{\"description\":\"Review\",\"prompt\":\"Look.\",\"subagent_type\":\"reviewer\"}",
                   text, sizeof(text)), 0);
    CHECK(api.nbody == 1 && strstr(api.body[0], "\"model\":\"claude-haiku-4-5\"") != 0);
    CHECK(api.nbody == 1 && strstr(api.body[0], "You review code.") != 0);
    /* unknown agents, a failed request, a stop */
    CHECK_INT(call(&t, "Task", "{\"description\":\"d\",\"prompt\":\"p\",\"subagent_type\":\"nobody\"}", text,
                   sizeof(text)), 1);
    CHECK(strstr(text, "Available agents: general-purpose, Explore, Plan, statusline-setup, claude-code-guide, claude, reviewer") != 0);
    api.fail_with = -2;
    CHECK_INT(call(&t, "Task", "{\"description\":\"d\",\"prompt\":\"p\",\"subagent_type\":\"Plan\"}", text,
                   sizeof(text)), 1);
    CHECK(strstr(text, "stopped the agent") != 0);
    /* no Task at the depth limit (Claude Code: three layers; CLAUDE_CODE_MAX_SUBAGENT_SPAWN_DEPTH) */
    t.depth = 3;
    CHECK_INT(call(&t, "Task", "{\"description\":\"d\",\"prompt\":\"p\",\"subagent_type\":\"Plan\"}", text,
                   sizeof(text)), 1);
    CHECK(strstr(text, "No such tool available here") != 0);
    t.depth = 1;
    t.max_depth = 1;
    CHECK_INT(call(&t, "Task", "{\"description\":\"d\",\"prompt\":\"p\",\"subagent_type\":\"Plan\"}", text,
                   sizeof(text)), 1);
    CHECK(strstr(text, "No such tool available here") != 0);
    t.depth = 0;
    t.max_depth = 0;
    api_reset();
    tools_free(&t);
}

static void test_webfetch(void)
{
    char text[4096];
    static const char redirect[] = "HTTP/1.1 301 Moved\r\nLocation: /page\r\nContent-Length: 0\r\n\r\n";
    static const char page[] =
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 157\r\n\r\n"
        "<html><head><title>Test</title><script>x=1</script></head><body><h1>Hello</h1><p>From the "
        "<a href=\"/f\">fixture</a> &amp; more.</p><ul><li>one</li><li>two</li></ul></body></html>";
    static const char away[] = "HTTP/1.1 302 Found\r\nLocation: https://elsewhere.example/x\r\nContent-Length: 0\r\n\r\n";
    cl_net wn;
    tools_setup();
    api_reset();
    jw_free(&web.req);
    memset(&web, 0, sizeof(web));
    jw_init(&web.req);
    wn.u = 0;
    wn.open = w_open;
    wn.send = w_send;
    wn.recv = w_recv;
    wn.close = w_close;
    wn.err = w_err;
    t.api.send = api_send;
    t.web = &wn;
    CHECK(strstr(tools_json(&t, "claude-opus-5-5"), "{\"name\":\"WebFetch\",") != 0);
    /* a redirect on the same host followed, the page as Markdown for the small model */
    web.resp[0] = redirect;
    web.resp[1] = page;
    web.n = 2;
    api.queue[0] = "fetch_answer.sse";
    api.n = 1;
    a.answer = ASK_ONCE;
    CHECK_INT(call(&t, "WebFetch", "{\"url\":\"http://test.example:8080/start\",\"prompt\":\"What does it say?\"}",
                   text, sizeof(text)), 0);
    CHECK_STR(text, "The page is the UP-Term test page. It says hello from the fixture and lists two items: one and two.");
    CHECK_INT(a.asked, 1);
    CHECK_STR(web.host, "test.example");
    CHECK_INT(web.port, 8080);
    CHECK(strstr(web.req.p, "GET /start HTTP/1.1\r\nHost: test.example:8080\r\n") != 0);
    CHECK(strstr(web.req.p, "GET /page HTTP/1.1\r\n") != 0);
    CHECK(strstr(web.req.p, "x-api-key") == 0);
    CHECK_INT(api.nbody, 1);
    if (api.nbody) {
        CHECK(strstr(api.body[0], "\"model\":\"claude-haiku-4-5\"") != 0);
        CHECK(strstr(api.body[0], "\"output_config\"") == 0);
        CHECK(strstr(api.body[0], "# Test\\n\\n# Hello\\n\\nFrom the [fixture](/f) & more.\\n\\n- one\\n- two") != 0);
        CHECK(strstr(api.body[0], "x=1") == 0);
        CHECK(strstr(api.body[0], "What does it say?") != 0);
        CHECK(strstr(api.body[0], "125-character maximum") != 0);
    }
    /* another host: told, not followed */
    web.next = 0;
    web.resp[0] = away;
    web.n = 1;
    CHECK_INT(call(&t, "WebFetch", "{\"url\":\"http://test.example/a\",\"prompt\":\"p\"}", text, sizeof(text)), 0);
    CHECK(!strncmp(text, "REDIRECT DETECTED: The URL redirects to a different host.", 57));
    CHECK(strstr(text, "Redirect URL: https://elsewhere.example/x") != 0);
    CHECK_INT(api.nbody, 1);
    /* not a URL, a failed connection */
    CHECK_INT(call(&t, "WebFetch", "{\"url\":\"ftp://x/\",\"prompt\":\"p\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "Invalid URL") != 0);
    CHECK_INT(call(&t, "WebFetch", "{\"url\":\"http://test.example/a\",\"prompt\":\"p\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "Failed to fetch") != 0);
    jw_free(&web.req);
    api_reset();
    tools_free(&t);
}

static void mk_tree(void)
{
    const char *base = getenv("TMPDIR");
    char wide[2100];
    int i;
    strcpy(dir, base && *base ? base : "/tmp");
    if (dir[strlen(dir) - 1] == '/')
        dir[strlen(dir) - 1] = 0;
    strcat(dir, "/claude_tools_XXXXXX");
    if (!mkdtemp(dir)) {
        dir[0] = 0;
        return;
    }
    sub("S");
    sub("sub");
    sub("src");
    sub("src/deep");
    sub("greet");
    put("S/Startup-Sequence", "SetPatch QUIET\nC:Version >NIL:\n", -1);
    put("notes.txt", "Gr\xfc\xdf" "e Welt\nline two\nline two\n", -1);
    put("bin.dat", "ab\0cd", 5);
    put("empty.txt", "", 0);
    put("other.txt", "alpha beta gamma\ngg\n", -1);
    put("long.txt", "line 1\nline 2\nline 3\nline 4\nline 5\nline 6\nline 7\nline 8\nline 9\nline 10\n", -1);
    put("ctx.txt", "a\nx1\nb\nc\ne\nx2\nf\n", -1);
    for (i = 0; i < 2050; i++)
        wide[i] = 'w';
    wide[2050] = '\n';
    put("wide.txt", wide, 2051);
    put("src/deep/a.c", "int a;\n/* a needle here */\n", -1);
    put("src/b.c", "/* needle */\nint b; /* NEEDLE */\n", -1);
    put("src/b.h", "/* Needle */\n", -1);
    put("greet/SKILL.md", "---\nname: greet\ndescription: Greets in style\n---\n\nSay Hallo.\n", -1);
    age("src/deep/a.c", 500);
    age("src/b.h", 300);
    age("src/b.c", 10);
}

void suite_claude_tools(void)
{
    test_path();
    test_perm();
    test_validate();
    mk_tree();
    CHECK(dir[0] != 0);
    if (!dir[0])
        return;
    test_files();
    test_search();
    test_bash();
    test_questions();
    test_ext();
    test_task();
    test_webfetch();
    {
        char cmd[600];
        strcpy(cmd, "rm -rf ");
        strcat(cmd, dir);
        CHECK_INT(system(cmd), 0);
    }
}
