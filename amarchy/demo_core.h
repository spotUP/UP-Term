/* The pure half of the Amarchy demo tour (demo.c owns the OS side): the scene table (names, seconds), the clock that
 * advances one scene per elapsed time and wraps with a loop count, the DEMO verb parse and the STATUS text. No OS types,
 * no allocation; host-tested by tests/test_demo_core.c. */
#ifndef AMARCHY_DEMO_CORE_H
#define AMARCHY_DEMO_CORE_H

#define DEMO_SCENES 10

typedef struct demo_clock {
    int running;
    int scene;   /* 0-based index of the scene in progress */
    int t;       /* ticks already run in that scene */
    int loops;   /* completed passes through the table */
    int fresh;   /* 1 until the scene's first tick has run */
} demo_clock;

typedef enum { DV_BAD = 0, DV_START, DV_STOP, DV_STATUS } demo_verb;

const char *demo_scene_name(int i);   /* 0-based; "" when out of range */
int demo_scene_secs(int i);           /* 0-based; 0 when out of range */
int demo_total_secs(void);

void demo_clock_init(demo_clock *c);
/* The text after DEMO: empty = start at scene 1, a number 1..DEMO_SCENES = start there, STOP, STATUS (any case). */
demo_verb demo_parse(const char *args, int *scene);
void demo_start(demo_clock *c, int scene);   /* scene 1-based */
void demo_stop(demo_clock *c);
/* One tick: returns the 0-based scene to run now with its t and first flag, or -1 when not running. The scene ends
 * on the call after it has had its seconds; the last scene wraps to the first and counts a loop. */
int demo_step(demo_clock *c, int *t, int *first);
void demo_next(demo_clock *c);               /* end the scene in progress at the next step (an early end) */
void demo_status_text(const demo_clock *c, char *out, int cap);   /* "demo running=1 scene=3 loops=0" */

#endif
