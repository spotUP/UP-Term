/* The Amarchy demo tour core: ten scenes, the total time, advance and wrap, the verb parse, STATUS text, STOP. */
#include "harness.h"
#include "../amarchy/demo_core.h"

static void test_table(void)
{
    int i;
    CHECK_INT(DEMO_SCENES, 10);
    for (i = 0; i < DEMO_SCENES; i++) {
        CHECK(demo_scene_name(i)[0] != 0);
        CHECK(demo_scene_secs(i) >= 10 && demo_scene_secs(i) <= 15);
    }
    CHECK(demo_total_secs() >= 100 && demo_total_secs() <= 150);
    CHECK_STR(demo_scene_name(DEMO_SCENES), "");
}

static void test_advance_wrap(void)
{
    demo_clock c;
    int t, first, s, i, total = demo_total_secs();
    demo_start(&c, 1);
    s = demo_step(&c, &t, &first);
    CHECK_INT(s, 0); CHECK_INT(t, 0); CHECK_INT(first, 1);
    s = demo_step(&c, &t, &first);
    CHECK_INT(s, 0); CHECK_INT(t, 1); CHECK_INT(first, 0);
    for (i = 2; i < demo_scene_secs(0); i++)
        demo_step(&c, &t, &first);
    s = demo_step(&c, &t, &first);          /* the call after scene 1 had its seconds */
    CHECK_INT(s, 1); CHECK_INT(t, 0); CHECK_INT(first, 1);
    /* finish the pass: every scene runs once, the loop counter moves on the wrap */
    demo_start(&c, 1);
    for (i = 0; i < total; i++)
        demo_step(&c, &t, &first);
    CHECK_INT(c.loops, 0);
    s = demo_step(&c, &t, &first);
    CHECK_INT(s, 0); CHECK_INT(first, 1); CHECK_INT(c.loops, 1);
    /* start at scene n */
    demo_start(&c, 7);
    CHECK_INT(demo_step(&c, &t, &first), 6);
    /* an early end */
    demo_next(&c);
    CHECK_INT(demo_step(&c, &t, &first), 7);
    CHECK_INT(first, 1);
}

static void test_parse_status_stop(void)
{
    demo_clock c;
    int n, t, first, k;
    char out[64];
    CHECK_INT(demo_parse("", &n), DV_START); CHECK_INT(n, 1);
    CHECK_INT(demo_parse("  4", &n), DV_START); CHECK_INT(n, 4);
    CHECK_INT(demo_parse("10", &n), DV_START); CHECK_INT(n, 10);
    CHECK_INT(demo_parse("11", &n), DV_BAD);
    CHECK_INT(demo_parse("0", &n), DV_BAD);
    CHECK_INT(demo_parse("3x", &n), DV_BAD);
    CHECK_INT(demo_parse("stop", &n), DV_STOP);
    CHECK_INT(demo_parse("Status", &n), DV_STATUS);
    CHECK_INT(demo_parse("stopper", &n), DV_BAD);
    demo_clock_init(&c);
    demo_status_text(&c, out, sizeof out);
    CHECK_STR(out, "demo running=0 scene=0 loops=0");
    demo_start(&c, 3);
    demo_status_text(&c, out, sizeof out);
    CHECK_STR(out, "demo running=1 scene=3 loops=0");
    demo_status_text(&c, out, 8);
    CHECK_STR(out, "demo ru");
    /* STOP from any scene ends the run; the next step does nothing */
    for (k = 1; k <= DEMO_SCENES; k++) {
        demo_start(&c, k);
        demo_step(&c, &t, &first);
        demo_stop(&c);
        CHECK_INT(c.running, 0);
        CHECK_INT(demo_step(&c, &t, &first), -1);
    }
}

void suite_demo_core(void)
{
    test_table();
    test_advance_wrap();
    test_parse_status_stop();
}
