/* UPDemo [SCENE] [BENCH] [ANSI] [SOLID] [FULL] (template SCENE/N,ANSI/S,FULL/S,SOLID/S,BENCH/S, any order; UPDemo ? asks): updemo in the console window it is
 * started from (an XCON: window in its xterm dialect, the default): one
 * list of scenes with captions, from SCENE (1-based) on; Space next scene,
 * B back, Q quit (Help > Demo tour runs it in a tab of its own). BENCH: the effect scenes
 * (or SCENE alone) for three seconds, then the frames a second each one
 * reached -- what the terminal draws in a second, scene by scene. The window's size is
 * the cursor's position after a move to the far corner (DSR 6): a query
 * every dialect answers, with the cursor put back. ANSI plays the version for
 * the 16 ANSI colours, SOLID the same in solid colours only (no shade blocks), FULL the 256 and 24-bit one; by itself it asks the
 * terminal (XTGETTCAP colors) and takes the 16-colour one on a screen of 16
 * colours or fewer. */
#include <stdlib.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "updemo.h"
#include "../tty/qmark.h"

static BPTR con;

static void out(void *user, const char *buf, long len)
{
    (void)user;
    Write(con, (APTR)buf, len);
}

static int key(void *user, int wait_ms)
{
    unsigned char c;
    (void)user;
    if (WaitForChar(con, wait_ms > 0 ? (LONG)wait_ms * 1000 : 1) && Read(con, &c, 1) == 1)
        return c;
    return -1;
}

static long ticks(void *user)
{
    struct DateStamp ds;
    (void)user;
    DateStamp(&ds);
    return (ds.ds_Days & 7) * 4320000L + ds.ds_Minute * 3000L + ds.ds_Tick;
}

/* "ESC [ rows ; cols R" (or CSI ...): 1 when it came */
static int size(int *cols, int *rows)
{
    int k, n = 0, v[2], seen = 0;
    v[0] = v[1] = 0;
    Write(con, (APTR)"\0337\033[999;999H\033[6n\0338", 20);
    for (;;) {
        k = key(0, 1000);
        if (k < 0)
            return 0;
        if (k == 0x9B || k == '[') {
            seen = 1;
            continue;
        }
        if (!seen)
            continue;
        if (k >= '0' && k <= '9')
            v[n] = v[n] * 10 + (k - '0');
        else if (k == ';' && n == 0)
            n = 1;
        else if (k == 'R')
            break;
        else
            return 0;
    }
    *rows = v[0];
    *cols = v[1];
    return 1;
}

#define UPDEMO_TEMPLATE "SCENE/N,ANSI/S,FULL/S,SOLID/S,BENCH/S"

int main(int argc, char **argv)
{
    updemo_io io;
    int cols = 0, rows = 0, rc, bench;
    updemo_args args;
    LONG opts[5] = { 0, 0, 0, 0, 0 }; /* SCENE (a pointer to a LONG), ANSI, FULL, SOLID, BENCH */
    struct RDArgs *rda;
    const char *line = (const char *)GetArgStr();
    (void)argc;
    (void)argv;
    /* `UPDemo ?` shows the template and ends (owner 2026-10-14: no
     * ReadArgs prompt waiting for the arguments; tty/qmark.h) */
    if (qmark_line(line)) {
        PutStr((STRPTR)UPDEMO_TEMPLATE "\n");
        return 0;
    }
    rda = ReadArgs((STRPTR)UPDEMO_TEMPLATE, opts, 0);
    if (!rda) {
        PrintFault(IoErr(), (STRPTR) "UPDemo");
        return 20;
    }
    updemo_args_make(&args, opts[0] ? (int)*(LONG *)opts[0] : 0, (int)opts[4], (int)opts[1], (int)opts[2],
                     (int)opts[3]);
    FreeArgs(rda); /* the values are copied */
    con = Open((STRPTR)"*", MODE_OLDFILE);
    if (!con || !IsInteractive(con)) {
        PutStr((STRPTR)"UPDemo: start it in a console window\n");
        return 20;
    }
    SetMode(con, 1);
    io.write = out;
    io.key = key;
    io.ticks = ticks;
    io.user = 0;
    bench = args.bench;
    updemo_colours(args.colours);
    if (!size(&cols, &rows))
        rc = 2;
    else {
        int first = args.scene ? args.scene - 1 : 0;
        if (bench && args.scene)
            updemo_only(1); /* BENCH n: scene n alone */
        else if (bench)
            updemo_bench(1);
        rc = updemo_run(&io, cols, rows, first, bench ? 150 : 0);
    }
    SetMode(con, 0);
    Close(con);
    if (bench && rc == 0) {
        int s;
        Printf((STRPTR)"UPDemo BENCH, %ldx%ld: frames a second (25 is the demo's cap)\n", (LONG)cols, (LONG)rows);
        for (s = 0; s < updemo_scenes(); s++) {
            long frames, t;
            if (!updemo_scene_benchmarked(s))
                continue;
            updemo_stats(s, &frames, &t);
            if (t > 0)
                Printf((STRPTR)"  %-16s %3ld.%ld\n", (LONG)updemo_scene_name(s), frames * 50 / t,
                       frames * 500 / t % 10);
        }
    }
    if (rc == 1)
        Printf((STRPTR)"UPDemo: the window is %ldx%ld, it needs %ldx%ld\n", (LONG)cols, (LONG)rows,
               (LONG)UPDEMO_MIN_COLS, (LONG)UPDEMO_MIN_ROWS);
    else if (rc == 2)
        PutStr((STRPTR)"UPDemo: the console does not answer (not an UP-Term window?)\n");
    return rc ? 10 : 0;
}
