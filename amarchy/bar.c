/* The Amarchy bar, the Amiga half (bar.h; the layout, hit-test and click map are bar_core.c, host-tested).
 * Colours are the drawn menu's roles (menu_theme.h) in the current Omarchy GUI theme, taken once as pens when
 * the bar opens. A tick allocates nothing: bar_set_text compares, and only a changed segment is drawn again. */
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/layers.h>
#include <proto/layers.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <string.h>
#include "bar.h"
#include "bar_core.h"
#include "act.h"
#include "cxdesc.h"
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
#define BAR_GAP 2
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
/* What the input handler reads (bar_takes_right_press runs in input.device's task, not ours): the bar's box as plain
 * numbers and a live flag that drops before CloseWindow, so the handler never touches a window being closed. The bar
 * is borderless and never moves, so the box taken at open stays right. */
static volatile int g_live, g_top, g_h;

static void draw_seg(const bar_seg *s)
{
    tw_fill(g_win, s->x, 0, s->x + s->w - 1, g_win->Height - 1, MT_BG);
    tw_text(g_win, s->x + g_bar.pad, s->text, MT_TEXT, MT_BG);
    g_draws++;
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
    bar_add(&g_bar, BAR_SEG_AGENTS, BAR_RIGHT, "Agents", TextLength(&g_scr->RastPort, (STRPTR) "Agents", 6));
    bar_layout(&g_bar, g_scr->Width);
    g_win = tw_open(g_scr, 0, g_scr->Height - h, g_scr->Width, h, BAR_TITLE, IDCMP_MOUSEBUTTONS | IDCMP_REFRESHWINDOW);
    if (!g_win) {
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
        out[n].ours = w && w->Title && !strcmp((const char *)w->Title, MENU_TITLE);
        n++;
    }
    UnlockLayerInfo(&g_scr->LayerInfo);
    return n;
}

/* A tick of the poll timer: raise the bar when a foreign window sits over it. */
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

static void act_panel(void)
{
    char line[KM_ID + 160];
    int text = 0, ax, ay;
    bar_panel_anchor(g_scr->Width, g_win->TopEdge, &ax, &ay);
    g_panels++;
    /* the cold path runs this line, so it names the program (an empty line started nothing) */
    if (!cx_menu_cmd(line, sizeof line, g_dir, KA_MENU, BAR_AGENT_NODE, UC_MENU_GRAPHICAL, &text))
        line[0] = 0;
    act_menu_open_at(g_dir, 0, KA_MENU, BAR_AGENT_NODE, line, BAR_PANEL_COLS, BAR_PANEL_ROWS, ax, ay);
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
            case BAR_ACT_PANEL: act_panel(); break;
            case BAR_ACT_LAUNCH_AGENT: act_launch(); break;
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

long bar_draws(void) { return g_draws; }
long bar_panels(void) { return g_panels; }
long bar_launches(void) { return g_launches; }
long bar_raises(void) { return g_raises; }
int bar_is_open(void) { return g_win != 0; }
