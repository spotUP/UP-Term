/* phaseprobe: conbench's shapes one at a time, each in a stream of its own
 * (ledger S1). Run in an UP-Term window whose handler was built with
 * PROF=1: closing each stream makes the handler print where the
 * time went -- waiting, vt_feed, drawing, the rest (packets, the loop) --
 * to the serial port (the rig: build/rig/serial.log), headed by the
 * shape's name written just before. Ticks of 1/50 s to stdout.
 *   phaseprobe [ONLY n] */
#include <string.h>
#include <stdlib.h>
#include <proto/exec.h>
#include <proto/dos.h>

static char blk[4096], longl[304], buf[64], line[300];

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
    case 5: /* bytewise */
        for (i = 0; i < 2400; i++) Write(o, blk + i % 78, 1);
        break;
    case 6: /* sgr-colour: eight colour runs of 8 characters, a write each, then a newline */
        for (i = 0; i < 300; i++) {
            for (j = 0; j < 8; j++) {
                buf[0] = 27; buf[1] = '['; buf[2] = '3'; buf[3] = (char)('0' + j); buf[4] = 'm';
                memcpy(buf + 5, "12345678", 8);
                Write(o, buf, 13);
            }
            Write(o, (APTR)"\n", 1);
        }
        break;
    case 8: { /* ncurses-frame: a Scinterm page as ncurses 6.6 writes it after the
               * one-write-per-frame patch (ide-spike 2026-10-19): 24 rows of cup, five
               * style runs each of sgr0, setaf + setab, eleven characters, then EL;
               * about 4.2 KB, one write a frame, twenty frames */
        static char frame[8192];
        int f, y, r, x;
        for (f = 0; f < 20; f++) {
            int n = 0;
#define S(str) do { const char *q_ = (str); while (*q_) frame[n++] = *q_++; } while (0)
#define NUM(v) do { int v_ = (v); if (v_ >= 100) frame[n++] = (char)('0' + v_ / 100); if (v_ >= 10) frame[n++] = (char)('0' + v_ / 10 % 10); frame[n++] = (char)('0' + v_ % 10); } while (0)
            for (y = 1; y <= 24; y++) {
                S("\033["); NUM(y); S(";1H");
                for (r = 0; r < 5; r++) {
                    S("\033(B\033[m");
                    if (r & 1) { S("\033[3"); NUM(1 + (r + y + f) % 7); S("m\033[4"); NUM((r + y) % 7); S("m"); }
                    else { S("\033[38;5;"); NUM(17 + (r * 13 + y + f) % 200); S("m\033[48;5;"); NUM(232 + (y + r) % 24); S("m"); }
                    for (x = 0; x < 11; x++) frame[n++] = (char)('a' + (x + y + r + f) % 26);
                }
                S("\033[K");
            }
#undef S
#undef NUM
            Write(o, frame, n);
        }
        break;
    }
    default: /* sgr-perchar: a colour before every character, 40 a line, a write a line */
        for (i = 0; i < 60; i++) {
            int n = 0;
            for (j = 0; j < 40; j++) {
                line[n++] = 27; line[n++] = '['; line[n++] = '3'; line[n++] = (char)('0' + j % 8); line[n++] = 'm';
                line[n++] = (char)(65 + (j + i) % 26);
            }
            line[n++] = '\n';
            Write(o, line, n);
        }
        break;
    }
}

int main(int argc, char **argv)
{
    static const char *const name[] = { "plain-lines", "scroll-nl", "clear-page", "sync-line", "wrap-long", "bytewise", "sgr-colour", "sgr-perchar", "ncurses-frame" };
    int id, i, only = argc > 2 && !strcmp(argv[1], "ONLY") ? atoi(argv[2]) : -1;
    for (i = 0; i < 4096; i++)
        blk[i] = (char)((i % 79) == 78 ? '\n' : 'a' + (i % 79) % 26);
    for (i = 0; i < 300; i++)
        longl[i] = (char)('A' + i % 26);
    for (id = 0; id < 9; id++) {
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
        Delay(25); /* its lines on the serial port before ours (one port, two writers) */
        o = Open((STRPTR)"*", MODE_OLDFILE);
        if (!o)
            return 20;
        t0 = now();
        shape(o, id);
        WaitForChar(o, 0);
        t0 = now() - t0;
        Delay(25);
        ser(name[id]);
        Delay(5);
        Close(o); /* the handler prints the shape's phases */
        Delay(25);
        Printf((STRPTR)"%-12s %4ld ticks\n", (LONG)name[id], t0);
    }
    return 0;
}
