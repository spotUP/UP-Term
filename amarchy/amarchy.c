/* Amarchy: the Left Amiga key commodity (plan 2026-10-22-amarchy-commodity.md, Phase C).
 * Usage: Run >NIL: Amarchy [CX_PRIORITY n]     ARexx port AMARCHY: RELOAD, SHOW, QUIT, RUN <row>, STATUS. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/notify.h>
#include <rexx/storage.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/rexxsyslib.h>
#include <intuition/intuitionbase.h>
#include <proto/intuition.h>
#include <string.h>
#include "cx.h"
#include "demo.h"
#include "cxdesc.h"
#include "act.h"
#include "win.h"
#include "gthemes_dos.h"
#include "desk_core.h"
#include "desk_dos.h"
#include "desk_confirm_dos.h"
#include "leaf.h"
#include "rexx_host.h"
#include "note.h"
#include "tick.h"
#include "wbmode.h"
#include "clipwatch.h"
#include "wblaunch.h"
#include "menu_core.h"
#include "tile_core.h"
#include "atiming.h"
#include "../claude/util.h"
#include "../config/keymap_load.h"
#include "../config/upconf.h"
#include "amconf_dos.h"
#include "help_core.h"
#include "../handler/upactive.h"
#include "../tty/qmark.h"
#include "menu_open.h"
#include "bar.h"
#include "themewin.h"
#include "barconf_dos.h"
#include "status_core.h"
#include "shot.h"
#include "wbreset.h"
#include "strut_patch.h"
#include "borderblank_patch.h"
#include "iconpen_dos.h"
#include "iconapply_core.h"
#include "keymap_default.inc"


/* cmd begins with word, any case */
static int starts(const char *cmd, const char *word)
{
    while (*word) {
        if ((*cmd++ | 0x20) != (*word++ | 0x20))
            return 0;
    }
    return 1;
}
/* vbcc's startup runs main on a stack of this size whatever the starter gives
 * it; the cookie says the same to AmigaOS 3.2 and to vsh (RULES.md rule 6).
 * The size comes from a measurement. A restart (Break, then Run) used 16780 bytes on rig 177
 * (thoughts/shared/research/s61-evidence/restart-crash-stack-measure.md), 396 more than the 16384 this
 * was. The stack block lies directly above the BSS hunk, and the end of that hunk holds the saved
 * vectors of the system patches (wbreset, strut, borderblank): the overrun wrote stack frames over
 * them and the next patched call jumped through the damage (Exception 3 a few seconds after Run,
 * about every second restart). 65536 also leaves room for the menu and settings paths, which the
 * measurement did not walk and which hold frames up to 8.8 KB (am_prefs alone is 4.6 KB per frame);
 * amarchy-gui runs the same code on the same size. tests/test_amarchy_stack.py pins the minimum. */
long __stack = 65536;
const char amarchy_stack_cookie[] = "$STACK: 65536";

#define NNOTIFY PREFS_NNOTIFY_AMARCHY /* the ENV: copies only: prefs_core.h says why */
static struct NotifyRequest nr[NNOTIFY];
static int nr_on[NNOTIFY];
static struct rexx_host rexx;
static struct MsgPort *notify_port;
static keymap g_km;
static long g_hits;
static int g_quit;

/* The directory Amarchy was started from (amarchy-menu ships beside it): UP-Term:bin is not on
 * every command path, so the helper runs by its full path. Resolved once at startup. */
static char g_dir[128];

static void dir_resolve(void)
{
    BPTR l = GetProgramDir();
    g_dir[0] = 0;
    if (l && !NameFromLock(l, (STRPTR)g_dir, sizeof(g_dir)))
        g_dir[0] = 0;
}

/* The key viewer's window, in cells (a fixed size: its text does not change with the node). */
#define VIEWER_COLS 76
#define VIEWER_ROWS 22

/* the menu-style setting (UC_MENU_*), read from the upconf at startup and on every reload */
static int g_style;
static int g_term_menu;   /* menu-in-terminal (UC_TERMMENU_*) */
static int g_borderblank; /* AM-BORDERBLANK: border-blank = on (the default) */

static void style_load(void)
{
    am_prefs *p;
    p = amconf_borrow();
    g_style = amconf_style(p);
    g_term_menu = amconf_term_menu(p);
    g_borderblank = amconf_flat_index(p, AM_BORDERBLANK) != 0;
    amconf_return(p);
}

/* AM-BORDERBLANK: the patch follows the setting (boot and every reload). An install error leaves the stock border. */
static void borderblank_follow(void)
{
    if (g_borderblank && !borderblank_on())
        borderblank_install();
    else if (!g_borderblank && borderblank_on())
        borderblank_remove();
}

/* AmTile was on when the session ended (ENV:Amarchy/tile reads "on"): start it again, asynchronously so the boot is
 * not held. AmTile ON then writes the state file itself. */
static void tile_boot(void)
{
    char buf[8];
    LONG n = 0;
    BPTR fh = Open((STRPTR)"ENV:Amarchy/tile", MODE_OLDFILE);
    if (fh) {
        n = Read(fh, buf, sizeof buf);
        Close(fh);
    }
    if (n > 0 && tile_state_on(buf, n))
        act_run("UP-Term:bin/AmTile ON");
}

/* menu: / viewer: the program the settings name (menu-style, or menu-in-terminal over a terminal), the
 * text one in its own terminal window sized from the tree, the graphical one in its own window. */
static int act_menu_kind(int kind, const char *node)
{
    char line[KM_ID + 160];
    int text = 0, cols = VIEWER_COLS, rows = VIEWER_ROWS, in_term = 0, full = 0;
    if (mo_close_open())
        return 1;   /* MENU-TOGGLE: a menu or viewer hotkey closes whichever menu is open */
    win_active_terminal(&in_term, &full);
    if (!cx_menu_cmd(line, sizeof(line), g_dir, kind, node, cx_menu_style(g_style, g_term_menu, in_term, full), &text))
        return 0;
    win_ticket_take();   /* K5a: the menu is about to become the active window; win: rows act on this one */
    if (!text)
        return act_menu_open(g_dir, 0, kind, node, line, cols, rows);   /* the drawn menu: its warm server, else cold */
    if (kind == KA_MENU) {
        keymap full;
        /* g_km is compacted (no [menu] rows, keymap_compact): the size comes from the full table,
         * loaded for this open and released again, the table amarchy-menu will show; the rows follow the terminal the hotkey was pressed over (the
         * commodity is no UP-Term window: self 0) */
        memset(&full, 0, sizeof full);
        keymap_load(&full, km_default_text);
        mn_osd_size_t(&full, strcmp(node, "root") ? node : "", &cols, &rows,
                      mn_cond(upactive_terminal_present(0), upactive_dopus_present(), upactive_workbench_present()));
        keymap_free(&full);
    }
    return act_menu_open(g_dir, 1, kind, node, line, cols, rows);
}

/* A decimal number after optional blanks and sign, 0 when there is none (atoi). */
static int parse_int(const char *s)
{
    int v = 0, neg = 0;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '-' || *s == '+')
        neg = *s++ == '-';
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

/* Signals of the bar's window and timer, read again whenever BAR ON or OFF changes them (0 while the bar is closed). */
static ULONG g_bs, g_bts;
static int g_bar_want = 1;  /* BAR ON (1) or OFF (0) said during the hold: applied when the hold ends (bar_plan) */
static int g_wb_hold;   /* 1 while the windows wait for the Workbench's final mode (W43) */

/* desk_dos_apply (R1.17) closes and reopens the bar around the Workbench reset IPrefs runs after a prefs USE: direct, the BAR verb cannot reach this process while it applies */
static int desk_wb_close(void)
{
    int was = bar_is_open();
    if (was)
        bar_close();
    g_bs = bar_sigmask();
    g_bts = bar_timer_sigmask();
    wbr_report(SIGBREAKF_CTRL_F, WBR_HIDE, bar_is_open());   /* WBRESET-EXT: the reset this apply starts waits on no one */
    return was;
}

/* g_bar_want is not touched: the state before the apply comes back */
static void desk_wb_open(int was)
{
    if (was && !g_wb_hold && !bar_is_open())
        bar_open(g_dir);
    g_bs = bar_sigmask();
    g_bts = bar_timer_sigmask();
    wbr_report(SIGBREAKF_CTRL_F, WBR_HIDE, bar_is_open());
}

/* WBRESET-EXT: another program closes the Workbench (IPrefs after `Palette ... USE`): the CloseWorkBench patch sends
 * Ctrl-F, the bar closes (desk_wb_close) and reopens once OpenWorkBench ran (desk_wb_open), as around our own apply. */
static int g_wbr_was;
static void wbr_wake(void)
{
    int d = wbr_client_wake();
    if (d == WBR_DO_HIDE)
        g_wbr_was = desk_wb_close();
    else if (d == WBR_DO_SHOW)
        desk_wb_open(g_wbr_was);
}

/* ICONPEN-MAIN-BOOT: the icon pens the Amarchy .info files name (32..47) are held by Amarchy main while the icons are applied
 * (the manifest of amarchy-icons exists): at start, after a reboot, and again after every Workbench reopen (ipd_resync inside
 * the tick). RESTORE removes the manifest, and main lets go. The decisions are ip_main_step (iconpen_core.c). */
static int g_ip_own;
static int icons_applied(void)
{
    BPTR l = Lock((STRPTR)IA_MANIFEST, ACCESS_READ);
    if (l)
        UnLock(l);
    return l != 0;
}

static void iconpens_follow(long now)
{
    (void)now;
    switch (ip_main_step(icons_applied(), g_ip_own)) {
    case IP_MAIN_TAKE:
        if (tw_libs_open()) {           /* graphics.library, the pens' base */
            ipd_owner(IP_OWNER_MAIN, 1);
            g_ip_own = 1;
        }
        break;
    case IP_MAIN_DROP:
        ipd_owner(IP_OWNER_MAIN, 0);
        tw_libs_close();
        g_ip_own = 0;
        break;
    case IP_MAIN_SYNC:
        ipd_resync();
        break;
    default:
        break;
    }
}

static void iconpens_stop(void)
{
    if (g_ip_own) {
        ipd_owner(IP_OWNER_MAIN, 0);
        tw_libs_close();
        g_ip_own = 0;
    }
}

/* every window Amarchy opens at start: the status bar (the menu servers and AmTile open theirs on use) */
static void start_windows(void)
{
    if (g_bar_want)
        bar_open(g_dir);                        /* the status bar; 0 = no screen, the commodity runs without it */
    g_bs = bar_sigmask();
    g_bts = bar_timer_sigmask();
}

/* RL16 W10: the welcome window, once, when welcome-shown is not on. A separate amarchy-gui process through the same
 * launch the Help hotkey uses; the window saves the key itself when the box is ticked. */
static void welcome_maybe(void)
{
    am_prefs *p;
    char line[KM_ID + 160];
    p = amconf_borrow();
    if (hc_should_welcome(p) && cx_helper_cmd(line, sizeof line, g_dir, "amarchy-gui", "welcome"))
        act_run(line);
    amconf_return(p);
}

/* S32-START-LOOK: the start look needs the Workbench screen, and Amarchy starts from S:User-Startup before LoadWB
 * opens it; ipd_lock_wb_screen never opens a closed Workbench (BOOT-31-WB-DEADLOCK), so these steps found no screen
 * and wrote nothing. Run once at start when the screen is up, else on the first tick that finds it (desk_start_due). */
static int g_desk_started, g_tick_ok;
static void desk_start_gate(long now)
{
    struct Screen *scr;
    int ready;
    if (g_desk_started)
        return;                                 /* the tick hook stays; done costs nothing */
    scr = ipd_lock_wb_screen();
    if (scr)
        UnlockPubScreen(0, scr);
    /* S57-START-POINTER-HANG: the screen is up at the start of LoadWB; no editor runs until Workbench has finished it.
     * Asked only when everything else is due: the round-trip waits for Workbench's reply. */
    ready = scr && g_tick_ok && !g_wb_hold ? desk_dos_wb_ready(now) != DESK_WBR_WAIT : 1;
    if (!desk_start_due(g_desk_started, scr != 0, g_tick_ok, g_wb_hold, ready))
        return;                                 /* S32-GREEN-LINE-96: wb_gate runs first on the tick and ends the hold */
    g_desk_started = 1;
    desk_dos_flat_pens_refresh();               /* FLAT-PENS-STALE-FILE: after the windows (or the W43 hold) are set up, the hooks above close/reopen them around a reset */
    desk_dos_marker_recover();                  /* FP-3 (R1.9): an apply that never reached Keep or Revert does not stick */
    desk_dos_wb_backdrop();                     /* S32-WB-BACKDROP: frameless Workbench root window */
    desk_dos_start_apply();                     /* S31-START-APPLY: after the recover, so a leftover revert runs first; prefs on reach ENV: */
}

/* the 1 s tick while held: opens the windows once the Workbench runs the saved mode, or after the timeout */
static void wb_gate(long now)
{
    int r;
    if (!g_wb_hold)
        return;
    r = wbm_check(now);
    if (r == WBM_WAIT)
        return;
    if (r == WBM_TIMEOUT)
        PutStr((STRPTR)"Amarchy: the Workbench never reached the saved screen mode; opening the windows anyway\n");
    g_wb_hold = 0;
    start_windows();
    welcome_maybe();   /* W43: no foreign window before the Workbench reset, so the welcome waits for the release */
}

/* The STATUS reply: hits=N rows=N enabled=N active=N wins=N snaps=N bar=N wbpens=N wbrefused=N shots=N notes=N ticks=N,
 * the keys of status_core.c (a test holds them under REXX_HOST_OUT); BAR STATUS answers bar draws panels launches raises ticks net (BAR_NET_*: 1 emulated, 2 no interface, 3 down, 4 up) strut_opens strut_clamped strut_zoom (strut_patch.c; STATUS is full at REXX_HOST_OUT). */
static void status_text(char *out, long cap, int bar_status)
{
    long val[STATUS_NVALS];
    if (bar_status) {
        val[0] = bar_is_open();
        val[1] = bar_draws();
        val[2] = bar_panels();
        val[3] = bar_launches();
        val[4] = bar_raises();
        val[5] = tick_count();
        val[6] = bar_net();
        strut_counts(&val[7], &val[8], &val[9]);
    } else {
        val[0] = g_hits;
        val[1] = cx_nrows();
        val[2] = cx_enabled();
        val[3] = upactive_count();
        val[4] = win_count();
        val[5] = win_snap_count();
        val[6] = bar_is_open();
        val[7] = (long)gt_dos_wb_pens;
        val[8] = (long)gt_dos_wb_refused;
        val[9] = shot_count();
        val[10] = note_posted();
        val[11] = tick_count();
    }
    status_line(bar_status, val, out, cap);
}

/* BAR STATUS | ON | OFF | TOGGLE | TEXT (the segments, bar_dump) | PENS (tw_describe). OFF closes the window and its raise timer (the tick keeps running), ON opens it again;
 * rc 10 for another word, 5 when ON finds no screen. */
static long bar_port(const char *args, char *out)
{
    int v = bar_verb(args), act;
    if (v == BARV_BAD)
        return 10;
    if (v == BARV_PENS) {
        tw_describe(out, REXX_HOST_OUT);
        return 0;
    }
    if (v == BARV_TEXT) {
        bar_text(out, REXX_HOST_OUT);
        return 0;
    }
    if (v == BARV_NET) {
        bar_net_info(out, REXX_HOST_OUT);
        return 0;
    }
    if (v == BARV_NETUP || v == BARV_NETDOWN)
        return bar_net_set(v == BARV_NETUP);
    act = bar_plan(v, g_wb_hold, bar_is_open(), &g_bar_want);
    if (act == BARA_CLOSE)
        bar_close();
    else if (act == BARA_OPEN)
        bar_open(g_dir);
    g_bs = bar_sigmask();
    g_bts = bar_timer_sigmask();
    if (act == BARA_OPEN && !bar_is_open())
        return 5;
    status_text(out, REXX_HOST_OUT, 1);
    return 0;
}

/* The one capture entry: the ARexx CAP verb and the cap: hotkey rows both come here. screen | window | region; 0 done,
 * 10 for any other word, else the shot code's own rc. */
static long cap_run(const char *kind)
{
    if (starts(kind, "screen"))
        return shot_screen(shot_default_dir());
    if (starts(kind, "window"))
        return shot_window(shot_default_dir());
    if (starts(kind, "region"))
        return shot_region(shot_default_dir());
    return 10;
}

/* Run the action of row `row` (hotkey or ARexx RUN): one place, one counter. */
static int do_row(int row)
{
    const km_bind *b = cx_row(row);
    int ok = 0;
    if (!b)
        return 0;
    g_hits++;
    if (b->kind == KA_MENU || b->kind == KA_VIEWER)
        at_mark("hotkey");   /* opt-in: AMARCHY_TIMING */
    switch (b->kind) {
    case KA_RUN:
    case KA_JOB:        /* a hotkey has no menu to show a job view in: run: */
    case KA_DETACH:
    case KA_SCAN: ok = act_run(b->arg); break;
    case KA_REXX:  ok = act_rexx(b->arg); break;
    case KA_WIN:   ok = act_win(b->arg); break;
    case KA_GTHEME: ok = gt_dos_select(b->arg, 0, 0); break;
    case KA_CAP:   ok = cap_run(b->arg) == 0; break;
    case KA_OPEN:  ok = act_open_uterm(); break;
    case KA_BAR: {
        char scratch[REXX_HOST_OUT];
        ok = bar_port(b->arg, scratch) == 0;
        break;
    }
    case KA_GUI: {
        char gl[KM_ID + 160];
        ok = cx_helper_cmd(gl, sizeof gl, g_dir, "amarchy-gui", b->arg) && act_run(gl);
        break;
    }
    case KA_AMARCHY:
        g_quit = 1;   /* as the ARexx QUIT: the loop ends and the normal shutdown follows */
        ok = 1;
        break;
    case KA_MENU: case KA_VIEWER:
        ok = act_menu_kind(b->kind, b->arg);
        break;
    default:       ok = 1; break;
    }
    return ok;
}

static void reload(void)
{
    style_load();
    borderblank_follow();
    desk_dos_flat_sync();   /* FLAT-PREFS-NOT-NOTIFIED: a flat switch written to the prefs file reaches amarchy-flat */
    cx_detach();   /* the gate must not read the table that is about to be released */
    keymap_reload_compact(&g_km, cx_row_installed);   /* a bad edit leaves the old table */
    cx_install(&g_km);
}

static void notify_start(void)
{
    int i;
    for (i = 0; i < NNOTIFY; i++) {
        nr[i].nr_Name = (STRPTR)prefs_notify_amarchy[i];
        nr[i].nr_Flags = NRF_SEND_MESSAGE;
        nr[i].nr_stuff.nr_Msg.nr_Port = notify_port;
        nr_on[i] = StartNotify(&nr[i]) ? 1 : 0;
    }
}

static void notify_stop(void)
{
    int i;
    struct Message *m;
    for (i = 0; i < NNOTIFY; i++)
        if (nr_on[i])
            EndNotify(&nr[i]);
    while (notify_port && (m = GetMsg(notify_port)) != 0)
        ReplyMsg(m);
}

static long rexx_cmd(void *ud, const char *cmd, char *out)
{
    long rc = 0;
    (void)ud;
    if (starts(cmd, "STATUS"))
        status_text(out, REXX_HOST_OUT, 0);
    else if (starts(cmd, "BAR") && (cmd[3] == ' ' || cmd[3] == '\t'))
        rc = bar_port(cmd + 3, out);
    else if (starts(cmd, "DEMO") && (cmd[4] == 0 || cmd[4] == ' ' || cmd[4] == '\t'))
        rc = demo_port(cmd + 4, out, REXX_HOST_OUT);
    else if (starts(cmd, "DESK CONFIRM"))   /* FP-1: an apply in another process; the verb carries the mask */
        dc_begin((unsigned)parse_int(cmd + 12));
    else if (starts(cmd, "RELOAD"))
        reload();
    else if (starts(cmd, "RUN"))
        rc = do_row(parse_int(cmd + 3)) ? 0 : 5;
    else if (starts(cmd, "WIN") && (cmd[3] == ' ' || cmd[3] == '\t'))
        rc = win_port(cmd + 4);
    else if (starts(cmd, "CAP") && (cmd[3] == ' ' || cmd[3] == '\t')) {
        /* the capture entry the keymap kind cap: (R4.7) also calls (cap_run): screen | window | region; the reply is the file */
        const char *a = cmd + 4;
        while (*a == ' ' || *a == '\t')
            a++;
        rc = cap_run(a);
        if (rc == 0)
            cl_cat(out, shot_last(), REXX_HOST_OUT);
    } else if (starts(cmd, "SHOTS")) {
        /* the capture counter: files written by shot.c since the start (STATUS is full at REXX_HOST_OUT) */
        char num[12];
        cl_ltoa(shot_count(), num);
        cl_cat(out, num, REXX_HOST_OUT);
    } else if (starts(cmd, "NOTES")) {
        /* the notice counters (note.c answers the same verb on AMARCHY.NOTIFY): notes=N shown=N last=<title> */
        rc = note_run(cmd, out);
    } else if (starts(cmd, "QUIT"))
        g_quit = 1;
    else if (starts(cmd, "SHOW"))
        act_menu_kind(KA_VIEWER, "");
    else
        rc = 10;
    return rc;
}

/* WBStartup launches of index rows (Workbench's job, which Directory Opus does not do): the launcher
 * (amarchy-gui, amarchy-menu) sends lf_launch to port LF_LAUNCH_PORT and goes; the program's message,
 * segment and locks stay here, on a reply port that lives as long as this process, until the program
 * replies when it ends. */
struct launch_msg { struct Message msg; lf_launch b; };
static struct MsgPort *launch_port, *wb_reply;
static int launches_out;

static void launch_serve(void)
{
    struct launch_msg *m;
    while ((m = (struct launch_msg *)GetMsg(launch_port)) != 0) {
        const char *path = m->b.path;
        char err[LF_LAUNCH_ERR];
        err[0] = 0;
        m->b.path[LF_LAUNCH_PATH - 1] = 0;
        if (!wb_reply)
            m->b.rc = -1, strcpy(m->b.err, "no reply port");
        else if (wbl_start(&path, 1, wb_reply, err, sizeof err) == 0) {
            m->b.rc = 0;
            launches_out++;
        } else {
            m->b.rc = -1;
            err[LF_LAUNCH_ERR - 1] = 0;
            strcpy(m->b.err, err);
        }
        ReplyMsg(&m->msg);
    }
}

static void launch_replied(void)
{
    struct Message *m;
    while ((m = GetMsg(wb_reply)) != 0) {
        wbl_free((struct WBStartup *)m);
        launches_out--;
    }
}

/* act.c reads the front screen (AMB3) from it */
struct IntuitionBase *IntuitionBase;

/* `Amarchy demo [stop|status|<n>]`: the CLI form of the DEMO verb, the same code path as the menu row (rexx_cmd).
 * Returns -1 when the line is not a demo line, else the program's return code. */
static int demo_cli(const char *a)
{
    static char out[REXX_HOST_OUT];
    char cmd[40];
    int n = 0;
    while (*a == ' ' || *a == '\t')
        a++;
    if (!starts(a, "DEMO") || (a[4] != 0 && a[4] != ' ' && a[4] != '\t' && a[4] != '\n'))
        return -1;
    a += 4;
    while (*a == ' ' || *a == '\t')
        a++;
    strcpy(cmd, "DEMO ");
    n = 5;
    while (*a && *a != '\n' && n < (int)sizeof cmd - 1)
        cmd[n++] = *a++;
    cmd[n] = 0;
    while (n > 5 && cmd[n - 1] == ' ')
        cmd[--n] = 0;
    if (!FindPort((STRPTR)"AMARCHY")) {
        PutStr((STRPTR)"Amarchy is not running\n");
        return 5;
    }
    out[0] = 0;
    n = (int)rexx_send("AMARCHY", cmd, out, (int)sizeof out);
    PutStr((STRPTR)out);
    PutStr((STRPTR)"\n");
    return n == 0 ? 0 : 10;
}

int main(void)
{
    ULONG sigs, cxs, rxs, nos, nts, lps, wrs, cws, tks;
    int tick_open_ok;
    int row, ev;
    struct RDArgs *rda;
    LONG args[1] = { 0 };
    int pri = 0;
    if ((ev = demo_cli((const char *)GetArgStr())) >= 0)
        return ev;
    if (qmark_line((const char *)GetArgStr())) { /* `Amarchy ?` shows the template and ends */
        PutStr((STRPTR)"CX_PRIORITY/K/N  |  DEMO [STOP|STATUS|n]\n");
        return 0;
    }
    if ((rda = ReadArgs((STRPTR)"CX_PRIORITY/K/N", args, 0))) {
        if (args[0])
            pri = (int)*(LONG *)args[0];
        FreeArgs(rda);
    }
    dir_resolve();
    if (amconf_migrate() == AMCONF_MIG_FAILED)
        PutStr((STRPTR)"Amarchy: could not write ENVARC:Amarchy/prefs; reading the UP-Term file's keys, retrying next start\n");
    style_load();
    gt_dos_term_boot();                         /* F3-THEME-BOOT: new consoles start in the active theme before any switch */
    {   /* bar.conf: start on|off and the clock formats (the other keys are parsed, not yet used) */
        static barconf bc;
        barconf_load(&bc);
        g_bar_want = bc.start;
        bar_set_clock_formats(bc.clock, bc.clockalt);
        bar_set_conf(&bc);
    }
    keymap_load_compact(&g_km, km_default_text, cx_row_installed);
    if (!cx_open(pri)) {
        cx_close();
        return 5;   /* already running (it was told), or commodities.library is missing */
    }
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    rexx_host_open(&rexx, "AMARCHY");
    note_open();
    wb_reply = CreateMsgPort();
    Forbid();
    launch_port = wb_reply && !FindPort((STRPTR)LF_LAUNCH_PORT) ? CreateMsgPort() : 0;
    if (launch_port) {
        launch_port->mp_Node.ln_Name = (char *)LF_LAUNCH_PORT;
        AddPort(launch_port);
    }
    Permit();
    notify_port = CreateMsgPort();
    if (notify_port)
        notify_start();
    cx_install(&g_km);
    tile_boot();
    act_menu_server_start(g_dir);               /* the warm menu server; the first press before it is up opens a menu of its own */
    cxs = cx_sigmask();
    rxs = rexx_host_sigmask(&rexx);
    nos = note_sigmask();
    nts = notify_port ? 1UL << notify_port->mp_SigBit : 0;
    lps = launch_port ? 1UL << launch_port->mp_SigBit : 0;
    wrs = wb_reply ? 1UL << wb_reply->mp_SigBit : 0;
    cw_start();                                 /* the clipboard history (Trigger > Clipboard history) */
    cws = cw_sigmask();
    tick_hook(note_expire);
    demo_set_bar(bar_port);
    tick_hook(demo_tick);
    tick_hook(bar_tick);
    tick_hook(tw_tick);                         /* R2.5b: a theme switch retakes the helper windows' pens */
    tick_hook(iconpens_follow);
    iconpens_follow(0);                         /* at boot, before the tick's first second */
    g_tick_ok = tick_open_ok = tick_open();                 /* the 1 s tick, before the bar and running without it */
    dc_init();                                  /* FP-1: the confirm requester and its 15 s timeout ride the tick */
    tks = tick_sigmask();
    /* W43: no window before the Workbench is in its final mode (IPrefs resets it late; the reset cannot run while a
     * foreign window is open). Ready now, or no tick to wait on: open at once; else the tick hook opens them. */
    desk_dos_wb_close = desk_wb_close;
    desk_dos_wb_open = desk_wb_open;
    strut_patch_install();                      /* BAR-STRIP-RESERVE: no patch (error) = windows open as before */
    wbr_patch_install();                        /* WBRESET-EXT: no patch (error) = the old behaviour */
    borderblank_follow();                       /* AM-BORDERBLANK: the setting read by style_load at startup */
    wbm_begin();
    g_wb_hold = tick_open_ok && wbm_check(0) == WBM_WAIT;
    if (!g_wb_hold)
        start_windows();
    else
        tick_hook(wb_gate);
    desk_start_gate(0);                         /* S32-START-LOOK: now if the Workbench screen is up, else from the 1 s tick */
    if (!g_desk_started)
        tick_hook(desk_start_gate);
    if (!g_wb_hold)
        welcome_maybe();
    while (!g_quit) {
        sigs = Wait(cxs | rxs | nos | nts | lps | wrs | cws | g_bs | g_bts | tks | shot_region_sigmask() | dc_sigmask() | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_F);
        if (sigs & SIGBREAKF_CTRL_C)
            break;
        if (sigs & SIGBREAKF_CTRL_F)
            wbr_wake();
        if (sigs & cxs)
            while ((ev = cx_next(&row)) != CXE_NONE) {
                if (ev == CXE_KEY)
                    do_row(row);
                else if (ev == CXE_BAR_RIGHT)
                    bar_right_click();
                else if (ev == CXE_UNIQUE)
                    reload();
                else if (ev == CXE_KILL)
                    g_quit = 1;
            }
        if (sigs & dc_sigmask())
            dc_serve();
        if (sigs & shot_region_sigmask())
            shot_region_serve();
        if (sigs & g_bs)
            bar_serve();
        if (sigs & g_bts)
            bar_timer_serve(sigs);
        if (sigs & tks)
            tick_serve(sigs);
        if (sigs & cws)
            cw_changed();
        if (sigs & rxs)
            rexx_host_serve(&rexx, rexx_cmd, 0);
        if (sigs & nos)
            note_serve();
        if (sigs & lps)
            launch_serve();
        if (sigs & wrs)
            launch_replied();
        if (sigs & nts) {
            struct Message *m;
            while ((m = GetMsg(notify_port)) != 0)
                ReplyMsg(m);
            reload();
        }
        wbr_report(SIGBREAKF_CTRL_F, WBR_HIDE, bar_is_open());   /* BAR ON / OFF, the hold, a reload: all end here */
    }
    demo_shutdown();
    bar_close();
    iconpens_stop();
    wbr_client_drop();
    wbr_patch_remove();
    strut_patch_remove();
    borderblank_remove();
    tick_close();
    act_menu_server_quit();
    cw_stop();
    notify_stop();
    if (notify_port)
        DeleteMsgPort(notify_port);
    note_close();
    rexx_host_close(&rexx, rexx_cmd, 0);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    cx_close();
    /* Quit (Exchange Remove, Ctrl-C, QUIT): no new launch, and the process stays until every program it
     * started has replied its WBStartup message, so no reply goes to a deleted port. The commodity is
     * already gone from Exchange and its hotkeys are released. */
    if (launch_port) {
        RemPort(launch_port);
        launch_serve();             /* requests that came in before the port went still get started */
        DeleteMsgPort(launch_port);
    }
    while (launches_out > 0) {
        Wait(1UL << wb_reply->mp_SigBit);
        launch_replied();
    }
    if (wb_reply)
        DeleteMsgPort(wb_reply);
    return 0;
}
