/* The themed window helper, the Amiga half (themewin.h). Pens come from the same path the bar always used:
 * gt_dos_load_current, menu_theme_roles_gui, vr_pen_obtain. */
#include <exec/types.h>
#include <exec/ports.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <string.h>
#include "themewin.h"
#include "gthemes_dos.h"
#include "../render/pens_amiga.h"

struct GfxBase *GfxBase;

static int g_gfx_refs, g_have_gfx;
static struct MsgPort *g_port;
static int g_wins;                       /* open helper windows */
static struct ColorMap *g_cm;
static long g_pen[MT_ROLES];
static int g_pen_got[MT_ROLES];

int tw_libs_open(void)
{
    if (!GfxBase) {
        GfxBase = (struct GfxBase *)OpenLibrary((STRPTR) "graphics.library", 39);
        g_have_gfx = GfxBase != 0;
    }
    if (!GfxBase)
        return 0;
    g_gfx_refs++;
    return 1;
}

void tw_libs_close(void)
{
    if (g_gfx_refs > 0 && --g_gfx_refs == 0 && g_have_gfx) {
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = 0;
        g_have_gfx = 0;
    }
}

static void pens_get(struct ColorMap *cm)
{
    uc_u32 rgb[MT_ROLES];
    gtheme th;
    int r;
    g_cm = cm;
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

long tw_pen(int role)
{
    return role >= 0 && role < MT_ROLES ? g_pen[role] : 1;
}

struct Window *tw_open(struct Screen *scr, int x, int y, int w, int h, const char *title, unsigned long idcmp)
{
    struct Window *win;
    int made_port = 0;
    if (!GfxBase || !scr)
        return 0;
    if (!g_port) {
        g_port = CreateMsgPort();
        if (!g_port)
            return 0;
        made_port = 1;
    }
    if (g_wins == 0)
        pens_get(scr->ViewPort.ColorMap);
    /* no IDCMP at open: the window gets the shared port below, then its classes */
    win = OpenWindowTags(0, WA_Left, x, WA_Top, y, WA_Width, w, WA_Height, h, WA_Title, (ULONG)title, WA_Borderless, TRUE,
                         WA_Activate, FALSE, WA_SimpleRefresh, TRUE, WA_RMBTrap, TRUE, WA_PubScreen, scr, TAG_DONE);
    if (!win) {
        if (g_wins == 0)
            pens_release();
        if (made_port) {
            DeleteMsgPort(g_port);
            g_port = 0;
        }
        return 0;
    }
    win->UserPort = g_port;
    ModifyIDCMP(win, idcmp);
    g_wins++;
    return win;
}

void tw_close(struct Window *win)
{
    struct IntuiMessage *m;
    struct Node *next;
    if (!win)
        return;
    /* Take this window's pending messages off the shared port, detach the port, then close: the usual safe close
     * of a window that shares its UserPort. */
    Forbid();
    for (m = (struct IntuiMessage *)g_port->mp_MsgList.lh_Head; m; m = (struct IntuiMessage *)next) {
        next = m->ExecMessage.mn_Node.ln_Succ;
        if (!next)
            break;
        if (m->IDCMPWindow == win) {
            Remove((struct Node *)m);
            ReplyMsg((struct Message *)m);
        }
    }
    win->UserPort = 0;
    ModifyIDCMP(win, 0);
    Permit();
    CloseWindow(win);
    if (--g_wins == 0) {
        pens_release();
        DeleteMsgPort(g_port);
        g_port = 0;
    }
}

int tw_text_px(struct Window *win, const char *s)
{
    return TextLength(win->RPort, (STRPTR)s, (ULONG)strlen(s));
}

void tw_text(struct Window *win, int x, const char *s, int fg, int bg)
{
    struct RastPort *rp = win->RPort;
    SetAPen(rp, (ULONG)tw_pen(fg));
    SetBPen(rp, (ULONG)tw_pen(bg));
    Move(rp, x, rp->TxBaseline + 1);
    Text(rp, (STRPTR)s, (ULONG)strlen(s));
}

void tw_fill(struct Window *win, int x0, int y0, int x1, int y1, int role)
{
    SetAPen(win->RPort, (ULONG)tw_pen(role));
    RectFill(win->RPort, x0, y0, x1, y1);
}

void tw_frame(struct Window *win, int x0, int y0, int x1, int y1, int role)
{
    struct RastPort *rp = win->RPort;
    SetAPen(rp, (ULONG)tw_pen(role));
    Move(rp, x0, y0);
    Draw(rp, x1, y0);
    Draw(rp, x1, y1);
    Draw(rp, x0, y1);
    Draw(rp, x0, y0);
}

unsigned long tw_sigmask(void)
{
    return g_port ? 1UL << g_port->mp_SigBit : 0;
}

int tw_next(struct Window **win, unsigned long *cls, unsigned *code, int *mx, int *my)
{
    struct IntuiMessage *m;
    if (!g_port || !(m = (struct IntuiMessage *)GetMsg(g_port)))
        return 0;
    *win = m->IDCMPWindow;
    *cls = m->Class;
    *code = m->Code;
    *mx = m->MouseX;
    *my = m->MouseY;
    ReplyMsg((struct Message *)m);
    return 1;
}
