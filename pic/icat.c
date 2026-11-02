/* icat -- show a picture in the console, the way kitty's icat does: any picture datatypes can read
 * (IFF ILBM, GIF, JPEG, PNG, ...), scaled to fit the window, drawn with the kitty graphics protocol
 * that the console understands (engine/kitty.c). One encoder, pic/kittyout.c.
 *
 *   icat FILE [WIDTH n] [HEIGHT n] [PLACE x,y] [CLEAR]
 *   icat CLEAR
 *
 * WIDTH and HEIGHT are in character cells. PLACE x,y moves the cursor to column x, row y first
 * (1-based). CLEAR deletes every picture of the window. A PNG goes as it is (f=100); every other
 * format is decoded by picture.datatype and goes as raw 32-bit pixels (f=32), without
 * compression: the engine can inflate but this tool has no deflate. A console that does not
 * answer the kitty query within a second gets a line of text (name, size, format) and the
 * return code 5 (WARN). Return codes: 0 shown, 5 not a kitty console, 10 cannot read FILE,
 * 20 bad arguments. Amiga only; 68000 safe. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/datatypes.h>
#include <datatypes/datatypes.h>
#include <datatypes/pictureclass.h>
#include <clib/alib_protos.h>
#include <string.h>
#include "kittyout.h"
#include "../handler/vtcon_packets.h"
#include "../tty/ldisc.h"
#include "../tty/qmark.h"

#include "upterm_version.h"
static const char vers[] = "$VER: icat " UPTERM_VERSION " (" UPTERM_DATE ") UP-Term";

#define TEMPLATE "FILE,WIDTH/K/N,HEIGHT/K/N,PLACE/K,CLEAR/S"
#define MAXSIDE 8192
#define MAXPIX 1000000L /* what the engine takes in one picture */

/* vbcc's startup runs main on a stack of this size; the cookie says the same to the OS and vsh
 * (RULES.md rule 6). The datatypes classes want a deep stack. */
long __stack = 65536;
const char icat_stack_cookie[] = "$STACK: 65536";

struct Library *DataTypesBase;

static void out_put(void *u, const char *s, long n)
{
    (void)u;
    if (n > 0)
        Write(Output(), (APTR)s, n);
}

static void say(const char *s)
{
    out_put(0, s, (long)strlen(s));
}

/* a diagnostic goes to the error stream (pr_CES, else the output), so "2>" sees it and a redirected
 * stdout does not hide it */
static void esay(const char *s)
{
    struct Process *pr = (struct Process *)FindTask(0);
    BPTR e = pr->pr_CES ? pr->pr_CES : Output();
    if (e)
        Write(e, (APTR)s, (long)strlen(s));
}

static void say_num(long v)
{
    char b[16];
    int i = 15;
    b[i] = 0;
    if (v <= 0)
        b[--i] = '0';
    while (v > 0) {
        b[--i] = (char)('0' + v % 10);
        v /= 10;
    }
    say(b + i);
}

/* the window: columns, rows and the cell in pixels, 0 when the output is no vtcon console */
static void window(int *cols, int *rows, int *cw, int *ch)
{
    BPTR out = Output();
    struct FileHandle *fh;
    vt_winsize ws;
    *cols = *rows = *cw = *ch = 0;
    if (!out || !IsInteractive(out))
        return;
    fh = (struct FileHandle *)BADDR(out);
    if (!fh->fh_Type)
        return;
    ws.ws_row = ws.ws_col = ws.ws_xpixel = ws.ws_ypixel = 0;
    if (DoPkt(fh->fh_Type, ACTION_VTCON_GWINSZ, fh->fh_Arg1, (LONG)&ws, 0, 0, 0) && ws.ws_col > 0 &&
        ws.ws_row > 0 && ws.ws_xpixel > 0 && ws.ws_ypixel > 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        *cw = ws.ws_xpixel / ws.ws_col;
        *ch = ws.ws_ypixel / ws.ws_row;
    }
}

/* the whole file in memory */
static unsigned char *slurp(const char *name, long *n)
{
    BPTR fh = Open((STRPTR)name, MODE_OLDFILE);
    struct FileInfoBlock *fib;
    unsigned char *buf = 0;
    long size = 0;
    if (!fh)
        return 0;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib) {
        if (ExamineFH(fh, fib))
            size = fib->fib_Size;
        FreeDosObject(DOS_FIB, fib);
    }
    if (size > 0 && size < 32L * 1024 * 1024) {
        buf = (unsigned char *)AllocVec((ULONG)size, MEMF_ANY);
        if (buf && Read(fh, buf, size) != size) {
            FreeVec(buf);
            buf = 0;
        }
    }
    Close(fh);
    *n = buf ? size : 0;
    return buf;
}

static int is_png(const unsigned char *b, long n)
{
    static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    return n >= 33 && memcmp(b, sig, 8) == 0;
}

static long be32(const unsigned char *p)
{
    return ((long)p[0] << 24) | ((long)p[1] << 16) | ((long)p[2] << 8) | p[3];
}

/* picture.datatype: the picture as RGBA bytes (w x h x 4), the datatype's name in fmt. 0 when no class reads it. */
static unsigned char *decode(const char *name, int *w, int *h, char *fmt, int fmtn)
{
    Object *o;
    struct BitMapHeader *bmh = 0;
    ULONG alpha = 0;
    unsigned char *px = 0;
    ULONG *row;
    LONG x, y;
    struct DataType *dt = 0;
    fmt[0] = 0;
    o = NewDTObject((APTR)name, DTA_SourceType, DTST_FILE, DTA_GroupID, GID_PICTURE, PDTA_DestMode,
                    PMODE_V43, PDTA_Remap, FALSE, TAG_DONE);
    if (!o)
        return 0;
    {
        struct gpLayout gl;
        gl.MethodID = DTM_PROCLAYOUT;
        gl.gpl_GInfo = 0;
        gl.gpl_Initial = 1;
        DoDTMethodA(o, 0, 0, (Msg)&gl);
    }
    GetDTAttrs(o, PDTA_BitMapHeader, (ULONG)&bmh, TAG_DONE);
    GetDTAttrs(o, PDTA_AlphaChannel, (ULONG)&alpha, TAG_DONE);
    GetDTAttrs(o, DTA_DataType, (ULONG)&dt, TAG_DONE);
    if (dt && dt->dtn_Header && dt->dtn_Header->dth_Name) {
        strncpy(fmt, (const char *)dt->dtn_Header->dth_Name, (size_t)fmtn - 1);
        fmt[fmtn - 1] = 0;
    }
    if (bmh && bmh->bmh_Width > 0 && bmh->bmh_Height > 0 && bmh->bmh_Width <= MAXSIDE &&
        bmh->bmh_Height <= MAXSIDE) {
        *w = bmh->bmh_Width;
        *h = bmh->bmh_Height;
        if (bmh->bmh_Masking == mskHasAlpha)
            alpha = 1;
        px = (unsigned char *)AllocVec((ULONG)*w * (ULONG)*h * 4, MEMF_ANY);
        row = (ULONG *)AllocVec((ULONG)*w * 4, MEMF_ANY);
        for (y = 0; px && row && y < *h; y++) {
            struct pdtBlitPixelArray pb;
            unsigned char *d = px + (long)y * *w * 4;
            pb.MethodID = PDTM_READPIXELARRAY;
            pb.pbpa_PixelData = row;
            pb.pbpa_PixelFormat = PBPAFMT_ARGB;
            pb.pbpa_PixelArrayMod = *w * 4;
            pb.pbpa_Left = 0;
            pb.pbpa_Top = y;
            pb.pbpa_Width = *w;
            pb.pbpa_Height = 1;
            /* straight to the class (amiga.lib DoMethodA), as the handler does: through DoDTMethodA it never returned on OS 3.2.2 */
            if (!DoMethodA(o, (Msg)&pb)) {
                FreeVec(px);
                px = 0;
                break;
            }
            for (x = 0; x < *w; x++) {
                ULONG v = row[x];
                d[4 * x] = (unsigned char)(v >> 16);
                d[4 * x + 1] = (unsigned char)(v >> 8);
                d[4 * x + 2] = (unsigned char)v;
                d[4 * x + 3] = alpha ? (unsigned char)(v >> 24) : 255; /* no alpha channel: opaque */
            }
        }
        if (row)
            FreeVec(row);
    }
    DisposeDTObject(o);
    return px;
}

/* ask the console for kitty graphics: 1 when it answers within about a second */
static int detect(void)
{
    BPTR in = Input();
    char buf[64];
    long n = 0, r;
    int tries;
    int ok = 0;
    if (!in || !IsInteractive(in))
        return 0;
    SetMode(in, 1);
    kout_query(out_put, 0);
    for (tries = 0; tries < 3 && !ok; tries++) {
        if (!WaitForChar(in, 400000))
            continue;
        r = Read(in, buf + n, (long)sizeof buf - n);
        if (r > 0)
            n += r;
        ok = kout_query_ok(buf, n);
    }
    SetMode(in, 0);
    return ok;
}

/* "x,y" -> column and row */
static int parse_place(const char *s, int *x, int *y)
{
    int v[2] = {0, 0}, i = 0;
    if (!s || !*s)
        return 0;
    for (; *s; s++) {
        if (*s >= '0' && *s <= '9')
            v[i] = v[i] * 10 + (*s - '0');
        else if (*s == ',' && i == 0)
            i = 1;
        else
            return 0;
    }
    *x = v[0];
    *y = v[1];
    return i == 1;
}

/* the size to show w x h at: inside the box (bw x bh pixels, 0 = no limit), never above the input */
static void fit_box(int w, int h, int bw, int bh, int *dw, int *dh)
{
    long fw = w, fh = h; /* scaled so that fw/w == fh/h */
    *dw = w;
    *dh = h;
    if (bw > 0 && fw > bw) {
        fh = fh * bw / fw;
        fw = bw;
    }
    if (bh > 0 && fh > bh) {
        fw = fw * bh / fh;
        fh = bh;
    }
    if (fw < 1)
        fw = 1;
    if (fh < 1)
        fh = 1;
    *dw = (int)fw;
    *dh = (int)fh;
}

int main(void)
{
    LONG opt[5] = {0, 0, 0, 0, 0};
    struct RDArgs *rd;
    const char *args = (const char *)GetArgStr();
    const char *name;
    int cols, rows, cw, ch, px = 0, py = 0, havepos, rc = 0;
    int bw, bh, dw, dh, w = 0, h = 0, kitty;
    long n = 0;
    unsigned char *file, *pix = 0;
    char fmt[48];
    kout_opts ko;
    if (qmark_line(args)) {
        PutStr((STRPTR)TEMPLATE "\n");
        return 0;
    }
    rd = ReadArgs((STRPTR)TEMPLATE, opt, 0);
    if (!rd) {
        PrintFault(IoErr(), (STRPTR) "icat");
        return 20;
    }
    name = (const char *)opt[0];
    havepos = opt[3] != 0;
    if (havepos && !parse_place((const char *)opt[3], &px, &py)) {
        esay("icat: PLACE is x,y\n");
        FreeArgs(rd);
        return 20;
    }
    if (!name && !opt[4]) {
        esay("icat: FILE or CLEAR\n");
        FreeArgs(rd);
        return 20;
    }
    window(&cols, &rows, &cw, &ch);
    if (opt[4]) {
        say("\033_Ga=d,q=2\033\\");
        if (!name) {
            FreeArgs(rd);
            return 0;
        }
    }
    file = slurp(name, &n);
    if (!file) {
        esay("icat: cannot read ");
        esay(name);
        esay("\n");
        FreeArgs(rd);
        return 10;
    }
    /* the box: WIDTH and HEIGHT cells, else the window */
    bw = opt[1] ? *(LONG *)opt[1] : cols;
    bh = opt[2] ? *(LONG *)opt[2] : 0;
    if (bw > 0 && cw > 0)
        bw *= cw;
    else
        bw = 0;
    if (bh > 0 && ch > 0)
        bh *= ch;
    else
        bh = 0;
    ko.cols = ko.rows = 0;
    ko.quiet = 2;
    fmt[0] = 0;
    if (is_png(file, n)) {
        w = (int)be32(file + 16);
        h = (int)be32(file + 20);
        strcpy(fmt, "PNG");
    } else {
        DataTypesBase = OpenLibrary((STRPTR) "datatypes.library", 39);
        if (DataTypesBase) {
            pix = decode(name, &w, &h, fmt, (int)sizeof fmt);
            CloseLibrary(DataTypesBase);
            DataTypesBase = 0;
        }
        if (!pix) {
            esay("icat: no datatype reads ");
            esay(name);
            esay(" (picture.datatype and the format's class must be installed: OS 3.2 has them, a 3.1 system may not)\n");
            FreeVec(file);
            FreeArgs(rd);
            return 10;
        }
    }
    kitty = detect();
    if (!kitty) {
        say(name);
        say(": ");
        say_num(w);
        say("x");
        say_num(h);
        say(" ");
        say(fmt[0] ? fmt : "picture");
        say(" (this console does not show pictures)\n");
        rc = 5;
    } else {
        if (havepos) {
            say("\033[");
            say_num(py);
            say(";");
            say_num(px);
            say("H");
        }
        fit_box(w, h, bw, bh, &dw, &dh);
        if (pix) {
            unsigned char *small = pix;
            long total = (long)dw * dh;
            if (total > MAXPIX)
                kout_fit(dw, dh, MAXPIX, dw, &dw, &dh);
            if (dw != w || dh != h) {
                small = (unsigned char *)AllocVec((ULONG)dw * (ULONG)dh * 4, MEMF_ANY);
                if (small)
                    kout_scale(pix, w, h, 4, small, dw, dh);
                else {
                    small = pix;
                    dw = w;
                    dh = h;
                }
            }
            if (!kout_pixels(out_put, 0, small, dw, dh, 4, &ko))
                rc = 10;
            if (small != pix)
                FreeVec(small);
        } else {
            /* the engine scales a PNG by cells; the cell box of the fitted size */
            if (cw > 0 && ch > 0 && (dw != w || dh != h)) {
                ko.cols = (dw + cw - 1) / cw;
                ko.rows = (dh + ch - 1) / ch;
            }
            if (!kout_png(out_put, 0, file, n, &ko))
                rc = 10;
        }
        say("\n");
    }
    if (pix)
        FreeVec(pix);
    FreeVec(file);
    FreeArgs(rd);
    return rc;
}
