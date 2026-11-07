/* The Amarchy demo tour, OS side: a tick-driven state machine over the scene table of demo_core.c. Scene bodies call
 * the functions the port verbs call (win_port, AmTile's port, bar_port, note_run, shot_screen, gt_dos_preview_send);
 * there is no second implementation of a move, a snap or a bar toggle. Nothing a theme scene does reaches the disk. */
#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <string.h>
#include "demo.h"
#include "demo_core.h"
#include "cx.h"
#include "win.h"
#include "win_core.h"
#include "win_apply.h"
#include "bar.h"
#include "note.h"
#include "shot.h"
#include "rexx_host.h"
#include "gthemes_dos.h"
#include "gthemes_scan_dos.h"
#include "sentinel_dos.h"

extern struct IntuitionBase *IntuitionBase;

#define MAXSNAP 24
#define DEMO_PREFIX "Amarchy demo"

typedef struct snapwin {
    struct Window *w;
    int left, top, width, height;
} snapwin;

/* What the desktop looked like before the demo; applied by stop_demo. */
static struct {
    snapwin win[MAXSNAP];
    int nwin;
    struct Window *active;
    struct Screen *front;
    char theme[GT_NAME];
    int tile;            /* AmTile answered STATUS */
    int tile_on, tile_nmaster, tile_ratio;
    char tile_layout[24];
    int bar_open;
} g_snap;

static demo_clock g_clk;
static int g_arm, g_arm_scene = 1, g_stop_req;
static struct Window *g_dw[2];
static char g_title[2][48];
static long (*g_bar)(const char *args, char *out);
static char g_out[REXX_HOST_OUT];
static gt_list g_themes;
static int g_ntheme, g_cur_theme = -1;

/* ---------- small helpers ---------- */

static void say(const char *title, const char *body, int secs)
{
    char cmd[160], num[8];
    int i = 0;
    strcpy(cmd, "NOTIFY ");
    strncat(cmd, title, 40);
    strcat(cmd, "|");
    strncat(cmd, body, 80);
    strcat(cmd, "|");
    if (secs > 9)
        secs = 9;
    num[i++] = (char)('0' + (secs < 1 ? 1 : secs));
    num[i] = 0;
    strcat(cmd, num);
    note_run(cmd, g_out);
}

static void set_title(const char *scene)
{
    int i;
    for (i = 0; i < 2; i++) {
        strcpy(g_title[i], DEMO_PREFIX " ");
        g_title[i][strlen(g_title[i])] = 0;
        strcat(g_title[i], i ? "B: " : "A: ");
        strncat(g_title[i], scene, 24);
        if (g_dw[i])
            SetWindowTitles(g_dw[i], (UBYTE *)g_title[i], (UBYTE *)-1);
    }
}

/* IBase locked: w is still a window of some screen. */
static int member(const struct Window *w)
{
    const struct Screen *s;
    const struct Window *x;
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
        for (x = s->FirstWindow; x; x = x->NextWindow)
            if (x == w)
                return 1;
    return 0;
}

/* Not part of the desktop the demo gives back: its own windows, and Amarchy's popups (menu, Welcome, Help, viewer), which
 * close when they lose the focus, which the first scene takes. */
static int is_demo_title(const char *t)
{
    return t && (!strncmp(t, "Amarchy", 7) || !strncmp(t, "Welcome to Amarchy", 18));
}

/* Activation is asynchronous: a scene activates a window on one tick and sends the win: verb on the next. */
static void activate(int i)
{
    if (g_dw[i])
        ActivateWindow(g_dw[i]);
}

static void wverb(const char *v) { win_port(v); }

/* ---------- AmTile: its own process, driven through its port ---------- */

static int tile_present(void) { return FindPort((STRPTR) "AMTILE") != 0; }

static long tile_cmd(const char *v)
{
    g_out[0] = 0;
    return rexx_send("AMTILE", v, g_out, (int)sizeof g_out);
}

/* "on|off <n> <retiles> <layout> <nmaster> <ratio> ..." */
static int tile_read(void)
{
    const char *p;
    int n, f = 0;
    if (tile_cmd("STATUS") != 0)
        return 0;
    p = g_out;
    g_snap.tile_on = !strncmp(p, "on", 2);
    while (*p && f < 6) {
        while (*p && *p != ' ')
            p++;
        while (*p == ' ')
            p++;
        f++;
        if (f == 3) {
            for (n = 0; *p && *p != ' ' && n < (int)sizeof g_snap.tile_layout - 1; n++)
                g_snap.tile_layout[n] = *p++;
            g_snap.tile_layout[n] = 0;
        } else if (f == 4 || f == 5) {
            int v = 0;
            while (*p >= '0' && *p <= '9')
                v = v * 10 + (*p++ - '0');
            if (f == 4)
                g_snap.tile_nmaster = v;
            else
                g_snap.tile_ratio = v;
        }
    }
    return 1;
}

static void tile_put_back(void)
{
    char v[48];
    int n;
    if (!g_snap.tile || !tile_present())
        return;
    strcpy(v, "LAYOUT ");
    strcat(v, g_snap.tile_layout);
    tile_cmd(v);
    strcpy(v, "NMASTER ");
    n = g_snap.tile_nmaster;
    {   /* decimal text of a small non-negative number */
        char d[12];
        int k = 0, m = n;
        if (m == 0)
            d[k++] = '0';
        while (m > 0) { d[k++] = (char)('0' + m % 10); m /= 10; }
        while (k > 0) { int l = (int)strlen(v); v[l] = d[--k]; v[l + 1] = 0; }
    }
    tile_cmd(v);
    strcpy(v, "RATIO ");
    n = g_snap.tile_ratio;
    {
        char d[12];
        int k = 0, m = n;
        if (m == 0)
            d[k++] = '0';
        while (m > 0) { d[k++] = (char)('0' + m % 10); m /= 10; }
        while (k > 0) { int l = (int)strlen(v); v[l] = d[--k]; v[l + 1] = 0; }
    }
    tile_cmd(v);
    tile_cmd(g_snap.tile_on ? "ON" : "OFF");
}

/* ---------- snapshot and restore ---------- */

static void snapshot(void)
{
    ULONG lock;
    struct Screen *s;
    struct Window *w;
    memset(&g_snap, 0, sizeof g_snap);
    gt_dos_current(g_snap.theme, (int)sizeof g_snap.theme);
    g_snap.bar_open = bar_is_open();
    if (tile_present())
        g_snap.tile = tile_read();
    lock = LockIBase(0);
    g_snap.active = IntuitionBase->ActiveWindow;
    g_snap.front = IntuitionBase->FirstScreen;
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
        for (w = s->FirstWindow; w && g_snap.nwin < MAXSNAP; w = w->NextWindow) {
            win_box b;
            win_screen sc;
            snapwin *e;
            if (win_is_amarchy((const char *)w->Title) || is_demo_title((const char *)w->Title))
                continue;
            win_box_of(w, &b, &sc);
            e = &g_snap.win[g_snap.nwin++];
            e->w = w;
            e->left = b.left;
            e->top = b.top;
            e->width = b.width;
            e->height = b.height;
        }
    UnlockIBase(lock);
}

/* 1 when every snapshot window is still there. */
static int snapshot_whole(void)
{
    ULONG lock = LockIBase(0);
    int i, ok = 1;
    for (i = 0; i < g_snap.nwin && ok; i++)
        ok = member(g_snap.win[i].w);
    UnlockIBase(lock);
    return ok;
}

static void open_windows(void)
{
    int i;
    for (i = 0; i < 2; i++) {
        struct TagItem tags[] = {
            { WA_Left, 40 + 60 * i }, { WA_Top, 60 + 40 * i }, { WA_Width, 260 }, { WA_Height, 110 },
            { WA_Title, (ULONG)g_title[i] }, { WA_DragBar, TRUE }, { WA_DepthGadget, TRUE }, { WA_SizeGadget, TRUE },
            { WA_SmartRefresh, TRUE }, { WA_Activate, i == 0 }, { WA_MinWidth, 100 }, { WA_MinHeight, 50 },
            { WA_MaxWidth, 0xFFFF }, { WA_MaxHeight, 0xFFFF }, { TAG_DONE, 0 }
        };
        strcpy(g_title[i], DEMO_PREFIX);
        g_dw[i] = OpenWindowTagList(0, tags);
    }
}

static void close_windows(void)
{
    int i;
    for (i = 0; i < 2; i++)
        if (g_dw[i]) {
            CloseWindow(g_dw[i]);
            g_dw[i] = 0;
        }
}

static void stop_demo(void)
{
    ULONG lock;
    int i;
    cx_watch_input(0);
    tile_put_back();
    if (bar_is_open() != g_snap.bar_open && g_bar)
        g_bar(g_snap.bar_open ? " ON" : " OFF", g_out);
    gt_dos_desk_restore();
    gt_dos_term_send(g_snap.theme);
    close_windows();
    lock = LockIBase(0);
    for (i = 0; i < g_snap.nwin; i++) {
        snapwin *e = &g_snap.win[i];
        if (member(e->w)) {                       /* a window that is gone is skipped */
            win_box b;
            win_screen sc;
            win_box_of(e->w, &b, &sc);
            b.left = e->left;
            b.top = e->top;
            b.width = e->width;
            b.height = e->height;
            win_apply_box(e->w, &b);
        }
    }
    UnlockIBase(lock);
    lock = LockIBase(0);
    if (g_snap.active && member(g_snap.active))
        ActivateWindow(g_snap.active);
    {
        const struct Screen *s;
        int there = 0;
        for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
            if (s == g_snap.front)
                there = 1;
        if (there && g_snap.front != IntuitionBase->FirstScreen)
            ScreenToFront(g_snap.front);
    }
    UnlockIBase(lock);
    demo_stop(&g_clk);
    g_cur_theme = -1;
    am_sentinel("AMARCHY_DEMO_DONE", g_clk.loops);
}

/* ---------- scenes: fn(t seconds into the scene, first) ---------- */

static void sc_windows(long t, int first)
{
    if (first) { set_title("Windows"); activate(0); }
    if (t == 1) wverb("move right 100");
    if (t == 3) wverb("move down 60");
    if (t == 5) wverb("size right 80");
    if (t == 7) wverb("zip");
    if (t == 9) wverb("zip");
    if (t == 11) wverb("move left 100");
}

static void sc_snap(long t, int first)
{
    if (first) { set_title("Snap"); activate(0); }
    if (t == 1) wverb("snap left");
    if (t == 3) wverb("snap right");
    if (t == 5) wverb("snap tl");
    if (t == 7) wverb("snap br");
    if (t == 9) wverb("snap center");
}

static void sc_focus(long t, int first)
{
    if (first) { set_title("Focus and cycle"); activate(1); }
    if (t == 1) wverb("snap right");
    if (t == 2) activate(0);
    if (t == 3) wverb("snap left");
    if (t == 4 || t == 6) wverb("cycle next");
    if (t == 8) wverb("focus right");
    if (t == 9) wverb("focus left");
}

static const char *const g_layouts[] = { "master-stack", "grid", "columns", "monocle", "dwindle" };

static void sc_tile(long t, int first)
{
    if (first) {
        set_title("Tiling");
        if (!tile_present()) {
            say("Tiling", "AmTile is not running: start it to see the layouts", 5);
            demo_next(&g_clk);
            return;
        }
    }
    if (t == 0) tile_cmd("ON");
    if (t >= 2 && t <= 10 && (t % 2) == 0) {
        char v[40];
        strcpy(v, "LAYOUT ");
        strcat(v, g_layouts[(t - 2) / 2]);
        tile_cmd(v);
    }
    if (t == 12) tile_read();                       /* STATUS on a later tick: the tiler answered */
    if (t == 13) tile_put_back();
}

static void theme_step(int idx)
{
    if (g_ntheme > 0) {
        gt_dos_preview_send(g_themes.name[idx % g_ntheme]);
        set_title(g_themes.name[idx % g_ntheme]);
    }
}

static void themes_load(void)
{
    int i;
    gt_dos_scan(&g_themes);
    g_ntheme = g_themes.n;
    g_cur_theme = 0;
    for (i = 0; i < g_ntheme; i++)
        if (!strcmp(g_themes.name[i], g_snap.theme))
            g_cur_theme = i;
}

static void sc_themes(long t, int first)
{
    if (first) { set_title("Themes"); themes_load(); }
    if (t == 1 || t == 4 || t == 7 || t == 10)
        theme_step(g_cur_theme + 1 + (int)(t / 3));
    if (t == 13) { gt_dos_desk_restore(); gt_dos_term_send(g_snap.theme); }
}

static void sc_look(long t, int first)
{
    if (first) {
        set_title("Desktop look");
        if (g_ntheme == 0)
            themes_load();
    }
    if (t == 1 || t == 4 || t == 7)
        theme_step(g_ntheme + g_cur_theme - 1 - (int)(t / 3));
    if (t == 9) { gt_dos_desk_restore(); gt_dos_term_send(g_snap.theme); }
}

static void sc_bar(long t, int first)
{
    if (first) set_title("Status bar");
    if (!g_bar)
        return;
    if (t == 1) g_bar(" OFF", g_out);
    if (t == 4) g_bar(" ON", g_out);
    if (t == 7) g_bar(" TOGGLE", g_out);
    if (t == 10) g_bar(g_snap.bar_open ? " ON" : " OFF", g_out);
}

static void sc_notes(long t, int first)
{
    if (first) set_title("Notices and clips");
    if (t == 1) say("Amarchy notices", "A notice appears for a few seconds, then goes", 3);
    if (t == 4) say("Clipboard history", "The last 20 clips stay one menu away", 3);
    if (t == 7) say("Posting", "Any program can post one through ARexx", 3);
}

static void sc_cap(long t, int first)
{
    if (first) set_title("Capture");
    if (t == 2) {
        if (shot_screen(shot_default_dir()) == 0) {
            say("Capture", "The whole screen was saved as an IFF picture", 4);
            DeleteFile((STRPTR)shot_last());      /* the demo leaves no file behind */
        }
    }
}

static void sc_launch(long t, int first)
{
    if (first) set_title("Launcher and packages");
    if (t == 1) say("Launcher", "The menu key opens apps, packages and themes", 4);
    if (t == 6) say("Help", "Amarchy help lists every key; Amarchy demo plays this tour", 4);
}

typedef struct scene_def {
    void (*fn)(long t, int first);
} scene_def;

/* One row per demo_core.c row, same order (the names and seconds live there so the host test sees them). */
static const scene_def g_scene[DEMO_SCENES] = {
    { sc_windows }, { sc_snap }, { sc_focus }, { sc_tile }, { sc_themes },
    { sc_look }, { sc_bar }, { sc_notes }, { sc_cap }, { sc_launch },
};

/* ---------- entry ---------- */

void demo_set_bar(long (*bar)(const char *args, char *out)) { g_bar = bar; }

int demo_port(const char *verb, char *out, int cap)
{
    int n;
    demo_clock shown = g_clk;
    switch (demo_parse(verb, &n)) {
    case DV_START:
        g_arm = 1;
        g_arm_scene = n;
        g_stop_req = 0;
        if (!shown.running)
            demo_start(&shown, n);
        break;
    case DV_STOP:
        if (g_clk.running || g_arm)
            g_stop_req = 1;
        break;
    case DV_STATUS:
        if (g_arm && !shown.running)            /* armed, first tick not yet run: the demo is starting, say so */
            demo_start(&shown, g_arm_scene);
        break;
    default:
        out[0] = 0;
        return 10;
    }
    if (g_stop_req)
        demo_stop(&shown);
    demo_status_text(&shown, out, cap);
    return 0;
}

void demo_tick(long secs)
{
    int s, t, first;
    (void)secs;
    if (g_stop_req) {
        g_stop_req = 0;
        g_arm = 0;
        if (g_clk.running)
            stop_demo();
        return;
    }
    if (g_arm) {
        g_arm = 0;
        if (!g_clk.running) {
            snapshot();
            set_title("Starting");
            open_windows();
            cx_watch_input(1);
        }
        demo_start(&g_clk, g_arm_scene);        /* a start while running jumps to that scene (loops reset) */
    }
    if (!g_clk.running)
        return;
    if (cx_input_seen() || !snapshot_whole()) {
        am_sentinel("AMARCHY_DEMO_WHY", cx_input_seen() ? cx_input_info() : 2);   /* 1000 + key code, 2000 + mouse code, 2 a snapshot window gone */
        stop_demo();
        return;
    }
    s = demo_step(&g_clk, &t, &first);
    if (s < 0)
        return;
    if (first)
        am_sentinel("AMARCHY_DEMO_SCENE", s + 1);
    g_scene[s].fn(t, first);
}

void demo_shutdown(void)
{
    if (g_clk.running)
        stop_demo();
}
