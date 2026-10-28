/* AmTile: the autotiling commodity (plan 2026-10-28-amarchy-autotile.md, phase 2). tile_core.c does every number
 * (what tiles, the order, the master-stack boxes); this file watches the UP-Term screen, hands the windows to
 * tile_core and applies the boxes.
 *
 * One instance: the ARexx port AMTILE. A second start sends its command-line word to that port and exits with
 * its return code. Commands: ON OFF TOGGLE RETILE RATIO <n> GAP <n> STATUS QUIT. STATUS answers
 * "on|off <tiled windows> <retile count>"; the retile count rises each time boxes are applied (the rig's
 * sentinel that this code ran). While on, a timer.device request polls the screen every TILE_POLL_MICROS; a
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
#include "win_apply.h"

#define PORT_NAME      "AMTILE"
#define SCREEN_NAME    "UP-Term"      /* the terminal screen's default public name (vtcon_handler.c, PUBSCREEN) */
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
static struct MsgPort *cx_port, *tm_port;
static CxObj *g_broker;
static struct timerequest *tm_req;
static int tm_open, tm_busy;
static int g_on, g_quit;
static int g_gap = TILE_GAP_DEFAULT;   /* the tile_gap config key is phase 3 (P3-3) */
static int g_ratio = TILE_RATIO_DEFAULT;
static long g_order[TILE_MAX];
static int g_norder;
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

/* The number after the first word, in lo..hi. */
static int num_arg(const char *cmd, int lo, int hi, int *out)
{
    long v = 0;
    int any = 0;
    while (*cmd && *cmd != ' ')
        cmd++;
    while (*cmd == ' ')
        cmd++;
    for (; *cmd >= '0' && *cmd <= '9'; cmd++, any = 1)
        v = v * 10 + (*cmd - '0');
    if (!any || *cmd || v < lo || v > hi)
        return 0;
    *out = (int)v;
    return 1;
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

static int timer_open(void)
{
    if (!(tm_port = CreateMsgPort()))
        return 0;
    if (!(tm_req = (struct timerequest *)CreateIORequest(tm_port, sizeof(struct timerequest))))
        return 0;
    if (OpenDevice((STRPTR)"timer.device", UNIT_VBLANK, (struct IORequest *)tm_req, 0))
        return 0;
    tm_open = 1;
    return 1;
}

static void timer_stop(void)
{
    if (tm_busy) {
        AbortIO((struct IORequest *)tm_req);
        WaitIO((struct IORequest *)tm_req);
        tm_busy = 0;
    }
}

static void timer_start(void)
{
    timer_stop();
    if (!tm_open)
        return;
    tm_req->tr_node.io_Command = TR_ADDREQUEST;
    tm_req->tr_time.tv_secs = TILE_POLL_MICROS / 1000000;
    tm_req->tr_time.tv_micro = TILE_POLL_MICROS % 1000000;
    SendIO((struct IORequest *)tm_req);
    tm_busy = 1;
}

static void timer_close(void)
{
    timer_stop();
    if (tm_open)
        CloseDevice((struct IORequest *)tm_req);
    tm_open = 0;
    if (tm_req)
        DeleteIORequest((struct IORequest *)tm_req);
    tm_req = 0;
    if (tm_port)
        DeleteMsgPort(tm_port);
    tm_port = 0;
}

/* ---- the screen walk and the apply ------------------------------------------------------------------------------ */

/* The UP-Term public screen, 0 when there is none. Only the pointer is kept: it is checked against IBase's screen
 * list under the lock before it is used. */
static struct Screen *term_screen(void)
{
    struct Screen *s = LockPubScreen((STRPTR)SCREEN_NAME);
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
} snap;

/* IBase locked: the screen's windows copied into sn. */
static void walk(const struct Screen *scr, snap *sn)
{
    const struct Window *x;
    sn->n = 0;
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

/* One look at the screen. force: retile even when the tiled set is unchanged. */
static void tick(int force)
{
    struct Screen *scr = term_screen();
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
    nold = g_norder;
    memcpy(old, g_order, sizeof(long) * (size_t)nold);
    g_norder = tile_order_update(g_order, g_norder, set, m);
    changed = g_norder != nold || memcmp(old, g_order, sizeof(long) * (size_t)nold) != 0;
    if ((changed || force) && g_norder > 0) {
        for (i = 0; i < g_norder; i++)
            for (j = 0; j < sn->n; j++)
                if (sn->w[j].id == g_order[i])
                    in[i] = sn->w[j];
        tile_master_stack(&sn->scr, g_gap, g_ratio, g_norder, in, out);
        lock = LockIBase(0);
        if (screen_alive(scr))
            for (i = 0; i < g_norder; i++)
                if (window_alive(scr, (struct Window *)g_order[i]))
                    win_apply_box((struct Window *)g_order[i], &out[i]);
        UnlockIBase(lock);
        g_retiles++;
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
        timer_start();
    } else {
        timer_stop();
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
        if (g_on)
            tick(1);
    } else if (starts(cmd, "RATIO")) {
        if (!num_arg(cmd, TILE_RATIO_MIN, TILE_RATIO_MAX, &v))
            return 10;
        g_ratio = v;
        if (g_on)
            tick(1);
    } else if (starts(cmd, "GAP")) {
        if (!num_arg(cmd, 0, 64, &v))
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
    timer_close();
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
    if (!IntuitionBase || !rexx_host_open(&rexx, PORT_NAME) || !broker_open() || !timer_open()) {
        shutdown();
        return 10;
    }
    command(0, line, result);
    while (!g_quit) {
        ULONG rx = rexx_host_sigmask(&rexx), cx = 1UL << cx_port->mp_SigBit, tm = 1UL << tm_port->mp_SigBit;
        ULONG sigs = Wait(rx | cx | tm | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
            break;
        if (sigs & cx)
            broker_serve();
        if (sigs & rx)
            rexx_host_serve(&rexx, command, 0);
        /* the bit can be left over from an AbortIO in timer_stop: only a finished request is a tick */
        if (tm_busy && (sigs & tm) && CheckIO((struct IORequest *)tm_req)) {
            WaitIO((struct IORequest *)tm_req);
            tm_busy = 0;
            if (g_on) {
                tick(0);
                timer_start();
            }
        }
    }
    shutdown();
    return 0;
}
