/* AmTile: the autotiling commodity (plan 2026-10-28-amarchy-autotile.md, phase 2). tile_core.c does every number
 * (what tiles, the order, the master-stack boxes); this file watches the UP-Term screen, hands the windows to
 * tile_core and applies the boxes.
 *
 * One instance: the ARexx port AMTILE. A second start sends its command-line word to that port and exits with
 * its return code. Commands: ON OFF TOGGLE RETILE GAP <n> STATUS QUIT, LAYOUT <name>|NEXT|PREV,
 * RATIO <n>|+n|-n, NMASTER <n>|+n|-n, CYCLE NEXT|PREV, PROMOTE. STATUS answers
 * "on|off <tiled windows> <retile count> <layout> <nmaster> <ratio>"; the retile count rises each time boxes are
 * applied (the rig's sentinel that this code ran). LAYOUT, RATIO and NMASTER persist (amconf hidden keys). While on, a timer.device request polls the screen every TILE_POLL_MICROS; a
 * retile happens only when the set of tiled windows changed (or on ON, RETILE, RATIO, GAP).
 *
 * Lock discipline is win.c's: the screen walk and the membership check run with IBase locked, and
 * ChangeWindowBox, which only queues a request, is called under the same lock, so a window cannot close between
 * the check and the call. LockPubScreen and the other screen calls run with it unlocked. */
#include <exec/types.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <libraries/commodities.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/commodities.h>
#include <string.h>
#include "rexx_host.h"
#include "tile_core.h"
#include "amconf_dos.h"
#include "win_apply.h"
#include "poll_timer.h"
#include "amconf_core.h"
#include "../handler/frontscr.h"

#define PORT_NAME      "AMTILE"
#define TILE_MAX       32             /* windows considered on the screen */
#define TILE_POLL_MICROS 250000UL
#define TITLE_MAX      16             /* tile_classify only compares against "Amarchy" */
#define STATE_DIR_ENV    "ENV:Amarchy"
#define STATE_DIR_ENVARC "ENVARC:Amarchy"
#define STATE_ENV        "ENV:Amarchy/tile"
#define STATE_ENVARC     "ENVARC:Amarchy/tile"

/* vbcc's startup runs main on a stack of this size whatever the starter gives
 * it; the cookie says the same to AmigaOS 3.2 and to vsh (RULES.md rule 6). */
long __stack = 16384;
const char amtile_stack_cookie[] = "$STACK: 16384";

struct Library *CxBase;
struct IntuitionBase *IntuitionBase;

static struct rexx_host rexx;
static struct MsgPort *cx_port;
static CxObj *g_broker;
static poll_timer g_poll;
static int g_on, g_quit;
static int g_gap = TILE_GAP_DEFAULT;   /* the tile-gap key of the Amarchy settings file, read at start and on RETILE */
static int g_ratio = TILE_RATIO_DEFAULT;
static int g_layout = TL_DWINDLE, g_nmaster = 1;
static long g_last_raised;   /* monocle: the window last brought to front, 0 outside monocle */
static long g_order[TILE_MAX];
static int g_norder;
static long g_scr_id;     /* the screen g_order was built on (tile_screen_switch) */
static long g_retiles;

/* cmd begins with word (any case) and the word ends there */
static int starts(const char *cmd, const char *word)
{
    while (*word) {
        if ((*cmd++ | 0x20) != (*word++ | 0x20))
            return 0;
    }
    return *cmd == 0 || *cmd == ' ';
}

/* The number after the first word, in lo..hi. rel NULL: digits only. rel non-NULL: an optional leading '+' or '-' is
 * accepted and *rel = +1/-1 (0 without a sign); with a sign the magnitude must be 1..(hi-lo) and *out holds the
 * magnitude, without a sign lo..hi as before. */
static int num_arg(const char *cmd, int lo, int hi, int *out, int *rel)
{
    long v = 0;
    int any = 0, sign = 0;
    while (*cmd && *cmd != ' ')
        cmd++;
    while (*cmd == ' ')
        cmd++;
    if (rel && (*cmd == '+' || *cmd == '-'))
        sign = *cmd++ == '+' ? 1 : -1;
    for (; *cmd >= '0' && *cmd <= '9'; cmd++, any = 1) {
        v = v * 10 + (*cmd - '0');
        if (v > 100000L)
            return 0;
    }
    if (!any || *cmd)
        return 0;
    if (sign ? (v < 1 || v > hi - lo) : (v < lo || v > hi))
        return 0;
    *out = (int)v;
    if (rel)
        *rel = sign;
    return 1;
}

/* The first word after the verb into w (cap bytes), 0 when there is none or it does not fit. */
static int word_arg(const char *cmd, char *w, int cap)
{
    int n = 0;
    while (*cmd && *cmd != ' ')
        cmd++;
    while (*cmd == ' ')
        cmd++;
    while (cmd[n] && cmd[n] != ' ')
        n++;
    if (n == 0 || n >= cap)
        return 0;
    memcpy(w, cmd, (size_t)n);
    w[n] = 0;
    return 1;
}

static int clamp_int(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static char *put_num(char *p, long n)
{
    char digits[12];
    int d = 0;
    do { digits[d++] = (char)('0' + n % 10); n /= 10; } while (n);
    while (d)
        *p++ = digits[--d];
    return p;
}

/* ---- state file: "on" or "off" in ENV: and ENVARC: ------------------------------------------------------------ */

static void state_write(const char *dir, const char *path)
{
    BPTR fh;
    const char *txt = g_on ? "on\n" : "off\n";
    UnLock(CreateDir((STRPTR)dir));   /* fails when the drawer exists, which is fine */
    fh = Open((STRPTR)path, MODE_NEWFILE);
    if (fh) {
        Write(fh, (APTR)txt, (LONG)strlen(txt));
        Close(fh);
    }
}

static void state_save(void)
{
    state_write(STATE_DIR_ENV, STATE_ENV);
    state_write(STATE_DIR_ENVARC, STATE_ENVARC);
}

/* ---- the poll timer -------------------------------------------------------------------------------------------- */

/* ---- the screen walk and the apply ------------------------------------------------------------------------------ */

/* The front screen (any public screen, not only UP-Term), 0 when the front screen is not public. Only the pointer is
 * kept: it is checked against IBase's screen list under the lock before it is used. */
static struct Screen *front_scr(void)
{
    struct Screen *s = lock_front_screen();
    if (s)
        UnlockPubScreen(0, s);
    return s;
}

/* IBase locked: scr is still a screen of Intuition's. */
static int screen_alive(const struct Screen *scr)
{
    const struct Screen *s;
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
        if (s == scr)
            return 1;
    return 0;
}

/* IBase locked: w is still a window of scr. */
static int window_alive(const struct Screen *scr, const struct Window *w)
{
    const struct Window *x;
    for (x = scr->FirstWindow; x; x = x->NextWindow)
        if (x == w)
            return 1;
    return 0;
}

typedef struct snap {
    tile_win w[TILE_MAX];
    char title[TILE_MAX][TITLE_MAX];
    int n;
    win_screen scr;
    long active;   /* the active window at the walk, 0 when none */
} snap;

/* IBase locked: the screen's windows copied into sn. */
static void walk(const struct Screen *scr, snap *sn)
{
    const struct Window *x;
    sn->n = 0;
    sn->active = (long)IntuitionBase->ActiveWindow;
    for (x = scr->FirstWindow; x && sn->n < TILE_MAX; x = x->NextWindow) {
        tile_win *t = &sn->w[sn->n];
        memset(t, 0, sizeof *t);
        win_box_of(x, &t->box, &sn->scr);
        t->id = (long)x;
        t->flags = ((x->Flags & WFLG_SIZEGADGET) ? TW_SIZEGADGET : 0) | ((x->Flags & WFLG_BACKDROP) ? TW_BACKDROP : 0) |
                   ((x->Flags & WFLG_BORDERLESS) ? TW_BORDERLESS : 0);
        if (x->Title) {
            strncpy(sn->title[sn->n], (const char *)x->Title, TITLE_MAX - 1);
            sn->title[sn->n][TITLE_MAX - 1] = 0;
            t->title = sn->title[sn->n];
        }
        sn->n++;
    }
}

/* Gap, ratio, nmaster and layout from the Amarchy settings file (amconf), the one reader. A GAP n command wins until
 * the next RETILE (or start), which reads the file again; LAYOUT, RATIO and NMASTER write through (cfg_save). */
static void cfg_load(void)
{
    am_prefs p;
    int k;
    amconf_load(&p);
    g_gap = amconf_tile_gap(&p);
    g_ratio = amconf_tile_ratio(&p);
    g_nmaster = amconf_tile_nmaster(&p);
    g_layout = tile_layout_parse(amconf_tile_layout(&p), &k) ? k : TL_DWINDLE;
}

/* The only write path: the layout, ratio and nmaster into ENVARC: then ENV: (gthemes_dos.c's gt_dos_select pattern).
 * A failed save is logged and does not fail the command. */
static void cfg_save(void)
{
    am_prefs p;
    int failed = 0;
    char *e;
    amconf_load(&p);
    strcpy(p.val[AM_TILELAYOUT], tile_layout_name(g_layout));
    e = put_num(p.val[AM_TILERATIO], g_ratio);
    *e = 0;
    e = put_num(p.val[AM_TILENMASTER], g_nmaster);
    *e = 0;
    if (amconf_save(&p, 1, &failed) != PREFS_INSTALL_OK)
        PutStr((STRPTR)"AmTile: could not save the layout settings\n");
}

/* The active window's id (the same read as win.c), 0 when none. */
static long active_id(void)
{
    ULONG lock = LockIBase(0);
    long id = (long)IntuitionBase->ActiveWindow;
    UnlockIBase(lock);
    return id;
}

/* One look at the screen. force: retile even when the tiled set is unchanged. */
static void tick(int force)
{
    struct Screen *scr = front_scr();
    snap *sn;
    long set[TILE_MAX], old[TILE_MAX];
    tile_win in[TILE_MAX];
    win_box out[TILE_MAX];
    ULONG lock;
    int i, j, m = 0, nold, changed;
    if (!scr || !IntuitionBase || !(sn = AllocVec(sizeof *sn, MEMF_PUBLIC | MEMF_CLEAR)))
        return;
    lock = LockIBase(0);
    if (screen_alive(scr))
        walk(scr, sn);
    UnlockIBase(lock);
    for (i = 0; i < sn->n; i++)
        if (tile_classify(&sn->w[i]) == TILE_TILE)
            set[m++] = sn->w[i].id;
    g_norder = tile_screen_switch(&g_scr_id, (long)scr, g_norder);
    nold = g_norder;
    memcpy(old, g_order, sizeof(long) * (size_t)nold);
    g_norder = tile_order_update(g_order, g_norder, set, m);
    changed = g_norder != nold || memcmp(old, g_order, sizeof(long) * (size_t)nold) != 0;
    if ((changed || force) && g_norder > 0) {
        for (i = 0; i < g_norder; i++)
            for (j = 0; j < sn->n; j++)
                if (sn->w[j].id == g_order[i])
                    in[i] = sn->w[j];
        tile_layout(g_layout, &sn->scr, g_gap, g_ratio, g_nmaster, g_norder, in, out);
        lock = LockIBase(0);
        if (screen_alive(scr))
            for (i = 0; i < g_norder; i++)
                if (window_alive(scr, (struct Window *)g_order[i]))
                    win_apply_box((struct Window *)g_order[i], &out[i]);
        UnlockIBase(lock);
        g_retiles++;
    }
    /* monocle: the active window on top, re-checked on every poll so activating another window raises it */
    if (g_layout == TL_MONOCLE && g_norder > 0) {
        long target = g_order[0];
        for (i = 0; i < g_norder; i++)
            if (g_order[i] == sn->active)
                target = sn->active;
        if (target != g_last_raised) {
            lock = LockIBase(0);
            if (screen_alive(scr) && window_alive(scr, (struct Window *)target))
                win_raise((struct Window *)target);
            UnlockIBase(lock);
            g_last_raised = target;
        }
    } else {
        g_last_raised = 0;
    }
    FreeVec(sn);
}

/* ---- ARexx ---------------------------------------------------------------------------------------------------- */

static void set_on(int on)
{
    if (on == g_on)
        return;
    g_on = on;
    state_save();
    if (g_broker)
        ActivateCxObj(g_broker, g_on);   /* Exchange shows Active only while tiling */
    if (g_on) {
        tick(1);
        pt_start(&g_poll, TILE_POLL_MICROS);
    } else {
        pt_stop(&g_poll);
    }
}

static long command(void *ud, const char *cmd, char *out)
{
    int v;
    (void)ud;
    while (*cmd == ' ')
        cmd++;
    out[0] = 0;
    if (starts(cmd, "ON")) {
        if (g_on)
            tick(1);
        else
            set_on(1);
    } else if (starts(cmd, "OFF")) {
        set_on(0);
    } else if (starts(cmd, "TOGGLE")) {
        set_on(!g_on);
    } else if (starts(cmd, "RETILE")) {
        cfg_load();
        if (g_on)
            tick(1);
    } else if (starts(cmd, "LAYOUT")) {
        char w[24];
        int k;
        if (!word_arg(cmd, w, sizeof w))
            return 10;
        if (starts(w, "NEXT"))
            k = tile_layout_step(g_layout, 1);
        else if (starts(w, "PREV"))
            k = tile_layout_step(g_layout, -1);
        else if (!tile_layout_parse(w, &k))
            return 10;
        g_layout = k;
        cfg_save();
        if (g_on)
            tick(1);
    } else if (starts(cmd, "RATIO")) {
        int rel = 0;
        if (!num_arg(cmd, TILE_RATIO_MIN, TILE_RATIO_MAX, &v, &rel))
            return 10;
        g_ratio = rel ? clamp_int(g_ratio + rel * v, TILE_RATIO_MIN, TILE_RATIO_MAX) : v;
        cfg_save();
        if (g_on)
            tick(1);
    } else if (starts(cmd, "NMASTER")) {
        int rel = 0;
        if (!num_arg(cmd, 1, TILE_NMASTER_MAX, &v, &rel))
            return 10;
        g_nmaster = rel ? clamp_int(g_nmaster + rel * v, 1, TILE_NMASTER_MAX) : v;
        cfg_save();
        if (g_on)
            tick(1);
    } else if (starts(cmd, "CYCLE")) {
        char w[8];
        int dir, idx;
        long next;
        struct Screen *scr;
        ULONG lock;
        if (!word_arg(cmd, w, sizeof w))
            return 10;
        if (starts(w, "NEXT"))
            dir = 1;
        else if (starts(w, "PREV"))
            dir = -1;
        else
            return 10;
        if (!g_on || g_norder == 0)
            return 5;
        idx = tile_cycle(g_order, g_norder, active_id(), dir);
        next = g_order[idx];
        scr = front_scr();
        lock = LockIBase(0);
        v = scr && screen_alive(scr) && window_alive(scr, (struct Window *)next);
        if (v)
            win_raise((struct Window *)next);
        UnlockIBase(lock);
        if (!v)
            return 5;
        ActivateWindow((struct Window *)next);
        if (g_layout == TL_MONOCLE)
            g_last_raised = next;
    } else if (starts(cmd, "PROMOTE")) {
        if (!g_on || g_norder == 0 || !tile_promote(g_order, g_norder, active_id()))
            return 5;
        tick(1);
    } else if (starts(cmd, "GAP")) {
        if (!num_arg(cmd, 0, 64, &v, 0))
            return 10;
        g_gap = v;
        if (g_on)
            tick(1);
    } else if (starts(cmd, "STATUS")) {
        char *p = out;
        strcpy(p, g_on ? "on " : "off ");
        p += strlen(p);
        p = put_num(p, g_norder);
        *p++ = ' ';
        p = put_num(p, g_retiles);
        *p++ = ' ';
        strcpy(p, tile_layout_name(g_layout));
        p += strlen(p);
        *p++ = ' ';
        p = put_num(p, g_nmaster);
        *p++ = ' ';
        p = put_num(p, g_ratio);
        *p = 0;
    } else if (starts(cmd, "QUIT")) {
        g_quit = 1;
    } else {
        return 10;
    }
    return 0;
}

/* ---- the broker (cx.c's pattern: one port, Exchange messages) ------------------------------------------------- */

static int broker_open(void)
{
    struct NewBroker nb;
    LONG err = 0;
    if (!(CxBase = OpenLibrary((STRPTR)"commodities.library", 37)))
        return 0;
    if (!(cx_port = CreateMsgPort()))
        return 0;
    memset(&nb, 0, sizeof(nb));
    nb.nb_Version = NB_VERSION;
    nb.nb_Name = (STRPTR)"AmTile";
    nb.nb_Title = (STRPTR)"AmTile window tiling";
    nb.nb_Descr = (STRPTR)"Tiles the UP-Term screen's windows (master and stack)";
    nb.nb_Unique = NBU_UNIQUE | NBU_NOTIFY;
    nb.nb_Flags = 0;
    nb.nb_Pri = 0;
    nb.nb_Port = cx_port;
    g_broker = CxBroker(&nb, &err);
    if (!g_broker)
        return 0;
    ActivateCxObj(g_broker, g_on);
    return 1;
}

static void broker_close(void)
{
    CxMsg *m;
    if (g_broker)
        DeleteCxObjAll(g_broker);
    g_broker = 0;
    if (cx_port) {
        while ((m = (CxMsg *)GetMsg(cx_port)) != 0)
            ReplyMsg((struct Message *)m);
        DeleteMsgPort(cx_port);
    }
    cx_port = 0;
    if (CxBase)
        CloseLibrary(CxBase);
    CxBase = 0;
}

/* Exchange: Disable is OFF, Enable is ON, Kill (and a second broker of the name) is QUIT. */
static void broker_serve(void)
{
    CxMsg *m;
    while ((m = (CxMsg *)GetMsg(cx_port)) != 0) {
        ULONG type = CxMsgType(m);
        ULONG id = CxMsgID(m);
        ReplyMsg((struct Message *)m);
        if (type != CXM_COMMAND)
            continue;
        switch (id) {
        case CXCMD_ENABLE:  set_on(1); break;
        case CXCMD_DISABLE: set_on(0); break;
        case CXCMD_KILL:    g_quit = 1; break;
        case CXCMD_UNIQUE:  g_quit = 1; break;
        }
    }
}

/* ---- main ---------------------------------------------------------------------------------------------------- */

static void shutdown(void)
{
    pt_close(&g_poll);
    broker_close();
    rexx_host_close(&rexx, command, 0);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    IntuitionBase = 0;
}

int main(int argc, char **argv)
{
    char line[64], result[REXX_HOST_OUT];
    int i;
    line[0] = 0;
    for (i = 1; i < argc; i++) {
        if (line[0] && strlen(line) < sizeof line - 2)
            strcat(line, " ");
        strncat(line, argv[i], sizeof line - strlen(line) - 1);
    }
    if (!line[0])
        strcpy(line, "ON");
    /* single instance: a running AmTile gets the word */
    if (rexx_port_exists(PORT_NAME)) {
        long rc = rexx_send(PORT_NAME, line, result, sizeof result);
        if (result[0]) {
            PutStr((STRPTR)result);
            PutStr((STRPTR)"\n");
        }
        return rc < 0 ? 10 : (int)rc;
    }
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    if (!IntuitionBase || !rexx_host_open(&rexx, PORT_NAME) || !broker_open() || !pt_open(&g_poll)) {
        shutdown();
        return 10;
    }
    cfg_load();
    command(0, line, result);
    while (!g_quit) {
        ULONG rx = rexx_host_sigmask(&rexx), cx = 1UL << cx_port->mp_SigBit, tm = pt_sigmask(&g_poll);
        ULONG sigs = Wait(rx | cx | tm | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
            break;
        if (sigs & cx)
            broker_serve();
        if (sigs & rx)
            rexx_host_serve(&rexx, command, 0);
        if (pt_fired(&g_poll, sigs) && g_on) {
            tick(0);
            pt_start(&g_poll, TILE_POLL_MICROS);
        }
    }
    shutdown();
    return 0;
}
