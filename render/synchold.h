/* How long a ?2026 (synchronized output) frame is waited for -- pure, no
 * OS calls, so the host suite can pin it (tests/test_protocol.c).
 *
 * A program that sets ?2026 is in the middle of a frame; vtwin_render draws
 * nothing until it resets the mode, or until this much time has passed (a
 * program that dies mid-frame must not freeze the window). The others, read
 * from their sources on 2026-10-04: foot 1 s (terminal.c,
 * term_enable_app_sync_updates: it_value.tv_sec = 1), kitty 2 s
 * (screen.c, screen_pause_rendering: 2000 ms), tmux 1 s. 1 s: foot's and
 * tmux's, long enough for a full redraw of a 68k program over a slow line,
 * short enough that a lost reset is only a hiccup. (It was 3 frames, 150 ms:
 * a 68000's full-screen redraw takes longer, and tore.) */
#ifndef SYNCHOLD_H
#define SYNCHOLD_H

#define VTWIN_FRAME_MICROS 50000L    /* the frame clock: 20 frames per second */
#define VTWIN_SYNC_HOLD_MICROS 1000000L

/* Hold the drawing for one more frame? held: the frames held so far. */
#define VTWIN_SYNC_HOLD(held) ((long)(held) * VTWIN_FRAME_MICROS < VTWIN_SYNC_HOLD_MICROS)

#endif
