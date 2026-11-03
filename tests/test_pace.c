/* render/pace.h: the frame clock's interval from the last frame's cost. */
#include "harness.h"
#include "../render/pace.h"

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

static void the_animation_clock_stops_when_hidden_or_scrolled_back(void)
{
    CHECK_INT(vt_anim_paused(0, 0), 0);
    CHECK_INT(vt_anim_paused(1, 0), 1);
    CHECK_INT(vt_anim_paused(0, 3), 1);
}

/* K36: the animation request waits the engine's due time (a 1 s gap sleeps 1 s), floored by the pace */
static void the_animation_request_waits_the_due_time_floored_by_the_pace(void)
{
    CHECK_INT((int)vt_anim_arm_us(1000000, 20000), 1000000);
    CHECK_INT((int)vt_anim_arm_us(40000, 20000), 40000);
    CHECK_INT((int)vt_anim_arm_us(10000, 60000), 60000);
    CHECK_INT((int)vt_anim_arm_us(0, 0), (int)VT_PACE_MIN_US);
}

void suite_pace(void)
{
    the_animation_request_waits_the_due_time_floored_by_the_pace();
    the_animation_clock_stops_when_hidden_or_scrolled_back();
    a_frame_waits_one_and_a_half_times_its_cost();
    a_cheap_frame_still_waits_20_ms_and_a_dear_one_at_most_160();
    eclock_ticks_become_microseconds();
}
