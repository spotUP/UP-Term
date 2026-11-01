/* The Amarchy notice queue (amarchy/note_core.c; roadmap P4 R4.8, D4 corner, D5 timeout and stacking). The clock is
 * an argument, so nothing here waits. The window and the AMARCHY.NOTIFY port (note.c) are the rig's. */
#include <string.h>
#include "../amarchy/note_core.h"
#include "harness.h"

static void test_parse(void)
{
    char t[NOTE_TITLE], b[NOTE_BODY], big[300];
    long s;
    int i;
    CHECK_INT(note_parse("Saved|shot-1.iff|7", t, b, &s), NOTE_OK);
    CHECK_STR(t, "Saved");
    CHECK_STR(b, "shot-1.iff");
    CHECK_INT(s, 7);
    CHECK_INT(note_parse("Only title", t, b, &s), NOTE_OK);
    CHECK_STR(t, "Only title");
    CHECK_STR(b, "");
    CHECK_INT(s, -1);                                   /* absent: the sender leaves it to the default */
    CHECK_INT(note_parse("T|B|", t, b, &s), NOTE_OK);
    CHECK_INT(s, -1);                                   /* empty seconds = default too */
    CHECK_INT(note_parse("T||0", t, b, &s), NOTE_OK);
    CHECK_STR(b, "");
    CHECK_INT(s, 0);                                    /* 0 is a real value: until clicked */
    CHECK_INT(note_parse("a\\|b|c\\\\d|3", t, b, &s), NOTE_OK);
    CHECK_STR(t, "a|b");                                /* \| is a pipe, \\ a backslash */
    CHECK_STR(b, "c\\d");
    CHECK_INT(note_parse("x\\y|z", t, b, &s), NOTE_OK);
    CHECK_STR(t, "x\\y");                               /* a lone backslash stays */
    CHECK_INT(note_parse("", t, b, &s), NOTE_ERR_EMPTY);
    CHECK_INT(note_parse("|body|3", t, b, &s), NOTE_ERR_EMPTY);
    CHECK_INT(note_parse("T|B|abc", t, b, &s), NOTE_ERR_SECS);
    CHECK_INT(note_parse("T|B|-1", t, b, &s), NOTE_ERR_SECS);
    CHECK_INT(note_parse("T|B|3601", t, b, &s), NOTE_ERR_SECS);
    CHECK_INT(note_parse("T|B|3600", t, b, &s), NOTE_OK);
    CHECK_INT(note_parse("T|B|3|extra", t, b, &s), NOTE_ERR_SECS);
    for (i = 0; i < 299; i++)
        big[i] = 'x';
    big[299] = 0;
    CHECK_INT(note_parse(big, t, b, &s), NOTE_OK);
    CHECK_INT(strlen(t), NOTE_TITLE - 1);               /* cut, not refused, and still terminated */
}

static void test_expiry(void)
{
    note_q q;
    note_init(&q);
    note_post(&q, "a", "", -1, 100);                    /* default 5 s */
    note_post(&q, "b", "", 2, 100);
    CHECK_INT(note_tick(&q, 101), 0);
    CHECK_INT(note_tick(&q, 101), 0);
    CHECK_INT(note_tick(&q, 102), 1);                   /* b: exactly t0 + 2 */
    CHECK_STR(q.item[0].title, "a");
    CHECK_INT(note_tick(&q, 104), 0);                   /* a: one second short */
    CHECK_INT(q.n, 1);
    CHECK_INT(note_tick(&q, 105), 1);                   /* a: exactly t0 + NOTE_SECS_DEFAULT */
    CHECK_INT(q.n, 0);
    CHECK_INT(q.expired, 2);
}

static void test_sticky(void)
{
    note_q q;
    note_init(&q);
    note_post(&q, "s", "", 0, 10);
    note_post(&q, "t", "", 1, 10);
    CHECK_INT(note_tick(&q, 100000), 1);                /* only t */
    CHECK_INT(q.n, 1);
    CHECK_STR(q.item[0].title, "s");
    CHECK_INT(note_dismiss(&q, 0), 0);                  /* a click removes it */
    CHECK_INT(q.n, 0);
    CHECK_INT(note_dismiss(&q, 0), -1);
}

static void test_overflow(void)
{
    note_q q;
    int i;
    char t[2] = { 'a', 0 };
    note_init(&q);
    for (i = 0; i < NOTE_MAX_SHOWN + 2; i++) {
        t[0] = (char)('a' + i);
        note_post(&q, t, "", 0, 0);
    }
    CHECK_INT(q.n, NOTE_MAX_SHOWN);
    CHECK_STR(q.item[0].title, "c");                    /* a and b were the oldest and went */
    CHECK_STR(q.item[NOTE_MAX_SHOWN - 1].title, "e");
    CHECK_INT(q.dropped, 2);
    CHECK_INT(q.posted, NOTE_MAX_SHOWN + 2);            /* notes= counts every accepted notice */
    CHECK_INT(q.item[0].id, 3);                         /* ids are not reused */
    CHECK_INT(note_clear(&q), NOTE_MAX_SHOWN);
    CHECK_INT(q.n, 0);
    CHECK_INT(q.posted, NOTE_MAX_SHOWN + 2);
}

static void test_slots(void)
{
    note_box box = { 0, 11, 639, 245 };                 /* below a menu bar, above the Amarchy bar */
    int x, y, x1, y1;
    CHECK_INT(note_slot(NOTE_TOP_RIGHT, &box, 200, 40, 0, &x, &y), 0);
    CHECK_INT(x, 640 - NOTE_GAP - 200);
    CHECK_INT(y, 11 + NOTE_GAP);
    CHECK_INT(note_slot(NOTE_TOP_RIGHT, &box, 200, 40, 2, &x1, &y1), 0);
    CHECK_INT(x1, x);
    CHECK_INT(y1, 11 + NOTE_GAP + 2 * (40 + NOTE_GAP));   /* stacks downward from a top corner */
    CHECK_INT(note_slot(NOTE_BOTTOM_LEFT, &box, 200, 40, 0, &x, &y), 0);
    CHECK_INT(x, NOTE_GAP);
    CHECK_INT(y + 40 - 1, 245 - NOTE_GAP);
    CHECK_INT(note_slot(NOTE_BOTTOM_LEFT, &box, 200, 40, 1, &x1, &y1), 0);
    CHECK_INT(y1, y - 40 - NOTE_GAP);                     /* stacks upward from a bottom corner */
    CHECK_INT(note_slot(NOTE_TOP_LEFT, &box, 200, 40, 0, &x, &y), 0);
    CHECK_INT(x, NOTE_GAP);
    CHECK_INT(note_slot(NOTE_BOTTOM_RIGHT, &box, 200, 40, 0, &x, &y), 0);
    CHECK_INT(x, 640 - NOTE_GAP - 200);
    /* a box too small for the slot gives no position, and a box too narrow for the width too */
    CHECK_INT(note_slot(NOTE_TOP_RIGHT, &box, 200, 100, 2, &x, &y), -1);
    CHECK_INT(note_slot(NOTE_TOP_RIGHT, &box, 700, 40, 0, &x, &y), -1);
    /* 1..N: every shown slot of the default size fits a 200 line screen */
    box.y1 = 199;
    for (x1 = 0; x1 < NOTE_MAX_SHOWN; x1++)
        CHECK_INT(note_slot(NOTE_TOP_RIGHT, &box, 200, 40, x1, &x, &y), 0);
}

static void test_hit(void)
{
    note_box box = { 0, 11, 639, 245 };
    note_q q;
    int x, y;
    note_init(&q);
    CHECK_INT(note_hit(&q, NOTE_TOP_RIGHT, &box, 200, 40, 500, 20), -1);
    note_post(&q, "old", "", 0, 0);
    note_post(&q, "new", "", 0, 0);
    note_slot(NOTE_TOP_RIGHT, &box, 200, 40, 0, &x, &y);            /* the newest sits in slot 0 */
    CHECK_INT(note_hit(&q, NOTE_TOP_RIGHT, &box, 200, 40, x + 5, y + 5), 1);
    CHECK_INT(note_hit(&q, NOTE_TOP_RIGHT, &box, 200, 40, x + 199, y + 39), 1);
    CHECK_INT(note_hit(&q, NOTE_TOP_RIGHT, &box, 200, 40, x + 200, y), -1);
    CHECK_INT(note_hit(&q, NOTE_TOP_RIGHT, &box, 200, 40, x, y + 40), -1);   /* the gap between two */
    note_slot(NOTE_TOP_RIGHT, &box, 200, 40, 1, &x, &y);
    CHECK_INT(note_hit(&q, NOTE_TOP_RIGHT, &box, 200, 40, x, y), 0);
    CHECK_INT(note_dismiss(&q, note_hit(&q, NOTE_TOP_RIGHT, &box, 200, 40, x, y)), 0);
    CHECK_STR(q.item[0].title, "new");
}

static void test_verbs(void)
{
    note_q q;
    char out[160];
    note_init(&q);
    CHECK_INT(note_verb(&q, "COUNT", 10, out, sizeof out), 0);
    CHECK_STR(out, "0");
    CHECK_INT(note_verb(&q, "NOTIFY Saved|shot-1.iff|7", 10, out, sizeof out), 0);
    CHECK_INT(q.n, 1);
    CHECK_INT(q.item[0].expires, 17);                   /* the clock the caller gave */
    CHECK_INT(note_verb(&q, "notify  Second", 11, out, sizeof out), 0);   /* case and spaces do not matter */
    CHECK_INT(note_verb(&q, "COUNT", 12, out, sizeof out), 0);
    CHECK_STR(out, "2");
    CHECK_INT(note_verb(&q, "NOTES", 12, out, sizeof out), 0);
    CHECK_STR(out, "notes=2 shown=2 last=Second");
    CHECK_INT(note_verb(&q, "NOTIFY |no title", 12, out, sizeof out), 10);
    CHECK_INT(note_verb(&q, "NOTIFY T|B|x", 12, out, sizeof out), 10);
    CHECK_INT(note_verb(&q, "NOTIFY", 12, out, sizeof out), 10);
    CHECK_INT(note_verb(&q, "BOGUS", 12, out, sizeof out), 10);
    CHECK_INT(q.posted, 2);                             /* refused ones are not counted */
    CHECK_INT(note_verb(&q, "CLEAR", 13, out, sizeof out), 0);
    CHECK_INT(note_verb(&q, "NOTES", 13, out, sizeof out), 0);
    CHECK_STR(out, "notes=2 shown=0 last=Second");      /* the count and last outlive the window */
}

static void test_cli(void)
{
    char t[NOTE_TITLE], b[NOTE_BODY], f[NOTE_ARG_BODYFILE], line[300];
    long s;
    CHECK_INT(note_cli_parse("Done \"build ok\" SECONDS 3", t, b, &s, f), NOTE_OK);
    CHECK_STR(t, "Done");
    CHECK_STR(b, "build ok");
    CHECK_INT(s, 3);
    CHECK_INT(note_cli_parse("-t 4 Done \"build ok\"", t, b, &s, f), NOTE_OK);   /* the Unix form */
    CHECK_INT(s, 4);
    CHECK_STR(b, "build ok");
    CHECK_INT(note_cli_parse("-t0 X", t, b, &s, f), NOTE_OK);
    CHECK_INT(s, 0);
    CHECK_INT(note_cli_parse("X seconds=9 BODYFILE=T:safeoff.txt", t, b, &s, f), NOTE_OK);
    CHECK_INT(s, 9);
    CHECK_STR(f, "T:safeoff.txt");
    CHECK_INT(note_cli_parse("X -f T:a.txt", t, b, &s, f), NOTE_OK);
    CHECK_STR(f, "T:a.txt");
    CHECK_INT(note_cli_parse("\"SECONDS\" \"-t\"", t, b, &s, f), NOTE_OK);      /* quoted words are text */
    CHECK_STR(t, "SECONDS");
    CHECK_STR(b, "-t");
    CHECK_INT(s, -1);
    CHECK_INT(note_cli_parse("", t, b, &s, f), NOTE_ERR_EMPTY);
    CHECK_INT(note_cli_parse("X Y Z", t, b, &s, f), NOTE_ERR_EMPTY);
    CHECK_INT(note_cli_parse("X -t", t, b, &s, f), NOTE_ERR_EMPTY);
    CHECK_INT(note_cli_parse("X -t abc", t, b, &s, f), NOTE_ERR_SECS);
    CHECK_INT(note_cli_parse("X -t 3601", t, b, &s, f), NOTE_ERR_SECS);
    CHECK_INT(note_line("a|b", "c\\d", 3, line, sizeof line), 11);
    CHECK_STR(line, "a\\|b|c\\\\d|3");
    CHECK_INT(note_line("T", "", -1, line, sizeof line), 2);
    CHECK_STR(line, "T|");
    CHECK_INT(note_line("T", "longbody", 5, line, 6), -1);
    {                                                   /* the sender's line is the port's input */
        note_q q;
        char out[160];
        note_init(&q);
        note_line("a|b", "c\\d", 0, line, sizeof line);
        CHECK_INT(note_verb(&q, "NOTIFY a\\|b|c\\\\d|0", 1, out, sizeof out), 0);
        CHECK_STR(q.item[0].title, "a|b");
        CHECK_STR(q.item[0].body, "c\\d");
        CHECK_INT(q.item[0].sticky, 1);
    }
}

void suite_note_core(void)
{
    test_verbs();
    test_cli();
    test_parse();
    test_expiry();
    test_sticky();
    test_overflow();
    test_slots();
    test_hit();
}
