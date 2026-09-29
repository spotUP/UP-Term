/* vtcon-handler: a DOS console handler (XCON:) on the vtengine.
 *
 * One process per window, like the ROM CON: -- DOS starts a new process
 * for every Open() because the handler never sets dn_Task. The first
 * packet is the startup packet, then FINDINPUT/FINDOUTPUT/FINDUPDATE with
 * the full name (XCON:x/y/w/h/title/options). Open("*") from the shell
 * running in the window reaches the same process through pr_ConsoleTask.
 *
 * Packets: matrix section 7.1 (thoughts/shared/research/
 * 2026-09-28_console-conformance-matrix.md). Options, in addition to the
 * CON: ones (CLOSE, WAIT, AUTO, SCREEN name, BACKDROP, NODRAG, NOBORDER,
 * NOSIZE, INACTIVE, SIMPLE, SMART): XTERM, AMIGA, PCANSI pick the
 * personality (XTERM is the default of XCON:), LATIN1 / CP437 make XTERM
 * take Latin-1 bytes (ixemul programs) or CP437 (programs drawing for an
 * IBM font, like BitchX) instead of UTF-8, FONT name/size.
 *
 * Built without a C startup: `handler_entry` must stay the first function.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/interrupts.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <devices/timer.h>
#include <devices/inputevent.h>
#include <devices/input.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/console.h>
#include <proto/diskfont.h>
#include <proto/timer.h>

#include <string.h>
#include "../engine/vtengine.h"
#include "../render/amiga_render.h"
#include "clip.h"
#include "lineedit.h"
#include "complete.h"
#include "vtcon_packets.h"
#include "../tty/ldisc.h"

/* rexx/rexxio.h: ARexx PUSH and QUEUE */
#ifndef ACTION_STACK
#define ACTION_STACK 2002L
#define ACTION_QUEUE 2003L
#endif

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Device *ConsoleDevice;
struct Library *DiskfontBase;
struct Library *LayersBase; /* LockLayer, for the renderer's planar path */
struct Device *TimerBase; /* for ReadEClock in the debug profile */

static LONG handler_main(void);

/* The entry point: first code in the hunk. */
LONG handler_entry(void)
{
    SysBase = *(struct ExecBase **)4L;
    return handler_main();
}

/* Build fingerprint, readable with `version L:vtcon-handler`. VTCON_BUILD
 * is the git revision, passed bare (vc drops quotes on its way to the
 * compiler) and turned into a string here. */
#ifndef VTCON_BUILD
#define VTCON_BUILD unknown
#endif
#define STR2(x) #x
#define STR(x) STR2(x)
static const char vers[] = "$VER: vtcon-handler 0.1 (29.9.26) " STR(VTCON_BUILD);

/* ---- the console ----------------------------------------------------------- */

#define IN_MAX 4096
#define READ_Q 16

typedef struct con {
    struct MsgPort *port;
    struct Window *win;
    struct Screen *locked;       /* the public screen we opened on */
    struct TextFont *font;
    int font_opened;
    vt_term *t;
    vr_render r;
    int opens;
    int raw;
    int wait_close;              /* WAIT: keep the window after the last Close */
    int closing;                 /* close gadget clicked */
    int eof;                     /* Ctrl-\ or close gadget in cooked mode */
    struct MsgPort *break_port;  /* who gets Ctrl-C/D/E/F */
    /* termios mode (ACTION_VTCON_TCSETA): a Unix program's line discipline */
    ldisc ld;
    int tty;                     /* in termios mode */
    struct Task *tty_owner;      /* the task that set it; its end ends the mode */
    int tty_ocol;                /* output column, for OXTABS */
    struct timerequest *rtimer;  /* VTIME for the first waiting read */
    int rtimer_busy, rtimer_fired;
    vt_u8 obuf[2048];            /* OPOST output of one chunk */
    struct Task *break_owner;    /* break_port's task, read while it was alive */
    struct MsgPort *home_port;   /* the opener's: the target again when break_port's process ends */
    enum vt_personality pers;
    int latin1;
    int cp437;
    /* bytes ready for Read (complete lines in cooked mode) */
    vt_u8 in[IN_MAX];
    int in_len;
    le_line le;                  /* the line being edited (cooked mode) */
    struct DosPacket *reads[READ_Q];
    int nreads;
    struct DosPacket *waitchar;
    struct MsgPort *timer_port;
    struct timerequest *timer;
    int timer_open, timer_busy;
    struct IOStdReq *rom_io;     /* ROM console unit for DISK_INFO callers */
    struct MsgPort *input_port;  /* input.device: size events for ixemul's SIGWINCH */
    struct IOStdReq *input_io;
    struct Interrupt winch_irq;  /* our input handler (priority 20), see post_sizewindow */
    struct InputEvent winch_ev;  /* the size event it adds to the chain */
    volatile UBYTE winch_pending;
    UBYTE winch_added;
    struct MsgPort *rom_port;
    /* window spec */
    WORD wx, wy, ww, wh;
    char title[80];
    char screen[64];
    char fontname[40];
    WORD fontsize;
    /* FONT1..FONT9, FRAKTUR: the fonts SGR 11-19 and 20 draw with */
    char altname[11][40];
    WORD altsize[11];
    struct TextFont *alt[11];
    WORD want_cols;              /* DECCOLM asked for this width (0: none) */
    ULONG wflags;
    int inactive;
    ULONG fg_rgb, bg_rgb;        /* DARK / FG / BG options, VR_KEEP = the screen's */
    int layout_dirty;
    int render_pending;          /* the grid is ahead of the screen */
    struct MsgPort *frame_port;  /* the frame clock (timer.device) */
    struct timerequest *frame;
    int frame_open, frame_busy;            /* a CSI t/u/x/y changed the text area */
    int auto_open;               /* AUTO: no window until the first read or write */
    int spec_parsed;
    struct Window *foreign;      /* WINDOW 0xaddr: not ours to open or close */
    struct MsgPort *comp_port;   /* Tab completion and command lookups answer here */
    struct complete_req *comp;   /* Tab */
    struct complete_req *check;  /* is the first word a command */
    struct complete_req *hist;   /* history file: load at open, append per line */
    int hist_busy;
    char hist_queue[512];        /* lines waiting for the file, '\n'-ended */
    int hist_queue_len;
    int comp_busy, check_busy;
    char checked[64];            /* the first word last sent to check */
    char *words[3];              /* ACTION_VTCON_WORDS lists by kind (1, 2), AllocVec'd */
    long words_len[3];
    struct Task *words_owner;    /* the shell that sent them: valid while it lives */
    int tabs;                    /* Tabs in a row */
    unsigned long edits;         /* keys that changed the line so far */
    unsigned long comp_edits;    /* edits when the running completion started */
    char menu[COMPLETE_NAMES];   /* the last completion's names */
    int menu_len, menu_n, menu_i, menu_start;
    ULONG foreign_idcmp;         /* its IDCMP before we added ours, to restore */
    struct DeviceNode *node;     /* our DOS device node (from the startup packet) */
    int ever_opened;
    int dragging, drag_moved;    /* mouse selection */
    int drag_ax, drag_ay;
    struct IOStdReq lib_io;      /* console.device CONU_LIBRARY, for RawKeyConvert */
#ifdef VTCON_DEBUG
    ULONG prof_out, prof_damage, prof_scroll, prof_writes, prof_bytes, prof_ndamage, prof_nscroll;
#endif
} con;

/* No mutable globals below this line except the library bases (the same
 * value in every process): every XCON: window is its own process running
 * this one loaded segment, so static state would be shared between them
 * (a second window zeroed the first one's state, 2026-09-29). Each process
 * allocates its con in handler_main. */

#ifdef VTCON_DEBUG
/* Debug trace (build with DEBUG=1) to RAM:vtcon.log. Lines collect in
 * memory and reach the file only from the main loop, between packets:
 * DOS holds the DosList lock while it waits for our reply to an Open, so
 * any DOS call made while handling one deadlocks. */
/* debug builds only: shared by all windows' processes, so trace one window */
static char dbg_buf[2048];
static int dbg_len;
static BPTR dbg_file;
static struct MsgPort *dbg_port;
static void dbg_flush(void);

static void dbg(const char *what, LONG a, LONG b)
{
    int i;
    LONG v[2];
    v[0] = a;
    v[1] = b;
    if (dbg_len > (int)sizeof(dbg_buf) - 80)
        return;
    for (i = 0; what[i] && i < 40; i++)
        dbg_buf[dbg_len++] = what[i];
    for (i = 0; i < 2; i++) {
        ULONG x = (ULONG)v[i];
        int k;
        dbg_buf[dbg_len++] = ' ';
        for (k = 28; k >= 0; k -= 4)
            dbg_buf[dbg_len++] = "0123456789abcdef"[(x >> k) & 15];
    }
    dbg_buf[dbg_len++] = '\n';
    if (dbg_file && dbg_port && IsListEmpty(&dbg_port->mp_MsgList))
        dbg_flush(); /* safe now: no queued packet for the Write to swallow */
}

/* The log file is opened at startup (no Open pending then) and written only
 * from the main loop when our port is empty: a DOS call made by the handler
 * waits for its reply on our own port and would take a queued packet for it
 * (that swallowed the first Open, 2026-09-29). */
static LONG dbg_total;

static void dbg_flush(void)
{
    if (!dbg_len)
        return;
    if (dbg_total > 32768) { /* a runaway trace must not fill RAM: */
        dbg_len = 0;
        return;
    }
    dbg_total += dbg_len;
    if (!dbg_file) {
        dbg_file = Open((STRPTR)"RAM:vtcon.log", MODE_READWRITE);
        if (dbg_file)
            Seek(dbg_file, 0, OFFSET_END);
    }
    if (dbg_file)
        Write(dbg_file, dbg_buf, dbg_len);
    dbg_len = 0;
}
#define DBG(w, a, b) dbg(w, (LONG)(a), (LONG)(b))
#define DBG_FLUSH() dbg_flush()
#else
#define DBG(w, a, b)
#define DBG_FLUSH()
#endif

static void reply(struct DosPacket *p, LONG r1, LONG r2)
{
    if (p->dp_Type != ACTION_WRITE && p->dp_Type != ACTION_READ)
        DBG("reply", p->dp_Type, r1);
    ReplyPkt(p, r1, r2);
}

/* ---- engine callbacks ------------------------------------------------------ */

static void in_append(con *c, const vt_u8 *b, int n)
{
    if (c->in_len + n > IN_MAX)
        n = IN_MAX - c->in_len;
    if (n > 0) {
        CopyMem((APTR)b, c->in + c->in_len, n);
        c->in_len += n;
    }
}

static void cb_damage(void *u, int x0, int y0, int x1, int y1)
{
#ifdef VTCON_DEBUG
    con *c = (con *)u;
    struct EClockVal e0, e1;
    ReadEClock(&e0);
#endif
    vr_damage(&((con *)u)->r, x0, y0, x1, y1);
#ifdef VTCON_DEBUG
    ReadEClock(&e1);
    c->prof_damage += e1.ev_lo - e0.ev_lo;
    c->prof_ndamage++;
#endif
}

static void cb_scroll(void *u, int top, int bot, int n)
{
#ifdef VTCON_DEBUG
    con *c = (con *)u;
    struct EClockVal e0, e1;
    ReadEClock(&e0);
#endif
    vr_scroll(&((con *)u)->r, top, bot, n);
#ifdef VTCON_DEBUG
    ReadEClock(&e1);
    c->prof_scroll += e1.ev_lo - e0.ev_lo;
    c->prof_nscroll++;
#endif
}

static void cb_reply(void *u, const vt_u8 *b, long n)
{
    /* reports enter the read stream, as the console's do */
    in_append((con *)u, b, (int)n);
}

static void cb_bell(void *u)
{
    con *c = (con *)u;
    DisplayBeep(c->win ? c->win->WScreen : 0);
}

static void cb_title(void *u, const char *s)
{
    con *c = (con *)u;
    int i;
    /* the title arrives as UTF-8; Intuition shows Latin-1 */
    for (i = 0; *s && i < (int)sizeof(c->title) - 1; s++) {
        unsigned char b = (unsigned char)*s;
        if (b < 0x80) {
            c->title[i++] = (char)b;
        } else if ((b & 0xE0) == 0xC0 && s[1]) {
            unsigned cp = ((b & 0x1F) << 6) | (s[1] & 0x3F);
            c->title[i++] = (char)(cp < 0x100 ? cp : '?');
            s++;
        } else if ((b & 0xC0) != 0x80) {
            c->title[i++] = '?';
        }
    }
    c->title[i] = 0;
    if (c->win)
        SetWindowTitles(c->win, (UBYTE *)c->title, (UBYTE *)~0);
}

static void resize(con *c);
static void sync_size(con *c);
static void post_sizewindow(con *c);

/* The engine's idea of the default colours: what the pens show. */
static void report_defaults(con *c)
{
    ULONG fg = vr_pen_rgb(&c->r, c->r.pen_default_fg), bg = vr_pen_rgb(&c->r, c->r.pen_default_bg);
    vt_set_default_colors(c->t, fg, bg, fg);
}

/* A program changed the palette or the default colours (OSC 4, 10-12):
 * new pens, then the whole window redrawn (the engine marked it). */
static void cb_colors(void *u)
{
    con *c = (con *)u;
    ULONG fg = vt_default_color(c->t, 0), bg = vt_default_color(c->t, 1);
    vr_palette_changed(&c->r);
    vr_set_defaults(&c->r, fg, bg);
}

/* Amiga page length, line length and offsets (CSI t / u / x / y): the text
 * area changes, as the console recomputes it (-1: back to automatic). The
 * resize happens after the write that asked for it (see output()). */
static void cb_layout(void *u, int which, int value)
{
    con *c = (con *)u;
    switch (which) {
    case VT_LAYOUT_PAGE_LENGTH: c->r.lay_rows = (WORD)value; break;
    case VT_LAYOUT_LINE_LENGTH: c->r.lay_cols = (WORD)value; break;
    case VT_LAYOUT_LEFT_OFFSET: c->r.lay_x = (WORD)value; break;
    case VT_LAYOUT_TOP_OFFSET: c->r.lay_y = (WORD)value; break;
    case VT_LAYOUT_COLUMNS: c->want_cols = (WORD)value; break;
    default: return;
    }
    c->layout_dirty = 1;
}

/* ---- the window -------------------------------------------------------------- */

static int str_ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'a' && x <= 'z')
            x -= 32;
        if (y >= 'a' && y <= 'z')
            y -= 32;
        if (x != y)
            return 0;
    }
    return *a == *b;
}

static int str_ipre(const char *s, const char *pre, const char **rest)
{
    while (*pre) {
        char x = *s, y = *pre;
        if (x >= 'a' && x <= 'z')
            x -= 32;
        if (x != y)
            return 0;
        s++;
        pre++;
    }
    if (*s && *s != ' ')
        return 0;
    while (*s == ' ')
        s++;
    *rest = s;
    return 1;
}

static void copy_str(char *d, const char *s, int max)
{
    int i;
    for (i = 0; s[i] && i < max - 1; i++)
        d[i] = s[i];
    d[i] = 0;
}

/* "rrggbb" (hex) or VR_KEEP when unreadable. */
static ULONG parse_rgb(const char *s)
{
    ULONG v = 0;
    int n = 0;
    if (*s == '#')
        s++;
    for (; *s && n < 6; s++, n++) {
        char h = *s;
        if (h >= '0' && h <= '9') v = v * 16 + (ULONG)(h - '0');
        else if (h >= 'a' && h <= 'f') v = v * 16 + (ULONG)(h - 'a' + 10);
        else if (h >= 'A' && h <= 'F') v = v * 16 + (ULONG)(h - 'A' + 10);
        else return VR_KEEP;
    }
    return n == 6 ? v : VR_KEEP;
}

/* FONT1..FONT9 give 1-9, FRAKTUR 10; 0 for any other option. */
static int alt_font_option(const char *field, const char **rest)
{
    char pre[6];
    int k;
    if (str_ipre(field, "FRAKTUR", rest))
        return 10;
    copy_str(pre, "FONT0", sizeof(pre));
    for (k = 1; k <= 9; k++) {
        pre[4] = (char)('0' + k);
        if (str_ipre(field, pre, rest))
            return k;
    }
    return 0;
}

/* "name.font size" of a font option. */
static void parse_font(const char *rest, char *name, int max, WORD *size)
{
    int i = 0;
    while (*rest == ' ')
        rest++;
    while (rest[i] && rest[i] != ' ' && i < max - 1) {
        name[i] = rest[i];
        i++;
    }
    name[i] = 0;
    rest += i;
    while (*rest == ' ')
        rest++;
    *size = 0;
    while (*rest >= '0' && *rest <= '9')
        *size = (WORD)(*size * 10 + (*rest++ - '0'));
}

/* "x/y/w/h/title/OPT/OPT..." after the colon. */
static void parse_spec(con *c, const char *s)
{
    char field[128];
    int fno = 0, k, colours = 0;
    c->wx = 0;
    c->wy = 0;
    c->ww = 640;
    c->wh = 200;
    copy_str(c->title, "vtcon", sizeof(c->title));
    c->pers = VT_XTERM;
    c->fg_rgb = c->bg_rgb = VR_KEEP;
    c->wflags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_SIZEGADGET | WFLG_SIZEBRIGHT |
                WFLG_ACTIVATE | WFLG_SMART_REFRESH;
    for (;;) {
        int n = 0;
        const char *rest;
        while (*s && *s != '/' && n < (int)sizeof(field) - 1) {
            if (*s == '\\' && s[1])
                s++;
            field[n++] = *s++;
        }
        field[n] = 0;
        if (fno < 4) {
            LONG v = 0;
            const char *p = field;
            int any = 0;
            while (*p >= '0' && *p <= '9') {
                v = v * 10 + (*p++ - '0');
                any = 1;
            }
            if (any) {
                if (fno == 0)
                    c->wx = (WORD)v;
                else if (fno == 1)
                    c->wy = (WORD)v;
                else if (fno == 2)
                    c->ww = (WORD)v;
                else
                    c->wh = (WORD)v;
            }
        } else if (fno == 4) {
            if (n)
                copy_str(c->title, field, sizeof(c->title));
        } else if (str_ieq(field, "CLOSE")) {
            c->wflags |= WFLG_CLOSEGADGET;
        } else if (str_ieq(field, "WAIT")) {
            c->wait_close = 1;
        } else if (str_ieq(field, "BACKDROP")) {
            c->wflags |= WFLG_BACKDROP | WFLG_BORDERLESS;
            c->wflags &= ~(WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_SIZEGADGET);
        } else if (str_ieq(field, "NODRAG")) {
            c->wflags &= ~WFLG_DRAGBAR;
        } else if (str_ieq(field, "NOBORDER")) {
            c->wflags |= WFLG_BORDERLESS;
        } else if (str_ieq(field, "NOSIZE")) {
            c->wflags &= ~(WFLG_SIZEGADGET | WFLG_SIZEBRIGHT);
        } else if (str_ieq(field, "NODEPTH")) {
            c->wflags &= ~WFLG_DEPTHGADGET;
        } else if (str_ieq(field, "INACTIVE")) {
            c->wflags &= ~WFLG_ACTIVATE;
        } else if (str_ieq(field, "AUTO")) {
            c->auto_open = 1; /* the window opens on the first read or write */
        } else if (str_ipre(field, "WINDOW", &rest)) {
            /* WINDOW 0xaddr: draw into a window someone else opened */
            ULONG v = 0;
            if (rest[0] == '0' && (rest[1] == 'x' || rest[1] == 'X'))
                rest += 2;
            for (; *rest; rest++) {
                char h = *rest;
                if (h >= '0' && h <= '9') v = v * 16 + (ULONG)(h - '0');
                else if (h >= 'a' && h <= 'f') v = v * 16 + (ULONG)(h - 'a' + 10);
                else if (h >= 'A' && h <= 'F') v = v * 16 + (ULONG)(h - 'A' + 10);
                else break;
            }
            c->foreign = (struct Window *)v;
        } else if (str_ieq(field, "SIMPLE") || str_ieq(field, "SMART")) {
            /* SMART refresh always: the cell grid redraws on refresh anyway */
        } else if (str_ieq(field, "XTERM")) {
            c->pers = VT_XTERM;
        } else if (str_ieq(field, "AMIGA")) {
            c->pers = VT_AMIGA;
        } else if (str_ieq(field, "PCANSI")) {
            c->pers = VT_PCANSI;
        } else if (str_ieq(field, "LATIN1")) {
            c->latin1 = 1;
        } else if (str_ieq(field, "DARK")) {
            c->fg_rgb = 0xC0C0C0UL; /* light grey on black */
            c->bg_rgb = 0x000000UL;
            colours = 1;
        } else if (str_ieq(field, "LIGHT")) {
            colours = 1; /* the screen's text and background pens */
        } else if (str_ipre(field, "FG", &rest)) {
            c->fg_rgb = parse_rgb(rest);
            colours = 1;
        } else if (str_ipre(field, "BG", &rest)) {
            c->bg_rgb = parse_rgb(rest);
            colours = 1;
        } else if (str_ieq(field, "CP437")) {
            c->cp437 = 1;
        } else if (str_ipre(field, "SCREEN", &rest)) {
            copy_str(c->screen, rest, sizeof(c->screen));
        } else if ((k = alt_font_option(field, &rest)) != 0) {
            /* FONT1..FONT9 name.font size: SGR 11-19; FRAKTUR: SGR 20 */
            parse_font(rest, c->altname[k], sizeof(c->altname[k]), &c->altsize[k]);
        } else if (str_ipre(field, "FONT", &rest)) {
            /* FONT name.font size */
            parse_font(rest, c->fontname, sizeof(c->fontname), &c->fontsize);
        }
        fno++;
        if (*s != '/')
            break;
        s++;
    }
    /* Black background unless the options chose colours (owner, 2026-09-29:
     * program colours sat on the Workbench's grey). The amiga personality
     * keeps the screen's pens: CON: programs draw with pens 0-3 as the
     * Workbench has them. LIGHT asks for the screen's pens anywhere. */
    if (!colours && c->pers != VT_AMIGA) {
        c->fg_rgb = 0xC0C0C0UL;
        c->bg_rgb = 0x000000UL;
    }
}

/* A fixed-width font by name ("topaz" or "topaz.font") and size; 0 if it
 * cannot be opened or is proportional. */
static struct TextFont *open_named(const char *fontname, WORD fontsize)
{
    struct TextFont *f = 0;
    if (fontname[0]) {
        struct TextAttr ta;
        char name[48];
        int i;
        copy_str(name, fontname, sizeof(name) - 6);
        for (i = 0; name[i]; i++)
            ;
        if (i < 5 || !str_ieq(name + i - 5, ".font"))
            copy_str(name + i, ".font", 6);
        ta.ta_Name = (STRPTR)name;
        ta.ta_YSize = (UWORD)(fontsize ? fontsize : 8);
        ta.ta_Style = 0;
        ta.ta_Flags = 0;
        f = OpenFont(&ta);
        if ((!f || f->tf_YSize != ta.ta_YSize) && DiskfontBase) {
            if (f)
                CloseFont(f);
            f = OpenDiskFont(&ta);
        }
        if (f && (f->tf_Flags & FPF_PROPORTIONAL)) {
            CloseFont(f);
            f = 0;
        }
    }
    return f;
}

static struct TextFont *open_font(con *c)
{
    struct TextFont *f = open_named(c->fontname, c->fontsize);
    if (f) {
        c->font_opened = 1;
        return f;
    }
    /* The system default font: the one the user chose for text, as the
     * Shell uses it. It is fixed width by definition. */
    return GfxBase->DefaultFont;
}

static void le_out(void *u, const unsigned char *b, long n);
static void history_load(con *c);

static int open_window(con *c)
{
    struct Screen *scr;
    struct TagItem tags[14];
    int n = 0;
    struct vt_callbacks cb;

    DBG("lockpub", 0, 0);
    scr = LockPubScreen(c->screen[0] ? (UBYTE *)c->screen : 0);
    if (!scr)
        scr = LockPubScreen(0);
    if (!scr)
        return 0;
    c->locked = scr;
    c->font = open_font(c);
    if (c->foreign) {
        /* WINDOW 0xaddr: use it as it is; add the IDCMP we need */
        c->win = c->foreign;
        c->foreign_idcmp = c->win->IDCMPFlags;
        if (!ModifyIDCMP(c->win, c->foreign_idcmp | IDCMP_RAWKEY | IDCMP_NEWSIZE |
                         IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS |
                         IDCMP_MOUSEMOVE))
            return 0;
        SetFont(c->win->RPort, c->font);
        goto have_window;
    }
    tags[n].ti_Tag = WA_Left;        tags[n++].ti_Data = c->wx;
    tags[n].ti_Tag = WA_Top;         tags[n++].ti_Data = c->wy;
    tags[n].ti_Tag = WA_Width;       tags[n++].ti_Data = c->ww;
    tags[n].ti_Tag = WA_Height;      tags[n++].ti_Data = c->wh;
    tags[n].ti_Tag = WA_Title;       tags[n++].ti_Data = (ULONG)c->title;
    tags[n].ti_Tag = WA_Flags;       tags[n++].ti_Data = c->wflags;
    tags[n].ti_Tag = WA_IDCMP;       tags[n++].ti_Data = IDCMP_RAWKEY | IDCMP_NEWSIZE |
        IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_ACTIVEWINDOW |
        IDCMP_INACTIVEWINDOW;
    tags[n].ti_Tag = WA_PubScreen;   tags[n++].ti_Data = (ULONG)scr;
    tags[n].ti_Tag = WA_MinWidth;    tags[n++].ti_Data = 80;
    tags[n].ti_Tag = WA_MinHeight;   tags[n++].ti_Data = 40;
    tags[n].ti_Tag = WA_MaxWidth;    tags[n++].ti_Data = (ULONG)~0;
    tags[n].ti_Tag = WA_MaxHeight;   tags[n++].ti_Data = (ULONG)~0;
    tags[n].ti_Tag = WA_AutoAdjust;  tags[n++].ti_Data = TRUE;
    tags[n].ti_Tag = TAG_DONE;       tags[n].ti_Data = 0;
    DBG("openwindow", scr, c->font);
    c->win = OpenWindowTagList(0, tags);
    DBG("window", c->win, 0);
    if (!c->win)
        return 0;
have_window:

    cb.damage = cb_damage;
    cb.scroll = cb_scroll;
    cb.reply = cb_reply;
    cb.bell = cb_bell;
    cb.title = cb_title;
    cb.layout = cb_layout;
    cb.colors = cb_colors;
    {
        /* size from the window before the engine exists */
        struct Window *w = c->win;
        int cols = (w->Width - w->BorderLeft - w->BorderRight) / c->font->tf_XSize;
        int rows = (w->Height - w->BorderTop - w->BorderBottom) / c->font->tf_YSize;
        c->t = vt_new(cols > 0 ? cols : 1, rows > 0 ? rows : 1, 500, &cb, c);
    }
    if (!c->t)
        return 0;
    DBG("vt_new", c->t, 0);
    vt_set_personality(c->t, c->pers);
    if (c->pers == VT_XTERM)
        vt_set_onlcr(c->t, 1); /* LF out as CR LF, as a Unix tty does */
    if (c->pers == VT_XTERM && c->latin1)
        vt_set_charset(c->t, VT_CS_LATIN1);
    else if (c->pers == VT_XTERM && c->cp437)
        vt_set_charset(c->t, VT_CS_CP437);
    vr_init(&c->r, c->win, c->font, c->t, c->pers == VT_PCANSI ? VT_ENC_CP437 : VT_ENC_LATIN1);
    {
        int k;
        for (k = 1; k <= 10; k++) {
            if (!c->alt[k] && c->altname[k][0])
                c->alt[k] = open_named(c->altname[k], c->altsize[k] ? c->altsize[k] : c->font->tf_YSize);
            vr_set_alt_font(&c->r, k, c->alt[k]);
            if (c->altname[k][0])
                DBG("altfont", k, c->r.alt_font[k] ? (LONG)c->r.alt_font[k] : -(LONG)c->alt[k] - 1);
        }
    }
    vr_set_defaults(&c->r, c->fg_rgb, c->bg_rgb);
    report_defaults(c);
    vt_set_cell_pixels(c->t, c->font->tf_XSize, c->font->tf_YSize);
    le_init(&c->le, c->t, le_out, c);
    c->le.utf8 = c->pers == VT_XTERM && !c->latin1 && !c->cp437;
    history_load(c); /* the saved history, read by a worker */
    DBG("vr_init", c->r.cols, c->r.rows);
    vr_redraw(&c->r);
    DBG("redrawn", 0, 0);
    vr_cursor_on(&c->r);
    return 1;
}

static void close_window(con *c)
{
    if (c->input_io) {
        if (c->winch_added) {
            c->input_io->io_Command = IND_REMHANDLER;
            c->input_io->io_Data = &c->winch_irq;
            DoIO((struct IORequest *)c->input_io);
            c->winch_added = 0;
        }
        CloseDevice((struct IORequest *)c->input_io);
        DeleteIORequest((struct IORequest *)c->input_io);
        c->input_io = 0;
    }
    if (c->input_port) {
        DeleteMsgPort(c->input_port);
        c->input_port = 0;
    }
    if (c->rom_io) {
        CloseDevice((struct IORequest *)c->rom_io);
        DeleteIORequest((struct IORequest *)c->rom_io);
        c->rom_io = 0;
    }
    if (c->rom_port) {
        DeleteMsgPort(c->rom_port);
        c->rom_port = 0;
    }
    if (c->t) {
        vr_free(&c->r);
        vt_free(c->t);
        c->t = 0;
    }
    if (c->win) {
        if (c->win == c->foreign)
            ModifyIDCMP(c->win, c->foreign_idcmp); /* hand it back as we found it */
        else
            CloseWindow(c->win);
        c->win = 0;
    }
    if (c->font && c->font_opened)
        CloseFont(c->font);
    c->font = 0;
    {
        int k;
        for (k = 1; k <= 10; k++)
            if (c->alt[k]) {
                CloseFont(c->alt[k]);
                c->alt[k] = 0;
            }
    }
    if (c->locked) {
        UnlockPubScreen(0, c->locked);
        c->locked = 0;
    }
}

/* ---- output ---------------------------------------------------------------- */

#define FRAME_MICROS 50000 /* 20 frames per second */

static void frame_start(con *c);

/* Draw what the grid has that the screen has not. */
static void render(con *c)
{
    if (!c->render_pending || !c->t)
        return;
    c->render_pending = 0;
    vr_cursor_off(&c->r);
    vt_flush(c->t);
    vr_cursor_on(&c->r);
    if (c->r.has_blink || vr_cursor_blinks(&c->r))
        frame_start(c); /* blinking cells or cursor: the frames keep coming */
}

static void frame_start(con *c)
{
    if (!c->frame_open || c->frame_busy)
        return;
    c->frame->tr_node.io_Command = TR_ADDREQUEST;
    c->frame->tr_time.tv_secs = 0;
    c->frame->tr_time.tv_micro = FRAME_MICROS;
    SendIO((struct IORequest *)c->frame);
    c->frame_busy = 1;
}

static void output(con *c, const vt_u8 *b, long n)
{
#ifdef VTCON_DEBUG
    struct EClockVal e0, e1;
    ReadEClock(&e0);
#endif
    if (!c->t)
        return;
    if (c->r.view) {
        /* new output shows the live screen, as xterm does; the engine's
         * pending batch goes first (ignored by the renderer while the view
         * is back), then the view redraws from the grid */
        render(c);
        vr_set_view(&c->r, 0);
    }
    if (!c->le.len)
        c->le.started = 0; /* the next line starts wherever this output ends */
    /* Frame-paced: the grid changes now, the screen at the next frame
     * (render()), so a flood of one-line writes costs one scroll blit
     * per frame instead of one per line (the chip bus of a 4-plane hires
     * screen could not keep up: 45 ms per scroll, cycle-exact rig). */
    vt_feed(c->t, b, n);
    c->render_pending = 1;
    if (c->layout_dirty) {
        c->layout_dirty = 0;
        render(c);
        if (c->want_cols && c->win) {
            /* DECCOLM: the window as wide as that many columns (as far
             * as the screen allows); its new size resizes the grid */
            struct Window *w = c->win;
            WORD width = (WORD)(c->want_cols * c->font->tf_XSize + w->BorderLeft + w->BorderRight);
            if (width > w->WScreen->Width - w->LeftEdge)
                width = (WORD)(w->WScreen->Width - w->LeftEdge);
            c->want_cols = 0;
            ChangeWindowBox(w, w->LeftEdge, w->TopEdge, width, w->Height);
        } else {
            resize(c); /* not inside vt_feed: the engine is mid-parse there */
        }
    }
    frame_start(c);
#ifdef VTCON_DEBUG
    ReadEClock(&e1);
    c->prof_out += e1.ev_lo - e0.ev_lo;
    c->prof_writes++;
    c->prof_bytes += n;
#endif
}

/* ---- reads ------------------------------------------------------------------ */

static void finish_waitchar(con *c, LONG result)
{
    if (!c->waitchar)
        return;
    if (c->timer_busy) {
        AbortIO((struct IORequest *)c->timer);
        WaitIO((struct IORequest *)c->timer);
        c->timer_busy = 0;
    }
    reply(c->waitchar, result, 0);
    c->waitchar = 0;
}

static int line_end(con *c)
{
    int i;
    for (i = 0; i < c->in_len; i++)
        if (c->in[i] == '\n')
            return i + 1;
    return 0;
}

/* Answer queued reads with what the input holds. */
/* ---- termios mode ----------------------------------------------------------- */

static void output(con *c, const vt_u8 *b, long n);
static void send_break(con *c, ULONG sig);

static void tty_echo(void *u, const unsigned char *s, int n)
{
    output((con *)u, s, n);
}

static void tty_signal(void *u, int sig)
{
    con *c = (con *)u;
    DBG("tty signal", sig, (long)c->tty_owner);
    /* the break signals a patched ixemul turns into Unix signals for the
     * foreground process group (vtcon_packets.h) */
    if (sig == LD_SIGINT)
        send_break(c, SIGBREAKF_CTRL_C);
    else if (sig == LD_SIGQUIT)
        send_break(c, SIGBREAKF_CTRL_E);
    else if (sig == LD_SIGTSTP)
        send_break(c, SIGBREAKF_CTRL_F);
}

static void rtimer_stop(con *c)
{
    if (c->rtimer_busy) {
        AbortIO((struct IORequest *)c->rtimer);
        WaitIO((struct IORequest *)c->rtimer);
        c->rtimer_busy = 0;
    }
    c->rtimer_fired = 0;
}

static void rtimer_start(con *c, ULONG tenths)
{
    rtimer_stop(c);
    if (!c->rtimer)
        return;
    c->rtimer->tr_node.io_Command = TR_ADDREQUEST;
    c->rtimer->tr_time.tv_secs = tenths / 10;
    c->rtimer->tr_time.tv_micro = (tenths % 10) * 100000;
    SendIO((struct IORequest *)c->rtimer);
    c->rtimer_busy = 1;
}

/* Leave termios mode: what the line discipline holds becomes plain input. */
static void tty_leave(con *c)
{
    unsigned char b[256];
    long n;
    int eof;
    if (!c->tty)
        return;
    c->ld.t.c_lflag &= ~(ld_flag)LD_ICANON; /* whole lines and a partial one, as bytes */
    ld_set(&c->ld, &c->ld.t, LD_TCSANOW);
    while ((n = ld_read(&c->ld, b, sizeof(b), &eof)) > 0)
        in_append(c, b, (int)n);
    rtimer_stop(c);
    c->tty = 0;
    c->tty_owner = 0;
    if (c->t)
        vt_set_onlcr(c->t, c->pers == VT_XTERM);
}

/* In termios mode, while the program that set it runs. */
static int tty_active(con *c)
{
    int alive;
    if (!c->tty)
        return 0;
    Forbid();
    alive = task_alive(c->tty_owner);
    Permit();
    if (!alive)
        tty_leave(c);
    return c->tty;
}

static void tty_enter(con *c, struct Task *owner)
{
    if (!c->tty) {
        /* typed ahead: input as it stands, not echoed again */
        void (*e)(void *, const unsigned char *, int) = c->ld.echo;
        ld_init(&c->ld);
        c->ld.echo = 0;
        c->ld.t.c_lflag &= ~(ld_flag)LD_ISIG;
        ld_input(&c->ld, c->in, c->in_len);
        c->in_len = 0;
        ld_defaults(&c->ld.t);
        c->ld.echo = e;
        c->tty = 1;
        c->tty_ocol = 0;
        if (c->t)
            vt_set_onlcr(c->t, 0); /* OPOST decides now (vttest m1 s04: raw LF is LF) */
    }
    c->tty_owner = owner;
}

/* Reads in termios mode: ICANON a line or an EOF; otherwise VMIN bytes,
 * or what came within VTIME tenths of a second. */
static void tty_reads(con *c)
{
    int canon = (c->ld.t.c_lflag & LD_ICANON) != 0;
    int vmin = c->ld.t.c_cc[LD_VMIN], vtime = c->ld.t.c_cc[LD_VTIME];
    while (c->nreads) {
        struct DosPacket *p = c->reads[0];
        long n;
        int eof, i;
        if (ld_read_ready(&c->ld) && (canon || vmin > 0 || c->ld.qlen > 0)) {
            n = ld_read(&c->ld, (unsigned char *)p->dp_Arg2, p->dp_Arg3, &eof);
        } else if (!canon && vmin == 0 && vtime == 0) {
            n = 0; /* a poll: nothing there */
        } else if (!canon && vtime > 0 && c->rtimer_fired) {
            n = ld_read(&c->ld, (unsigned char *)p->dp_Arg2, p->dp_Arg3, &eof);
        } else {
            /* wait; VTIME counts from now (VMIN 0) or from the first byte */
            if (!canon && vtime > 0 && !c->rtimer_busy && (vmin == 0 || c->ld.qlen > 0))
                rtimer_start(c, (ULONG)vtime);
            break;
        }
        rtimer_stop(c);
        reply(p, n < 0 ? 0 : n, 0);
        for (i = 1; i < c->nreads; i++)
            c->reads[i - 1] = c->reads[i];
        c->nreads--;
    }
    if (c->waitchar && (canon ? c->ld.lines > 0 : c->ld.qlen > 0))
        finish_waitchar(c, DOSTRUE);
}

static void service_reads(con *c)
{
    if (tty_active(c)) {
        tty_reads(c);
        return;
    }
    while (c->nreads) {
        struct DosPacket *p = c->reads[0];
        LONG want = p->dp_Arg3, n;
        int i;
        if (c->in_len) {
            n = c->raw ? c->in_len : line_end(c);
            if (!n && !c->raw)
                n = c->in_len; /* a report or a mode switch left bytes without a newline */
        } else if (c->eof) {
            n = 0;
            c->eof = 0;
            reply(p, 0, 0);
            goto next;
        } else {
            return;
        }
        if (n > want)
            n = want;
        CopyMem(c->in, (APTR)p->dp_Arg2, n);
        for (i = n; i < c->in_len; i++)
            c->in[i - n] = c->in[i];
        c->in_len -= (int)n;
        reply(p, n, 0);
    next:
        for (i = 1; i < c->nreads; i++)
            c->reads[i - 1] = c->reads[i];
        c->nreads--;
    }
    if (c->waitchar && c->in_len)
        finish_waitchar(c, DOSTRUE);
}

/* ---- cooked line editing ------------------------------------------------------ */

static void echo(con *c, const vt_u8 *b, int n)
{
    output(c, b, n);
}

static void le_out(void *u, const unsigned char *b, long n)
{
    output((con *)u, b, n);
}

/* Tab in the cooked line. The first Tab sends the word before the cursor
 * to a worker (complete.c): the first word of the line completes against
 * commands, any other against file names. A second Tab lists the matches
 * under the line, further Tabs cycle through them (zsh's menu). */
static int word_start(const le_line *le)
{
    int a = le->pos;
    while (a > 0 && le->buf[a - 1] != ' ' && le->buf[a - 1] != '"' && le->buf[a - 1] != '=')
        a--;
    return a;
}

static int copy_latin1(const le_line *le, int a, int b, char *out, int max)
{
    int i, k = 0;
    for (i = a; i < b && k < max - 1; i++) {
        unsigned char ch = le->buf[i];
        if (le->utf8 && (ch & 0xE0) == 0xC0 && i + 1 < b) {
            ch = (unsigned char)(((ch & 0x1F) << 6) | (le->buf[i + 1] & 0x3F));
            i++; /* file names are Latin-1 */
        }
        out[k++] = (char)ch;
    }
    out[k] = 0;
    return k;
}

static int ensure_worker(con *c)
{
    if (!c->comp_port)
        c->comp_port = CreateMsgPort();
    if (!c->comp)
        c->comp = (struct complete_req *)AllocVec(sizeof(struct complete_req), MEMF_CLEAR);
    if (!c->check)
        c->check = (struct complete_req *)AllocVec(sizeof(struct complete_req), MEMF_CLEAR);
    if (!c->hist)
        c->hist = (struct complete_req *)AllocVec(sizeof(struct complete_req), MEMF_CLEAR);
    return c->comp_port && c->comp && c->check && c->hist;
}

/* The process Ctrl-C goes to and whose directory completion uses: the
 * last ACTION_CHANGE_SIGNAL target while it lives (a program the Shell
 * runs may set itself and end without handing it back: the handler then
 * signalled, and the completion worker read, a process that was gone --
 * #80000008 on the rig), else the process that opened the window. Call
 * under Forbid to use the answer. */
static struct Task *break_task(con *c)
{
    if (c->break_port && task_alive(c->break_owner))
        return c->break_owner;
    c->break_port = c->home_port;
    c->break_owner = 0;
    if (c->home_port && task_alive((struct Task *)c->home_port->mp_SigTask))
        return (struct Task *)c->home_port->mp_SigTask;
    return 0;
}

static struct Process *opener(con *c)
{
    struct Task *t;
    Forbid();
    t = break_task(c);
    Permit();
    return (struct Process *)t;
}

static void forget_words(con *c)
{
    int k;
    for (k = 1; k <= 2; k++) {
        if (c->words[k])
            FreeVec(c->words[k]);
        c->words[k] = 0;
        c->words_len[k] = 0;
    }
    c->words_owner = 0;
}

/* The shell's list of kind (VTCON_WORDS_*), while the shell that sent it
 * runs; 0 otherwise (a shell that ended takes its words with it). */
static const char *shell_words(con *c, int kind, long *len)
{
    int alive;
    Forbid();
    alive = task_alive(c->words_owner);
    Permit();
    if (!alive) {
        if (c->words_owner)
            forget_words(c);
        return 0;
    }
    *len = c->words_len[kind];
    return c->words[kind];
}

static int in_words(const char *list, long len, const char *w)
{
    long k = 0;
    while (list && k < len) {
        if (!strcmp(list + k, w))
            return 1;
        k += (long)strlen(list + k) + 1;
    }
    return 0;
}

/* ACTION_VTCON_WORDS: a shell's names, kept while it runs. */
static void take_words(con *c, struct DosPacket *p)
{
    int kind = (int)p->dp_Arg2;
    long len = p->dp_Arg4;
    char *copy;
    if (kind < 1 || kind > 2 || len < 0 || !p->dp_Arg3)
        return;
    if (len > VTCON_WORDS_MAX)
        len = VTCON_WORDS_MAX;
    while (len > 0 && ((const char *)p->dp_Arg3)[len - 1])
        len--; /* whole names only */
    copy = (char *)AllocVec(len + 1, MEMF_ANY);
    if (!copy)
        return;
    CopyMem((APTR)p->dp_Arg3, copy, len);
    copy[len] = 0;
    if (c->words_owner != (struct Task *)p->dp_Port->mp_SigTask)
        forget_words(c); /* another shell now: the old one's words go */
    if (c->words[kind])
        FreeVec(c->words[kind]);
    c->words[kind] = copy;
    c->words_len[kind] = len;
    c->words_owner = (struct Task *)p->dp_Port->mp_SigTask;
    c->checked[0] = 0; /* colour the command word again with what it knows */
}

/* Is the word at a in command position? At the line's start, and with a
 * shell attached also after | ; & ( (in the AmigaDOS Shell ; starts a
 * comment, so not without one). */
static int command_position(con *c, int a)
{
    const le_line *le = &c->le;
    long n;
    int k = a;
    while (k > 0 && le->buf[k - 1] == ' ')
        k--;
    if (k == 0)
        return 1;
    return shell_words(c, VTCON_WORDS_COMMANDS, &n) && strchr("|;&(", le->buf[k - 1]) != 0;
}

static void start_completion(con *c)
{
    le_line *le = &c->le;
    int a;
    long n = 0;
    const char *extra = 0;
    if (c->comp_busy || !ensure_worker(c))
        return;
    a = word_start(le);
    c->menu_start = a;
    if (a < le->pos && le->buf[a] == '$') {
        c->comp->mode = COMPLETE_VARS;
        extra = shell_words(c, VTCON_WORDS_VARIABLES, &n);
        c->menu_start = a + (a + 1 < le->pos && le->buf[a + 1] == '{' ? 2 : 1);
    } else if (command_position(c, a)) {
        c->comp->mode = COMPLETE_COMMANDS;
        extra = shell_words(c, VTCON_WORDS_COMMANDS, &n);
    } else {
        c->comp->mode = COMPLETE_FILES;
    }
    c->comp->extra_len = 0;
    if (extra && n > 0 && n <= (long)sizeof(c->comp->extra)) {
        CopyMem((APTR)extra, c->comp->extra, n);
        c->comp->extra_len = n;
    }
    copy_latin1(le, a, le->pos, c->comp->word, COMPLETE_MAX);
    c->comp_edits = c->edits;
    if (complete_start(c->comp, c->comp_port, opener(c)))
        c->comp_busy = 1;
}

/* Is the first word a command? Asked whenever it changes (the answer
 * colours it green or red, see le_set_command). */
static void check_command(con *c)
{
    unsigned char w[64];
    le_first_word(&c->le, w, sizeof(w));
    if (!w[0] || c->check_busy || !strcmp((const char *)w, c->checked))
        return;
    {
        /* a word the shell knows (a function, a builtin): no lookup */
        long n;
        const char *list = shell_words(c, VTCON_WORDS_COMMANDS, &n);
        if (list && in_words(list, n, (const char *)w)) {
            strcpy(c->checked, (const char *)w);
            le_set_command(&c->le, w, 1);
            return;
        }
    }
    if (!ensure_worker(c))
        return;
    copy_latin1(&c->le, 0, c->le.len, c->check->word, COMPLETE_MAX); /* then cut */
    {
        int i = 0;
        char *q = c->check->word;
        while (*q == ' ')
            q++;
        while (q[i] && q[i] != ' ')
            i++;
        memmove(c->check->word, q, i);
        c->check->word[i] = 0;
    }
    strcpy(c->checked, (const char *)w);
    c->check->mode = CHECK_COMMAND;
    if (complete_start(c->check, c->comp_port, opener(c)))
        c->check_busy = 1;
}

/* The history file: loaded once when the window opens, then each entered
 * line appended (one worker at a time; lines queue meanwhile). */
static void history_next(con *c)
{
    int i;
    if (c->hist_busy || !c->hist_queue_len || !ensure_worker(c))
        return;
    for (i = 0; i < c->hist_queue_len && c->hist_queue[i] != '\n' && i < COMPLETE_MAX - 1; i++)
        c->hist->word[i] = c->hist_queue[i];
    c->hist->word[i] = 0;
    while (i < c->hist_queue_len && c->hist_queue[i] != '\n')
        i++;
    i++;
    memmove(c->hist_queue, c->hist_queue + i, c->hist_queue_len - i);
    c->hist_queue_len -= i;
    c->hist->mode = HISTORY_APPEND;
    if (complete_start(c->hist, c->comp_port, opener(c)))
        c->hist_busy = 1;
}

static void history_save(con *c, const unsigned char *line, int n)
{
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == ' '))
        n--;
    if (n <= 0 || c->hist_queue_len + n + 1 > (int)sizeof(c->hist_queue))
        return;
    CopyMem((APTR)line, c->hist_queue + c->hist_queue_len, n);
    c->hist_queue_len += n;
    c->hist_queue[c->hist_queue_len++] = '\n';
    history_next(c);
}

static void history_load(con *c)
{
    if (!ensure_worker(c) || c->hist_busy)
        return;
    c->hist->data = (char *)AllocVec(HISTORY_KEEP * 2 * 256, MEMF_ANY);
    if (!c->hist->data)
        return;
    c->hist->data_max = HISTORY_KEEP * 2 * 256;
    c->hist->mode = HISTORY_LOAD;
    if (complete_start(c->hist, c->comp_port, opener(c)))
        c->hist_busy = 1;
}

static void cooked_key(con *c, const vt_u8 *b, int n, long key, int mods);

static void type_text(con *c, const char *s)
{
    for (; *s; s++) {
        vt_u8 out[8];
        int k = vt_encode_key(c->t, (unsigned char)*s, 0, out);
        if (!c->raw && le_key(&c->le, (unsigned char)*s, 0, out, k)) {
            in_append(c, c->le.buf, c->le.len);
            le_reset(&c->le);
        }
    }
}

static void finish_completion(con *c)
{
    struct complete_req *q;
    while ((q = (struct complete_req *)GetMsg(c->comp_port)) != 0) {
        if (q == c->hist) {
            c->hist_busy = 0;
            if (q->mode == HISTORY_LOAD && q->data) {
                long a = 0, i;
                for (i = 0; i < q->data_len; i++)
                    if (q->data[i] == '\n') {
                        le_hist_add(&c->le, (const unsigned char *)q->data + a, (int)(i - a));
                        a = i + 1;
                    }
                FreeVec(q->data);
                q->data = 0;
            }
            history_next(c);
            continue;
        }
        if (q == c->check) {
            c->check_busy = 0;
            if (!c->raw) {
                unsigned char w[64];
                int i;
                /* the answer is for the Latin-1 word; compare in the
                 * line's encoding, as le_set_command does */
                le_first_word(&c->le, w, sizeof(w));
                for (i = 0; c->checked[i] && w[i] == (unsigned char)c->checked[i]; i++)
                    ;
                if (!c->checked[i] && !w[i])
                    le_set_command(&c->le, w, q->matches);
            }
            check_command(c); /* the word may have changed meanwhile */
            continue;
        }
        c->comp_busy = 0;
        c->menu_n = 0;
        if (c->edits != c->comp_edits)
            continue; /* the line changed while it ran (rig: typed text got the
                       * answer for an older word): the answer is for no word now */
        if (q->matches > 1 && q->names_len < (int)sizeof(c->menu)) {
            CopyMem(q->names, c->menu, q->names_len);
            c->menu_len = q->names_len;
            c->menu_n = q->matches;
            c->menu_i = -1;
        }
        if (!q->add[0] && q->matches != 1) {
            DisplayBeep(c->win ? c->win->WScreen : 0); /* none, or several with no more in common */
            continue;
        }
        type_text(c, q->add);
        check_command(c); /* the completed word gets its colour */
        if (q->matches > 1)
            DisplayBeep(c->win ? c->win->WScreen : 0); /* completed as far as they agree */
    }
}

/* The next Tab after a completion: show the menu, then cycle through it. */
static void menu_tab(con *c)
{
    if (c->tabs == 2) {
        le_show_list(&c->le, c->menu, c->menu_len);
        return;
    }
    {
        int k = 0, i;
        const char *name;
        c->menu_i = (c->menu_i + 1) % c->menu_n;
        for (i = 0; i < c->menu_i; i++)
            k += (int)strlen(c->menu + k) + 1;
        name = c->menu + k;
        {
            /* the name in the line's encoding */
            unsigned char enc[COMPLETE_MAX * 2];
            int n = 0;
            for (; *name && n < (int)sizeof(enc) - 3; name++)
                n += vt_encode_key(c->t, (unsigned char)*name, 0, enc + n);
            le_replace_word(&c->le, word_start(&c->le) < c->menu_start ? word_start(&c->le)
                                                                          : c->menu_start,
                            enc, n);
        }
    }
}

static void cooked_key(con *c, const vt_u8 *b, int n, long key, int mods)
{
    if (key == VT_KEY_TAB) {
        c->tabs++;
        if (c->tabs >= 2 && c->menu_n > 1)
            menu_tab(c);
        else
            start_completion(c);
        return;
    }
    c->tabs = 0;
    c->menu_n = 0;
    c->edits++;
    if (!key && n == 1 && b[0] == 0x1C) { /* Ctrl-\: end of file */
        c->eof = 1;
        return;
    }
    if (le_key(&c->le, key ? key : (n ? (long)b[0] : 0), mods, b, n)) {
        in_append(c, c->le.buf, c->le.len);
        history_save(c, c->le.buf, c->le.len);
        le_reset(&c->le);
        c->checked[0] = 0;
        return;
    }
    check_command(c);
}

/* ---- keyboard ------------------------------------------------------------------- */

static long special_key(UWORD code)
{
    switch (code) {
    case 0x4C: return VT_KEY_UP;
    case 0x4D: return VT_KEY_DOWN;
    case 0x4E: return VT_KEY_RIGHT;
    case 0x4F: return VT_KEY_LEFT;
    case 0x41: return VT_KEY_BACKSPACE;
    case 0x42: return VT_KEY_TAB;
    case 0x43: return VT_KEY_KP_ENTER;
    case 0x44: return VT_KEY_RETURN;
    case 0x45: return VT_KEY_ESCAPE;
    case 0x46: return VT_KEY_DELETE;
    case 0x47: return VT_KEY_INSERT;
    case 0x48: return VT_KEY_PAGE_UP;
    case 0x49: return VT_KEY_PAGE_DOWN;
    case 0x4B: return VT_KEY_F11;
    case 0x5F: return VT_KEY_HELP;
    case 0x6F: return VT_KEY_F12;
    case 0x70: return VT_KEY_HOME;
    case 0x71: return VT_KEY_END;
    default: break;
    }
    if (code >= 0x50 && code <= 0x59)
        return VT_KEY_F1 + (code - 0x50);
    return 0;
}

/* Amiga numeric keypad raw codes (the A1200/A500 layout). */
static long keypad_key(UWORD code)
{
    switch (code) {
    case 0x0F: return VT_KEY_KP_0;
    case 0x1D: return VT_KEY_KP_1;
    case 0x1E: return VT_KEY_KP_2;
    case 0x1F: return VT_KEY_KP_3;
    case 0x2D: return VT_KEY_KP_4;
    case 0x2E: return VT_KEY_KP_5;
    case 0x2F: return VT_KEY_KP_6;
    case 0x3D: return VT_KEY_KP_7;
    case 0x3E: return VT_KEY_KP_8;
    case 0x3F: return VT_KEY_KP_9;
    case 0x3C: return VT_KEY_KP_DOT;
    case 0x4A: return VT_KEY_KP_MINUS;
    case 0x5E: return VT_KEY_KP_PLUS;
    case 0x5D: return VT_KEY_KP_STAR;
    case 0x5C: return VT_KEY_KP_SLASH;
    case 0x5A: return VT_KEY_KP_LPAREN;
    case 0x5B: return VT_KEY_KP_RPAREN;
    default: return 0;
    }
}

static void send_break(con *c, ULONG sig)
{
    struct Task *t;
    Forbid();
    if ((t = break_task(c)) != 0)
        Signal(t, sig);
    Permit();
    DBG("break to", (long)sig, (long)t);
}

static void cooked_key(con *c, const vt_u8 *b, int n, long key, int mods);

/* The selection's text to the clipboard, as Latin-1 (the clipboard's). */
static void copy_selection(con *c)
{
    /* allocated, not static: every window's process runs this code */
    char *utf, *lat;
    int ax, ay, bx, by;
    long n, i, k = 0;
    if (!vr_selection(&c->r, &ax, &ay, &bx, &by))
        return;
    utf = (char *)AllocVec(16384, MEMF_ANY);
    if (!utf)
        return;
    lat = utf; /* converted in place: Latin-1 is never longer */
    n = vt_copy_text(c->t, ax, ay, bx, by, utf, 16384);
    for (i = 0; i < n; i++) {
        unsigned char b = (unsigned char)utf[i];
        if (b < 0x80) {
            lat[k++] = (char)b;
        } else if ((b & 0xE0) == 0xC0 && i + 1 < n) {
            unsigned cp = ((b & 0x1F) << 6) | (utf[i + 1] & 0x3F);
            lat[k++] = (char)(cp < 0x100 ? cp : '?');
            i++;
        } else if ((b & 0xC0) != 0x80) {
            lat[k++] = '?'; /* beyond Latin-1 */
        }
    }
    clip_write(lat, k);
    FreeVec(utf);
}

/* The clipboard, typed into the program: Return for each line break, and
 * bracketed when the program asked for it (?2004). */
static void paste(con *c)
{
    char *text = (char *)AllocVec(8192, MEMF_ANY); /* not static: see copy_selection */
    long n, i;
    vt_u8 out[40];
    int k;
    if (!text)
        return;
    n = clip_read(text, 8192);
    if (n <= 0) {
        FreeVec(text);
        return;
    }
    if (c->raw) {
        k = vt_encode_paste(c->t, 0, out);
        in_append(c, out, k);
    }
    for (i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)text[i];
        long key = ch == '\n' ? VT_KEY_RETURN : (long)ch;
        if (ch == '\r')
            continue;
        k = vt_encode_key(c->t, key, 0, out);
        if (c->raw)
            in_append(c, out, k);
        else
            cooked_key(c, out, k, key == VT_KEY_RETURN ? key : 0, 0);
    }
    if (c->raw) {
        k = vt_encode_paste(c->t, 1, out);
        in_append(c, out, k);
    }
    FreeVec(text);
    service_reads(c);
}

/* Right Amiga C/V copy and paste, Right Amiga Up/Down and Shift+PgUp/PgDn
 * move through the scrollback. Returns 1 when the key was the console's. */
static int console_key(con *c, UWORD code, UWORD qual)
{
    int page = c->r.rows > 1 ? c->r.rows - 1 : 1;
    if (qual & IEQUALIFIER_RCOMMAND) {
        switch (code) {
        case 0x33: copy_selection(c); return 1;
        case 0x34: paste(c); return 1;
        case 0x4C: vr_set_view(&c->r, c->r.view + 1); return 1;
        case 0x4D: vr_set_view(&c->r, c->r.view - 1); return 1;
        default: return 0;
        }
    }
    if (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) {
        if (code == 0x48) {
            vr_set_view(&c->r, c->r.view + page);
            return 1;
        }
        if (code == 0x49) {
            vr_set_view(&c->r, c->r.view - page);
            if (!c->r.view)
                vr_cursor_on(&c->r);
            return 1;
        }
    }
    return 0;
}

static void key_event(con *c, struct IntuiMessage *im)
{
    UWORD code = im->Code, qual = im->Qualifier;
    long key;
    int mods = 0, n = 0;
    vt_u8 out[40];
    if ((qual & IEQUALIFIER_REPEAT) && !(vt_modes(c->t) & VT_MODE_AUTOREPEAT))
        return; /* DECARM off: a held key types once */
    if (vt_raw_events(c->t) & (1UL << 1)) {
        /* the program asked for raw keyboard events (CSI 1 {): an input
         * event report per key, press and release (matrix 5.2) */
        char b[80];
        int k = 0;
        long v[8];
        int i;
        v[0] = 1;
        v[1] = 0;
        v[2] = code;
        v[3] = qual;
        v[4] = v[5] = 0;
        if (im->IAddress) {
            /* the previous two down keys (dead keys): one ULONG, prev1
             * code and qualifier in the high word, prev2 in the low */
            ULONG pv = *(ULONG *)im->IAddress;
            v[4] = (long)(pv >> 16);
            v[5] = (long)(pv & 0xFFFF);
        }
        v[6] = (long)im->Seconds;
        v[7] = (long)im->Micros;
        b[k++] = (char)0x9B;
        for (i = 0; i < 8; i++) {
            char d[12];
            int m = 0;
            unsigned long x = (unsigned long)v[i];
            if (i)
                b[k++] = ';';
            do {
                d[m++] = (char)('0' + x % 10);
                x /= 10;
            } while (x);
            while (m)
                b[k++] = d[--m];
        }
        b[k++] = '|';
        in_append(c, (const vt_u8 *)b, k);
        service_reads(c);
        return;
    }
    if (code & IECODE_UP_PREFIX)
        return;
    if (console_key(c, code, qual))
        return; /* copy, paste, scrollback: the console's own keys */
    if (c->r.view)
        vr_set_view(&c->r, 0);
    if (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
        mods |= VT_MOD_SHIFT;
    if (qual & IEQUALIFIER_CONTROL)
        mods |= VT_MOD_CTRL;
    /* Meta (the ESC prefix, VT_MOD_ALT) is Left Amiga + key: Alt belongs
     * to the keymap, where many layouts type ; @ { [ with it (the rig's
     * ';' is Alt + 0x29; Alt-as-Meta turned it into ESC + o-umlaut). */
    if (qual & IEQUALIFIER_LCOMMAND)
        mods |= VT_MOD_ALT;
    key = special_key(code);
    if (!key && c->pers == VT_XTERM && (vt_modes(c->t) & VT_MODE_APP_KEYPAD))
        key = keypad_key(code); /* DECKPAM: the keypad sends SS3 codes */
    if (key) {
        n = vt_encode_key(c->t, key, mods, out);
    } else {
        /* Character keys go through the keymap (dead keys, Alt chars). */
        struct InputEvent ie;
        UBYTE buf[16];
        LONG k, i;
        ie.ie_NextEvent = 0;
        ie.ie_Class = IECLASS_RAWKEY;
        ie.ie_SubClass = 0;
        ie.ie_Code = code;
        /* the keymap sees Alt (national characters); Left Amiga, our
         * Meta, it does not */
        ie.ie_Qualifier = (UWORD)(qual & ~IEQUALIFIER_LCOMMAND);
        ie.ie_EventAddress = *(APTR *)im->IAddress;
        k = RawKeyConvert(&ie, (STRPTR)buf, sizeof(buf), 0);
        for (i = 0; i < k && n < (int)sizeof(out) - 8; i++) {
            /* The keymap already applied Ctrl: pass the character, with
             * Meta only (vt_encode_key adds the ESC for xterm). */
            n += vt_encode_key(c->t, buf[i], (c->pers == VT_XTERM) ? (mods & VT_MOD_ALT) : 0,
                               out + n);
        }
    }
    if (!n)
        return;
    if (tty_active(c)) {
        /* a Unix program's terminal: the line discipline takes the key (ISIG
         * makes ^C a break, ^\\ and ^Z signals; the rest is input) */
        DBG("tty key", out[0], (long)((c->ld.t.c_lflag & LD_ISIG) ? c->ld.t.c_cc[LD_VSUSP] : -1));
        ld_input(&c->ld, out, n);
        if (c->rtimer_busy && c->ld.t.c_cc[LD_VMIN] > 0)
            rtimer_start(c, c->ld.t.c_cc[LD_VTIME]); /* VTIME is between bytes */
        service_reads(c);
        return;
    }
    /* Break keys: Ctrl-C..F signal the opener in cooked mode (and Amiga raw
     * mode); an xterm window in raw mode sends the byte only, as a Unix tty
     * without ISIG does. */
    if (n == 1 && out[0] >= 0x03 && out[0] <= 0x06 && !key &&
        (!c->raw || c->pers != VT_XTERM)) {
        send_break(c, SIGBREAKF_CTRL_C << (out[0] - 0x03));
        if (!c->raw)
            return;
    }
    if (c->raw) {
        in_append(c, out, n);
    } else {
        cooked_key(c, out, n, key, mods);
    }
    service_reads(c);
}

/* ---- IDCMP ------------------------------------------------------------------------ */

/* An Amiga input event report (matrix 5.2) for a window class, when the
 * program asked for that class with CSI n { (ixemul asks for 12, resize). */
static int raw_report(con *c, int cls)
{
    char b[48];
    int n = 0, k;
    static const char tail[] = ";0;0;0;0;0;0;0|";
    if (!(vt_raw_events(c->t) & (1UL << cls)))
        return 0;
    b[n++] = (char)0x9B;
    if (cls >= 10)
        b[n++] = (char)('0' + cls / 10);
    b[n++] = (char)('0' + cls % 10);
    for (k = 0; tail[k]; k++)
        b[n++] = tail[k];
    in_append(c, (const vt_u8 *)b, n);
    service_reads(c);
    return 1;
}

static void resize(con *c)
{
    if (vr_layout(&c->r)) {
        vt_resize(c->t, c->r.cols, c->r.rows);
        sync_size(c);
        post_sizewindow(c);
    }
    vr_redraw(&c->r);
    vr_cursor_on(&c->r);
    raw_report(c, 12); /* IECLASS_SIZEWINDOW */
}

/* The mouse: reports to a program that asked for them (Shift held gives
 * the mouse back to selection, as in xterm), else drag-to-select. */
static void mouse_event(con *c, struct IntuiMessage *im)
{
    int x, y, btn = -1, kind = 0, n;
    vt_u8 out[40];
    int shift = (im->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
    int in = vr_cell_at(&c->r, im->MouseX, im->MouseY, &x, &y);
    if (im->Class == IDCMP_MOUSEMOVE) {
        if (c->dragging && in) {
            vr_select(&c->r, 1, c->drag_ax, c->drag_ay, x, y - c->r.view);
            c->drag_moved = 1;
        }
        return;
    }
    if (im->Code == SELECTDOWN) { btn = 0; kind = 0; }
    else if (im->Code == SELECTUP) { btn = 0; kind = 1; }
    else if (im->Code == MENUDOWN) { btn = 2; kind = 0; }
    else if (im->Code == MENUUP) { btn = 2; kind = 1; }
    if (btn < 0)
        return;
    if (!shift && in && !c->dragging) {
        n = vt_encode_mouse(c->t, btn, kind, x, y, 0, out);
        if (n) {
            in_append(c, out, n);
            service_reads(c);
            return;
        }
    }
    if (btn != 0)
        return;
    if (kind == 0 && in) {
        c->dragging = 1;
        c->drag_moved = 0;
        c->drag_ax = x;
        c->drag_ay = y - c->r.view;
        vr_select(&c->r, 0, 0, 0, 0, 0);
        ReportMouse(TRUE, c->win);
    } else if (kind == 1 && c->dragging) {
        c->dragging = 0;
        ReportMouse(FALSE, c->win);
        if (!c->drag_moved)
            vr_select(&c->r, 0, 0, 0, 0, 0); /* a click clears the selection */
    }
}

static void idcmp(con *c)
{
    struct IntuiMessage *im;
    if (c->win && !IsListEmpty(&c->win->UserPort->mp_MsgList))
        render(c); /* resize, refresh, selection: on the current screen */
    while (c->win && (im = (struct IntuiMessage *)GetMsg(c->win->UserPort))) {
        ULONG cls = im->Class;
        switch (cls) {
        case IDCMP_RAWKEY:
            key_event(c, im);
            break;
        case IDCMP_NEWSIZE:
            resize(c);
            break;
        case IDCMP_REFRESHWINDOW:
            BeginRefresh(c->win);
            vr_redraw(&c->r);
            EndRefresh(c->win, TRUE);
            vr_cursor_on(&c->r);
            break;
        case IDCMP_CLOSEWINDOW:
            c->closing = 1;
            if (!c->raw)
                c->eof = 1;
            else if (!raw_report(c, 11)) /* IECLASS_CLOSEWINDOW, if asked */
                send_break(c, SIGBREAKF_CTRL_C);
            break;
        case IDCMP_MOUSEBUTTONS:
        case IDCMP_MOUSEMOVE:
            mouse_event(c, im);
            break;
        default:
            break;
        }
        ReplyMsg((struct Message *)im);
    }
}

/* ---- packets ------------------------------------------------------------------------ */

/* The ROM unit's size fields are what ixemul's TIOCGWINSZ reads
 * (cu_XMax/YMax + 1, __tioctl.c:247). The ROM console recomputes them only
 * when written to, which vtcon never does, so after a resize they stayed
 * at the old size (rig, 2026-09-29: tcsh kept 47x26 in a 97-column
 * window). The grid is vtcon's: its size is written there. */
static void sync_size(con *c)
{
    struct ConUnit *cu;
    if (!c->rom_io || !c->t)
        return;
    cu = (struct ConUnit *)c->rom_io->io_Unit;
    cu->cu_XMax = (WORD)(vt_cols(c->t) - 1);
    cu->cu_YMax = (WORD)(vt_rows(c->t) - 1);
}

/* ixemul raises SIGWINCH from an input handler at priority 10 that
 * watches the chain for IECLASS_SIZEWINDOW events on the program's window
 * (ix_sigwinch.c). On Kickstart 3.1 Intuition sends none -- a probe
 * handler at priority 10 counted 0 for a drag of the size gadget and 0
 * for one written with IND_WRITEEVENT, which enters above Intuition and
 * does not come out of it (rig, 2026-09-29). So this handler, at 20,
 * between Intuition (50) and ixemul, adds that event to the chain after
 * a resize; input.device's 10 Hz timer events run it soon after. */
static struct InputEvent *winch_handler(__reg("a0") struct InputEvent *chain, __reg("a1") APTR data)
{
    con *c = (con *)data;
    if (c->winch_pending) {
        c->winch_pending = 0;
        c->winch_ev.ie_NextEvent = chain;
        return &c->winch_ev;
    }
    return chain;
}

static void post_sizewindow(con *c)
{
    if (!c->win)
        return;
    if (!c->winch_added) {
        c->input_port = CreateMsgPort();
        if (!c->input_port)
            return;
        c->input_io = (struct IOStdReq *)CreateIORequest(c->input_port, sizeof(struct IOStdReq));
        if (!c->input_io || OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)c->input_io, 0)) {
            if (c->input_io)
                DeleteIORequest((struct IORequest *)c->input_io);
            c->input_io = 0;
            DeleteMsgPort(c->input_port);
            c->input_port = 0;
            return;
        }
        memset(&c->winch_ev, 0, sizeof(c->winch_ev));
        c->winch_ev.ie_Class = IECLASS_SIZEWINDOW;
        c->winch_irq.is_Code = (void (*)())winch_handler;
        c->winch_irq.is_Data = (APTR)c;
        c->winch_irq.is_Node.ln_Pri = 20;
        c->winch_irq.is_Node.ln_Name = (char *)"vtcon SIGWINCH";
        c->input_io->io_Command = IND_ADDHANDLER;
        c->input_io->io_Data = &c->winch_irq;
        DoIO((struct IORequest *)c->input_io);
        c->winch_added = 1;
    }
    c->winch_ev.ie_EventAddress = (APTR)c->win;
    c->winch_pending = 1;
    DBG("sigwinch", c->winch_added, 0);
}

static struct IOStdReq *rom_console(con *c)
{
    /* DISK_INFO callers get a real console.device unit on our window, so
     * programs that read ConUnit fields (window size in characters) or
     * send it CMD_WRITE keep working. Opened on first use only; its
     * cursor is switched off at once, our renderer owns the window. */
    if (c->rom_io || !c->win)
        return c->rom_io;
    c->rom_port = CreateMsgPort();
    if (!c->rom_port)
        return 0;
    c->rom_io = (struct IOStdReq *)CreateIORequest(c->rom_port, sizeof(struct IOStdReq));
    if (!c->rom_io)
        return 0;
    c->rom_io->io_Data = c->win;
    c->rom_io->io_Length = sizeof(struct Window);
    if (OpenDevice((STRPTR)"console.device", CONU_STANDARD, (struct IORequest *)c->rom_io, 0)) {
        DeleteIORequest((struct IORequest *)c->rom_io);
        c->rom_io = 0;
        return 0;
    }
    c->rom_io->io_Command = CMD_WRITE;
    c->rom_io->io_Data = (APTR)"\x9b" "0 p";
    c->rom_io->io_Length = 4;
    DoIO((struct IORequest *)c->rom_io);
    sync_size(c);
    /* opening a console unit on a window clears it in the ROM console's
     * pens: every ixemul program asks DISK_INFO at startup, and the window
     * turned Workbench grey with black only behind the text drawn after
     * (rig, 2026-09-29). Our grid is the truth: draw it all again. */
    if (c->t)
        vr_redraw(&c->r);
    return c->rom_io;
}

static void start_timer(con *c, ULONG micros)
{
    if (!c->timer_open || c->timer_busy)
        return;
    c->timer->tr_node.io_Command = TR_ADDREQUEST;
    c->timer->tr_time.tv_secs = micros / 1000000;
    c->timer->tr_time.tv_micro = micros % 1000000;
    SendIO((struct IORequest *)c->timer);
    c->timer_busy = 1;
}

static void packet(con *c, struct DosPacket *p)
{
    if (p->dp_Type != ACTION_WRITE && p->dp_Type != ACTION_READ)
        DBG("packet", p->dp_Type, p->dp_Arg1);
    switch (p->dp_Type) {
    case ACTION_FINDINPUT:
    case ACTION_FINDOUTPUT:
    case ACTION_FINDUPDATE: {
        struct FileHandle *fh = (struct FileHandle *)BADDR(p->dp_Arg1);
        if (!c->spec_parsed) {
            /* the first open names the window */
            UBYTE *b = (UBYTE *)BADDR(p->dp_Arg3);
            char name[256];
            int i, n = b ? b[0] : 0, colon = -1;
            for (i = 0; i < n && i < 255; i++) {
                name[i] = (char)b[i + 1];
                if (colon < 0 && name[i] == ':')
                    colon = i;
            }
            name[i] = 0;
            parse_spec(c, colon >= 0 ? name + colon + 1 : name);
            c->spec_parsed = 1;
            c->break_port = c->home_port = p->dp_Port;
            c->break_owner = (struct Task *)p->dp_Port->mp_SigTask;
            DBG("open window", c->ww, c->wh);
            if (!c->auto_open && !open_window(c)) {
                DBG("open failed", c->win, c->t);
                close_window(c);
                reply(p, DOSFALSE, ERROR_NO_FREE_STORE);
                return;
            }
        }
        if (c->node && c->node->dn_Task == c->port) {
            Forbid();
            c->node->dn_Task = 0; /* the next Open starts a new process */
            Permit();
        }
        fh->fh_Port = (struct MsgPort *)DOSTRUE; /* interactive */
        fh->fh_Arg1 = (LONG)c;
        c->opens++;
        c->ever_opened = 1;
        reply(p, DOSTRUE, 0);
        return;
    }
    case ACTION_END:
        c->opens--;
        DBG("prof writes/bytes", c->prof_writes, c->prof_bytes);
        DBG("prof out/damage", c->prof_out, c->prof_damage);
        DBG("prof scroll/n", c->prof_scroll, c->prof_nscroll);
        DBG("prof ndamage", c->prof_ndamage, 0);
    DBG("prof direct/text", c->r.n_direct, c->r.n_text);
        reply(p, DOSTRUE, 0);
        return;
    case ACTION_READ:
        if (!c->win && !open_window(c)) {
            reply(p, -1, ERROR_NO_FREE_STORE);
            return;
        }
        if (c->nreads < READ_Q) {
            c->reads[c->nreads++] = p;
            service_reads(c);
        } else {
            reply(p, -1, ERROR_NO_FREE_STORE);
        }
        return;
    case ACTION_WRITE:
        if (!c->win && !open_window(c)) {
            reply(p, -1, ERROR_NO_FREE_STORE);
            return;
        }
        if (tty_active(c) && (c->ld.t.c_oflag & LD_OPOST)) {
            /* OPOST in chunks: a byte may become 8 (a tab under OXTABS) */
            const unsigned char *b = (const unsigned char *)p->dp_Arg2;
            LONG left = p->dp_Arg3;
            while (left > 0) {
                LONG k = left > (LONG)sizeof(c->obuf) / 8 ? (LONG)sizeof(c->obuf) / 8 : left;
                output(c, c->obuf, ld_output(&c->ld.t, b, k, c->obuf, &c->tty_ocol));
                b += k;
                left -= k;
            }
        } else {
            output(c, (const vt_u8 *)p->dp_Arg2, p->dp_Arg3);
        }
        reply(p, p->dp_Arg3, 0);
        service_reads(c); /* the output may have queued a report */
        return;
    case ACTION_SCREEN_MODE:
        tty_leave(c); /* the Amiga way to set a mode: termios mode is over */
        if (c->raw && !p->dp_Arg1) {
            c->raw = 0;
        } else if (!c->raw && p->dp_Arg1) {
            /* a partly typed line becomes input as it stands */
            in_append(c, c->le.buf, c->le.len);
            le_reset(&c->le);
            c->raw = 1;
        }
        reply(p, DOSTRUE, 0);
        service_reads(c);
        return;
    case ACTION_WAIT_CHAR:
        if (c->in_len || c->eof) {
            reply(p, DOSTRUE, 0);
        } else if (c->waitchar) {
            reply(p, DOSFALSE, 0); /* one waiter at a time */
        } else {
            c->waitchar = p;
            start_timer(c, (ULONG)p->dp_Arg1);
        }
        return;
    case ACTION_STACK:
    case ACTION_QUEUE: {
        /* ARexx PUSH / QUEUE (rexx/rexxio.h): a line into the input, at the
         * front (stack) or the end (queue), read as if typed but not shown
         * -- as the ROM CON: does (rig: rx "push 'echo pushed'; queue ...") */
        const vt_u8 *b = (const vt_u8 *)p->dp_Arg2;
        LONG n = p->dp_Arg3;
        if (n < 0 || !b)
            n = 0;
        if (n > IN_MAX - c->in_len)
            n = IN_MAX - c->in_len;
        if (p->dp_Type == ACTION_STACK && n > 0) {
            memmove(c->in + n, c->in, c->in_len);
            CopyMem((APTR)b, c->in, n);
            c->in_len += n;
        } else {
            in_append(c, b, (int)n);
        }
        reply(p, n, 0);
        service_reads(c);
        return;
    }
    case ACTION_VTCON_TCGETA:
        if (!p->dp_Arg2) {
            reply(p, DOSFALSE, ERROR_REQUIRED_ARG_MISSING);
            return;
        }
        if (tty_active(c)) {
            CopyMem(&c->ld.t, (APTR)p->dp_Arg2, sizeof(vt_termios));
        } else {
            /* what the window does now, in termios terms */
            vt_termios t;
            ld_defaults(&t);
            if (c->raw)
                t.c_lflag &= ~(ld_flag)(LD_ICANON | LD_ECHO | LD_ISIG | LD_IEXTEN);
            CopyMem(&t, (APTR)p->dp_Arg2, sizeof(t));
        }
        reply(p, DOSTRUE, 0);
        return;
    case ACTION_VTCON_TCSETA:
        if (!p->dp_Arg2) {
            reply(p, DOSFALSE, ERROR_REQUIRED_ARG_MISSING);
            return;
        }
        tty_enter(c, (struct Task *)p->dp_Port->mp_SigTask);
        ld_set(&c->ld, (const vt_termios *)p->dp_Arg2, (int)p->dp_Arg3);
        reply(p, DOSTRUE, 0);
        service_reads(c);
        return;
    case ACTION_VTCON_GWINSZ:
        if (!p->dp_Arg2 || !c->t) {
            reply(p, DOSFALSE, p->dp_Arg2 ? ERROR_OBJECT_NOT_FOUND : ERROR_REQUIRED_ARG_MISSING);
            return;
        }
        {
            vt_winsize *ws = (vt_winsize *)p->dp_Arg2;
            ws->ws_row = (unsigned short)vt_rows(c->t);
            ws->ws_col = (unsigned short)vt_cols(c->t);
            ws->ws_xpixel = (unsigned short)(ws->ws_col * c->r.cw);
            ws->ws_ypixel = (unsigned short)(ws->ws_row * c->r.ch);
        }
        reply(p, DOSTRUE, 0);
        return;
    case ACTION_VTCON_SWINSZ:
        reply(p, DOSFALSE, ERROR_ACTION_NOT_KNOWN); /* a window's size is the window's */
        return;
    case ACTION_VTCON_WORDS:
        take_words(c, p);
        reply(p, DOSTRUE, 0);
        return;
    case ACTION_CHANGE_SIGNAL:
        if (p->dp_Arg2) {
            c->break_port = (struct MsgPort *)p->dp_Arg2;
            c->break_owner = (struct Task *)c->break_port->mp_SigTask;
        }
        reply(p, DOSTRUE, 0);
        return;
    case ACTION_DISK_INFO: {
        struct InfoData *id = (struct InfoData *)BADDR(p->dp_Arg1);
        LONG i;
        if (!c->win)
            open_window(c); /* as V47's con-handler does: the caller wants the window */
        for (i = 0; i < (LONG)sizeof(*id); i++)
            ((UBYTE *)id)[i] = 0;
        id->id_DiskType = 0x434F4E00L; /* 'CON\0' (no NDK name; value unverified against the ROM) */
        id->id_VolumeNode = (BPTR)c->win;
        id->id_InUse = (LONG)rom_console(c);
        reply(p, DOSTRUE, 0);
        return;
    }
    case ACTION_IS_FILESYSTEM:
        reply(p, DOSFALSE, 0);
        return;
    case ACTION_FLUSH:
        reply(p, DOSTRUE, 0);
        return;
    case ACTION_SEEK:
        reply(p, -1, ERROR_ACTION_NOT_KNOWN);
        return;
    default:
        reply(p, DOSFALSE, ERROR_ACTION_NOT_KNOWN);
        return;
    }
}

/* ---- main ---------------------------------------------------------------------------- */

static LONG handler_main(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    struct DosPacket *p;
    con *c;

    WaitPort(&me->pr_MsgPort);
    p = (struct DosPacket *)GetMsg(&me->pr_MsgPort)->mn_Node.ln_Name; /* the startup packet */
    c = (con *)AllocVec(sizeof(con), MEMF_ANY | MEMF_CLEAR);
    if (!c)
        return 0; /* cannot even reply: DOS will see the process end */
    c->port = &me->pr_MsgPort;

    DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR)"dos.library", 39);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 36);
    LayersBase = OpenLibrary((STRPTR)"layers.library", 39);
    c->timer_port = CreateMsgPort();
    if (c->timer_port) {
        c->timer = (struct timerequest *)CreateIORequest(c->timer_port, sizeof(struct timerequest));
        if (c->timer && !OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)c->timer, 0)) {
            c->timer_open = 1;
            TimerBase = c->timer->tr_node.io_Device;
            /* VTIME's timer: the same unit, its own request */
            c->rtimer = (struct timerequest *)CreateIORequest(c->timer_port, sizeof(struct timerequest));
            if (c->rtimer) {
                c->rtimer->tr_node.io_Device = c->timer->tr_node.io_Device;
                c->rtimer->tr_node.io_Unit = c->timer->tr_node.io_Unit;
            }
        }
    }
    c->ld.echo = tty_echo;
    c->ld.signal = tty_signal;
    c->ld.user = c;
    ld_init(&c->ld);
    c->frame_port = CreateMsgPort();
    if (c->frame_port) {
        c->frame = (struct timerequest *)CreateIORequest(c->frame_port, sizeof(struct timerequest));
        if (c->frame && !OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)c->frame, 0))
            c->frame_open = 1;
    }
    if (!OpenDevice((STRPTR)"console.device", (ULONG)CONU_LIBRARY, (struct IORequest *)&c->lib_io, 0))
        ConsoleDevice = c->lib_io.io_Device; /* RawKeyConvert; the same device for every process */
    if (!DOSBase || !IntuitionBase || !GfxBase || !ConsoleDevice || !LayersBase) {
        if (DOSBase)
            ReplyPkt(p, DOSFALSE, ERROR_NO_FREE_STORE);
        FreeVec(c);
        return 0;
    }
    if (!vers[0])
        return 0; /* keeps the $VER string linked in */
    DBG("startup", p->dp_Type, p->dp_Arg3);
#ifdef VTCON_DEBUG
    dbg_port = c->port;
#endif
    DBG_FLUSH(); /* opens the log while no Open is pending */
    /* DOS sends the Open that started us to dn_Task, so it is set now and
     * cleared again after that first Open (see packet()): every later
     * Open of XCON: then starts its own process and window, as CON: does. */
    c->node = (struct DeviceNode *)BADDR(p->dp_Arg3);
    if (c->node)
        c->node->dn_Task = c->port;
    ReplyPkt(p, DOSTRUE, 0);

    for (;;) {
        ULONG wait = 1UL << c->port->mp_SigBit;
        struct Message *m;
        if (c->win)
            wait |= 1UL << c->win->UserPort->mp_SigBit;
        if (c->timer_port)
            wait |= 1UL << c->timer_port->mp_SigBit;
        if (c->comp_port)
            wait |= 1UL << c->comp_port->mp_SigBit;
        if (c->frame_port)
            wait |= 1UL << c->frame_port->mp_SigBit;
        /* No trace lines in the loop itself: each log Write's reply re-arms
         * our DOS signal, so a trace here wakes the loop forever and filled
         * RAM: on the rig (2026-09-29). */
        wait = Wait(wait);
        while ((m = GetMsg(c->port)))
            packet(c, (struct DosPacket *)m->mn_Node.ln_Name);
        if (c->frame_busy && CheckIO((struct IORequest *)c->frame)) {
            WaitIO((struct IORequest *)c->frame);
            c->frame_busy = 0;
            render(c); /* the frame is due */
            if (vr_blink_tick(&c->r))
                frame_start(c);
        }
        if (!c->frame_open)
            render(c); /* no frame clock: draw at once */
        idcmp(c);
        if (c->comp_port)
            finish_completion(c);
        if (c->rtimer_busy && CheckIO((struct IORequest *)c->rtimer)) {
            WaitIO((struct IORequest *)c->rtimer);
            c->rtimer_busy = 0;
            c->rtimer_fired = 1; /* VTIME is up: the waiting read takes what there is */
        }
        if (c->timer_busy && CheckIO((struct IORequest *)c->timer)) {
            WaitIO((struct IORequest *)c->timer);
            c->timer_busy = 0;
            if (c->waitchar) {
                reply(c->waitchar, DOSFALSE, 0);
                c->waitchar = 0;
            }
        }
        service_reads(c);
        /* Done when every handle is closed -- but only after the first Open:
         * before it, opens is 0 too (a wake-up between the startup packet and
         * that Open used to end the handler, leaving dn_Task at a dead port). */
        if (c->ever_opened && c->opens <= 0 && (!c->wait_close || c->closing) && !c->nreads &&
            !c->comp_busy && !c->check_busy && !c->hist_busy) /* a worker holds our request */
            break;
    }
    render(c);
    if (c->frame_busy) {
        AbortIO((struct IORequest *)c->frame);
        WaitIO((struct IORequest *)c->frame);
    }
    if (c->frame_open)
        CloseDevice((struct IORequest *)c->frame);
    if (c->frame)
        DeleteIORequest((struct IORequest *)c->frame);
    if (c->frame_port)
        DeleteMsgPort(c->frame_port);
    Forbid();
    if (c->node && c->node->dn_Task == c->port)
        c->node->dn_Task = 0; /* never leave DOS a port that is going away */
    Permit();
    close_window(c);
    forget_words(c);
    if (c->comp)
        FreeVec(c->comp);
    if (c->check)
        FreeVec(c->check);
    if (c->hist) {
        if (c->hist->data)
            FreeVec(c->hist->data);
        FreeVec(c->hist);
    }
    if (c->comp_port)
        DeleteMsgPort(c->comp_port);
    rtimer_stop(c);
    if (c->rtimer)
        DeleteIORequest((struct IORequest *)c->rtimer);
    c->rtimer = 0;
    if (c->timer_open) {
        CloseDevice((struct IORequest *)c->timer);
        c->timer_open = 0;
    }
    if (c->timer)
        DeleteIORequest((struct IORequest *)c->timer);
    if (c->timer_port)
        DeleteMsgPort(c->timer_port);
    if (ConsoleDevice)
        CloseDevice((struct IORequest *)&c->lib_io); /* CONU_LIBRARY must be closed too (matrix 6.2) */
    CloseLibrary(DiskfontBase);
    CloseLibrary(LayersBase);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    CloseLibrary((struct Library *)DOSBase);
    FreeVec(c);
    return 0;
}
