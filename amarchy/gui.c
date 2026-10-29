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
#include "desk_dos.h"
#include "pkgs_dos.h"
#include "menu_open.h"
#include "taskalive.h"
#include "atiming.h"
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
#include "job_dos.h"

/* vbcc's startup runs main on this much, and the cookie is what vsh, the Shell and Workbench read from the
 * file to start it on that much. tests/test_amarchy_stack.py reads both from the built binary. */
#define GUI_STACK 65536
#define GUI_STR2(x) #x
#define GUI_STR(x) GUI_STR2(x)
long __stack = GUI_STACK;
const char amarchy_gui_stack_cookie[] = "$STACK: " GUI_STR(GUI_STACK);

#define USAGE "usage: amarchy-gui menu [node [at AX AY]] | viewer | serve\n"
#define MAX_LINES 200
#define EDIT_MAX 80
#define PUT_MAX 384          /* the longest label put() cuts */

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

enum { M_LIST, M_INPUT, M_MSG, M_VIEW, M_JOB };
#define JOB_ROWS 18           /* rows of the job view: the body and, as the last one, the footer */
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
    uc_u32 ink[MT_ROLES];        /* the colour each role draws in (menu_theme_ink) */
    char drawn[GT_NAME];         /* the theme the pens are from: the saved one, or the Style > Theme row previewed */
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
static int hidden;   /* the menu window is closed for a Workbench reset (desk_dos_own_close) */
static jc job;                   /* the running job's output (job_dos feeds it); one job at a time */
static int job_started;          /* the last lf_run started a job view (main re-enters the list after it) */
static int job_on;               /* a job view is up: event_loop polls it */
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

/* Publishes the glyphs drawn through coverage (aa_ink) so far as AMARCHY_AA_GLYPHS: TrueType always, a bitmap
 * font on a true-colour screen too, 0 for a bitmap font on a palette screen; a rig reads whether that path ran. */
static void sentinel_aa(const gui *g)
{
    sentinel_publish("AMARCHY_AA_GLYPHS", mt_aa_glyphs(g->mt));
}

/* Publishes the labels of the rows the list shows, joined by '|', as AMARCHY_ROWS: the rig reads which
 * rows were drawn (a menu's text is pixels, no uitree gadget). */
static void sentinel_rows(const gui *g)
{
    char b[256];
    mn_rows_join(g->it, g->idx, g->count - g->shift, b, sizeof b);
    SetVar((STRPTR) "AMARCHY_ROWS", (STRPTR)b, -1, GVF_GLOBAL_ONLY);
}

/* Publishes where the list is: AMARCHY_TOP is the index of the first drawn row and AMARCHY_SEL the selected row. AMARCHY_ROWS
 * lists the whole level (published on a full draw), so a scroll moves these two and the rig tells a scrolled list from a
 * stuck one by them. */
static void sentinel_cursor(const gui *g)
{
    if (g->mode != M_LIST)
        return;
    sentinel_publish("AMARCHY_TOP", g->top);
    sentinel_publish("AMARCHY_SEL", g->sel);
}

/* ---- text and fills ----------------------------------------------------------------------------------- */

static int text_len(const char *s, void *ctx)
{
    return mt_width((menu_text *)ctx, s);
}

/* the pen's colour as 0xRRGGBB */
static ULONG pen_rgb(const gui *g, long pen)
{
    ULONG c[3];
    GetRGB32(g->cm, (ULONG)pen, 1, c);
    return ((c[0] >> 24) << 16) | ((c[1] >> 24) << 8) | (c[2] >> 24);
}

/* what menu_text draws a role in (a bitmap font finds the nearest pen again) */
static ULONG role_rgb(const gui *g, int role)
{
    return g->ink[role];
}

static void fill(gui *g, int role, int x0, int y0, int x1, int y1)
{
    if (x1 < x0 || y1 < y0)
        return;
    if (mt_fill(g->mt, g->rp, x0, y0, x1, y1, g->ink[role]))
        return;
    SetAPen(g->rp, (ULONG)g->pen[role]);
    RectFill(g->rp, x0, y0, x1, y1);
}

/* A text in a box of height boxh whose top is y, cut to maxw pixels with an ellipsis, in role over the box's
 * background role bgrole (the caller has filled the box in it). */
/* keep >= 0: s[keep..] stays whole when the text is clipped (a settings row's ": Value"); the label is cut */
static void put_keep(gui *g, int role, int bgrole, int x, int y, int boxh, const char *s, int len, int maxw, int keep)
{
    char buf[PUT_MAX], out[PUT_MAX];
    if (len <= 0 || maxw <= 0)
        return;
    if (len > PUT_MAX - 1)
        len = PUT_MAX - 1;
    memcpy(buf, s, (size_t)len);
    buf[len] = 0;
    mt_fit_tail(g->mt, buf, keep, maxw, out, sizeof out);
    if (keep > 0 && keep < 24) {      /* a settings row: AMARCHY_FIT_<label> is the text as drawn, fitted, for the rig */
        char nm[40];
        int k;
        memcpy(nm, "AMARCHY_FIT_", 12);
        for (k = 0; k < keep; k++)
            nm[12 + k] = (buf[k] >= '0' && buf[k] <= '9') || ((buf[k] | 32) >= 'a' && (buf[k] | 32) <= 'z') ? buf[k] : '_';
        nm[12 + keep] = 0;
        SetVar((STRPTR)nm, (STRPTR)out, -1, GVF_GLOBAL_ONLY);
    }
    if (!out[0])
        return;
    mt_draw(g->mt, g->rp, x, y + (boxh - mt_height(g->mt)) / 2, out, role_rgb(g, role), role_rgb(g, bgrole));
    SetDrMd(g->rp, JAM1);   /* a bitmap font's draw left JAM2 */
}

static void put(gui *g, int role, int bgrole, int x, int y, int boxh, const char *s, int len, int maxw)
{
    put_keep(g, role, bgrole, x, y, boxh, s, len, maxw, -1);
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
    } else if (g->mode == M_JOB) {
        jc_header(&job, h, sizeof h);
        s = h[0] ? h : jb_title();
    }
    put(g, MT_HEAD_ROLE, MT_BG, q->head_x + q->m.row_pad, q->head_y, q->m.head_h, s, (int)strlen(s),
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
            char t[MN_LABEL + 4];
            strncpy(t, it->label, MN_LABEL);
            t[MN_LABEL] = 0;
            strcat(t, " >");
            put(g, role, rbg, tx, y, q->m.row_h, t, (int)strlen(t), tw);
        } else {
            long rgb = g->chip[it - g->it];
            if (rgb >= 0) {                       /* a colour row: a filled chip at the right edge, label left of it */
                int cs = q->m.row_h - 4, cx = tx + tw - cs;
                if (mt_direct(g->mt)) {           /* a true-colour screen: the colour itself, no pen */
                    fill(g, MT_TEXT, cx - 1, y + 1, cx + cs, y + 2 + cs);      /* border in the text colour */
                    mt_fill(g->mt, g->rp, cx, y + 2, cx + cs - 1, y + 1 + cs, (ULONG)rgb);
                } else {
                    long p = vr_pen_obtain(g->cm, (unsigned long)rgb);
                    if (p >= 0) {
                        fill(g, MT_TEXT, cx - 1, y + 1, cx + cs, y + 2 + cs);
                        SetAPen(g->rp, (ULONG)p);
                        RectFill(g->rp, cx, y + 2, cx + cs - 1, y + 1 + cs);
                        ReleasePen(g->cm, (ULONG)p);
                    }
                }
                tw -= cs + 4 + q->m.row_pad;
            }
            {
                const char *sep = MN_IS_SETTING(it) ? strstr(it->label, ": ") : 0;   /* the value is not what is cut */
                put_keep(g, role, rbg, tx, y, q->m.row_h, it->label, (int)strlen(it->label), tw,
                         sep ? (int)(sep - it->label) : -1);
            }
        }
        if (MN_IS_GROUP(it) && !minimal)
            put(g, role, rbg, q->list_x + q->list_w - q->m.row_pad - g->chev_w, y, q->m.row_h, ">", 1, g->chev_w);
    } else if (g->mode == M_JOB) {
        int body = q->visible - 1;
        if (e < body) {
            int t = jc_top(&job, body) + e;
            if (t < jc_count(&job))
                put(g, MT_TEXT, rbg, tx, y, q->m.row_h, jc_line(&job, t), (int)strlen(jc_line(&job, t)), tw);
        } else if (e == body) {
            char f[JOB_FOOT_MAX], h[JOB_FOOT_MAX];
            int tone = jc_tone(&job), fw, hw;
            jc_footer(&job, f, sizeof f);
            jc_hints(&job, h, sizeof h);
            put(g, tone == JT_OK ? MT_OK : tone == JT_FAIL ? MT_FAIL : MT_DIM, rbg, tx, y, q->m.row_h, f,
                (int)strlen(f), tw);
            /* The key hints sit at the right, as in the text menu's footer; the status word wins when both
             * do not fit. */
            fw = mt_width(g->mt, f);
            hw = mt_width(g->mt, h);
            if (fw + 2 * g->chev_w + hw <= tw)
                put(g, MT_DIM, rbg, tx + tw - hw, y, q->m.row_h, h, (int)strlen(h), hw);
        }
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
    sentinel_cursor(g);
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

static void pens_get(gui *g, const char *name);
static void pens_release(gui *g);

/* Style > Theme previews the theme under the cursor; any other row draws the saved theme (gt_preview_pick). The
 * pens change only when the theme does; the caller repaints (g->drawn tells it the theme changed). */
static void preview_sync(gui *g)
{
    char path[IX_PATH], want[GT_NAME], cur[GT_NAME];
    const char *row = 0;
    if (g->sel >= g->shift && g->sel < g->count) {
        const mn_item *it = &g->it[g->idx[g->sel - g->shift]];
        if (MN_IS_INDEX(it) && ix_launch_path(&ixs, it, path, (int)sizeof path))
            row = path;
    }
    gt_dos_current(cur, (int)sizeof cur);
    if (!gt_preview_pick(row, cur, g->drawn, want, (int)sizeof want))
        return;
    pens_release(g);
    pens_get(g, !strcmp(want, cur) ? 0 : want);
}

/* The level for the node and filter; the cursor goes to the entry labelled keep when it is still shown, else
 * the first real entry. */
static int keep_setting = -1;   /* the SDI_* id of the setting row the cursor stays on after a set, else -1 */

static int load_due(gui *g);

static void list_rebuild_keep(gui *g, const char *keep)
{
    char cur[GT_NAME], cur_label[MN_LABEL];
    int n, pos;
    if (!load_due(g))
        return;
    n = ix_level_t(&km, &ixs, g->node, g->filter, g->it, g->cap, menu_cond);
    lf_setting_relabel(g->it, n);       /* a settings row shows its value */
    lf_setting_rechip(g->it, n, g->chip);
    g->shift = g->wt.row[0] ? 1 : 0;
    g->count = mn_filter(g->it, n, g->filter, g->idx, g->cap) + g->shift;
    if (!keep) {                         /* the theme list opens on the saved theme: the preview starts on it, not on row 1 */
        gt_dos_current(cur, (int)sizeof cur);
        gt_label(cur, cur, cur_label, (int)sizeof cur_label);
        keep = cur_label;
    }
    pos = keep_setting >= 0 ? mn_pick_setting(g->it, g->idx, g->count - g->shift, keep_setting)
                            : mn_pick(g->it, g->idx, g->count - g->shift, keep);
    keep_setting = -1;
    g->sel = g->shift + (pos > 0 ? pos : 0);
    g->top = 0;
    if (pos > 0) {                       /* the layout (visible rows) is needed to scroll to it */
        mg_layout(&g->g, &g->mf, g->w, g->count, &g->ms);
        g->top = mg_scroll(&g->g, g->sel, 0);
    }
    preview_sync(g);
    relayout(g, g->count);
}

static void list_rebuild(gui *g)
{
    list_rebuild_keep(g, 0);
}

static void set_sel(gui *g, int s)
{
    int old = g->sel, top = g->top;
    char drawn_was[GT_NAME];
    strcpy(drawn_was, g->drawn);
    if (g->count <= g->shift)
        return;
    if (s < g->shift)
        s = g->shift;
    if (s >= g->count)
        s = g->count - 1;
    g->sel = s;
    g->top = mg_scroll(&g->g, s, top);
    if (s != old && g->mode == M_LIST)
        preview_sync(g);
    if (strcmp(g->drawn, drawn_was)) {
        draw_full(g);           /* a preview recoloured the menu: every row and the header */
    } else if (g->top != top) {
        draw_list(g);
    } else if (s != old) {
        draw_row(g, s);         /* the new row first: a frame between the copies shows two selected, never none */
        draw_row(g, old);
        sentinel_publish("AMARCHY_BEAM_WAITS", g->beam_waits);
        sentinel_publish("AMARCHY_TEARS", g->tears);
        sentinel_publish("AMARCHY_LATE", g->late);
    sentinel_aa(g);
        sentinel_publish("AMARCHY_BLITS", g->blits);
        sentinel_cursor(g);
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
static int gui_run_job(void *ctx, const char *line, const char *title);
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

/* Size the level buffers for the index as it is now (ix_level_cap). 0 when memory ran out. */
static int size_level(gui *g)
{
    free(g->it);
    free(g->idx);
    free(g->chip);
    g->cap = ix_level_cap(&ixs);
    g->it = malloc(g->cap * sizeof *g->it);
    g->chip = malloc(g->cap * sizeof *g->chip);
    g->idx = malloc(g->cap * sizeof *g->idx);
    return g->it && g->idx;
}

/* The scan finished: read the new index (the Apps level has new rows). 0 when memory ran out. */
static int reload_index(gui *g)
{
    ix_free(&ixs);
    index_drawers();
    ix_load_file(&km, &ixs, IX_CACHE_PATH);
    lf_load_listers(&ixs);
    gt_dos_load_rows(&ixs);
    if (size_level(g))
        return 1;
    g->quit = g->stop = 1;
    return 0;
}

/* Install > Packages is entered for the first time this open: upkg gives its rows now, not on every open
 * (ix_pkgs_due), and the level buffers grow. 0 when memory ran out. */
static int load_due(gui *g)
{
    if (!pk_dos_due(&ixs, g->node))
        return 1;
    pk_dos_load_rows(&ixs);
    if (size_level(g))
        return 1;
    g->quit = g->stop = 1;
    return 0;
}

/* One poll of the record; a changed row redraws the list, DONE also reloads the index. */
static void watch_poll(gui *g)
{
    char keep[MN_LABEL];
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
    char keep[MN_LABEL];
    int alive0 = 0, have0 = ixw_snapshot(&p0, &alive0);   /* the run counter before the launch */
    ui.ask_value = gui_ask_value;
    ui.refused = gui_refused;
    ui.ctx = g;
    ui.self_in_mru = 0;
    ui.win = g->win;
    ui.run_job = gui_run_job;
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
    ui.run_job = gui_run_job;
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

/* Packages > Search asks its text, then enters the node that lists the matches; a package action row runs its job
 * (Remove only after its confirm page), Launch closes the menu through the index-row path, No goes back. */
static void activate_pkg(gui *g, const mn_item *it)
{
    lf_ui ui;
    mn_item row = *it;                      /* the rebuilds below overwrite g->it */
    int back = lf_pkg_back_levels(&row), k;
    ui.ask_value = gui_ask_value;
    ui.refused = gui_refused;
    ui.ctx = g;
    ui.self_in_mru = 0;
    ui.win = g->win;
    ui.run_job = gui_run_job;
    if (lf_pkg_search_row(g->node, &row)) {
        if (lf_pkg_search(&ixs, &ui)) {
            mn_child_node(g->node, &row, g->node, sizeof g->node);
            g->filter[0] = 0;
            list_rebuild(g);
        }
        return;
    }
    if (row.ix >= 0 && row.seg[0] == PK_ACT_LAUNCH) {
        char path[PK_PATH + 16];
        if (pk_row_path(ixs.pk.r[row.ix].launch, path, (int)sizeof path)) {
            strncpy(g->ixpath, path, sizeof g->ixpath - 1);
            strncpy(g->ixlabel, ixs.pk.r[row.ix].name, sizeof g->ixlabel - 1);
            g->stop = 1;                    /* lf_open_index starts it, as for an index row */
            return;
        }
    } else if (row.ix >= 0 && row.seg[0] && row.seg[0] != PK_ACT_NO) {
        lf_pkg_action(&ixs, &row, &ui);
        if (g->quit)
            return;
        g->mode = M_LIST;                   /* the job view closed: the list again */
        g->stop = 0;
    }
    for (k = 0; k < back; k++)
        go_up(g);
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
    if (MN_IS_PKGACT(it) || lf_pkg_search_row(g->node, it)) {
        activate_pkg(g, it);
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
        if (g->sel >= g->shift && g->sel < g->count && mn_right_enters(&g->it[g->idx[g->sel - g->shift]]))
            activate(g, 1);              /* the Return path; a leaf is not run by Right */
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

/* The job view's keys: the mapping lives in job_core (jc_key_act). */
static void job_redraw(gui *g)
{
    paint_begin(g);
    draw_header(g);
    draw_list(g);
    paint_end(g, 0, 0, g->g.w - 1, g->g.h - 1);
}

static void key_job(gui *g, int k)
{
    static const char map[] = { K_UP, K_DOWN, K_PGUP, K_PGDN, K_ESC, K_ENTER };
    int jk = JKEY_OTHER, i, d, body = g->g.visible - 1;
    for (i = 0; i < 6; i++)
        if (map[i] == k)
            jk = JKEY_UP + i;
    switch (jc_key_act(&job, jk, body, &d)) {
    case JA_SCROLL:
        jc_scroll(&job, d, body);
        draw_list(g);
        break;
    case JA_CANCEL:
        jb_cancel(&job);
        job_redraw(g);
        break;
    case JA_CLOSE:
        g->stop = 1;
        break;
    }
}

/* The tick of the job: take the new output; redraw when lines, the state or the progress changed. */
static void job_tick(gui *g)
{
    long total = job.total;
    int state = job.state, pn = job.prog_n, pm = job.prog_m, ended = job.ended;
    jb_poll_publish(&job);
    if (g->mode == M_JOB && !g->await &&
        (job.total != total || job.state != state || job.prog_n != pn || job.prog_m != pm || job.ended != ended))
        job_redraw(g);
}

static void key(gui *g, int k, int ch, int wheel)
{
    if (g->await)
        return;
    if (wheel) {
        if (g->mode == M_JOB) {
            int d, body = g->g.visible - 1;
            (void)jc_key_act(&job, wheel < 0 ? JKEY_UP : JKEY_DOWN, body, &d);
            jc_scroll(&job, d * 3, body);
            draw_list(g);
            return;
        }
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
    case M_JOB:
        key_job(g, k);
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
        else if (g->mode == M_JOB)
            key_job(g, K_ESC);
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
        ULONG jm = job_on ? jb_sigmask() : 0;       /* every mode: a nested one must not starve the job */
        ULONG got = Wait((1UL << g->win->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C | jm);
        if (jm && (got & jm))
            job_tick(g);
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

/* name: the theme to draw (a Style > Theme preview), 0 for the saved one */
static void pens_get(gui *g, const char *name)
{
    uc_u32 rgb[MT_ROLES], got[MT_ROLES];
    gtheme th;
    int r, themed = 1;
    /* the drawn menu's colours are the Omarchy GUI theme (Style > Theme, AM_GTHEME), not the terminal's */
    if (name)
        gt_dos_load(name, &th);
    else
        gt_dos_load_current(&th);
    strncpy(g->drawn, th.name, sizeof g->drawn - 1);
    g->drawn[sizeof g->drawn - 1] = 0;
    menu_theme_roles_gui(&th, rgb);
    SetVar((STRPTR) "AMARCHY_THEME", (STRPTR)th.name, -1, GVF_GLOBAL_ONLY);   /* sentinel: the rig reads which theme drew */
    if (minimal) {
        /* two colours: the theme's background and foreground; equal ones would hide the selection, so both
         * fall back to the screen's own pens (0, 1) */
        themed = menu_theme_minimal(rgb);
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
    /* a role with no pen of its own (few-colour screen) draws in its fallback role's pen (menu_theme.h) */
    for (r = 0; r < MT_ROLES; r++) {
        int fb = menu_theme_fallback_role(r);
        if (fb >= 0 && !g->pen_got[r])
            g->pen[r] = g->pen[fb];
    }
    g->pens = 0;
    for (r = 0; r < MT_ROLES; r++) {
        int q, dup = 0;
        for (q = 0; q < r; q++)
            if (g->pen[q] == g->pen[r])
                dup = 1;
        g->pens += !dup;
    }
    for (r = 0; r < MT_ROLES; r++)
        got[r] = pen_rgb(g, g->pen[r]);
    menu_theme_ink(g->ink, rgb, got, mt_direct(g->mt), themed);
}

static void pens_release(gui *g)
{
    int r;
    for (r = 0; r < MT_ROLES; r++)
        if (g->pen_got[r])
            ReleasePen(g->cm, (ULONG)g->pen[r]);
}

/* A Style > Theme pick saved the theme (lf_open_index): the open menu takes its colours at once and stays on the
 * theme list, which marks the picked theme (current) with the cursor on it (owner, 2026-10-28: switch live). */
static void theme_live(gui *g, const char *name)
{
    char keep[MN_LABEL];
    pens_release(g);
    pens_get(g, 0);
    gt_dos_load_rows(&ixs);
    gt_label(name, name, keep, (int)sizeof keep);
    g->chosen = -1;
    g->mode = M_LIST;
    g->stop = 0;
    g->ixpath[0] = g->ixlabel[0] = 0;
    list_rebuild_keep(g, keep);
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

/* A value for an action that needs one: the header shows the prompt, the first row is the edit line. The prompt is
 * a nested event loop: on return the window is back in the mode, header and scroll it had, with stop cleared, so a
 * caller that stays in the menu (Packages > Search, a setting edit) keeps its loop running. The edit loop's Enter or
 * Escape set stop, and left it set the outer loop ended too and the menu closed. */
static int gui_ask_value(void *ctx, const char *label, const char *usage, char *out, int n)
{
    gui *g = (gui *)ctx;
    int ok, mode0, top0;
    const char *head0;
    if (g->quit)
        return 0;
    mode0 = g->mode;
    head0 = g->head;
    top0 = g->top;
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
    g->mode = mode0;
    g->head = head0;
    g->top = top0;
    g->edone = g->stop = 0;
    if (!g->quit && g->mode == M_LIST)
        relayout(g, g->count);
    return ok;
}

/* A job: row: run the command and show its output; returns when the view closes. Esc/Enter after the end goes back
 * to the menu row; a hotkey or CTRL_C while it runs cancels the job first (no orphan child). A job that cannot start
 * runs as run: (lf_run_line). */
static int gui_run_job(void *ctx, const char *line, const char *title)
{
    gui *g = (gui *)ctx;
    lf_ui ui;
    if (g->quit)
        return 0;
    if (!jb_start(&job, line, 0, title)) {
        ui.ask_value = gui_ask_value;
        ui.refused = gui_refused;
        ui.ctx = g;
        ui.self_in_mru = 0;
        ui.win = g->win;
        ui.run_job = 0;
        return lf_run_line(line, &ui);
    }
    job_on = 1;
    job_started = 1;
    g->mode = M_JOB;
    g->stop = 0;
    g->top = 0;
    relayout(g, JOB_ROWS);
    event_loop(g);
    if (job.state == JC_RUNNING)
        jb_cancel(&job);
    lf_job_done(&ixs, line, job.state);   /* an upkg job: the Packages list reloads at the next rebuild */
    job_on = 0;
    jb_end();
    return 0;
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

/* desk_dos_apply: the menu window, its pens and its screen lock go before the palette is written (IPrefs defers the reset while a
 * window of ours is on the Workbench screen) and come back once it applied */
static int gui_hide(void)
{
    gui *g = &G;
    if (!g->win || hidden)
        return 0;
    CloseWindow(g->win);
    g->win = 0;
    g->rp = 0;
    pens_release(g);
    memset(g->pen_got, 0, sizeof g->pen_got);
    UnlockPubScreen(0, g->scr);
    g->scr = 0;
    hidden = 1;
    return 1;
}

static void gui_show(void)
{
    gui *g = &G;
    if (!hidden)
        return;
    hidden = 0;
    g->scr = lock_front_screen();
    if (!g->scr)
        g->scr = LockPubScreen(0);
    if (!g->scr)
        return;
    g->cm = g->scr->ViewPort.ColorMap;
    pens_get(g, 0);
    g->win = window_open(g);
    if (!g->win)
        return;
    g->rp = g->win->RPort;
    SetDrMd(g->rp, JAM1);
    g->ticking = 0;
    watch_idcmp(g);
    draw_full(g);
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

/* One open of the menu or the key viewer: what a fresh process did after its arguments, the previous open's
 * state reset first, so the warm server (`serve`) and the cold `menu` / `viewer` command lines run the same
 * code (plan 2026-10-27-amarchy-gui-warm.md). Keymap, index, screen, pens, font and icons load per open: a
 * settings or theme change shows at the next press. The libraries stay open (main). Returns the exit code. */
static int gui_open(int viewer, const char *node, int anchored, int ax, int ay)
{
    gui *g = &G;
    int rc = 0;

    at_mark("start");
    memset(g, 0, sizeof *g);
    keep_setting = -1;
    lf_reset();
    mo_register();   /* MENU-TOGGLE: the menu hotkey again ends this menu; clears a stale Ctrl-C */
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
    g->it = 0;
    g->idx = 0;
    g->chip = 0;
    if (!size_level(g)) {
        PutStr((STRPTR) "amarchy-gui: out of memory\n");
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
    at_mark("font");
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
    g->ms.anchored = anchored;
    g->ms.ax = ax;
    g->ms.ay = ay;
    pens_get(g, 0);
    if (app_icons == UC_ICONS_AMIGA && !minimal)
        ig_open(g->scr, ixs.n);      /* no icon.library V44: the index rows keep the generic glyph */
    at_mark("icons");

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
        if (!load_due(g)) {
            PutStr((STRPTR) "amarchy-gui: out of memory\n");
            rc = 20;
            goto done;
        }
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
    at_mark("window");
    hidden = 0;
    desk_dos_own_close = gui_hide;
    desk_dos_own_open = gui_show;
    g->rp = g->win->RPort;
    g->frame = (GfxBase->DisplayFlags & PAL) ? 313 : 263;
    SetDrMd(g->rp, JAM1);
    draw_full(g);
    at_mark("ready");
    watch_idcmp(g);
    for (;;) {
        event_loop(g);
        if (!((g->chosen >= 0 || g->ixpath[0]) && !g->quit))
            break;
        {
            lf_ui ui;
            char keep[MN_LABEL];
            ui.ask_value = gui_ask_value;
            ui.refused = gui_refused;
            ui.ctx = g;
            ui.self_in_mru = 0;
            ui.win = g->win;
            ui.run_job = gui_run_job;
            g->stop = 0;
            keep[0] = 0;
            if (g->chosen >= 0)
                strncpy(keep, km.menu[g->chosen].label, sizeof keep - 1);
            keep[sizeof keep - 1] = 0;
            job_started = 0;
            /* a script or a new window needs no menu around it; a terminal action or a program that cannot start may refuse */
            if (g->ixpath[0]) {                    /* an index row may be refused: the window stays for the message */
            } else if (lf_plan(km.menu[g->chosen].kind) != LF_REQUEST && lf_plan(km.menu[g->chosen].kind) != LF_WINDOW &&
                       lf_plan(km.menu[g->chosen].kind) != LF_JOB &&
                       lf_plan_t(km.menu[g->chosen].kind, 0) != LF_OPEN_THEN_REQUEST) {
                cleanup(g);
                g->rp = 0;
            }
            rc = g->ixpath[0] ? lf_open_index(g->ixlabel, g->ixpath, &ui) : lf_run(&km.menu[g->chosen], &ui);
            if (!rc && g->ixpath[0] && !g->quit && gt_path_name(g->ixpath, keep, (int)sizeof keep)) {
                theme_live(g, keep);
                continue;
            }
            if (!job_started || g->quit)
                break;
            /* a job view closed: back to the menu row it came from */
            g->chosen = -1;
            g->mode = M_LIST;
            g->stop = 0;
            list_rebuild_keep(g, keep);
        }
    }
done:
    desk_dos_own_close = 0;
    desk_dos_own_open = 0;
    cleanup(g);
    free(g->it);
    free(g->idx);
    free(g->chip);
    g->it = 0;
    g->idx = 0;
    g->chip = 0;
    ix_free(&ixs);
    keymap_free(&km);
    mo_unregister();
    return rc;
}

/* `amarchy-gui serve`: the warm drawn menu (plan 2026-10-27-amarchy-gui-warm.md), the pattern of
 * `amarchy-menu serve` (menu.c). The commodity sends an ms_msg to GS_PORT per press; the reply goes at once,
 * then the menu opens (gui_open). The message's spec is the text menu's and is not read here. */
static int serve(void)
{
    struct MsgPort *port;
    struct ms_msg *m, q;
    int quit = 0;
    port = CreateMsgPort();
    if (!port)
        return 20;
    port->mp_Node.ln_Name = (char *)GS_PORT;
    Forbid();
    {
        struct MsgPort *old = FindPort((STRPTR)GS_PORT);
        if (old && task_alive((struct Task *)old->mp_SigTask, GS_TASK_NAME)) {
            Permit();
            DeleteMsgPort(port);
            return 5;                          /* a live server exists */
        }
        if (old)
            RemPort(old);                      /* a killed server's stale port */
    }
    AddPort(port);
    Permit();
    while (!quit) {
        Wait(1UL << port->mp_SigBit | SIGBREAKF_CTRL_C);
        SetSignal(0, SIGBREAKF_CTRL_C);        /* a stray toggle after a close must not end the server */
        while (!quit && (m = (struct ms_msg *)GetMsg(port)) != 0) {
            q = *m;
            if (q.kind == MS_QUIT) {
                Forbid();
                RemPort(port);                 /* gone before the commodity sees the reply */
                Permit();
                quit = 1;
            }
            ReplyMsg(&m->m);                   /* at once: the commodity never waits for the menu */
            if (!quit) {
                q.node[sizeof q.node - 1] = 0;
                gui_open(q.kind == MS_VIEWER, q.node[0] && strcmp(q.node, "root") ? q.node : "", (int)q.anchored, (int)q.ax, (int)q.ay);
            }
        }
    }
    while ((m = (struct ms_msg *)GetMsg(port)) != 0)
        ReplyMsg(&m->m);
    DeleteMsgPort(port);
    return 0;
}

int main(int argc, char **argv)
{
    int rc;

    lf_init();
    if (qmark_argv(argc, argv) || (argc > 1 && !strcmp(argv[1], "help"))) {
        PutStr((STRPTR) USAGE);
        return 0;
    }
    if (argc < 2 || (strcmp(argv[1], "menu") && strcmp(argv[1], "viewer") && strcmp(argv[1], "serve"))) {
        PutStr((STRPTR) USAGE);
        return 5;
    }
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR) "intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR) "graphics.library", 39);
    if (!IntuitionBase || !GfxBase) {
        PutStr((STRPTR) "amarchy-gui: needs AmigaOS 3.0 or newer\n");
        rc = 20;
    } else if (!strcmp(argv[1], "serve"))
        rc = serve();
    else {
        int ax = 0, ay = 0, anchored;
        /* `menu <node> at AX AY`: the cold path of an anchored open (the bar's panel) */
        anchored = ms_anchor_args(argc, argv, 3, &ax, &ay);
        rc = gui_open(!strcmp(argv[1], "viewer"), argc > 2 && strcmp(argv[2], "root") ? argv[2] : "", anchored, ax, ay);
    }
    ix_pkg_free(&ixs);
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
