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

void suite_pace(void)
{
    a_frame_waits_one_and_a_half_times_its_cost();
    a_cheap_frame_still_waits_20_ms_and_a_dear_one_at_most_160();
    eclock_ticks_become_microseconds();
}
