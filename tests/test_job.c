/* The Amarchy job view core: lines, end marker, footer, scroll (amarchy/job_core.c; plan 2026-10-27-amarchy-job-view.md). */
#include <stdio.h>
#include <string.h>
#include "../amarchy/job_core.h"
#include "harness.h"

static jc J;
static void feed(const char *s) { jc_feed(&J, s, (long)strlen(s)); }
static const char *foot(void) { static char b[JOB_FOOT_MAX]; jc_footer(&J, b, (int)sizeof b); return b; }
static const char *hints(void) { static char b[JOB_FOOT_MAX]; jc_hints(&J, b, (int)sizeof b); return b; }

/* "L<i><post>" without sprintf (c89 build, deprecated warnings are errors). */
static void mkl(char *b, int i, const char *post)
{
    char d[12];
    int n = 0;
    do { d[n++] = (char)('0' + i % 10); i /= 10; } while (i);
    *b++ = 'L';
    while (n) *b++ = d[--n];
    strcpy(b, post);
}

static void test_job_lines(void)
{
    jc_init(&J);
    feed("one\ntw");
    CHECK_INT(jc_count(&J), 1);
    feed("o\nthree");                       /* partial line carried across chunks */
    CHECK_INT(jc_count(&J), 2);
    CHECK_STR(jc_line(&J, 1), "two");
    feed("\n");
    CHECK_STR(jc_line(&J, 2), "three");
    jc_init(&J);
    feed("a\rb\r\nc\n\nd\n\r\n");           /* bare CR ends a line, CRLF is one end, empty lines kept */
    CHECK_INT(jc_count(&J), 6);
    CHECK_STR(jc_line(&J, 0), "a");
    CHECK_STR(jc_line(&J, 1), "b");
    CHECK_STR(jc_line(&J, 2), "c");
    CHECK_STR(jc_line(&J, 3), "");
    CHECK_STR(jc_line(&J, 4), "d");
    CHECK_STR(jc_line(&J, 5), "");
    jc_init(&J);
    jc_feed(&J, "x\r", 2);                  /* CRLF split across chunks is still one end */
    jc_feed(&J, "\ny\n", 3);
    CHECK_INT(jc_count(&J), 2);
    jc_init(&J);
    feed("a\tb\n\tc\nabcdefgh\ti\n");       /* tab expands to the next multiple of 8 */
    CHECK_STR(jc_line(&J, 0), "a       b");
    CHECK_STR(jc_line(&J, 1), "        c");
    CHECK_STR(jc_line(&J, 2), "abcdefgh        i");
}

static void test_job_ring(void)
{
    char b[16];
    int i;
    jc_init(&J);
    for (i = 0; i < JOB_MAX_LINES + 5; i++) { mkl(b, i, "\n"); feed(b); }
    CHECK_INT(jc_count(&J), JOB_MAX_LINES);
    CHECK_INT(jc_dropped(&J), 5);
    CHECK_STR(jc_line(&J, 0), "L5");
    mkl(b, JOB_MAX_LINES + 4, "");
    CHECK_STR(jc_line(&J, JOB_MAX_LINES - 1), b);
    CHECK_STR(jc_line(&J, JOB_MAX_LINES), "");
    jc_init(&J);
    for (i = 0; i < JOB_LINE_MAX + 30; i++) feed("x");
    feed("\n");
    CHECK_INT(strlen(jc_line(&J, 0)), JOB_LINE_MAX);   /* long line cut */
}

static void test_job_marker(void)
{
    jc_init(&J);
    feed("work\n@JOBRC 0\n");
    CHECK_INT(jc_count(&J), 1);             /* the marker is never a line */
    CHECK(J.ended);
    CHECK_INT(J.rc, 0);
    CHECK_INT(J.state, JC_DONE);
    jc_init(&J);
    feed("a\n@JOB");
    CHECK(!J.ended);
    feed("RC 15");                          /* split across chunks */
    CHECK(!J.ended);
    feed("\n");
    CHECK(J.ended);
    CHECK_INT(J.rc, 15);
    CHECK_INT(jc_count(&J), 1);
    jc_init(&J);
    feed("@JOBRC 5\r\n");
    CHECK_INT(J.rc, 5);
    jc_init(&J);
    feed("@JOBRCX\n");                     /* not the marker */
    CHECK(!J.ended);
    CHECK_INT(jc_count(&J), 1);
}

/* Output that ends without a newline puts the marker at the end of the last text line. */
static void test_job_marker_after_text(void)
{
    jc_init(&J);
    feed("abc@JOBRC 3\n");
    CHECK_INT(jc_count(&J), 1);
    CHECK_STR(jc_line(&J, 0), "abc");
    CHECK(J.ended);
    CHECK_INT(J.rc, 3);
    CHECK_INT(J.state, JC_FAILED);
    jc_init(&J);
    feed("abc@JOB");
    feed("RC 3\n");
    CHECK_INT(jc_count(&J), 1);
    CHECK_STR(jc_line(&J, 0), "abc");
    CHECK(J.ended);
    CHECK_INT(J.rc, 3);
    CHECK_INT(J.state, JC_FAILED);
    jc_init(&J);
    feed("x @JOBRC 2 y\n");               /* text after the number: not the marker */
    CHECK(!J.ended);
    CHECK_INT(jc_count(&J), 1);
    CHECK_STR(jc_line(&J, 0), "x @JOBRC 2 y");
}

static void test_job_footer(void)
{
    jc_init(&J);
    CHECK_STR(foot(), "Running...");
    CHECK_INT(J.state, JC_RUNNING);
    CHECK_STR(hints(), "Escape: Cancel");
    feed("@JOBRC 0\n");
    CHECK_STR(foot(), "Done");
    CHECK_STR(hints(), "Enter: Accept  Escape: Back");
    jc_init(&J); feed("@JOBRC 5\n");
    CHECK_INT(J.state, JC_DONE);
    CHECK_STR(foot(), "Nothing to do");
    jc_init(&J); feed("@JOBRC 15\n");
    CHECK_INT(J.state, JC_FAILED);
    CHECK_STR(foot(), "Needs answers: run it from a terminal");
    jc_init(&J); feed("@JOBRC 20\n");
    CHECK_STR(foot(), "Failed");
    jc_init(&J); feed("@JOBRC 12\n");
    CHECK_STR(foot(), "Package not found");
    jc_init(&J); feed("@JOBRC 13\n");
    CHECK_STR(foot(), "Network error");
    jc_init(&J); feed("@JOBRC 14\n");
    CHECK_STR(foot(), "Package script failed");
    jc_init(&J); feed("@JOBRC 11\n");
    CHECK_STR(foot(), "Usage error");
    jc_init(&J); feed("@JOBRC 7\n");
    CHECK_INT(J.state, JC_FAILED);
    CHECK_STR(foot(), "Failed (code 7)");
    jc_init(&J);
    jc_cancel(&J);
    CHECK_INT(J.state, JC_CANCELLED);
    CHECK_STR(foot(), "Cancelled");
    feed("@JOBRC 0\n");                    /* marker after cancel keeps cancelled, the code is recorded */
    CHECK_INT(J.state, JC_CANCELLED);
    CHECK(J.ended);
    jc_init(&J); feed("@JOBRC 0\n");
    jc_cancel(&J);                          /* cancel after the end is ignored */
    CHECK_INT(J.state, JC_DONE);
    jc_init(&J); feed("@JOBRC 20\n");
    jc_cancel(&J);
    CHECK_INT(J.state, JC_FAILED);
}

static char pub_name[40], pub_word[JOB_FOOT_MAX];
static int pub_calls;
static void fake_pub(const char *name, const char *word)
{
    strcpy(pub_name, name);
    strcpy(pub_word, word);
    pub_calls++;
}

static void test_job_word_sentinel(void)
{
    static const char *const marks[] = { "@JOBRC 0\n", "@JOBRC 5\n", "@JOBRC 12\n", "@JOBRC 13\n", "@JOBRC 20\n", "@JOBRC 77\n" };
    int i;
    for (i = 0; i < 6; i++) {
        jc_init(&J);
        feed(marks[i]);
        pub_calls = 0;
        jc_publish_word(&J, fake_pub);
        CHECK_INT(pub_calls, 1);
        CHECK_STR(pub_name, "AMARCHY_JOB_WORD");
        CHECK_STR(pub_word, foot());
    }
    jc_init(&J);
    feed("@JOBRC 12\n");
    jc_publish_word(&J, fake_pub);
    CHECK_STR(pub_word, "Package not found");
    jc_init(&J);
    feed("@JOBRC 13\n");
    jc_publish_word(&J, fake_pub);
    CHECK_STR(pub_word, "Network error");
    jc_init(&J);
    feed("@JOBRC 77\n");
    jc_publish_word(&J, fake_pub);
    CHECK_STR(pub_word, "Failed (code 77)");
}

static void test_job_gone(void)
{
    jc_init(&J); feed("working\n");
    jc_gone(&J);                            /* process gone, no marker: the job ends as failed */
    CHECK(J.ended);
    CHECK_INT(J.state, JC_FAILED);
    CHECK_INT(J.rc, -1);
    jc_init(&J); feed("working\n");
    jc_cancel(&J);
    jc_gone(&J);                            /* cancel then gone: cancelled, and ended */
    CHECK(J.ended);
    CHECK_INT(J.state, JC_CANCELLED);
    CHECK_STR(foot(), "Cancelled");
    jc_init(&J); feed("@JOBRC 0\n");
    jc_gone(&J);                            /* gone after the marker changes nothing */
    CHECK_INT(J.state, JC_DONE);
    CHECK_INT(J.rc, 0);
    jc_init(&J); feed("@JOBRC 7\n");
    jc_gone(&J);
    CHECK_INT(J.rc, 7);
}

static void test_job_progress(void)
{
    char h[JOB_FOOT_MAX * 2];
    jc_init(&J);
    jc_header(&J, h, (int)sizeof h);
    CHECK_STR(h, "");
    feed("[PROGRESS 1/7] fetch Gamma\nnoise\n[PROGRESS 3/7] install Gamma\n[PROGRESS x/7] bad\n");
    CHECK_INT(J.prog_n, 3);
    CHECK_INT(J.prog_m, 7);
    jc_header(&J, h, (int)sizeof h);
    CHECK_STR(h, "3 of 7 install Gamma");
    CHECK_INT(jc_count(&J), 4);             /* the progress lines stay visible */
    CHECK_STR(jc_line(&J, 2), "[PROGRESS 3/7] install Gamma");
}

static void test_job_scroll(void)
{
    char b[16];
    int i;
    jc_init(&J);
    for (i = 0; i < 20; i++) { mkl(b, i, "\n"); feed(b); }
    CHECK_INT(jc_top(&J, 5), 15);           /* follows the tail */
    feed("L20\n");
    CHECK_INT(jc_top(&J, 5), 16);
    jc_scroll(&J, -3, 5);
    CHECK_INT(jc_top(&J, 5), 13);
    CHECK(!J.follow);
    feed("L21\nL22\n");                     /* scrolled view stays put */
    CHECK_INT(jc_top(&J, 5), 13);
    jc_scroll(&J, -100, 5);
    CHECK_INT(jc_top(&J, 5), 0);
    jc_scroll(&J, 5, 5);
    CHECK_INT(jc_top(&J, 5), 5);
    jc_scroll(&J, 100, 5);                  /* back at the end: following again */
    CHECK(J.follow);
    feed("L23\n");
    CHECK_INT(jc_top(&J, 5), jc_count(&J) - 5);
    jc_init(&J);
    feed("a\nb\n");
    CHECK_INT(jc_top(&J, 10), 0);           /* fewer lines than rows */
    jc_scroll(&J, -1, 10);
    CHECK(J.follow);
}

static void test_job_keys(void)
{
    int d;
    jc_init(&J);
    CHECK_INT(jc_tone(&J), JT_DIM);
    CHECK_INT(jc_key_act(&J, JKEY_ESC, 10, &d), JA_CANCEL);
    CHECK_INT(jc_key_act(&J, JKEY_ENTER, 10, &d), JA_NONE);
    CHECK_INT(jc_key_act(&J, JKEY_UP, 10, &d), JA_SCROLL);
    CHECK_INT(d, -1);
    CHECK_INT(jc_key_act(&J, JKEY_PGDN, 10, &d), JA_SCROLL);
    CHECK_INT(d, 9);
    CHECK_INT(jc_key_act(&J, JKEY_PGUP, 1, &d), JA_SCROLL);
    CHECK_INT(d, -1);
    CHECK_INT(jc_key_act(&J, JKEY_OTHER, 10, &d), JA_NONE);
    feed("x\n@JOBRC 0\n");
    CHECK_INT(jc_tone(&J), JT_OK);
    CHECK_INT(jc_key_act(&J, JKEY_ESC, 10, &d), JA_CLOSE);
    CHECK_INT(jc_key_act(&J, JKEY_ENTER, 10, &d), JA_CLOSE);
    jc_init(&J);
    feed("@JOBRC 20\n");
    CHECK_INT(jc_tone(&J), JT_FAIL);
    jc_init(&J);
    feed("@JOBRC 5\n");
    CHECK_INT(jc_tone(&J), JT_OK);          /* nothing to do counts as Done */
    jc_init(&J);
    jc_cancel(&J);
    CHECK_INT(jc_tone(&J), JT_FAIL);
    CHECK_INT(jc_key_act(&J, JKEY_ESC, 10, &d), JA_CLOSE);
}

static void test_job_first_new(void)
{
    int i;
    jc_init(&J);
    feed("a\nb\nc\n");
    CHECK_INT((int)jc_first_new(&J, 0), 0);
    CHECK_INT((int)jc_first_new(&J, 2), 2);
    CHECK_INT((int)jc_first_new(&J, J.total), 3);
    for (i = 0; i < JOB_MAX_LINES + 10; i++)
        feed("x\n");
    CHECK_INT((int)jc_first_new(&J, 0), (int)jc_dropped(&J));     /* seen lines the ring dropped are skipped */
    CHECK_INT((int)jc_first_new(&J, J.total - 1), (int)(J.total - 1));
}

void suite_job(void)
{
    test_job_keys();
    test_job_lines();
    test_job_ring();
    test_job_marker();
    test_job_marker_after_text();
    test_job_footer();
    test_job_word_sentinel();
    test_job_gone();
    test_job_progress();
    test_job_scroll();
    test_job_first_new();
}
