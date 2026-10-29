/* The Amarchy bar, the Amiga half (bar.h; the layout, hit-test and click map are bar_core.c, host-tested).
 * Colours are the drawn menu's roles (menu_theme.h) in the current Omarchy GUI theme, taken once as pens when
 * the bar opens. A tick allocates nothing: bar_set_text compares, and only a changed segment is drawn again. */
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <string.h>
#include "bar.h"
#include "bar_core.h"
#include "act.h"
#include "cxdesc.h"
#include "gthemes_dos.h"
#include "menu_theme.h"
#include "../config/keymap.h"
#include "../config/upconf.h"
#include "../handler/frontscr.h"
#include "../handler/upreq.h"
#include "../render/pens_amiga.h"

extern struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

#define BAR_PAD 4
#define BAR_GAP 2
#define BAR_PANEL_COLS 40
#define BAR_PANEL_ROWS 10

static struct Window *g_win;
static struct Screen *g_scr;
static struct ColorMap *g_cm;
static bar g_bar;
static long g_pen[MT_ROLES];
static int g_pen_got[MT_ROLES];
static int g_have_gfx;
static char g_dir[128];
static long g_draws, g_panels, g_launches;
/* What the input handler reads (bar_takes_right_press runs in input.device's task, not ours): the bar's box as plain
 * numbers and a live flag that drops before CloseWindow, so the handler never touches a window being closed. The bar
 * is borderless and never moves, so the box taken at open stays right. */
static volatile int g_live, g_top, g_h;

static void pens_get(void)
{
    uc_u32 rgb[MT_ROLES];
    gtheme th;
    int r;
    gt_dos_load_current(&th);
    menu_theme_roles_gui(&th, rgb);
    for (r = 0; r < MT_ROLES; r++) {
        long p = vr_pen_obtain(g_cm, rgb[r]);
        g_pen_got[r] = p >= 0;
        g_pen[r] = p >= 0 ? p : (r == MT_BG ? 0 : 1);
    }
}

static void pens_release(void)
{
    int r;
    for (r = 0; r < MT_ROLES; r++)
        if (g_pen_got[r]) {
            ReleasePen(g_cm, (ULONG)g_pen[r]);
            g_pen_got[r] = 0;
        }
}

static int text_px(struct RastPort *rp, const char *s)
{
    return TextLength(rp, (STRPTR)s, (ULONG)strlen(s));
}

static void draw_seg(const bar_seg *s)
{
    struct RastPort *rp = g_win->RPort;
    int h = g_win->Height;
    int base = rp->TxBaseline + 1;
    SetAPen(rp, (ULONG)g_pen[MT_BG]);
    RectFill(rp, s->x, 0, s->x + s->w - 1, h - 1);
    SetAPen(rp, (ULONG)g_pen[MT_TEXT]);
    SetBPen(rp, (ULONG)g_pen[MT_BG]);
    Move(rp, s->x + g_bar.pad, base);
    Text(rp, (STRPTR)s->text, (ULONG)strlen(s->text));
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
    struct RastPort *rp = g_win->RPort;
    int i;
    SetAPen(rp, (ULONG)g_pen[MT_BG]);
    RectFill(rp, 0, 0, g_win->Width - 1, g_win->Height - 1);
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
    if (!GfxBase) {
        GfxBase = (struct GfxBase *)OpenLibrary((STRPTR) "graphics.library", 39);
        g_have_gfx = GfxBase != 0;
    }
    if (!GfxBase)
        return 0;
    strncpy(g_dir, dir ? dir : "", sizeof g_dir - 1);
    g_dir[sizeof g_dir - 1] = 0;
    g_scr = LockPubScreen((STRPTR) "Workbench");
    if (!g_scr)
        g_scr = LockPubScreen(0);
    if (!g_scr)
        return 0;
    g_cm = g_scr->ViewPort.ColorMap;
    font = g_scr->RastPort.Font;
    h = (font ? font->tf_YSize : 8) + 2;
    pens_get();
    bar_init(&g_bar, g_scr->Width, BAR_PAD, BAR_GAP);
    bar_add(&g_bar, BAR_SEG_AGENTS, BAR_RIGHT, "Agents", text_px(&g_scr->RastPort, "Agents"));
    bar_layout(&g_bar, g_scr->Width);
    g_win = OpenWindowTags(0, WA_Left, 0, WA_Top, g_scr->Height - h, WA_Width, g_scr->Width, WA_Height, h,
                           WA_Borderless, TRUE, WA_Activate, FALSE, WA_SimpleRefresh, TRUE, WA_RMBTrap, TRUE,
                           WA_PubScreen, g_scr, WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_REFRESHWINDOW, TAG_DONE);
    if (!g_win) {
        pens_release();
        UnlockPubScreen(0, g_scr);
        g_scr = 0;
        return 0;
    }
    paint_all();
    g_top = g_win->TopEdge;
    g_h = g_win->Height;
    g_live = 1;
    return 1;
}

void bar_close(void)
{
    g_live = 0;
    if (g_win) {
        CloseWindow(g_win);
        g_win = 0;
    }
    if (g_scr) {
        pens_release();
        UnlockPubScreen(0, g_scr);
        g_scr = 0;
    }
    if (g_have_gfx) {
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = 0;
        g_have_gfx = 0;
    }
}

unsigned long bar_sigmask(void)
{
    return g_win ? 1UL << g_win->UserPort->mp_SigBit : 0;
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
    struct IntuiMessage *m;
    if (!g_win)
        return;
    while ((m = (struct IntuiMessage *)GetMsg(g_win->UserPort)) != 0) {
        ULONG cls = m->Class;
        UWORD code = m->Code;
        WORD x = m->MouseX;
        ReplyMsg((struct Message *)m);
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
int bar_is_open(void) { return g_win != 0; }
