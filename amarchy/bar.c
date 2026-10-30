/* The Amarchy bar, the Amiga half (bar.h; the layout, hit-test and click map are bar_core.c, host-tested).
 * Colours are the drawn menu's roles (menu_theme.h) in the current Omarchy GUI theme, taken as pens when
 * the bar opens and again on a theme switch (themewin.c tw_tick). A tick allocates nothing: bar_set_text compares, and only a changed segment is drawn again. */
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/intuitionbase.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <exec/libraries.h>
#include <proto/dos.h>
#include <graphics/layers.h>
#include <proto/layers.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <libraries/bsdsocket.h>
#include <proto/bsdsocket.h>
#include <string.h>
#include "bar.h"
#include "bar_core.h"
#include "act.h"
#include "tick.h"
#include "cxdesc.h"
#include "barconf_core.h"
#include "proclaunch.h"
#include "poll_timer.h"
#include "themewin.h"
#include "menu_theme.h"
#include "../config/keymap.h"
#include "../config/upconf.h"
#include "../handler/frontscr.h"
#include "../handler/upreq.h"

extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;      /* themewin.c opens and owns it */
struct Library *LayersBase;

#define BAR_PAD 4
#define BAR_GAP 12                /* the least room between two slots (AM-BAR-COLUMNS) */
#define BAR_PANEL_COLS 40
#define BAR_PANEL_ROWS 10
#define BAR_LAYERS_MAX 32      /* layers in front of the bar that one tick looks at */
#define MENU_TITLE "Amarchy"   /* the drawn menu, the viewer and the agents panel (gui.c, win.c) */

static struct Window *g_win;
static struct Screen *g_scr;
static bar g_bar;
static char g_dir[128];
static long g_draws, g_panels, g_launches, g_raises;
static poll_timer g_poll;
static int g_have_layers;
static long g_ticks;
static char g_fmt_clock[32], g_fmt_alt[32];     /* bar.conf clock / clockalt, empty = built in */
static barconf g_conf;                          /* bar.conf: segment order, on/off, tray rows (set by bar_set_conf) */
static int g_conf_set;
static long g_runs;                             /* tray clicks that started a command */
static int g_clock_alt;                         /* the clock segment shows the date until the next click */
static char g_scrname[BAR_MAX_SCREENS][32];     /* full public screen names of the list on the bar, by entry */
/* What the input handler reads (bar_takes_right_press runs in input.device's task, not ours): the bar's box as plain
 * numbers and a live flag that drops before CloseWindow, so the handler never touches a window being closed. The bar
 * is borderless and never moves, so the box taken at open stays right. */
static volatile int g_live, g_top, g_h;

/* The CPU idle counter (R2.8): a process at priority -128 that only counts, so it runs when nothing else is ready. It
 * stops on SIGBREAKF_CTRL_C and tells the opener it is gone with Signal under Forbid, so the opener never reads memory
 * of a process that is still running its tail. Probe numbers: thoughts/shared/research/2026-10-29-amarchy-p2-probes.md. */
#define IDLE_STACK 4096
#define IDLE_CAL_TICKS 25                        /* start-up calibration: half a second of DateStamp ticks */
#define NET_EVERY_TICKS 250UL                    /* the network is asked every 5 s (50 DateStamp ticks per second) */
static volatile unsigned long g_idle_count;
static struct Task *g_idle_parent;
static ULONG g_idle_done_mask;
static struct Process *g_idle_proc;
static BYTE g_idle_sig = -1;
static unsigned long g_cpu_cal, g_cpu_last_count, g_cpu_last_tick, g_net_last_tick;
struct Library *SocketBase;              /* open only while sample_net looks */
static char g_net_name[24];                      /* the interface the segment names */
static int g_net_kind = -1;                      /* BAR_NET_*; -1 = not asked yet */

static void idle_entry(void)
{
    int i;
    for (;;) {
        for (i = 0; i < 256; i++)
            g_idle_count++;
        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C)
            break;
    }
    Forbid();
    Signal(g_idle_parent, g_idle_done_mask);
}

static unsigned long ds_ticks(void)
{
    struct DateStamp d;
    DateStamp(&d);   /* days * 1440 * 3000 wraps mod 2^32: the difference of two readings stays right */
    return ((unsigned long)d.ds_Days * 1440UL + (unsigned long)d.ds_Minute) * 3000UL + (unsigned long)d.ds_Tick;
}

static void idle_start(void)
{
    g_idle_sig = AllocSignal(-1);
    if (g_idle_sig < 0)
        return;
    g_idle_done_mask = 1UL << g_idle_sig;
    g_idle_parent = FindTask(0);
    SetSignal(0, g_idle_done_mask);              /* clear a stale done flag */
    g_idle_count = 0;
    g_idle_proc = CreateNewProcTags(NP_Entry, (ULONG)idle_entry, NP_Priority, -128, NP_StackSize, IDLE_STACK,
                                    NP_Name, (ULONG) "Amarchy bar idle", TAG_DONE);
    if (!g_idle_proc) {
        FreeSignal(g_idle_sig);
        g_idle_sig = -1;
    }
}

static void idle_stop(void)
{
    if (g_idle_proc) {
        SetSignal(0, g_idle_done_mask);              /* clear a stale done flag */
        Signal(&g_idle_proc->pr_Task, SIGBREAKF_CTRL_C);
        Wait(g_idle_done_mask);
        g_idle_proc = 0;
    }
    if (g_idle_sig >= 0) {
        FreeSignal(g_idle_sig);
        g_idle_sig = -1;
    }
}

static int text_w(const char *t);

static void draw_seg(const bar_seg *s)
{
    char t[BAR_TEXT];
    size_t n;
    if (s->hidden || s->w <= 0)
        return;
    n = strlen(s->text);
    memcpy(t, s->text, n + 1);
    /* bar_layout clips a segment to the room left (a long screen title on a narrow bar): cut the text to it too */
    while (n > 0 && text_w(t) > s->w - 2 * g_bar.pad)
        t[--n] = 0;
    tw_fill(g_win, s->x, 0, s->x + s->w - 1, g_win->Height - 1, MT_BG);
    tw_text(g_win, s->x + g_bar.pad, t, s->accent ? MT_SELBOX : MT_TEXT, MT_BG);
    g_draws++;
}

static void paint_all(void);

static int text_w(const char *t)
{
    return TextLength(&g_scr->RastPort, (STRPTR)t, (ULONG)strlen(t));
}

/* AM-BAR-COLUMNS: the widest text a segment can show, measured in the bar font, becomes its fixed slot. The widest
 * digit of the font stands in for every digit (a proportional font has digits of different widths). */
static int widest_digit(void)
{
    char c[2] = { '0', 0 };
    int d, w, best = '0', bw = 0;
    for (d = '0'; d <= '9'; d++) {
        c[0] = (char)d;
        w = text_w(c);
        if (w > bw) {
            bw = w;
            best = d;
        }
    }
    return best;
}

static int widest_clock(const char *fmt, int dg)
{
    int wd, mo, best = 0, w, n = dg - '0';
    char t[BAR_TEXT];
    for (wd = 0; wd < 7; wd++)
        for (mo = 0; mo < 12; mo++) {
            bar_fmt_time(t, sizeof t, fmt, wd, n * 11, mo, n * 1111, n * 11, n * 11);
            w = text_w(t);
            if (w > best)
                best = w;
        }
    return best;
}

/* The net segment's slot for the interface name now known: the widest of its four texts. */
static int net_slot(void)
{
    static const int kinds[4] = { BAR_NET_EMULATED, BAR_NET_STACK, BAR_NET_DOWN, BAR_NET_UP };
    char t[BAR_TEXT];
    int i, w, best = 0;
    for (i = 0; i < 4; i++) {
        bar_fmt_net(t, sizeof t, kinds[i], g_net_name);
        w = text_w(t);
        if (w > best)
            best = w;
    }
    return best;
}

/* CPU, RAM, clock (both formats, bar.conf overrides included) and Network: slots from the formats themselves. */
static void set_slots(void)
{
    int dg = widest_digit(), a, b;
    char t[BAR_TEXT];
    bar_fmt_cpu(t, sizeof t, 100);
    bar_set_slot(&g_bar, BAR_SEG_CPU, text_w(t));
    bar_fmt_ram(t, sizeof t, 4294967295UL, 4294967295UL);
    bar_set_slot(&g_bar, BAR_SEG_RAM, text_w(t));
    a = widest_clock(g_fmt_clock[0] ? g_fmt_clock : BAR_CLOCK_FMT, dg);
    b = widest_clock(g_fmt_alt[0] ? g_fmt_alt : BAR_DATE_FMT, dg);
    bar_set_slot(&g_bar, BAR_SEG_CLOCK, a > b ? a : b);
    bar_set_slot(&g_bar, BAR_SEG_NET, net_slot());
}

static int set_seg(int id, const char *t)
{
    return bar_set_text(&g_bar, id, t, text_w(t));
}

/* The wall clock, as the clock segment shows it (the mode is the click's flip). 1 when the text changed. */
static int sample_clock(void)
{
    struct DateStamp ds;
    char t[BAR_TEXT];
    int wd, d, m, y;
    DateStamp(&ds);
    bar_days_to_date(ds.ds_Days, &wd, &d, &m, &y);
    bar_fmt_time(t, sizeof t, g_clock_alt ? (g_fmt_alt[0] ? g_fmt_alt : BAR_DATE_FMT) : (g_fmt_clock[0] ? g_fmt_clock : BAR_CLOCK_FMT), wd, d, m, y, ds.ds_Minute / 60, ds.ds_Minute % 60);
    return set_seg(BAR_SEG_CLOCK, t);
}

static int sample_ram(void)
{
    char t[BAR_TEXT];
    bar_fmt_ram(t, sizeof t, AvailMem(MEMF_CHIP), AvailMem(MEMF_FAST));
    return set_seg(BAR_SEG_RAM, t);
}

/* CPU load since the last sample, from the idle counter against the best rate seen. 1 when the text changed. */
static int sample_cpu(void)
{
    char t[BAR_TEXT];
    unsigned long now_t, now_c, dc, dt;
    if (!g_idle_proc)
        return 0;
    now_t = ds_ticks();
    now_c = g_idle_count;
    dc = now_c - g_cpu_last_count;
    dt = now_t - g_cpu_last_tick;
    g_cpu_last_count = now_c;
    g_cpu_last_tick = now_t;
    if (!dt || dt > BAR_CPU_MAX_TICKS)
        return 0;
    g_cpu_cal = bar_cpu_cal(g_cpu_cal, dc, dt);
    bar_fmt_cpu(t, sizeof t, bar_cpu_pct(dc, dt, g_cpu_cal));
    return set_seg(BAR_SEG_CPU, t);
}

/* bar_core.h's SM numbers are the Roadshow header's: a drift fails the compile (a negative array size). */
typedef char bar_sm_offline_matches_header[(SM_Offline == BAR_SM_OFFLINE) ? 1 : -1];
typedef char bar_sm_online_matches_header[(SM_Online == BAR_SM_ONLINE) ? 1 : -1];
typedef char bar_sm_down_matches_header[(SM_Down == BAR_SM_DOWN) ? 1 : -1];
typedef char bar_sm_up_matches_header[(SM_Up == BAR_SM_UP) ? 1 : -1];

/* The interface list of the open stack: the first interface that is up, else the first one (name copied into `name`).
 * IFQ_State's ti_Data is a pointer to a LONG. Returns BAR_NET_UP / DOWN, or BAR_NET_STACK when no interface answered. */
static int query_net(char *name, int cap)
{
    struct List *l = ObtainInterfaceList();
    struct Node *nd;
    int best = BAR_NET_STACK;
    name[0] = 0;
    for (nd = l ? l->lh_Head : 0; nd && nd->ln_Succ; nd = nd->ln_Succ) {
        LONG st = -1;
        struct TagItem tags[2];
        int k;
        if (!nd->ln_Name)
            continue;
        tags[0].ti_Tag = IFQ_State;
        tags[0].ti_Data = (ULONG)&st;
        tags[1].ti_Tag = TAG_DONE;
        tags[1].ti_Data = 0;
        if (QueryInterfaceTagList((STRPTR)nd->ln_Name, tags) != 0)
            continue;
        k = bar_net_state_kind((long)st);
        if (k == BAR_NET_STACK || (best != BAR_NET_STACK && best >= k))
            continue;
        best = k;
        strncpy(name, nd->ln_Name, (size_t)cap - 1);
        name[cap - 1] = 0;
    }
    if (l)
        ReleaseInterfaceList(l);
    return best;
}

/* The network, every 5 s: the library is opened for the look and closed at once. A library whose id string starts with
 * "UAE" is the emulator's stub, and any tag in QueryInterfaceTagList kills it, so it is never called. A real stack is
 * asked for its interfaces and their IFQ_State (plan R2.10); the segment is redrawn only when the kind or name changes. */
static int sample_net(int force)
{
    char t[BAR_TEXT], name[sizeof g_net_name];
    unsigned long now = ds_ticks();
    int kind;
    if (!force && g_net_kind >= 0 && now - g_net_last_tick < NET_EVERY_TICKS)
        return 0;
    g_net_last_tick = now;
    name[0] = 0;
    SocketBase = OpenLibrary((STRPTR) "bsdsocket.library", 0);
    kind = SocketBase ? bar_net_kind(SocketBase->lib_IdString ? (const char *)SocketBase->lib_IdString : "") : BAR_NET_NONE;
    if (kind == BAR_NET_STACK)
        kind = query_net(name, sizeof name);
    if (SocketBase) {
        CloseLibrary(SocketBase);
        SocketBase = 0;
    }
    if (kind == g_net_kind && !strcmp(name, g_net_name))
        return 0;
    g_net_kind = kind;
    strcpy(g_net_name, name);
    bar_fmt_net(t, sizeof t, kind, name);
    bar_set_slot(&g_bar, BAR_SEG_NET, net_slot());
    return set_seg(BAR_SEG_NET, t) | 1;
}

/* The public screens, short names; the front one in the accent pen. The list is copied under the lock and released at
 * once, nothing is drawn while it is held. */
static int sample_screens_on(void);
static int sample_screens(void)
{
    if (!g_conf.seg_on[BC_SEG_SCREENS])
        return 0;
    return sample_screens_on();
}

static int sample_screens_on(void)
{
    char shortn[BAR_MAX_SCREENS][BAR_TEXT];
    const char *names[BAR_MAX_SCREENS];
    int tw[BAR_MAX_SCREENS], n = 0, front = -1, i;
    struct List *l = LockPubScreenList();
    struct Node *nd;
    struct Screen *fs = IntuitionBase->FirstScreen;
    for (nd = l ? l->lh_Head : 0; nd && nd->ln_Succ && n < BAR_MAX_SCREENS; nd = nd->ln_Succ) {
        struct PubScreenNode *psn = (struct PubScreenNode *)nd;
        strncpy(g_scrname[n], nd->ln_Name ? nd->ln_Name : "", sizeof g_scrname[n] - 1);
        g_scrname[n][sizeof g_scrname[n] - 1] = 0;
        if (psn->psn_Screen == fs)
            front = n;
        n++;
    }
    UnlockPubScreenList();
    for (i = 0; i < n; i++) {
        bar_fmt_screen(shortn[i], sizeof shortn[i], g_scrname[i]);
        names[i] = shortn[i];
        tw[i] = text_w(shortn[i]);
    }
    return bar_set_screens(&g_bar, n, names, tw, front);
}

/* One sampling pass: the clock every tick, the RAM every second tick (`ram`), the screens every tick. The bar is laid
 * out and repainted whole when anything changed (a segment that grew or shrank moves its neighbours). */
static int sample(int ram_every, int force)
{
    int ch = 0;
    ch |= sample_clock();
    ch |= sample_screens();
    ch |= sample_cpu();
    ch |= sample_net(force);
    if (force || ram_every <= 1 || !(g_ticks % ram_every))
        ch |= sample_ram();
    g_ticks++;
    if (ch)
        bar_layout(&g_bar, g_bar.width);
    return ch;
}

/* The commodity tick (tick_hook): sampling only while the bar is up. */
void bar_tick(long now)
{
    (void)now;
    if (!g_win)
        return;
    if (sample(2, 0))
        paint_all();
}

static void paint_dirty(void)
{
    int i;
    for (i = bar_take_dirty(&g_bar, 0); i >= 0; i = bar_take_dirty(&g_bar, i + 1))
        draw_seg(&g_bar.seg[i]);
}

/* every segment and the gaps between them: first paint and a refresh */
static void paint_all(void)
{
    int i;
    tw_fill(g_win, 0, 0, g_win->Width - 1, g_win->Height - 1, MT_BG);
    for (i = 0; i < g_bar.n; i++)
        g_bar.seg[i].dirty = 1;
    paint_dirty();
}

/* tw_tick retook the pens after a theme switch: the whole bar again, in them */
static void bar_recolour(struct Window *w)
{
    (void)w;
    paint_all();
}

/* The segments in the order and groups bar.conf asks for (barconf_plan); the tray is one segment per run row. */
static void add_planned(void)
{
    static const int side[BC_NGROUPS] = { BAR_LEFT, BAR_CENTRE, BAR_RIGHT };
    bc_place p[BC_NSEGS];
    int n = barconf_plan(&g_conf, p), i, r;
    for (i = 0; i < n; i++) {
        int sd = side[p[i].group];
        switch (p[i].seg) {
        case BC_SEG_SCREENS: bar_add(&g_bar, BAR_SEG_MORE, sd, "", 0); break;
        case BC_SEG_CLOCK: bar_add(&g_bar, BAR_SEG_CLOCK, sd, "", 0); break;
        case BC_SEG_CPU: bar_add(&g_bar, BAR_SEG_CPU, sd, "", 0); break;
        case BC_SEG_NETWORK: bar_add(&g_bar, BAR_SEG_NET, sd, "", 0); break;
        case BC_SEG_RAM: bar_add(&g_bar, BAR_SEG_RAM, sd, "", 0); break;
        case BC_SEG_AGENTS: bar_add(&g_bar, BAR_SEG_AGENTS, sd, "Agents", text_w("Agents")); break;
        case BC_SEG_SYSTEM: bar_add(&g_bar, BAR_SEG_SYSTEM, sd, "System", text_w("System")); break;
        case BC_SEG_TRAY:
            for (r = 0; r < g_conf.nrun; r++)
                bar_add(&g_bar, BAR_SEG_TRAY0 + r, sd, g_conf.run[r].label, text_w(g_conf.run[r].label));
            break;
        default: break;
        }
    }
}

int bar_open(const char *dir)
{
    struct TextFont *font;
    int h;
    if (g_win)
        return 1;
    if (!IntuitionBase)
        return 0;
    if (!tw_libs_open())
        return 0;
    if (!LayersBase) {
        LayersBase = OpenLibrary((STRPTR) "layers.library", 36);
        g_have_layers = LayersBase != 0;
    }
    strncpy(g_dir, dir ? dir : "", sizeof g_dir - 1);
    g_dir[sizeof g_dir - 1] = 0;
    g_scr = LockPubScreen((STRPTR) "Workbench");
    if (!g_scr)
        g_scr = LockPubScreen(0);
    if (!g_scr) {
        tw_libs_close();
        return 0;
    }
    font = g_scr->RastPort.Font;
    h = (font ? font->tf_YSize : 8) + 2;
    bar_init(&g_bar, g_scr->Width, BAR_PAD, BAR_GAP);
    if (!g_conf_set)
        barconf_defaults(&g_conf);
    add_planned();
    set_slots();
    g_clock_alt = 0;
    g_net_kind = -1;
    g_cpu_cal = 0;
    idle_start();
    if (g_idle_proc) {
        /* calibrate: half a second of a mostly idle machine; later samples only ever raise the rate */
        g_cpu_last_tick = ds_ticks();
        g_cpu_last_count = g_idle_count;
        Delay(IDLE_CAL_TICKS);
        {
            unsigned long dt = ds_ticks() - g_cpu_last_tick;
            g_cpu_cal = bar_cpu_cal(0, g_idle_count - g_cpu_last_count, dt);
        }
        g_cpu_last_tick = ds_ticks();
        g_cpu_last_count = g_idle_count;
    }
    sample(2, 1);
    bar_layout(&g_bar, g_scr->Width);
    g_win = tw_open(g_scr, 0, g_scr->Height - h, g_scr->Width, h, BAR_TITLE, IDCMP_MOUSEBUTTONS | IDCMP_REFRESHWINDOW, bar_recolour,
                  0);   /* a plain window, not a backdrop one: a backdrop layer stays behind Workbench's plain root layer and gets no clicks (BAR-CLICK) */
    if (!g_win) {
        idle_stop();
        UnlockPubScreen(0, g_scr);
        g_scr = 0;
        tw_libs_close();
        return 0;
    }
    paint_all();
    g_top = g_win->TopEdge;
    g_h = g_win->Height;
    g_live = 1;
    /* no layers.library or timer: the bar still works, it just is not re-raised */
    if (LayersBase && pt_open(&g_poll))
        pt_start(&g_poll, BAR_RAISE_POLL_MICROS);
    return 1;
}

void bar_close(void)
{
    g_live = 0;
    idle_stop();
    pt_close(&g_poll);
    if (g_win) {
        tw_close(g_win);
        g_win = 0;
    }
    if (g_scr) {
        UnlockPubScreen(0, g_scr);
        g_scr = 0;
    }
    if (g_have_layers) {
        CloseLibrary(LayersBase);
        LayersBase = 0;
        g_have_layers = 0;
    }
    tw_libs_close();
}

unsigned long bar_sigmask(void)
{
    return g_win ? tw_sigmask() : 0;
}

unsigned long bar_timer_sigmask(void)
{
    return g_win ? pt_sigmask(&g_poll) : 0;
}

/* The layers in front of the bar's, front first, under the screen's LayerInfo lock, as plain rectangles for
 * bar_covered. Only reads: the raise itself runs after the unlock (Intuition queues WindowToFront and takes the
 * layer locks itself, so it must not be called with LockLayerInfo held). */
static int front_layers(bar_lyr *out, int max)
{
    struct Layer *l;
    int n = 0;
    LockLayerInfo(&g_scr->LayerInfo);
    for (l = g_scr->LayerInfo.top_layer; l && l != g_win->WLayer && n < max; l = l->back) {
        struct Window *w = (struct Window *)l->Window;
        out[n].l = l->bounds.MinX;
        out[n].t = l->bounds.MinY;
        out[n].r = l->bounds.MaxX;
        out[n].b = l->bounds.MaxY;
        out[n].backdrop = w && bar_is_desktop((unsigned long)w->Flags, l->bounds.MinX, l->bounds.MaxX, g_scr->Width);
        out[n].ours = w && w->Title && !strcmp((const char *)w->Title, MENU_TITLE);
        n++;
    }
    UnlockLayerInfo(&g_scr->LayerInfo);
    return n;
}

/* A tick of the poll timer: raise the bar when a foreign backdrop window (Workbench's own) sits over it; normal
 * windows stay in front of the bar. */
void bar_timer_serve(unsigned long sigs)
{
    bar_lyr lyr[BAR_LAYERS_MAX];
    int n;
    if (!g_win || !g_scr || !pt_fired(&g_poll, sigs))
        return;
    n = front_layers(lyr, BAR_LAYERS_MAX);
    if (bar_covered(g_win->LeftEdge, g_win->TopEdge, g_win->LeftEdge + g_win->Width - 1,
                    g_win->TopEdge + g_win->Height - 1, lyr, n)) {
        WindowToFront(g_win);
        g_raises++;
    }
    pt_start(&g_poll, BAR_RAISE_POLL_MICROS);
}

static void act_panel(const char *node)
{
    char line[KM_ID + 160];
    int text = 0, ax, ay;
    bar_panel_anchor(g_scr->Width, g_win->TopEdge, &ax, &ay);
    g_panels++;
    /* the cold path runs this line, so it names the program (an empty line started nothing) */
    if (!cx_menu_cmd(line, sizeof line, g_dir, KA_MENU, node, UC_MENU_GRAPHICAL, &text))
        line[0] = 0;
    act_menu_open_at(g_dir, 0, KA_MENU, node, line, BAR_PANEL_COLS, BAR_PANEL_ROWS, ax, ay);
}

/* Brings the public screen of list entry `i` to the front: looked up by name at click time, never by a kept pointer. */
static void act_screen_front(int i)
{
    struct Screen *sc;
    if (i < 0 || i >= BAR_MAX_SCREENS || !g_scrname[i][0])
        return;
    sc = LockPubScreen((STRPTR)g_scrname[i]);
    if (!sc)
        return;
    ScreenToFront(sc);
    UnlockPubScreen(0, sc);
}

/* A tray click: the run row's command line through proclaunch, the program (first word) giving the stack cookie. */
static void tray_run(int row)
{
    char prog[BC_CMD];
    int i = 0;
    const char *c;
    if (row < 0 || row >= g_conf.nrun)
        return;
    c = g_conf.run[row].cmd;
    while (c[i] && c[i] != ' ' && c[i] != '\t' && i < (int)sizeof prog - 1) {
        prog[i] = c[i];
        i++;
    }
    prog[i] = 0;
    g_runs++;
    pl_start(c, prog, 262144, "Amarchy tray");
}

/* the same request super+l's MENU_AGENT makes: the handler opens a tab running the preferred agent */
static void act_launch(void)
{
    upreq_body r;
    g_launches++;
    if (upreq_fill(&r, KA_MENUID, BAR_LAUNCH_MENUID))
        upreq_send(&r, 0);
}

void bar_serve(void)
{
    struct Window *w;
    unsigned long cls;
    unsigned code;
    int x, y;
    if (!g_win)
        return;
    while (tw_next(&w, &cls, &code, &x, &y)) {
        if (w != g_win)
            continue;
        if (cls == IDCMP_REFRESHWINDOW) {
            BeginRefresh(g_win);
            paint_all();
            EndRefresh(g_win, TRUE);
        } else if (cls == IDCMP_MOUSEBUTTONS && (code == SELECTDOWN || code == MENUDOWN)) {
            int id;
            switch (bar_click(&g_bar, x, code == SELECTDOWN ? BAR_BTN_LEFT : BAR_BTN_RIGHT, &id)) {
            case BAR_ACT_PANEL: act_panel(BAR_AGENT_NODE); break;
            case BAR_ACT_SYSTEM: act_panel(BAR_SYSTEM_NODE); break;
            case BAR_ACT_CLOCK:
                g_clock_alt = !g_clock_alt;
                if (sample_clock()) {
                    bar_layout(&g_bar, g_bar.width);
                    paint_all();
                }
                break;
            case BAR_ACT_SCREEN: act_screen_front(id - BAR_SEG_SCREEN0); break;
            case BAR_ACT_LAUNCH_AGENT: act_launch(); break;
            case BAR_ACT_RUN: tray_run(id - BAR_SEG_TRAY0); break;
            default: break;
            }
        }
    }
    paint_dirty();
}

/* Runs in the input task: reads the front screen and the pointer, no calls into Intuition. */
int bar_takes_right_press(void)
{
    struct Screen *front;
    if (!g_live || !g_scr || !IntuitionBase)
        return 0;
    front = IntuitionBase->FirstScreen;
    if (front != g_scr)
        return 0;
    return bar_press_act(&g_bar, g_scr->MouseX, g_scr->MouseY, g_top, g_h, BAR_BTN_RIGHT) ==
           BAR_ACT_LAUNCH_AGENT;
}

void bar_right_click(void) { act_launch(); }

void bar_set_conf(const barconf *c)
{
    g_conf = *c;
    g_conf_set = 1;
}

void bar_set_clock_formats(const char *clock, const char *alt)
{
    strncpy(g_fmt_clock, clock, sizeof g_fmt_clock - 1);
    strncpy(g_fmt_alt, alt, sizeof g_fmt_alt - 1);
}

int bar_net(void) { return g_net_kind; }
void bar_text(char *out, int cap) { bar_dump(&g_bar, out, cap); }
long bar_draws(void) { return g_draws; }
long bar_panels(void) { return g_panels; }
long bar_runs(void) { return g_runs; }
long bar_launches(void) { return g_launches; }
long bar_raises(void) { return g_raises; }
int bar_is_open(void) { return g_win != 0; }
