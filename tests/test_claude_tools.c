/* The Claude client's tools (claude/tools.c): AmigaOS path names, the
 * permission rules, input validation, and every tool against a temporary
 * tree on the host (claude/sys_posix.c). */
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "harness.h"
#include "../claude/path.h"
#include "../claude/tools.h"
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
    for (i = 0; i < T_TODO_WRITE; i++)
        CHECK(perm_must_ask(&p, i, 0));
    /* the todo list changes nothing but the screen: never asked, even in plan mode */
    CHECK(!perm_must_ask(&p, T_TODO_WRITE, 0));
    /* one answer allows all three read-only tools */
    perm_grant(&p, T_GREP);
    CHECK(!perm_must_ask(&p, T_READ_FILE, 0));
    CHECK(!perm_must_ask(&p, T_LIST_DIR, 0));
    CHECK(!perm_must_ask(&p, T_GREP, 0));
    CHECK(perm_must_ask(&p, T_WRITE_FILE, 0));
    CHECK(perm_must_ask(&p, T_EDIT_FILE, 0));
    CHECK(perm_must_ask(&p, T_RUN_COMMAND, 0));
    /* outside the start directory: always */
    CHECK(perm_must_ask(&p, T_READ_FILE, 1));
    /* a writing tool allowed for the session: that tool only */
    perm_grant(&p, T_WRITE_FILE);
    CHECK(!perm_must_ask(&p, T_WRITE_FILE, 0));
    CHECK(perm_must_ask(&p, T_EDIT_FILE, 0));
    CHECK(perm_must_ask(&p, T_WRITE_FILE, 1));
    /* accept edits: writes and edits inside the start directory run, a
     * command and anything outside still ask */
    memset(&p, 0, sizeof(p));
    p.mode = PERM_ACCEPT;
    CHECK(!perm_must_ask(&p, T_WRITE_FILE, 0));
    CHECK(!perm_must_ask(&p, T_EDIT_FILE, 0));
    CHECK(perm_must_ask(&p, T_EDIT_FILE, 1));
    CHECK(perm_must_ask(&p, T_RUN_COMMAND, 0));
    CHECK(perm_must_ask(&p, T_READ_FILE, 0));
    CHECK(!perm_refused(&p, T_RUN_COMMAND));
    /* plan: only the read-only tools (and the todo list) run */
    p.mode = PERM_PLAN;
    CHECK(perm_refused(&p, T_WRITE_FILE));
    CHECK(perm_refused(&p, T_EDIT_FILE));
    CHECK(perm_refused(&p, T_RUN_COMMAND));
    CHECK(!perm_refused(&p, T_GREP));
    CHECK(!perm_refused(&p, T_TODO_WRITE));
}

static int bad(int tool, const char *in)
{
    jv v;
    char err[200];
    if (json_parse(in, (long)strlen(in), &v))
        return -2;
    return tools_validate(tool, v, err, sizeof(err));
}

static void test_validate(void)
{
    jv v;
    CHECK_INT(json_parse(tools_json(), (long)strlen(tools_json()), &v), 0);
    CHECK_INT(json_count(v), T_COUNT);
    CHECK_INT(bad(T_READ_FILE, "{\"path\":\"S:x\"}"), 0);
    CHECK_INT(bad(T_READ_FILE, "{}"), -1);
    CHECK_INT(bad(T_READ_FILE, "[\"S:x\"]"), -1);
    CHECK_INT(bad(T_READ_FILE, "{\"path\":7}"), -1);
    CHECK_INT(bad(T_READ_FILE, "{\"path\":\"x\",\"extra\":1}"), -1);
    CHECK_INT(bad(T_GREP, "{\"pattern\":\"x\"}"), 0);
    CHECK_INT(bad(T_GREP, "{\"pattern\":\"x\",\"ignore_case\":\"yes\"}"), -1);
    CHECK_INT(bad(T_GREP, "{\"pattern\":\"x\",\"ignore_case\":true,\"path\":\"S\"}"), 0);
    CHECK_INT(bad(T_EDIT_FILE, "{\"path\":\"a\",\"old_string\":\"b\"}"), -1);
    CHECK_INT(bad(T_RUN_COMMAND, "{\"command\":\"Version\"}"), 0);
    CHECK_INT(bad(T_TODO_WRITE, "{\"todos\":[{\"content\":\"a\",\"status\":\"pending\"}]}"), 0);
    CHECK_INT(bad(T_TODO_WRITE, "{\"todos\":[]}"), 0);
    CHECK_INT(bad(T_TODO_WRITE, "{\"todos\":[{\"content\":\"a\",\"status\":\"maybe\"}]}"), -1);
    CHECK_INT(bad(T_TODO_WRITE, "{\"todos\":[{\"content\":\"a\"}]}"), -1);
    CHECK_INT(bad(T_TODO_WRITE, "{\"todos\":[\"a\"]}"), -1);
    CHECK_INT(bad(T_TODO_WRITE, "{\"todos\":\"a\"}"), -1);
    CHECK_INT(tools_id("todo_write"), T_TODO_WRITE);
    CHECK_INT(tools_id("read_file"), T_READ_FILE);
    CHECK_INT(tools_id("bash"), -1);
}

/* ---- the tools on a temporary tree ---- */

typedef struct asker {
    int asked, shown;
    int answer;
    int outside;
    char last[300];
    /* the preview: the file before and after (an edit's, a write's) */
    int previews;
    char before[200], after[200];
    /* the result hook */
    int results, last_error;
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
    (void)tool;
    a->shown++;
    strncpy(a->last, what, sizeof(a->last) - 1);
}

static int on_ask(void *u, const char *tool, const char *what, int outside)
{
    asker *a = (asker *)u;
    (void)tool;
    (void)what;
    a->asked++;
    a->outside = outside;
    return a->answer;
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

static void test_tree(void)
{
    sys_posix sp;
    cl_sys sys;
    cl_tools t;
    asker a;
    char text[4096], buf[256], *tmp;
    const char *base = getenv("TMPDIR");
    jw out;
    strcpy(dir, base && *base ? base : "/tmp");
    if (dir[strlen(dir) - 1] == '/')
        dir[strlen(dir) - 1] = 0;
    strcat(dir, "/claude_tools_XXXXXX");
    tmp = mkdtemp(dir);
    CHECK(tmp != 0);
    if (!tmp)
        return;
    sub("S");
    sub("sub");
    sub("sub/deep");
    put("S/Startup-Sequence", "SetPatch QUIET\nC:Version >NIL:\n", -1);
    put("notes.txt", "Gr\xfc\xdf" "e Welt\nline two\nline two\n", -1);
    put("bin.dat", "ab\0cd", 5);
    put("sub/deep/a.txt", "a needle here\n", -1);

    sys_posix_init(&sp, &sys);
    memset(&t, 0, sizeof(t));
    memset(&a, 0, sizeof(a));
    t.sys = &sys;
    CHECK_INT(sys.canon(sys.u, dir, t.root, sizeof(t.root)), 0);
    t.timeout_s = 60;
    t.u = &a;
    t.show = on_show;
    t.ask = on_ask;

    /* read: asks, the answer "always" covers the read-only tools */
    a.answer = ASK_SESSION;
    CHECK_INT(call(&t, "read_file", "{\"path\":\"S/Startup-Sequence\"}", text, sizeof(text)), 0);
    CHECK_STR(text, "SetPatch QUIET\nC:Version >NIL:\n");
    CHECK_INT(a.asked, 1);
    CHECK_INT(a.shown, 1);
    CHECK_INT(call(&t, "list_dir", "{\"path\":\".\"}", text, sizeof(text)), 0);
    CHECK_INT(a.asked, 1);
    CHECK_STR(text, "bin.dat  5\nnotes.txt  29\nS/\nsub/\n");
    CHECK_INT(call(&t, "read_file", "{\"path\":\"bin.dat\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "binary") != 0);
    CHECK_INT(call(&t, "read_file", "{\"path\":\"nothere\"}", text, sizeof(text)), 1);
    /* a Latin-1 file reaches Claude as UTF-8 */
    CHECK_INT(call(&t, "read_file", "{\"path\":\"notes.txt\"}", text, sizeof(text)), 0);
    CHECK(!strncmp(text, "Gr\xc3\xbc\xc3\x9f" "e Welt\n", 13));

    /* grep: substring, wildcard, ignoring case, a tree */
    CHECK_INT(call(&t, "grep", "{\"pattern\":\"needle\"}", text, sizeof(text)), 0);
    CHECK(strstr(text, "sub/deep/a.txt:1: a needle here\n") != 0);
    CHECK_INT(call(&t, "grep", "{\"pattern\":\"Set*QUIET\",\"path\":\"S\"}", text, sizeof(text)), 0);
    CHECK(strstr(text, "Startup-Sequence:1: SetPatch QUIET") != 0);
    CHECK_INT(call(&t, "grep", "{\"pattern\":\"SETPATCH\",\"ignore_case\":true}", text, sizeof(text)), 0);
    CHECK(strstr(text, ":1: SetPatch") != 0);
    CHECK_INT(call(&t, "grep", "{\"pattern\":\"SETPATCH\"}", text, sizeof(text)), 0);
    CHECK_STR(text, "(no matches)");
    CHECK_INT(a.asked, 1);

    /* write: asks every time; "no" writes nothing */
    a.answer = ASK_NO;
    CHECK_INT(call(&t, "write_file", "{\"path\":\"new.txt\",\"content\":\"hello\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "declined") != 0);
    CHECK_INT(get("new.txt", buf, sizeof(buf)), -1);
    CHECK_INT(a.asked, 2);
    a.answer = ASK_ONCE;
    CHECK_INT(call(&t, "write_file", "{\"path\":\"new.txt\",\"content\":\"hello \\u00fc\"}", text, sizeof(text)), 0);
    CHECK_INT(get("new.txt", buf, sizeof(buf)), 8);
    CHECK_STR(buf, "hello \xc3\xbc");
    CHECK_INT(a.asked, 3);

    /* edit: unique match; a Latin-1 file stays Latin-1 */
    CHECK_INT(call(&t, "edit_file", "{\"path\":\"notes.txt\",\"old_string\":\"Gr\\u00fc\\u00dfe Welt\","
                                   "\"new_string\":\"Gr\\u00fc\\u00dfe World\"}", text, sizeof(text)), 0);
    get("notes.txt", buf, sizeof(buf));
    CHECK_STR(buf, "Gr\xfc\xdf" "e World\nline two\nline two\n");
    CHECK_INT(call(&t, "edit_file", "{\"path\":\"notes.txt\",\"old_string\":\"line two\",\"new_string\":\"x\"}",
                   text, sizeof(text)), 1);
    CHECK(strstr(text, "more than once") != 0);
    CHECK_INT(call(&t, "edit_file", "{\"path\":\"notes.txt\",\"old_string\":\"absent\",\"new_string\":\"x\"}",
                   text, sizeof(text)), 1);
    CHECK(strstr(text, "not found") != 0);
    get("notes.txt", buf, sizeof(buf));
    CHECK_STR(buf, "Gr\xfc\xdf" "e World\nline two\nline two\n");

    /* outside the start directory: asked even though reads are allowed */
    a.asked = 0;
    a.answer = ASK_NO;
    CHECK_INT(call(&t, "read_file", "{\"path\":\"/outside.txt\"}", text, sizeof(text)), 1);
    CHECK_INT(a.asked, 1);
    CHECK_INT(a.outside, 1);

    /* a command: its output and return code */
    a.answer = ASK_ONCE;
    CHECK_INT(call(&t, "run_command", "{\"command\":\"echo hi; exit 5\"}", text, sizeof(text)), 0);
    CHECK_STR(text, "Return code 5.\nhi\n");

    /* control characters from the model never reach the screen as such */
    CHECK_INT(call(&t, "run_command", "{\"command\":\"echo \\u001b[2J\"}", text, sizeof(text)), 0);
    CHECK(strchr(a.last, 0x1b) == 0);

    /* invalid input: an error result, nothing run, nothing asked */
    a.asked = 0;
    CHECK_INT(call(&t, "write_file", "{\"path\":\"x\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "missing property: content") != 0);
    CHECK_INT(call(&t, "no_such_tool", "{}", text, sizeof(text)), 1);
    jw_init(&out);
    {
        const char cut[] = "{\"path\": \"RAM:x\", \"content\": \"cut";
        tools_run(&t, "toolu_bad", "write_file", 0, cut, (long)strlen(cut), &out);
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
    CHECK_INT(call(&t, "edit_file", "{\"path\":\"new.txt\",\"old_string\":\"hello\",\"new_string\":\"bye\"}",
                   text, sizeof(text)), 1);
    CHECK_INT(a.previews, 1);
    CHECK_STR(a.before, "hello \xc3\xbc");
    CHECK_STR(a.after, "bye \xc3\xbc");
    CHECK(strstr(text, "tell you what to do differently") != 0);
    CHECK_INT(t.stop, 1);
    CHECK_INT(get("new.txt", buf, sizeof(buf)), 8);
    CHECK_STR(buf, "hello \xc3\xbc");
    /* the rest of that round is not run, not even asked */
    CHECK_INT(call(&t, "read_file", "{\"path\":\"new.txt\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "not run") != 0);
    CHECK_INT(a.asked, 1);
    CHECK_INT(a.results, 2);
    CHECK_INT(a.last_error, 1);
    t.stop = 0;
    /* a write of a new file previews no "before" */
    a.answer = ASK_ONCE;
    CHECK_INT(call(&t, "write_file", "{\"path\":\"fresh.txt\",\"content\":\"one\\ntwo\\n\"}", text, sizeof(text)), 0);
    CHECK_INT(a.previews, 2);
    CHECK_STR(a.before, "");
    CHECK_STR(a.after, "one\ntwo\n");
    /* accept edits: the edit runs without a question */
    t.perm.mode = PERM_ACCEPT;
    a.asked = 0;
    CHECK_INT(call(&t, "edit_file", "{\"path\":\"fresh.txt\",\"old_string\":\"two\",\"new_string\":\"2\"}",
                   text, sizeof(text)), 0);
    CHECK_INT(a.asked, 0);
    CHECK_INT(get("fresh.txt", buf, sizeof(buf)), 6);
    /* plan mode: a command is refused with a result that says why, unasked */
    t.perm.mode = PERM_PLAN;
    CHECK_INT(call(&t, "run_command", "{\"command\":\"echo no\"}", text, sizeof(text)), 1);
    CHECK(strstr(text, "plan mode is on") != 0);
    CHECK_INT(a.asked, 0);
    CHECK_INT(call(&t, "todo_write", "{\"todos\":[{\"content\":\"x\",\"status\":\"pending\"}]}", text,
                   sizeof(text)), 0);
    CHECK(strstr(text, "Todos updated") != 0);
    t.perm.mode = PERM_DEFAULT;

    {
        char cmd[600];
        strcpy(cmd, "rm -rf ");
        strcat(cmd, dir);
        CHECK_INT(system(cmd), 0);
    }
}

void suite_claude_tools(void)
{
    test_path();
    test_perm();
    test_validate();
    test_tree();
}
