/* phaseprobe: conbench's shapes one at a time, each in a stream of its own
 * (ledger S1). Run in an UP-Term window whose handler was built with
 * PROF=1 SERIAL=1: closing each stream makes the handler print where the
 * time went -- waiting, vt_feed, drawing, the rest (packets, the loop) --
 * to the serial port (the rig: build/rig/serial.log), headed by the
 * shape's name written just before. Ticks of 1/50 s to stdout.
 *   phaseprobe [ONLY n] */
#include <string.h>
#include <stdlib.h>
#include <proto/exec.h>
#include <proto/dos.h>

static char blk[4096], longl[304], buf[64];

static long now(void) { struct DateStamp ds; DateStamp(&ds); return ds.ds_Minute * 3000L + ds.ds_Tick; }

/* The serial port, through exec's RawPutChar: the shape's name before the
 * handler's lines for it. */
static void ser_putc(__reg("d0") char ch) = "\tmove.l\ta6,-(sp)\n\tmove.l\t4.w,a6\n\tjsr\t-516(a6)\n\tmove.l\t(sp)+,a6";
static void ser(const char *s) { while (*s) ser_putc(*s++); ser_putc('\n'); }

static void shape(BPTR o, int id)
{
    int i, j;
    switch (id) {
    case 0: /* plain-lines: a 79-byte line a write */
        for (i = 0; i < 816; i++) Write(o, blk, 79);
        break;
    case 1: /* scroll-nl: ten bare newlines a write */
        for (i = 0; i < 1200; i++) Write(o, (APTR)"\n\n\n\n\n\n\n\n\n\n", 10);
        break;
    case 2: /* clear-page: form feed, then 20 lines */
        for (i = 0; i < 40; i++) { Write(o, (APTR)"\f", 1); for (j = 0; j < 20; j++) Write(o, blk, 79); }
        break;
    case 3: /* sync-line: a line, then the barrier */
        for (i = 0; i < 200; i++) { Write(o, blk, 79); WaitForChar(o, 0); }
        break;
    case 4: /* wrap-long: 300 characters, no newline */
        for (i = 0; i < 140; i++) Write(o, longl, 300);
        break;
    default: /* bytewise */
        for (i = 0; i < 2400; i++) Write(o, blk + i % 78, 1);
        break;
    }
}

int main(int argc, char **argv)
{
    static const char *const name[] = { "plain-lines", "scroll-nl", "clear-page", "sync-line", "wrap-long", "bytewise" };
    int id, i, only = argc > 2 && !strcmp(argv[1], "ONLY") ? atoi(argv[2]) : -1;
    for (i = 0; i < 4096; i++)
        blk[i] = (char)((i % 79) == 78 ? '\n' : 'a' + (i % 79) % 26);
    for (i = 0; i < 300; i++)
        longl[i] = (char)('A' + i % 26);
    for (id = 0; id < 6; id++) {
        BPTR o;
        long t0;
        if (only >= 0 && id != only)
            continue;
        o = Open((STRPTR)"*", MODE_OLDFILE);
        if (!o)
            return 20;
        Write(o, (APTR)"\033[0m\f", 5);
        WaitForChar(o, 0);
        Close(o); /* the handler's profile restarts here */
        o = Open((STRPTR)"*", MODE_OLDFILE);
        if (!o)
            return 20;
        t0 = now();
        shape(o, id);
        WaitForChar(o, 0);
        t0 = now() - t0;
        ser(name[id]);
        Close(o); /* the handler prints the shape's phases */
        Printf((STRPTR)"%-12s %4ld ticks\n", (LONG)name[id], t0);
    }
    return 0;
}
