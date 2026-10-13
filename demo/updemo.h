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

/* The demo: one list of scenes, each with a caption on the status line --
 * styles, colours, character sets, Unicode, scroll regions, a tmux split,
 * vsh, completion, themes switched live, mouse reports, links, sixel
 * images, synchronized output, a resize, reflow -- then the demoscene
 * effects (copper bars, plasma, fire, rotozoomer, vector cubes, a sine
 * scroller, palette cycling). Space, Return and Right go to the next scene, B and Left back,
 * Q, Esc and Ctrl-C end it; a mouse report is no key (the mouse scene shows
 * them). BENCH: any key ends it. It asks the window's size (CSI 18 t)
 * before each scene and follows it.
 * Runs the scenes from `first` (0-based) in a cols x rows terminal, on the
 * alternate screen, and leaves the terminal as it was. scene_ticks: every
 * scene's length in 1/50 s, 0 for each scene's own.
 * 0 when it ran, 1 when the terminal is smaller than UPDEMO_MIN_*. */
int updemo_run(const updemo_io *io, int cols, int rows, int first, long scene_ticks);

/* 1: the next updemo_run plays scene `first` alone (UPDemo BENCH n: one
 * scene's frame rate, and a PROF=1 handler's profile of it alone). */
void updemo_only(int on);

/* 1: the next updemo_run plays only the benchmarked scenes (UPDemo BENCH) */
void updemo_bench(int on);

/* Which colours the demo plays in. UPDEMO_ANSI: the 16-colour version, made for the standard ANSI colours
 * (SGR 30-37, 90-97, 40-47, 100-107 and no palette writes; shade blocks
 * U+2591-2593 and the full block for gradients). UPDEMO_FULL: the 256 and
 * 24-bit colours version. UPDEMO_AUTO (the default): the 16-colour one when
 * the terminal says it has 16 colours or fewer (XTGETTCAP colors, which
 * UP-Term answers with the screen's pens), else the full one. */
#define UPDEMO_AUTO 0
#define UPDEMO_ANSI 1
#define UPDEMO_FULL 2
void updemo_colours(int mode);

/* After updemo_run: 1 when it played the 16-colour version */
int updemo_ansi(void);

/* The count in XTGETTCAP's answer to a colors query (DCS 1 + r 636F6C6F7273=hex ST); 0 when none */
int updemo_colour_count(const char *reply, long len);

/* The command line of UPDemo and updemo: [n] [BENCH [n]] [ANSI | FULL] */
typedef struct updemo_args {
    int bench;   /* BENCH */
    int scene;   /* n, 1-based; 0: none */
    int colours; /* UPDEMO_AUTO, _ANSI or _FULL */
} updemo_args;
void updemo_parse_args(int argc, char **argv, updemo_args *a);

/* The 16-colour version's ramps (an effect's value -> a foreground, a
 * background and a shade: 0 a solid colour, 1-3 the blocks U+2591-2593 of
 * the foreground over the background), k in 0..updemo_ramp_count() - 1.
 * v in 0..255; returns the step's number on the ramp, -1 for a bad k or v. */
int updemo_ramp_count(void);
const char *updemo_ramp_name(int k);
int updemo_ramp_step(int k, int v, int *fgc, int *bgc, int *shade);

/* how many scenes there are, scene s's name, and whether BENCH measures it */
int updemo_scenes(void);
const char *updemo_scene_name(int s);
int updemo_scene_benchmarked(int s);

/* After updemo_run: what scene s drew -- frames, and the 1/50 s they took
 * (a terminal benchmark: UPDemo BENCH prints frames a second per scene). */
void updemo_stats(int s, long *frames, long *ticks);

#endif
