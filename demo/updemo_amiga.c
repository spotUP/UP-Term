/* UPDemo [SCENE | BENCH | TOUR [SCENE]]: updemo in the console window it is
 * started from (an XCON: window in its xterm dialect, the default). BENCH:
 * every scene for three seconds, then the frames a second each one reached
 * -- what the terminal draws in a second, scene by scene. TOUR: the tour
 * of what the terminal does (Help > Demo tour runs it in a tab of its
 * own; any key ends it). The window's size is
 * the cursor's position after a move to the far corner (DSR 6): a query
 * every dialect answers, with the cursor put back. */
#include <stdlib.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "updemo.h"

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

int main(int argc, char **argv)
{
    updemo_io io;
    int cols = 0, rows = 0, rc, bench, tour;
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
    bench = argc > 1 && (argv[1][0] == 'B' || argv[1][0] == 'b');
    tour = argc > 1 && (argv[1][0] == 'T' || argv[1][0] == 't');
    if (!size(&cols, &rows))
        rc = 2;
    else if (tour)
        rc = updemo_tour(&io, cols, rows, argc > 2 ? atoi(argv[2]) - 1 : 0, 0);
    else
        rc = updemo_run(&io, cols, rows, argc > 1 && !bench ? atoi(argv[1]) - 1 : 0, bench ? 150 : 0);
    if (rc == 1 && tour) {
        /* from the menu the tab closes when this ends: the reason stays
         * readable until a key, or ten seconds */
        static const char why[] = "UPDemo: the window is too small for the tour (76 x 20 at least). "
                                  "Press a key.\n";
        Write(con, (APTR)why, sizeof(why) - 1);
        key(0, 10000);
    }
    SetMode(con, 0);
    Close(con);
    if (bench && rc == 0) {
        int s;
        Printf((STRPTR)"UPDemo BENCH, %ldx%ld: frames a second (25 is the demo's cap)\n", (LONG)cols, (LONG)rows);
        for (s = 0; s < updemo_scenes(); s++) {
            long frames, t;
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
