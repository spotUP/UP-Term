/* proc_core -- the portable part of ps and top (Unix tool ports 3.3): the task table, the CPU accounting of
 * the VBlank sampler and the text of both programs. Portable C89, host-tested (tests/test_proc.c); the AmigaOS
 * side (proc/ps_amiga.c, proc/top_amiga.c, proc/vbsample.s) fills the table and the sample ring.
 *
 * CPU (owner 2026-10-11): AmigaOS keeps no time per task. top adds an interrupt server to the vertical blank
 * (50 Hz on PAL) that writes SysBase->ThisTask into a ring, with bit 0 set when that task is not running (exec
 * idles in Wait with ThisTask left on the last task). A task's share is its samples over all samples of the
 * interval: statistical, no exec patch, the same on 3.1 and 3.2. */
#ifndef PROC_CORE_H
#define PROC_CORE_H

#define PC_NAME 32
#define PC_RING 1024            /* samples kept: 20 s at 50 Hz (a power of two) */
#define PC_IDLE 1UL             /* bit 0 of a sample: the CPU was idle */

enum { PC_RUN, PC_READY, PC_WAIT, PC_OTHER };

typedef struct pc_task {
    unsigned long id;           /* the task's address (what the sampler records) */
    char name[PC_NAME];         /* the task's node name */
    char cmd[PC_NAME];          /* a CLI process's command (its CLI's command name), "" none */
    int pri;
    int state;                  /* PC_RUN ... */
    int cli;                    /* the CLI number, 0 none */
    int process;                /* 1 a DOS process, 0 a plain task */
    long samples;               /* top: samples of this interval */
} pc_task;

/* The sampler's ring as top reads it: ring[k & (PC_RING - 1)] is sample k; `count` samples were taken so far. */
typedef struct pc_ring {
    unsigned long count;
    unsigned long slot[PC_RING];
} pc_ring;

/* The interval's samples, from sample `from` up to the ring's count (at most PC_RING of them, the newest),
 * added to the tasks of t by id. *idle gets the idle samples, *gone those of a task that is no longer in t.
 * Returns the number of samples looked at. */
long pc_account(pc_task *t, int n, const pc_ring *r, unsigned long from, long *idle, long *gone);

/* part of total in tenths of a per cent, rounded (0 when total is 0) */
int pc_tenths(long part, long total);

/* tenths as "100.0", " 97.5", "  0.0" (5 characters) into out */
void pc_pct(char *out, int tenths);

/* t sorted for top: most samples first, then by name; for ps: by CLI number, then by name */
void pc_sort_cpu(pc_task *t, int n);
void pc_sort_ps(pc_task *t, int n);

/* A task's state as a word: "running", "ready", "waiting", "other" */
const char *pc_state(int state);

/* The name ps and top show: a CLI process's command, else the task's name */
const char *pc_shown(const pc_task *t);

/* ps: the header and one task's line (no newline), cut to width columns (0: no cut) */
void pc_ps_header(char *out, int cap, int width);
void pc_ps_row(char *out, int cap, const pc_task *t, int width);

/* top: the summary line (tasks, busy and idle per cent of the interval, free chip and fast memory in bytes),
 * the table header and one task's line, cut to width columns (0: no cut) */
void pc_top_summary(char *out, int cap, int ntasks, long busy, long total, unsigned long chip, unsigned long fast,
                    int width);
void pc_top_header(char *out, int cap, int width);
void pc_top_row(char *out, int cap, const pc_task *t, long total, int width);

#endif
