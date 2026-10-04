/* The Claude client's stream state machine (claude/stream.c) on the
 * recorded streams in tests/claude/, and the conversation (claude/conv.c):
 * history, request body, rollback, cost. */
#include <stdlib.h>
#include "harness.h"
#include "claude_load.h"
#include "../claude/sse.h"
#include "../claude/stream.h"
#include "../claude/conv.h"

typedef struct shown {
    char text[1024];
    long n;
    int blocks[16];
    int nblocks;
} shown;

static void ui_text(void *u, const char *s, long n)
{
    shown *w = (shown *)u;
    if (w->n + n < (long)sizeof(w->text)) {
        memcpy(w->text + w->n, s, (size_t)n);
        w->n += n;
        w->text[w->n] = 0;
    }
}

static void ui_block(void *u, int type, const char *name)
{
    shown *w = (shown *)u;
    (void)name;
    if (w->nblocks < 16)
        w->blocks[w->nblocks++] = type;
}

static void to_stream(void *u, const char *ev, const char *data, long n)
{
    stream_event((cl_stream *)u, ev, data, n);
}

/* replay a recording; step 0: whole, else that many bytes at a time */
static int replay(const char *name, cl_stream *s, shown *w, long step)
{
    long n, i;
    char *b = claude_load(name, &n);
    sse p;
    cl_stream_ui ui;
    if (!b)
        return -1;
    memset(w, 0, sizeof(*w));
    memset(&ui, 0, sizeof(ui));
    ui.u = w;
    ui.text = ui_text;
    ui.block = ui_block;
    stream_init(s, &ui);
    sse_init(&p, to_stream, s);
    if (!step)
        step = n;
    for (i = 0; i < n; i += step)
        sse_feed(&p, b + i, n - i < step ? n - i : step);
    sse_free(&p);
    free(b);
    return 0;
}

static const char text_want[] =
    "Hello from the Amiga!\n\n```c\nint x = \"q\";\n```\nGr\xc3\xbc\xc3\x9f" "e \xe2\x80\x94 ok \xf0\x9f\x98\x80";
static const char text_content[] =
    "[{\"type\":\"thinking\",\"thinking\":\"\",\"signature\":\"EqQBCkgIBhABGAIiQL+sig/part1==+part2\"},"
    "{\"type\":\"text\",\"text\":\"Hello from the Amiga!\\n\\n```c\\nint x = \\\"q\\\";\\n```\\n"
    "Gr\xc3\xbc\xc3\x9f" "e \xe2\x80\x94 ok \xf0\x9f\x98\x80\"}]";

static void test_text(void)
{
    cl_stream s;
    shown w;
    jw c;
    long step;
    int bad = 0;
    /* any split of the bytes gives the same turn */
    for (step = 0; step <= 64; step = step ? step * 2 : 1) {
        if (replay("text.sse", &s, &w, step))
            return;
        jw_init(&c);
        stream_content(&s, &c);
        if (s.state != ST_DONE || strcmp(w.text, text_want) || !c.p || strcmp(c.p, text_content))
            bad++;
        jw_free(&c);
        stream_free(&s);
    }
    CHECK_INT(bad, 0);
    replay("text.sse", &s, &w, 0);
    CHECK_INT(s.state, ST_DONE);
    CHECK_STR(s.stop_reason, "end_turn");
    CHECK_STR(s.model, "claude-opus-5-5");
    CHECK_INT(s.in_tok, 25);
    CHECK_INT(s.cache_w, 1200);
    CHECK_INT(s.out_tok, 42);
    CHECK_INT(w.nblocks, 2);
    CHECK_INT(w.blocks[0], B_THINKING);
    CHECK_INT(w.blocks[1], B_TEXT);
    CHECK_STR(w.text, text_want);
    jw_init(&c);
    CHECK_INT(stream_content(&s, &c), 0);
    CHECK_STR(c.p, text_content);
    jw_free(&c);
    CHECK_INT(stream_tools(&s), 0);
    stream_free(&s);
}

static void test_tool_use(void)
{
    cl_stream s;
    shown w;
    sblock *t;
    jv in, p;
    jw c;
    char path[64];
    if (replay("tool_use.sse", &s, &w, 3))
        return;
    CHECK_STR(s.stop_reason, "tool_use");
    CHECK_INT(stream_tools(&s), 2);
    t = stream_tool(&s, 0);
    CHECK(t != 0);
    if (!t)
        return;
    CHECK_STR(t->id, "toolu_01ReadStartup");
    CHECK_STR(t->name, "read_file");
    CHECK(t->input_ok);
    /* the input assembled from the fragments is the object, parsed */
    CHECK_INT(json_parse(t->a.p, t->a.n, &in), 0);
    CHECK(json_get(in, "path", &p));
    json_str(p, path, sizeof(path));
    CHECK_STR(path, "S/Startup-Sequence");
    CHECK_INT(json_count(in), 1);
    t = stream_tool(&s, 1);
    CHECK(t && !strcmp(t->name, "list_dir") && t->input_ok);
    jw_init(&c);
    stream_content(&s, &c);
    CHECK(strstr(c.p, "{\"type\":\"tool_use\",\"id\":\"toolu_01ReadStartup\",\"name\":\"read_file\","
                      "\"input\":{\"path\": \"S/Startup-Sequence\"}}") != 0);
    CHECK(strstr(c.p, "\"signature\":\"ErUBCkYIBhgCIkDtoolsig==\"") != 0);
    jw_free(&c);
    stream_free(&s);
}

static void test_stops(void)
{
    cl_stream s;
    shown w;
    sblock *t;
    jw c;
    if (!replay("refusal.sse", &s, &w, 0)) {
        CHECK_STR(s.stop_reason, "refusal");
        CHECK_STR(s.stop_category, "cyber");
        CHECK_INT(s.state, ST_DONE);
        stream_free(&s);
    }
    if (!replay("max_tokens.sse", &s, &w, 0)) {
        CHECK_STR(s.stop_reason, "max_tokens");
        t = stream_tool(&s, 0);
        CHECK(t != 0);
        /* the cut input is not an object: never run */
        CHECK(t && !t->input_ok);
        stream_free(&s);
    }
    if (!replay("overloaded.sse", &s, &w, 0)) {
        CHECK_INT(s.state, ST_ERROR);
        CHECK_STR(s.err_type, "overloaded_error");
        CHECK_STR(s.err_msg, "Overloaded");
        stream_free(&s);
    }
    if (!replay("fallback.sse", &s, &w, 0)) {
        jw_init(&c);
        stream_content(&s, &c);
        /* before the fallback block only text stays; after it, all */
        CHECK_STR(c.p, "[{\"type\":\"text\",\"text\":\"Partial \"},"
                       "{\"type\":\"fallback\",\"from\":{\"model\":\"claude-opus-5-5\"},\"to\":{\"model\":\"claude-opus-5\"}},"
                       "{\"type\":\"text\",\"text\":\"answer.\"}]");
        CHECK_INT(stream_tools(&s), 0);
        CHECK_STR(w.text, "Partial answer.");
        jw_free(&c);
        stream_free(&s);
    }
    /* order broken: a delta for a block that never started */
    {
        const char d[] = "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"x\"}}";
        const char m[] = "{\"type\":\"message_start\",\"message\":{\"id\":\"m\",\"model\":\"x\"}}";
        stream_init(&s, 0);
        CHECK_INT(stream_event(&s, "message_start", m, (long)strlen(m)), 0);
        CHECK_INT(stream_event(&s, "content_block_delta", d, (long)strlen(d)), -1);
        CHECK_INT(s.state, ST_BAD);
        stream_free(&s);
        stream_init(&s, 0);
        CHECK_INT(stream_event(&s, "message_start", "{not json", 9), -1);
        stream_free(&s);
    }
}

static const char body_want[] =
    "{\"model\":\"claude-opus-5-5\",\"max_tokens\":64000,\"stream\":true,"
    "\"output_config\":{\"effort\":\"medium\"},\"fallbacks\":\"default\","
    "\"cache_control\":{\"type\":\"ephemeral\"},"
    "\"system\":[{\"type\":\"text\",\"text\":\"You are on an Amiga.\",\"cache_control\":{\"type\":\"ephemeral\"}}],"
    "\"tools\":[{\"name\":\"t\"}],\"tool_choice\":{\"type\":\"auto\"},"
    "\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"hi\"}]},"
    "{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"hello\"}]}]}";

static int add(cl_conv *c, int user, const char *j)
{
    return conv_add(c, user, j, (long)strlen(j));
}

static void test_conv(void)
{
    cl_conv c;
    cl_opts o;
    cl_mark m;
    jw b;
    jv v;
    char d[32];
    conv_init(&c);
    CHECK_INT(conv_add_user_text(&c, "hi", 2), 0);
    CHECK_INT(add(&c, 0, "[{\"type\":\"text\",\"text\":\"hello\"}]"), 0);
    memset(&o, 0, sizeof(o));
    o.model = "claude-opus-5-5";
    o.effort = "medium";
    o.max_tokens = 64000;
    o.system = "You are on an Amiga.";
    o.tools = "[{\"name\":\"t\"}]";
    jw_init(&b);
    CHECK_INT(conv_body(&c, &o, &b), 0);
    CHECK_STR(b.p, body_want);
    CHECK_INT(json_parse(b.p, b.n, &v), 0);
    /* no fallbacks or effort where the model refuses them */
    jw_reset(&b);
    o.model = "claude-haiku-4-5";
    conv_body(&c, &o, &b);
    CHECK(strstr(b.p, "fallbacks") == 0);
    CHECK(strstr(b.p, "output_config") == 0);
    CHECK_STR(conv_beta("claude-haiku-4-5"), "");
    CHECK_STR(conv_beta("claude-opus-5-5"), "server-side-fallback-2026-07-01");

    /* a prompt then a rollback: the history is as before */
    m = conv_mark(&c);
    conv_add_user_text(&c, "second", 6);
    CHECK_INT(c.n, 3);
    conv_rollback(&c, m);
    CHECK_INT(c.n, 2);

    /* a trailing tool_result message takes the next prompt as one more block,
     * and a rollback restores it byte for byte */
    add(&c, 0, "[{\"type\":\"tool_use\",\"id\":\"t1\",\"name\":\"x\",\"input\":{}}]");
    add(&c, 1, "[{\"type\":\"tool_result\",\"tool_use_id\":\"t1\",\"content\":\"ok\"}]");
    m = conv_mark(&c);
    conv_add_user_text(&c, "go on", 5);
    CHECK_INT(c.n, 4);
    CHECK_STR(c.m[3].json, "[{\"type\":\"tool_result\",\"tool_use_id\":\"t1\",\"content\":\"ok\"},"
                           "{\"type\":\"text\",\"text\":\"go on\"}]");
    conv_rollback(&c, m);
    CHECK_STR(c.m[3].json, "[{\"type\":\"tool_result\",\"tool_use_id\":\"t1\",\"content\":\"ok\"}]");
    CHECK_INT(c.m[3].n, (long)strlen(c.m[3].json));
    /* earlier messages are never touched */
    CHECK_STR(c.m[1].json, "[{\"type\":\"text\",\"text\":\"hello\"}]");
    jw_reset(&b);
    CHECK_INT(conv_messages(&c, &b), 0);
    CHECK_INT(json_parse(b.p, b.n, &v), 0);
    CHECK_INT(json_count(v), 4);
    jw_free(&b);
    conv_free(&c);

    /* cost: Claude Opus 5.5 at $4 / $20 / $0.20 read / $5 write per million */
    conv_init(&c);
    conv_usage(&c, "claude-opus-5-5", 1000000, 100000, 200000, 500000);
    /* 4.00 + 2.00 + 1.00 + 0.10 = 7.10 */
    CHECK_INT((long)c.cost_micro, 7100000L);
    conv_dollars(c.cost_micro, d, sizeof(d));
    CHECK_STR(d, "$7.1000");
    conv_usage(&c, "claude-opus-5-5", 25, 42, 1200, 0);
    /* 25*4 + 42*20 + 1200*5 = 100 + 840 + 6000 = 6940 micro-dollars */
    CHECK_INT((long)c.cost_micro, 7106940L);
    conv_dollars(c.cost_micro, d, sizeof(d));
    CHECK_STR(d, "$7.1069");
    conv_usage(&c, "some-other-model", 10, 10, 0, 0);
    CHECK_INT(c.unpriced, 1);
    CHECK_INT(c.requests, 3);
    CHECK_INT(c.in_tok, 1000035);
    conv_dollars(49, d, sizeof(d));
    CHECK_STR(d, "$0.0000");
    conv_dollars(999950, d, sizeof(d));
    CHECK_STR(d, "$1.0000");
    conv_free(&c);
}

void suite_claude_stream(void)
{
    test_text();
    test_tool_use();
    test_stops();
    test_conv();
}
