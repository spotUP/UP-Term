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
#include <dos/dostags.h>
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
#include "../render/vtwin.h"
#include "../device/upc_public.h"
#include "clip.h"
#include "lineedit.h"
#include "complete.h"
#include "brk.h"
#include "vtcon_packets.h"
#include "../tty/ldisc.h"
#include "../config/upconf.h"

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
    vtwin w;                     /* the window: engine, renderer, fonts, frame clock (render/vtwin.h) */
    struct Screen *locked;       /* the public screen we opened on */
    int opens;
    int raw;
    int wait_close;              /* WAIT: keep the window after the last Close */
    int closing;                 /* close gadget clicked */
    int eof;                     /* Ctrl-\ or close gadget in cooked mode */
    brk brk;                     /* who gets Ctrl-C/D/E/F (brk.h) */
    /* termios mode (ACTION_VTCON_TCSETA): a Unix program's line discipline */
    ldisc ld;
    int tty;                     /* in termios mode */
    struct Task *tty_owner;      /* the task that set it; its end ends the mode */
    int tty_ocol;                /* output column, for OXTABS */
    struct timerequest *rtimer;  /* VTIME for the first waiting read */
    int rtimer_busy, rtimer_fired;
    vt_u8 obuf[2048];            /* OPOST output of one chunk */
    /* bytes ready for Read (complete lines in cooked mode) */
    vt_u8 in[IN_MAX];
    int in_len;
    le_line le;                  /* the line being edited (cooked mode) */
    struct DosPacket *reads[READ_Q];
    int nreads;
    struct Task *held[8];        /* ACTION_VTCON_HOLD: their reads wait */
    int nheld;
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
    volatile UBYTE winch_closing; /* teardown started: name no window again */
    struct MsgPort *rom_port;
    /* window spec */
    WORD wx, wy, ww, wh;
    char screen[64];
    ULONG wflags;
    int inactive;
    int auto_open;               /* AUTO: no window until the first read or write */
    int auto_held;               /* DISK_INFO holds an AUTO window open until UNDISK_INFO (V47) */
    int auto_shut;               /* the close gadget shut an AUTO window: close it after idcmp() */
    int spec_parsed;
    char profile[UC_NAME];       /* the config profile (spec's PROFILE; "default") */
    int colours_spec;            /* the spec chose colours (DARK/FG/BG/LIGHT): it beats the profile */
    upconf *conf;                /* the user config, read at startup (config/upconf.h) */
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
    /* the find prompt (Right Amiga F): its own small window, open while the
     * console keeps running -- the program's output must not stop while a
     * query is typed */
    struct MsgPort *find_port;
    struct Window *find_win;
    struct Gadget *find_gad;      /* reported back by AddGadget, for RefreshGadgets */
    struct Gadget find_gadget;    /* the gadget struct, filled by hand (see below) */
    struct StringInfo find_si;    /* its text; Intuition maintains find_buf through it */
    char find_buf[VT_FIND_QUERY_MAX]; /* the query */
    ULONG foreign_idcmp;         /* its IDCMP before we added ours, to restore */
    struct DeviceNode *node;     /* our DOS device node (from the startup packet) */
    int ever_opened;
    struct IOStdReq lib_io;      /* console.device CONU_LIBRARY, for RawKeyConvert */
#ifdef VTCON_DEBUG
    ULONG prof_out, prof_writes, prof_bytes;
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
#elif defined(VTCON_SERIAL)
/* Trace (build with SERIAL=1) to the serial port through exec's RawPutChar
 * (LVO -516): no DOS call, so it cannot race the packets as the RAM: log's
 * Write can; safe anywhere. The rig writes the port to build/rig/serial.log. */
static void ser_putc(__reg("d0") char ch) =
    "\tmove.l\ta6,-(sp)\n\tmove.l\t4.w,a6\n\tjsr\t-516(a6)\n\tmove.l\t(sp)+,a6";
static void ser_dbg(const char *what, LONG a, LONG b)
{
    int i, k;
    LONG v[2];
    v[0] = a;
    v[1] = b;
    ser_putc('v');
    ser_putc(' ');
    for (i = 0; what[i] && i < 40; i++)
        ser_putc(what[i]);
    for (i = 0; i < 2; i++) {
        ser_putc(' ');
        for (k = 28; k >= 0; k -= 4)
            ser_putc("0123456789abcdef"[((ULONG)v[i] >> k) & 15]);
    }
    ser_putc('\n');
}
#define DBG(w, a, b) ser_dbg(w, (LONG)(a), (LONG)(b))
#define DBG_FLUSH()
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

static int tty_active(con *c);
static void service_reads(con *c);

static void h_reply(void *u, const vt_u8 *b, long n)
{
    con *c = (con *)u;
    /* reports enter the read stream, as the console's do. In termios mode
     * that stream is the line discipline, as for a typed key: in the cooked
     * buffer they were never read, yet made WAIT_CHAR answer yes -- tmux's
     * select saw input, its read blocked until the next key, and every key
     * showed one key late (tmux asks for DA and colours at start) */
    if (tty_active(c)) {
        ld_input(&c->ld, b, (int)n);
        service_reads(c);
        return;
    }
    in_append(c, b, (int)n);
}

/* bytes straight into the read stream (raw reports, mouse, paste marks) */
static void h_input(void *u, const vt_u8 *b, long n)
{
    con *c = (con *)u;
    in_append(c, b, (int)n);
    service_reads(c);
}

static void typed(con *c, const vt_u8 *out, int n, long key, int mods);
static void cooked_key(con *c, const vt_u8 *b, int n, long key, int mods);

static void h_key(void *u, const vt_u8 *b, int n, long key, int mods)
{
    typed((con *)u, b, n, key, mods);
}

static void h_pasted(void *u, const vt_u8 *b, int n, long key)
{
    con *c = (con *)u;
    if (c->raw)
        in_append(c, b, n);
    else
        cooked_key(c, b, n, key, 0);
    service_reads(c);
}

static int h_raw(void *u)
{
    return ((con *)u)->raw;
}

static void sync_size(con *c);
static void post_sizewindow(con *c);

static void h_resized(void *u)
{
    con *c = (con *)u;
    sync_size(c);
    post_sizewindow(c);
}

static const vtwin_host host = { h_reply, h_input, h_key, h_pasted, h_raw, h_resized };

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

/* Is our DOS device node named `want` (case-insensitive)? The CON:/RAW:
 * swap (console plan DD19, H5) points those entries at this handler; it
 * then serves them in the Amiga personality, RAW: starting raw. */
static int node_named(con *c, const char *want)
{
    const UBYTE *b;
    int i, n;
    if (!c->node || !c->node->dn_Name)
        return 0;
    b = (const UBYTE *)BADDR(c->node->dn_Name);
    n = b[0];
    for (i = 0; i < n; i++) {
        char x = (char)b[i + 1], y = want[i];
        if (!y)
            return 0;
        if (x >= 'a' && x <= 'z')
            x = (char)(x - 32);
        if (x != y)
            return 0;
    }
    return want[n] == 0;
}

/* ---- the user config (plan 2026-10-01) ---------------------------------------- */

/* /ENV/up-term/up-term, read once per process while it is idle -- before
 * the first Open, so no DOS call can race a packet (the handler's rule).
 * The kit's Install creates the directory; a missing or empty file is not
 * an error: every lookup then misses and the built-in defaults stand, so
 * a window with no config behaves exactly as before this feature. */
#define CONF_MAX UC_MAX_FILE

/* Reading the file is DOS work, and a handler must not do DOS work while DOS
 * is waiting for its reply: DOS holds the DosList lock for exactly that
 * period, so our own Open() blocks and the rig answers every NewShell XCON:
 * with "Software Failure - wait for disk activity to finish" (2026-10-02).
 * This process was started by the Open we just answered, so DOS is still
 * forwarding that reply. The same lock blocks OpenLibrary() here too.
 *
 * So the read runs in its own process, the way handler/complete.c's worker
 * already does its lookups: a task may block on DOS without blocking the
 * packet loop, and pr_WindowPtr = -1 keeps a requester off the screen. The
 * caller waits for the reply before it serves any packet, so the profile is
 * in place before the first Open is answered. */
struct config_msg {
    struct Message msg;
    upconf *conf;
};

static void config_worker(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    struct config_msg *m;
    BPTR f;
    char *buf;

    WaitPort(&me->pr_MsgPort); /* the request, before any DOS call */
    m = (struct config_msg *)GetMsg(&me->pr_MsgPort);
    me->pr_WindowPtr = (APTR)-1; /* no requesters from a file that may be absent */
    /* ENV: first, as Amiga prefs do (the editor's Use writes there; Install
     * and Save copy to both), ENVARC: when ENV: has none (a fresh boot
     * before ENVARC: was copied) */
    f = Open((STRPTR)"ENV:up-term/up-term", MODE_OLDFILE);
    if (!f)
        f = Open((STRPTR)"ENVARC:up-term/up-term", MODE_OLDFILE);
    if (f) {
        /* Read straight up to the cap rather than Seek()ing to the end first:
         * on 3.1 that Seek answers 0 for this file, and the read is skipped. */
        buf = (char *)AllocVec(CONF_MAX + 1, MEMF_ANY);
        if (buf) {
            LONG got = Read(f, buf, CONF_MAX);
            buf[got > 0 ? got : 0] = 0;
            upconf_parse(m->conf, buf, got > 0 ? got : 0);
            FreeVec(buf);
        }
        Close(f);
    }
    Forbid(); /* the opener frees m: end before it can run on */
    ReplyMsg((struct Message *)m);
}

static void config_load(con *c)
{
    struct MsgPort *port;
    struct config_msg *m;
    struct Process *w;

    if (!c->conf)
        return; /* out of memory: the built-in defaults stand */
    port = CreateMsgPort();
    m = (struct config_msg *)AllocVec(sizeof(*m), MEMF_CLEAR);
    if (!port || !m) {
        if (m)
            FreeVec(m);
        if (port)
            DeleteMsgPort(port);
        return;
    }
    m->msg.mn_ReplyPort = port;
    m->conf = c->conf;
    w = CreateNewProcTags(NP_Entry, (ULONG)config_worker,
                          NP_Name, (ULONG)"vtcon config", NP_StackSize, 8192,
                          NP_Input, 0, NP_Output, 0,
                          NP_CloseInput, FALSE, NP_CloseOutput, FALSE,
                          NP_ConsoleTask, 0, TAG_DONE);
    if (!w) {
        FreeVec(m);
        DeleteMsgPort(port);
        return; /* no worker: the built-in defaults stand */
    }
    /* CreateMsgPort binds the port to us: the worker's reply sets its
     * signal whenever it comes, so WaitPort cannot miss it. The port stays
     * private -- AddPort put it on exec's public list, and deleting it
     * without RemPort left a freed node there (every window open corrupted
     * the system port list, 2026-10-02 review). The worker only reads a file:
     * waiting for it here is what the ROM con-handler does for its own
     * reads; DOS is not waiting on us any more, so it can answer the worker. */
    PutMsg(&w->pr_MsgPort, (struct Message *)m);
    WaitPort(port);
    GetMsg(port);
    FreeVec(m);
    DeleteMsgPort(port);
}

static int profile_exists(const upconf *cf, const char *name)
{
    const char *names[UC_MAX_PROFILES + 1];
    int n = upconf_profiles(cf, names), i;
    for (i = 0; i < n; i++)
        if (str_ieq(names[i], name))
            return 1;
    return 0;
}

/* The profile's values for every knob the spec did not set (precedence:
 * built-in defaults < profile < window spec; plan 2026-10-01). A profile
 * the file does not know falls back to "default". */
static void apply_profile(con *c)
{
    const char *p, *v;
    if (!c->conf)
        return;
    p = c->profile;
    if (!profile_exists(c->conf, p))
        p = profile_exists(c->conf, "default") ? "default" : 0;
    if (!p)
        return;
    v = upconf_str(c->conf, p, "font", 0);
    if (v && !c->w.fontname[0]) {
        char f[UC_MAX_VALUE];
        int i;
        copy_str(f, v, sizeof(f));
        for (i = 0; f[i]; i++)
            if (f[i] == ':')
                f[i] = ' '; /* the profile's "NAME:SIZE" is the spec's "NAME SIZE" */
        parse_font(f, c->w.fontname, sizeof(c->w.fontname), &c->w.fontsize);
    }
    if (!c->colours_spec) {
        ULONG fg = upconf_rgb(c->conf, p, "fg", VR_KEEP);
        ULONG bg = upconf_rgb(c->conf, p, "bg", VR_KEEP);
        if (fg != VR_KEEP)
            c->w.fg_rgb = fg;
        if (bg != VR_KEEP)
            c->w.bg_rgb = bg;
    }
    c->w.sb_lines = (int)upconf_int(c->conf, p, "scrollback", 0); /* 0 = the built-in 500 */
    v = upconf_str(c->conf, p, "cursor", 0);
    if (v) {
        if (str_ieq(v, "block"))
            c->w.cursor_style = 1;
        else if (str_ieq(v, "underline"))
            c->w.cursor_style = 3;
        else if (str_ieq(v, "bar"))
            c->w.cursor_style = 5;
    }
    v = upconf_str(c->conf, p, "cursor-blink", 0);
    if (v)
        c->w.cursor_blink = str_ieq(v, "on");
    v = upconf_str(c->conf, p, "cursor-color", 0);
    if (v && !str_ieq(v, "inverse"))
        c->w.cursor_rgb = upconf_rgb(c->conf, p, "cursor-color", VR_KEEP);
    /* either key alone is enough: whichever is unset keeps the swapped value */
    c->w.sel_fg_rgb = upconf_rgb(c->conf, p, "selection-fg", VR_KEEP);
    c->w.sel_bg_rgb = upconf_rgb(c->conf, p, "selection-bg", VR_KEEP);
    v = upconf_str(c->conf, p, "bell", 0);
    if (v) {
        if (str_ieq(v, "none"))
            c->w.bell = 0;
        else if (str_ieq(v, "visual"))
            c->w.bell = 2;
        else if (str_ieq(v, "beep"))
            c->w.bell = 1;
    }
    v = upconf_str(c->conf, p, "bold-bright", 0);
    if (v)
        c->w.bold_bright = str_ieq(v, "on");
    v = upconf_str(c->conf, p, "meta", 0);
    if (v)
        c->w.meta_alt = str_ieq(v, "alt");
    v = upconf_str(c->conf, p, "copy-on-select", 0);
    if (v)
        c->w.copy_on_select = str_ieq(v, "on");
    v = upconf_str(c->conf, p, "wheel", 0);
    if (v)
        c->w.wheel_scroll = !str_ieq(v, "ignore");
    v = upconf_str(c->conf, p, "palette", 0);
    if (v)
        upconf_palette_parse(v, c->w.pal16); /* 0x01RRGGBB, 0 = not remapped */
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
    copy_str(c->w.title, "vtcon", sizeof(c->w.title));
    c->w.pers = VT_XTERM;
    if (node_named(c, "CON") || node_named(c, "RAW"))
        c->w.pers = VT_AMIGA; /* options below may still choose another */
    if (node_named(c, "RAW"))
        c->raw = 1;
    c->w.fg_rgb = c->w.bg_rgb = VR_KEEP;
    vtwin_profile_defaults(&c->w); /* a profile may change them */
    copy_str(c->profile, "default", sizeof(c->profile));
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
                copy_str(c->w.title, field, sizeof(c->w.title));
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
            c->w.pers = VT_XTERM;
        } else if (str_ieq(field, "AMIGA")) {
            c->w.pers = VT_AMIGA;
        } else if (str_ieq(field, "PCANSI")) {
            c->w.pers = VT_PCANSI;
        } else if (str_ieq(field, "LATIN1")) {
            c->w.latin1 = 1;
        } else if (str_ieq(field, "DARK")) {
            c->w.fg_rgb = 0xC0C0C0UL; /* light grey on black */
            c->w.bg_rgb = 0x000000UL;
            colours = 1;
        } else if (str_ieq(field, "LIGHT")) {
            colours = 1; /* the screen's text and background pens */
        } else if (str_ipre(field, "FG", &rest)) {
            c->w.fg_rgb = parse_rgb(rest);
            colours = 1;
        } else if (str_ipre(field, "BG", &rest)) {
            c->w.bg_rgb = parse_rgb(rest);
            colours = 1;
        } else if (str_ieq(field, "CP437")) {
            c->w.cp437 = 1;
        } else if (str_ipre(field, "SCREEN", &rest)) {
            copy_str(c->screen, rest, sizeof(c->screen));
        } else if ((k = alt_font_option(field, &rest)) != 0) {
            /* FONT1..FONT9 name.font size: SGR 11-19; FRAKTUR: SGR 20 */
            parse_font(rest, c->w.altname[k], sizeof(c->w.altname[k]), &c->w.altsize[k]);
        } else if (str_ipre(field, "FONT", &rest)) {
            /* FONT name.font size */
            parse_font(rest, c->w.fontname, sizeof(c->w.fontname), &c->w.fontsize);
        } else if (str_ipre(field, "PROFILE", &rest)) {
            /* PROFILE name: the config profile (the file's, else "default") */
            while (*rest == ' ')
                rest++;
            copy_str(c->profile, rest, sizeof(c->profile));
        }
        fno++;
        if (*s != '/')
            break;
        s++;
    }
    c->colours_spec = colours;
    /* Black background unless the options chose colours (owner, 2026-09-29:
     * program colours sat on the Workbench's grey). The amiga personality
     * keeps the screen's pens: CON: programs draw with pens 0-3 as the
     * Workbench has them. LIGHT asks for the screen's pens anywhere. */
    if (!colours && c->w.pers != VT_AMIGA) {
        c->w.fg_rgb = 0xC0C0C0UL;
        c->w.bg_rgb = 0x000000UL;
    }
    apply_profile(c); /* the profile's values, under the spec's own options */
}

static void le_out(void *u, const unsigned char *b, long n);
static void history_load(con *c);
static void close_window(con *c);

static void find_close(con *c); /* the find prompt (see the find prompt section) */

static int open_window(con *c)
{
    struct Screen *scr;
    struct TagItem tags[14];
    struct Window *win;
    int n = 0;

    c->winch_closing = 0; /* read/write retry the open: arm the handler again */
    DBG("lockpub", 0, 0);
    scr = LockPubScreen(c->screen[0] ? (UBYTE *)c->screen : 0);
    if (!scr)
        scr = LockPubScreen(0);
    if (!scr)
        return 0;
    c->locked = scr;
    vtwin_open_font(&c->w);
    if (c->foreign) {
        /* WINDOW 0xaddr: use it as it is; add the IDCMP we need */
        win = c->foreign;
        c->foreign_idcmp = win->IDCMPFlags;
        if (!ModifyIDCMP(win, c->foreign_idcmp | IDCMP_RAWKEY | IDCMP_NEWSIZE |
                         IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS |
                         IDCMP_MOUSEMOVE | IDCMP_EXTENDEDMOUSE))
            return 0;
        SetFont(win->RPort, c->w.font);
        goto have_window;
    }
    tags[n].ti_Tag = WA_Left;        tags[n++].ti_Data = c->wx;
    tags[n].ti_Tag = WA_Top;         tags[n++].ti_Data = c->wy;
    tags[n].ti_Tag = WA_Width;       tags[n++].ti_Data = c->ww;
    tags[n].ti_Tag = WA_Height;      tags[n++].ti_Data = c->wh;
    tags[n].ti_Tag = WA_Title;       tags[n++].ti_Data = (ULONG)c->w.title;
    tags[n].ti_Tag = WA_Flags;       tags[n++].ti_Data = c->wflags;
    tags[n].ti_Tag = WA_IDCMP;       tags[n++].ti_Data = IDCMP_RAWKEY | IDCMP_NEWSIZE |
        IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE |
        IDCMP_EXTENDEDMOUSE | IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW;
    tags[n].ti_Tag = WA_PubScreen;   tags[n++].ti_Data = (ULONG)scr;
    tags[n].ti_Tag = WA_MinWidth;    tags[n++].ti_Data = 80;
    tags[n].ti_Tag = WA_MinHeight;   tags[n++].ti_Data = 40;
    tags[n].ti_Tag = WA_MaxWidth;    tags[n++].ti_Data = (ULONG)~0;
    tags[n].ti_Tag = WA_MaxHeight;   tags[n++].ti_Data = (ULONG)~0;
    tags[n].ti_Tag = WA_AutoAdjust;  tags[n++].ti_Data = TRUE;
    tags[n].ti_Tag = TAG_DONE;       tags[n].ti_Data = 0;
    DBG("openwindow", scr, c->w.font);
    win = OpenWindowTagList(0, tags);
    DBG("window", win, 0);
    if (!win)
        return 0;
have_window:
    if (!vtwin_attach(&c->w, win)) {
        c->w.win = win; /* close_window closes it */
        close_window(c); /* no window without its grid: read/write retry the open */
        return 0;
    }
    DBG("vt_new", c->w.t, 0);
    le_init(&c->le, c->w.t, le_out, c);
    c->le.utf8 = c->w.pers == VT_XTERM && !c->w.latin1 && !c->w.cp437;
    history_load(c); /* the saved history, read by a worker */
    DBG("vr_init", c->w.r.cols, c->w.r.rows);
    return 1;
}

static void close_window(con *c)
{
    struct Window *win = c->w.win;
    c->winch_closing = 1; /* stop naming this window before we dismantle it */
    find_close(c); /* the prompt belongs to the window */
    DBG("close_window", win, c->w.t);
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
    vtwin_detach(&c->w); /* engine, renderer, fonts; the frame clock stops */
    if (win) {
        if (win == c->foreign)
            ModifyIDCMP(win, c->foreign_idcmp); /* hand it back as we found it */
        else
            CloseWindow(win);
    }
    if (c->locked) {
        UnlockPubScreen(0, c->locked);
        c->locked = 0;
    }
}

/* ---- output ---------------------------------------------------------------- */

static void output(con *c, const vt_u8 *b, long n)
{
#ifdef VTCON_DEBUG
    struct EClockVal e0, e1;
    ReadEClock(&e0);
#endif
    if (!c->w.t)
        return;
    if (!c->le.len)
        c->le.started = 0; /* the next line starts wherever this output ends */
    vtwin_write(&c->w, b, n);
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

/* Is read p from a process a shell holds (ACTION_VTCON_HOLD)? */
static int read_held(con *c, struct DosPacket *p)
{
    struct Task *t = p->dp_Port ? (struct Task *)p->dp_Port->mp_SigTask : 0;
    int i;
    for (i = 0; i < c->nheld; i++)
        if (c->held[i] == t)
            return 1;
    return 0;
}

/* The first queued read that may be answered, or -1. */
static int next_read(con *c)
{
    int i;
    for (i = 0; i < c->nreads; i++)
        if (!read_held(c, c->reads[i]))
            return i;
    return -1;
}

static void drop_read(con *c, int k)
{
    int i;
    for (i = k + 1; i < c->nreads; i++)
        c->reads[i - 1] = c->reads[i];
    c->nreads--;
}

static void hold_task(con *c, struct Task *t, int on)
{
    int i, k = 0;
    /* drop the holds of processes that are gone, and t's own */
    for (i = 0; i < c->nheld; i++) {
        int alive;
        Forbid();
        alive = task_alive(c->held[i]);
        Permit();
        if (alive && c->held[i] != t)
            c->held[k++] = c->held[i];
    }
    c->nheld = k;
    if (on && t && c->nheld < 8)
        c->held[c->nheld++] = t;
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
    if (c->w.t)
        vt_set_onlcr(c->w.t, c->w.pers == VT_XTERM);
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
        if (c->w.t)
            vt_set_onlcr(c->w.t, 0); /* OPOST decides now (vttest m1 s04: raw LF is LF) */
    }
    c->tty_owner = owner;
}

/* Reads in termios mode: ICANON a line or an EOF; otherwise VMIN bytes,
 * or what came within VTIME tenths of a second. */
static void tty_reads(con *c)
{
    int k;
    while ((k = next_read(c)) >= 0) {
        struct DosPacket *p = c->reads[k];
        long n = 0;
        int eof, arm;
        int act = ld_read_action(&c->ld, c->rtimer_fired, &arm);
        if (act == LD_RD_WAIT) {
            if (arm && !c->rtimer_busy)
                rtimer_start(c, (ULONG)c->ld.t.c_cc[LD_VTIME]);
            break;
        }
        if (act == LD_RD_TAKE)
            n = ld_read(&c->ld, (unsigned char *)p->dp_Arg2, p->dp_Arg3, &eof);
        rtimer_stop(c);
#ifdef VTCON_DEBUG
        dbg("TTYREAD task/n", (LONG)p->dp_Port->mp_SigTask, n);
#endif
        reply(p, n < 0 ? 0 : n, 0);
        drop_read(c, k);
    }
    if (c->waitchar && ld_input_pending(&c->ld))
        finish_waitchar(c, DOSTRUE);
}

static void service_reads(con *c)
{
    if (tty_active(c)) {
        tty_reads(c);
        return;
    }
    int k;
    while ((k = next_read(c)) >= 0) {
        struct DosPacket *p = c->reads[k];
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
        drop_read(c, k);
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

/* The process Ctrl-C goes to and whose directory completion uses
 * (brk.h). Call under Forbid to use the answer. */
static struct Task *break_task(con *c)
{
    return brk_task(&c->brk);
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
        int k = vt_encode_key(c->w.t, (unsigned char)*s, 0, out);
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
            if (!c->w.t)
                continue; /* the window closed (AUTO): no line to colour */
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
        if (!c->w.t)
            continue; /* the window closed (AUTO): the answer has no line */
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
            DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* none, or several with no more in common */
            continue;
        }
        type_text(c, q->add);
        check_command(c); /* the completed word gets its colour */
        if (q->matches > 1)
            DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* completed as far as they agree */
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
                n += vt_encode_key(c->w.t, (unsigned char)*name, 0, enc + n);
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

static void send_break(con *c, ULONG sig)
{
#ifdef VTCON_DEBUG
    struct Task *t = brk_send(&c->brk, sig);
    DBG("break to", (long)sig, (long)t);
#else
    brk_send(&c->brk, sig);
#endif
}

static void cooked_key(con *c, const vt_u8 *b, int n, long key, int mods);

/* A typed key, encoded by the window (vtwin_key): the break keys, the
 * line discipline, the line editor or the raw stream. */
static void typed(con *c, const vt_u8 *out, int n, long key, int mods)
{
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
     * without ISIG does. ^\ and ^Z are the CTRL_E and CTRL_F breaks too, as
     * in termios mode (VQUIT, VSUSP): a shell running a job that never set
     * termios still hears the suspend key (vsh S8). */
    if (n == 1 && !key && (!c->raw || c->w.pers != VT_XTERM)) {
        ULONG brk = 0;
        if (out[0] >= 0x03 && out[0] <= 0x06)
            brk = SIGBREAKF_CTRL_C << (out[0] - 0x03);
        else if (out[0] == 0x1C)
            brk = SIGBREAKF_CTRL_E;
        else if (out[0] == 0x1A)
            brk = SIGBREAKF_CTRL_F;
        if (brk) {
            send_break(c, brk);
            if (!c->raw)
                return;
        }
    }
    if (c->raw) {
        in_append(c, out, n);
    } else {
        cooked_key(c, out, n, key, mods);
    }
    service_reads(c);
}

/* ---- find prompt ----------------------------------------------------------------- */

/* Right Amiga F raises this: a one-line window with a string gadget, beside
 * the console window. Enter searches and closes, so F again repeats the last
 * query (vtwin_find(NULL) re-searches it); Escape closes without searching.
 *
 * The prompt is a window and a port of its own, so the console window keeps
 * the IDCMP it was opened with: output, resize and the wheel keep working
 * while the query is typed, which is the point of a find in a live terminal.
 *
 * This NDK has neither REQ_STR_GETANSWER nor a requester tag that asks for a
 * string, so the gadget is a hand-built struct filled field by field, exactly
 * as the Prefs editor does it (prefs/upprefs.c mk()). Intuition maintains the
 * text in StringInfo: the keys are not decoded here, only Enter and Escape
 * are acted on. */

#define FIND_STRING GTYP_GADGET0002   /* the type the Prefs editor uses for a string */
#define FIND_GAD_X  6
#define FIND_GAD_Y  3
#define FIND_GAD_W  340
#define FIND_GAD_H  18

static void find_close(con *c)
{
    if (c->find_win) {
        CloseWindow(c->find_win); /* the gadget is a plain struct, not allocated */
        c->find_win = 0;
        c->find_gad = 0;
    }
    if (c->find_port) {
        struct Message *m;
        /* whatever the prompt still holds must not answer a dead port */
        while ((m = GetMsg(c->find_port)))
            ReplyMsg(m);
        DeleteMsgPort(c->find_port);
        c->find_port = 0;
    }
}

/* Search for the text in the prompt, or repeat the console's last query when
 * the prompt is empty. Beeps when nothing matched: a find that scrolled
 * nowhere must not be silence. */
static void find_run(con *c)
{
    const char *q = c->find_buf; /* the gadget's StringInfo.Buffer */
    if (vtwin_find(&c->w, q[0] ? q : 0)) {
        find_close(c); /* the line is on screen: the prompt has done its job */
        return;
    }
    DisplayBeep(c->find_win ? c->find_win->WScreen
                            : (c->w.win ? c->w.win->WScreen : 0));
}

static int find_open(con *c)
{
    struct TagItem tags[14];
    struct Gadget *g;
    int n = 0;
    if (!c->w.win)
        return 0;
    if (c->find_win) {
        WindowToFront(c->find_win); /* already up: keep typing into the same query */
        return 1;
    }
    if (!c->find_port)
        c->find_port = CreateMsgPort();
    if (!c->find_port)
        return 0;
    /* beside the console window, moved back on screen if it hangs over */
    tags[n].ti_Tag = WA_Left;       tags[n++].ti_Data =
        (LONG)(c->w.win->LeftEdge + c->w.win->Width + 2);
    tags[n].ti_Tag = WA_Top;        tags[n++].ti_Data =
        (LONG)(c->w.win->TopEdge + c->w.win->Height + 2);
    tags[n].ti_Tag = WA_Width;      tags[n++].ti_Data = 356;
    tags[n].ti_Tag = WA_Height;     tags[n++].ti_Data = 26;
    tags[n].ti_Tag = WA_Title;      tags[n++].ti_Data = (ULONG)"Find";
    tags[n].ti_Tag = WA_IDCMP;      tags[n++].ti_Data = IDCMP_GADGETUP | IDCMP_RAWKEY;
    tags[n].ti_Tag = WA_PubScreen;  tags[n++].ti_Data = (ULONG)c->w.win->WScreen;
    c->find_buf[0] = 0;
    g = &c->find_gadget;
    g->NextGadget = 0;
    g->LeftEdge = FIND_GAD_X;
    g->TopEdge = FIND_GAD_Y;
    g->Width = FIND_GAD_W;
    g->Height = FIND_GAD_H;
    g->Flags = 0;
    g->Activation = GACT_IMMEDIATE; /* Intuition types into it (StringInfo) */
    g->GadgetType = FIND_STRING;
    g->GadgetRender = 0;
    g->SelectRender = 0;
    g->GadgetText = 0;
    g->MutualExclude = 0;
    g->SpecialInfo = (APTR)&c->find_si;
    g->GadgetID = 0;
    g->UserData = 0;
    c->find_si.Buffer = c->find_buf;
    c->find_si.MaxChars = (WORD)sizeof(c->find_buf);
    c->find_si.BufferPos = 0;
    c->find_si.DispPos = 0;
    tags[n].ti_Tag = WA_Gadgets;    tags[n++].ti_Data = (ULONG)g;
    tags[n].ti_Tag = TAG_DONE;      tags[n++].ti_Data = 0;
    c->find_win = OpenWindowTagList(0, tags);
    if (!c->find_win) {
        find_close(c);
        return 0;
    }
    /* the window answers this port now: nothing has been waited on yet */
    c->find_win->UserPort = c->find_port;
    c->find_gad = g; /* in the window's chain: RefreshGadgets and events use it */
    RefreshGadgets(c->find_gad, c->find_win, 0);
    return 1;
}

/* The prompt's events, drained whenever the console is idle. Enter runs the
 * search (find_run may close the prompt), Escape closes it. Everything else
 * is Intuition's: it edits the string and redraws the gadget itself. */
static void find_idcmp(con *c)
{
    struct IntuiMessage *im;
    struct Message *m;
    while (c->find_port && (m = GetMsg(c->find_port))) {
        int run;
        ULONG cls;
        im = (struct IntuiMessage *)m;
        cls = im->Class;
        if (cls != IDCMP_GADGETUP &&
            !(cls == IDCMP_RAWKEY && (im->Code == 0x0D || im->Code == 0x1B))) {
            ReplyMsg(m);
            continue;
        }
        run = cls == IDCMP_GADGETUP || im->Code == 0x0D;
        ReplyMsg(m);
        if (run)
            find_run(c); /* may close the prompt and its port */
        else
            find_close(c);
        return; /* one action per pass, and the port may be gone now */
    }
}

static void idcmp(con *c)
{
    struct IntuiMessage *im;
    if (c->w.win && !IsListEmpty(&c->w.win->UserPort->mp_MsgList))
        vtwin_render(&c->w); /* resize, refresh, selection: on the current screen */
    while (c->w.win && (im = (struct IntuiMessage *)GetMsg(c->w.win->UserPort))) {
        ULONG cls = im->Class;
        switch (cls) {
        case IDCMP_RAWKEY:
            /* Right Amiga F: the find prompt, opened here rather than in
             * vtwin because the prompt is a window of ours */
            if ((im->Qualifier & IEQUALIFIER_RCOMMAND) && im->Code == 0x46) {
                find_open(c);
                break;
            }
            /* IAddress: the previous two down keys (dead keys) */
            vtwin_key(&c->w, im->Code, im->Qualifier, im->IAddress ? *(ULONG *)im->IAddress : 0,
                      im->Seconds, im->Micros);
            break;
        case IDCMP_NEWSIZE:
            vtwin_resize(&c->w);
            break;
        case IDCMP_REFRESHWINDOW:
            vtwin_refresh(&c->w);
            break;
        case IDCMP_CLOSEWINDOW:
            /* AUTO: the gadget shuts the window only, and the next read or
             * write opens it again -- unless a read waits (then it is EOF as
             * without AUTO) or DISK_INFO gave the window out (ROM 40.x and
             * V47, tools/rig/autoprobe_rig.py) */
            if (c->auto_open && !c->auto_held && !c->nreads && c->w.win != c->foreign) {
                c->auto_shut = 1;
                break;
            }
            c->closing = 1;
            if (!c->raw)
                c->eof = 1;
            else if (!vtwin_raw_report(&c->w, 11)) /* IECLASS_CLOSEWINDOW, if asked */
                send_break(c, SIGBREAKF_CTRL_C);
            break;
        case IDCMP_MOUSEBUTTONS:
        case IDCMP_MOUSEMOVE:
            vtwin_mouse(&c->w, cls == IDCMP_MOUSEMOVE, im->Code, im->Qualifier, im->MouseX, im->MouseY);
            break;
        case IDCMP_EXTENDEDMOUSE:
            /* the mouse wheel (OS 3.9+): the IntuiWheelData behind IAddress.
             * WheelX is the main wheel; forward (up) is the negative delta.
             * Anything else extended-mouse reports is not ours. */
            if (im->Code == IMSGCODE_INTUIWHEELDATA && im->IAddress) {
                struct IntuiWheelData *wd = (struct IntuiWheelData *)im->IAddress;
                if (wd->Version == INTUIWHEELDATA_VERSION)
                    vtwin_wheel(&c->w, wd->WheelX < 0, im->MouseX, im->MouseY);
            }
            break;
        default:
            break;
        }
        ReplyMsg((struct Message *)im);
    }
    if (c->auto_shut) {
        c->auto_shut = 0;
        close_window(c);
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
    if (!c->rom_io || !c->w.t)
        return;
    cu = (struct ConUnit *)c->rom_io->io_Unit;
    cu->cu_XMax = (WORD)(vt_cols(c->w.t) - 1);
    cu->cu_YMax = (WORD)(vt_rows(c->w.t) - 1);
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
    /* Runs off input.device's 10 Hz timer, so it can fire while close_window
     * is dismantling the window it would name. An injected IECLASS_SIZEWINDOW
     * that reaches Intuition after the window is gone is a stale pointer. */
    if (c->winch_closing)
        return chain;
    if (c->winch_pending) {
        c->winch_pending = 0;
        c->winch_ev.ie_NextEvent = chain;
        return &c->winch_ev;
    }
    return chain;
}

static void post_sizewindow(con *c)
{
    if (!c->w.win)
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
    c->winch_ev.ie_EventAddress = (APTR)c->w.win;
    c->winch_pending = 1;
    DBG("sigwinch", c->winch_added, 0);
}

/* Is console.device UP-Term's (the 'UPTC' magic at offset 36, DD13)? */
static int upterm_device(void)
{
    struct Library *d;
    int ours;
    Forbid();
    d = (struct Library *)FindName(&SysBase->DeviceList, (STRPTR)"console.device");
    ours = upc_is_upterm(d);
    Permit();
    return ours;
}

static struct IOStdReq *rom_console(con *c)
{
    /* DISK_INFO callers get a real console.device unit on our window, so
     * programs that read ConUnit fields (window size in characters) or
     * send it CMD_WRITE keep working. Opened on first use only; its
     * cursor is switched off at once, our renderer owns the window. */
    if (c->rom_io || !c->w.win)
        return c->rom_io;
    c->rom_port = CreateMsgPort();
    if (!c->rom_port)
        return 0;
    c->rom_io = (struct IOStdReq *)CreateIORequest(c->rom_port, sizeof(struct IOStdReq));
    if (!c->rom_io)
        return 0;
    c->rom_io->io_Data = c->w.win;
    c->rom_io->io_Length = sizeof(struct Window);
    DBG("rom open", c->w.win, 0);
    /* with UP-Term's console.device in, ask it for the ROM's unit: a second
     * engine must not draw on this window (console plan DD18, D5.1) */
    if (OpenDevice((STRPTR)"console.device", CONU_STANDARD, (struct IORequest *)c->rom_io,
                   upterm_device() ? UPCONFLAG_ROM : 0)) {
        DeleteIORequest((struct IORequest *)c->rom_io);
        c->rom_io = 0;
        return 0;
    }
    c->rom_io->io_Command = CMD_WRITE;
    c->rom_io->io_Data = (APTR)"\x9b" "0 p";
    c->rom_io->io_Length = 4;
    DBG("rom write", 0, 0);
    DoIO((struct IORequest *)c->rom_io);
    DBG("rom sync", 0, 0);
    sync_size(c);
    /* opening a console unit on a window clears it in the ROM console's
     * pens: every ixemul program asks DISK_INFO at startup, and the window
     * turned Workbench grey with black only behind the text drawn after
     * (rig, 2026-09-29). Our grid is the truth: draw it all again. */
    if (c->w.t)
        vr_redraw(&c->w.r);
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
            brk_open(&c->brk, p->dp_Port);
            DBG("open window", c->ww, c->wh);
            if (!c->auto_open && !open_window(c)) {
                DBG("open failed", c->w.win, c->w.t);
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
#ifdef VTCON_DEBUG
        DBG("prof writes/bytes", c->prof_writes, c->prof_bytes);
        DBG("prof out", c->prof_out, 0);
        DBG("prof direct/text", c->w.r.n_direct, c->w.r.n_text);
#endif
        reply(p, DOSTRUE, 0);
        return;
    case ACTION_READ:
        if (!c->w.win && !open_window(c)) {
            reply(p, -1, ERROR_NO_FREE_STORE);
            return;
        }
        if (c->nreads < READ_Q) {
            c->reads[c->nreads++] = p;
#ifdef VTCON_DEBUG
            dbg("READ task/nreads", (LONG)p->dp_Port->mp_SigTask, c->nreads);
#endif
            service_reads(c);
        } else {
            reply(p, -1, ERROR_NO_FREE_STORE);
        }
        return;
    case ACTION_WRITE:
        if (!c->w.win && !open_window(c)) {
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
        /* a newer WAIT_CHAR ends the older one: it is stale (ixemul's
         * select sends one per call, and one with no timeout before it
         * closes a file whose select is still out) */
        if (c->waitchar)
            finish_waitchar(c, DOSFALSE);
#ifdef VTCON_DEBUG
        dbg("WAIT task/timeout", (LONG)p->dp_Port->mp_SigTask, p->dp_Arg1);
        dbg("WAIT in/eof", c->in_len, c->eof);
        dbg("WAIT tty/pending", c->tty, c->tty ? ld_input_pending(&c->ld) : -1);
#endif
        if (c->in_len || c->eof || (tty_active(c) && ld_input_pending(&c->ld))) {
            reply(p, DOSTRUE, 0);
        } else if (p->dp_Arg1 <= 0) {
            reply(p, DOSFALSE, 0); /* a poll */
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
            reply(p, DOSTRUE, 1); /* Res2 1: termios mode (vtcon_packets.h) */
        } else {
            /* what the window does now, in termios terms */
            vt_termios t;
            ld_defaults(&t);
            if (c->raw)
                t.c_lflag &= ~(ld_flag)(LD_ICANON | LD_ECHO | LD_ISIG | LD_IEXTEN);
            CopyMem(&t, (APTR)p->dp_Arg2, sizeof(t));
            reply(p, DOSTRUE, 0);
        }
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
    case ACTION_VTCON_NREAD:
        if (tty_active(c))
            reply(p, ld_nread(&c->ld), 0);
        else
            reply(p, c->raw ? c->in_len : line_end(c), 0);
        return;
    case ACTION_VTCON_GWINSZ:
        if (!p->dp_Arg2 || !c->w.t) {
            reply(p, DOSFALSE, p->dp_Arg2 ? ERROR_OBJECT_NOT_FOUND : ERROR_REQUIRED_ARG_MISSING);
            return;
        }
        {
            vt_winsize *ws = (vt_winsize *)p->dp_Arg2;
            ws->ws_row = (unsigned short)vt_rows(c->w.t);
            ws->ws_col = (unsigned short)vt_cols(c->w.t);
            ws->ws_xpixel = (unsigned short)(ws->ws_col * c->w.r.cw);
            ws->ws_ypixel = (unsigned short)(ws->ws_row * c->w.r.ch);
        }
        reply(p, DOSTRUE, 0);
        return;
    case ACTION_VTCON_SWINSZ:
        reply(p, DOSFALSE, ERROR_ACTION_NOT_KNOWN); /* a window's size is the window's */
        return;
    case ACTION_VTCON_HOLD:
        hold_task(c, (struct Task *)p->dp_Arg2, p->dp_Arg3 != 0);
        reply(p, DOSTRUE, 0);
        service_reads(c);
        return;
    case ACTION_VTCON_WORDS:
        take_words(c, p);
        reply(p, DOSTRUE, 0);
        return;
    case ACTION_CHANGE_SIGNAL:
        brk_change(&c->brk, (struct MsgPort *)p->dp_Arg2);
        reply(p, DOSTRUE, 0);
        return;
    case ACTION_DISK_INFO: {
        struct InfoData *id = (struct InfoData *)BADDR(p->dp_Arg1);
        LONG i;
        if (!c->w.win)
            open_window(c); /* as V47's con-handler does: the caller wants the window */
        c->auto_held = 1;   /* the caller has the window: AUTO no longer shuts it */
        for (i = 0; i < (LONG)sizeof(*id); i++)
            ((UBYTE *)id)[i] = 0;
        id->id_DiskType = 0x434F4E00L; /* 'CON\0' (no NDK name; value unverified against the ROM) */
        id->id_VolumeNode = (BPTR)c->w.win;
        id->id_InUse = (LONG)rom_console(c);
        reply(p, DOSTRUE, 0);
        return;
    }
    case ACTION_UNDISK_INFO:
        /* V47: AUTO shuts the window on its close gadget again (ROM 40.x
         * does not know the packet and keeps the window for good) */
        c->auto_held = 0;
        reply(p, DOSTRUE, 0);
        return;
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
    vtwin_init(&c->w, &host, c); /* the frame clock */
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
    c->conf = (upconf *)AllocVec(sizeof(upconf), MEMF_ANY | MEMF_CLEAR);
    config_load(c); /* ENVARC:up-term/up-term, read by a worker (see above) */

    for (;;) {
        ULONG wait = 1UL << c->port->mp_SigBit;
        struct Message *m;
        if (c->w.win)
            wait |= 1UL << c->w.win->UserPort->mp_SigBit;
        if (c->timer_port)
            wait |= 1UL << c->timer_port->mp_SigBit;
        if (c->comp_port)
            wait |= 1UL << c->comp_port->mp_SigBit;
        wait |= vtwin_sigmask(&c->w);
        if (c->find_port)
            wait |= 1UL << c->find_port->mp_SigBit;
        /* No trace lines in the loop itself: each log Write's reply re-arms
         * our DOS signal, so a trace here wakes the loop forever and filled
         * RAM: on the rig (2026-09-29). */
        wait = Wait(wait);
        while ((m = GetMsg(c->port)))
            packet(c, (struct DosPacket *)m->mn_Node.ln_Name);
        vtwin_tick(&c->w); /* the frame clock: draw what is due, blink */
        find_idcmp(c); /* the find prompt, while it is open */
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
    vtwin_render(&c->w);
    close_window(c);
    vtwin_cleanup(&c->w);
    Forbid();
    if (c->node && c->node->dn_Task == c->port)
        c->node->dn_Task = 0; /* never leave DOS a port that is going away */
    Permit();
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
    if (c->conf)
        FreeVec(c->conf);
    FreeVec(c);
    return 0;
}
