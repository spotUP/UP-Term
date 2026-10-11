/* render/pace.h: the frame clock's interval from the last frame's cost;
 * render/jumpscroll.h: jump scroll's bookkeeping. */
#include "harness.h"
#include "../render/pace.h"
#include "../render/jumpscroll.h"

static void a_frame_waits_one_and_a_half_times_its_cost(void)
{
    CHECK_INT((int)vt_pace_next(40000), 60000);
    CHECK_INT((int)vt_pace_next(100000), 150000);
}

static void a_cheap_frame_still_waits_20_ms_and_a_dear_one_at_most_160(void)
{
    CHECK_INT((int)vt_pace_next(0), 20000);       /* an echo comes at once, not every 0 ms */
    CHECK_INT((int)vt_pace_next(5000), 20000);
    CHECK_INT((int)vt_pace_next(400000), 160000); /* a full repaint never stalls output long */
}

static void eclock_ticks_become_microseconds(void)
{
    CHECK_INT((int)vt_pace_us(709379, 709379), 1000000);   /* PAL EClock, one second */
    CHECK_INT((int)vt_pace_us(35469, 709379), 50000);      /* a 50 ms frame */
    CHECK_INT((int)vt_pace_us(113501, 709379), 160000);    /* 160 ms: no overflow */
    CHECK_INT((int)vt_pace_us(100, 0), 0);
}

/* Return held at a shell prompt (key repeat, the rig's 50 ms): each repeat
 * is a key, a render pass that scrolls one line (the echo), and a pass that
 * draws the prompt (the Shell's next Read). The screen must scroll with the
 * grid -- every scroll moves one row of pixels and the jump stays 0 -- not
 * jump a screenful ahead so the line is drawn at the top and the rows under
 * it are blank (2026-10-12: jump reached 10 of 11 rows). */
static void holding_return_scrolls_line_by_line_instead_of_restarting_at_the_top(void)
{
    vt_jump j = {0, 0, 0};
    int i, rows = 11, worst = 0;
    for (i = 0; i < 200; i++) {
        int b;
        vt_jump_key(&j);
        vt_jump_pass(&j, rows);
        b = vt_jump_scroll(&j, rows, 1); /* the echoed Return */
        if (b != 1)
            worst = b > worst ? b : (worst ? worst : -1);
        if (j.jump > worst)
            worst = j.jump;
        vt_jump_pass(&j, rows); /* the prompt, drawn before the Shell's Read waits */
    }
    CHECK_INT(worst, 0);
    CHECK_INT(j.jump, 0);
}

/* A flood (no keys, a scroll in every pass) still jumps: the step doubles
 * 2, 4, 8 up to a screenful less one row, and the text moves into the blank
 * rows below it without a blit. */
static void a_flood_still_jumps_and_its_step_stops_at_a_screenful(void)
{
    vt_jump j = {0, 0, 0};
    int rows = 11;
    vt_jump_pass(&j, rows);
    CHECK_INT(vt_jump_scroll(&j, rows, 1), 1);  /* the first scroll: no step yet */
    vt_jump_pass(&j, rows);
    CHECK_INT(vt_jump_scroll(&j, rows, 1), 3);  /* step 2: two rows spare */
    CHECK_INT(j.jump, 2);
    vt_jump_pass(&j, rows);
    CHECK_INT(vt_jump_scroll(&j, rows, 1), 0);  /* into the blank rows: no pixels move */
    CHECK_INT(j.jump, 1);
    vt_jump_pass(&j, rows);
    vt_jump_pass(&j, rows);                     /* a pass that did not scroll resets the step */
    CHECK_INT(j.step, 0);
    j.scrolled = 1;
    j.step = 8;
    vt_jump_pass(&j, rows);
    CHECK_INT(j.step, 10);                      /* 16, cut to rows - 1 */
}

/* A key in the middle of a jump keeps the text where it is (the next lines
 * fill the blank rows; the frame clock's settle moves it back down). */
static void a_key_during_a_jump_moves_no_pixels(void)
{
    vt_jump j = {5, 4, 1};
    vt_jump_key(&j);
    vt_jump_pass(&j, 11);
    CHECK_INT(j.step, 0);
    CHECK_INT(vt_jump_scroll(&j, 11, 1), 0);
    CHECK_INT(j.jump, 4);
}

void suite_pace(void)
{
    holding_return_scrolls_line_by_line_instead_of_restarting_at_the_top();
    a_flood_still_jumps_and_its_step_stops_at_a_screenful();
    a_key_during_a_jump_moves_no_pixels();
    a_frame_waits_one_and_a_half_times_its_cost();
    a_cheap_frame_still_waits_20_ms_and_a_dear_one_at_most_160();
    eclock_ticks_become_microseconds();
}
