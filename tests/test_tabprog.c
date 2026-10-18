/* handler/tabprog.h: a program tab's script, and the window it needs. */
#include <string.h>
#include "harness.h"
#include "../handler/tabprog.h"

static void a_failed_demo_keeps_its_tab_open_until_return(void)
{
    char s[256];
    CHECK(tabprog_script(s, sizeof(s), "", "C:UPDemo"));
    CHECK(strstr(s, "FailAt 2147483647\nC:UPDemo\n") == s);
    /* UPDemo's "the window is WxH, it needs 76x20" (rc 10) stays readable */
    CHECK(strstr(s, "C:UPDemo\nIf WARN\nAsk \"Press Return to close this tab.\"\nEndIf\nEndCLI >NIL:\n") != 0);
}

static void claude_tab_keeps_its_stack_line(void)
{
    char s[256];
    CHECK(tabprog_script(s, sizeof(s), "Stack 32768\n", "C:Claude"));
    CHECK(strstr(s, "FailAt 2147483647\nStack 32768\nC:Claude\nIf WARN\n") == s);
}

static void a_script_too_long_for_the_buffer_is_refused(void)
{
    char s[40];
    CHECK(!tabprog_script(s, sizeof(s), "", "C:UPDemo"));
}

static void a_small_window_grows_to_the_demo_and_the_tab_bar(void)
{
    int c, r;
    /* 60x18, no tab bar yet (it takes 2 rows when the demo's tab opens) */
    CHECK(tabprog_grow(60, 18, 76, 20, 2, &c, &r));
    CHECK_INT(c, 76);
    CHECK_INT(r, 22);
    /* 80x20 with no bar: wide enough, but the bar would leave 18 rows */
    CHECK(tabprog_grow(80, 20, 76, 20, 2, &c, &r));
    CHECK_INT(c, 80);
    CHECK_INT(r, 22);
}

static void a_big_window_is_left_alone(void)
{
    int c, r;
    CHECK(!tabprog_grow(80, 48, 76, 20, 2, &c, &r));
    CHECK(!tabprog_grow(76, 20, 76, 20, 0, &c, &r)); /* the bar is up already */
    CHECK_INT(c, 76);
    CHECK_INT(r, 20);
    CHECK(!tabprog_grow(40, 10, 0, 0, 0, &c, &r));   /* Claude: no size */
}

void suite_tabprog(void)
{
    a_failed_demo_keeps_its_tab_open_until_return();
    claude_tab_keeps_its_stack_line();
    a_script_too_long_for_the_buffer_is_refused();
    a_small_window_grows_to_the_demo_and_the_tab_bar();
    a_big_window_is_left_alone();
}
