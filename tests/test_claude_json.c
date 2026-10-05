/* The Claude client's JSON reader/writer (claude/json.c) and server-sent
 * events (claude/sse.c). */
#include <stdlib.h>
#include "harness.h"
#include "../claude/json.h"
#include "../claude/sse.h"

static int valid(const char *s)
{
    jv v;
    return json_parse(s, (long)strlen(s), &v) == 0;
}

static void test_reader(void)
{
    const char doc[] = " {\"a\": [1, -2.5e3, true, false, null], \"b\": {\"c\": \"d\"},"
                       " \"esc\": \"q\\\"\\\\\\/\\b\\f\\n\\r\\t\\u0041\", \"n\": 64000} ";
    jv v, x, k, e;
    jit it;
    char buf[64];
    long l;
    CHECK_INT(json_parse(doc, (long)strlen(doc), &v), 0);
    CHECK_INT(json_type(v), J_OBJ);
    CHECK_INT(json_count(v), 4);
    CHECK(json_get(v, "a", &x));
    CHECK_INT(json_type(x), J_ARR);
    CHECK_INT(json_count(x), 5);
    json_iter(x, &it);
    CHECK(json_next(&it, 0, &e));
    CHECK_INT(json_long(e, 0), 1);
    CHECK(json_next(&it, 0, &e));
    CHECK_INT(json_type(e), J_NUM);
    CHECK(json_next(&it, 0, &e));
    CHECK_INT(json_type(e), J_TRUE);
    CHECK(json_get(v, "b", &x) && json_get(x, "c", &e) && json_streq(e, "d"));
    CHECK(!json_get(v, "missing", &x));
    CHECK(json_get(v, "esc", &x));
    l = json_str(x, buf, sizeof(buf));
    CHECK_INT(l, 10);
    CHECK(!memcmp(buf, "q\"\\/\b\f\n\r\tA", 10));
    CHECK(json_streq(x, "q\"\\/\b\f\n\r\tA"));
    CHECK(json_get(v, "n", &x));
    CHECK_INT(json_long(x, -1), 64000);
    json_iter(v, &it);
    CHECK(json_next(&it, &k, &e) && json_streq(k, "a"));
    /* a cut buffer: full length reported, terminated */
    CHECK(json_get(v, "esc", &x));
    CHECK_INT(json_str(x, buf, 4), 10);
    CHECK_INT((long)strlen(buf), 3);
}

static void test_unicode(void)
{
    jv v;
    char buf[32];
    const char s1[] = "\"Gr\\u00fc\\u00dfe \\u2014 \\ud83d\\ude00\"";
    const char s2[] = "\"lone \\ud83d x\"";
    const char s3[] = "\"raw \xc3\xbc ok\"";
    CHECK_INT(json_parse(s1, (long)strlen(s1), &v), 0);
    json_str(v, buf, sizeof(buf));
    CHECK_STR(buf, "Gr\xc3\xbc\xc3\x9f" "e \xe2\x80\x94 \xf0\x9f\x98\x80");
    CHECK_INT(json_parse(s2, (long)strlen(s2), &v), 0);
    json_str(v, buf, sizeof(buf));
    CHECK_STR(buf, "lone \xef\xbf\xbd x");
    CHECK_INT(json_parse(s3, (long)strlen(s3), &v), 0);
    json_str(v, buf, sizeof(buf));
    CHECK_STR(buf, "raw \xc3\xbc ok");
}

static void test_malformed(void)
{
    char deep[200];
    int i;
    CHECK(valid("{}"));
    CHECK(valid("[]"));
    CHECK(valid("0"));
    CHECK(valid("-0.5E+2"));
    CHECK(!valid(""));
    CHECK(!valid("{"));
    CHECK(!valid("{\"a\":}"));
    CHECK(!valid("{\"a\" 1}"));
    CHECK(!valid("{a:1}"));
    CHECK(!valid("[1,]"));
    CHECK(!valid("[1 2]"));
    CHECK(!valid("01"));
    CHECK(!valid("1."));
    CHECK(!valid("tru"));
    CHECK(!valid("\"abc"));
    CHECK(!valid("\"tab\there\""));
    CHECK(!valid("\"bad \\x\""));
    CHECK(!valid("\"bad \\u12g4\""));
    CHECK(!valid("{} {}"));
    CHECK(!valid("{\"a\":1}x"));
    /* nesting: JSON_DEPTH is fine, one deeper is refused, nothing recurses */
    for (i = 0; i < JSON_DEPTH; i++)
        deep[i] = '[';
    for (i = 0; i < JSON_DEPTH; i++)
        deep[JSON_DEPTH + i] = ']';
    deep[2 * JSON_DEPTH] = 0;
    CHECK(valid(deep));
    for (i = 0; i <= JSON_DEPTH; i++)
        deep[i] = '[';
    for (i = 0; i <= JSON_DEPTH; i++)
        deep[JSON_DEPTH + 1 + i] = ']';
    deep[2 * JSON_DEPTH + 2] = 0;
    CHECK(!valid(deep));
}

static void test_writer(void)
{
    jw w;
    jv v;
    char buf[64];
    const char in[] = "a\"b\\c\n\x01\x7f \xc3\xbc \xfc \xe2\x80\x94";
    jw_init(&w);
    jw_str(&w, in, (long)strlen(in));
    /* control escaped, valid UTF-8 kept, a lone Latin-1 byte converted */
    CHECK_STR(w.p, "\"a\\\"b\\\\c\\n\\u0001\x7f \xc3\xbc \xc3\xbc \xe2\x80\x94\"");
    CHECK_INT(json_parse(w.p, w.n, &v), 0);
    json_str(v, buf, sizeof(buf));
    CHECK_STR(buf, "a\"b\\c\n\x01\x7f \xc3\xbc \xc3\xbc \xe2\x80\x94");
    jw_reset(&w);
    jw_long(&w, -64000);
    CHECK_STR(w.p, "-64000");
    jw_free(&w);
}

/* ---- sse ---- */

typedef struct evlog {
    char s[2048];
    int n;
} evlog;

static void on_event(void *u, const char *ev, const char *data, long n)
{
    evlog *l = (evlog *)u;
    char line[512];
    long k;
    l->n++;
    if (n > 400)
        n = 400;
    k = (long)strlen(ev);
    memcpy(line, ev, (size_t)k);
    line[k++] = '=';
    memcpy(line + k, data, (size_t)n);
    line[k + n] = '|';
    line[k + n + 1] = 0;
    if (strlen(l->s) + strlen(line) < sizeof(l->s))
        strcat(l->s, line);
}

static const char sse_in[] =
    ": a comment\n"
    "event: content_block_delta\n"
    "data: {\"x\":1}\n"
    "\n"
    "event: two\r\n"
    "data: line1\r\n"
    "data:line2\r\n"
    "id: 7\r\n"
    "\r\n"
    "data: no event name\r"
    "\r"
    "event: empty-no-data\n"
    "\n";
static const char sse_want[] = "content_block_delta={\"x\":1}|two=line1\nline2|message=no event name|";

static void test_sse(void)
{
    long n = (long)strlen(sse_in), at;
    int bad = 0;
    for (at = -1; at <= n; at++) {
        sse s;
        evlog l;
        l.s[0] = 0;
        l.n = 0;
        sse_init(&s, on_event, &l);
        if (at < 0) {
            long i;
            for (i = 0; i < n; i++)
                sse_feed(&s, sse_in + i, 1);
        } else {
            sse_feed(&s, sse_in, at);
            sse_feed(&s, sse_in + at, n - at);
        }
        if (strcmp(l.s, sse_want) || l.n != 3)
            bad++;
        sse_free(&s);
    }
    CHECK_INT(bad, 0);
}

void suite_claude_json(void)
{
    test_reader();
    test_unicode();
    test_malformed();
    test_writer();
    test_sse();
}
