/* updemo: UP-Term's show-off, a program for the terminal -- what an xterm
 * dialect on the Amiga can draw: text styles, 256 and 24-bit colours, box
 * drawing, double-size lines, scroll regions, live palette changes, and a
 * set of text-mode demoscene effects (copper bars, plasma, fire,
 * rotozoomer, vector cubes, a sine scroller).
 *
 * The core (updemo.c) is portable C89 with no OS calls: it writes bytes
 * and asks for keys and the time through updemo_io, so the host tests run
 * it through the engine (tests/test_updemo.c) and the same code runs in an
 * XCON: window (updemo_amiga.c) and in a Unix terminal (updemo_posix.c). */
#ifndef UPDEMO_H
#define UPDEMO_H

typedef struct updemo_io {
    void (*write)(void *user, const char *buf, long len);
    /* a typed byte, waiting up to wait_ms for one; -1 when none came */
    int  (*key)(void *user, int wait_ms);
    /* the time in 1/50 s, from any start */
    long (*ticks)(void *user);
    void *user;
} updemo_io;

#define UPDEMO_MIN_COLS 76
#define UPDEMO_MIN_ROWS 20

/* Runs the scenes from `first` (0-based) in a cols x rows terminal, on the
 * alternate screen, and leaves the terminal as it was. scene_ticks: every
 * scene's length in 1/50 s, 0 for each scene's own. Keys: Space / Return
 * the next scene, B the one before, Q / Esc / Ctrl-C quit.
 * 0 when it ran, 1 when the terminal is smaller than UPDEMO_MIN_*. */
int updemo_run(const updemo_io *io, int cols, int rows, int first, long scene_ticks);

/* how many scenes there are, and scene s's name */
int updemo_scenes(void);
const char *updemo_scene_name(int s);

/* After updemo_run: what scene s drew -- frames, and the 1/50 s they took
 * (a terminal benchmark: UPDemo BENCH prints frames a second per scene). */
void updemo_stats(int s, long *frames, long *ticks);

#endif
