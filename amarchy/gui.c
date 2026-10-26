/* amarchy-gui: the drawn Amarchy menu and key viewer (plan 2026-10-23-amarchy-drawn-menu.md, G5). A vbcc
 * Intuition program, no ixemul. Same command line as amarchy-menu, so the commodity switches only the name:
 *     amarchy-gui menu [node]      amarchy-gui viewer
 * Rows and walking come from the one keymap (keymap_load, menu_core), the rectangles from menu_geom.c, the
 * colours from the Omarchy GUI theme (Style > Theme, AM_GTHEME: gthemes_core.c via menu_theme_roles_gui, pens_get), a leaf is run by leaf.c.
 * Modes in the one window: list, input (a leaf's value), message (a refusal), viewer (the [bind] lines).
 * Every full draw bumps the global variable AMARCHY_DRAWS: the sentinel tools/rig/amarchy_gui_rig.py (G7)
 * reads; AMARCHY_ROWS lists the drawn rows' labels; AMARCHY_ICON_LOADS beside it counts the .info images icon_gfx.c loaded (plan app-index, I3). */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/var.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <graphics/layers.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <stdlib.h>
#include <string.h>
#include "menu_core.h"
#include "index_core.h"
#include "ixwatch.h"
#include "menu_geom.h"
#include "menu_theme.h"
#include "leaf.h"
#include "gthemes_dos.h"
#include "menu_open.h"
#include "menu_icon.h"
#include "menu_font.h"
#include "menu_text.h"
#include "../prefs/prefs_core.h"
#include "icon_core.h"
#include "../config/upconf_lex.h"
#include "icon_gfx.h"
#include "../config/keymap_load.h"
#include "../config/upconf.h"
#include "amconf_dos.h"
#include "../handler/frontscr.h"
#include "../handler/upreq.h"
#include "../handler/upactive.h"
#include "../render/pens_amiga.h"
#include "../render/amiga_render.h"
#include "../tty/qmark.h"
#include "keymap_default.inc"

/* vbcc's startup runs main on this much, and the cookie is what vsh, the Shell and Workbench read from the
 * file to start it on that much. tests/test_amarchy_stack.py reads both from the built binary. */
#define GUI_STACK 65536
#define GUI_STR2(x) #x
#define GUI_STR(x) GUI_STR2(x)
long __stack = GUI_STACK;
const char amarchy_gui_stack_cookie[] = "$STACK: " GUI_STR(GUI_STACK);

#define USAGE "usage: amarchy-gui menu [node] | viewer\n"
#define MAX_LINES 200
#define EDIT_MAX 80
#define PUT_MAX 384          /* the longest label put() cuts */

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

enum { M_LIST, M_INPUT, M_MSG, M_VIEW };
enum { K_NONE, K_CHAR, K_ESC, K_ENTER, K_BS, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_PGUP, K_PGDN };

typedef struct gui {
    struct Screen *scr;
    struct Window *win;
    struct RastPort *rp;
    menu_text *mt;               /* every label's font: TrueType through aa_ink or bitmap (menu_text.c), closed at exit */
    struct ColorMap *cm;
    mg_font mf;
    mg_geom g;
    int w;                       /* the window's width, every level */
    mg_screen ms;                /* the screen */
    int chev_w;
    int icon_w, icon_h;          /* the icon column's width and the icon's height, 0 for none */
    long pen[MT_ROLES];
    int pens;                    /* distinct pens obtained (2 in the minimal mode) */
    char pen_got[MT_ROLES];
    int mode;
    int await;                   /* ChangeWindowBox sent, IDCMP_CHANGEWINDOW not yet seen */
    int active_seen;
    int stop;                    /* the running event loop ends */
    int quit;                    /* the window is going: every loop ends */
    long draws;
    struct BitMap *bb;           /* the back buffer every draw goes to, else 0 (g->rp is the window's) */
    struct RastPort brp;         /* its rastport */
    int bw, bh;                  /* its size */
    int nest;                    /* draws running: the outermost one blits */
    long blits;                  /* back buffer to window copies */
    long blit_rate;              /* beam lines a row's copy took per 65536 pixels, 0 = not measured */
    int frame;                   /* beam lines in a frame */
    long beam_waits;             /* blits held back until the beam is clear of their box */
    long tears;                  /* blits the beam crossed half copied, from the time each took */
    long late;                   /* blits that took longer than estimated */
    /* list */
    char node[KM_ID * 2], filter[KM_LABEL];
    mn_item *it;                 /* one level's items, ix_level_cap(&ixs) of them */
    int *idx, cap, count;
    long *chip;                  /* per item of it: the 0xRRGGBB colour chip of a colour row, else -1 (lf_setting_rechip) */
    int sel, top;
    ix_watcher wt;               /* the scan this window watches; its status row is entry 0 while wt.row is set */
    int shift;                   /* 1 when entry 0 is the status row (never selectable), else 0 */
    int ticks;                   /* IDCMP_INTUITICKS since the last poll */
    int ticking;                 /* IDCMP_INTUITICKS is on */
    char ixlabel[KM_LABEL];
    char ixpath[IX_PATH];        /* the index row to launch (its tool or project), else empty */
    int chosen;                  /* index into keymap.menu of the leaf to run, else -1 */
    /* viewer and message: lines of text */
    const char *ln[MAX_LINES];
    int ll[MAX_LINES], nlines;
    const char *head;            /* viewer/message/input header text */
    /* input */
    char edit[EDIT_MAX];
    int elen, edone;
} gui;

static keymap km;
static ix_index ixs;
static int menu_cond;            /* decided once at open: mn_cond(terminal present, DOPUS.1 present, WORKBENCH present) */
static gui G;
static char view_text[KM_MAX_BIND * 100];
static char msg_text[UPREQ_ANSWER + 140];
static char prompt_text[UPREQ_ANSWER + 80];

/* ---- sentinel ----------------------------------------------------------------------------------------- */

/* Publishes n as the global variable name: a rig case reads it after the run. */
static void sentinel_publish(const char *name, long n)
{
    char b[16];
    int i = 15;
    b[i] = 0;
    do {
        b[--i] = (char)('0' + n % 10);
        n /= 10;
    } while (n && i > 0);
    SetVar((STRPTR) name, (STRPTR)(b + i), -1, GVF_GLOBAL_ONLY);
}

/* Publishes the antialiased glyphs drawn so far as AMARCHY_AA_GLYPHS: 0 for a bitmap font, so a rig reads
 * whether the TrueType path ran. */
static void sentinel_aa(const gui *g)
{
    sentinel_publish("AMARCHY_AA_GLYPHS", mt_aa_glyphs(g->mt));
}

/* Publishes the labels of the rows the list shows, joined by '|', as AMARCHY_ROWS: the rig reads which
 * rows were drawn (a menu's text is pixels, no uitree gadget). */
static void sentinel_rows(const gui *g)
{
    char b[256];
    int i, n = 0;
    b[0] = 0;
    for (i = g->shift; i < g->count; i++) {
        const char *l = g->it[g->idx[i - g->shift]].label;
        int len = (int)strlen(l);
        if (n + len + 2 >= (int)sizeof b)
            break;
        if (i > g->shift)
            b[n++] = '|';
        memcpy(b + n, l, len);
        n += len;
        b[n] = 0;
    }
    SetVar((STRPTR) "AMARCHY_ROWS", (STRPTR)b, -1, GVF_GLOBAL_ONLY);
}

/* ---- text and fills ----------------------------------------------------------------------------------- */

static int text_len(const char *s, void *ctx)
{
    return mt_width((menu_text *)ctx, s);
}

/* the pen's colour as 0xRRGGBB: what menu_text draws a role in (a bitmap font finds this pen again) */
static ULONG role_rgb(const gui *g, int role)
{
    ULONG c[3];
    GetRGB32(g->cm, (ULONG)g->pen[role], 1, c);
    return ((c[0] >> 24) << 16) | ((c[1] >> 24) << 8) | (c[2] >> 24);
}

static void fill(gui *g, int role, int x0, int y0, int x1, int y1)
{
    if (x1 < x0 || y1 < y0)
        return;
    SetAPen(g->rp, (ULONG)g->pen[role]);
    RectFill(g->rp, x0, y0, x1, y1);
}

/* A text in a box of height boxh whose top is y, cut to maxw pixels with an ellipsis, in role over the box's
 * background role bgrole (the caller has filled the box in it). */
static void put(gui *g, int role, int bgrole, int x, int y, int boxh, const char *s, int len, int maxw)
{
    char buf[PUT_MAX], out[PUT_MAX];
    if (len <= 0 || maxw <= 0)
        return;
    if (len > PUT_MAX - 1)
        len = PUT_MAX - 1;
    memcpy(buf, s, (size_t)len);
    buf[len] = 0;
    mt_fit(g->mt, buf, maxw, out, sizeof out);
    if (!out[0])
        return;
    mt_draw(g->mt, g->rp, x, y + (boxh - mt_height(g->mt)) / 2, out, role_rgb(g, role), role_rgb(g, bgrole));
    SetDrMd(g->rp, JAM1);   /* a bitmap font's draw left JAM2 */
}

/* ---- back buffer -------------------------------------------------------------------------------------- */

/* Every draw goes to an offscreen bitmap and reaches the window in one blit when the outermost draw ends, so
 * the window never shows a row half drawn (cleared, then box, icon, text): drawn on the window, a moving
 * selection flickered. When the bitmap cannot be had, g->rp stays the window's rastport. */
static void buffer_fit(gui *g)
{
    int w = g->g.w, h = g->g.h + g->g.m.row_h;   /* a row of room below: the peek row's text runs past the list */
    if (g->bb && g->bw >= w && g->bh >= h)
        return;
    if (g->bb) {
        WaitBlit();
        FreeBitMap(g->bb);
    }
    g->bb = AllocBitMap((ULONG)w, (ULONG)h, g->scr->RastPort.BitMap->Depth, BMF_MINPLANES | BMF_CLEAR,
                        g->scr->RastPort.BitMap);
    g->rp = g->win->RPort;
    if (!g->bb)
        return;
    g->bw = w;
    g->bh = h;
    InitRastPort(&g->brp);
    g->brp.BitMap = g->bb;
    g->rp = &g->brp;
    SetDrMd(g->rp, JAM1);
}

static void paint_begin(gui *g)
{
    if (!g->nest++)
        buffer_fit(g);
}

/* The beam line the window-relative line y is shown on. */
static int beam_line(gui *g, int y)
{
    struct ViewPort *vp = &g->scr->ViewPort;
    int l = g->win->TopEdge + y;
    if (vp->Modes & LACE)
        l /= 2;
    return l + vp->DyOffset + (GfxBase->ActiView ? GfxBase->ActiView->DyOffset : 0);
}

/* The beam line now; a frame's length grows to the longest line seen. */
static int beam_now(gui *g)
{
    int b = (int)VBeamPos();
    if (b >= g->frame)
        g->frame = b + 1;
    return b;
}

/* The back buffer's window-relative box x0,y0 - x1,y1 (a row or less high) to the window. The copy is one blit
 * per plane (the A1200's 8-plane Workbench: a row takes half a frame), so the beam must stay off the box from
 * the first plane to the last, or it shows lines with mixed planes (the white lines of 2026-10-25): the copy
 * starts where the beam is off the box and stays off until the copy is done (mg_blit_safe), from the time the
 * last copies took. */
static void copy_box(gui *g, int x0, int y0, int x1, int y1)
{
    long area;
    int top, h, est, b0, b, d, laps = 0;
    area = (long)(x1 - x0 + 1) * (y1 - y0 + 1);
    top = beam_line(g, y0);
    h = beam_line(g, y1) - top + 1;
    est = (int)(g->blit_rate * area / 65536);
    WaitBlit();                                 /* the back buffer's drawing is done */
    b = b0 = beam_now(g);
    if (!mg_blit_safe(b, top, h, g->frame, est)) {
        g->beam_waits++;
        while (!mg_blit_safe(b, top, h, g->frame, est) && laps < 2) {
            d = beam_now(g);
            if (d < b)
                laps++;                         /* a frame wrapped: no clear start in two is not waited for */
            b = d;
        }
    }
    b0 = b;
    BltBitMapRastPort(g->bb, x0, y0, g->win->RPort, x0, y0, x1 - x0 + 1, y1 - y0 + 1, 0xC0);
    WaitBlit();
    d = beam_now(g) - b0;
    if (d < 0)
        d += g->frame;
    if (!mg_blit_safe(b0, top, h, g->frame, d > 0 ? d : 1))
        g->tears++;                             /* from the time this copy took, not the estimate */
    if (est && d > est)
        g->late++;
    g->blit_rate = mg_blit_rate(g->blit_rate, (long)d * 65536 / area);
    g->blits++;
}

/* The end of a draw that changed the window-relative box x0,y0 - x1,y1: copied a row at a time (mg_blit_strip),
 * as a taller box has no start the beam cannot cross. */
static void paint_end(gui *g, int x0, int y0, int x1, int y1)
{
    int i, s0, s1;
    if (--g->nest || !g->bb || x1 < x0 || y1 < y0)
        return;
    for (i = 0; mg_blit_strip(y0, y1, g->g.m.row_h, i, &s0, &s1); i++)
        copy_box(g, x0, s0, x1, s1);
}

static void buffer_free(gui *g)
{
    if (g->bb) {
        WaitBlit();
        FreeBitMap(g->bb);
    }
    g->bb = 0;
}

/* ---- drawing ------------------------------------------------------------------------------------------ */

static int shown_rows(const gui *g)
{
    return g->g.visible + (mg_more(&g->g, g->top) ? 1 : 0);
}

/* menu-style = minimal: text rows, no border, no icon column, two pens */
static int minimal;

static void draw_border(gui *g)
{
    const mg_metrics *m = &g->g.m;
    if (minimal)
        return;
    fill(g, MT_BORDER_ROLE, 0, 0, g->g.w - 1, m->by - 1);
    fill(g, MT_BORDER_ROLE, 0, g->g.h - m->by, g->g.w - 1, g->g.h - 1);
    fill(g, MT_BORDER_ROLE, 0, m->by, m->bx - 1, g->g.h - m->by - 1);
    fill(g, MT_BORDER_ROLE, g->g.w - m->bx, m->by, g->g.w - 1, g->g.h - m->by - 1);
}

static void draw_header(gui *g)
{
    char h[KM_ID + KM_LABEL + 4];
    const char *s = g->head;
    const mg_geom *q = &g->g;
    fill(g, MT_BG, q->head_x, q->head_y, q->head_x + q->head_w - 1, q->head_y + q->m.head_h - 1);
    if (g->mode == M_LIST) {
        mg_header(g->node, h, sizeof h);
        if (g->filter[0]) {
            strcat(h, ": ");
            strcat(h, g->filter);
        }
        s = h;
    }
    put(g, MT_DIM, MT_BG, q->head_x + q->m.row_pad, q->head_y, q->m.head_h, s, (int)strlen(s),
        q->head_w - 2 * q->m.row_pad);
}

/* The icon of the list item `it` at node (draw_row calls it for the shown rows only, so icons load only
 * for visible rows): its coverage, icon_w x icon_h, at (x, y) in the row of height
 * boxh. Strong coverage in the row's colour, a weaker one in the dim pen; the rest is the row's box. */
/* amarchy-app-icons = amiga: the index rows show their .info images; anything else keeps generic glyphs */
static int app_icons;      /* UC_ICONS_*: the key amarchy-app-icons */

static void draw_icon(gui *g, int role, const mn_item *it, int x, int y, int boxh)
{
    static vt_u8 cov[MI_MAX_W * MI_MAX];
    char id[KM_ID * 2 + 2];
    const vt_u8 *glyph;
    int s = g->icon_w, sh = g->icon_h, px, py, x0;
    if (MN_IS_INDEX(it)) {
        /* an app or game of the index: its own .info image when the upconf asks for them and one can be
         * had; else the generic glyph of the level the row is shown in ("apps" when that level has none) */
        char path[IX_PATH];
        if (app_icons == UC_ICONS_AMIGA && ix_icon_path(&ixs, it, path, sizeof path) &&
            ig_draw(g->rp, path, x, y + (boxh - sh) / 2, s, sh))
            return;
    }
    mi_item_id(g->node, it, id, sizeof id);
    glyph = mi_find(id);
    if (!glyph || !mi_render(glyph, s, sh, cov))
        return;
    y += (boxh - sh) / 2;
    for (py = 0; py < sh; py++)
        for (px = 0; px < s;) {
            int lv = cov[py * s + px] >= 170 ? 2 : (cov[py * s + px] >= 64 ? 1 : 0);
            x0 = px;
            while (px < s && (cov[py * s + px] >= 170 ? 2 : (cov[py * s + px] >= 64 ? 1 : 0)) == lv)
                px++;
            if (lv)
                fill(g, lv == 2 || role != MT_TEXT ? role : MT_DIM, x + x0, y + py, x + px - 1, y + py);
        }
}

/* One row of the current mode, entry e (0-based among all entries), when it is shown. */
static void draw_row(gui *g, int e)
{
    const mg_geom *q = &g->g;
    int i = e - g->top, y, bottom, sel, rbg, tx, tw;
    if (g->await || e < 0 || e >= q->n || i < 0 || i >= shown_rows(g))
        return;
    y = mg_row_y(q, i);
    bottom = y + q->m.row_h - 1;
    if (bottom > q->list_y + q->list_h - 1)
        bottom = q->list_y + q->list_h - 1;
    sel = (g->mode == M_LIST && e == g->sel) || g->mode == M_INPUT;
    rbg = sel ? MT_SELBOX : MT_BG;
    paint_begin(g);
    fill(g, sel ? MT_SELBOX : MT_BG, q->list_x, y, q->list_x + q->list_w - 1, bottom);
    tx = q->list_x + q->m.row_pad;
    tw = q->list_w - 2 * q->m.row_pad;
    if (g->mode == M_LIST && e < g->shift) {      /* the scan's status row */
        put(g, MT_DIM, rbg, tx, y, q->m.row_h, g->wt.row, (int)strlen(g->wt.row), tw);
    } else if (g->mode == M_LIST) {
        const mn_item *it = &g->it[g->idx[e - g->shift]];
        int role = sel ? MT_ACCENT : MT_TEXT;
        int ic = g->icon_w ? g->icon_w + q->m.icon_gap : 0;
        if (ic) {
            draw_icon(g, role, it, tx, y, q->m.row_h);
            tx += ic;
            tw -= ic;
        }
        if (minimal && MN_IS_GROUP(it)) {         /* the group marker is part of the text: label then " >" */
            char t[KM_LABEL + 4];
            strncpy(t, it->label, KM_LABEL);
            t[KM_LABEL] = 0;
            strcat(t, " >");
            put(g, role, rbg, tx, y, q->m.row_h, t, (int)strlen(t), tw);
        } else {
            long rgb = g->chip[it - g->it];
            if (rgb >= 0) {                       /* a colour row: a filled chip at the right edge, label left of it */
                int cs = q->m.row_h - 4, cx = tx + tw - cs, p = vr_pen_obtain(g->cm, (unsigned long)rgb);
                if (p >= 0) {
                    SetAPen(g->rp, (ULONG)g->pen[MT_TEXT]);
                    RectFill(g->rp, cx - 1, y + 1, cx + cs, y + 2 + cs);        /* border in the text colour */
                    SetAPen(g->rp, (ULONG)p);
                    RectFill(g->rp, cx, y + 2, cx + cs - 1, y + 1 + cs);
                    ReleasePen(g->cm, (ULONG)p);
                }
                tw -= cs + 4 + q->m.row_pad;
            }
            put(g, role, rbg, tx, y, q->m.row_h, it->label, (int)strlen(it->label), tw);
        }
        if (MN_IS_GROUP(it) && !minimal)
            put(g, role, rbg, q->list_x + q->list_w - q->m.row_pad - g->chev_w, y, q->m.row_h, ">", 1, g->chev_w);
    } else if (g->mode == M_INPUT) {
        char t[EDIT_MAX + 2];
        memcpy(t, g->edit, (size_t)g->elen);
        t[g->elen] = '_';
        put(g, MT_ACCENT, rbg, tx, y, q->m.row_h, t, g->elen + 1, tw);
    } else
        put(g, MT_TEXT, rbg, tx, y, q->m.row_h, g->ln[e], g->ll[e], tw);
    paint_end(g, q->list_x, y, q->list_x + q->list_w - 1, bottom);
}

/* The rows area (not a full draw: the sentinel counts those). */
static void draw_list(gui *g)
{
    const mg_geom *q = &g->g;
    int e;
    if (g->await)
        return;
    paint_begin(g);
    fill(g, MT_BG, q->list_x, q->list_y, q->list_x + q->list_w - 1, q->list_y + q->list_h - 1);
    for (e = g->top; e < g->top + shown_rows(g); e++)
        draw_row(g, e);
    /* the peek row's text runs past the list: put the padding and the border back */
    fill(g, MT_BG, q->m.bx, q->list_y + q->list_h, q->w - q->m.bx - 1, q->h - q->m.by - 1);
    draw_border(g);
    paint_end(g, 0, 0, g->g.w - 1, g->g.h - 1);
}

static void draw_full(gui *g)
{
    if (g->await)
        return;
    paint_begin(g);
    fill(g, MT_BG, 0, 0, g->g.w - 1, g->g.h - 1);
    draw_border(g);
    draw_header(g);
    draw_list(g);
    paint_end(g, 0, 0, g->g.w - 1, g->g.h - 1);
    g->draws++;
    sentinel_publish("AMARCHY_DRAWS", g->draws);
    sentinel_publish("AMARCHY_BLITS", g->blits);
    sentinel_publish("AMARCHY_BEAM_WAITS", g->beam_waits);
    sentinel_publish("AMARCHY_TEARS", g->tears);
    sentinel_publish("AMARCHY_LATE", g->late);
    sentinel_aa(g);
    if (g->mode == M_LIST)
        sentinel_rows(g);
    sentinel_publish("AMARCHY_ICON_LOADS", ig_loads());
    sentinel_publish("AMARCHY_MODE", minimal ? UC_MENU_MINIMAL : UC_MENU_GRAPHICAL);
    sentinel_publish("AMARCHY_PENS", g->pens);
    sentinel_publish("AMARCHY_ICON_RESTORED", ig_restored());
}

/* ---- geometry ----------------------------------------------------------------------------------------- */

/* The window takes the layout of n entries: a changed box is sent to Intuition and drawn after its
 * IDCMP_CHANGEWINDOW; an unchanged one is drawn at once. */
static void relayout(gui *g, int n)
{
    struct Window *w = g->win;
    mg_layout(&g->g, &g->mf, g->w, n, &g->ms);
    if (w->LeftEdge != g->g.x || w->TopEdge != g->g.y || w->Width != g->g.w || w->Height != g->g.h) {
        g->await = 1;
        ChangeWindowBox(w, g->g.x, g->g.y, g->g.w, g->g.h);
    } else
        draw_full(g);
}

/* The level for the node and filter; the cursor goes to the entry labelled keep when it is still shown, else
 * the first real entry. */
static int keep_setting = -1;   /* the SDI_* id of the setting row the cursor stays on after a set, else -1 */

static void list_rebuild_keep(gui *g, const char *keep)
{
    int n = ix_level_t(&km, &ixs, g->node, g->filter, g->it, g->cap, menu_cond);
    int pos;
    lf_setting_relabel(g->it, n);       /* a settings row shows its value */
    lf_setting_rechip(g->it, n, g->chip);
    g->shift = g->wt.row[0] ? 1 : 0;
    g->count = mn_filter(g->it, n, g->filter, g->idx, g->cap) + g->shift;
    pos = keep_setting >= 0 ? mn_pick_setting(g->it, g->idx, g->count - g->shift, keep_setting)
                            : mn_pick(g->it, g->idx, g->count - g->shift, keep);
    keep_setting = -1;
    g->sel = g->shift + (pos > 0 ? pos : 0);
    g->top = 0;
    if (pos > 0) {                       /* the layout (visible rows) is needed to scroll to it */
        mg_layout(&g->g, &g->mf, g->w, g->count, &g->ms);
        g->top = mg_scroll(&g->g, g->sel, 0);
    }
    relayout(g, g->count);
}

static void list_rebuild(gui *g)
{
    list_rebuild_keep(g, 0);
}

static void set_sel(gui *g, int s)
{
    int old = g->sel, top = g->top;
    if (g->count <= g->shift)
        return;
    if (s < g->shift)
        s = g->shift;
    if (s >= g->count)
        s = g->count - 1;
    g->sel = s;
    g->top = mg_scroll(&g->g, s, top);
    if (g->top != top) {
        draw_list(g);
    } else if (s != old) {
        draw_row(g, s);         /* the new row first: a frame between the copies shows two selected, never none */
        draw_row(g, old);
        sentinel_publish("AMARCHY_BEAM_WAITS", g->beam_waits);
        sentinel_publish("AMARCHY_TEARS", g->tears);
        sentinel_publish("AMARCHY_LATE", g->late);
    sentinel_aa(g);
        sentinel_publish("AMARCHY_BLITS", g->blits);
    }
}

/* viewer and message: the first shown line moves, there is no selection */
static void scroll_lines(gui *g, int d)
{
    int top = g->top + d, max = g->g.n - g->g.visible;
    if (top > max)
        top = max;
    if (top < 0)
        top = 0;
    if (top != g->top) {
        g->top = top;
        draw_list(g);
    }
}

/* ---- events ------------------------------------------------------------------------------------------- */

static void go_up(gui *g)
{
    char up[KM_ID * 2];
    if (mn_parent(g->node, up, sizeof up)) {
        strcpy(g->node, up);
        g->filter[0] = 0;
        list_rebuild(g);
    } else
        g->quit = g->stop = 1;
}

static int gui_ask_value(void *ctx, const char *label, const char *usage, char *out, int n);
static void gui_refused(void *ctx, const char *label, long rc, const char *answer);

/* IDCMP_INTUITICKS only while a scan is watched: the window is a launcher, ticks come while it is in use. */
static void watch_idcmp(gui *g)
{
    int want = g->wt.on;
    if (want == g->ticking || !g->win)
        return;
    g->ticking = want;
    g->ticks = 0;
    ModifyIDCMP(g->win, want ? g->win->IDCMPFlags | IDCMP_INTUITICKS : g->win->IDCMPFlags & ~IDCMP_INTUITICKS);
}

/* The extra music and demo drawer names of Amarchy's file (ix_row_kind), set before every index load. */
static void index_drawers(void)
{
    am_prefs ap;
    amconf_load(&ap);
    ix_set_drawers(ap.val[AM_MUSIC], ap.val[AM_DEMO]);
    ix_set_hidden_drawers(ap.val[AM_HIDE]);
    app_icons = amconf_icons(&ap);
    minimal = amconf_style(&ap) == UC_MENU_MINIMAL;
}

/* The scan finished: read the new index (the Apps level has new rows). 0 when memory ran out. */
static int reload_index(gui *g)
{
    free(g->it);
    free(g->idx);
    free(g->chip);
    g->chip = 0;
    g->it = 0;
    g->idx = 0;
    ix_free(&ixs);
    index_drawers();
    ix_load_file(&km, &ixs, IX_CACHE_PATH);
    lf_load_listers(&ixs);
    gt_dos_load_rows(&ixs);
    g->cap = ix_level_cap(&ixs);
    g->it = malloc(g->cap * sizeof *g->it);
    g->chip = malloc(g->cap * sizeof *g->chip);
    g->idx = malloc(g->cap * sizeof *g->idx);
    if (g->it && g->idx)
        return 1;
    g->quit = g->stop = 1;
    return 0;
}

/* One poll of the record; a changed row redraws the list, DONE also reloads the index. */
static void watch_poll(gui *g)
{
    char keep[KM_LABEL];
    int done;
    if (!ixw_poll(&g->wt))
        return;
    sentinel_publish("watch", g->wt.state);
    done = g->wt.state == IXW_DONE;
    keep[0] = 0;
    if (g->sel >= g->shift && g->sel < g->count)
        strcpy(keep, g->it[g->idx[g->sel - g->shift]].label);
    if (done && !reload_index(g))
        return;
    list_rebuild_keep(g, keep);
    watch_idcmp(g);
}

/* A scan: leaf: run it at once, the window stays and shows the scan. */
static void start_scan(gui *g, const mn_item *it)
{
    lf_ui ui;
    ix_prog p0;
    char keep[KM_LABEL];
    int alive0 = 0, have0 = ixw_snapshot(&p0, &alive0);   /* the run counter before the launch */
    ui.ask_value = gui_ask_value;
    ui.refused = gui_refused;
    ui.ctx = g;
    ui.self_in_mru = 0;
    ui.win = g->win;
    if (!ix_watcher_adopt_live(&g->wt, have0, &p0, alive0)) {    /* one scanner at a time: show the live one */
        if (lf_run(&km.menu[it->row], &ui) != LF_WATCH)
            return;
        ix_watcher_begin(&g->wt, 1, have0, &p0, alive0);
    }
    sentinel_publish("watch", g->wt.state);
    strcpy(keep, it->label);             /* the rebuild overwrites it */
    list_rebuild_keep(g, keep);
    watch_idcmp(g);
}

/* Enter or Right on a settings row. Enter on a check or cycle row sets the next value; Right on a cycle row
 * opens its submenu of values (MN_ROW_CHOICE rows, the current one marked); Enter on a value sets it and goes
 * back. The level is rebuilt with the new label. */
static void activate_setting(gui *g, const mn_item *it, int right)
{
    lf_ui ui;
    ui.ask_value = gui_ask_value;
    ui.refused = gui_refused;
    ui.ctx = g;
    ui.self_in_mru = 0;
    ui.win = g->win;
    if (MN_IS_SETTING(it) && (right || SD_HAS_PICKER(&sd_settings[it->ix]) || SD_HAS_LIST(&sd_settings[it->ix]))) {
        if (sd_settings[it->ix].kind == SD_CYCLE || SD_HAS_PICKER(&sd_settings[it->ix]) || SD_HAS_LIST(&sd_settings[it->ix])) {
            mn_child_node(g->node, it, g->node, sizeof g->node);
            g->filter[0] = 0;
            list_rebuild(g);
        }
        return;
    }
    {
        mn_item row = *it;                  /* the rebuild below overwrites g->it */
        if (!lf_setting_enter(&row, &ui))
            return;
        if (MN_IS_CHOICE(&row)) {
            go_up(g);                       /* the pick is made: back to the page */
            return;
        }
        keep_setting = row.ix;
        list_rebuild(g);
    }
}

static void activate(gui *g, int right)
{
    const mn_item *it;
    if (g->sel < g->shift || g->sel >= g->count)
        return;
    it = &g->it[g->idx[g->sel - g->shift]];
    if (MN_IS_INDEX(it)) {
        if (ix_launch_path(&ixs, it, g->ixpath, sizeof g->ixpath)) {
            strncpy(g->ixlabel, it->label, sizeof g->ixlabel - 1);
            g->stop = 1;
        }
        return;
    }
    if (MN_IS_SETTING(it) || MN_IS_CHOICE(it)) {
        activate_setting(g, it, right);
        return;
    }
    if (MN_IS_GROUP(it)) {
        mn_child_node(g->node, it, g->node, sizeof g->node);
        g->filter[0] = 0;
        list_rebuild(g);
    } else if (lf_plan(km.menu[it->row].kind) == LF_SCAN) {
        start_scan(g, it);
    } else {
        g->chosen = it->row;
        g->stop = 1;
    }
}

static void key_list(gui *g, int k, int ch)
{
    int l = (int)strlen(g->filter);
    switch (k) {
    case K_ESC:
        if (mn_back(0, g->filter, g->node) == MN_BACK_CLEAR_FILTER) {
            g->filter[0] = 0;
            list_rebuild(g);
        } else
            go_up(g);
        break;
    case K_LEFT:
        go_up(g);
        break;
    case K_ENTER:
        activate(g, 0);
        break;
    case K_RIGHT:
        activate(g, 1);
        break;
    case K_UP:
        set_sel(g, g->sel - 1);
        break;
    case K_DOWN:
        set_sel(g, g->sel + 1);
        break;
    case K_PGUP:
        set_sel(g, g->sel - g->g.visible);
        break;
    case K_PGDN:
        set_sel(g, g->sel + g->g.visible);
        break;
    case K_BS:
        if (mn_back(1, g->filter, g->node) == MN_BACK_DROP_CHAR) {
            g->filter[l - 1] = 0;
            list_rebuild(g);
        } else
            go_up(g);                       /* no filter: back, as Escape */
        break;
    case K_CHAR:
        if (l < (int)sizeof g->filter - 1) {
            g->filter[l] = (char)ch;
            g->filter[l + 1] = 0;
            list_rebuild(g);
        }
        break;
    }
}

static void key_input(gui *g, int k, int ch)
{
    switch (k) {
    case K_ESC:
        g->elen = 0;
        g->stop = 1;
        break;
    case K_ENTER:
        g->edone = 1;
        g->stop = 1;
        break;
    case K_BS:
        if (g->elen) {
            g->elen--;
            draw_row(g, 0);
        }
        break;
    case K_CHAR:
        if (g->elen < EDIT_MAX - 1) {
            g->edit[g->elen++] = (char)ch;
            draw_row(g, 0);
        }
        break;
    }
}

static void key_view(gui *g, int k)
{
    switch (k) {
    case K_ESC:
        g->quit = g->stop = 1;
        break;
    case K_UP:
        scroll_lines(g, -1);
        break;
    case K_DOWN:
        scroll_lines(g, 1);
        break;
    case K_PGUP:
        scroll_lines(g, -g->g.visible);
        break;
    case K_PGDN:
        scroll_lines(g, g->g.visible);
        break;
    }
}

static void key(gui *g, int k, int ch, int wheel)
{
    if (g->await)
        return;
    if (wheel) {
        if (g->mode == M_LIST)
            set_sel(g, g->sel + wheel);
        else if (g->mode == M_VIEW)
            scroll_lines(g, wheel * 3);
        return;
    }
    switch (g->mode) {
    case M_LIST:
        key_list(g, k, ch);
        break;
    case M_INPUT:
        key_input(g, k, ch);
        break;
    case M_MSG:
        g->stop = 1;                        /* any key closes */
        break;
    default:
        key_view(g, k);
    }
}

static void raw_key(gui *g, int code)
{
    switch (code) {
    case 0x4C: key(g, K_UP, 0, 0); break;
    case 0x4D: key(g, K_DOWN, 0, 0); break;
    case 0x4E: key(g, K_RIGHT, 0, 0); break;
    case 0x4F: key(g, K_LEFT, 0, 0); break;
    case 0x48: key(g, K_PGUP, 0, 0); break;
    case 0x49: key(g, K_PGDN, 0, 0); break;
    case 0x7A: key(g, K_NONE, 0, -1); break;     /* wheel up */
    case 0x7B: key(g, K_NONE, 0, 1); break;      /* wheel down */
    }
}

static void vanilla_key(gui *g, int c)
{
    if (c == 27)
        key(g, K_ESC, 0, 0);
    else if (c == 13 || c == 10)
        key(g, K_ENTER, 0, 0);
    else if (c == 8 || c == 127)
        key(g, K_BS, 0, 0);
    else if (c >= 32 && c < 127)
        key(g, K_CHAR, c, 0);
}

static void mouse_button(gui *g, int code, int mx, int my)
{
    int h;
    if (g->await)
        return;
    if (code == MENUDOWN) {                    /* right button: back, as Escape without a filter */
        if (g->mode == M_LIST)
            go_up(g);
        else if (g->mode == M_VIEW)
            g->quit = g->stop = 1;
        else
            g->stop = 1;
        return;
    }
    if (code != SELECTDOWN)
        return;
    if (g->mode == M_MSG) {
        g->stop = 1;
        return;
    }
    if (g->mode != M_LIST)
        return;
    h = mg_hit(&g->g, g->top, mx, my);
    if (h >= g->shift) {
        set_sel(g, h);
        activate(g, 0);
    }
}

static void mouse_move(gui *g, int mx, int my)
{
    int h;
    if (g->await || g->mode != M_LIST)
        return;
    h = mg_hit(&g->g, g->top, mx, my);
    if (h >= 0 && h < g->top + g->g.visible && h != g->sel)
        set_sel(g, h);
}

/* Runs until g->stop (this mode is done) or g->quit (the window goes). */
static void event_loop(gui *g)
{
    struct IntuiMessage *im;
    while (!g->stop && !g->quit) {
        ULONG got = Wait((1UL << g->win->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C);
        if (got & SIGBREAKF_CTRL_C) {          /* MENU-TOGGLE: the menu hotkey pressed again */
            g->quit = g->stop = 1;
            break;
        }
        while (!g->stop && (im = (struct IntuiMessage *)GetMsg(g->win->UserPort)) != 0) {
            ULONG cls = im->Class;
            UWORD code = im->Code;
            WORD mx = im->MouseX, my = im->MouseY;
            ReplyMsg((struct Message *)im);
            switch (cls) {
            case IDCMP_VANILLAKEY:
                vanilla_key(g, code);
                break;
            case IDCMP_RAWKEY:
                if (!(code & IECODE_UP_PREFIX))
                    raw_key(g, code);
                break;
            case IDCMP_MOUSEBUTTONS:
                mouse_button(g, code, mx, my);
                break;
            case IDCMP_MOUSEMOVE:
                mouse_move(g, mx, my);
                break;
            case IDCMP_CHANGEWINDOW:
                if (g->await) {
                    g->await = 0;
                    draw_full(g);
                }
                break;
            case IDCMP_INTUITICKS:          /* about 10 a second: poll the scan every 10th */
                if (g->wt.on && ++g->ticks >= 10) {
                    g->ticks = 0;
                    watch_poll(g);
                }
                break;
            case IDCMP_ACTIVEWINDOW:
                g->active_seen = 1;
                break;
            case IDCMP_INACTIVEWINDOW:      /* a launcher: another window taking the focus closes it */
                if (g->active_seen)
                    g->quit = g->stop = 1;
                break;
            }
        }
    }
}

/* ---- screen, font, pens, window ---------------------------------------------------------------------- */

static void pens_get(gui *g)
{
    uc_u32 rgb[MT_ROLES];
    gtheme th;
    int r;
    /* the drawn menu's colours are the Omarchy GUI theme (Style > Theme, AM_GTHEME), not the terminal's */
    gt_dos_load_current(&th);
    menu_theme_roles_gui(&th, rgb);
    SetVar((STRPTR) "AMARCHY_THEME", (STRPTR)th.name, -1, GVF_GLOBAL_ONLY);   /* sentinel: the rig reads which theme drew */
    if (minimal) {
        /* two colours: the theme's background and foreground; equal ones would hide the selection, so both
         * fall back to the screen's own pens (0, 1) */
        int themed = menu_theme_minimal(rgb);
        for (r = 0; r < MT_ROLES; r++) {
            int is_bg = r == MT_BG || r == MT_ACCENT;
            long p = themed ? vr_pen_obtain(g->cm, rgb[r]) : -1;
            g->pen_got[r] = p >= 0;
            g->pen[r] = p >= 0 ? p : (is_bg ? 0 : 1);
        }
    } else {
        for (r = 0; r < MT_ROLES; r++) {
            long p = vr_pen_obtain(g->cm, rgb[r]);
            g->pen_got[r] = p >= 0;
            g->pen[r] = p >= 0 ? p : (r == MT_BG ? 0 : 1);
        }
    }
    g->pens = 0;
    for (r = 0; r < MT_ROLES; r++) {
        int q, dup = 0;
        for (q = 0; q < r; q++)
            if (g->pen[q] == g->pen[r])
                dup = 1;
        g->pens += !dup;
    }
}

static void pens_release(gui *g)
{
    int r;
    for (r = 0; r < MT_ROLES; r++)
        if (g->pen_got[r])
            ReleasePen(g->cm, (ULONG)g->pen[r]);
}

static struct Window *window_open(gui *g)
{
    struct TagItem tags[] = {
        { WA_Left, g->g.x }, { WA_Top, g->g.y }, { WA_Width, g->g.w }, { WA_Height, g->g.h },
        { WA_PubScreen, (ULONG)g->scr }, { WA_Title, (ULONG) "Amarchy" },
        { WA_Borderless, TRUE }, { WA_Activate, TRUE }, { WA_RMBTrap, TRUE },
        { WA_SmartRefresh, TRUE }, { WA_ReportMouse, TRUE },
        { WA_BackFill, (ULONG)LAYERS_NOBACKFILL },
        { WA_IDCMP, IDCMP_VANILLAKEY | IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE |
                    IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW | IDCMP_CHANGEWINDOW },
        { TAG_DONE, 0 }
    };
    return OpenWindowTagList(0, tags);
}

/* ---- viewer and message lines ------------------------------------------------------------------------ */

static void lines_split(gui *g, char *text)
{
    char *p = text, *nl;
    g->nlines = 0;
    while (*p && g->nlines < MAX_LINES) {
        nl = strchr(p, '\n');
        g->ln[g->nlines] = p;
        g->ll[g->nlines++] = nl ? (int)(nl - p) : (int)strlen(p);
        if (!nl)
            break;
        p = nl + 1;
    }
}

/* the message cut into lines that fit tw pixels, at most max of them */
static void lines_wrap(gui *g, const char *s, int tw, int max)
{
    int left = (int)strlen(s);
    g->nlines = 0;
    while (left > 0 && g->nlines < max) {
        int n = mt_prefix(g->mt, s, left, tw);
        if (n <= 0)
            n = 1;
        g->ln[g->nlines] = s;
        g->ll[g->nlines++] = n;
        s += n;
        left -= n;
    }
}

/* ---- leaf callbacks ---------------------------------------------------------------------------------- */

/* A value for an action that needs one: the header shows the prompt, the first row is the edit line. */
static int gui_ask_value(void *ctx, const char *label, const char *usage, char *out, int n)
{
    gui *g = (gui *)ctx;
    int ok;
    if (g->quit)
        return 0;
    strcpy(prompt_text, label);
    strcat(prompt_text, " (");
    strncat(prompt_text, usage, UPREQ_ANSWER);
    strcat(prompt_text, "):");
    g->head = prompt_text;
    g->mode = M_INPUT;
    g->edone = g->stop = 0;             /* out[0] is the text the line starts with */
    g->elen = (int)strlen(out) < EDIT_MAX ? (int)strlen(out) : EDIT_MAX - 1;
    memcpy(g->edit, out, (size_t)g->elen);
    g->top = 0;
    relayout(g, 1);
    event_loop(g);
    ok = g->edone && g->elen > 0 && !g->quit;
    if (ok) {
        int l = g->elen < n - 1 ? g->elen : n - 1;
        memcpy(out, g->edit, (size_t)l);
        out[l] = 0;
    }
    return ok;
}

/* A refused action says why before the window closes; any key closes. */
static void gui_refused(void *ctx, const char *label, long rc, const char *answer)
{
    gui *g = (gui *)ctx;
    const char *why = answer && answer[0] ? answer :
                      rc == UPREQ_NOPORT ? "no UP-Term window to run it in" :
                      rc == UPREQ_NOWINDOW ? "the terminal has no window" :
                      rc == UPREQ_BADVERSION ? "the terminal is another UP-Term version: reboot" :
                      "the terminal does not know it";
    if (g->quit)
        return;
    strcpy(msg_text, label);
    strcat(msg_text, ": ");
    strncat(msg_text, why, UPREQ_ANSWER);
    g->head = "Amarchy";
    g->mode = M_MSG;
    g->stop = 0;
    g->top = 0;
    lines_wrap(g, msg_text, g->g.list_w - 2 * g->g.m.row_pad, MG_MAX_ROWS);
    relayout(g, g->nlines);
    event_loop(g);
}

/* ---- main -------------------------------------------------------------------------------------------- */

static void cleanup(gui *g)
{
    ig_close();
    buffer_free(g);
    if (g->win)
        CloseWindow(g->win);
    g->win = 0;
    if (g->cm)
        pens_release(g);
    g->cm = 0;
    if (g->scr)
        UnlockPubScreen(0, g->scr);
    g->scr = 0;
    mt_close(g->mt);
    g->mt = 0;
}

/* The screen's pixel shape: DisplayInfo Resolution into ms.rx, ms.ry (0, 0 when unknown), and 1 when the
 * pixels are square. */
static void pick_font(gui *g)
{
    int square = menu_screen_aspect(g->scr, &g->ms.rx, &g->ms.ry);
    const char *spec;
    am_prefs ap;
    amconf_load(&ap);
    spec = ap.val[AM_FONT];   /* "name:size", as the terminal's font key */
    g->mt = mt_open(g->scr, square, spec);
}

int main(int argc, char **argv)
{
    gui *g = &G;
    int viewer, rc = 0;
    const char *node;

    lf_init();
    if (qmark_argv(argc, argv) || (argc > 1 && !strcmp(argv[1], "help"))) {
        PutStr((STRPTR) USAGE);
        return 0;
    }
    if (argc < 2 || (strcmp(argv[1], "menu") && strcmp(argv[1], "viewer"))) {
        PutStr((STRPTR) USAGE);
        return 5;
    }
    mo_register();   /* MENU-TOGGLE: the menu hotkey again ends this menu */
    viewer = !strcmp(argv[1], "viewer");
    node = argc > 2 && strcmp(argv[2], "root") ? argv[2] : "";
    keymap_load(&km, km_default_text);
    index_drawers();
    ix_load_file(&km, &ixs, IX_CACHE_PATH);
    lf_load_listers(&ixs);
    gt_dos_load_rows(&ixs);
    menu_cond = mn_cond(upactive_terminal_present(0), upactive_dopus_present(), upactive_workbench_present());   /* a plain Intuition window: never in the MRU */
    {   /* a scan that runs already (started from a menu that closed on deactivation) is shown too */
        ix_prog p0;
        int alive0 = 0, have0 = ixw_snapshot(&p0, &alive0);
        ix_watcher_begin(&g->wt, 0, have0, have0 ? &p0 : 0, alive0);
    }
    g->cap = ix_level_cap(&ixs);
    g->it = malloc(g->cap * sizeof *g->it);
    g->chip = malloc(g->cap * sizeof *g->chip);
    g->idx = malloc(g->cap * sizeof *g->idx);
    if (!g->it || !g->idx) {
        PutStr((STRPTR) "amarchy-gui: out of memory\n");
        rc = 20;
        goto done;
    }

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR) "intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR) "graphics.library", 39);
    if (!IntuitionBase || !GfxBase) {
        PutStr((STRPTR) "amarchy-gui: needs AmigaOS 3.0 or newer\n");
        rc = 20;
        goto done;
    }
    g->scr = lock_front_screen();
    if (!g->scr)
        g->scr = LockPubScreen(0);
    if (!g->scr) {
        PutStr((STRPTR) "amarchy-gui: no screen\n");
        rc = 20;
        goto done;
    }
    g->cm = g->scr->ViewPort.ColorMap;
    pick_font(g);
    if (!g->mt) {
        PutStr((STRPTR) "amarchy-gui: no memory for the font\n");
        rc = 20;
        goto done;
    }
    g->mf.ysize = mt_height(g->mt);
    g->mf.cw = mt_bitmap_font(g->mt) ? mt_bitmap_font(g->mt)->tf_XSize : mt_width(g->mt, "n");
    g->ms.w = g->scr->Width;
    g->ms.h = g->scr->Height;
    g->ms.minimal = minimal;
    pens_get(g);
    if (app_icons == UC_ICONS_AMIGA && !minimal)
        ig_open(g->scr, ixs.n);      /* no icon.library V44: the index rows keep the generic glyph */

    /* the font's text lengths before the window exists */
    /* the minimal menu's group marker is part of the label's text: " >" after it, no gap column */
    g->chev_w = text_len(minimal ? " >" : ">", g->mt);
    g->chosen = -1;
    g->ixpath[0] = g->ixlabel[0] = 0;
    strcpy(g->node, node);
    if (viewer) {
        int i, w = 0;
        mg_metrics m;
        mn_viewer_text(&km, view_text, sizeof view_text);
        g->mode = M_VIEW;
        g->head = "Key bindings";
        lines_split(g, view_text);
        for (i = 0; i < g->nlines; i++) {
            int lw = mt_width_n(g->mt, g->ln[i], g->ll[i]);
            if (lw > w)
                w = lw;
        }
        mg_metrics_for(&m, &g->mf, &g->ms);
        w += 2 * m.bx + 2 * m.hpad + 2 * m.row_pad;
        if (w < m.min_w)
            w = m.min_w;
        if (w > g->ms.w - 2 * m.margin_x)
            w = g->ms.w - 2 * m.margin_x;
        g->w = w;
        mg_layout(&g->g, &g->mf, g->w, g->nlines, &g->ms);
    } else {
        int n;
        mg_metrics m;
        g->mode = M_LIST;
        mg_metrics_for(&m, &g->mf, &g->ms);
        /* no icon column in the minimal menu or with amarchy-app-icons = none */
        g->icon_h = mi_count() > 0 && !minimal && app_icons != UC_ICONS_NONE ? mi_size(&m) : 0;
        g->icon_w = mi_count() > 0 && !minimal && app_icons != UC_ICONS_NONE ? mi_width(&m, &g->ms) : 0;
        g->w = mg_width_t(&km, g->node, &g->mf, g->icon_w, g->chev_w, text_len, g->mt, &g->ms, menu_cond);
        n = ix_level_t(&km, &ixs, g->node, g->filter, g->it, g->cap, menu_cond);
        lf_setting_relabel(g->it, n);
        lf_setting_rechip(g->it, n, g->chip);
        g->shift = g->wt.row[0] ? 1 : 0;
        g->count = mn_filter(g->it, n, g->filter, g->idx, g->cap) + g->shift;
        g->sel = g->shift;
        mg_layout(&g->g, &g->mf, g->w, g->count, &g->ms);
    }
    g->win = window_open(g);
    if (!g->win) {
        PutStr((STRPTR) "amarchy-gui: cannot open the window\n");
        rc = 20;
        goto done;
    }
    g->rp = g->win->RPort;
    g->frame = (GfxBase->DisplayFlags & PAL) ? 313 : 263;
    SetDrMd(g->rp, JAM1);
    draw_full(g);
    watch_idcmp(g);
    event_loop(g);

    if ((g->chosen >= 0 || g->ixpath[0]) && !g->quit) {
        lf_ui ui;
        ui.ask_value = gui_ask_value;
        ui.refused = gui_refused;
        ui.ctx = g;
        ui.self_in_mru = 0;
        ui.win = g->win;
        g->stop = 0;
        /* a script or a new window needs no menu around it; a terminal action or a program that cannot start may refuse */
        if (g->ixpath[0]) {                    /* an index row may be refused: the window stays for the message */
        } else if (lf_plan(km.menu[g->chosen].kind) != LF_REQUEST && lf_plan(km.menu[g->chosen].kind) != LF_WINDOW &&
                   lf_plan_t(km.menu[g->chosen].kind, 0) != LF_OPEN_THEN_REQUEST) {
            cleanup(g);
            g->rp = 0;
        }
        rc = g->ixpath[0] ? lf_open_index(g->ixlabel, g->ixpath, &ui) : lf_run(&km.menu[g->chosen], &ui);
    }
done:
    cleanup(g);
    free(g->it);
    free(g->idx);
    free(g->chip);
    ix_free(&ixs);
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
