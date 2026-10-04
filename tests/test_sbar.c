/* render/sbar: the scroll bar's knob from the scrollback view, and back. */
#include "harness.h"
#include "../render/sbar.h"

static sbar_knob k;

static void the_live_screen_puts_the_knob_at_the_bottom(void)
{
    sbar_from_view(&k, 500, 24, 0, 0);
    CHECK_INT(k.live, 1);
    CHECK_INT(k.total, 524);
    CHECK_INT(k.visible, 24);
    CHECK_INT(k.top, 500);
    CHECK_INT(k.shift, 0);
}

static void the_knob_size_is_rows_over_scrollback_plus_rows(void)
{
    sbar_from_view(&k, 76, 24, 0, 0);
    CHECK_INT(k.total, 100);
    CHECK_INT(k.visible, 24); /* a quarter of the track */
}

static void scrolling_back_moves_the_knob_up(void)
{
    sbar_from_view(&k, 500, 24, 23, 0);
    CHECK_INT(k.top, 477);
    sbar_from_view(&k, 500, 24, 500, 0); /* the oldest line: the top */
    CHECK_INT(k.top, 0);
    sbar_from_view(&k, 500, 24, 9999, 0); /* past it: clamped */
    CHECK_INT(k.top, 0);
}

static void no_scrollback_fills_the_track(void)
{
    sbar_from_view(&k, 0, 24, 0, 0);
    CHECK_INT(k.live, 0);
    CHECK_INT(k.total, 24);
    CHECK_INT(k.visible, 24);
    CHECK_INT(k.top, 0);
    CHECK_INT(sbar_to_view(&k, 0, 0), 0);
}

static void the_alternate_screen_shows_no_scrollback(void)
{
    sbar_from_view(&k, 500, 24, 0, 1);
    CHECK_INT(k.live, 0);
    CHECK_INT(k.visible, k.total);
    CHECK_INT(sbar_to_view(&k, 500, 0), 0); /* a drag there leaves the live screen */
}

static void dragging_the_knob_gives_the_view_back(void)
{
    int v, bad = 0;
    for (v = 0; v <= 500; v++) {
        sbar_from_view(&k, 500, 24, v, 0);
        if (sbar_to_view(&k, 500, k.top) != v)
            bad++;
    }
    CHECK_INT(bad, 0);
    sbar_from_view(&k, 500, 24, 0, 0);
    CHECK_INT(sbar_to_view(&k, 500, 0), 500);   /* dragged to the top */
    CHECK_INT(sbar_to_view(&k, 500, 499), 1);   /* one line above the bottom */
    CHECK_INT(sbar_to_view(&k, 500, 500), 0);   /* the bottom: live */
    CHECK_INT(sbar_to_view(&k, 500, 60000), 0); /* past it */
}

static void a_scrollback_over_16_bits_is_counted_in_bigger_units(void)
{
    int v, bad = 0;
    sbar_from_view(&k, 100000L, 24, 0, 0);
    CHECK_INT(k.shift, 1);
    CHECK_INT(k.total, 50012);
    CHECK_INT(k.visible, 12);
    CHECK_INT(k.top, 50000); /* the bottom exactly */
    sbar_from_view(&k, 100000L, 24, 100000, 0);
    CHECK_INT(k.top, 0);
    CHECK_INT(sbar_to_view(&k, 100000L, 0), 100000);
    /* back from the knob within one unit (two lines) */
    for (v = 100; v <= 100000; v += 997) {
        int got;
        sbar_from_view(&k, 100000L, 24, v, 0);
        got = sbar_to_view(&k, 100000L, k.top);
        if (got < v - 1 || got > v + 1)
            bad++;
    }
    CHECK_INT(bad, 0);
}

static void the_same_knob_needs_no_update(void)
{
    sbar_knob a, b;
    sbar_from_view(&a, 500, 24, 3, 0);
    sbar_from_view(&b, 500, 24, 3, 0);
    CHECK_INT(sbar_equal(&a, &b), 1);
    sbar_from_view(&b, 500, 24, 4, 0);
    CHECK_INT(sbar_equal(&a, &b), 0);
    sbar_from_view(&b, 501, 24, 3, 0); /* a line more in the scrollback */
    CHECK_INT(sbar_equal(&a, &b), 0);
}

void suite_sbar(void)
{
    the_live_screen_puts_the_knob_at_the_bottom();
    the_knob_size_is_rows_over_scrollback_plus_rows();
    scrolling_back_moves_the_knob_up();
    no_scrollback_fills_the_track();
    the_alternate_screen_shows_no_scrollback();
    dragging_the_knob_gives_the_view_back();
    a_scrollback_over_16_bits_is_counted_in_bigger_units();
    the_same_knob_needs_no_update();
}
