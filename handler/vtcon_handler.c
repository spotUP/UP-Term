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
#include <dos/notify.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <devices/timer.h>
#include <devices/inputevent.h>
#include <devices/input.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/displayinfo.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/console.h>
#include <proto/diskfont.h>
#include <proto/gadtools.h>
#include <libraries/gadtools.h>
#include <proto/timer.h>

#include <string.h>
#include "../engine/vtengine.h"
#include "../render/amiga_render.h"
#include "../render/vtwin.h"
#include "../device/upc_public.h"
#include "sbar_gad.h"
#include "clip.h"
#include "lineedit.h"
#include "complete.h"
#include "menu_ids.h"
#include "slash.h"
#include "brk.h"
#include "vtcon_packets.h"
#include "../tty/ldisc.h"
#include "../config/termurl.h"
#include "../config/upconf.h"
#include "../prefs/prefs_core.h"

/* rexx/rexxio.h: ARexx PUSH and QUEUE */
#ifndef ACTION_FORCE
#define ACTION_FORCE 2001L /* the V47 Shell: a command line into the console's input */
#endif
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
struct Library *GadToolsBase; /* the window's menu (may be 0: no menu) */
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

#define TAB_MAX 9 /* tabs in one window */

/* /theme with no name (W30): the themes drawer's list under the line
 * (lineedit's le_menu), the chosen entry's colours on the window once the
 * bar rests (le_menu_rested on the frame clock: a theme is a full repaint,
 * and holding Down put every theme passed on the window -- owner
 * 2026-10-05), Escape putting the window's own back at once. Allocated with
 * the names after it while the list is up and until the last theme read
 * is in. */
struct theme_menu {
    le_menu m;
    ULONG fg, bg, cur, sfg, sbg, pal[16]; /* the window's colours when it opened */
    int asked;                   /* the entry a read is out for (-1 none) */
    int last;                    /* the entry read and taken last, a theme or not (-1 none) */
    int shown;                   /* the entry whose colours the window has (-1: its own) */
    int end;                     /* LE_MENU_TAKE / _CANCEL once the list is closed */
    char shown_path[COMPLETE_MAX]; /* the file `shown` was read from */
};

typedef struct con {
    struct MsgPort *port;
    vtwin w;                     /* the window: engine, renderer, fonts, frame clock (render/vtwin.h) */
    struct Screen *locked;       /* the public screen we opened on */
    int opens;
    int raw;
    int medium;                  /* V47 medium mode (SetMode 2): cooked, but TAB reports at once */
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
    /* a screen of its own (OWNSCREEN, FULLSCREEN, PUBSCREEN name; plan
     * 2026-10-03-screens-and-dctelnet.md P1): 0 none, 1 own, 2 full screen */
    int own_screen;
    char pubname[32];            /* PUBSCREEN name; "" = UP-Term, UP-Term.2 ... */
    ULONG mode_id;               /* SCREENMODE 0x...; INVALID_ID = the Workbench's */
    int depth;                   /* DEPTH n; 0 = 4 on a native screen, 8 on a card */
    struct Screen *myscreen;     /* the screen opened for this window */
    int screen_want;             /* a move to this screen kind asked for (-1 none): done after the IDCMP loop */
    struct Screen *screen_closing; /* ours, its close refused (a visitor still on it): tried again */
    int inactive;
    int auto_open;               /* AUTO: no window until the first read or write */
    int auto_held;               /* DISK_INFO holds an AUTO window open until UNDISK_INFO (V47) */
    int auto_shut;               /* the close gadget shut an AUTO window: close it after idcmp() */
    int spec_parsed;
    char profile[UC_NAME];       /* the config profile (spec's PROFILE; "default") */
    char link_open[UC_MAX_VALUE]; /* the profile's link-open, or /link-open's ("": OpenURL %s) */
    int colours_spec;            /* the spec chose colours (DARK/FG/BG/LIGHT): it beats the profile */
    ULONG spec_fg, spec_bg;      /* the colours before any profile (a profile switch starts there) */
    upconf *save_work;           /* Save settings to profile: the table once the file is written
                                  * (while the worker writes it; then the window's c->conf) */
    /* tabs (plans/2026-10-03-tabs.md): a host owns the window, a tab is a
     * process drawing into it */
    struct Window *own_win;      /* the window this process opened and owns (0: a tab's) */
    char tab_host[48];           /* TAB name: become a tab of that host */
    int is_tab;                  /* a tab: the window is its host's */
    struct Message *hiding;      /* host: the HIDE the old tab has not answered yet */
    int shown;                   /* a tab: the active one (draws, its menus on the window) */
    struct MsgPort *tab_port;    /* host: its public port (tabs register); tab: from its host */
    struct MsgPort *tab_reply;   /* replies to the messages this process sent */
    struct MsgPort *host_pub;    /* a tab: its host's public port */
    struct RastPort tab_rp;      /* a tab: its own RastPort on the shared window */
    struct con *tab_list[TAB_MAX]; /* host: its tabs in bar order, itself among them */
    int ntabs, active;
    int host_only;               /* host: its own shell ended, it keeps the window for the tabs */
    char tab_name[32];           /* host: its public port's name */
    struct MsgPort *watch_port;  /* the live-update watcher's replies (watch_worker) */
    struct watch_msg *watch;
    struct Process *watch_task;
    int watch_held;              /* the window has the message (the watcher waits for it) */
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
    int reader_shell;            /* the last ACTION_READ came from a shell (W31: colour its first word) */
    int tabs;                    /* Tabs in a row */
    int kingcon;                 /* profile completion = kingcon: KingCON's keys and
                                  * selection window (research/2026-10-02_kingcon-completion.md) */
    unsigned long edits;         /* keys that changed the line so far */
    unsigned long comp_edits;    /* edits when the running completion started */
    int comp_partial;            /* W22: the last command completion was partial (1 unix,
                                  * 2 kingcon): asked again when the warm-up ends */
    unsigned long partial_edits; /* edits after its answer: a key since drops the refine */
    unsigned long partial_gen;   /* complete_warm_gen() its lookup saw */
    int next_cold;               /* the next completion may read a directory (the refine) */
    int partial_now;             /* no warm-up to wait for: refine at once */
    struct Menu *menustrip;     /* the window's menu (GadTools), 0 without one */
    /* the scroll bar (SB1): the window owner's gadget in its right border,
     * showing the active terminal's knob (a tab's arrives by TM_KNOB) */
    int sbar_on;                 /* the setting: profile scrollbar = show | hide, the menu */
    sbar_gad sbar;
    sbar_knob knob_last;         /* the knob the window shows, for a scroll bar turned on */
    int knob_last_valid;
    char *menu;                  /* the last completion's names (COMPLETE_NAMES, made at the first menu) */
    int menu_len, menu_n, menu_i, menu_start;
    struct theme_menu *tm;       /* /theme's list (W30), 0 when none */
    int line_held;               /* a typed /theme's empty line waits for its list to close */
    char *theme_file;            /* the theme file the window has (COMPLETE_MAX; 0: none yet):
                                  * the next theme requester and list start in its drawer */
    /* the find prompt (Right Amiga F): its own small window, open while the
     * console keeps running -- the program's output must not stop while a
     * query is typed */
    /* KingCON completion (completion = kingcon): the word the request was
     * for, and the "Select ..." window (see the KingCON section) */
    int kc_start, kc_quote;      /* le_kc_word's answer when the request went out */
    int kc_list;                 /* the request is Ctrl+D's directory listing */
    int kc_style;                /* LE_KC_* from the profile's kingcon-mode (FNCMODE) */
    int kc_info;                 /* kingcon-info = show: .info files listed */
    int kc_cache;                /* kingcon-cache (default on): command directories cached */
    int kc_cyc, kc_cyc_i;        /* an inline cycle (B, or C armed): its entry, -1 before the first */
    int kc_cyc_window;           /* C with W: the next Tab opens the window */
    int kc_cyc_n, kc_cyc_mode;
    char *kc_cyc_names;          /* the cycle's entries, NUL-separated */
    long kc_cyc_len;
    unsigned char *kc_snap;      /* the line before the cycle, LE_MAX bytes */
    int kc_snap_pos;
    struct MsgPort *sel_port;
    struct Window *sel_win;
    APTR sel_vi;
    struct Gadget *sel_glist, *sel_lv;
    struct List sel_list;
    struct Node *sel_nodes;
    char *sel_names;             /* the entries, NUL-separated, with their suffixes */
    int sel_n, sel_i, sel_click; /* entries, selected, last clicked (double-click) */
    ULONG sel_secs, sel_mics;
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
#if defined(VTCON_DEBUG) || defined(VTCON_PROF)
    ULONG prof_out, prof_writes, prof_bytes;
#endif
#ifdef VTCON_PROF
    /* the phase profile (PROF=1 SERIAL=1; ledger S1): EClock ticks spent
     * waiting for work and in all, since the last stream closed; the
     * output and render phases are prof_out and the vtwin's prof_render.
     * Printed to the serial port when a stream closes, then reset. */
    ULONG prof_idle, prof_t0;
    ULONG prof_pk[3], prof_npk[3]; /* packet() time and count: writes, WAIT_CHAR, the rest */
#endif
} con;

/* What one window costs (research/2026-10-04_window-memory.md), held at
 * compile time in the 68k build itself: the build fails when one of
 * these climbs back past its bound. Measured 2026-10-04: con 66126 ->
 * 24402, upconf 56240 -> 17844, a request without lists 11084 -> 852. */
typedef char con_size_bound[sizeof(con) <= 26000 ? 1 : -1];
typedef char upconf_size_bound[sizeof(upconf) <= 18500 ? 1 : -1];
typedef char complete_req_size_bound[sizeof(struct complete_req) <= 1024 ? 1 : -1];

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
static void rtimer_start(con *c, ULONG tenths);

/* In termios mode the read stream is the line discipline: everything for
 * the program -- a key, a report, a paste, a mouse or focus report -- goes
 * through it, as on a Unix tty. 0 when the window is not in termios mode.
 * Bytes put in the cooked buffer instead were never read there, yet made
 * WAIT_CHAR answer yes (tmux's select saw input, its read blocked until
 * the next key); and a paste or a mouse click in a termios program (Claude
 * Code over uptelnet, tmux) never arrived (ledger A1.3). */
static int to_tty(con *c, const vt_u8 *b, int n)
{
    if (!tty_active(c))
        return 0;
    ld_input(&c->ld, b, n);
    if (c->rtimer_busy && c->ld.t.c_cc[LD_VMIN] > 0)
        rtimer_start(c, c->ld.t.c_cc[LD_VTIME]); /* VTIME is between bytes */
    service_reads(c);
    return 1;
}

static void h_reply(void *u, const vt_u8 *b, long n)
{
    con *c = (con *)u;
    /* reports enter the read stream, as the console's do */
    if (to_tty(c, b, (int)n))
        return;
    in_append(c, b, (int)n);
}

/* bytes straight into the read stream (raw reports, mouse, paste marks) */
static void h_input(void *u, const vt_u8 *b, long n)
{
    con *c = (con *)u;
    if (to_tty(c, b, (int)n))
        return;
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
    if (to_tty(c, b, n))
        return;
    if (c->raw)
        in_append(c, b, n);
    else
        cooked_key(c, b, n, key, 0);
    service_reads(c);
}

/* the program reads bytes, not our line editor: Amiga raw mode, or
 * termios mode (paste marks and focus reports are for it) */
static int h_raw(void *u)
{
    con *c = (con *)u;
    return c->raw || tty_active(c);
}

static void sync_size(con *c);
static void post_sizewindow(con *c);

static void h_resized(void *u)
{
    con *c = (con *)u;
    sync_size(c);
    le_resized(&c->le); /* a reflow moved the line being edited */
    if (c->tm && c->tm->m.open)
        c->tm->m.drawn_top = -1; /* and /theme's list: whole at the next key */
    post_sizewindow(c);
}

static void h_titled(void *u);
static void h_open_link(void *u, const char *uri);
static void h_knob(void *u, const sbar_knob *k);
static const vtwin_host host = { h_reply, h_input, h_key, h_pasted, h_raw, h_resized, h_titled, h_open_link,
                                 h_knob };

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

/* The profile file into conf, in a process that may make DOS calls (a
 * worker). ENV: first, as Amiga prefs do (the editor's Use writes there;
 * Install and Save copy to both), ENVARC: when ENV: has none (a fresh boot
 * before ENVARC: was copied). */
static void read_conf(upconf *conf)
{
    BPTR f = Open((STRPTR)"ENV:up-term/up-term", MODE_OLDFILE);
    char *buf;
    if (!f)
        f = Open((STRPTR)"ENVARC:up-term/up-term", MODE_OLDFILE);
    if (!f)
        return;
    /* Read straight up to the cap rather than Seek()ing to the end first:
     * on 3.1 that Seek answers 0 for this file, and the read is skipped. */
    buf = (char *)AllocVec(CONF_MAX + 2, MEMF_ANY);
    if (buf) {
        /* a byte more than the cap: a longer file is used as far as it
         * fits, and marked (overflow) so Save settings to profile does not
         * write it back cut (prefs_stage refuses it) */
        LONG got = Read(f, buf, CONF_MAX + 1);
        LONG use = got > CONF_MAX ? CONF_MAX : got > 0 ? got : 0;
        buf[use] = 0;
        upconf_parse(conf, buf, use);
        if (got > CONF_MAX)
            conf->overflow = 1;
        FreeVec(buf);
    }
    Close(f);
}

static void config_worker(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    struct config_msg *m;

    WaitPort(&me->pr_MsgPort); /* the request, before any DOS call */
    m = (struct config_msg *)GetMsg(&me->pr_MsgPort);
    me->pr_WindowPtr = (APTR)-1; /* no requesters from a file that may be absent */
    read_conf(m->conf);
    Forbid(); /* the opener frees m: end before it can run on */
    ReplyMsg((struct Message *)m);
}

/* Live updates (owner 2026-10-03: "make prefs update all open windows live
 * ... but per tab"): a watcher process per window holds a DOS notification
 * on ENV:up-term/up-term. When UP-Term Prefs (Use or Save), another
 * window's Save settings to profile or an editor writes it, the watcher
 * reads it into a table of its own (m->conf, allocated then) and hands m
 * over; the window keeps that table as its own, frees the one it had and,
 * when its own profile's section changed, applies it live (watch_take).
 * So a window holds one table, not a second one waiting for a change
 * that may never come (research/2026-10-04_window-memory.md). One message
 * goes back and forth, so the table is never read while the other side
 * writes it. Quitting: the window sends m back
 * with quit set, or signals CTRL_C while the watcher holds m; the watcher
 * answers with done set and ends. */
struct watch_msg {
    struct Message msg;
    upconf *conf;
    int quit, done;
};

static void watch_worker(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    struct watch_msg *m;
    struct NotifyRequest nr;
    LONG sig;
    int have = 1, quitting = 0, watching;
    WaitPort(&me->pr_MsgPort);
    m = (struct watch_msg *)GetMsg(&me->pr_MsgPort);
    me->pr_WindowPtr = (APTR)-1;
    sig = AllocSignal(-1);
    memset(&nr, 0, sizeof(nr));
    nr.nr_Name = (STRPTR)"ENV:up-term/up-term";
    nr.nr_Flags = NRF_SEND_SIGNAL;
    nr.nr_stuff.nr_Signal.nr_Task = &me->pr_Task;
    nr.nr_stuff.nr_Signal.nr_SignalNum = (UBYTE)sig;
    watching = sig >= 0 && StartNotify(&nr);
    for (;;) {
        ULONG got = Wait((watching ? 1UL << sig : 0) | SIGBREAKF_CTRL_C |
                         (1UL << me->pr_MsgPort.mp_SigBit));
        struct Message *back;
        while ((back = GetMsg(&me->pr_MsgPort)) != 0) {
            have = 1; /* the window is done with the last table */
            if (m->quit)
                quitting = 1;
        }
        if (got & SIGBREAKF_CTRL_C)
            quitting = 1;
        if (quitting && have)
            break;
        if (have && watching && (got & (1UL << sig))) {
            Delay(5); /* a writer that renames its new file in: let it finish */
            if (m->conf)
                FreeVec(m->conf); /* not taken (the window always takes it) */
            m->conf = (upconf *)AllocVec(sizeof(upconf), MEMF_ANY | MEMF_CLEAR);
            if (m->conf) { /* no memory: this change is missed, the window keeps its table */
                read_conf(m->conf);
                have = 0;
                ReplyMsg(&m->msg);
            }
        }
    }
    if (watching)
        EndNotify(&nr);
    if (sig >= 0)
        FreeSignal(sig);
    m->done = 1;
    Forbid(); /* the window frees m: end before it can run on */
    ReplyMsg(&m->msg);
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

static void watch_start(con *c)
{
    if (!c->conf)
        return;
    c->watch_port = CreateMsgPort();
    c->watch = (struct watch_msg *)AllocVec(sizeof(struct watch_msg), MEMF_PUBLIC | MEMF_CLEAR);
    if (!c->watch_port || !c->watch)
        goto fail; /* the watcher makes its table when the file changes */
    c->watch->msg.mn_ReplyPort = c->watch_port;
    c->watch->msg.mn_Length = sizeof(struct watch_msg);
    c->watch_task = CreateNewProcTags(NP_Entry, (ULONG)watch_worker, NP_Name, (ULONG)"vtcon watch",
                                      NP_StackSize, 8192, NP_Input, 0, NP_Output, 0,
                                      NP_CloseInput, FALSE, NP_CloseOutput, FALSE,
                                      NP_ConsoleTask, 0, TAG_DONE);
    if (!c->watch_task)
        goto fail;
    PutMsg(&c->watch_task->pr_MsgPort, &c->watch->msg);
    return;
fail:
    if (c->watch) {
        FreeVec(c->watch);
        c->watch = 0;
    }
    if (c->watch_port)
        DeleteMsgPort(c->watch_port);
    c->watch_port = 0;
}

static void watch_stop(con *c)
{
    struct watch_msg *m;
    if (!c->watch_task)
        return;
    if (c->watch_held) {
        c->watch->quit = 1;
        c->watch_held = 0;
        PutMsg(&c->watch_task->pr_MsgPort, &c->watch->msg);
    } else
        Signal(&c->watch_task->pr_Task, SIGBREAKF_CTRL_C);
    for (;;) {
        WaitPort(c->watch_port);
        m = (struct watch_msg *)GetMsg(c->watch_port);
        if (!m)
            continue;
        if (m->done)
            break;
        m->quit = 1; /* a table on its way when we asked: back, quitting */
        PutMsg(&c->watch_task->pr_MsgPort, &m->msg);
    }
    c->watch_task = 0;
    if (c->watch->conf)
        FreeVec(c->watch->conf); /* a table on its way when we quit */
    FreeVec(c->watch);
    c->watch = 0;
    DeleteMsgPort(c->watch_port);
    c->watch_port = 0;
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

/* A profile's or a theme's colours other than fg/bg: cursor, selection,
 * palette. theme: what the section does not set goes back to the built-in
 * look (a theme replaces the colours, as UP-Term Prefs' Theme... does). */
static void apply_colours_rest(con *c, const upconf *cf, const char *p, int theme)
{
    const char *v = upconf_str(cf, p, "cursor-color", 0);
    int i;
    if (v && !str_ieq(v, "inverse"))
        c->w.cursor_rgb = upconf_rgb(cf, p, "cursor-color", VR_KEEP);
    else if (theme)
        c->w.cursor_rgb = VR_KEEP;
    /* either key alone is enough: whichever is unset keeps the swapped value */
    c->w.sel_fg_rgb = upconf_rgb(cf, p, "selection-fg", VR_KEEP);
    c->w.sel_bg_rgb = upconf_rgb(cf, p, "selection-bg", VR_KEEP);
    v = upconf_str(cf, p, "palette", 0);
    if (theme)
        for (i = 0; i < 16; i++)
            c->w.pal16[i] = 0;
    if (v)
        upconf_palette_parse(v, c->w.pal16); /* 0x01RRGGBB, 0 = not remapped */
}

/* fg and bg, then the rest. */
static void apply_colours(con *c, const upconf *cf, const char *p, int theme)
{
    ULONG fg = upconf_rgb(cf, p, "fg", VR_KEEP);
    ULONG bg = upconf_rgb(cf, p, "bg", VR_KEEP);
    if (fg != VR_KEEP)
        c->w.fg_rgb = fg;
    if (bg != VR_KEEP)
        c->w.bg_rgb = bg;
    apply_colours_rest(c, cf, p, theme);
}

/* The profile's values for every knob the spec did not set (precedence:
 * built-in defaults < profile < window spec; plan 2026-10-01). A profile
 * the file does not know falls back to "default". */
static void apply_profile(con *c)
{
    const char *p, *v;
    c->link_open[0] = 0; /* none: OpenURL %s (h_open_link) */
    if (!c->conf)
        return;
    p = c->profile;
    if (!profile_exists(c->conf, p))
        p = profile_exists(c->conf, "default") ? "default" : 0;
    if (!p)
        return;
    /* OSC 8: the command a Ctrl + clicked link runs, %s the URL */
    copy_str(c->link_open, upconf_str(c->conf, p, "link-open", ""), sizeof(c->link_open));
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
    /* a screen of its own (P1): screen = workbench | own | fullscreen,
     * screen-mode = 0xID, screen-depth = n -- under the window's own
     * OWNSCREEN / FULLSCREEN / PUBSCREEN / SCREENMODE / DEPTH */
    if (!c->own_screen && !c->screen[0]) {
        v = upconf_str(c->conf, p, "screen", 0);
        if (v && str_ieq(v, "own"))
            c->own_screen = 1;
        else if (v && str_ieq(v, "fullscreen"))
            c->own_screen = 2;
    }
    if (c->mode_id == (ULONG)INVALID_ID && (v = upconf_str(c->conf, p, "screen-mode", 0)) != 0) {
        ULONG m = 0;
        if (v[0] == '0' && (v[1] == 'x' || v[1] == 'X'))
            v += 2;
        for (; *v; v++) {
            char h = *v;
            if (h >= '0' && h <= '9') m = m * 16 + (ULONG)(h - '0');
            else if (h >= 'a' && h <= 'f') m = m * 16 + (ULONG)(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') m = m * 16 + (ULONG)(h - 'A' + 10);
            else break;
        }
        c->mode_id = m;
    }
    if (!c->depth && upconf_get(c->conf, p, "screen-depth"))
        c->depth = (int)upconf_int(c->conf, p, "screen-depth", 0);
    /* font-aspect = off: the font as asked on every screen (P2) */
    v = upconf_str(c->conf, p, "font-aspect", 0);
    c->w.aspect_off = v && str_ieq(v, "off");
    /* glyphs the bitmap font lacks: an outline font's (F1); "" none */
    v = upconf_str(c->conf, p, "font-fallback", 0);
    copy_str(c->w.fallback, v ? v : "", sizeof(c->w.fallback));
    if (!c->colours_spec)
        apply_colours(c, c->conf, p, 0);
    else
        apply_colours_rest(c, c->conf, p, 0);
    /* no key: the built-in 500 (sb_lines 0); "scrollback = 0": none (-1),
     * as Settings > Scrollback > None saves it */
    if (upconf_get(c->conf, p, "scrollback")) {
        long n = upconf_int(c->conf, p, "scrollback", 0);
        c->w.sb_lines = n > 0 ? (int)n : -1;
    } else
        c->w.sb_lines = 0;
    /* the steady DECSCUSR shapes (2, 4, 6): blinking is cursor-blink's
     * (1, 3, 5 blink by themselves -- every shape blinked once the frame
     * clock ran, whatever Blinking said) */
    v = upconf_str(c->conf, p, "cursor", 0);
    if (v) {
        if (str_ieq(v, "block"))
            c->w.cursor_style = 2;
        else if (str_ieq(v, "underline"))
            c->w.cursor_style = 4;
        else if (str_ieq(v, "bar"))
            c->w.cursor_style = 6;
    }
    v = upconf_str(c->conf, p, "cursor-blink", 0);
    if (v)
        c->w.cursor_blink = str_ieq(v, "on");
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
    v = upconf_str(c->conf, p, "program-clipboard", 0);
    if (v) /* OSC 52: write (default) | read-write | off */
        c->w.clip_access = str_ieq(v, "off") ? 0 : str_ieq(v, "read-write") ? VT_CLIP_WRITE | VT_CLIP_READ
                         : VT_CLIP_WRITE;
    v = upconf_str(c->conf, p, "copy-on-select", 0);
    if (v)
        c->w.copy_on_select = str_ieq(v, "on");
    v = upconf_str(c->conf, p, "wheel", 0);
    if (v)
        c->w.wheel_scroll = !str_ieq(v, "ignore");
    v = upconf_str(c->conf, p, "reflow", 0);
    if (v)
        c->w.reflow = !str_ieq(v, "off");
    v = upconf_str(c->conf, p, "backspace", 0);
    if (v)
        c->w.backspace_bs = str_ieq(v, "bs"); /* del (^?, the default) or bs (^H) */
    v = upconf_str(c->conf, p, "scrollbar", 0);
    if (v)
        c->sbar_on = !str_ieq(v, "hide");
    v = upconf_str(c->conf, p, "completion", 0);
    c->kingcon = v && str_ieq(v, "kingcon");
    c->kc_style = le_kc_fncmode(upconf_str(c->conf, p, "kingcon-mode", ""));
    v = upconf_str(c->conf, p, "kingcon-info", 0);
    c->kc_info = v && str_ieq(v, "show");
    v = upconf_str(c->conf, p, "kingcon-cache", 0);
    c->kc_cache = !(v && str_ieq(v, "off"));
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
    c->own_screen = 0;
    c->screen_want = -1;
    c->pubname[0] = 0;
    c->mode_id = (ULONG)INVALID_ID;
    c->depth = 0;
    c->wflags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_SIZEGADGET | WFLG_SIZEBRIGHT |
                WFLG_ACTIVATE | WFLG_SMART_REFRESH;
    c->sbar_on = 1; /* a sizable window shows its scroll bar unless the profile hides it */
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
        } else if (str_ieq(field, "OWNSCREEN")) {
            c->own_screen = c->own_screen ? c->own_screen : 1;
        } else if (str_ieq(field, "FULLSCREEN")) {
            c->own_screen = 2;
        } else if (str_ipre(field, "PUBSCREEN", &rest)) {
            /* a screen of its own, public under this name */
            copy_str(c->pubname, rest, sizeof(c->pubname));
            c->own_screen = c->own_screen ? c->own_screen : 1;
        } else if (str_ipre(field, "SCREENMODE", &rest)) {
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
            c->mode_id = v;
        } else if (str_ipre(field, "DEPTH", &rest)) {
            c->depth = 0;
            for (; *rest >= '0' && *rest <= '9'; rest++)
                c->depth = c->depth * 10 + (*rest - '0');
        } else if (str_ipre(field, "SCREEN", &rest)) {
            copy_str(c->screen, rest, sizeof(c->screen));
        } else if ((k = alt_font_option(field, &rest)) != 0) {
            /* FONT1..FONT9 name.font size: SGR 11-19; FRAKTUR: SGR 20 */
            parse_font(rest, c->w.altname[k], sizeof(c->w.altname[k]), &c->w.altsize[k]);
        } else if (str_ipre(field, "FONT", &rest)) {
            /* FONT name.font size */
            parse_font(rest, c->w.fontname, sizeof(c->w.fontname), &c->w.fontsize);
        } else if (str_ipre(field, "TAB", &rest)) {
            /* TAB port: a tab in that host's window (UP-Term > New tab) */
            copy_str(c->tab_host, rest, sizeof(c->tab_host));
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
    c->spec_fg = c->w.fg_rgb;
    c->spec_bg = c->w.bg_rgb;
    apply_profile(c); /* the profile's values, under the spec's own options */
}

static void le_out(void *u, const unsigned char *b, long n);
static void history_load(con *c);
static void close_window(con *c);

static void find_close(con *c); /* the find prompt (see the find prompt section) */
static void sel_close(con *c);  /* KingCON's selection window (see the KingCON section) */
static void sel_idcmp(con *c);
static void kc_tab(con *c, int mode);
static void kc_finish(con *c, struct complete_req *q);
static void kc_cyc_end(con *c);
static int sb_size(const con *c);
static void watch_take(con *c);
static void tab_command(con *c, int what);
static void sbar_apply(con *c);
static int kc_cyc_key(con *c, const vt_u8 *b, int n, long key, int mods);
static void kc_menu(con *c, int mode);
static int find_open(con *c);
static void close_gadget(con *c);

/* ---- the window's menu -------------------------------------------------------- */

/* the items' ids: handler/menu_ids.h (the slash commands name them too) */

/* Right Amiga C, V and F stay what they were: Intuition now hands them in
 * as MENUPICK, which picks the same actions. */
static const struct NewMenu menu_def[] = {
    { NM_TITLE, (STRPTR)"UP-Term", 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"New tab", (STRPTR)"T", 0, 0, (APTR)MENU_TAB_NEW },
    { NM_ITEM, (STRPTR)"Next tab", (STRPTR)".", 0, 0, (APTR)MENU_TAB_NEXT },
    { NM_ITEM, (STRPTR)"Previous tab", (STRPTR)",", 0, 0, (APTR)MENU_TAB_PREV },
    { NM_ITEM, (STRPTR)"Close tab", 0, 0, 0, (APTR)MENU_TAB_CLOSE },
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Preferences...", 0, 0, 0, (APTR)MENU_PREFS },
    { NM_ITEM, (STRPTR)"About UP-Term...", 0, 0, 0, (APTR)MENU_ABOUT },
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Close window", 0, 0, 0, (APTR)MENU_CLOSE },
    { NM_TITLE, (STRPTR)"Edit", 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Copy", (STRPTR)"C", 0, 0, (APTR)MENU_COPY },
    { NM_ITEM, (STRPTR)"Paste", (STRPTR)"V", 0, 0, (APTR)MENU_PASTE },
    { NM_ITEM, (STRPTR)"Select all", (STRPTR)"A", 0, 0, (APTR)MENU_SELECT_ALL },
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Find...", (STRPTR)"F", 0, 0, (APTR)MENU_FIND },
    { NM_ITEM, (STRPTR)"Find next", (STRPTR)"G", 0, 0, (APTR)MENU_FIND_NEXT },
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Clear screen", (STRPTR)"K", 0, 0, (APTR)MENU_CLEAR_SCREEN },
    { NM_ITEM, (STRPTR)"Clear scrollback", 0, 0, 0, (APTR)MENU_CLEAR_SB },
    { NM_ITEM, (STRPTR)"Reset terminal", 0, 0, 0, (APTR)MENU_RESET },
    { NM_TITLE, (STRPTR)"View", 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Bigger font", (STRPTR)"+", 0, 0, (APTR)MENU_FONT_BIGGER },
    { NM_ITEM, (STRPTR)"Smaller font", (STRPTR)"-", 0, 0, (APTR)MENU_FONT_SMALLER },
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"80 x 24", 0, 0, 0, (APTR)MENU_SIZE_80X24 },
    { NM_ITEM, (STRPTR)"132 x 43", 0, 0, 0, (APTR)MENU_SIZE_132X43 },
    { NM_END, 0, 0, 0, 0, 0 }
};

/* With completion = kingcon, KingCON's Complete menu follows (its items
 * are the keys', and the window's switches; a switch here is this window's
 * only -- Prefs keeps the profile). Checkmarks are set in menu_add. */
#define MENU_KC_ITEMS 10
static const struct NewMenu menu_kc[MENU_KC_ITEMS] = {
    { NM_TITLE, (STRPTR)"Complete", 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Filename", 0, 0, 0, (APTR)MENU_KC_FILE },
    { NM_ITEM, (STRPTR)"Command", 0, 0, 0, (APTR)MENU_KC_COMMAND },
    { NM_ITEM, (STRPTR)"Device", 0, 0, 0, (APTR)MENU_KC_DEVICE },
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Enable cache", 0, CHECKIT | MENUTOGGLE, 0, (APTR)MENU_KC_CACHE },
    { NM_ITEM, (STRPTR)"Reset cache", 0, 0, 0, (APTR)MENU_KC_RESET },
    { NM_ITEM, (STRPTR)"Purge cache", 0, 0, 0, (APTR)MENU_KC_PURGE },
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Show .info", 0, CHECKIT | MENUTOGGLE, 0, (APTR)MENU_KC_INFO }
};

/* Help, the strip's last menu: the tour of what the terminal does
 * (UPDemo TOUR, in a new tab; /demo too) */
#define MENU_HELP_ITEMS 2
static const struct NewMenu menu_help[MENU_HELP_ITEMS] = {
    { NM_TITLE, (STRPTR)"Help", 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Demo tour", 0, 0, 0, (APTR)MENU_DEMO }
};

/* Settings: what UP-Term Prefs sets for a profile, for this window, live
 * (plan H9). The checkmarks show the window's settings (menu_checked); a
 * pick changes the window only -- Prefs keeps the profile. MutualExclude
 * bits are the item's place in its submenu. KingCON's .info and cache
 * switches are in its Complete menu. */
#define MENU_SET_ITEMS 49
static const struct NewMenu menu_set[MENU_SET_ITEMS] = {
    { NM_TITLE, (STRPTR)"Settings", 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Font...", 0, 0, 0, (APTR)MENU_SET_FONT },
    { NM_ITEM, (STRPTR)"Theme...", 0, 0, 0, (APTR)MENU_SET_THEME },
    { NM_ITEM, (STRPTR)"Screen", 0, 0, 0, 0 },
    { NM_SUB, (STRPTR)"Workbench", 0, CHECKIT, 6, (APTR)MENU_SCREEN_WB },
    { NM_SUB, (STRPTR)"Own screen", 0, CHECKIT, 5, (APTR)MENU_SCREEN_OWN },
    { NM_SUB, (STRPTR)"Full screen", 0, CHECKIT, 3, (APTR)MENU_SCREEN_FULL },
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Cursor", 0, 0, 0, 0 },
    { NM_SUB, (STRPTR)"Block", 0, CHECKIT, 6, (APTR)MENU_SET_BLOCK },
    { NM_SUB, (STRPTR)"Underline", 0, CHECKIT, 5, (APTR)MENU_SET_UNDERLINE },
    { NM_SUB, (STRPTR)"Bar", 0, CHECKIT, 3, (APTR)MENU_SET_BAR },
    { NM_SUB, NM_BARLABEL, 0, 0, 0, 0 },
    { NM_SUB, (STRPTR)"Blinking", 0, CHECKIT | MENUTOGGLE, 0, (APTR)MENU_SET_BLINK },
    { NM_ITEM, (STRPTR)"Bell", 0, 0, 0, 0 },
    { NM_SUB, (STRPTR)"None", 0, CHECKIT, 6, (APTR)MENU_SET_BELL_NONE },
    { NM_SUB, (STRPTR)"Beep", 0, CHECKIT, 5, (APTR)MENU_SET_BELL_BEEP },
    { NM_SUB, (STRPTR)"Visual", 0, CHECKIT, 3, (APTR)MENU_SET_BELL_VISUAL },
    { NM_ITEM, (STRPTR)"Scrollback", 0, 0, 0, 0 },
    { NM_SUB, (STRPTR)"None", 0, CHECKIT, 30, (APTR)MENU_SET_SB_NONE },
    { NM_SUB, (STRPTR)"500 lines", 0, CHECKIT, 29, (APTR)MENU_SET_SB_500 },
    { NM_SUB, (STRPTR)"1000 lines", 0, CHECKIT, 27, (APTR)MENU_SET_SB_1000 },
    { NM_SUB, (STRPTR)"2000 lines", 0, CHECKIT, 23, (APTR)MENU_SET_SB_2000 },
    { NM_SUB, (STRPTR)"5000 lines", 0, CHECKIT, 15, (APTR)MENU_SET_SB_5000 },
    { NM_ITEM, (STRPTR)"Bold is bright", 0, CHECKIT | MENUTOGGLE, 0, (APTR)MENU_SET_BOLD },
    { NM_ITEM, (STRPTR)"Meta key", 0, 0, 0, 0 },
    { NM_SUB, (STRPTR)"Left Amiga", 0, CHECKIT, 2, (APTR)MENU_SET_META_AMIGA },
    { NM_SUB, (STRPTR)"Alt", 0, CHECKIT, 1, (APTR)MENU_SET_META_ALT },
    { NM_ITEM, (STRPTR)"Copy on select", 0, CHECKIT | MENUTOGGLE, 0, (APTR)MENU_SET_COPY },
    { NM_ITEM, (STRPTR)"Wheel scrolls", 0, CHECKIT | MENUTOGGLE, 0, (APTR)MENU_SET_WHEEL },
    { NM_ITEM, (STRPTR)"Reflow on resize", 0, CHECKIT | MENUTOGGLE, 0, (APTR)MENU_SET_REFLOW },
    { NM_ITEM, (STRPTR)"Scroll bar", 0, CHECKIT | MENUTOGGLE, 0, (APTR)MENU_SET_SCROLLBAR },
    { NM_ITEM, (STRPTR)"Backspace key sends", 0, 0, 0, 0 },
    { NM_SUB, (STRPTR)"Delete", 0, CHECKIT, 2, (APTR)MENU_SET_BS_DEL },
    { NM_SUB, (STRPTR)"Backspace", 0, CHECKIT, 1, (APTR)MENU_SET_BS_BS },
    { NM_ITEM, (STRPTR)"Programs may", 0, 0, 0, 0 },
    { NM_SUB, (STRPTR)"Set the clipboard", 0, CHECKIT, 6, (APTR)MENU_SET_CLIP_WRITE },
    { NM_SUB, (STRPTR)"Set and read the clipboard", 0, CHECKIT, 5, (APTR)MENU_SET_CLIP_READ_WRITE },
    { NM_SUB, (STRPTR)"Not use the clipboard", 0, CHECKIT, 3, (APTR)MENU_SET_CLIP_OFF },
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Tab completion", 0, 0, 0, 0 },
    { NM_SUB, (STRPTR)"Unix", 0, CHECKIT, 2, (APTR)MENU_SET_UNIX },
    { NM_SUB, (STRPTR)"KingCON", 0, CHECKIT, 1, (APTR)MENU_SET_KINGCON },
    { NM_ITEM, (STRPTR)"KingCON style", 0, 0, 0, 0 },
    { NM_SUB, (STRPTR)"Window", 0, CHECKIT | MENUTOGGLE, 0, (APTR)MENU_SET_KC_W },
    { NM_SUB, (STRPTR)"List", 0, CHECKIT | MENUTOGGLE, 0, (APTR)MENU_SET_KC_L },
    { NM_SUB, (STRPTR)"Cycle", 0, CHECKIT | MENUTOGGLE, 0, (APTR)MENU_SET_KC_B },
    { NM_SUB, (STRPTR)"Common part first", 0, CHECKIT | MENUTOGGLE, 0, (APTR)MENU_SET_KC_C },
    { NM_SUB, (STRPTR)"Silent", 0, CHECKIT | MENUTOGGLE, 0, (APTR)MENU_SET_KC_S }
};

/* the window's scrollback in lines (the spec's -1 none, 0 the built-in 500) */
static int sb_size(const con *c)
{
    return c->w.sb_lines < 0 ? 0 : c->w.sb_lines ? c->w.sb_lines : 500;
}

/* Is the setting behind a checkmark item on in this window? */
static int menu_checked(const con *c, LONG id)
{
    int cs = c->w.cursor_style;
    switch (id) {
    case MENU_SCREEN_WB: return c->own_screen == 0;
    case MENU_SCREEN_OWN: return c->own_screen == 1;
    case MENU_SCREEN_FULL: return c->own_screen == 2;
    case MENU_SET_BLOCK: return cs <= 2;
    case MENU_SET_UNDERLINE: return cs == 3 || cs == 4;
    case MENU_SET_BAR: return cs >= 5;
    case MENU_SET_BLINK: return c->w.cursor_blink;
    case MENU_SET_BELL_NONE: return c->w.bell == 0;
    case MENU_SET_BELL_BEEP: return c->w.bell == 1;
    case MENU_SET_BELL_VISUAL: return c->w.bell == 2;
    case MENU_SET_BOLD: return c->w.bold_bright;
    case MENU_SET_META_AMIGA: return !c->w.meta_alt;
    case MENU_SET_META_ALT: return c->w.meta_alt;
    case MENU_SET_COPY: return c->w.copy_on_select;
    case MENU_SET_WHEEL: return c->w.wheel_scroll;
    case MENU_SET_REFLOW: return c->w.reflow;
    case MENU_SET_SCROLLBAR: return c->sbar_on;
    case MENU_SET_BS_DEL: return !c->w.backspace_bs;
    case MENU_SET_BS_BS: return c->w.backspace_bs;
    case MENU_SET_CLIP_WRITE: return c->w.clip_access == VT_CLIP_WRITE;
    case MENU_SET_CLIP_READ_WRITE: return c->w.clip_access == (VT_CLIP_WRITE | VT_CLIP_READ);
    case MENU_SET_CLIP_OFF: return c->w.clip_access == 0;
    case MENU_SET_UNIX: return !c->kingcon;
    case MENU_SET_KINGCON: return c->kingcon;
    case MENU_SET_KC_W: return (c->kc_style & LE_KC_WINDOW) != 0;
    case MENU_SET_KC_L: return (c->kc_style & LE_KC_LIST) != 0;
    case MENU_SET_KC_B: return (c->kc_style & LE_KC_CYCLE) != 0;
    case MENU_SET_KC_C: return (c->kc_style & LE_KC_COMMON) != 0;
    case MENU_SET_KC_S: return (c->kc_style & LE_KC_SILENT) != 0;
    case MENU_KC_CACHE: return c->kc_cache;
    case MENU_KC_INFO: return c->kc_info;
    case MENU_SET_SB_NONE: return sb_size(c) == 0;
    case MENU_SET_SB_500: return sb_size(c) == 500;
    case MENU_SET_SB_1000: return sb_size(c) == 1000;
    case MENU_SET_SB_2000: return sb_size(c) == 2000;
    case MENU_SET_SB_5000: return sb_size(c) == 5000;
    }
    return 0;
}

static void menu_add(con *c, struct Window *win)
{
    APTR vi;
    if (!GadToolsBase || win == c->foreign)
        return; /* a window someone else opened keeps its own menus */
    {
        /* UP-Term, Settings, the Complete menu under KingCON completion, Help */
        struct NewMenu nm[sizeof(menu_def) / sizeof(menu_def[0]) + MENU_SET_ITEMS + 2 + UC_MAX_PROFILES +
                          MENU_KC_ITEMS + MENU_HELP_ITEMS];
        int n = sizeof(menu_def) / sizeof(menu_def[0]) - 1, i; /* without the NM_END */
        CopyMem((APTR)menu_def, nm, n * sizeof(struct NewMenu));
        CopyMem((APTR)menu_set, nm + n, sizeof(menu_set));
        for (i = n; i < n + MENU_SET_ITEMS; i++)
            if (!c->kingcon && (LONG)nm[i].nm_UserData == 0 && nm[i].nm_Label &&
                nm[i].nm_Label != NM_BARLABEL && !strcmp((const char *)nm[i].nm_Label, "KingCON style"))
                nm[i].nm_Flags |= NM_ITEMDISABLED; /* Unix completion has no styles */
        n += MENU_SET_ITEMS;
        {
            /* Profile: the file's profiles, the window's checked; picking
             * one switches the window to it (profile_switch) */
            const char *names[UC_MAX_PROFILES + 1];
            int np = c->conf ? upconf_profiles(c->conf, names) : 0, k;
            memset(&nm[n], 0, sizeof(struct NewMenu) * 2);
            nm[n].nm_Type = NM_ITEM;
            nm[n].nm_Label = (STRPTR)"Profile";
            n++;
            if (!np) {
                nm[n].nm_Type = NM_SUB;
                nm[n].nm_Label = (STRPTR)"default";
                nm[n].nm_Flags = CHECKIT | CHECKED | NM_ITEMDISABLED;
                n++;
            }
            for (k = 0; k < np && k < UC_MAX_PROFILES; k++, n++) {
                memset(&nm[n], 0, sizeof(struct NewMenu));
                nm[n].nm_Type = NM_SUB;
                nm[n].nm_Label = (STRPTR)names[k];
                nm[n].nm_Flags = CHECKIT | (str_ieq(names[k], c->profile) ? CHECKED : 0);
                nm[n].nm_MutualExclude = ~(1L << k) & ((1L << np) - 1);
                nm[n].nm_UserData = (APTR)(LONG)(MENU_SET_PROFILE0 + k);
            }
            memset(&nm[n], 0, sizeof(struct NewMenu));
            nm[n].nm_Type = NM_ITEM;
            nm[n].nm_Label = (STRPTR)"Save settings to profile";
            nm[n].nm_UserData = (APTR)(LONG)MENU_SET_SAVE;
            if (!c->conf)
                nm[n].nm_Flags = NM_ITEMDISABLED;
            n++;
        }
        if (c->kingcon) {
            CopyMem((APTR)menu_kc, nm + n, sizeof(menu_kc));
            n += MENU_KC_ITEMS;
        }
        CopyMem((APTR)menu_help, nm + n, sizeof(menu_help));
        n += MENU_HELP_ITEMS;
        for (i = 0; i < n; i++)
            if ((nm[i].nm_Flags & CHECKIT) && menu_checked(c, (LONG)nm[i].nm_UserData))
                nm[i].nm_Flags |= CHECKED;
        CopyMem((APTR)&menu_def[sizeof(menu_def) / sizeof(menu_def[0]) - 1], nm + n,
                sizeof(struct NewMenu));
        c->menustrip = CreateMenusA(nm, 0);
    }
    vi = c->menustrip ? GetVisualInfoA(win->WScreen, 0) : 0;
    if (!vi || !LayoutMenus(c->menustrip, vi, GTMN_NewLookMenus, TRUE, TAG_DONE) ||
        ((!c->is_tab || c->shown) && !SetMenuStrip(win, c->menustrip))) {
        FreeMenus(c->menustrip);
        c->menustrip = 0;
    }
    if (vi)
        FreeVisualInfo(vi);
}

static void menu_remove(con *c, struct Window *win)
{
    if (!c->menustrip)
        return;
    if (win && (!c->is_tab || c->shown))
        ClearMenuStrip(win); /* a hidden tab's strip is not on the window */
    FreeMenus(c->menustrip);
    c->menustrip = 0;
}

/* The Prefs editor, started by a process of its own: the handler makes no
 * DOS call on its own port (a reply waiting there can swallow a packet),
 * and SystemTags is all DOS. The worker starts the editor asynchronously
 * and ends. */
static void launcher(void)
{
    static struct EasyStruct missing = {
        sizeof(struct EasyStruct), 0, (UBYTE *)"UP-Term",
        (UBYTE *)"The preferences editor is not installed:\nC:UP-Term Prefs was not found.\n\n"
                 "Install UP-Term from its archive to get it.",
        (UBYTE *)"OK"
    };
    BPTR in, out, lock;
    /* the editor's errors would go to NIL: below: say here that it is not
     * there, instead of a menu item that does nothing */
    if (!(lock = Lock((STRPTR)"C:UP-Term Prefs", SHARED_LOCK))) {
        EasyRequestArgs(0, &missing, 0, 0);
        return;
    }
    UnLock(lock);
    in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    if (in && out &&
        SystemTags((STRPTR)"\"C:UP-Term Prefs\"", SYS_Input, in, SYS_Output, out, SYS_Asynch, TRUE,
                   TAG_DONE) != -1)
        return; /* the streams are the editor's now */
    if (in)
        Close(in);
    if (out)
        Close(out);
}

static void prefs_launch(void)
{
    CreateNewProcTags(NP_Entry, (ULONG)launcher, NP_Name, (ULONG)"UP-Term Prefs launcher",
                      NP_StackSize, 8192, NP_Input, 0, NP_Output, 0, NP_CloseInput, FALSE,
                      NP_CloseOutput, FALSE, NP_ConsoleTask, 0, TAG_DONE);
}

#define WORK_COMP 1  /* the completion request (Tab, ASL, font, theme, save) */
#define WORK_CHECK 2 /* is the first word a command */
#define WORK_HIST 4  /* the history file */
static int ensure_worker(con *c, int want);
static struct Process *opener(con *c);

/* Settings > Font...: the ASL font requester, in the completion worker (it
 * reads FONTS:, DOS calls the handler must not make); the answer arrives
 * in finish_completion. */
static void font_ask(con *c)
{
    if (c->comp_busy || !ensure_worker(c, WORK_COMP) || !c->w.win)
        return;
    copy_str(c->comp->word, c->w.fontname[0] ? c->w.fontname : "", COMPLETE_MAX);
    c->comp->font_size = c->w.font ? c->w.font->tf_YSize : 8;
    c->comp->mode = COMPLETE_FONT;
    c->comp->kingcon = 0;
    c->comp->screen = c->w.win->WScreen;
    if (complete_start(c->comp, c->comp_port, opener(c)))
        c->comp_busy = 1;
}

static void rgb_hex(ULONG rgb, char *out)
{
    static const char d[] = "0123456789ABCDEF";
    int i;
    if (rgb == VR_KEEP) {
        out[0] = 0;
        return;
    }
    for (i = 5; i >= 0; i--, rgb >>= 4)
        out[i] = d[rgb & 15];
    out[6] = 0;
}

/* The window's settings now, as UP-Term Prefs' fields for its profile:
 * the profile's own values first (what no menu sets, the scrollback),
 * then everything the window has changed. */
static void window_fields(con *c, prefs_fields *f)
{
    int cs = c->w.cursor_style, i, k = 0;
    prefs_from_conf(f, c->conf, c->profile);
    f->cursor = cs == 3 || cs == 4 ? PREFS_CURSOR_UNDERLINE : cs >= 5 ? PREFS_CURSOR_BAR
              : PREFS_CURSOR_BLOCK;
    f->blink = c->w.cursor_blink != 0;
    f->bell = c->w.bell == 0 ? PREFS_BELL_NONE : c->w.bell == 2 ? PREFS_BELL_VISUAL : PREFS_BELL_BEEP;
    f->bold = c->w.bold_bright != 0;
    f->meta_alt = c->w.meta_alt != 0;
    f->copy_sel = c->w.copy_on_select != 0;
    f->wheel = c->w.wheel_scroll != 0;
    f->reflow = c->w.reflow != 0;
    f->scrollbar = c->sbar_on != 0;
    f->completion = c->kingcon ? PREFS_COMPLETE_KINGCON : PREFS_COMPLETE_UNIX;
    f->backspace_bs = c->w.backspace_bs != 0;
    f->clipboard = c->w.clip_access == 0 ? PREFS_CLIP_OFF
                 : (c->w.clip_access & VT_CLIP_READ) ? PREFS_CLIP_READ_WRITE : PREFS_CLIP_WRITE;
    copy_str(f->linkopen, c->link_open, sizeof(f->linkopen));
    k = 0;
    if (c->kc_style & LE_KC_WINDOW) f->kcmode[k++] = 'W';
    if (c->kc_style & LE_KC_LIST) f->kcmode[k++] = 'L';
    if (c->kc_style & LE_KC_CYCLE) f->kcmode[k++] = 'B';
    if (c->kc_style & LE_KC_COMMON) f->kcmode[k++] = 'C';
    if (c->kc_style & LE_KC_SILENT) f->kcmode[k++] = 'S';
    f->kcmode[k] = 0;
    f->kcinfo = c->kc_info != 0;
    f->kccache = c->kc_cache != 0;
    {
        /* the scrollback, as the profile's "scrollback = n" (0: none) */
        char n[12];
        int m = 0, v = sb_size(c);
        k = 0;
        do
            n[m++] = (char)('0' + v % 10);
        while ((v /= 10) != 0);
        while (m)
            f->sb[k++] = n[--m];
        f->sb[k] = 0;
    }
    if (c->w.fontname[0]) {
        /* "NAME SIZE", as apply_profile reads it */
        char n[12];
        int m = 0, sz = c->w.fontsize;
        copy_str(f->font, c->w.fontname, sizeof(f->font) - 8);
        do
            n[m++] = (char)('0' + sz % 10);
        while ((sz /= 10) != 0);
        k = (int)strlen(f->font);
        f->font[k++] = ' ';
        while (m)
            f->font[k++] = n[--m];
        f->font[k] = 0;
    }
    rgb_hex(c->w.fg_rgb, f->fg);
    rgb_hex(c->w.bg_rgb, f->bg);
    rgb_hex(c->w.cursor_rgb, f->curcol);
    rgb_hex(c->w.sel_fg_rgb, f->selfg);
    rgb_hex(c->w.sel_bg_rgb, f->selbg);
    for (i = 0; i < 16; i++)
        rgb_hex((c->w.pal16[i] & 0x01000000UL) ? (c->w.pal16[i] & 0xFFFFFFUL) : VR_KEEP, f->pal[i]);
}

/* The file buffer of a save or a theme and the staged table: they live
 * while the worker has them, not for the rest of the window (16 KB and
 * 18 KB; research/2026-10-04_window-memory.md). */
static void comp_data_done(con *c)
{
    if (c->comp->data) {
        FreeVec(c->comp->data);
        c->comp->data = 0;
    }
    if (c->save_work) {
        FreeVec(c->save_work);
        c->save_work = 0;
    }
}

/* Settings > Save settings to profile: the file staged here (prefs_core,
 * as UP-Term Prefs does it), written by the worker (DOS) into ENV: and
 * ENVARC:; the window's table follows once both are in place. */
static void save_ask(con *c)
{
    prefs_fields f;
    long len;
    if (c->comp_busy || !ensure_worker(c, WORK_COMP) || !c->conf)
        return;
    if (!c->comp->data && !(c->comp->data = (char *)AllocVec(UC_MAX_FILE + 1, MEMF_ANY)))
        return;
    if (!c->save_work && !(c->save_work = (upconf *)AllocVec(sizeof(upconf), MEMF_ANY))) {
        comp_data_done(c);
        return;
    }
    window_fields(c, &f);
    len = prefs_validate(&f) ? -1
        : prefs_stage(c->save_work, c->conf, c->profile, &f, c->comp->data, UC_MAX_FILE + 1);
    if (len < 0) {
        comp_data_done(c);
        DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* no room for it, or the file was not read whole */
        return;
    }
    c->comp->data_len = len;
    c->comp->mode = CONFIG_SAVE;
    c->comp->kingcon = 0;
    if (complete_start(c->comp, c->comp_port, opener(c)))
        c->comp_busy = 1;
    else
        comp_data_done(c);
}

/* A theme request for the worker (DOS): COMPLETE_THEME -- name a theme's
 * name or path, "" the requester; the file read, its colours put on the
 * window in finish_completion -- or COMPLETE_THEMES, /theme's list. Both
 * look in the drawer prefs_dos_theme_drawer picks from the window's theme
 * and the handler's own file (W30). 1 when it went out. */
static int theme_req(con *c, int mode, const char *name)
{
    struct complete_req *q;
    if (c->comp_busy || !ensure_worker(c, WORK_COMP) || !c->w.win)
        return 0;
    q = c->comp;
    if (mode == COMPLETE_THEME && !q->data && !(q->data = (char *)AllocVec(UC_MAX_FILE + 1, MEMF_ANY)))
        return 0;
    copy_str(q->word, name, COMPLETE_MAX); /* a name: that theme, no requester */
    /* extra: the window's theme file, then the file DOS loaded us from
     * (L:vtcon-handler, the rig's VTC:vtcon-handler): the worker takes
     * its drawer (complete.h) */
    copy_str(q->extra, c->theme_file ? c->theme_file : "", COMPLETE_MAX);
    q->extra_len = (long)strlen(q->extra) + 1;
    q->extra[q->extra_len] = 0;
    if (c->node && c->node->dn_Handler) {
        const UBYTE *b = (const UBYTE *)BADDR(c->node->dn_Handler);
        if (b[0] < COMPLETE_MAX) {
            CopyMem((APTR)(b + 1), q->extra + q->extra_len, b[0]);
            q->extra[q->extra_len + b[0]] = 0;
        }
    }
    q->extra_len += (long)strlen(q->extra + q->extra_len) + 1;
    q->data_max = UC_MAX_FILE + 1;
    q->mode = mode;
    q->kingcon = 0;
    q->screen = c->w.win->WScreen;
    if (complete_start(q, c->comp_port, opener(c))) {
        c->comp_busy = 1;
        return 1;
    }
    comp_data_done(c);
    return 0;
}

/* Settings > Theme..., /theme NAME */
static void theme_ask(con *c, const char *name)
{
    theme_req(c, COMPLETE_THEME, name);
}

/* The window has the theme read from path: the next requester and list
 * start in its drawer, the list marks it. */
static void theme_keep(con *c, const char *path)
{
    if (!c->theme_file && !(c->theme_file = (char *)AllocVec(COMPLETE_MAX, MEMF_ANY)))
        return;
    copy_str(c->theme_file, path, COMPLETE_MAX);
}

/* The theme's colours on the window, live: one profile section, read as
 * a profile's are (apply_colours); what it does not set goes back to the
 * built-in look. 1 when the text was a theme. */
static int theme_apply(con *c, const char *text, long len)
{
    upconf *t = (upconf *)AllocVec(sizeof(upconf), MEMF_ANY | MEMF_CLEAR);
    const char *names[UC_MAX_PROFILES + 1];
    int ok = 0;
    if (t) {
        upconf_parse(t, text, len);
        if (upconf_profiles(t, names) > 0 && upconf_has(t, names[0], "fg")) {
            apply_colours(c, t, names[0], 1);
            vtwin_apply_settings(&c->w);
            ok = 1;
        }
        FreeVec(t);
    }
    if (!ok)
        DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* not a theme file */
    return ok;
}

/* ---- /theme's list (W30) ------------------------------------------------------ */

static void output(con *c, const vt_u8 *b, long n);

/* The reader's empty line, held while the list was up: a shell shows its
 * prompt where the list was. */
static void theme_line_release(con *c)
{
    if (c->line_held) {
        c->line_held = 0;
        in_append(c, (const vt_u8 *)"\n", 1);
    }
}

/* Entry i's file read for its colours -- unless a read is out (the answer
 * asks again for the entry chosen by then), or i was the last read. */
static void theme_menu_read(con *c, int i)
{
    struct theme_menu *tm = c->tm;
    if (tm->asked >= 0 || i == tm->last || i == tm->shown)
        return;
    if (theme_req(c, COMPLETE_THEME, le_menu_name(&tm->m, i)))
        tm->asked = i;
}

/* The window's own colours back, if an entry's are on it. */
static void theme_menu_restore(con *c)
{
    struct theme_menu *tm = c->tm;
    int i;
    if (tm->shown < 0 || !c->w.t)
        return;
    c->w.fg_rgb = tm->fg;
    c->w.bg_rgb = tm->bg;
    c->w.cursor_rgb = tm->cur;
    c->w.sel_fg_rgb = tm->sfg;
    c->w.sel_bg_rgb = tm->sbg;
    for (i = 0; i < 16; i++)
        c->w.pal16[i] = tm->pal[i];
    vtwin_apply_settings(&c->w);
    tm->shown = -1;
}

/* The list is closed. Taken: the chosen theme is read now if it is not on
 * the window yet (a preview still waiting for the rest is not waited for),
 * and stays -- the window's theme from now on. Cancelled: the window's own
 * colours come back at once, a read still out is dropped when it comes. */
static void theme_menu_settle(con *c)
{
    struct theme_menu *tm = c->tm;
    int sel = tm->m.sel;
    if (tm->end == LE_MENU_TAKE)
        theme_menu_read(c, sel); /* not on the window yet */
    else
        theme_menu_restore(c);
    if (tm->asked >= 0)
        return; /* finish_completion comes back here */
    if (tm->end == LE_MENU_TAKE && tm->shown == sel) {
        theme_keep(c, tm->shown_path);
    } else {
        if (tm->end == LE_MENU_TAKE)
            DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* that file is no theme */
        theme_menu_restore(c);
    }
    FreeVec(tm);
    c->tm = 0;
}

/* The worker's listing of the themes drawer: the list under the line, on
 * the window's theme (marked) or the first. */
static void theme_menu_open(con *c, struct complete_req *q)
{
    struct theme_menu *tm;
    char *names;
    int i;
    if (!c->w.t || c->raw || c->edits != c->comp_edits) {
        theme_line_release(c); /* gone, or typed on meanwhile: no list */
        return;
    }
    if (!q->matches) {
        static const char none[] = "theme: no themes in ", nodir[] =
            "theme: no themes drawer (the kit installs them in " PREFS_THEMES_DIR ")";
        if (q->add[0]) {
            output(c, (const vt_u8 *)none, (long)sizeof(none) - 1);
            output(c, (const vt_u8 *)q->add, (long)strlen(q->add));
        } else
            output(c, (const vt_u8 *)nodir, (long)sizeof(nodir) - 1);
        output(c, (const vt_u8 *)"\r\n", 2);
        theme_line_release(c);
        return;
    }
    tm = (struct theme_menu *)AllocVec(sizeof(*tm) + q->names_len + 1, MEMF_ANY | MEMF_CLEAR);
    if (!tm) {
        DisplayBeep(c->w.win ? c->w.win->WScreen : 0);
        theme_line_release(c);
        return;
    }
    names = (char *)(tm + 1);
    CopyMem(q->names, names, q->names_len);
    tm->fg = c->w.fg_rgb;
    tm->bg = c->w.bg_rgb;
    tm->cur = c->w.cursor_rgb;
    tm->sfg = c->w.sel_fg_rgb;
    tm->sbg = c->w.sel_bg_rgb;
    for (i = 0; i < 16; i++)
        tm->pal[i] = c->w.pal16[i];
    tm->asked = tm->last = tm->shown = -1;
    c->tm = tm;
    i = prefs_theme_index(names, q->matches, c->theme_file);
    le_menu_open(&c->le, &tm->m, names, q->matches, i, i);
}

/* A key while the list is up: move (the rows that changed drawn, the
 * entry's colours on the window once the bar rests), take, or cancel. */
static void theme_menu_key(con *c, const vt_u8 *b, int n, long key, int mods)
{
    struct theme_menu *tm = c->tm;
    int r = le_menu_key(&tm->m, key ? key : (n ? (long)b[0] : 0), mods, b, n);
    if (r == LE_MENU_MOVED) {
        le_menu_draw(&c->le, &tm->m);
        if (c->w.frame_open)
            vtwin_clock(&c->w); /* the rest is counted on the frame clock (theme_menu_wait) */
        else
            theme_menu_read(c, tm->m.sel); /* no clock to wait on: at once */
    } else if (r == LE_MENU_TAKE || r == LE_MENU_CANCEL) {
        tm->end = r;
        le_menu_close(&c->le, &tm->m);
        theme_line_release(c);
        theme_menu_settle(c);
    }
}

/* The frame clock waited us: once the bar has rested, the chosen entry's
 * colours go on the window -- one full repaint where the bar stops, not
 * one for every theme it passed. */
static void theme_menu_wait(con *c, ULONG us)
{
    struct theme_menu *tm = c->tm;
    if (!tm || !tm->m.open)
        return;
    if (le_menu_rested(&tm->m, (long)us))
        theme_menu_read(c, tm->m.sel);
    else if (tm->m.rest_us > 0)
        vtwin_clock(&c->w); /* still counting: the clock keeps running */
}

/* Settings > Profile: the window takes the file's k-th profile, live --
 * the built-in look first, then the profile, as a window opening with it
 * gets (what the window's own spec set, its colours, still wins); its font
 * when it names one. */
static void profile_switch(con *c, int k)
{
    const char *names[UC_MAX_PROFILES + 1];
    char oldname[40];
    WORD oldsize = c->w.fontsize;
    int sb_was;
    if (!c->conf || k >= upconf_profiles(c->conf, names) || !c->w.t)
        return;
    copy_str(c->profile, names[k], sizeof(c->profile));
    copy_str(oldname, c->w.fontname, sizeof(oldname));
    sel_close(c);
    kc_cyc_end(c);
    sb_was = sb_size(c);
    vtwin_profile_defaults(&c->w);
    c->w.fg_rgb = c->spec_fg;
    c->w.bg_rgb = c->spec_bg;
    c->w.fontname[0] = 0; /* apply_profile sets it only when empty */
    c->sbar_on = 1;
    apply_profile(c);
    if (!c->is_tab)
        sbar_apply(c); /* the profile's scroll bar, live (a tab's window is its host's) */
    if (c->w.fontname[0] && (strcmp(c->w.fontname, oldname) || c->w.fontsize != oldsize)) {
        char want[40];
        WORD wsize = c->w.fontsize;
        copy_str(want, c->w.fontname, sizeof(want));
        copy_str(c->w.fontname, oldname, sizeof(c->w.fontname));
        c->w.fontsize = oldsize;
        if (!vtwin_set_font(&c->w, want, wsize))
            DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* the profile's font is not there */
    } else {
        copy_str(c->w.fontname, oldname, sizeof(c->w.fontname));
        c->w.fontsize = oldsize;
    }
    if (sb_size(c) != sb_was)
        vtwin_set_scrollback(&c->w, sb_size(c)); /* the profile's scrollback, live too */
    vtwin_apply_settings(&c->w);
}

/* The watcher's new table: the window takes it, and when its own
 * profile's section changed puts it on live (profile_switch); the menus
 * follow either way (a profile may have come or gone). */
static void watch_take(con *c)
{
    struct watch_msg *m;
    while (c->watch_port && (m = (struct watch_msg *)GetMsg(c->watch_port)) != 0) {
        int changed = !upconf_profile_equal(c->conf, m->conf, c->profile);
        FreeVec(c->conf); /* the new table is the window's now: no copy, no second table */
        c->conf = m->conf;
        m->conf = 0;
        if (c->w.t && c->w.win) {
            if (changed) {
                const char *names[UC_MAX_PROFILES + 1];
                int n = upconf_profiles(c->conf, names), k;
                for (k = 0; k < n && !str_ieq(names[k], c->profile); k++)
                    ;
                if (k < n)
                    profile_switch(c, k);
            }
            menu_remove(c, c->w.win);
            menu_add(c, c->w.win);
        }
        c->watch_held = 0;
        PutMsg(&c->watch_task->pr_MsgPort, &m->msg); /* back: the next change */
    }
}

/* A Settings (or Complete) checkmark picked: the window's setting, live.
 * on is the item's checkmark after the pick. 1 when it was one. */
static int menu_setting(con *c, LONG id, int on)
{
    int restyle = 0, bit = 0;
    switch (id) {
    case MENU_SET_BLOCK: c->w.cursor_style = 2; restyle = 1; break; /* steady: Blinking blinks it */
    case MENU_SET_UNDERLINE: c->w.cursor_style = 4; restyle = 1; break;
    case MENU_SET_BAR: c->w.cursor_style = 6; restyle = 1; break;
    case MENU_SET_BLINK: c->w.cursor_blink = on; restyle = 1; break;
    case MENU_SET_BELL_NONE: c->w.bell = 0; break;
    case MENU_SET_BELL_BEEP: c->w.bell = 1; break;
    case MENU_SET_BELL_VISUAL: c->w.bell = 2; break;
    case MENU_SET_BOLD: c->w.bold_bright = on; restyle = 1; break;
    case MENU_SET_META_AMIGA: c->w.meta_alt = 0; break;
    case MENU_SET_META_ALT: c->w.meta_alt = 1; break;
    case MENU_SET_COPY: c->w.copy_on_select = on; break;
    case MENU_SET_WHEEL: c->w.wheel_scroll = on; break;
    case MENU_SET_REFLOW: c->w.reflow = on; restyle = 1; break;
    case MENU_SET_SCROLLBAR:
        c->sbar_on = on;
        tab_command(c, MENU_SET_SCROLLBAR); /* the window's: its owner shows or hides it */
        break;
    case MENU_SET_BS_DEL: c->w.backspace_bs = 0; restyle = 1; break;
    case MENU_SET_BS_BS: c->w.backspace_bs = 1; restyle = 1; break;
    case MENU_SET_CLIP_WRITE: c->w.clip_access = VT_CLIP_WRITE; restyle = 1; break;
    case MENU_SET_CLIP_READ_WRITE: c->w.clip_access = VT_CLIP_WRITE | VT_CLIP_READ; restyle = 1; break;
    case MENU_SET_CLIP_OFF: c->w.clip_access = 0; restyle = 1; break;
    case MENU_SET_UNIX:
    case MENU_SET_KINGCON:
        sel_close(c);
        kc_cyc_end(c);
        c->tabs = 0;
        c->menu_n = 0;
        c->kingcon = id == MENU_SET_KINGCON;
        break;
    case MENU_SET_KC_W: bit = LE_KC_WINDOW; break;
    case MENU_SET_KC_L: bit = LE_KC_LIST; break;
    case MENU_SET_KC_B: bit = LE_KC_CYCLE; break;
    case MENU_SET_KC_C: bit = LE_KC_COMMON; break;
    case MENU_SET_KC_S: bit = LE_KC_SILENT; break;
    case MENU_KC_CACHE: c->kc_cache = on; break;
    case MENU_KC_INFO: c->kc_info = on; break;
    case MENU_SET_SB_NONE: case MENU_SET_SB_500: case MENU_SET_SB_1000: case MENU_SET_SB_2000:
    case MENU_SET_SB_5000:
        {
            static const int lines[] = { 0, 500, 1000, 2000, 5000 };
            if (!vtwin_set_scrollback(&c->w, lines[id - MENU_SET_SB_NONE]))
                DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* no memory for it */
            restyle = 1;
        }
        break;
    default: return 0;
    }
    if (bit) {
        /* KingCON's rules: W and L/B exclude each other; no style at all is W */
        kc_cyc_end(c);
        if (on)
            c->kc_style |= bit;
        else
            c->kc_style &= ~bit;
        if (on && bit == LE_KC_WINDOW)
            c->kc_style &= ~(LE_KC_LIST | LE_KC_CYCLE);
        if (on && (bit == LE_KC_LIST || bit == LE_KC_CYCLE))
            c->kc_style &= ~LE_KC_WINDOW;
        if (!(c->kc_style & (LE_KC_WINDOW | LE_KC_LIST | LE_KC_CYCLE | LE_KC_COMMON)))
            c->kc_style |= LE_KC_WINDOW;
    }
    if (restyle)
        vtwin_apply_settings(&c->w); /* the cursor and colours drawn anew */
    return 1;
}

/* Edit > Clear screen: in the Shell's line, as Ctrl-L does there (the
 * prompt and the line drawn again at the top); for a program, the screen
 * cleared and the cursor home. */
static void clear_screen(con *c)
{
    static const vt_u8 ff = 0x0C;
    static const vt_u8 home_clear[] = "\033[H\033[2J";
    if (!c->raw && !tty_active(c)) {
        cooked_key(c, &ff, 1, 0, 0);
        return;
    }
    vtwin_write(&c->w, home_clear, sizeof(home_clear) - 1);
}

/* UP-Term > About: the build, in a requester on the window's screen */
static void about(con *c)
{
    struct EasyStruct es;
    es.es_StructSize = sizeof(es);
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)"About UP-Term";
    es.es_TextFormat = (UBYTE *)"UP-Term\n\nA terminal for AmigaOS 3: xterm, Amiga and PC-ANSI,\n"
                                "tabs, profiles, outline fonts, slash commands.\n\nBuild %s\n"
                                "Type /help in a window for the commands.\n\n"
                                "Colour emoji: Twemoji by Twitter and contributors, CC-BY 4.0.";
    es.es_GadgetFormat = (UBYTE *)"OK";
    if (c->w.win)
        EasyRequest(c->w.win, &es, 0, (ULONG)STR(VTCON_BUILD));
}

/* One menu item's action by its id, from a pick or a typed /command
 * (handler/slash.c names the same ids). 0: not an item; 1: done; 2: done,
 * a checkmark may have moved (the caller builds the strip again); 3: done,
 * and the strip or the window may be gone. */
static int menu_run(con *c, LONG id, int on)
{
    switch (id) {
    case MENU_COPY: vtwin_copy(&c->w); return 1;
    case MENU_PASTE: vtwin_paste(&c->w); service_reads(c); return 1;
    case MENU_FIND: find_open(c); return 1;
    case MENU_PREFS: prefs_launch(); return 1;
    case MENU_CLOSE: close_gadget(c); return 3; /* as the close gadget */
    case MENU_TAB_NEW: case MENU_TAB_NEXT: case MENU_TAB_PREV: case MENU_TAB_CLOSE: case MENU_DEMO:
        tab_command(c, (int)id);
        return 3;
    case MENU_KC_FILE: kc_menu(c, COMPLETE_FILES); return 1;
    case MENU_KC_COMMAND: kc_menu(c, COMPLETE_COMMANDS); return 1;
    case MENU_KC_DEVICE: kc_menu(c, COMPLETE_DEVICES); return 1;
    case MENU_KC_RESET: complete_cache_reset(); return 1;
    case MENU_KC_PURGE: complete_cache_purge(); return 1;
    case MENU_SET_FONT: font_ask(c); return 1;
    case MENU_SET_THEME: theme_ask(c, ""); return 1;
    case MENU_SET_SAVE: save_ask(c); return 1;
    case MENU_ABOUT: about(c); return 1;
    case MENU_SCREEN_WB: case MENU_SCREEN_OWN: case MENU_SCREEN_FULL:
        /* after the IDCMP loop has replied its messages (the window goes) */
        c->screen_want = (int)(id - MENU_SCREEN_WB);
        return 3;
    case MENU_SELECT_ALL: vtwin_select_all(&c->w); return 1;
    case MENU_FIND_NEXT:
        if (!vtwin_find(&c->w, 0))
            DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* no more, or no query yet */
        return 1;
    case MENU_CLEAR_SCREEN: clear_screen(c); return 1;
    case MENU_CLEAR_SB: vtwin_clear_scrollback(&c->w); return 1;
    case MENU_RESET: vtwin_reset(&c->w); return 1;
    case MENU_FONT_BIGGER: case MENU_FONT_SMALLER:
        if (!vtwin_font_step(&c->w, id == MENU_FONT_BIGGER ? 1 : -1))
            DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* no designed size that way */
        return 1;
    case MENU_SIZE_80X24: case MENU_SIZE_132X43:
        if (!vtwin_set_size(&c->w, id == MENU_SIZE_80X24 ? 80 : 132, id == MENU_SIZE_80X24 ? 24 : 43))
            DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* the screen is too small */
        return 1;
    default:
        break;
    }
    if (id >= MENU_SET_PROFILE0 && id < MENU_SET_PROFILE0 + UC_MAX_PROFILES) {
        profile_switch(c, (int)(id - MENU_SET_PROFILE0));
        return 2;
    }
    return menu_setting(c, id, on) ? 2 : 0;
}

static void menu_pick(con *c, UWORD code)
{
    while (code != MENUNULL && c->menustrip && c->w.win) {
        struct MenuItem *it = ItemAddress(c->menustrip, code);
        int r;
        if (!it)
            break;
        r = menu_run(c, (LONG)GTMENUITEM_USERDATA(it), (it->Flags & CHECKED) != 0);
        if (r == 2 && c->w.win) {
            /* the strip again: every checkmark from the settings (one pick
             * can move others: KingCON's W clears L and B) */
            menu_remove(c, c->w.win);
            menu_add(c, c->w.win);
        }
        if (r >= 2)
            return; /* the old strip, and its NextSelect chain, are gone */
        code = it->NextSelect;
    }
}

static struct Window *tab_register(con *c);
static void tab_unregister(con *c);

/* ---- a screen of its own (P1) ------------------------------------------------ */

/* Intuition's pens for a screen of our own, from the Workbench's: each of
 * its DrawInfo pens with the colour it has there. cols holds the 16 ANSI
 * colours (SA_Colors32, 32-bit components); with room past them (more
 * than 4 planes) each Workbench colour gets a pen of its own after them,
 * else the nearest ANSI pen. pens: the SA_Pens array (NUMDRIPENS + 1),
 * ~0-ended.
 * The number of pens added past the 16; without a Workbench, a grey look
 * from the ANSI colours. */
static int wb_pens(struct Screen *wb, int depth, ULONG *cols, UWORD *pens)
{
    static const UWORD grey[] = { 0, 7, 0, 15, 0, 7, 0, 8, 15, 0, 7, 0, (UWORD)~0 };
    struct DrawInfo *di = wb ? GetScreenDrawInfo(wb) : 0;
    UWORD from[NUMDRIPENS];
    int i, j, n = 0, np;
    if (!di) {
        for (i = 0; i < (int)(sizeof(grey) / sizeof(grey[0])); i++)
            pens[i] = grey[i];
        return 0;
    }
    np = di->dri_NumPens < NUMDRIPENS ? di->dri_NumPens : NUMDRIPENS;
    for (i = 0; i < np; i++) {
        ULONG rgb[3];
        UWORD wp = di->dri_Pens[i];
        for (j = 0; j < i; j++)
            if (from[j] == wp)
                break;
        from[i] = wp;
        if (j < i) {
            pens[i] = pens[j]; /* the same Workbench pen: the same one here */
            continue;
        }
        GetRGB32(wb->ViewPort.ColorMap, wp, 1, rgb);
        if (depth > 4) {
            cols[1 + (16 + n) * 3] = rgb[0];
            cols[2 + (16 + n) * 3] = rgb[1];
            cols[3 + (16 + n) * 3] = rgb[2];
            pens[i] = (UWORD)(16 + n++);
        } else {
            /* 16 pens, all ANSI: the nearest of them */
            ULONG best = ~0UL;
            int k;
            for (k = 0; k < 16; k++) {
                LONG dr = (LONG)(cols[1 + k * 3] >> 24) - (LONG)(rgb[0] >> 24);
                LONG dg = (LONG)(cols[2 + k * 3] >> 24) - (LONG)(rgb[1] >> 24);
                LONG db = (LONG)(cols[3 + k * 3] >> 24) - (LONG)(rgb[2] >> 24);
                ULONG d = (ULONG)(dr * dr + dg * dg + db * db);
                if (d < best) {
                    best = d;
                    pens[i] = (UWORD)k;
                }
            }
        }
    }
    pens[np] = (UWORD)~0; /* the pens this Intuition knows; it fills the rest */
    FreeScreenDrawInfo(wb, di);
    return n;
}

/* The screen for OWNSCREEN / FULLSCREEN / PUBSCREEN: the Workbench's mode
 * and size unless SCREENMODE says otherwise, 32 colours on an AGA screen
 * (16 on ECS, 8 planes on a graphics card), its first 16 pens the
 * terminal's ANSI colours (the profile's palette over xterm's) so every
 * colour has its exact pen, the next the Workbench's colours for
 * Intuition's frames and bars (wb_pens). Public, so programs can
 * open on it: the name given, else "UP-Term", "UP-Term.2" ... Locked for
 * the window as a public screen is (c->locked). */
static struct Screen *own_screen_open(con *c)
{
    ULONG cols[1 + (16 + NUMDRIPENS) * 3 + 1];
    UWORD pens[NUMDRIPENS + 1];
    struct TagItem t[20];
    struct Screen *wb, *scr = 0;
    ULONG mode = c->mode_id, err = 0;
    int depth = c->depth, i, n, k, extra;
    char name[32];
    wb = LockPubScreen((UBYTE *)"Workbench");
    if (wb && mode == (ULONG)INVALID_ID)
        mode = GetVPModeID(&wb->ViewPort);
    if (mode == (ULONG)INVALID_ID)
        mode = HIRES_KEY;
    if (!depth) {
        /* the mode's own kind decides: a graphics card's mode (foreign to
         * the chipset) is chunky, 8 bits at least; a native one gets the 4
         * planes the 16 colours need, 5 where the chipset has them (a
         * Workbench on a card asking for a PAL mode got 8 planes before) */
        struct DisplayInfo di;
        struct DimensionInfo dims;
        depth = 4;
        if (GetDisplayInfoData(0, (UBYTE *)&di, sizeof(di), DTAG_DISP, mode) &&
            (di.PropertyFlags & DIPF_IS_FOREIGN))
            depth = 8;
        /* a P96 mode is not marked foreign (rig 2026-10-03: 0x50041303 got
         * 4 planes asked and 24 bits given); its depth says it */
        if (GetDisplayInfoData(0, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS, mode)) {
            if (dims.MaxDepth > 8)
                depth = 8;
            else if (depth == 4 && dims.MaxDepth >= 5)
                depth = 5; /* AGA: the Workbench's own colours past the 16 (wb_pens) */
        }
    }
    if (depth < 4)
        depth = 4; /* 16 pens: the ANSI colours */
    if (depth > 8)
        depth = 8;
    for (i = 0; i < 16; i++) {
        ULONG rgb = (c->w.pal16[i] & 0x01000000UL) ? (c->w.pal16[i] & 0xFFFFFFUL) : vt_palette_rgb(0, i);
        cols[1 + i * 3] = ((rgb >> 16) & 0xFF) * 0x01010101UL;
        cols[2 + i * 3] = ((rgb >> 8) & 0xFF) * 0x01010101UL;
        cols[3 + i * 3] = (rgb & 0xFF) * 0x01010101UL;
    }
    /* the frames, title bars and menus in the user's Workbench colours
     * (owner 2026-10-03: "they should use the colors from the users wb
     * settings") */
    extra = wb_pens(wb, depth, cols, pens);
    if (wb)
        UnlockPubScreen(0, wb);
    cols[0] = ((ULONG)(16 + extra) << 16) | 0;
    cols[1 + (16 + extra) * 3] = 0;
    for (k = 1; k <= 9 && !scr; k++) {
        if (c->pubname[0])
            copy_str(name, c->pubname, sizeof(name));
        else
            copy_str(name, "UP-Term", sizeof(name));
        if (k > 1 && !c->pubname[0]) {
            n = (int)strlen(name);
            name[n] = '.';
            name[n + 1] = (char)('0' + k);
            name[n + 2] = 0;
        }
        n = 0;
        t[n].ti_Tag = SA_DisplayID;  t[n++].ti_Data = mode;
        t[n].ti_Tag = SA_Depth;      t[n++].ti_Data = (ULONG)depth;
        t[n].ti_Tag = SA_Width;      t[n++].ti_Data = (ULONG)STDSCREENWIDTH;
        t[n].ti_Tag = SA_Height;     t[n++].ti_Data = (ULONG)STDSCREENHEIGHT;
        t[n].ti_Tag = SA_Overscan;   t[n++].ti_Data = OSCAN_TEXT;
        t[n].ti_Tag = SA_Title;      t[n++].ti_Data = (ULONG)"UP-Term";
        t[n].ti_Tag = SA_PubName;    t[n++].ti_Data = (ULONG)name;
        t[n].ti_Tag = SA_Pens;       t[n++].ti_Data = (ULONG)pens;
        t[n].ti_Tag = SA_Colors32;   t[n++].ti_Data = (ULONG)cols;
        t[n].ti_Tag = SA_SharePens;  t[n++].ti_Data = TRUE;
        t[n].ti_Tag = SA_Interleaved; t[n++].ti_Data = TRUE;
        t[n].ti_Tag = SA_AutoScroll; t[n++].ti_Data = TRUE;
        t[n].ti_Tag = SA_ShowTitle;  t[n++].ti_Data = c->own_screen == 2 ? FALSE : TRUE;
        t[n].ti_Tag = SA_ErrorCode;  t[n++].ti_Data = (ULONG)&err;
        t[n].ti_Tag = TAG_DONE;      t[n].ti_Data = 0;
        err = 0;
        scr = OpenScreenTagList(0, t);
        DBG("own screen mode/depth", mode, depth);
        DBG("own screen scr/err", scr, err);
        if (!scr && (err != OSERR_PUBNOTUNIQUE || c->pubname[0]))
            break; /* not a name clash, or the name was the user's: no other try */
    }
    if (!scr)
        return 0;
    /* the 16 ANSI pens shared, with their colours: ObtainBestPen matches
     * only allocated pens and otherwise takes a free one and sets ITS
     * colour -- the first requests overwrote the palette (rig, PAL hires
     * 4 planes: 9 of the 16 colours moved) */
    for (i = 0; i < 16 + extra; i++)
        ObtainPen(scr->ViewPort.ColorMap, (ULONG)i, cols[1 + i * 3], cols[2 + i * 3], cols[3 + i * 3], 0);
    PubScreenStatus(scr, 0); /* public now: programs may open on it */
    copy_str(c->pubname, name, sizeof(c->pubname));
    c->myscreen = scr;
    return scr;
}

/* The screen goes with its last window: private again (no new visitors),
 * then closed -- a visitor still open (another program's window) keeps it
 * a little longer, and then it stays until that program closes. */
static void own_screen_close(con *c)
{
    int tries;
    if (!c->myscreen)
        return;
    PubScreenStatus(c->myscreen, PSNF_PRIVATE);
    for (tries = 0; tries < 10; tries++) {
        if (CloseScreen(c->myscreen)) {
            c->myscreen = 0;
            return;
        }
        WaitTOF(); /* a visitor's window closing */
    }
    /* another program's window is still on it: closed when that goes
     * (own_screen_retry, from the main loop, and before the handler ends) */
    c->screen_closing = c->myscreen;
    c->myscreen = 0;
}

/* A screen whose close was refused: again; 1 while it is still open. */
static int own_screen_retry(con *c)
{
    if (c->screen_closing && CloseScreen(c->screen_closing))
        c->screen_closing = 0;
    return c->screen_closing != 0;
}

/* The screen the window opens on, locked: its own (OWNSCREEN /
 * FULLSCREEN / PUBSCREEN, opened now if need be), the named public screen,
 * the default one. 0 when none can be locked. */
static struct Screen *lock_screen(con *c)
{
    struct Screen *scr = 0;
    DBG("own_screen", c->own_screen, c->myscreen);
    if (c->own_screen && !c->tab_host[0] && !c->foreign && (c->myscreen || own_screen_open(c)))
        scr = LockPubScreen((UBYTE *)c->pubname); /* ours, locked as any public screen */
    if (!scr)
        scr = LockPubScreen(c->screen[0] ? (UBYTE *)c->screen : 0);
    if (!scr)
        scr = LockPubScreen(0);
    return scr;
}

/* The window on scr: the spec's place and flags, or the whole screen,
 * borderless, for FULLSCREEN on its own screen. */
static struct Window *create_window(con *c, struct Screen *scr)
{
    struct TagItem tags[18];
    struct Window *win;
    int n = 0;
    if (c->myscreen && scr == c->myscreen && c->own_screen == 2) {
        /* FULLSCREEN: the whole screen, no borders, behind everything */
        tags[n].ti_Tag = WA_Left;    tags[n++].ti_Data = 0;
        tags[n].ti_Tag = WA_Top;     tags[n++].ti_Data = 0;
        tags[n].ti_Tag = WA_Width;   tags[n++].ti_Data = scr->Width;
        tags[n].ti_Tag = WA_Height;  tags[n++].ti_Data = scr->Height;
        tags[n].ti_Tag = WA_Flags;   tags[n++].ti_Data = WFLG_BACKDROP | WFLG_BORDERLESS | WFLG_ACTIVATE |
                                                           WFLG_SMART_REFRESH;
        /* no window title (it would draw a title bar over the text): the
         * screen's title bar, shown with the menus, says it */
        tags[n].ti_Tag = WA_ScreenTitle; tags[n++].ti_Data = (ULONG)c->w.title;
        c->w.title_on_screen = 1;
    } else {
        c->w.title_on_screen = 0;
        tags[n].ti_Tag = WA_Left;    tags[n++].ti_Data = c->wx;
        tags[n].ti_Tag = WA_Top;     tags[n++].ti_Data = c->wy;
        tags[n].ti_Tag = WA_Width;   tags[n++].ti_Data = c->ww;
        tags[n].ti_Tag = WA_Height;  tags[n++].ti_Data = c->wh;
        tags[n].ti_Tag = WA_Flags;   tags[n++].ti_Data = c->wflags;
        tags[n].ti_Tag = WA_Title;   tags[n++].ti_Data = (ULONG)c->w.title;
    }
    tags[n].ti_Tag = WA_IDCMP;       tags[n++].ti_Data = IDCMP_RAWKEY | IDCMP_NEWSIZE |
        IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE |
        IDCMP_EXTENDEDMOUSE | IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW | IDCMP_MENUPICK;
    tags[n].ti_Tag = WA_PubScreen;   tags[n++].ti_Data = (ULONG)scr;
    tags[n].ti_Tag = WA_MinWidth;    tags[n++].ti_Data = 80;
    tags[n].ti_Tag = WA_MinHeight;   tags[n++].ti_Data = 40;
    tags[n].ti_Tag = WA_MaxWidth;    tags[n++].ti_Data = (ULONG)~0;
    tags[n].ti_Tag = WA_MaxHeight;   tags[n++].ti_Data = (ULONG)~0;
    tags[n].ti_Tag = WA_AutoAdjust;  tags[n++].ti_Data = TRUE;
    /* the 3.x menu look (screen-coloured, not the 1.3 black and white);
     * LayoutMenus asks for it too (menu_add) */
    tags[n].ti_Tag = WA_NewLookMenus; tags[n++].ti_Data = TRUE;
    tags[n].ti_Tag = TAG_DONE;       tags[n].ti_Data = 0;
    DBG("openwindow", scr, c->w.font);
    win = OpenWindowTagList(0, tags);
    DBG("window", win, 0);
    return win;
}

static int open_window(con *c)
{
    struct Screen *scr;
    struct Window *win;

    c->winch_closing = 0; /* read/write retry the open: arm the handler again */
    DBG("lockpub", 0, 0);
    scr = lock_screen(c);
    if (!scr)
        return 0;
    c->locked = scr;
    vtwin_set_screen(&c->w, scr); /* square pixels or tall: the font pair's choice */
    vtwin_open_font(&c->w);
    if (c->tab_host[0] && (win = tab_register(c)) != 0)
        goto have_window; /* a tab: the host's window, its RastPort ours */
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
    win = create_window(c, scr);
    if (!win)
        return 0;
have_window:
    if (!c->is_tab)
        c->own_win = win; /* ours: its IDCMP is read here (a foreign one's too) */
    if (!vtwin_attach(&c->w, win)) {
        c->w.win = win; /* close_window closes it */
        close_window(c); /* no window without its grid: read/write retry the open */
        return 0;
    }
    DBG("vt_new", c->w.t, 0);
    menu_add(c, win);
    sbar_apply(c); /* our own sizable window: the scroll bar in its border */
    le_free(&c->le); /* an AUTO window opening again: the last one's history (reloaded below) */
    le_init(&c->le, c->w.t, le_out, c);
    c->le.utf8 = c->w.pers == VT_XTERM && !c->w.latin1 && !c->w.cp437;
    history_load(c); /* the saved history, read by a worker */
    DBG("vr_init", c->w.r.cols, c->w.r.rows);
    return 1;
}

/* What belongs to the window and not to the terminal: the Find prompt,
 * the selection window, menus, the input handler, the ROM console unit.
 * close_window, and a move to another screen, take these down first. */
static void window_parts_close(con *c)
{
    find_close(c); /* the prompt belongs to the window */
    sel_close(c);  /* so does the selection window */
    if (c->tm && c->tm->m.open) {
        /* and /theme's list: cancelled, the reader gets its line */
        c->tm->m.open = 0;
        c->tm->end = LE_MENU_CANCEL;
        theme_line_release(c);
        theme_menu_settle(c);
    }
    sbar_gad_close(&c->sbar); /* and the scroll bar in its border */
    kc_cyc_end(c);
    menu_remove(c, c->w.win);
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
}

static void close_window(con *c)
{
    struct Window *win = c->w.win;
    c->winch_closing = 1; /* stop naming this window before we dismantle it */
    DBG("close_window", win, c->w.t);
    window_parts_close(c);
    if (c->is_tab)
        tab_unregister(c); /* the host stops sending before the grid goes */
    vtwin_detach(&c->w); /* engine, renderer, fonts; the frame clock stops */
    if (win && !c->is_tab) {
        if (win == c->foreign)
            ModifyIDCMP(win, c->foreign_idcmp); /* hand it back as we found it */
        else
            CloseWindow(win);
    }
    c->own_win = 0;
    c->is_tab = 0;
    if (c->locked) {
        UnlockPubScreen(0, c->locked);
        c->locked = 0;
    }
    own_screen_close(c); /* its screen goes with it */
}

/* The window to another screen, its terminal with it (Settings > Screen,
 * /screen; P1.3): 0 the Workbench (or the SCREEN the spec named), 1 a
 * screen of its own, 2 full screen. The terminal is unbound from the old
 * window and bound to the new one -- text, scrollback, modes, the line
 * being edited stay. Not with tabs (they share the window) or someone
 * else's window. 1 when it moved. */
static int screen_switch(con *c, int mode)
{
    struct Screen *scr;
    struct Window *win;
    if (!c->w.t || !c->own_win || c->is_tab || c->foreign || c->ntabs >= 2 || mode < 0 || mode > 2)
        return 0;
    if (mode == c->own_screen)
        return 1;
    window_parts_close(c);
    vtwin_unbind(&c->w);
    CloseWindow(c->own_win);
    c->own_win = 0;
    if (c->locked) {
        UnlockPubScreen(0, c->locked);
        c->locked = 0;
    }
    own_screen_close(c); /* own -> full opens a fresh one: borderless, no title bar */
    c->own_screen = mode;
    scr = lock_screen(c);
    win = scr ? create_window(c, scr) : 0;
    if (!win && mode) {
        /* no screen of its own (no memory, a bad mode): back where it was */
        if (scr)
            UnlockPubScreen(0, scr);
        own_screen_close(c);
        c->own_screen = 0;
        scr = lock_screen(c);
        win = scr ? create_window(c, scr) : 0;
    }
    if (!win) {
        if (scr)
            UnlockPubScreen(0, scr);
        c->auto_shut = 0;
        c->eof = 1; /* nowhere to show the terminal: the reader sees the end */
        return 0;
    }
    c->locked = scr;
    c->own_win = win;
    vtwin_set_screen(&c->w, scr);
    vtwin_rebind(&c->w, win);
    vtwin_fit_aspect(&c->w); /* topaz <-> Topaz Pro for the new screen's pixels */
    menu_add(c, win);
    sbar_apply(c); /* none on the borderless full-screen window: it has no border */
    return c->own_screen == mode;
}

static void screen_pending(con *c)
{
    if (c->screen_want >= 0) {
        int m = c->screen_want;
        c->screen_want = -1;
        if (!screen_switch(c, m))
            DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* tabs, or no such screen */
    }
}

/* ---- output ---------------------------------------------------------------- */

static void output(con *c, const vt_u8 *b, long n)
{
#if defined(VTCON_DEBUG) || defined(VTCON_PROF)
    struct EClockVal e0, e1;
    ReadEClock(&e0);
#endif
    if (!c->w.t)
        return;
    if (!c->le.len)
        c->le.started = 0; /* the next line starts wherever this output ends */
    vtwin_write(&c->w, b, n);
#if defined(VTCON_DEBUG) || defined(VTCON_PROF)
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
    DBG("tty leave owner/alive", (long)c->tty_owner, task_alive(c->tty_owner));
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
    DBG("tty enter owner/was", (long)owner, c->tty);
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

/* The worker's reply port and the requests asked for (WORK_*), each made
 * when it is first needed: the history's at open, the check's at the first
 * typed word, the completion's (11 KB, with its lists) at the first Tab --
 * not all three at open (research/2026-10-04_window-memory.md). */
static int ensure_worker(con *c, int want)
{
    if (!c->comp_port)
        c->comp_port = CreateMsgPort();
    if ((want & WORK_COMP) && !c->comp)
        c->comp = complete_req_new(1);
    if ((want & WORK_CHECK) && !c->check)
        c->check = complete_req_new(0);
    if ((want & WORK_HIST) && !c->hist)
        c->hist = complete_req_new(0);
    return c->comp_port && (!(want & WORK_COMP) || c->comp) && (!(want & WORK_CHECK) || c->check) &&
           (!(want & WORK_HIST) || c->hist);
}

/* The completion menu's names (COMPLETE_NAMES), made at the first menu. */
static char *menu_buf(con *c)
{
    if (!c->menu)
        c->menu = (char *)AllocVec(COMPLETE_NAMES, MEMF_ANY);
    return c->menu;
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
    if (c->comp_busy || !ensure_worker(c, WORK_COMP))
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
    c->comp->kingcon = 0;
    c->comp->no_cache = 0;
    c->comp->cold = c->next_cold;
    c->next_cold = 0;
    c->comp->extra_len = 0;
    if (extra && n > 0 && n <= COMPLETE_EXTRA) {
        CopyMem((APTR)extra, c->comp->extra, n);
        c->comp->extra_len = n;
    }
    copy_latin1(le, a, le->pos, c->comp->word, COMPLETE_MAX);
    c->comp_edits = c->edits;
    if (complete_start(c->comp, c->comp_port, opener(c)))
        c->comp_busy = 1;
}

/* A shell is reading: an AmigaShell between commands (its CLI has no
 * command loaded), or the shell that told us its words (vsh, which runs as
 * a command). Any other program reading a line (Ask, C:Claude PLAIN, an
 * installer) gets its line without command colours (W31). */
static int task_is_shell(con *c, struct Task *t)
{
    struct CommandLineInterface *cli;
    if (!t)
        return 1; /* unknown: keep the old behaviour */
    if (t == c->words_owner)
        return 1;
    if (t->tc_Node.ln_Type != NT_PROCESS || !((struct Process *)t)->pr_CLI)
        return 0;
    cli = (struct CommandLineInterface *)BADDR(((struct Process *)t)->pr_CLI);
    return cli->cli_Module == 0;
}

/* Is the first word a command? Asked whenever it changes (the answer
 * colours it green or red, see le_set_command). */
static void check_command(con *c)
{
    unsigned char w[64];
    if (!c->reader_shell) {
        le_no_command(&c->le);
        return;
    }
    le_first_word(&c->le, w, sizeof(w));
    if (!w[0] || c->check_busy || !strcmp((const char *)w, c->checked))
        return;
    if (c->le.len && c->le.buf[0] == '/') {
        /* one of UP-Term's /commands: green, as a command the shell has */
        slash_cmd sc;
        char e[8];
        if (slash_parse((const char *)w, (int)strlen((const char *)w), &sc, e, sizeof(e)) != SLASH_NOT_OURS) {
            strcpy(c->checked, (const char *)w);
            le_set_command(&c->le, w, 1);
            return;
        }
    }
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
    if (!ensure_worker(c, WORK_CHECK))
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

/* W44: a key changed the first word as it is typed -- the word stays plain
 * and its colour waits for the keys to rest, counted on the frame clock
 * (command_wait), as /theme's list counts its rest; with no clock, at once. */
static void command_clock(con *c)
{
    if (c->le.cmd_rest_us <= 0)
        return;
    if (c->w.frame_open)
        vtwin_clock(&c->w);
    else
        le_command_now(&c->le);
}

/* The frame clock waited us: once the keys have rested, the word's colour
 * on screen (its cells only); still counting, the clock keeps running. */
static void command_wait(con *c, ULONG us)
{
    if (c->raw || !c->w.t) {
        c->le.cmd_rest_us = 0; /* no line being edited to colour */
        return;
    }
    if (!le_command_rested(&c->le, (long)us) && c->le.cmd_rest_us > 0)
        vtwin_clock(&c->w);
}

/* The history file: loaded once when the window opens, then each entered
 * line appended (one worker at a time; lines queue meanwhile). */
static void history_next(con *c)
{
    int i;
    if (c->hist_busy || !c->hist_queue_len || !ensure_worker(c, WORK_HIST))
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
    if (!ensure_worker(c, WORK_HIST) || c->hist_busy)
        return;
    c->hist->data = 0; /* the worker makes it at the file's size */
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
    if (!c->raw)
        le_command_now(&c->le); /* a completion, not typing: the word's colour at once */
}

static void kc_put(con *c, const char *name);
static void kc_tab(con *c, int mode);

/* W22: a command completion answered before every command directory was
 * cached (complete.c never makes a Tab wait for a directory to be read).
 * What the cached names share goes in, never the space or "/" that would
 * end the word, with no beep and no menu (they would judge from half a
 * list); when the warm-up has read the rest, the Tab is asked again --
 * unless a key came meanwhile. KingCON's window, list and cycle (several
 * names) work on what is cached, as they always did on KingCON's cache. */
static void partial_answer(con *c, struct complete_req *q)
{
    if (q->kingcon) {
        if (q->matches > 1) {
            kc_finish(c, q);
            return;
        }
        if (q->matches == 1)
            kc_put(c, q->common);
    } else if (q->add[0]) {
        type_text(c, q->add);
        check_command(c);
    }
    c->comp_partial = q->kingcon ? 2 : 1;
    c->partial_edits = c->edits;
    c->partial_gen = q->warm_gen;
    c->partial_now = complete_warm_wait(FindTask(0), 1UL << c->comp_port->mp_SigBit, q->warm_gen,
                                        opener(c));
}

/* the warm-up ended (its signal is the completion port's): the Tab again,
 * now allowed to read a directory still missing (it ends there) */
static void partial_refine(con *c)
{
    int kind = c->comp_partial;
    if (!kind || (!c->partial_now && complete_warm_gen() == c->partial_gen))
        return;
    c->comp_partial = 0;
    c->partial_now = 0;
    complete_warm_forget(FindTask(0));
    if (c->edits != c->partial_edits || c->comp_busy || !c->w.t || c->raw)
        return; /* the line moved on */
    c->next_cold = 1;
    if (kind == 2)
        kc_tab(c, COMPLETE_COMMANDS);
    else
        start_completion(c);
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
        if (q->mode == CONFIG_SAVE) {
            if (q->matches && c->save_work) {
                FreeVec(c->conf); /* the staged table is the file's now: it becomes the window's */
                c->conf = c->save_work;
                c->save_work = 0;
            } else
                DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* not written: the old file stands */
            comp_data_done(c);
            continue;
        }
        if (q->mode == COMPLETE_THEME && c->tm && c->tm->asked >= 0) {
            /* an entry of /theme's list: its colours on the window */
            struct theme_menu *tm = c->tm;
            int got = tm->asked;
            tm->asked = -1;
            if (tm->end != LE_MENU_CANCEL && got == tm->m.sel) {
                tm->last = got;
                if (q->matches && c->w.t && theme_apply(c, q->data, q->data_len)) {
                    tm->shown = got;
                    copy_str(tm->shown_path, q->add, COMPLETE_MAX);
                }
            } /* else cancelled, or the bar moved on: no repaint for it */
            comp_data_done(c);
            if (tm->end)
                theme_menu_settle(c);
            else if (!tm->m.rest_us)
                theme_menu_read(c, tm->m.sel); /* moved and rested meanwhile */
            continue;
        }
        if (q->mode == COMPLETE_THEME) {
            if (q->matches && c->w.t) {
                if (theme_apply(c, q->data, q->data_len))
                    theme_keep(c, q->add);
            } else if (q->word[0])
                DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* "/theme NAME": no such theme */
            comp_data_done(c);
            continue;
        }
        if (q->mode == COMPLETE_THEMES) {
            theme_menu_open(c, q);
            continue;
        }
        if (q->mode == COMPLETE_FONT) {
            /* Settings > Font...: the window in the chosen font, live */
            if (q->matches && c->w.t && !vtwin_set_font(&c->w, q->add, (WORD)q->font_size))
                DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* not fixed width, or gone */
            continue;
        }
        c->menu_n = 0;
        if (!c->w.t)
            continue; /* the window closed (AUTO): the answer has no line */
        if (c->edits != c->comp_edits)
            continue; /* the line changed while it ran (rig: typed text got the
                       * answer for an older word): the answer is for no word now */
        if (q->partial && !q->cold && q->mode == COMPLETE_COMMANDS) { /* a refine is final */
            partial_answer(c, q);
            continue;
        }
        if (q->kingcon) {
            kc_finish(c, q);
            continue;
        }
        if (q->matches > 1 && q->names_len < COMPLETE_NAMES && menu_buf(c)) {
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
    partial_refine(c); /* a warm-up we waited for has ended */
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

/* ---- slash commands (ledger C1, handler/slash.c) ------------------------------- */

/* a = b + c + d, cut to cap */
static void cat3(char *a, int cap, const char *b, const char *c2, const char *d)
{
    copy_str(a, b, cap);
    if ((int)strlen(a) < cap - 1)
        copy_str(a + strlen(a), c2, cap - (int)strlen(a));
    if ((int)strlen(a) < cap - 1)
        copy_str(a + strlen(a), d, cap - (int)strlen(a));
}

/* RRGGBB or none into *rgb (VR_KEEP for none); 0 when neither */
static int slash_colour(const char *arg, ULONG *rgb)
{
    uc_u32 v;
    if (str_ieq(arg, "none")) {
        *rgb = VR_KEEP;
        return 1;
    }
    if (!upconf_hex6(arg, &v))
        return 0;
    *rgb = (ULONG)v;
    return 1;
}

/* A /command line: run it, and its answer into ans (one or more lines,
 * each ending "\n"). typed: from the line editor (the reader waits for the
 * line), not C:UPTerm's packet. 0 when the line is not one (the
 * program's), 1 done, 2 refused (ans says why). */
static int slash_run(con *c, const char *line, int len, char *ans, int cap, int typed)
{
    slash_cmd cmd;
    char err[160];
    int r = slash_parse(line, len, &cmd, err, sizeof(err)), m;
    const char *name;
    ULONG rgb;
    if (r == SLASH_NOT_OURS)
        return 0;
    ans[0] = 0;
    if (r == SLASH_ERROR) {
        cat3(ans, cap, err, "\n", "");
        return 2;
    }
    name = cmd.def->name;
    cat3(ans, cap, name, ": ", cmd.arg[0] ? cmd.arg : "done");
    cat3(ans + strlen(ans), cap - (int)strlen(ans), "\n", "", "");
    switch (cmd.id) {
    case SLASH_HELP:
        if (slash_help(cmd.arg, ans, cap) < 0) {
            cat3(ans, cap, "help: no command ", cmd.arg, "\n");
            return 2;
        }
        return 1;
    case SLASH_SCROLLBACK: {
        long n = 0;
        const char *p = cmd.arg;
        if (!str_ieq(p, "none")) {
            for (; *p >= '0' && *p <= '9' && n < 100000L; p++)
                n = n * 10 + (*p - '0');
            if (*p || !n) {
                cat3(ans, cap, name, ": LINES | none", "\n");
                return 2;
            }
        }
        if (!vtwin_set_scrollback(&c->w, (int)n)) {
            cat3(ans, cap, name, ": no memory for that many", "\n");
            return 2;
        }
        return 1;
    }
    case SLASH_FONT:
        if (!cmd.arg[0]) {
            font_ask(c);
            cat3(ans, cap, "", "", "");
            return 1;
        }
        {
            char fname[40];
            WORD size = 0;
            parse_font(cmd.arg, fname, sizeof(fname), &size);
            if (!vtwin_set_font(&c->w, fname, size)) {
                cat3(ans, cap, name, ": no fixed-width font ", "by that name and size\n");
                return 2;
            }
        }
        return 1;
    case SLASH_FALLBACK:
        copy_str(c->w.fallback, str_ieq(cmd.arg, "none") ? "" : cmd.arg, sizeof(c->w.fallback));
        vtwin_apply_settings(&c->w);
        if (c->w.fallback[0] && !c->w.outline) {
            cat3(ans, cap, name, ": not installed, or no engine for it (FONTS:", "<name>.otag, LIBS:ttf.library)\n");
            return 2;
        }
        return 1;
    case SLASH_FG: case SLASH_BG: case SLASH_CURSOR_COLOR: case SLASH_SEL_FG: case SLASH_SEL_BG:
        if (!slash_colour(cmd.arg, &rgb)) {
            cat3(ans, cap, name, ": RRGGBB | none", "\n");
            return 2;
        }
        if (cmd.id == SLASH_FG)
            c->w.fg_rgb = rgb;
        else if (cmd.id == SLASH_BG)
            c->w.bg_rgb = rgb;
        else if (cmd.id == SLASH_CURSOR_COLOR)
            c->w.cursor_rgb = rgb;
        else if (cmd.id == SLASH_SEL_FG)
            c->w.sel_fg_rgb = rgb;
        else
            c->w.sel_bg_rgb = rgb;
        vtwin_apply_settings(&c->w);
        return 1;
    case SLASH_KC_MODE:
        kc_cyc_end(c);
        c->kc_style = le_kc_fncmode(cmd.arg);
        break;
    case SLASH_LINK_OPEN:
        /* this window's, until Save settings to profile keeps it */
        copy_str(c->link_open, str_ieq(cmd.arg, "none") ? "" : cmd.arg, sizeof(c->link_open));
        return 1;
    case SLASH_THEME:
        if (!cmd.arg[0] && typed) {
            /* the themes under the line, chosen with the keys (W30); the
             * reader's empty line waits until the list closes */
            c->comp_edits = c->edits;
            if (!theme_req(c, COMPLETE_THEMES, "")) {
                cat3(ans, cap, name, ": busy, try again", "\n");
                return 2;
            }
            c->line_held = 1;
            ans[0] = 0;
            return 1;
        }
        theme_ask(c, cmd.arg);
        if (!cmd.arg[0])
            ans[0] = 0; /* the requester answers (C:UPTerm /theme) */
        return 1;
    case SLASH_PROFILE: {
        const char *names[UC_MAX_PROFILES + 1];
        int np = c->conf ? upconf_profiles(c->conf, names) : 0, k;
        for (k = 0; k < np && !str_ieq(names[k], cmd.arg); k++)
            ;
        if (k == np) {
            cat3(ans, cap, name, ": no profile ", cmd.arg);
            cat3(ans + strlen(ans), cap - (int)strlen(ans), "\n", "", "");
            return 2;
        }
        cmd.id = MENU_SET_PROFILE0 + k;
        break;
    }
    case SLASH_SIZE: {
        long cols = 0, rows = 0;
        const char *p = cmd.arg;
        for (; *p >= '0' && *p <= '9'; p++)
            cols = cols * 10 + (*p - '0');
        if (*p == 'x' || *p == 'X')
            for (p++; *p >= '0' && *p <= '9'; p++)
                rows = rows * 10 + (*p - '0');
        if (*p || cols < 2 || rows < 1 || cols > 400 || rows > 200) {
            cat3(ans, cap, name, ": COLSxROWS, e.g. 80x24", "\n");
            return 2;
        }
        if (!vtwin_set_size(&c->w, (int)cols, (int)rows)) {
            cat3(ans, cap, name, ": the screen is too small for it", "\n");
            return 2;
        }
        return 1;
    }
    case SLASH_FIND:
        if (!vtwin_find(&c->w, cmd.arg[0] ? cmd.arg : 0)) {
            cat3(ans, cap, name, ": not found", "\n");
            return 2;
        }
        return 1;
    default:
        break;
    }
    if (cmd.id == SLASH_KC_MODE)
        m = 2;
    else
        m = menu_run(c, cmd.id, cmd.on);
    if (m == 2 && c->w.win) {
        menu_remove(c, c->w.win); /* the menus' checkmarks follow */
        menu_add(c, c->w.win);
    }
    if (cmd.id == MENU_SET_SAVE || cmd.id == MENU_PREFS)
        ans[0] = 0; /* their own requester / window answers */
    return 1;
}

/* Tab on a /command line: the table's names and values, as the Shell's
 * completion offers files (Tab again lists them, then cycles). 0 when the
 * line is not a command line. */
static int slash_tab(con *c)
{
    le_line *le = &c->le;
    char line[256], add[COMPLETE_MAX], *names;
    const char *profiles[UC_MAX_PROFILES + 1];
    int np = c->conf ? upconf_profiles(c->conf, profiles) : 0;
    int n, from, i, len, common, typed;
    len = copy_latin1(le, 0, le->pos, line, sizeof(line));
    /* the handler's stack is small: the candidates on the heap */
    if (!(names = (char *)AllocVec(COMPLETE_NAMES, MEMF_ANY)))
        return 0;
    n = slash_complete(line, len, profiles, np, names, COMPLETE_NAMES, &from);
    if (n <= 0) {
        FreeVec(names);
        return 0; /* not a command line, or nothing fits: the Shell's completion */
    }
    /* what all candidates agree on past what is typed */
    typed = len - from;
    common = (int)strlen(names);
    {
        int k = 0;
        for (i = 0; i < n; i++) {
            const char *nm = names + k;
            int j = 0;
            while (j < common && nm[j] == names[j])
                j++;
            common = j;
            k += (int)strlen(nm) + 1;
        }
    }
    add[0] = 0;
    if (common > typed)
        copy_str(add, names + typed, common - typed + 1);
    if (n == 1 && (int)strlen(add) < COMPLETE_MAX - 1)
        strcat(add, " ");
    c->menu_n = 0;
    if (n > 1 && menu_buf(c)) {
        int k = 0;
        for (i = 0; i < n; i++)
            k += (int)strlen(names + k) + 1;
        CopyMem(names, c->menu, k);
        c->menu_len = k;
        c->menu_n = n;
        c->menu_i = -1;
        c->menu_start = from;
    }
    FreeVec(names);
    if (add[0])
        type_text(c, add);
    else if (n > 1)
        DisplayBeep(c->w.win ? c->w.win->WScreen : 0); /* several, nothing more in common */
    return 1;
}

static void cooked_key(con *c, const vt_u8 *b, int n, long key, int mods)
{
    if (c->tm && c->tm->m.open) {
        theme_menu_key(c, b, n, key, mods); /* /theme's list has the keys */
        return;
    }
    sel_close(c); /* typing in the console ends a selection, as any key ends KingCON's */
    if (c->kc_cyc && kc_cyc_key(c, b, n, key, mods))
        return;   /* Tab, Shift+Tab, Alt+Tab or Ctrl+S inside a cycle */
    kc_cyc_end(c); /* any other key ends it */
    if (c->medium) {
        /* V47 medium mode (SetMode 2, the 3.2 Shell): TAB, Shift+TAB, Up and
         * Down go to the reader at once as CSI code;length;cursor+1 U and the
         * line -- codes 12, 13, 2, 3 (measured on 3.2.3 with the ROM
         * con-handler: "abcd" with the cursor two left gives 12;4;3U abcd).
         * The Shell answers with ACTION_FORCE (the line it made); it keeps
         * the history, so Up and Down are its. */
        int code = key == VT_KEY_TAB ? ((mods & VT_MOD_SHIFT) ? 13 : 12)
                 : key == VT_KEY_UP && !mods ? 2 : key == VT_KEY_DOWN && !mods ? 3 : 0;
        if (code) {
            char rep[24];
            int n = 0, v[3], i;
            v[0] = code;
            v[1] = c->le.len;
            v[2] = c->le.pos + 1;
            rep[n++] = (char)0x9b;
            for (i = 0; i < 3; i++) {
                char d[8];
                int m = 0, x = v[i];
                do
                    d[m++] = (char)('0' + x % 10);
                while ((x /= 10) != 0);
                while (m)
                    rep[n++] = d[--m];
                rep[n++] = i < 2 ? ';' : 'U';
            }
            in_append(c, (const vt_u8 *)rep, n);
            in_append(c, c->le.buf, c->le.len);
            return;
        }
    }
    if (key == VT_KEY_TAB && !mods && c->le.len && c->le.buf[0] == '/') {
        c->tabs++;
        if (c->tabs >= 2 && c->menu_n > 1) {
            menu_tab(c);
            return;
        }
        if (slash_tab(c))
            return;
        c->tabs = 0; /* a path: the Shell's completion */
    }
    if (c->kingcon && key == VT_KEY_TAB) {
        kc_tab(c, (mods & VT_MOD_SHIFT) ? COMPLETE_DEVICES
                  : (mods & (VTWIN_MOD_ALTKEY | VT_MOD_ALT)) ? COMPLETE_COMMANDS : COMPLETE_FILES);
        return;
    }
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
        char line[256], *ans = 0;
        int l = copy_latin1(&c->le, 0, c->le.len, line, sizeof(line));
        history_save(c, c->le.buf, c->le.len);
        /* the answer (the help is long) on the heap: the stack is small, and
         * a static would be every window's (one code, many processes) */
        if (l > 1 && line[0] == '/' && line[1] >= 'a' && line[1] <= 'z' &&
            (ans = (char *)AllocVec(4096, MEMF_ANY)) != 0 && slash_run(c, line, l, ans, 4096, 1)) {
            /* UP-Term's: its answer on screen, and an empty line for the
             * reader -- a shell shows a fresh prompt */
            le_reset(&c->le);
            if (ans[0]) {
                int i, k = 0;
                char crlf[2];
                crlf[0] = '\r';
                crlf[1] = '\n';
                for (i = 0; ans[i]; i++)
                    if (ans[i] == '\n') {
                        output(c, (const vt_u8 *)ans + k, i - k);
                        output(c, (const vt_u8 *)crlf, 2);
                        k = i + 1;
                    }
            }
            FreeVec(ans);
            if (!c->line_held)
                in_append(c, (const vt_u8 *)"\n", 1); /* else when /theme's list closes */
            c->checked[0] = 0;
            return;
        }
        if (ans)
            FreeVec(ans);
        in_append(c, c->le.buf, c->le.len);
        le_reset(&c->le);
        c->checked[0] = 0;
        return;
    }
    check_command(c);
    command_clock(c); /* a word being typed: its colour when the keys rest */
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
        to_tty(c, out, n);
        return;
    }
    /* Break keys: Ctrl-C..F signal the opener in cooked mode (and Amiga raw
     * mode); an xterm window in raw mode sends the byte only, as a Unix tty
     * without ISIG does. ^\ and ^Z are the CTRL_E and CTRL_F breaks too, as
     * in termios mode (VQUIT, VSUSP): a shell running a job that never set
     * termios still hears the suspend key (vsh S8). */
    /* KingCON: Ctrl+D on a line with text lists the word's directory (on
     * an empty line it stays the break) */
    if (c->kingcon && !c->raw && n == 1 && !key && out[0] == 0x04 && c->le.len > 0) {
        sel_close(c);
        kc_tab(c, -1);
        service_reads(c);
        return;
    }
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

/* ---- KingCON completion ------------------------------------------------------------ */

/* completion = kingcon (thoughts/shared/research/2026-10-02_kingcon-completion.md,
 * after KingCON by David Larsson; no code of it is used). Tab completes file
 * names wherever the word is (a word that finds none tries device names),
 * Shift+Tab devices, volumes and assigns, Alt+Tab commands, Ctrl+D on a line
 * with text lists the word's directory. One match goes in with its suffix;
 * several open a "Select ..." window: Tab / Shift+Tab / cursor keys move
 * (wrapping), Return or a double-click takes the name, Escape or Cancel
 * leaves the line as it was. The scan is complete.c's, in its worker. */

/* mode: enum complete_mode, or -1 for Ctrl+D's listing */
static void kc_tab(con *c, int mode)
{
    le_line *le = &c->le;
    long n = 0;
    const char *extra = 0;
    int a, q;
    if (c->comp_busy || !ensure_worker(c, WORK_COMP))
        return;
    a = le_kc_word(le, &q);
    c->kc_start = a;
    c->kc_quote = q;
    c->kc_list = mode < 0;
    copy_latin1(le, a, le->pos, c->comp->word, COMPLETE_MAX);
    if (c->kc_list) {
        /* the directory the word names: its contents */
        int l = (int)strlen(c->comp->word);
        if (l && c->comp->word[l - 1] != '/' && c->comp->word[l - 1] != ':' && l < COMPLETE_MAX - 1)
            strcpy(c->comp->word + l, "/");
        mode = COMPLETE_FILES;
    }
    if (mode == COMPLETE_COMMANDS)
        extra = shell_words(c, VTCON_WORDS_COMMANDS, &n);
    /* Tab on an empty word, window style: KingCON's file requester */
    if (mode == COMPLETE_FILES && !c->kc_list && a == le->pos && (c->kc_style & LE_KC_WINDOW) &&
        c->w.win) {
        mode = COMPLETE_ASL;
        c->comp->screen = c->w.win->WScreen;
    }
    c->comp->mode = mode;
    c->comp->kingcon = 1;
    c->comp->show_info = c->kc_info;
    c->comp->no_cache = !c->kc_cache;
    c->comp->cold = c->next_cold;
    c->next_cold = 0;
    c->comp->extra_len = 0;
    if (extra && n > 0 && n <= COMPLETE_EXTRA) {
        CopyMem((APTR)extra, c->comp->extra, n);
        c->comp->extra_len = n;
    }
    c->comp_edits = c->edits;
    if (complete_start(c->comp, c->comp_port, opener(c)))
        c->comp_busy = 1;
}

/* One entry (Latin-1, with its suffix) into the line, KingCON's way. */
static void kc_put(con *c, const char *name)
{
    unsigned char enc[COMPLETE_MAX * 2];
    int n = 0;
    for (; *name && n < (int)sizeof(enc) - 4; name++)
        n += vt_encode_key(c->w.t, (unsigned char)*name, 0, enc + n);
    le_kc_insert(&c->le, c->kc_start, c->kc_quote, enc, n);
    check_command(c); /* a completed first word gets its colour */
}

static int sel_open(con *c, const char *names, long len, int count, int mode);

static void kc_beep(con *c)
{
    if (!(c->kc_style & LE_KC_SILENT))
        DisplayBeep(c->w.win ? c->w.win->WScreen : 0);
}

static void kc_cyc_end(con *c)
{
    if (c->kc_cyc_names)
        FreeVec(c->kc_cyc_names);
    c->kc_cyc_names = 0;
    c->kc_cyc = c->kc_cyc_window = 0;
}

/* Keep the entries and the line as it is now, for an inline cycle. */
static int kc_cyc_begin(con *c, struct complete_req *q)
{
    kc_cyc_end(c);
    if (!c->kc_snap && !(c->kc_snap = (unsigned char *)AllocVec(LE_MAX, MEMF_ANY)))
        return 0;
    if (!(c->kc_cyc_names = (char *)AllocVec(q->names_len + 1, MEMF_ANY)))
        return 0;
    CopyMem(q->names, c->kc_cyc_names, q->names_len);
    c->kc_cyc_names[q->names_len] = 0;
    c->kc_cyc_len = q->names_len;
    c->kc_cyc_n = q->matches;
    c->kc_cyc_mode = q->mode;
    CopyMem(c->le.buf, c->kc_snap, c->le.pos);
    c->kc_snap_pos = c->le.pos;
    c->kc_cyc_i = -1;
    c->kc_cyc = 1;
    return 1;
}

/* Entry i of the cycle in place of the word (both ends wrap). */
static void kc_cyc_put(con *c, int i)
{
    unsigned char enc[COMPLETE_MAX * 2];
    const char *name = c->kc_cyc_names;
    int k, n = 0;
    c->kc_cyc_i = i = (i % c->kc_cyc_n + c->kc_cyc_n) % c->kc_cyc_n;
    for (k = 0; k < i; k++)
        name += strlen(name) + 1;
    for (; *name && n < (int)sizeof(enc) - 4; name++)
        n += vt_encode_key(c->w.t, (unsigned char)*name, 0, enc + n);
    le_kc_redo(&c->le, c->kc_snap, c->kc_snap_pos, c->kc_start, c->kc_quote, enc, n);
    c->edits++;
    c->comp_edits = c->edits; /* the window, if one opens now, is for this line */
    check_command(c);
}

/* A key while a cycle runs: Tab next, Shift+Tab previous, Alt+Tab the
 * current again, Ctrl+S (or Tab when C armed the window) the window.
 * 0: not one of them (the cycle ends). */
static int kc_cyc_key(con *c, const vt_u8 *b, int n, long key, int mods)
{
    if ((key == VT_KEY_TAB && c->kc_cyc_window) || (!key && n == 1 && b[0] == 0x13)) {
        int ok = sel_open(c, c->kc_cyc_names, c->kc_cyc_len, c->kc_cyc_n, c->kc_cyc_mode);
        kc_cyc_end(c);
        if (!ok)
            kc_beep(c);
        return 1;
    }
    if (key != VT_KEY_TAB)
        return 0;
    if (mods & (VTWIN_MOD_ALTKEY | VT_MOD_ALT)) {
        if (c->kc_cyc_i >= 0)
            kc_cyc_put(c, c->kc_cyc_i);
        return 1;
    }
    if (mods & VT_MOD_SHIFT)
        kc_cyc_put(c, c->kc_cyc_i < 0 ? c->kc_cyc_n - 1 : c->kc_cyc_i - 1);
    else
        kc_cyc_put(c, c->kc_cyc_i + 1);
    return 1;
}

/* The Complete menu's Filename / Command / Device: as the keys, in a
 * cooked line (a raw stream has no line to complete). */
static void kc_menu(con *c, int mode)
{
    if (c->raw || tty_active(c))
        return;
    sel_close(c);
    kc_cyc_end(c);
    kc_tab(c, mode);
}

/* The typed part the names were matched against: after the word's last
 * '/' or ':' (a device word: all of it). */
static int kc_typed_len(const struct complete_req *q)
{
    const char *w = q->word, *p;
    if (q->mode == COMPLETE_DEVICES)
        return (int)strlen(w);
    for (p = w; *p; p++)
        if (*p == '/' || *p == ':')
            w = p + 1;
    return (int)strlen(w);
}

static void kc_finish(con *c, struct complete_req *q)
{
    int style = c->kc_style;
    if (c->kc_list) {
        /* Ctrl+D: the names under the line, then the prompt and the line */
        if (q->matches)
            le_kc_show_list(&c->le, q->names, q->names_len);
        else
            kc_beep(c);
        return;
    }
    if (q->mode == COMPLETE_ASL) {
        if (q->matches)
            kc_put(c, q->add);
        return; /* cancelled: nothing, as KingCON */
    }
    if (!q->matches) {
        kc_beep(c);
        return;
    }
    if (q->matches == 1) {
        kc_put(c, q->names);
        return;
    }
    if ((style & LE_KC_COMMON) && (int)strlen(q->common) > kc_typed_len(q)) {
        /* C: the part all names share first; W or B then arm the cycle */
        int armed = (style & (LE_KC_WINDOW | LE_KC_CYCLE)) && kc_cyc_begin(c, q);
        if (armed)
            c->kc_cyc_window = (style & LE_KC_WINDOW) != 0;
        kc_put(c, q->common);
        if (armed) {
            c->edits++;
            c->comp_edits = c->edits;
        }
        return;
    }
    if (style & LE_KC_WINDOW) {
        if (!sel_open(c, q->names, q->names_len, q->matches, q->mode))
            kc_beep(c);
        return;
    }
    if (style & LE_KC_LIST)
        le_kc_show_list(&c->le, q->names, q->names_len); /* L: the list, before a cycle's first step */
    if ((style & LE_KC_CYCLE) && kc_cyc_begin(c, q))
        kc_cyc_put(c, 0);
}

#define SEL_ROWS 12              /* names the list shows at once */
#define SEL_W    300             /* its width in pixels (KingCON's window is about this) */
#define SEL_PAD  6

/* Closed the find prompt's way: the port is ours, not Intuition's. */
static void sel_close(con *c)
{
    if (c->sel_win) {
        struct Window *w = c->sel_win;
        struct Node *n, *next;
        Forbid();
        for (n = c->sel_port->mp_MsgList.lh_Head; (next = n->ln_Succ) != 0; n = next)
            if (((struct IntuiMessage *)n)->IDCMPWindow == w) {
                Remove(n);
                ReplyMsg((struct Message *)n);
            }
        w->UserPort = 0;
        ModifyIDCMP(w, 0);
        Permit();
        CloseWindow(w);
        c->sel_win = 0;
    }
    if (c->sel_glist) {
        FreeGadgets(c->sel_glist);
        c->sel_glist = 0;
        c->sel_lv = 0;
    }
    if (c->sel_vi) {
        FreeVisualInfo(c->sel_vi);
        c->sel_vi = 0;
    }
    if (c->sel_nodes) {
        FreeVec(c->sel_nodes);
        c->sel_nodes = 0;
    }
    if (c->sel_names) {
        FreeVec(c->sel_names);
        c->sel_names = 0;
    }
    if (c->sel_port) {
        DeleteMsgPort(c->sel_port);
        c->sel_port = 0;
    }
    c->sel_n = 0;
}

static void sel_move(con *c, int to)
{
    if (!c->sel_n)
        return;
    c->sel_i = (to % c->sel_n + c->sel_n) % c->sel_n; /* both ends wrap */
    GT_SetGadgetAttrs(c->sel_lv, c->sel_win, 0, GTLV_Selected, (ULONG)c->sel_i,
                      GTLV_MakeVisible, (ULONG)c->sel_i, TAG_DONE);
}

/* The selected entry into the line, and the window closed. */
static void sel_take(con *c)
{
    int i, k = 0;
    char name[COMPLETE_MAX];
    for (i = 0; i < c->sel_i; i++)
        k += (int)strlen(c->sel_names + k) + 1;
    strncpy(name, c->sel_names + k, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    sel_close(c);
    if (c->edits == c->comp_edits && c->w.t)
        kc_put(c, name);
}

static int sel_open(con *c, const char *names, long len, int count, int mode)
{
    struct NewGadget ng;
    struct Gadget *g, *str;
    struct Screen *scr;
    struct TextAttr *ta;
    struct TagItem tags[11];
    LONG fh, bar, lvh, ww, wh, left, top;
    int i, k, t = 0;
    const char *title = mode == COMPLETE_DEVICES ? "Select device"
                      : mode == COMPLETE_COMMANDS ? "Select command" : "Select filename";
    if (!c->w.win || !GadToolsBase)
        return 0;
    sel_close(c);
    scr = c->w.win->WScreen;
    ta = scr->Font;
    fh = ta->ta_YSize;
    bar = scr->WBorTop + fh + 1;
    c->sel_names = (char *)AllocVec(len + 1, MEMF_ANY);
    c->sel_nodes = (struct Node *)AllocVec(sizeof(struct Node) * count, MEMF_CLEAR);
    c->sel_port = CreateMsgPort();
    c->sel_vi = GetVisualInfoA(scr, 0);
    if (!c->sel_names || !c->sel_nodes || !c->sel_port || !c->sel_vi) {
        sel_close(c);
        return 0;
    }
    CopyMem((APTR)names, c->sel_names, len);
    c->sel_names[len] = 0;
    /* NewList (amiga.lib's), by hand */
    c->sel_list.lh_Head = (struct Node *)&c->sel_list.lh_Tail;
    c->sel_list.lh_Tail = 0;
    c->sel_list.lh_TailPred = (struct Node *)&c->sel_list.lh_Head;
    for (i = 0, k = 0; i < count && k < len; i++) {
        char *e = c->sel_names + k;
        int l = (int)strlen(e);
        k += l + 1;
        c->sel_nodes[i].ln_Name = e;
        AddTail(&c->sel_list, &c->sel_nodes[i]);
    }
    c->sel_n = i;
    c->sel_i = 0;
    c->sel_click = -1;
    /* the list with the selected name under it (GadTools puts the
     * ShowSelected field inside the list's height), then OK and Cancel */
    lvh = SEL_ROWS * fh + 4 + fh + 6;
    ww = scr->WBorLeft + SEL_PAD + SEL_W + SEL_PAD + scr->WBorRight;
    wh = bar + SEL_PAD + lvh + SEL_PAD + fh + 6 + SEL_PAD + scr->WBorBottom;
    if (ww > scr->Width || wh > scr->Height) {
        lvh = 6 * fh + 4 + fh + 6; /* a small screen: fewer rows */
        wh = bar + SEL_PAD + lvh + SEL_PAD + fh + 6 + SEL_PAD + scr->WBorBottom;
        if (ww > scr->Width || wh > scr->Height) {
            sel_close(c);
            return 0;
        }
    }
    g = CreateContext(&c->sel_glist);
    memset(&ng, 0, sizeof(ng));
    ng.ng_VisualInfo = c->sel_vi;
    ng.ng_TextAttr = ta;
    /* the string that shows the selection: made first, the list points at
     * it and places it */
    ng.ng_LeftEdge = scr->WBorLeft + SEL_PAD;
    ng.ng_TopEdge = bar + SEL_PAD;
    ng.ng_Width = SEL_W;
    ng.ng_Height = fh + 6;
    str = g = CreateGadget(STRING_KIND, g, &ng, GTST_MaxChars, COMPLETE_MAX, TAG_DONE);
    ng.ng_TopEdge = bar + SEL_PAD;
    ng.ng_Height = lvh;
    ng.ng_GadgetID = 1;
    c->sel_lv = g = CreateGadget(LISTVIEW_KIND, g, &ng, GTLV_Labels, (ULONG)&c->sel_list,
                                 GTLV_ShowSelected, (ULONG)str, GTLV_Selected, 0,
                                 TAG_DONE);
    ng.ng_TopEdge = bar + SEL_PAD + lvh + SEL_PAD;
    ng.ng_Width = 80;
    ng.ng_Height = fh + 6;
    ng.ng_GadgetText = (UBYTE *)"OK";
    ng.ng_GadgetID = 2;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    ng.ng_LeftEdge = scr->WBorLeft + SEL_PAD + SEL_W - 80;
    ng.ng_GadgetText = (UBYTE *)"Cancel";
    ng.ng_GadgetID = 3;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    if (!g) {
        sel_close(c);
        return 0;
    }
    /* over the console window, centred */
    left = c->w.win->LeftEdge + (c->w.win->Width - ww) / 2;
    top = c->w.win->TopEdge + (c->w.win->Height - wh) / 2;
    if (left + ww > scr->Width)
        left = scr->Width - ww;
    if (top + wh > scr->Height)
        top = scr->Height - wh;
    if (left < 0)
        left = 0;
    if (top < 0)
        top = 0;
    tags[t].ti_Tag = WA_Left;      tags[t++].ti_Data = (ULONG)left;
    tags[t].ti_Tag = WA_Top;       tags[t++].ti_Data = (ULONG)top;
    tags[t].ti_Tag = WA_Width;     tags[t++].ti_Data = (ULONG)ww;
    tags[t].ti_Tag = WA_Height;    tags[t++].ti_Data = (ULONG)wh;
    tags[t].ti_Tag = WA_Title;     tags[t++].ti_Data = (ULONG)title;
    tags[t].ti_Tag = WA_Flags;     tags[t++].ti_Data = WFLG_DRAGBAR | WFLG_DEPTHGADGET |
                                                   WFLG_CLOSEGADGET | WFLG_ACTIVATE;
    tags[t].ti_Tag = WA_IDCMP;     tags[t++].ti_Data = 0; /* the port is ours: below */
    tags[t].ti_Tag = WA_PubScreen; tags[t++].ti_Data = (ULONG)scr;
    tags[t].ti_Tag = WA_Gadgets;   tags[t++].ti_Data = (ULONG)c->sel_glist;
    tags[t].ti_Tag = WA_NewLookMenus; tags[t++].ti_Data = TRUE;
    tags[t].ti_Tag = TAG_DONE;     tags[t].ti_Data = 0;
    c->sel_win = OpenWindowTagList(0, tags);
    if (!c->sel_win) {
        sel_close(c);
        return 0;
    }
    c->sel_win->UserPort = c->sel_port;
    if (!ModifyIDCMP(c->sel_win, LISTVIEWIDCMP | BUTTONIDCMP | IDCMP_VANILLAKEY | IDCMP_RAWKEY |
                     IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW)) {
        sel_close(c);
        return 0;
    }
    GT_RefreshWindow(c->sel_win, 0);
    return 1;
}

/* The selection window's events: GadTools' own first (GT_GetIMsg). */
static void sel_idcmp(con *c)
{
    struct IntuiMessage *im;
    while (c->sel_port && (im = GT_GetIMsg(c->sel_port))) {
        ULONG cls = im->Class;
        UWORD code = im->Code, qual = im->Qualifier;
        ULONG secs = im->Seconds, mics = im->Micros;
        struct Gadget *gad = (struct Gadget *)im->IAddress;
        int jump = (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | IEQUALIFIER_LALT |
                            IEQUALIFIER_RALT)) ? SEL_ROWS : 1;
        if (cls == IDCMP_REFRESHWINDOW) {
            GT_BeginRefresh(c->sel_win);
            GT_EndRefresh(c->sel_win, TRUE);
        }
        GT_ReplyIMsg(im);
        switch (cls) {
        case IDCMP_VANILLAKEY:
            if (code == 0x0D) {
                sel_take(c);
                return;
            }
            if (code == 0x1B) {
                sel_close(c);
                return;
            }
            if (code == 0x09)
                sel_move(c, c->sel_i + ((qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? -1 : 1));
            break;
        case IDCMP_RAWKEY:
            /* Shift+Tab has no character in the keymap: it comes raw */
            if (code == 0x42)
                sel_move(c, c->sel_i + ((qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? -1 : 1));
            else if (code == 0x4C)
                sel_move(c, c->sel_i - jump);
            else if (code == 0x4D)
                sel_move(c, c->sel_i + jump);
            else if (code == 0x43) { /* Enter on the keypad */
                sel_take(c);
                return;
            }
            break;
        case IDCMP_GADGETUP:
            if (gad->GadgetID == 1) {
                /* a click: a second one on the same name in time takes it */
                int dbl = (int)code == c->sel_click && DoubleClick(c->sel_secs, c->sel_mics, secs, mics);
                c->sel_i = code;
                c->sel_click = code;
                c->sel_secs = secs;
                c->sel_mics = mics;
                if (dbl) {
                    sel_take(c);
                    return;
                }
            } else if (gad->GadgetID == 2) {
                sel_take(c);
                return;
            } else if (gad->GadgetID == 3) {
                sel_close(c);
                return;
            }
            break;
        case IDCMP_CLOSEWINDOW:
            sel_close(c);
            return;
        }
    }
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
 * string, so the gadget is an Intuition string gadget (GTYP_STRGADGET with
 * its StringInfo) filled field by field. Intuition maintains the text and
 * reports Return or Enter as GADGETUP (GACT_RELVERIFY); Escape, or Return
 * while the gadget is not active, arrive as VANILLAKEY, and the close
 * gadget cancels too.
 *
 * The window's IDCMP goes to find_port, a port the window did not create:
 * opened with no IDCMP, given the port, then ModifyIDCMP; closed the RKM's
 * CloseWindowSafely way (strip its messages, detach the port, no IDCMP,
 * then CloseWindow) so Intuition never frees our port as its own nor
 * leaves a message pointing at a closed window. */

#define FIND_GAD_W  340
#define FIND_PAD    4             /* inner margin around the gadget */

static void find_close(con *c)
{
    if (c->find_win) {
        struct Window *w = c->find_win;
        struct Node *n, *next;
        Forbid();
        /* whatever the window still has queued must not outlive it */
        for (n = c->find_port->mp_MsgList.lh_Head; (next = n->ln_Succ) != 0; n = next)
            if (((struct IntuiMessage *)n)->IDCMPWindow == w) {
                Remove(n);
                ReplyMsg((struct Message *)n);
            }
        w->UserPort = 0;          /* not Intuition's to free */
        ModifyIDCMP(w, 0);        /* and no more messages for it */
        Permit();
        CloseWindow(w);           /* the gadget is a plain struct, not allocated */
        c->find_win = 0;
        c->find_gad = 0;
    }
    if (c->find_port) {
        DeleteMsgPort(c->find_port); /* empty: its only window's messages were stripped */
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
    struct TagItem tags[10];
    struct Gadget *g;
    struct Screen *scr;
    LONG bar, fh, gw, ww, wh, left, top;
    int n = 0;
    if (!c->w.win)
        return 0;
    if (c->find_win) {
        WindowToFront(c->find_win); /* already up: keep typing into the same query */
        ActivateWindow(c->find_win);
        return 1;
    }
    scr = c->w.win->WScreen;
    /* the frame on this screen: title bar, borders, and the font the gadget
     * is drawn in (the window's, which is the screen's) */
    bar = scr->WBorTop + scr->Font->ta_YSize + 1;
    fh = scr->Font->ta_YSize;
    gw = FIND_GAD_W;
    ww = scr->WBorLeft + FIND_PAD + gw + FIND_PAD + scr->WBorRight;
    wh = bar + FIND_PAD + fh + FIND_PAD + scr->WBorBottom;
    /* never wider or taller than the screen: the field gives up the width */
    if (ww > scr->Width) {
        gw -= ww - scr->Width;
        ww = scr->Width;
    }
    if (wh > scr->Height || gw < 16)
        return 0; /* no room for a field on this screen */
    /* beside the console window, moved back on screen if it hangs over */
    left = c->w.win->LeftEdge + c->w.win->Width + 2;
    top = c->w.win->TopEdge + c->w.win->Height + 2;
    if (left + ww > scr->Width)
        left = scr->Width - ww;
    if (top + wh > scr->Height)
        top = scr->Height - wh;
    if (left < 0)
        left = 0;
    if (top < 0)
        top = 0;
    if (!c->find_port)
        c->find_port = CreateMsgPort();
    if (!c->find_port)
        return 0;
    c->find_buf[0] = 0;
    g = &c->find_gadget;
    g->NextGadget = 0;
    g->LeftEdge = (WORD)(scr->WBorLeft + FIND_PAD);
    g->TopEdge = (WORD)(bar + FIND_PAD);
    g->Width = (WORD)gw;
    g->Height = (WORD)fh;
    g->Flags = 0;
    g->Activation = GACT_RELVERIFY; /* Return / Enter end the edit with GADGETUP */
    g->GadgetType = GTYP_STRGADGET;
    g->GadgetRender = 0;
    g->SelectRender = 0;
    g->GadgetText = 0;
    g->MutualExclude = 0;
    g->SpecialInfo = (APTR)&c->find_si;
    g->GadgetID = 0;
    g->UserData = 0;
    c->find_si.Buffer = (UBYTE *)c->find_buf;
    c->find_si.UndoBuffer = 0;
    c->find_si.MaxChars = (WORD)sizeof(c->find_buf);
    c->find_si.BufferPos = 0;
    c->find_si.DispPos = 0;
    c->find_si.Extension = 0;
    tags[n].ti_Tag = WA_Left;       tags[n++].ti_Data = (ULONG)left;
    tags[n].ti_Tag = WA_Top;        tags[n++].ti_Data = (ULONG)top;
    tags[n].ti_Tag = WA_Width;      tags[n++].ti_Data = (ULONG)ww;
    tags[n].ti_Tag = WA_Height;     tags[n++].ti_Data = (ULONG)wh;
    tags[n].ti_Tag = WA_Title;      tags[n++].ti_Data = (ULONG)"Find";
    tags[n].ti_Tag = WA_Flags;      tags[n++].ti_Data = WFLG_DRAGBAR | WFLG_DEPTHGADGET |
                                                    WFLG_CLOSEGADGET | WFLG_ACTIVATE;
    tags[n].ti_Tag = WA_IDCMP;      tags[n++].ti_Data = 0; /* the port is ours: below */
    tags[n].ti_Tag = WA_PubScreen;  tags[n++].ti_Data = (ULONG)scr;
    tags[n].ti_Tag = WA_Gadgets;    tags[n++].ti_Data = (ULONG)g;
    tags[n].ti_Tag = TAG_DONE;      tags[n].ti_Data = 0;
    c->find_win = OpenWindowTagList(0, tags);
    if (!c->find_win) {
        find_close(c);
        return 0;
    }
    c->find_win->UserPort = c->find_port;
    if (!ModifyIDCMP(c->find_win, IDCMP_GADGETUP | IDCMP_VANILLAKEY | IDCMP_CLOSEWINDOW)) {
        find_close(c);
        return 0;
    }
    c->find_gad = g; /* in the window's chain: RefreshGadgets and events use it */
    RefreshGadgets(c->find_gad, c->find_win, 0);
    ActivateGadget(c->find_gad, c->find_win, 0); /* type straight away */
    return 1;
}

/* The prompt's events, drained whenever the console is idle. Return or
 * Enter runs the search (find_run may close the prompt); Escape and the
 * close gadget close it. Everything else is Intuition's: it edits the
 * string and redraws the gadget itself. */
static void find_idcmp(con *c)
{
    struct IntuiMessage *im;
    while (c->find_port && (im = (struct IntuiMessage *)GetMsg(c->find_port))) {
        ULONG cls = im->Class;
        UWORD code = im->Code;
        ReplyMsg((struct Message *)im);
        if (cls == IDCMP_GADGETUP || (cls == IDCMP_VANILLAKEY && code == 0x0D)) {
            find_run(c); /* may close the prompt and its port */
            return;      /* one action per pass, and the port may be gone now */
        }
        if (cls == IDCMP_CLOSEWINDOW || (cls == IDCMP_VANILLAKEY && code == 0x1B)) {
            find_close(c);
            return;
        }
    }
}

/* The close gadget, or the menu's Close window. AUTO: the gadget shuts the
 * window only, and the next read or write opens it again -- unless a read
 * waits (then it is EOF as without AUTO) or DISK_INFO gave the window out
 * (ROM 40.x and V47, tools/rig/autoprobe_rig.py) */
static void close_gadget(con *c)
{
    if (c->auto_open && !c->auto_held && !c->nreads && c->w.win != c->foreign) {
        c->auto_shut = 1;
        return;
    }
    c->closing = 1;
    if (!c->raw)
        c->eof = 1;
    else if (!vtwin_raw_report(&c->w, 11)) /* IECLASS_CLOSEWINDOW, if asked */
        send_break(c, SIGBREAKF_CTRL_C);
}

/* One window event for this process's terminal: from its own window's
 * IDCMP, or forwarded by the host when this process is a tab (wheel: 1 up,
 * -1 down, for IDCMP_EXTENDEDMOUSE). */
static void dispatch(con *c, ULONG cls, UWORD code, UWORD qual, ULONG prev, ULONG secs, ULONG mics,
                     WORD mx, WORD my, int wheel)
{
    switch (cls) {
    case IDCMP_RAWKEY:
        /* Right Amiga F: the find prompt, opened here rather than in vtwin
         * because the prompt is a window of ours */
        if ((qual & IEQUALIFIER_RCOMMAND) && code == 0x46) {
            find_open(c);
            break;
        }
        vtwin_key(&c->w, code, qual, prev, secs, mics);
        break;
    case IDCMP_NEWSIZE:
        vtwin_resize(&c->w);
        break;
    case IDCMP_REFRESHWINDOW:
        if (c->is_tab)
            vtwin_show(&c->w, 1); /* the host did Begin/EndRefresh: draw it all */
        else
            vtwin_refresh(&c->w);
        break;
    case IDCMP_CLOSEWINDOW:
        close_gadget(c);
        break;
    case IDCMP_MENUPICK:
        menu_pick(c, code);
        break;
    case IDCMP_MOUSEBUTTONS:
    case IDCMP_MOUSEMOVE:
        vtwin_mouse(&c->w, cls == IDCMP_MOUSEMOVE, code, qual, mx, my, secs, mics);
        break;
    case IDCMP_EXTENDEDMOUSE:
        if (wheel)
            vtwin_wheel(&c->w, wheel > 0, qual, mx, my);
        break;
    case IDCMP_ACTIVEWINDOW:
    case IDCMP_INACTIVEWINDOW:
        vtwin_focus(&c->w, cls == IDCMP_ACTIVEWINDOW); /* ?1004: CSI I / CSI O */
        break;
    case IDCMP_IDCMPUPDATE:
        /* the scroll bar (sbar_route): code is the gadget, prev the knob's top */
        if (code == SBAR_GID_PROP) {
            if (prev != (ULONG)~0)
                vtwin_knob_moved(&c->w, prev);
        } else
            vtwin_knob_lines(&c->w, code == SBAR_GID_UP ? 1 : -1);
        break;
    default:
        break;
    }
}

static int tab_route(con *c, struct IntuiMessage *im, int wheel);
static void sbar_route(con *c, ULONG id, ULONG top);

static void idcmp(con *c)
{
    struct IntuiMessage *im;
    ULONG knob_top = (ULONG)~0; /* the knob's last position in this batch */
    if (c->own_win && c->w.win && !IsListEmpty(&c->own_win->UserPort->mp_MsgList))
        vtwin_render(&c->w); /* resize, refresh, selection: on the current screen */
    while (c->own_win && (im = (struct IntuiMessage *)GetMsg(c->own_win->UserPort))) {
        ULONG cls = im->Class;
        int wheel = 0;
        if (cls == IDCMP_EXTENDEDMOUSE && im->Code == IMSGCODE_INTUIWHEELDATA && im->IAddress) {
            /* the mouse wheel (OS 3.9+): the IntuiWheelData behind IAddress.
             * WheelX is the main wheel; forward (up) is the negative delta.
             * Anything else extended-mouse reports is not ours. */
            struct IntuiWheelData *wd = (struct IntuiWheelData *)im->IAddress;
            if (wd->Version == INTUIWHEELDATA_VERSION)
                wheel = wd->WheelX < 0 ? 1 : -1;
        }
        if (cls == IDCMP_IDCMPUPDATE) {
            /* the scroll bar: read now, the tag list goes with the reply */
            ULONG id, top;
            if (sbar_gad_event(im, &id, &top)) {
                if (id == SBAR_GID_PROP)
                    knob_top = top; /* a drag sends many: the view moves once, to the last */
                else
                    sbar_route(c, id, top);
            }
            ReplyMsg((struct Message *)im);
            continue;
        }
        /* with tabs the host takes its own keys and the bar, and the
         * active tab gets the rest */
        if (!tab_route(c, im, wheel))
            dispatch(c, cls, im->Code, im->Qualifier,
                     (cls == IDCMP_RAWKEY && im->IAddress) ? *(ULONG *)im->IAddress : 0,
                     im->Seconds, im->Micros, im->MouseX, im->MouseY, wheel);
        ReplyMsg((struct Message *)im);
    }
    if (knob_top != (ULONG)~0 && c->own_win)
        sbar_route(c, SBAR_GID_PROP, knob_top);
    if (c->auto_shut) {
        c->auto_shut = 0;
        close_window(c);
    }
    screen_pending(c); /* a pick or a /screen: the messages are replied now */
}

/* ---- tabs ------------------------------------------------------------------- */

/* plans/2026-10-03-tabs.md. Every tab is a handler process of its own (its
 * shell, engine, profile, menus); the host -- the process that opened the
 * window -- owns the window and its IDCMP, draws the bar and routes the
 * events to the active tab. Messages go both ways; whoever sends one frees
 * it when it comes back (tab_replies), REGISTER and UNREGISTER are waited
 * for. */
enum { TM_REGISTER = 1, TM_UNREGISTER, TM_TITLE, TM_COMMAND, TM_EVENT, TM_SHOW, TM_HIDE, TM_INSET,
       TM_KNOB };

struct tab_msg {
    struct Message msg;
    int type;
    con *from;
    struct Window *win;      /* REGISTER's answer: the window */
    WORD inset;              /* REGISTER's answer, INSET: the bar's height */
    int ok, what;            /* REGISTER's answer; COMMAND: MENU_TAB_* */
    ULONG cls;               /* EVENT: the IDCMP class and its fields */
    UWORD code, qual;
    ULONG prev, secs, mics;
    WORD mx, my;
    int wheel;
    sbar_knob knob;          /* KNOB: the shown tab's scroll bar knob */
};

static struct tab_msg *tab_msg(con *c, int type)
{
    struct tab_msg *t;
    if (!c->tab_reply && !(c->tab_reply = CreateMsgPort()))
        return 0;
    t = (struct tab_msg *)AllocVec(sizeof(*t), MEMF_PUBLIC | MEMF_CLEAR);
    if (!t)
        return 0;
    t->msg.mn_ReplyPort = c->tab_reply;
    t->msg.mn_Length = sizeof(*t);
    t->type = type;
    t->from = c;
    return t;
}

static void tab_show_active(con *c);

static void tab_replies(con *c)
{
    struct Message *m;
    while (c->tab_reply && (m = GetMsg(c->tab_reply)) != 0) {
        if (m == c->hiding) {
            c->hiding = 0; /* the old tab has stopped drawing: the window is free */
            tab_show_active(c);
        }
        FreeVec(m);
    }
}

/* host -> a tab, answered later (freed by tab_replies) */
static void tab_post(con *c, con *to, int type, WORD inset, struct IntuiMessage *im, int wheel)
{
    struct tab_msg *t = to->tab_port ? tab_msg(c, type) : 0;
    if (!t)
        return;
    t->inset = inset;
    if (im) {
        t->cls = im->Class;
        t->code = im->Code;
        t->qual = im->Qualifier;
        t->prev = (im->Class == IDCMP_RAWKEY && im->IAddress) ? *(ULONG *)im->IAddress : 0;
        t->secs = im->Seconds;
        t->mics = im->Micros;
        t->mx = im->MouseX;
        t->my = im->MouseY;
        t->wheel = wheel;
    }
    PutMsg(to->tab_port, &t->msg);
}

/* ---- the scroll bar (SB1) ---- */

/* The window's scroll bar shown or not, as the setting says: only on the
 * window this process owns, and only when its right border has room
 * (sizing gadget there; never the borderless full-screen window). */
static void sbar_apply(con *c)
{
    struct Window *win = c->own_win;
    if (!c->sbar_on || !win || c->is_tab || win == c->foreign || !sbar_gad_fits(win)) {
        sbar_gad_close(&c->sbar);
        return;
    }
    if (!c->sbar.win && sbar_gad_open(&c->sbar, win) && c->knob_last_valid)
        sbar_gad_set(&c->sbar, &c->knob_last);
}

/* vtwin's knob changed: on our window's scroll bar, or a tab's to its host
 * (only the shown tab's: a hidden one's vtwin gives none). At most once a
 * frame. */
static void h_knob(void *u, const sbar_knob *k)
{
    con *c = (con *)u;
    if (c->is_tab) {
        struct tab_msg *t = c->host_pub && c->shown ? tab_msg(c, TM_KNOB) : 0;
        if (t) {
            t->knob = *k;
            PutMsg(c->host_pub, &t->msg);
        }
        return;
    }
    c->knob_last = *k;
    c->knob_last_valid = 1;
    sbar_gad_set(&c->sbar, k);
}

/* The scroll bar moved: to the terminal shown (ours, or the active tab's). */
static void sbar_route(con *c, ULONG id, ULONG top)
{
    con *to = c->ntabs && c->active >= 0 && c->active < c->ntabs ? c->tab_list[c->active] : c;
    struct tab_msg *t;
    if (to == c) {
        dispatch(c, IDCMP_IDCMPUPDATE, (UWORD)id, 0, top, 0, 0, 0, 0, 0);
        return;
    }
    if (!to->tab_port || !(t = tab_msg(c, TM_EVENT)))
        return;
    t->cls = IDCMP_IDCMPUPDATE;
    t->code = (UWORD)id;
    t->prev = top;
    PutMsg(to->tab_port, &t->msg);
}

static WORD bar_height(con *c)
{
    return c->own_win && c->ntabs >= 2 ? (WORD)(c->own_win->WScreen->RastPort.Font->tf_YSize + 4) : 0;
}

/* the bar: one cell per tab, its number and title, the active one in the
 * screen's fill pens (in a RastPort of its own: the terminal's pens and
 * font stay the terminal's) */
static void bar_draw(con *c)
{
    struct Window *win = c->own_win;
    struct RastPort rp;
    struct DrawInfo *di;
    struct TextFont *tf;
    struct TextExtent te;
    WORD x0, y0, w, h, i;
    UWORD *pens;
    if (!win || c->ntabs < 2)
        return;
    rp = *win->RPort;
    tf = win->WScreen->RastPort.Font;
    SetFont(&rp, tf);
    di = GetScreenDrawInfo(win->WScreen);
    if (!di)
        return;
    pens = di->dri_Pens;
    x0 = win->BorderLeft;
    y0 = win->BorderTop;
    w = win->Width - win->BorderLeft - win->BorderRight;
    h = bar_height(c);
    SetDrMd(&rp, JAM1);
    for (i = 0; i < c->ntabs; i++) {
        con *t = c->tab_list[i];
        WORD a = x0 + (WORD)((LONG)w * i / c->ntabs), b = x0 + (WORD)((LONG)w * (i + 1) / c->ntabs) - 1;
        char label[96];
        int k = 0, n;
        label[k++] = (char)('1' + i);
        label[k++] = ' ';
        copy_str(label + k, t->w.title[0] ? t->w.title : "UP-Term", (int)sizeof(label) - k);
        SetAPen(&rp, pens[i == c->active ? FILLPEN : BACKGROUNDPEN]);
        RectFill(&rp, a, y0, b, y0 + h - 2);
        SetAPen(&rp, pens[SHADOWPEN]);
        Move(&rp, b, y0);
        Draw(&rp, b, y0 + h - 2);
        n = (int)TextFit(&rp, (STRPTR)label, (UWORD)strlen(label), &te, 0, 1,
                         (UWORD)(b - a - 6 > 0 ? b - a - 6 : 0), (UWORD)h);
        SetAPen(&rp, pens[i == c->active ? FILLTEXTPEN : TEXTPEN]);
        Move(&rp, a + 3, y0 + 2 + tf->tf_Baseline);
        Text(&rp, (STRPTR)label, (UWORD)n);
    }
    SetAPen(&rp, pens[SHADOWPEN]);
    Move(&rp, x0, y0 + h - 1);
    Draw(&rp, x0 + w - 1, y0 + h - 1);
    FreeScreenDrawInfo(win->WScreen, di);
}

/* the bar came or went: every tab's text area moves */
static void tab_insets(con *c)
{
    WORD h = bar_height(c);
    int i;
    for (i = 0; i < c->ntabs; i++) {
        if (c->tab_list[i] == c) {
            if (c->w.t)
                vtwin_set_inset(&c->w, h);
        } else
            tab_post(c, c->tab_list[i], TM_INSET, h, 0, 0);
    }
    if (!h && c->own_win && c->w.t && !c->host_only)
        vtwin_show(&c->w, 1); /* the bar's pixels are text area again */
}

/* The window's text area has one owner at a time: the active tab draws, no
 * other. Handing it over is a handshake: the old tab hides (takes its
 * cursor away, stops drawing) and answers, and only then is the new one
 * shown. Shown at once, the old tab's last cursor flip or output -- a
 * complement, a row -- landed on the new tab's pixels (owner, 2026-10-03:
 * "the cursor from the other tab leaks into the second tab"). */
static void tab_show_active(con *c)
{
    con *t;
    if (c->hiding || c->active < 0 || c->active >= c->ntabs || !c->own_win)
        return;
    t = c->tab_list[c->active];
    if (t == c) {
        vtwin_show(&c->w, 1);
        if (c->menustrip)
            SetMenuStrip(c->own_win, c->menustrip);
    } else
        tab_post(c, t, TM_SHOW, 0, 0, 0);
}

static void tab_activate(con *c, int i)
{
    con *old;
    if (i < 0 || i >= c->ntabs || !c->own_win)
        return;
    /* while a hand-over is under way the one being left still draws until
     * it answers, and the one it was going to is not shown yet: no new hide */
    old = !c->hiding && c->active < c->ntabs ? c->tab_list[c->active] : 0;
    ClearMenuStrip(c->own_win); /* whoever's strip it was */
    c->active = i;
    if (old && old != c->tab_list[i]) {
        if (old == c)
            vtwin_show(&c->w, 0);
        else if (old->tab_port && (c->hiding = (struct Message *)tab_msg(c, TM_HIDE)) != 0)
            PutMsg(old->tab_port, c->hiding); /* answered: tab_replies shows the new one */
    }
    if (!old || old != c->tab_list[i])
        tab_show_active(c);
    bar_draw(c);
}

static void tab_remove(con *c, con *t)
{
    int i, was;
    for (i = 0; i < c->ntabs && c->tab_list[i] != t; i++)
        ;
    if (i == c->ntabs)
        return;
    was = i == c->active;
    for (; i < c->ntabs - 1; i++)
        c->tab_list[i] = c->tab_list[i + 1];
    c->ntabs--;
    if (c->active >= c->ntabs)
        c->active = c->ntabs - 1;
    if (!c->ntabs)
        return;
    tab_insets(c);
    if (was) {
        c->active = c->active < 0 ? 0 : c->active;
        /* the one that went drew last: the new active one draws all */
        ClearMenuStrip(c->own_win);
        tab_show_active(c); /* the one that left is not drawing: it waits on us */
    }
    bar_draw(c);
}

/* DOS work the handler must not do itself (a NewShell, a command): a
 * worker process gets msg and does it, then frees msg. */
static void start_worker(void (*entry)(void), const char *name, struct Message *msg)
{
    struct Process *p = CreateNewProcTags(NP_Entry, (ULONG)entry, NP_Name, (ULONG)name,
                                          NP_StackSize, 8192, NP_Input, 0, NP_Output, 0, NP_CloseInput, FALSE,
                                          NP_CloseOutput, FALSE, NP_ConsoleTask, 0, TAG_DONE);
    if (p)
        PutMsg(&p->pr_MsgPort, msg);
    else
        FreeVec(msg);
}

/* the message a worker was started with (start_worker) */
static struct Message *worker_msg(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    WaitPort(&me->pr_MsgPort);
    me->pr_WindowPtr = (APTR)-1;
    return GetMsg(&me->pr_MsgPort);
}

static void run_async(const char *cmd)
{
    SystemTags((STRPTR)cmd, SYS_Asynch, TRUE, SYS_Input, Open((STRPTR)"NIL:", MODE_OLDFILE),
               SYS_Output, Open((STRPTR)"NIL:", MODE_NEWFILE), TAG_DONE);
}

/* New tab: a shell on an XCON: stream that names this host. The NewShell
 * is DOS work: a worker process does it. The new tab takes `profile`, and
 * starts in the directory the shell of the tab it came from last reported
 * (OSC 7) when that is on this machine. */
struct tab_spawn_msg {
    struct Message msg;
    char cmd[200];
    char cwd[VT_URI_MAX];        /* the OSC 7 URL, "" none */
    int demo;                    /* Help > Demo tour: UPDemo's tour in the tab, not a shell */
};

/* Help > Demo tour: the tab runs UPDemo's tour and closes when it ends.
 * 1 when the tab is on its way; 0 when C:UPDemo is not there (said in a
 * requester, as Preferences... says it of the editor). */
static int tab_demo_script(struct tab_spawn_msg *m)
{
    static struct EasyStruct missing = {
        sizeof(struct EasyStruct), 0, (UBYTE *)"UP-Term",
        (UBYTE *)"The demo is not installed:\nC:UPDemo was not found.\n\n"
                 "Install UP-Term from its archive to get it.",
        (UBYTE *)"OK"
    };
    BPTR f, lock;
    if (!(lock = Lock((STRPTR)"C:UPDemo", SHARED_LOCK))) {
        EasyRequestArgs(0, &missing, 0, 0);
        return 0;
    }
    UnLock(lock);
    if (!(f = Open((STRPTR)"T:UP-Term-demo", MODE_NEWFILE)))
        return 0;
    FPuts(f, (STRPTR)"FailAt 2147483647\nC:UPDemo TOUR\nEndCLI >NIL:\n");
    Close(f);
    strcat(m->cmd, " FROM T:UP-Term-demo");
    return 1;
}

static void tab_spawner(void)
{
    struct tab_spawn_msg *m = (struct tab_spawn_msg *)worker_msg();
    BPTR f, lock;
    char host[64], dir[256];
    int vsh, cd;
    if (m->demo) {
        if (tab_demo_script(m))
            run_async(m->cmd);
        Forbid();
        FreeVec(m);
        return;
    }
    /* vsh in the tab when there is one (the UP-Term icon's shell), else the
     * AmigaDOS Shell; the FROM script ends the shell with it -- whatever vsh
     * returns: its last command's status (127 for a name not found) failed
     * the script at FailAt 10, and the tab stayed open on the Shell's prompt */
    if ((vsh = (lock = Lock((STRPTR)"C:vsh", SHARED_LOCK)) != 0) != 0)
        UnLock(lock);
    if (GetVar((STRPTR)"HOSTNAME", (STRPTR)host, sizeof(host), 0) <= 0)
        host[0] = 0;
    cd = m->cwd[0] && termurl_cwd_dir(m->cwd, host, dir, sizeof(dir));
    if ((vsh || cd) && (f = Open((STRPTR)"T:UP-Term-tab", MODE_NEWFILE)) != 0) {
        FPuts(f, (STRPTR)"FailAt 2147483647\n");
        if (cd) { /* termurl_cwd_dir refused anything a quote cannot hold */
            FPuts(f, (STRPTR)"CD \"");
            FPuts(f, (STRPTR)dir);
            FPuts(f, (STRPTR)"\"\n");
        }
        if (vsh)
            FPuts(f, (STRPTR)"C:vsh\nEndCLI >NIL:\n");
        Close(f);
        strcat(m->cmd, " FROM T:UP-Term-tab");
    }
    run_async(m->cmd);
    Forbid();
    FreeVec(m);
}

/* OSC 8: a hyperlink Ctrl + clicked. The profile's link-open names the
 * command (%s the URL; OpenURL, from the OpenURL package, by default); a
 * worker runs it. */
struct link_msg {
    struct Message msg;
    char cmd[VT_URI_MAX + 256];
};

static void link_opener(void)
{
    struct link_msg *m = (struct link_msg *)worker_msg();
    run_async(m->cmd);
    Forbid();
    FreeVec(m);
}

static void h_open_link(void *u, const char *uri)
{
    con *c = (con *)u;
    struct link_msg *m = (struct link_msg *)AllocVec(sizeof(*m), MEMF_PUBLIC | MEMF_CLEAR);
    if (!m)
        return;
    if (!termurl_link_command(c->link_open[0] ? c->link_open : "OpenURL %s", uri, m->cmd,
                              sizeof(m->cmd))) {
        FreeVec(m); /* a URL a command line cannot quote is not opened */
        DisplayBeep(c->w.win ? c->w.win->WScreen : 0);
        return;
    }
    start_worker(link_opener, "UP-Term link", &m->msg);
}

/* 1 when the new tab is on its way; demo: the tab plays the tour */
static int tab_spawn(con *c, const char *profile, con *from, int demo)
{
    struct tab_spawn_msg *m;
    if (c->ntabs >= TAB_MAX || !c->own_win || c->foreign)
        return 0;
    if (!c->tab_port) {
        /* the public port tabs find us by */
        char hex[9];
        ULONG v = (ULONG)c;
        int i;
        for (i = 7; i >= 0; i--, v >>= 4)
            hex[i] = "0123456789ABCDEF"[v & 15];
        hex[8] = 0;
        copy_str(c->tab_name, "UPTermTabs.", sizeof(c->tab_name));
        strcat(c->tab_name, hex);
        if (!(c->tab_port = CreateMsgPort()))
            return 0;
        c->tab_port->mp_Node.ln_Name = c->tab_name;
        c->tab_port->mp_Node.ln_Pri = 0;
        AddPort(c->tab_port);
    }
    m = (struct tab_spawn_msg *)AllocVec(sizeof(*m), MEMF_PUBLIC | MEMF_CLEAR);
    if (!m)
        return 0;
    m->demo = demo;
    copy_str(m->cmd, "NewShell \"XCON:0/0/320/100/UP-Term/TAB ", sizeof(m->cmd));
    strcat(m->cmd, c->tab_name);
    strcat(m->cmd, "/PROFILE ");
    strcat(m->cmd, profile);
    strcat(m->cmd, "\"");
    if (from && from->w.t && !demo)
        copy_str(m->cwd, vt_cwd(from->w.t), sizeof(m->cwd));
    start_worker(tab_spawner, "UP-Term new tab", &m->msg);
    return 1;
}

/* a New / Next / Previous / Close tab from the host's own menu, or from a tab's */
static void host_command(con *c, int what, con *from)
{
    switch (what) {
    case MENU_TAB_NEW:
        tab_spawn(c, from->profile, from, 0);
        break;
    case MENU_DEMO:
        /* the tour in a tab of its own: what it does to its screen (the
         * main screen, a resize) leaves the user's session alone */
        if (!tab_spawn(c, from->profile, from, 1))
            DisplayBeep(c->own_win ? c->own_win->WScreen : 0); /* no room for a tab */
        break;
    case MENU_TAB_NEXT:
        if (c->ntabs >= 2)
            tab_activate(c, (c->active + 1) % c->ntabs);
        break;
    case MENU_TAB_PREV:
        if (c->ntabs >= 2)
            tab_activate(c, (c->active + c->ntabs - 1) % c->ntabs);
        break;
    case MENU_SET_SCROLLBAR:
        /* the window's scroll bar, from its own menu or a tab's */
        c->sbar_on = from->sbar_on;
        sbar_apply(c);
        break;
    case MENU_TAB_CLOSE:
        if (c->ntabs >= 2 && c->tab_list[c->active] != c) {
            struct IntuiMessage im;
            memset(&im, 0, sizeof(im));
            im.Class = IDCMP_CLOSEWINDOW;
            tab_post(c, c->tab_list[c->active], TM_EVENT, 0, &im, 0);
        } else
            close_gadget(c); /* as the close gadget: the active tab is ours */
        break;
    }
}

static void tab_command(con *c, int what)
{
    if (c->is_tab && c->host_pub) {
        struct tab_msg *t = tab_msg(c, TM_COMMAND);
        if (t) {
            t->what = what;
            PutMsg(c->host_pub, &t->msg);
        }
    } else
        host_command(c, what, c);
}

/* The host's IDCMP with tabs: the bar and the window's own events here,
 * the rest to the active tab. 0: not handled (one tab, or the host's
 * own tab is the active one -- dispatch() takes it). */
static int tab_route(con *c, struct IntuiMessage *im, int wheel)
{
    int i;
    struct Window *win = c->own_win;
    if (c->ntabs < 2 && !c->host_only)
        return 0;
    /* Right Amiga 1-9: that tab (the digits' raw codes are the same on
     * every keymap, unlike [ and ], which a Swedish map makes letters) */
    if (im->Class == IDCMP_RAWKEY && (im->Qualifier & IEQUALIFIER_RCOMMAND) &&
        im->Code >= 0x01 && im->Code <= 0x09) {
        if (im->Code - 1 < c->ntabs)
            tab_activate(c, im->Code - 1);
        return 1;
    }
    switch (im->Class) {
    case IDCMP_NEWSIZE:
        for (i = 0; i < c->ntabs; i++) {
            if (c->tab_list[i] == c)
                vtwin_resize(&c->w);
            else
                tab_post(c, c->tab_list[i], TM_EVENT, 0, im, wheel);
        }
        bar_draw(c);
        return 1;
    case IDCMP_REFRESHWINDOW:
        BeginRefresh(win);
        EndRefresh(win, TRUE);
        bar_draw(c);
        if (c->tab_list[c->active] == c)
            vtwin_show(&c->w, 1);
        else
            tab_post(c, c->tab_list[c->active], TM_EVENT, 0, im, wheel);
        return 1;
    case IDCMP_MOUSEBUTTONS:
        if (im->Code == SELECTDOWN && im->MouseY >= win->BorderTop &&
            im->MouseY < win->BorderTop + bar_height(c) && im->MouseX >= win->BorderLeft) {
            LONG w = win->Width - win->BorderLeft - win->BorderRight;
            tab_activate(c, (int)((LONG)(im->MouseX - win->BorderLeft) * c->ntabs / (w > 0 ? w : 1)));
            return 1;
        }
        break;
    }
    if (c->ntabs && c->tab_list[c->active] == c)
        return 0;
    if (c->ntabs)
        tab_post(c, c->tab_list[c->active], TM_EVENT, 0, im, wheel);
    return 1;
}

/* the host's port: tabs registering, leaving, renamed, asking */
static void host_msgs(con *c)
{
    struct tab_msg *t;
    while (c->tab_port && !c->is_tab && (t = (struct tab_msg *)GetMsg(c->tab_port)) != 0) {
        switch (t->type) {
        case TM_REGISTER:
            if (c->own_win && c->ntabs < TAB_MAX && (c->ntabs || !c->host_only)) {
                if (!c->ntabs) {
                    c->tab_list[0] = c;  /* the host's own session: the first tab */
                    c->ntabs = 1;
                    c->active = 0;
                }
                c->tab_list[c->ntabs++] = t->from;
                t->ok = 1;
                t->win = c->own_win;
                t->inset = bar_height(c);
                ReplyMsg(&t->msg);
                tab_insets(c); /* a second tab: the bar comes */
                tab_activate(c, c->ntabs - 1);
                continue;
            }
            break;
        case TM_UNREGISTER:
            tab_remove(c, t->from);
            break;
        case TM_TITLE:
            bar_draw(c);
            break;
        case TM_COMMAND:
            host_command(c, t->what, t->from);
            break;
        case TM_KNOB:
            if (c->ntabs && c->active >= 0 && c->active < c->ntabs && c->tab_list[c->active] == t->from) {
                c->knob_last = t->knob;
                c->knob_last_valid = 1;
                sbar_gad_set(&c->sbar, &t->knob);
            }
            break;
        }
        ReplyMsg(&t->msg);
    }
}

/* a tab: what its host sends */
static void tab_msgs(con *c)
{
    struct tab_msg *t;
    while (c->is_tab && c->tab_port && (t = (struct tab_msg *)GetMsg(c->tab_port)) != 0) {
        switch (t->type) {
        case TM_EVENT:
            dispatch(c, t->cls, t->code, t->qual, t->prev, t->secs, t->mics, t->mx, t->my, t->wheel);
            break;
        case TM_SHOW:
            c->shown = 1;
            vtwin_show(&c->w, 1);
            if (c->menustrip && c->w.win)
                SetMenuStrip(c->w.win, c->menustrip);
            break;
        case TM_HIDE:
            c->shown = 0; /* the host took our strip off the window */
            vtwin_show(&c->w, 0);
            break;
        case TM_INSET:
            vtwin_set_inset(&c->w, t->inset);
            break;
        }
        ReplyMsg(&t->msg);
    }
}

/* A tab's Open: the host's window, joined. 0 when there is no such host
 * (gone, or never was): the tab opens a window of its own instead. */
static struct Window *tab_register(con *c)
{
    struct tab_msg *t;
    struct MsgPort *pub;
    struct Window *win = 0;
    if (!(c->tab_port = CreateMsgPort()))
        return 0;
    t = tab_msg(c, TM_REGISTER);
    if (!t)
        goto none;
    Forbid();
    pub = FindPort((STRPTR)c->tab_host);
    if (pub)
        PutMsg(pub, &t->msg);
    Permit();
    if (!pub) {
        FreeVec(t);
        goto none;
    }
    WaitPort(c->tab_reply);
    GetMsg(c->tab_reply);
    if (t->ok) {
        win = t->win;
        c->is_tab = 1;
        c->host_pub = pub;
        c->tab_rp = *win->RPort;      /* the window's layer, our own pens and font */
        c->w.own_rp = &c->tab_rp;
        c->w.inset_top = t->inset;
        c->w.foreign_window = 1;      /* never its flags: the host's window */
        c->w.r.off = 1;               /* nothing drawn until the host shows us */
    }
    FreeVec(t);
    if (win)
        return win;
none:
    DeleteMsgPort(c->tab_port);
    c->tab_port = 0;
    return 0;
}

static void tab_unregister(con *c)
{
    struct tab_msg *t = tab_msg(c, TM_UNREGISTER);
    if (t && c->host_pub) {
        PutMsg(c->host_pub, &t->msg);
        for (;;) {
            /* the host may still be sending until it has taken us out */
            struct Message *m;
            Wait((1UL << c->tab_reply->mp_SigBit) | (1UL << c->tab_port->mp_SigBit));
            while ((m = GetMsg(c->tab_port)) != 0)
                ReplyMsg(m); /* not acted on: we are leaving */
            if ((m = GetMsg(c->tab_reply)) != 0) {
                if (m == &t->msg)
                    break;
                FreeVec(m); /* an older one of ours */
            }
        }
        FreeVec(t);
    } else if (t)
        FreeVec(t);
    tab_replies(c);
    {
        struct Message *m;
        while ((m = GetMsg(c->tab_port)) != 0)
            ReplyMsg(m);
    }
    DeleteMsgPort(c->tab_port);
    c->tab_port = 0;
    c->host_pub = 0;
    c->shown = 0;
    c->w.own_rp = 0;
}

static void h_titled(void *u)
{
    con *c = (con *)u;
    if (c->is_tab && c->host_pub) {
        struct tab_msg *t = tab_msg(c, TM_TITLE);
        if (t)
            PutMsg(c->host_pub, &t->msg);
    } else if (c->ntabs >= 2)
        bar_draw(c);
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
        if (!c->ever_opened)
            complete_warm(opener(c)); /* W22: the command cache read before the first Tab */
        c->ever_opened = 1;
        reply(p, DOSTRUE, 0);
        return;
    }
    case ACTION_END:
        c->opens--;
        DBG("render mask/planar", c->w.r.mask, c->w.r.planar);
        DBG("render pens fg/bg", c->w.r.pen_default_fg, c->w.r.pen_default_bg);
        DBG("render bg ink/seen", c->w.r.bg_ink, c->w.r.seen);
#ifdef VTCON_DEBUG
        DBG("prof writes/bytes", c->prof_writes, c->prof_bytes);
        DBG("prof out", c->prof_out, 0);
        DBG("prof direct/text", c->w.r.n_direct, c->w.r.n_text);
#endif
#ifdef VTCON_PROF
        {
            /* the phases in EClock ticks since the last close: total,
             * waiting, vt_feed (out), drawing (render) -- the rest is the
             * packets and the loop */
            struct EClockVal e;
            ULONG freq = ReadEClock(&e);
            DBG("PROF eclock/total", freq, e.ev_lo - c->prof_t0);
            DBG("PROF idle/out", c->prof_idle, c->prof_out);
            DBG("PROF render/frames", c->w.prof_render, c->w.prof_frames);
            DBG("PROF rows/scroll", c->w.prof_part[0], c->w.prof_part[1]);
            {
                extern ULONG vr_prof[4];
                DBG("PROF cursor/paint", c->w.prof_part[2], vr_prof[0]);
                DBG("PROF paints/texts", vr_prof[2], c->w.r.n_text);
                DBG("PROF waitblit/vtfeed", vr_prof[1], c->w.prof_part[3]);
                c->w.prof_part[3] = 0;
                vr_prof[0] = vr_prof[1] = vr_prof[2] = 0;
                c->w.r.n_text = 0;
            }
            DBG("PROF pkwrite/n", c->prof_pk[0], c->prof_npk[0]);
            DBG("PROF pkwait/n", c->prof_pk[1], c->prof_npk[1]);
            DBG("PROF pkother/n", c->prof_pk[2], c->prof_npk[2]);
            c->prof_pk[0] = c->prof_pk[1] = c->prof_pk[2] = 0;
            c->prof_npk[0] = c->prof_npk[1] = c->prof_npk[2] = 0;
            DBG("PROF writes/bytes", c->prof_writes, c->prof_bytes);
            c->w.prof_part[0] = c->w.prof_part[1] = c->w.prof_part[2] = 0;
            c->prof_idle = c->prof_out = c->prof_writes = c->prof_bytes = 0;
            c->w.prof_render = c->w.prof_frames = 0;
            c->prof_t0 = e.ev_lo;
        }
#endif
        reply(p, DOSTRUE, 0);
        return;
    case ACTION_READ:
        if (!c->w.win && !open_window(c)) {
            reply(p, -1, ERROR_NO_FREE_STORE);
            return;
        }
        vtwin_render(&c->w); /* what was written is on screen before the read waits */
        if (c->nreads < READ_Q) {
            c->reader_shell = task_is_shell(c, p->dp_Port ? (struct Task *)p->dp_Port->mp_SigTask : 0);
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
#if defined(VTCON_SERIAL)
        if (c->medium && p->dp_Arg2 && p->dp_Arg3 > 0) {
            /* T3 probe: what a V47 Shell writes in medium mode, 4 bytes a line */
            const UBYTE *wb = (const UBYTE *)p->dp_Arg2;
            LONG k;
            DBG("medwrite len", p->dp_Arg3, 0);
            for (k = 0; k < p->dp_Arg3 && k < 64; k += 4)
                DBG("medwrite", k, ((LONG)wb[k] << 24) | ((LONG)(k + 1 < p->dp_Arg3 ? wb[k + 1] : 0) << 16) |
                                       ((LONG)(k + 2 < p->dp_Arg3 ? wb[k + 2] : 0) << 8) |
                                       (LONG)(k + 3 < p->dp_Arg3 ? wb[k + 3] : 0));
        }
#endif
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
    case ACTION_FORCE: {
        /* The V47 Shell's answer to a medium-mode report (T3, measured on
         * 3.2): the command line it made -- a TAB completion, a history
         * line -- as Arg2 / Arg3; a leading 0x02 replaces the line being
         * edited, otherwise the text is typed in where the cursor is. The
         * line stays in the editor (shown, editable); Return sends it. */
        const UBYTE *b = (const UBYTE *)p->dp_Arg2;
        LONG n = p->dp_Arg3, k;
        if (n < 0 || !b)
            n = 0;
        if (n > 0 && b[0] == 0x02) {
            vt_u8 none = 0;
            while (c->le.pos < c->le.len)
                le_key(&c->le, VT_KEY_END, 0, &none, 0);
            le_replace_word(&c->le, 0, b + 1, (int)(n - 1));
        } else {
            for (k = 0; k < n; k++)
                if (b[k] >= 0x20)
                    le_key(&c->le, b[k], 0, &b[k], 1);
            le_command_now(&c->le); /* put in, not typed: no rest */
        }
        c->checked[0] = 0;
        check_command(c);
        reply(p, n, 0);
        return;
    }
    case ACTION_SCREEN_MODE:
        tty_leave(c); /* the Amiga way to set a mode: termios mode is over */
        DBG("screenmode", p->dp_Arg1, c->raw);
        if (c->medium && p->dp_Arg1 == 1) {
            /* medium -> raw (the V47 Shell lists completions): the line is
             * the Shell's already (the report carried it; it forces it back
             * after) -- dropped, not handed over as typed input */
            le_reset(&c->le);
            c->medium = 0;
            c->raw = 1;
            reply(p, DOSTRUE, 0);
            service_reads(c);
            return;
        }
        /* 2: the V47 medium mode -- lines edited as cooked, TAB (and the
         * history keys) reported to the reader at once (T3, DP5) */
        c->medium = p->dp_Arg1 == 2;
        if (p->dp_Arg1 == 2) {
            c->raw = 0;
            reply(p, DOSTRUE, 0);
            service_reads(c);
            return;
        }
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
        /* a program that looks for input sees its output on screen first,
         * not at the next frame (More and Ed feel it; CCON does the same;
         * and conbench's SYNC barrier is this packet: without the draw
         * our barrier numbers left a frame's work out) */
        vtwin_render(&c->w);
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
    case ACTION_VTCON_COMMAND:
        /* C:UPTerm: a slash command for this window, its answer back */
        if (!p->dp_Arg2 || !p->dp_Arg3 || p->dp_Arg4 < 2 || !c->w.t) {
            reply(p, 0, ERROR_REQUIRED_ARG_MISSING);
            return;
        }
        {
            const char *line = (const char *)p->dp_Arg2;
            char *ans = (char *)p->dp_Arg3;
            ans[0] = 0;
            reply(p, slash_run(c, line, (int)strlen(line), ans, (int)p->dp_Arg4, 0), 0);
        }
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

/* The host's own shell ended while other tabs live: its tab goes from the
 * bar, its session (engine, menus) goes, the window stays for the others. */
static void host_retire(con *c)
{
    tab_remove(c, c);
    c->host_only = 1;
    find_close(c);
    sel_close(c);
    kc_cyc_end(c);
    if (c->menustrip) {
        FreeMenus(c->menustrip); /* tab_remove put another tab's strip on the window */
        c->menustrip = 0;
    }
    vtwin_detach(&c->w);
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
    c->reader_shell = 1; /* until the first read says who reads */

    DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR)"dos.library", 39);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    /* diskfont.library: opened by the worker that loads a font from disk (render/vtwin.c disk_font) */
    GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 39);
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
    c->kc_style = LE_KC_WINDOW; /* KingCON's defaults, before any profile */
    c->kc_cache = 1;
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
    watch_start(c); /* and its changes, live (watch_worker) */

    for (;;) {
        ULONG wait = 1UL << c->port->mp_SigBit, waited;
        struct Message *m;
        if (c->own_win)
            wait |= 1UL << c->own_win->UserPort->mp_SigBit;
        if (c->tab_port)
            wait |= 1UL << c->tab_port->mp_SigBit;
        if (c->tab_reply)
            wait |= 1UL << c->tab_reply->mp_SigBit;
        if (c->timer_port)
            wait |= 1UL << c->timer_port->mp_SigBit;
        if (c->comp_port)
            wait |= 1UL << c->comp_port->mp_SigBit;
        wait |= vtwin_sigmask(&c->w);
        if (c->find_port)
            wait |= 1UL << c->find_port->mp_SigBit;
        if (c->watch_port)
            wait |= 1UL << c->watch_port->mp_SigBit;
        if (c->sel_port)
            wait |= 1UL << c->sel_port->mp_SigBit;
        own_screen_retry(c); /* an AUTO window's screen a visitor kept open */
        screen_pending(c);   /* a /screen from C:UPTerm (a packet, not a key) */
        /* No trace lines in the loop itself: each log Write's reply re-arms
         * our DOS signal, so a trace here wakes the loop forever and filled
         * RAM: on the rig (2026-09-29). */
#ifdef VTCON_PROF
        {
            struct EClockVal e0, e1;
            ReadEClock(&e0);
            wait = Wait(wait);
            ReadEClock(&e1);
            c->prof_idle += e1.ev_lo - e0.ev_lo;
        }
#else
        wait = Wait(wait);
#endif
        while ((m = GetMsg(c->port)))
#ifdef VTCON_PROF
        {
            struct DosPacket *pk = (struct DosPacket *)m->mn_Node.ln_Name;
            struct EClockVal p0, p1;
            int k = pk->dp_Type == ACTION_WRITE ? 0 : pk->dp_Type == ACTION_WAIT_CHAR ? 1 : 2;
            ReadEClock(&p0);
            packet(c, pk);
            ReadEClock(&p1);
            c->prof_pk[k] += p1.ev_lo - p0.ev_lo;
            c->prof_npk[k]++;
        }
#else
            packet(c, (struct DosPacket *)m->mn_Node.ln_Name);
#endif
        if (!(wait & ~(1UL << c->port->mp_SigBit)) && c->w.frame_open && !c->w.dragging) {
            /* only a DOS packet woke us: no other port has a message, no
             * timer is done, the frame is not due (each has its signal in
             * the mask above). A flood of small writes paid for a dozen
             * empty checks each (conbench bytewise: 0.53 ms a write
             * against CCON's 0.37; S1) */
            service_reads(c);
            if (c->opens > 0 || !c->ever_opened)
                continue;
        } else {
        waited = vtwin_tick(&c->w); /* the frame clock: draw what is due, blink */
        if (waited && c->tm)
            theme_menu_wait(c, waited); /* /theme's list: has the bar rested? */
        if (waited && c->le.cmd_rest_us > 0)
            command_wait(c, waited); /* W44: the command word's keys rested? */
        find_idcmp(c); /* the find prompt, while it is open */
        sel_idcmp(c);  /* KingCON's selection window, while it is open */
        watch_take(c); /* the profile file changed: this window's profile, live */
        host_msgs(c);  /* tabs: registering, leaving, asking (the host) */
        tab_msgs(c);   /* tabs: events and show/hide from the host (a tab) */
        tab_replies(c);
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
        }
        /* Done when every handle is closed -- but only after the first Open:
         * before it, opens is 0 too (a wake-up between the startup packet and
         * that Open used to end the handler, leaving dn_Task at a dead port). */
        if (c->ever_opened && c->opens <= 0 && (!c->wait_close || c->closing) && !c->nreads &&
            !c->comp_busy && !c->check_busy && !c->hist_busy && !c->host_only) { /* a worker holds our request */
            if (c->own_win && c->ntabs >= 2) {
                /* our own shell ended, other tabs live: their window stays
                 * ours until the last one goes; our session goes now */
                host_retire(c);
                continue;
            }
            break;
        }
        if (c->host_only && !c->ntabs)
            break; /* the last tab went */
    }
    if (c->host_only) {
        CloseWindow(c->own_win);
        c->own_win = 0;
        if (c->locked) {
            UnlockPubScreen(0, c->locked);
            c->locked = 0;
        }
        own_screen_close(c);
    } else {
        vtwin_render(&c->w);
        close_window(c);
    }
    while (own_screen_retry(c)) {
        /* our screen still has another program's window on it: the
         * process stays until that closes (DOS sends it nothing more: a
         * new Open starts a new process) */
        int f;
        for (f = 0; f < 25; f++)
            WaitTOF();
    }
    if (c->tab_port && !c->is_tab) {
        RemPort(c->tab_port);
        DeleteMsgPort(c->tab_port);
        c->tab_port = 0;
    }
    tab_replies(c);
    if (c->tab_reply)
        DeleteMsgPort(c->tab_reply);
    vtwin_cleanup(&c->w);
    Forbid();
    if (c->node && c->node->dn_Task == c->port)
        c->node->dn_Task = 0; /* never leave DOS a port that is going away */
    Permit();
    watch_stop(c);
    le_free(&c->le);
    forget_words(c);
    kc_cyc_end(c);
    if (c->kc_snap)
        FreeVec(c->kc_snap);
    if (c->save_work)
        FreeVec(c->save_work);
    if (c->comp) {
        if (c->comp->data)
            FreeVec(c->comp->data);
        FreeVec(c->comp);
    }
    if (c->check)
        FreeVec(c->check);
    if (c->menu)
        FreeVec(c->menu);
    if (c->tm)
        FreeVec(c->tm);
    if (c->theme_file)
        FreeVec(c->theme_file);
    if (c->hist) {
        if (c->hist->data)
            FreeVec(c->hist->data);
        FreeVec(c->hist);
    }
    complete_warm_forget(FindTask(0)); /* its signal is the port's */
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
    if (GadToolsBase)
        CloseLibrary(GadToolsBase);
    CloseLibrary(LayersBase);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    CloseLibrary((struct Library *)DOSBase);
    if (c->conf)
        FreeVec(c->conf);
    FreeVec(c);
    return 0;
}
