/* top -- the tasks by CPU use, refreshed (Unix tool ports 3.3).
 *
 * CPU (owner 2026-10-11): a VBlank interrupt server (proc/vbsample.s) samples the running task 50 times a second;
 * a task's share is its samples over the interval's. No exec patch, the same on 3.1 and 3.2. The server is added
 * when top starts and removed on every way out (q, Ctrl-C, the end of -n, an error).
 *
 *   top [-b] [-d SECONDS] [-n COUNT]
 *   -b  batch: plain lines, no screen drawing (for scripts and logs)
 *   -d  seconds between refreshes (1-20, default 2)
 *   -n  refreshes before top ends (default: until q or Ctrl-C; -b alone: 1)
 * In a window: the alternate screen, q or Ctrl-C ends it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <hardware/intbits.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "proc_core.h"
#include "tasks_amiga.h"
#include "../handler/vtcon_packets.h"
#include "../tty/ldisc.h"
#include "../tty/qmark.h"

#define MAX_TASKS 256

extern void vb_sample(void); /* proc/vbsample.s */

static struct Interrupt *server;
static pc_ring *ring;
static int screen, raw;

static void stop_sampler(void)
{
    if (server) {
        RemIntServer(INTB_VERTB, server);
        FreeMem(server, sizeof(*server));
        server = 0;
    }
    if (ring) {
        FreeMem(ring, sizeof(*ring));
        ring = 0;
    }
}

static int start_sampler(void)
{
    ring = (pc_ring *)AllocMem(sizeof(*ring), MEMF_PUBLIC | MEMF_CLEAR);
    server = (struct Interrupt *)AllocMem(sizeof(*server), MEMF_PUBLIC | MEMF_CLEAR);
    if (!ring || !server) {
        stop_sampler();
        return 0;
    }
    server->is_Node.ln_Type = NT_INTERRUPT;
    server->is_Node.ln_Pri = -60; /* after the system's own VERTB work */
    server->is_Node.ln_Name = (char *)"top sampler";
    server->is_Data = (APTR)ring;
    server->is_Code = (void (*)(void))vb_sample;
    AddIntServer(INTB_VERTB, server);
    return 1;
}

/* everything top changed, put back: the sampler, the console's mode, the main screen */
static void leave(void)
{
    stop_sampler();
    if (raw)
        SetMode(Input(), 0);
    raw = 0;
    if (screen) {
        fputs("\033[?25h\033[?1049l", stdout);
        fflush(stdout);
    }
    screen = 0;
}

static int window_size(int *cols, int *rows)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(Output());
    vt_winsize w;
    if (!fh || !fh->fh_Type || !DoPkt(fh->fh_Type, ACTION_VTCON_GWINSZ, fh->fh_Arg1, (LONG)&w, 0, 0, 0))
        return 0;
    *cols = w.ws_col;
    *rows = w.ws_row;
    return 1;
}

/* wait `ticks` fiftieths of a second; 1 when the user asked to stop (Ctrl-C, or q in a window) */
static int wait_or_quit(long ticks)
{
    while (ticks > 0) {
        long step = ticks < 10 ? ticks : 10;
        if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
            return 1;
        if (raw) {
            if (WaitForChar(Input(), step * 20000L)) {
                char c;
                if (Read(Input(), &c, 1) == 1 && (c == 'q' || c == 'Q' || c == 3))
                    return 1;
            }
        } else
            Delay(step);
        ticks -= step;
    }
    return (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) != 0;
}

static void usage(void)
{
    printf("usage: top [-b] [-d SECONDS] [-n COUNT]\n"
           "The tasks by CPU use, sampled 50 times a second, refreshed every SECONDS (default 2).\n"
           "-b plain lines (batch), -n refreshes before the end. q or Ctrl-C ends it.\n");
}

int main(int argc, char **argv)
{
    static pc_task t[MAX_TASKS];
    char line[200];
    int batch = 0, secs = 2, count = -1, cols = 80, rows = 24, i, n, shown = 0;
    unsigned long from;
    if (qmark_argv(argc, argv)) { /* `top ?` shows the usage and ends */
        usage();
        return 0;
    }
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-b"))
            batch = 1;
        else if (!strcmp(argv[i], "-d") && i + 1 < argc)
            secs = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-n") && i + 1 < argc)
            count = atoi(argv[++i]);
        else {
            usage();
            return strcmp(argv[i], "-h") && strcmp(argv[i], "--help") ? 10 : 0;
        }
    }
    if (secs < 1 || secs > 20) {
        printf("top: -d takes 1 to 20 seconds\n");
        return 10;
    }
    if (count < 0 && batch)
        count = 1;
    if (!start_sampler()) {
        printf("top: no memory for the sampler\n");
        return 20;
    }
    atexit(leave); /* also when the C library ends the program (a break inside its I/O) */
    if (!batch) {
        window_size(&cols, &rows);
        if (IsInteractive(Input()) && SetMode(Input(), 1))
            raw = 1;
        fputs("\033[?1049h\033[?25l", stdout);
        screen = 1;
    }
    from = ring->count;
    while (count != 0) {
        long idle = 0, gone = 0, total;
        int quit = wait_or_quit((long)secs * 50L), lines;
        if (quit && shown)
            break;
        n = pa_snapshot(t, MAX_TASKS);
        total = pc_account(t, n, ring, from, &idle, &gone);
        from += (unsigned long)total;
        pc_sort_cpu(t, n);
        if (!batch) {
            window_size(&cols, &rows);
            fputs("\033[H\033[2J", stdout);
        }
        pc_top_summary(line, sizeof(line), n, total - idle, total, AvailMem(MEMF_CHIP), AvailMem(MEMF_FAST),
                       batch ? 0 : cols);
        printf("%s\n", line);
        pc_top_header(line, sizeof(line), batch ? 0 : cols);
        printf(batch ? "%s\n" : "\033[7m%s\033[0m\n", line);
        lines = batch ? n : (rows - 3 < n ? rows - 3 : n);
        for (i = 0; i < lines; i++) {
            pc_top_row(line, sizeof(line), &t[i], total, batch ? 0 : cols);
            printf("%s\n", line);
        }
        if (batch)
            printf("\n");
        fflush(stdout);
        shown = 1;
        if (quit)
            break;
        if (count > 0)
            count--;
    }
    leave();
    return 0;
}
