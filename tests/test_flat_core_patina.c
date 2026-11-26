/* The patina-flat state block (render/flat_core.c; plan 2026-10-29-patina-flat-ui-patch.md FLAT-C0). The block
 * is shared with Patina through the public semaphore, so its offsets are a contract: pinned here byte by byte. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../render/flat_core.h"
#include "../render/flat_glyph.h"
#include "../patina/roundaa_core.h"
#include "../render/desk_core.h"
#include "../render/iconpen_core.h"
#define ROMSPAN_VEC
#include "harness.h"

/* ---- FLAT-C2: glyph geometry against pixel goldens, and the dispatcher decision (what flat.c's sysiclass dispatcher
 * runs: count, flat_glyph_draw, else the old hook). The canvas is a char grid; a pen value is its character. */
#define CV 40
static char cv[CV][CV + 1];
static int g_old_calls, g_disp_calls;
static int g_frame_pen = -1;                            /* the frame pen the fake dispatcher passes (-1: none known) */

static void cv_clear(void)
{
    int y;
    for (y = 0; y < CV; y++) {
        memset(cv[y], '?', CV);
        cv[y][CV] = 0;
    }
}

static void cv_fill(void *ctx, int x0, int y0, int x1, int y1, int pen)
{
    int x, y;
    (void)ctx;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (x >= 0 && x < CV && y >= 0 && y < CV)
                cv[y][x] = (char)pen;
}

/* 0c DOPUS-ENV-UNSTYLED (owner, rig 63, DOpus Environment "buttons are text only"): under flat pens DrawBox / DrawFieldBox
 * drew nothing. They now draw the flat outline in pen_line on the inclusive rectangle, side columns wide, and never touch
 * the inside (the caller's label). Fails on the old code: no flat_dopus_box_draw, and the d4 SKIP branch drew no pixel. */
/* DT-11: the lister toolbar "<<" arrow. dopus5's RenderImage (LVO -654) is the fourth D4 element; the arrow is told
 * from every other image by the basename of its path (Image_Data node.ln_Name, opened from DOpus5:Images/ToolbarArrow.image)
 * and redrawn as two flat left triangles in one pen. Fails on the old code: no element, no matcher, no glyph. */
static void test_dopus_arrow(void)
{
    static const char *const want[] = {
        "?????????????",
        "?????#???#???",
        "????##??##???",
        "???###?###???",
        "??########???",
        "???###?###???",
        "????##??##???",
        "?????#???#???",
        "?????????????",
    };
    uint32_t sums[FLAT_EL_COUNT] = { 0x780fd46bu, 0xba2cf172u, 0xbdaa116au, 0xf8b63678u };
    int y, n, why = -1;
    CHECK_INT(FLAT_EL_RENDERIMAGE, 3);
    CHECK_INT(FLAT_LVO_RENDERIMAGE, -654);
    CHECK_INT(flat_dopus_gate(flat_dopus_table(&n), n, 73, 8, sums, &why), 15);   /* 5.104: RenderImage measured from the vendor hunk */
    sums[FLAT_EL_RENDERIMAGE] ^= 1;
    CHECK_INT(flat_dopus_gate(flat_dopus_table(&n), n, 73, 8, sums, &why), 7);    /* other RenderImage bytes: that element stays stock */
    CHECK_INT(flat_body_prologue(FLAT_EL_RENDERIMAGE, (const unsigned char *)"\x4f\xef\xff\x64\x48\xe7\x3f\x3e"), 0);  /* LVO swap only */
    CHECK_INT(flat_dopus_arrow_name("DOpus5:Images/ToolbarArrow.image"), 1);
    CHECK_INT(flat_dopus_arrow_name("dopus5:images/toolbararrow.image"), 1);
    CHECK_INT(flat_dopus_arrow_name("ToolbarArrow.image"), 1);
    CHECK_INT(flat_dopus_arrow_name("Work:ToolbarArrow.image"), 1);
    CHECK_INT(flat_dopus_arrow_name("DOpus5:Images/ToolbarArrow.image.bak"), 0);
    CHECK_INT(flat_dopus_arrow_name("DOpus5:Images/MyToolbarArrow.image"), 0);
    CHECK_INT(flat_dopus_arrow_name("DOpus5:Buttons/Arrow.image"), 0);
    CHECK_INT(flat_dopus_arrow_name(""), 0);
    CHECK_INT(flat_dopus_arrow_name(0), 0);
    cv_clear();
    CHECK_INT(flat_glyph_dleft(0, 0, 13, 9, 1, '#', cv_fill, 0), 14);
    for (y = 0; y < 9; y++)
        CHECK(memcmp(cv[y], want[y], 13) == 0);
    cv_clear();
    CHECK_INT(flat_glyph_dleft(0, 0, 13, 9, 2, '#', cv_fill, 0), 6);              /* hires: half the rows, same columns */
    CHECK(memcmp(cv[3], "????##??##???", 13) == 0);
    CHECK(memcmp(cv[4], "??########???", 13) == 0);
    CHECK(memcmp(cv[5], "????##??##???", 13) == 0);
    CHECK(memcmp(cv[2], "?????????????", 13) == 0);
    CHECK_INT(flat_glyph_dleft(0, 0, 4, 9, 1, '#', cv_fill, 0), 0);               /* too narrow: stock image */
    CHECK_INT(flat_glyph_dleft(0, 0, 13, 4, 1, '#', cv_fill, 0), 0);              /* too low */
    CHECK_INT(flat_glyph_dleft(0, 0, 13, 9, 1, '#', 0, 0), 0);
}

static void test_dopus_box_outline(void)
{
    flat_state s;
    int x, y;
    flat_init(&s);
    s.enabled = 1;
    s.pen_line = 7;
    s.side = 1;
    cv_clear();
    CHECK_INT(flat_dopus_box_draw(&s, 2, 3, 11, 8, cv_fill, 0), 1);
    for (y = 0; y < CV; y++)
        for (x = 0; x < CV; x++) {
            int in = x >= 2 && x <= 11 && y >= 3 && y <= 8;
            int edge = in && (x == 2 || x == 11 || y == 3 || y == 8);
            if (cv[y][x] != (edge ? 7 : '?'))
                CHECK_INT(cv[y][x], edge ? 7 : '?');
        }
    s.side = 2;                                     /* a tall-pixel screen: two outline columns at each side */
    cv_clear();
    CHECK_INT(flat_dopus_box_draw(&s, 2, 3, 11, 8, cv_fill, 0), 1);
    CHECK_INT(cv[5][3], 7);
    CHECK_INT(cv[5][10], 7);
    CHECK_INT(cv[5][4], '?');
    CHECK_INT(cv[4][4], '?');
    cv_clear();
    CHECK_INT(flat_dopus_box_draw(&s, 2, 3, 3, 8, cv_fill, 0), 0);   /* under 3x3: nothing, caller draws nothing */
    CHECK_INT(cv[3][2], '?');
    CHECK_INT(flat_dopus_box_draw(&s, 2, 3, 11, 8, 0, 0), 0);
}

/* DOPUS-BUTTON-NO-RING (owner s70 O5-O7): the ring rule and the face-pen band. Mirrors d4_flat_pens: when the decision is
 * SKIP and flat_dopus_ring says 0, the border is painted in the interior's pen (stand-in for ReadPixel: 4) through
 * flat_dopus_box_draw; ring-keeping elements paint pen_line. Revert check: ring in pen_line fails the face asserts. */
static void test_dopus_no_ring(void)
{
    flat_state s;
    int x, y, side;
    CHECK_INT(flat_dopus_ring(FLAT_EL_DRAWBOX, 0), 0);        /* raised Program button: face */
    CHECK_INT(flat_dopus_ring(FLAT_EL_DRAWBOX, 1), 1);        /* pressed toolbar button stays outlined */
    CHECK_INT(flat_dopus_ring(FLAT_EL_FIELDBOX, 0), 0);       /* string fields: no ring (owner 2026-11-10, s71) */
    /* d0 of L_DrawBox is a BOOL (WORD): the high word is garbage (DOpus info-field frame: d0 = 0x22000000, measured s71) */
    CHECK_INT(flat_dopus_recessed(0x22000000L), 0);
    CHECK_INT(flat_dopus_recessed(0), 0);
    CHECK_INT(flat_dopus_recessed(1), 1);
    CHECK_INT(flat_dopus_recessed(0x0000FFFFL), 1);
    CHECK_INT(flat_dopus_recessed(0x00010000L), 0);
    CHECK_INT(flat_dopus_ring(FLAT_EL_DRAWBOX, flat_dopus_recessed(0x22000000L)), 0);
    CHECK_INT(flat_dopus_face_pen(4, 0), 4);                  /* read ok: the interior pen */
    CHECK_INT(flat_dopus_face_pen(-1, 0), 0);                 /* read failed: the background pen, never the accent */
    CHECK_INT(flat_dopus_face_pen(0, 5), 0);                  /* pen 0 is a valid read */
    CHECK_INT(flat_dopus_ring(FLAT_EL_BEVEL, 0), 1);          /* Environment class-drawn buttons keep the ring (O7) */
    for (side = 1; side <= 2; side++) {
        flat_init(&s);
        s.enabled = 1;
        s.pen_line = 7;
        s.side = (uint8_t)side;
        cv_clear();
        if (!flat_dopus_ring(FLAT_EL_DRAWBOX, 0))
            s.pen_line = 4;                                    /* the face pen */
        CHECK_INT(flat_dopus_box_draw(&s, 2, 3, 11, 8, cv_fill, 0), 1);
        for (y = 0; y < CV; y++)
            for (x = 0; x < CV; x++) {
                int in = x >= 2 && x <= 11 && y >= 3 && y <= 8;
                int edge = in && (x < 2 + side || x > 11 - side || y == 3 || y == 8);
                int want = edge ? 4 : '?';
                if (cv[y][x] != want)
                    CHECK_INT(cv[y][x], want);
                if (cv[y][x] == 7)
                    CHECK_INT(1, 0);                           /* no pen_line pixel anywhere */
            }
    }
}

/* the fake dispatcher: the same three steps as flat.c c2_disp_reg / c2_disp_stk */
static int fake_dispatch(const flat_state *st, unsigned long which, unsigned long method, unsigned long ids, int ox, int oy, int w, int h)
{
    g_disp_calls++;
    if (flat_glyph_draw(st, which, method, ids, ox, oy, w, h, g_frame_pen, st->pen_text, -1, 1, cv_fill, 0))
        return 1;
    g_old_calls++;                                      /* the saved routine */
    return 0;
}

static void glyph_state(flat_state *st)
{
    flat_init(st);
    st->enabled = 1;
    st->pen_text = '#';
    st->pen_line = '+';
    st->pen_back = '.';
}

/* DOPUS-LOCK (S2b.8): the solid padlock at side 1 and 2; open vs closed shackle, same pens, box never inverted */
static int lock_rows(const flat_state *st, unsigned long ids, int side, int w, int h, char rows[][CV + 1])
{
    int y, x, r;
    cv_clear();
    r = flat_glyph_draw(st, FLAT_W_LOCK, FLAT_IM_DRAW, ids, 3, 2, w, h, -1, st->pen_text, -1, side, cv_fill, 0);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++)
            rows[y][x] = cv[2 + y][3 + x];
        rows[y][w] = 0;
    }
    return r;
}

/* S2b.3 / S2b.8: the type map d5_draw applies to BoopsiImageData.type, then the draw it routes to (the sentinel: pixels set). */
static void test_dopus_image_which(void)
{
    flat_state st;
    char rows[CV][CV + 1];
    int type, y, x, lit, which, routed = 0;
    glyph_state(&st);
    CHECK_INT(flat_dopus_image_which(6), FLAT_W_ICONIFY);
    CHECK_INT(flat_dopus_image_which(8), FLAT_W_LOCK);
    for (type = -1; type < 40; type++) {
        if (type == 6 || type == 8)
            continue;
        CHECK_INT(flat_dopus_image_which(type), -1);       /* arrows, BBOX, gauge ...: stock */
    }
    for (type = 6; type <= 8; type++) {
        which = flat_dopus_image_which(type);
        if (which < 0)
            continue;
        CHECK_INT(which == FLAT_W_LOCK ? lock_rows(&st, 0, 1, 16, 14, rows) : 1, 1);
        routed++;
    }
    CHECK_INT(routed, 2);
    lit = 0;
    for (y = 0; y < 14; y++)
        for (x = 0; x < 16; x++)
            lit += rows[y][x] == '#';
    CHECK_INT(lit > 0, 1);                                  /* the lock type reached the glyph and drew pixels */
}

/* GLYPH-TEXT-PEN: DOpus lock and iconify on the title band draw in the title text pen the band gives them (c2_get_pens p->text),
 * the same pen the cloned zoom and depth take; before, the flat glyphs ignored it and drew in pen_text. */
static void test_band_glyphs_take_text_pen(void)
{
    static const unsigned long whichs[3] = { FLAT_W_LOCK, FLAT_W_LOCK, FLAT_W_ICONIFY };
    static const unsigned long idss[3] = { 0, 1, 0 };          /* open lock, closed lock (IDS_SELECTED), iconify */
    flat_state st;
    int i, y, x, side, text, other;
    glyph_state(&st);
    for (side = 1; side <= 2; side++)
        for (i = 0; i < 3; i++) {
            cv_clear();
            CHECK_INT(flat_glyph_draw(&st, whichs[i], FLAT_IM_DRAW, idss[i], 0, 0, 16 * side, 14, '=', -1, '@', side, cv_fill, 0), 1);
            text = other = 0;
            for (y = 0; y < 14; y++)
                for (x = 0; x < 16 * side; x++) {
                    text += cv[y][x] == '@';
                    other += cv[y][x] != '@' && cv[y][x] != '=';
                }
            CHECK_INT(text > 0, 1);                 /* the glyph lines are in the text pen */
            CHECK_INT(other, 0);                    /* no pen_text pixel: only band and text pen */
        }
    /* one rule for both paths: the clone's glyph pen equals the flat glyph's */
    CHECK_INT(flat_glyph_fg_pen(&st, FLAT_ROLE_TEXT, '@', '=', -1), '@');
    CHECK_INT(flat_glyph_fg_pen(&st, FLAT_ROLE_TEXT, -1, '=', -1), '#');
    CHECK_INT(flat_glyph_fg_pen(&st, FLAT_ROLE_BACK, '@', '=', -1), flat_clone_pen(&st, FLAT_ROLE_BACK, '=', -1));
}

static void test_lock_glyph(void)
{
    static const char *const want_open[14] = { "................", "................", "................",
        "......###.......", "......#.#.......", "......#.........", "......#.........", ".....#####......",
        ".....#####......", ".....#####......", ".....#####......", "................", "................",
        "................" };
    flat_state st;
    char op[CV][CV + 1], cl[CV][CV + 1], ins[CV][CV + 1];
    int side, y, w, h = 14, diff;
    glyph_state(&st);
    CHECK_INT(flat_glyph_known(FLAT_W_LOCK), 1);
    CHECK_INT(flat_glyph_known(FLAT_W_LOCKED), 1);
    CHECK_INT(flat_glyph_window_gadget(FLAT_W_LOCK), 1);
    CHECK_INT(flat_glyph_on_band(FLAT_W_LOCKED), 1);
    CHECK_INT(flat_clone_route(&st, FLAT_W_LOCK, FLAT_IM_DRAW), 0);        /* never a cloned ROM image */
    CHECK_INT(flat_clone_route(&st, FLAT_W_LOCKED, FLAT_IM_DRAW), 0);
    CHECK_INT(flat_clone_route(&st, FLAT_W_CLOSE, FLAT_IM_DRAW), 1);       /* the others keep the clone */
    for (side = 1; side <= 2; side++) {
        w = 16 * side;
        CHECK_INT(lock_rows(&st, 0, side, w, h, op), 1);
        CHECK_INT(lock_rows(&st, 1, side, w, h, cl), 1);                   /* IDS_SELECTED: closed */
        CHECK_INT(lock_rows(&st, 6, side, w, h, ins), 1);                  /* IDS_INACTIVESELECTED: closed shape */
        for (y = 0; y < h; y++) {                                          /* inactive selected: the closed shape in the line pen */
            int x;
            for (x = 0; x < w; x++)
                CHECK((ins[y][x] == '+') == (cl[y][x] == '#'));
        }
        CHECK(op[0][0] == '.' && cl[0][0] == '.');                         /* the box keeps its pen: not inverted */
        for (diff = 0, y = 0; y < h; y++) {
            diff += strcmp(op[y], cl[y]) != 0;
            CHECK(strchr(cl[y], '?') == NULL && strchr(op[y], '?') == NULL);   /* every pixel of the box painted */
        }
        CHECK_INT(diff, 2);                                                /* open vs closed: the two rows of the right leg */
        CHECK(strcmp(op[3], cl[3]) == 0 && strcmp(op[7], cl[7]) == 0);     /* top row and body are the same */
        CHECK(cl[3][8 * side - 1] == '#' && cl[10][8 * side - 1] == '#');          /* shackle top, body bottom */
        CHECK(cl[2][8 * side - 1] == '.' && cl[11][8 * side - 1] == '.');          /* nothing above or below */
    }
    for (y = 0; y < h; y++) {                                              /* side 1 open: the golden */
        lock_rows(&st, 0, 1, 16, h, op);
        CHECK(strcmp(op[y], want_open[y]) == 0);
    }
    CHECK_INT(lock_rows(&st, 0, 1, 6, 6, op), 0);                          /* under FLAT_GLYPH_MIN_SIZE: old hook */
}

static void test_glyph_goldens(void)
{
    static const struct { const char *name; unsigned long which; int w, h; const char *rows; } gold[] = {
#include "data/flat_glyph_golden.inc"
    };
    flat_state st;
    unsigned i;
    glyph_state(&st);
    CHECK_INT(sizeof gold / sizeof gold[0], 24);                    /* 8 glyphs x 3 box sizes */
    for (i = 0; i < sizeof gold / sizeof gold[0]; i++) {
        char got[CV * (CV + 1) + 1], *o = got;
        const char *r = gold[i].rows;
        int y, x, ok = 1;
        cv_clear();
        CHECK_INT(fake_dispatch(&st, gold[i].which, FLAT_IM_DRAW, 0, 3, 2, gold[i].w, gold[i].h), 1);
        for (y = 0; y < gold[i].h && ok; y++) {                      /* the box sits at (3, 2); the golden row is w chars and '|' */
            for (x = 0; x < gold[i].w; x++)
                *o++ = cv[2 + y][3 + x];
            *o++ = '|';
            *o = 0;
            if (strncmp(o - gold[i].w - 1, r, gold[i].w + 1)) {
                ok = 0;
                printf("  glyph %s %dx%d row %d: got [%.*s] want [%.*s]\n", gold[i].name, gold[i].w, gold[i].h, y,
                       gold[i].w, o - gold[i].w - 1, gold[i].w, r);
            }
            r += gold[i].w + 1;
        }
        CHECK(ok);
        CHECK(cv[1][3] == '?' && cv[2 + gold[i].h][3] == '?' && cv[2][2] == '?' && cv[2][3 + gold[i].w] == '?');   /* nothing outside the box */
    }
}

/* FLAT-GADGET-BG: the window gadgets' box takes the frame pen; selected and menu glyphs keep their own background */
static void test_glyph_frame_bg(void)
{
    flat_state st;
    glyph_state(&st);
    g_frame_pen = '7';
    cv_clear();
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 14, 12);              /* normal */
    CHECK_INT(cv[0][0], '7');
    CHECK_INT(cv[11][13], '7');
    cv_clear();
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 5, 0, 0, 14, 12);              /* inactive */
    CHECK_INT(cv[0][0], '7');
    cv_clear();
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 1, 0, 0, 14, 12);              /* selected: inverted, not the frame */
    CHECK_INT(cv[0][0], '#');
    cv_clear();
    fake_dispatch(&st, FLAT_W_MENUCHECK, FLAT_IM_DRAW, 0, 0, 0, 14, 12);          /* menu glyph: the back pen */
    CHECK_INT(cv[0][0], '.');
    g_frame_pen = -1;
    cv_clear();
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 14, 12);              /* no frame pen known: the back pen */
    CHECK_INT(cv[0][0], '.');
}

static void test_glyph_states(void)
{
    flat_state st;
    flat_prim n[FLAT_GLYPH_MAX_PRIMS], s[FLAT_GLYPH_MAX_PRIMS], d[FLAT_GLYPH_MAX_PRIMS];
    int cn, cs, cd, i;
    glyph_state(&st);
    cn = flat_glyph_plan(FLAT_W_CLOSE, 14, 12, FLAT_G_NORMAL, n, FLAT_GLYPH_MAX_PRIMS);
    cs = flat_glyph_plan(FLAT_W_CLOSE, 14, 12, FLAT_G_SELECTED, s, FLAT_GLYPH_MAX_PRIMS);
    cd = flat_glyph_plan(FLAT_W_CLOSE, 14, 12, FLAT_G_INACTIVE, d, FLAT_GLYPH_MAX_PRIMS);
    CHECK_INT(cn, 9);                                               /* the box and two runs per row of the 4 cross rows */
    CHECK_INT(cs, cn);
    CHECK_INT(cd, cn);
    CHECK_INT(n[0].role, FLAT_ROLE_FRAME);                          /* first rect = the box in the frame's colour */
    CHECK_INT(s[0].role, FLAT_ROLE_TEXT);                           /* selected inverts */
    CHECK_INT(d[0].role, FLAT_ROLE_FRAME);
    for (i = 1; i < cn; i++) {
        CHECK_INT(n[i].role, FLAT_ROLE_TEXT);
        CHECK_INT(s[i].role, FLAT_ROLE_BACK);
        CHECK_INT(d[i].role, FLAT_ROLE_LINE);                       /* inactive: only the pen differs */
        CHECK(n[i].x0 == s[i].x0 && n[i].y1 == d[i].y1);            /* same geometry in every state */
    }
    CHECK_INT(flat_glyph_state(0), FLAT_G_NORMAL);
    CHECK_INT(flat_glyph_state(1), FLAT_G_SELECTED);
    CHECK_INT(flat_glyph_state(5), FLAT_G_INACTIVE);
    CHECK_INT(flat_glyph_state(7), FLAT_G_INACTIVE);
    /* drawn through the dispatcher: the selected box is the text pen with background-pen lines */
    cv_clear();
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 1, 0, 0, 14, 12);
    CHECK_INT(cv[0][0], '#');
    CHECK_INT(cv[4][4], '.');
    cv_clear();
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 5, 0, 0, 14, 12);
    CHECK_INT(cv[0][0], '.');
    CHECK_INT(cv[4][4], '+');
    /* S93-GADGET-GLYPH: the close gadget is a cross inside the outline; the ground shows beside the diagonals */
    cv_clear();
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 14, 12);
    CHECK_INT(cv[4][4], '#');
    CHECK_INT(cv[5][6], '#');
    CHECK_INT(cv[4][6], '.');
    CHECK_INT(cv[5][3], '.');
    CHECK_INT(cv[0][0], '.');
}

/* FLAT-SOLID-FILL: zoom and iconify hold a filled block in every state (close and depth are outlines since S93-GADGET-GLYPH),
 * and an inactive window differs from an active one in colour only (same rectangles, only the role of the glyph pen changes). */
static void test_glyph_solid_and_state_equal(void)
{
    static const unsigned long win[] = { FLAT_W_CLOSE, FLAT_W_DEPTH, FLAT_W_ZOOM, FLAT_W_SIZE, FLAT_W_ICONIFY };
    static const int dim[2][2] = { { 14, 12 }, { 24, 20 } };
    static const int states[2] = { FLAT_G_NORMAL, FLAT_G_INACTIVE };
    unsigned i;
    int k, z, j;
    for (i = 0; i < sizeof win / sizeof win[0]; i++) {
        for (z = 0; z < 2; z++) {
            flat_prim p[2][FLAT_GLYPH_MAX_PRIMS];
            int c[2];
            for (k = 0; k < 2; k++)
                c[k] = flat_glyph_plan(win[i], dim[z][0], dim[z][1], states[k], p[k], FLAT_GLYPH_MAX_PRIMS);
            CHECK(c[0] > 1);
            CHECK_INT(c[1], c[0]);                                  /* same number of rectangles */
            for (k = 0; k < 2; k++) {
                int solid = 0;                                      /* a glyph-pen rectangle at least 2 x 2: a block, not a line */
                for (j = 1; j < c[k]; j++)
                    if (p[k][j].role != FLAT_ROLE_FRAME && p[k][j].role != FLAT_ROLE_BACK &&
                        p[k][j].x1 - p[k][j].x0 >= 1 && p[k][j].y1 - p[k][j].y0 >= 1)
                        solid = 1;
                CHECK(solid || win[i] == FLAT_W_SIZE ||          /* S69-ARROW-ASPECT: the size triangle is rows 1 px high (shape: test_arrow_s69_shape) */
                      win[i] == FLAT_W_CLOSE || win[i] == FLAT_W_DEPTH);   /* outlines: test_glyph_gadgets_distinct */
            }
            for (j = 0; j < c[0]; j++) {                            /* identical geometry; only the glyph pen role moves */
                CHECK(p[0][j].x0 == p[1][j].x0 && p[0][j].y0 == p[1][j].y0 &&
                      p[0][j].x1 == p[1][j].x1 && p[0][j].y1 == p[1][j].y1);
                if (p[0][j].role != p[1][j].role)
                    CHECK(p[0][j].role == FLAT_ROLE_TEXT && p[1][j].role == FLAT_ROLE_LINE);
            }
        }
    }
}

/* S93-GADGET-GLYPH (owner, OS 3.1 rig 93: "plain squares in different positions"): close, depth, zoom and iconify each draw
 * a different shape; depth, zoom and iconify share one outer frame. Close = a bare cross, no frame; depth = the frame holding a smaller
 * outline low left; zoom = an outline holding a solid block in its top-left corner; iconify = a solid block. */
static void glyph_raster(unsigned long which, int w, int h, char c[24][24])
{
    flat_prim p[FLAT_GLYPH_MAX_PRIMS];
    int n = flat_glyph_plan(which, w, h, FLAT_G_NORMAL, p, FLAT_GLYPH_MAX_PRIMS), j, x, y;
    CHECK(n > 1);
    for (j = 0; j < n; j++)
        for (y = p[j].y0; y <= p[j].y1; y++)
            for (x = p[j].x0; x <= p[j].x1; x++)
                c[y][x] = p[j].role == FLAT_ROLE_TEXT ? '#' : '.';
}
static void test_glyph_gadgets_distinct(void)
{
    static const unsigned long win[4] = { FLAT_W_CLOSE, FLAT_W_DEPTH, FLAT_W_ZOOM, FLAT_W_ICONIFY };
    static const int dim[2][2] = { { 14, 12 }, { 24, 20 } };
    static char c[4][24][24];
    int z, i, k, x, y;
    for (z = 0; z < 2; z++) {
        int w = dim[z][0], h = dim[z][1], fx0 = w, fy0 = h, fx1 = -1, fy1 = -1, cx0 = w, cy0 = h, cx1 = -1, cy1 = -1, holes = 0, corners = 0;
        memset(c, 0, sizeof c);
        for (i = 0; i < 4; i++)
            glyph_raster(win[i], w, h, c[i]);
        for (i = 0; i < 4; i++)                                     /* pairwise different pixels */
            for (k = i + 1; k < 4; k++)
                CHECK(memcmp(c[i], c[k], sizeof c[i]) != 0);
        for (y = 0; y < h; y++)                                     /* the cross's ink bounds, and the shared frame's (depth ink) */
            for (x = 0; x < w; x++) {
                if (c[0][y][x] == '#') {
                    cx0 = x < cx0 ? x : cx0; cy0 = y < cy0 ? y : cy0; cx1 = x > cx1 ? x : cx1; cy1 = y > cy1 ? y : cy1;
                }
                if (c[1][y][x] == '#') {
                    fx0 = x < fx0 ? x : fx0; fy0 = y < fy0 ? y : fy0; fx1 = x > fx1 ? x : fx1; fy1 = y > fy1 ? y : fy1;
                }
            }
        CHECK(cx1 - cx0 >= 2 && cy1 - cy0 >= 2);
        CHECK_INT(c[0][(cy0 + cy1) / 2][(cx0 + cx1) / 2], '#');    /* close: a bare cross, centre inked */
        CHECK(cx0 > fx0 && cx1 < fx1 && cy0 > fy0 && cy1 < fy1);   /* no ink on the gadget box's outer ring: the cross sits in the inner area */
        for (y = cy0; y <= cy1; y++)                                /* symmetric left to right and top to bottom */
            for (x = cx0; x <= cx1; x++) {
                CHECK(c[0][y][x] == c[0][y][cx0 + cx1 - x]);
                CHECK(c[0][y][x] == c[0][cy0 + cy1 - y][x]);
            }
        for (y = 1; y < h - 1; y++)                                 /* depth: two outlines = two separate inner holes and */
            for (x = 1; x < w - 1; x++)                             /* two top-left outline corners, the back one up-right */
                if (c[1][y][x] == '#' && c[1][y][x + 1] == '#' && c[1][y + 1][x] == '#' && c[1][y - 1][x] != '#' &&
                    c[1][y][x - 1] != '#')
                    corners++;
                else if (c[1][y][x] == '.' && c[1][y - 1][x] == '#' && c[1][y][x - 1] == '#' && c[1][y - 1][x - 1] == '#')
                    holes++;
        CHECK(corners == 2 || (z == 0 && corners == 1));            /* the small size: the front window touches the frame top, one corner */
        CHECK(holes >= 2);                                          /* the overlap can close a third pocket */
        for (y = 0, k = 0; y < h; y++)                              /* zoom: a 3x3 solid block inside an outline */
            for (x = 0; x + 2 < w && y + 2 < h; x++)
                if (c[2][y][x] == '#' && c[2][y][x + 2] == '#' && c[2][y + 2][x] == '#' && c[2][y + 1][x + 1] == '#' &&
                    c[2][y + 2][x + 2] == '#')
                    k = 1;
        CHECK(k);
        for (i = 2; i < 4; i++)                                     /* depth, zoom and iconify share one outer frame: same ring pixels */
            for (y = fy0; y <= fy1; y++)
                for (x = fx0; x <= fx1; x++)
                    if (y == fy0 || y == fy1 || x == fx0 || x == fx1)
                        CHECK_INT(c[i][y][x], c[1][y][x]);
        CHECK_INT(c[3][fy1 - 2][fx0 + 2], '#');                     /* iconify: a solid block in the lower left, inside the frame */
        CHECK_INT(c[3][fy1 - 2][fx0 + 1], '.');                     /* one ground column between block and frame */
    }
}

/* RL19 SELECTA-AA-ROUND: antialiased corners. Palette: pen 0 background 0x000000, pen 1 box 0xFFFFFF, pens 2..4 greys 0x404040 0x808080 0xC0C0C0 */
static void round_fill(void *ctx, int x0, int y0, int x1, int y1, int pen);
static int round_read(void *ctx, int x, int y);
static const unsigned long raa_test_pal[5] = { 0x000000UL, 0xFFFFFFUL, 0x404040UL, 0x808080UL, 0xC0C0C0UL };
static unsigned long raa_test_rgb(void *ctx, int pen) { (void)ctx; return raa_test_pal[pen]; }
static void test_round_aa(void)
{
    struct round_canvas { int pix[40][60]; } *cv = (struct round_canvas *)calloc(1, sizeof *cv);
    flat_state fs;
    int x, y, blended = 0, mid = 0, stock0, pen_bd, ground_px, other_px;
    memset(&fs, 0, sizeof fs);
    fs.enabled = 1; fs.pen_line = 1; fs.pen_back = 0; fs.pen_button = 4; fs.pen_text = 2;
    /* raa_cover: inside 255, outside 0, the edge between */
    CHECK_INT(raa_cover(20, 14, 4, 10, 7), 255);
    CHECK_INT(raa_cover(20, 14, 4, 4, 0), 255);                     /* straight edge beside the corner square */
    CHECK_INT(raa_cover(20, 14, 4, 0, 0), 0);                       /* the very corner pixel: outside the circle */
    CHECK_INT(raa_cover(20, 14, 4, 19, 13), 0);
    CHECK_INT(raa_cover(20, 14, 4, 3, 3), 255);                     /* nearest the circle centre */
    CHECK_INT(raa_cover(20, 14, 4, 1, 1) > 0 && raa_cover(20, 14, 4, 1, 1) < 255, 1);
    CHECK_INT(raa_cover(20, 14, 4, 2, 0) > 0 && raa_cover(20, 14, 4, 2, 0) < 255, 1);
    CHECK_INT(raa_cover(20, 14, 4, 19, 0) == raa_cover(20, 14, 4, 0, 0) && raa_cover(20, 14, 4, 0, 13) == raa_cover(20, 14, 4, 0, 0), 1);   /* mirrored */
    CHECK_INT(raa_cover(20, 14, 4, 20, 0), 0);                      /* off the box */
    CHECK_INT(raa_round_inset(20, 14, 4, 0) >= raa_round_inset(20, 14, 4, 1) && raa_round_inset(20, 14, 4, 4), 0);
    /* raa_radius: 2 under 12 px high, else 4 */
    CHECK_INT(raa_radius(11), 2);
    CHECK_INT(raa_radius(12), 4);
    /* raa_nearest from a known palette */
    CHECK_INT(raa_nearest(raa_test_rgb, 0, 5, 0x000000UL), 0);
    CHECK_INT(raa_nearest(raa_test_rgb, 0, 5, 0xFFFFFFUL), 1);
    CHECK_INT(raa_nearest(raa_test_rgb, 0, 5, 0x7F7F7FUL), 3);
    CHECK_INT(raa_nearest(raa_test_rgb, 0, 5, 0x454545UL), 2);
    CHECK_INT(raa_nearest(raa_test_rgb, 0, 5, 0xBBBBBBUL), 4);
    /* stock corners for the same box, to compare against */
    for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 0;
    CHECK_INT(flat_frame_draw_round(&fs, 0, 10, 10, 20, 14, 1, 0, round_fill, round_read, cv), 1);
    stock0 = cv->pix[11][11];
    CHECK_INT(stock0 == 1 || stock0 == 0, 1);                       /* stock: hard, one of fill or backdrop */
    /* the antialiased box: intermediate pens appear in the corners, the middle keeps the fill */
    for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 0;
    CHECK_INT(raa_frame_draw(&fs, 0, 9, 9, 22, 16, 1, 0, round_fill, round_read, raa_test_rgb, 5, cv, &blended), 1);
    CHECK_INT(cv->pix[17][20], 1);                                  /* body */
    CHECK_INT(cv->pix[10][10], 0);                                  /* corner pixel: backdrop */
    for (y = 10; y < 14; y++) for (x = 10; x < 14; x++) if (cv->pix[y][x] >= 2) mid++;
    CHECK_INT(mid > 0, 1);                                          /* grey pens in the corner: the blend ran */
    CHECK_INT(blended > 0, 1);
    pen_bd = cv->pix[10][12];
    CHECK_INT(pen_bd >= 2 && pen_bd <= 4, 1);                       /* edge pixel (2,0): between box and backdrop */
    CHECK_INT(cv->pix[10][27] >= 2 && cv->pix[23][29] == 0, 1);     /* other corners alike */
    /* RL19 right corners: a wide button (40x14, the rig's is 80x14) blends all four corners alike, mirrored, on a plain backdrop */
    for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 0;
    CHECK_INT(raa_frame_draw(&fs, 0, 9, 9, 42, 16, 1, 0, round_fill, round_read, raa_test_rgb, 5, cv, &blended), 1);
    for (y = 0, mid = 0; y < 4; y++)
        for (x = 0; x < 4; x++) {
            int tl = cv->pix[10 + y][10 + x];
            CHECK_INT(cv->pix[10 + y][49 - x], tl);
            CHECK_INT(cv->pix[23 - y][10 + x], tl);
            CHECK_INT(cv->pix[23 - y][49 - x], tl);
            mid += tl >= 2;
        }
    CHECK_INT(mid > 0, 1);
    /* RL19 rig reading: on the btnbgprobe 2x2 checker the blend mixes the ONE pen diagonally outside each corner. FLAT-SPACING:
     * that pen now lies on the band the inset painted in pen_back (0), so all four corners blend against the window ground alike,
     * whatever the checker held there. */
    for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = ((x >> 1) + (y >> 1)) & 1 ? 1 : 0;
    CHECK_INT(cv->pix[9][9] == 0 && cv->pix[9][50] == 1 && cv->pix[24][9] == 0 && cv->pix[24][50] == 1, 1);   /* the rig's phase */
    CHECK_INT(raa_frame_draw(&fs, 0, 9, 9, 42, 16, 1, 0, round_fill, round_read, raa_test_rgb, 5, cv, &blended), 1);
    for (y = 0; y < 4; y++)
        for (x = 0; x < 4; x++) {
            CHECK_INT(cv->pix[10 + y][10 + x], cv->pix[23 - y][10 + x]);                /* left corners: blended with pen 0 */
            CHECK_INT(cv->pix[10 + y][49 - x], cv->pix[10 + y][10 + x]);                /* right corners: the band's pen 0 too */
            CHECK_INT(cv->pix[23 - y][49 - x], cv->pix[10 + y][10 + x]);
        }
    CHECK_INT(cv->pix[10][12] >= 2 && cv->pix[10][12] <= 4, 1);                         /* left edge pixel still between */
    CHECK_INT(cv->pix[9][50] == 0 && cv->pix[24][50] == 0 && cv->pix[16][9] == 0 && cv->pix[16][50] == 0, 1);   /* the band is ground */
    /* fallbacks: the stock corners, no blend */
    for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 0;
    CHECK_INT(raa_frame_draw(&fs, 0, 9, 9, 22, 16, 1, 0, round_fill, round_read, raa_test_rgb, 0, cv, &blended), 1);
    CHECK_INT(blended, 0);
    CHECK_INT(cv->pix[11][11], stock0);
    CHECK_INT(cv->pix[10][10], 0);                                  /* stock cut still made */
    CHECK_INT(raa_frame_draw(&fs, 0, 9, 9, 22, 16, 1, 0, round_fill, round_read, raa_test_rgb, 300, cv, &blended) && !blended, 1);
    CHECK_INT(raa_frame_draw(&fs, 0, 10, 10, 20, 14, 0, 0, round_fill, round_read, raa_test_rgb, 5, cv, &blended) && !blended, 1);   /* unfilled */
    CHECK_INT(raa_frame_draw(&fs, 0, 9, 9, 22, 16, 1, FLAT_ROLE_SECONDARY, round_fill, round_read, raa_test_rgb, 5, cv, &blended), 1);   /* a secondary is a fill: antialiased like the primary */
    for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 9;       /* pen 9 is not in a 5 pen palette: unreadable */
    fs.pen_back = 9;                                                            /* the band the inset paints stays unreadable */
    CHECK_INT(raa_frame_draw(&fs, 0, 9, 9, 22, 16, 1, 0, round_fill, round_read, raa_test_rgb, 5, cv, &blended) && !blended, 1);
    CHECK_INT(cv->pix[10][10], 9);
    fs.pen_back = 0;
    /* the scroller knob takes the same path */
    memset(&fs, 0, sizeof fs);
    fs.enabled = 1; fs.pen_text = 2; fs.pen_line = 3; fs.pen_back = 0; fs.pen_outline = 3; fs.pen_button = 4; fs.pen_knob = 1; fs.pen_inbar = 1;
    for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 0;
    blended = 0;
    CHECK_INT(raa_knob_frame_draw(&fs, 8, 1, 1, 9, 9, 12, 20, round_fill, round_read, raa_test_rgb, 5, cv, &blended), 1);
    CHECK_INT(blended > 0, 1);
    CHECK_INT(cv->pix[10][10], 0);
    CHECK_INT(cv->pix[14][14] == flat_knob_state_pen(&fs, 8, 1), 1);
    mid = 0;
    for (y = 10; y < 14; y++) for (x = 10; x < 14; x++) if (cv->pix[y][x] >= 2) mid++;
    CHECK_INT(mid > 0, 1);
    /* INPUT-GROUND: ground pen >= 0 = the whole box in that pen, rounded as the buttons, no outline; pen < 0 draws nothing */
    for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 0;
    blended = 0;
    CHECK_INT(raa_ground_frame_draw(&fs, -1, 1, 9, 9, 32, 16, round_fill, round_read, raa_test_rgb, 5, cv, &blended), 0);
    CHECK_INT(cv->pix[12][20], 0);
    CHECK_INT(raa_ground_frame_draw(&fs, 2, 1, 9, 9, 32, 16, round_fill, round_read, raa_test_rgb, 5, cv, &blended), 1);
    ground_px = 0; other_px = 0;
    for (y = 10; y < 24; y++) for (x = 10; x < 40; x++) {
        int corner = (x < 14 || x >= 36) && (y < 14 || y >= 20);
        if (cv->pix[y][x] == 2) ground_px++;
        else if (!corner) other_px++;                          /* outside the four corner squares every pixel is the ground */
    }
    CHECK_INT(cv->pix[16][25], 2);                                  /* interior */
    CHECK_INT(cv->pix[10][20] == 2 && cv->pix[23][20] == 2 && cv->pix[16][10] == 2 && cv->pix[16][39] == 2, 1);   /* edges are ground: no outline, no inner frame */
    CHECK_INT(cv->pix[10][10] != 2, 1);                             /* corner pixel is cut */
    CHECK_INT(ground_px > 30 * 14 - 16, 1);
    CHECK_INT(other_px, 0);
    /* C8-LOOK: the 3.1 cycle and listview boxes are this ground box (flat.c ground_frame). Non-square pixels (side 2), backdrop 0:
     * fill and edge centres are the ground pen 2, no pen_line (3) anywhere, the corner pixels are cut on both ends */
    for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 0;
    CHECK_INT(raa_ground_frame_draw(&fs, 2, 2, 5, 5, 44, 14, round_fill, round_read, raa_test_rgb, 5, cv, &blended), 1);
    other_px = 0;
    for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) if (cv->pix[y][x] == 3) other_px++;
    CHECK_INT(other_px, 0);
    CHECK_INT(cv->pix[12][27] == 2 && cv->pix[6][27] == 2 && cv->pix[17][27] == 2 && cv->pix[12][7] == 2 && cv->pix[12][46] == 2, 1);
    CHECK_INT(cv->pix[6][7] != 2 && cv->pix[6][46] != 2 && cv->pix[17][7] != 2 && cv->pix[17][46] != 2, 1);
    free(cv);
}

static void test_glyph_route(void)
{
    flat_state st;
    static const unsigned long unknown[] = { 0x04, 0x0E, 0x0F, 0x05, 0x1B, 0x2A, 0xFFFF };
    static const unsigned long other[] = { 0x101UL /* OM_NEW */, 0x103UL /* OM_GET */, 0x203UL /* IM_HITTEST */,
                                           0x204UL /* IM_ERASE */, 0x206UL /* IM_DRAWFRAME */, 0x207UL /* IM_FRAMEBOX */, 0 };
    unsigned i;
    glyph_state(&st);
    g_old_calls = g_disp_calls = 0;
    for (i = 0; i < sizeof unknown / sizeof unknown[0]; i++) {
        cv_clear();
        CHECK_INT(fake_dispatch(&st, unknown[i], FLAT_IM_DRAW, 0, 0, 0, 14, 12), 0);
        CHECK_INT(cv[0][0], '?');                                    /* nothing drawn */
    }
    CHECK_INT(g_old_calls, (int)(sizeof unknown / sizeof unknown[0]));      /* every unknown Which reached the old hook */
    g_old_calls = 0;
    for (i = 0; i < sizeof other / sizeof other[0]; i++) {
        cv_clear();
        CHECK_INT(fake_dispatch(&st, FLAT_W_CLOSE, other[i], 0, 0, 0, 14, 12), 0);
        CHECK_INT(cv[0][0], '?');
    }
    CHECK_INT(g_old_calls, (int)(sizeof other / sizeof other[0]));          /* every other method too */
    /* switch off, held, box too small: the old hook, nothing drawn */
    g_old_calls = 0;
    cv_clear();
    st.enabled = 0;
    CHECK_INT(fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 14, 12), 0);
    st.enabled = 1;
    st.held = 1;
    CHECK_INT(fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 14, 12), 0);
    st.held = 0;
    CHECK_INT(fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, FLAT_GLYPH_MIN_SIZE - 1, 12), 0);
    CHECK_INT(fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 14, FLAT_GLYPH_MIN_SIZE - 1), 0);
    CHECK_INT(g_old_calls, 4);
    CHECK_INT(cv[0][0], '?');
    CHECK_INT(flat_glyph_route(0, FLAT_W_CLOSE, FLAT_IM_DRAW), 0);
    /* and the draw path does not reach the old hook */
    g_old_calls = 0;
    CHECK_INT(fake_dispatch(&st, FLAT_W_ZOOM, FLAT_IM_DRAW, 0, 0, 0, 14, 12), 1);
    CHECK_INT(g_old_calls, 0);
    CHECK_INT(cv[0][0], '.');
}

/* 3.1 sysiclass answers no OM_GET: the gadget type of the drawn window gadget gives SYSIA_Which */

struct outline_cv_t { unsigned char *cv; int w, h, bad; };
static void outline_fill(void *ctx, int x0, int y0, int x1, int y1, int pen)
{
    struct outline_cv_t *c = ctx;
    int x, y;
    if (pen != 5 || x0 < 0 || y0 < 0 || x1 >= c->w || y1 >= c->h)
        c->bad++;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (x >= 0 && y >= 0 && x < c->w && y < c->h && c->cv[y * c->w + x]++)
                c->bad++;                                                   /* painted twice */
}

struct frame_rec_t { int n, r[8][5]; };

/* S62-FLAT-RF-TX-BUDGET: reference copies of the array forms at 4e5cf2e5, before the one-cut-at-a-time rewrite. */
static int old_button_corners(int w, int h, int out[][4])
{
    int c, n = 0, tall = h >= FLAT_ROUND_TALL;
    if (w < 3 || h < 3) return 0;
    for (c = 0; c < 4; c++) {
        int x = (c & 1) ? w - 1 : 0, y = (c & 2) ? h - 1 : 0;
        int ix = (c & 1) ? -1 : 1, iy = (c & 2) ? -1 : 1;
        out[n][0] = x; out[n][1] = y; out[n][2] = x - ix; out[n][3] = y - iy; n++;
        if (tall) {
            out[n][0] = x + ix; out[n][1] = y; out[n][2] = x + ix; out[n][3] = y - iy; n++;
            out[n][0] = x; out[n][1] = y + iy; out[n][2] = x - ix; out[n][3] = y + iy; n++;
        }
    }
    return n;
}
static int old_cuts_box(int kx, int ky, int kw, int kh, int x0, int y0, int x1, int y1, int out[][2])
{
    int cut[FLAT_ROUND_MAX][4], n = old_button_corners(kw, kh, cut), i, m = 0;
    for (i = 0; i < n; i++) {
        int px = kx + cut[i][0], py = ky + cut[i][1];
        if (px < x0 || px > x1 || py < y0 || py > y1) continue;
        out[m][0] = px; out[m][1] = py; m++;
    }
    return m;
}

/* The single-cut accessors give exactly the cuts, order and values of the old arrays. */
static void test_round_cut_at(void)
{
    int w, h, i, n, bad = 0, cut[4], old[FLAT_ROUND_MAX][4];
    for (w = 0; w <= 40; w++)
        for (h = 0; h <= 40; h++) {
            n = old_button_corners(w, h, old);
            for (i = -2; i < FLAT_ROUND_MAX + 2; i++) {
                int got = flat_button_corner(w, h, i, cut);
                if (i >= 0 && i < n) {
                    if (!got || memcmp(cut, old[i], sizeof cut)) bad++;
                } else if (got) bad++;
            }
        }
    CHECK_INT(bad, 0);
    {
        int kx, ky, kw, kh, d0, d1, xy[2], ref[FLAT_ROUND_MAX][2], all[FLAT_ROUND_MAX][2];
        for (kw = 0; kw <= 16; kw += 1)
            for (kh = 0; kh <= 16; kh += 3)
                for (d0 = -1; d0 <= 1; d0++)
                    for (d1 = -1; d1 <= 1; d1++) {
                        int k = 0;
                        kx = 5; ky = 7;
                        n = old_cuts_box(kx, ky, kw, kh, kx + d0, ky + d0, kx + kw - 1 + d1, ky + kh - 1 + d1, ref);
                        for (i = 0; i < FLAT_ROUND_MAX; i++)
                            if (flat_scroller_cut_at(kx, ky, kw, kh, kx + d0, ky + d0, kx + kw - 1 + d1, ky + kh - 1 + d1, i, xy)) {
                                if (k >= n || xy[0] != ref[k][0] || xy[1] != ref[k][1]) bad++;
                                k++;
                            }
                        if (k != n) bad++;
                        if (flat_scroller_cuts_box(kx, ky, kw, kh, kx + d0, ky + d0, kx + kw - 1 + d1, ky + kh - 1 + d1, all) != n
                            || memcmp(all, ref, sizeof all[0] * n)) bad++;
                    }
    }
    CHECK_INT(bad, 0);
}

/* S69-KNOB-ASPECT: a vertical and a horizontal knob get the same thickness and mirrored caps on every pixel aspect. */
static int knob_cut_n(int side, int kw, int kh, int xy[][2])
{
    int i, n = 0;
    for (i = 0; i < FLAT_ROUND_MAX; i++)
        if (flat_knob_cut_at(side, 0, 0, kw, kh, 0, 0, kw - 1, kh - 1, i, xy[n]))
            n++;
    return n;
}

static void test_knob_aspect(void)
{
    int side, off, hw, vw, nh, nv, i, mirrored, hx[FLAT_ROUND_MAX][2], vx[FLAT_ROUND_MAX][2];
    for (side = 1; side <= 4; side *= 2) {
        hw = flat_knob_cross(10, 6, 1, &off);                       /* the horizontal knob is 6 rows thick: gadget cross 6 stays 6 */
        CHECK_INT(hw, 6);
        vw = flat_knob_cross(10 * side, 6, side, &off);             /* vertical gadget 10 * side columns: 6 rows = 6 * side columns, centred */
        CHECK_INT(vw, 6 * side);
        CHECK_INT(off, (10 * side - 6 * side) / 2);
        CHECK_INT(flat_knob_thickness(vw, 200, side), 6);
        CHECK_INT(flat_knob_thickness(200, 6, side), 6);
        nh = knob_cut_n(side, 200, 6, hx);
        nv = knob_cut_n(side, vw, 200, vx);
        CHECK_INT(nh, 4);                                           /* old rule picked 12 for the vertical knob (height 200) */
        CHECK_INT(nv, 4);
        mirrored = 1;                                               /* corner cut (c): horizontal (x, y) <-> vertical (y, x) */
        for (i = 0; i < nh; i++) {
            int cx = vx[i][0] ? 1 : 0, cy = vx[i][1] ? 1 : 0;       /* which corner the vertical cut sits in */
            int hxc = hx[i][0] ? 1 : 0, hyc = hx[i][1] ? 1 : 0;
            if (!((cx == hxc && cy == hyc) || (cx == hyc && cy == hxc))) mirrored = 0;
            if ((vx[i][0] != 0 && vx[i][0] != vw - 1) || (vx[i][1] != 0 && vx[i][1] != 199)) mirrored = 0;
        }
        CHECK_INT(mirrored, 1);
    }
    CHECK_INT(flat_knob_cross(10, 0, 1, &off), 10);                 /* no horizontal prop: the knob stays whole */
    CHECK_INT(flat_knob_cross(10, 12, 1, &off), 10);                /* never wider than the gadget */
    CHECK_INT(flat_knob_thickness(40, 200, 2), 20);                 /* a genuinely thick knob keeps the round caps */
    CHECK_INT(knob_cut_n(2, 40, 200, vx), 12);
}

/* raa_partial_mask bit y*4+x is exactly raa_cover < 255 inside the corner square. */
static void test_round_partial_mask(void)
{
    int w, h, r, x, y, bad = 0;
    for (w = 1; w <= 24; w++)
        for (h = 1; h <= 24; h++)
            for (r = 0; r <= 4 && r <= w / 2 && r <= h / 2; r++) {
                unsigned m = raa_partial_mask(w, h, r), want = 0;
                for (y = 0; y < r; y++) for (x = 0; x < r; x++)
                    if (raa_cover(w, h, r, x, y) < 255) want |= 1u << (y * 4 + x);
                if (m != want) bad++;
            }
    CHECK_INT(bad, 0);
}

/* flat_frame_draw_round puts back every read pen >= 0 as before the byte array (a pen over 255 as its low byte, the pen SetAPen
 * keeps); an unreadable outside pixel (-1) leaves the cut as drawn. */
static void test_round_pen_byte(void)
{
    static int a[40][60], b[40][60];
    int cut[4], i, x, y, bad = 0;
    flat_state fs;
    memset(&fs, 0, sizeof fs);
    fs.enabled = 1; fs.pen_line = 1; fs.pen_back = 0; fs.pen_button = 4; fs.pen_text = 2;
    for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) a[y][x] = b[y][x] = 300;
    a[24][20] = b[24][20] = 3;                                       /* the outside pixel of the bottom-right corner cut */
    CHECK_INT(flat_frame_draw_round(&fs, 0, 0, 10, 20, 14, 1, 0, round_fill, 0, b), 1);   /* as drawn, no rounding */
    CHECK_INT(flat_frame_draw_round(&fs, 0, 0, 10, 20, 14, 1, 0, round_fill, round_read, a), 1);
    for (i = 0; flat_button_corner(20, 14, i, cut); i++) {
        int sx = cut[2], sy = 10 + cut[3], want;
        if (sx < 0) want = b[10 + cut[1]][cut[0]];                   /* x = -1: unreadable, left as drawn */
        else if (sx == 20 && sy == 24) want = 3;
        else want = 300 & 255;
        if (a[10 + cut[1]][cut[0]] != want) bad++;
    }
    CHECK_INT(i, 12);
    CHECK_INT(bad, 0);
}

static void round_fill(void *ctx, int x0, int y0, int x1, int y1, int pen)
{
    int (*p)[60] = (int (*)[60])ctx, x, y;
    for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) p[y][x] = pen;
}
static int round_read(void *ctx, int x, int y)
{
    int (*p)[60] = (int (*)[60])ctx;
    return x < 0 || y < 0 || x >= 60 || y >= 40 ? -1 : p[y][x];
}

static void frame_rec(void *ctx, int x0, int y0, int x1, int y1, int pen)
{
    struct frame_rec_t *f = (struct frame_rec_t *)ctx;
    if (f->n < 8) { f->r[f->n][0] = x0; f->r[f->n][1] = y0; f->r[f->n][2] = x1; f->r[f->n][3] = y1; f->r[f->n][4] = pen; f->n++; }
}


typedef struct { int n, pen, r[4][4]; } ring_rec;
static void ring_rec_fill(void *ctx, int x0, int y0, int x1, int y1, int pen)
{
    ring_rec *rr = ctx;
    rr->r[rr->n][0] = x0; rr->r[rr->n][1] = y0; rr->r[rr->n][2] = x1; rr->r[rr->n][3] = y1;
    rr->pen = pen;
    rr->n++;
}

/* S71-DOPUS-BORDER-BREAK: the glyph path (arrows, size) fills its whole box in the frame pen, which erased the window outline
 * beside the scroller arrows. glyph_then_ring (c2_glyph's two steps) must leave the ring in the edge pen: the LAST fill that
 * touches a ring pixel is the edge pen, and it is not refilled when nothing was drawn or no ring is given. */
typedef struct { int n, r[64][5]; } rect_log;
static void rect_log_fill(void *ctx, int x0, int y0, int x1, int y1, int pen)
{
    rect_log *l = ctx;
    if (l->n < 64) { l->r[l->n][0] = x0; l->r[l->n][1] = y0; l->r[l->n][2] = x1; l->r[l->n][3] = y1; l->r[l->n][4] = pen; l->n++; }
}
static int rect_log_pen_at(const rect_log *l, int x, int y)
{
    int i, pen = -1;
    for (i = 0; i < l->n; i++)
        if (x >= l->r[i][0] && x <= l->r[i][2] && y >= l->r[i][1] && y <= l->r[i][3]) pen = l->r[i][4];
    return pen;
}
static int glyph_then_ring(const flat_state *st, unsigned long which, unsigned long method, unsigned long ids, int ox, int oy,
                           int w, int h, int frame_pen, int knob_pen, int text_pen, int side, flat_fill_fn fill, void *ctx,
                           const flat_ring *ring)
{   /* the two steps of flat.c c2_glyph: flat_glyph_draw, then c2_ring_refill -> flat_ring_refill_after */
    int drawn = flat_glyph_draw(st, which, method, ids, ox, oy, w, h, frame_pen, knob_pen, text_pen, side, fill, ctx);
    flat_ring_refill_after(drawn, ring, ox, oy, w, h, ctx);
    return drawn;
}
static void test_glyph_keeps_window_ring(void)
{
    flat_state st;
    rect_log l;
    flat_ring ring;
    glyph_state(&st);
    ring.w = 640; ring.h = 256; ring.px = 2; ring.pen = 'E'; ring.fill = rect_log_fill;
    memset(&l, 0, sizeof l);
    CHECK_INT(glyph_then_ring(&st, FLAT_W_UP, FLAT_IM_DRAW, 0, 622, 214, 18, 11, 'F', -1, -1, 2, rect_log_fill, &l, &ring), 1);
    CHECK_INT(rect_log_pen_at(&l, 639, 220), 'E');          /* right ring column beside the up arrow: edge pen */
    CHECK_INT(rect_log_pen_at(&l, 638, 214), 'E');
    CHECK_INT(rect_log_pen_at(&l, 630, 220) != 'E', 1);     /* inside the box the glyph pens stay */
    memset(&l, 0, sizeof l);
    CHECK_INT(glyph_then_ring(&st, FLAT_W_LEFT, FLAT_IM_DRAW, 0, 600, 244, 32, 12, 'F', -1, -1, 2, rect_log_fill, &l, &ring), 1);
    CHECK_INT(rect_log_pen_at(&l, 610, 255), 'E');          /* bottom ring row under the left arrow */
    memset(&l, 0, sizeof l);
    CHECK_INT(glyph_then_ring(&st, FLAT_W_UP, FLAT_IM_DRAW, 0, 622, 214, 18, 11, 'F', -1, -1, 2, rect_log_fill, &l, 0), 1);
    CHECK_INT(rect_log_pen_at(&l, 639, 220) == 'E', 0);     /* no ring given: nothing refilled */
    memset(&l, 0, sizeof l);
    CHECK_INT(glyph_then_ring(&st, FLAT_W_UP, FLAT_IM_DRAW, 0, 622, 214, 4, 4, 'F', -1, -1, 2, rect_log_fill, &l, &ring), 0);
    CHECK_INT(l.n, 0);                                      /* not drawn (box too small): no ring fill either */
}

static void test_which_from_gtype(void)
{
    CHECK_INT(flat_which_from_gtype(0x8000u | 0x40), FLAT_W_DEPTH);
    CHECK_INT(flat_which_from_gtype(0x8000u | 0x60 | 1), FLAT_W_ZOOM);
    CHECK_INT(flat_which_from_gtype(0x8000u | 0x10), FLAT_W_SIZE);
    CHECK_INT(flat_which_from_gtype(0x8000u | 0x80 | 1), FLAT_W_CLOSE);
    CHECK_INT(flat_which_from_gtype(0x8000u | 0x20), -1);      /* GTYP_WDRAGGING is not a glyph */
    CHECK_INT(flat_which_from_gtype(0x0001u), -1);
    /* FLAT-C7: the screen title bar's depth gadget is type 0xc055 (SYSGADGET|SCRGADGET|GTYP_SDEPTH|BOOLGADGET) */
    CHECK_INT(flat_which_from_gtype(0xc055u), FLAT_W_DEPTH);
    {
        int img, other;
        CHECK_INT(flat_gadget_which(0xc055u, &img, 0, &img), FLAT_W_DEPTH);
        CHECK_INT(flat_gadget_which(0xc055u, &other, &img, &img), FLAT_W_DEPTH);
        CHECK_INT(flat_gadget_which(0xc055u, &other, 0, &img), -1);         /* another gadget's image */
        CHECK_INT(flat_gadget_which(0x4055u, &img, 0, &img), -1);           /* not a system gadget */
        CHECK_INT(flat_gadget_which(0xc035u, &img, 0, &img), -1);           /* screen drag bar: no glyph */
    }
    /* DOPUS-ARROW-INACTIVE: the arrow of a border gadget read from geometry (no OM_NEW record) */
    {
        int img, other, b;
        CHECK_INT(flat_arrow_border(0x0005u, 0x0010u | 0x0002u, &img, 0, &img), 1);     /* right border pair member */
        CHECK_INT(flat_arrow_border(0x0005u, 0x0080u, 0, &img, &img), 2);               /* bottom border, SelectRender */
        CHECK_INT(flat_arrow_border(0x0005u, 0x0002u, &img, 0, &img), 0);               /* no border flag */
        CHECK_INT(flat_arrow_border(0x0005u, 0x0020u, &img, 0, &img), 0);               /* GACT_LEFTBORDER: not an arrow border */
        CHECK_INT(flat_arrow_border(0x0005u, 0x0040u, &img, 0, &img), 0);               /* GACT_TOPBORDER */
        CHECK_INT(flat_arrow_border(0x0003u, 0x0010u, &img, 0, &img), 0);               /* prop gadget */
        CHECK_INT(flat_arrow_border(0x0004u, 0x0010u, &img, 0, &img), 0);               /* string gadget */
        CHECK_INT(flat_arrow_border(0xc055u, 0x0010u, &img, 0, &img), 0);               /* system gadget */
        CHECK_INT(flat_arrow_border(0x8005u, 0x0010u, &img, 0, &img), 0);               /* system flag set */
        CHECK_INT(flat_arrow_border(0x0005u, 0x0010u, &other, 0, &img), 0);             /* image of another gadget */
        b = 1;
        CHECK_INT(flat_arrow_which(b, 1, 1), FLAT_W_UP);       /* a partner lies below: this is the up arrow */
        CHECK_INT(flat_arrow_which(b, 1, 0), FLAT_W_DOWN);
        CHECK_INT(flat_arrow_which(2, 1, 1), FLAT_W_LEFT);
        CHECK_INT(flat_arrow_which(2, 1, 0), FLAT_W_RIGHT);
        CHECK_INT(flat_arrow_which(1, 0, 0), -1);              /* single gadget: stock draws */
        CHECK_INT(flat_arrow_which(0, 1, 1), -1);
        (void)other;
    }
    /* DOPUS-ZOOM-HIJACK: a lister zoom gadget the app turned into a plain gadget (top, height, border_top, has_sys_zoom, has_sys_depth) */
    {
        int img, other;
        CHECK_INT(flat_zoom_hijacked(0x0005u, &img, 0, &img, 0, 14, 14, 0, 1), 1);       /* hijacked zoom */
        CHECK_INT(flat_zoom_hijacked(0x0005u, 0, &img, &img, 0, 14, 14, 0, 1), 1);       /* SelectRender */
        CHECK_INT(flat_zoom_hijacked(0x0005u, &img, 0, &img, 0, 14, 14, 1, 1), 0);       /* window still has a system zoom */
        CHECK_INT(flat_zoom_hijacked(0x0005u, &img, 0, &img, 0, 14, 14, 0, 0), 0);       /* no depth gadget */
        CHECK_INT(flat_zoom_hijacked(0x0005u, &img, 0, &img, 540, 14, 14, 0, 1), 0);     /* below the top border (RELBOTTOM resolved) */
        CHECK_INT(flat_zoom_hijacked(0x0005u, &img, 0, &img, -2, 14, 14, 0, 1), 0);      /* above the window */
        CHECK_INT(flat_zoom_hijacked(0x0005u, &other, 0, &img, 0, 14, 14, 0, 1), 0);     /* image of another gadget */
        CHECK_INT(flat_zoom_hijacked(0x8065u, &img, 0, &img, 0, 14, 14, 0, 1), 0);       /* system gadget: the existing path */
        (void)other;
    }
    /* DOPUS-ARROW-WHICH: an app-made arrow (not a system gadget) resolves through the table recorded at OM_NEW */
    {
        flat_img_rec tab[FLAT_IMG_RECS];
        int objs[FLAT_IMG_RECS + 2], i;
        unsigned long sz = 99;
        memset(tab, 0, sizeof tab);
        CHECK_INT(flat_gadget_which(0x0001u, &objs[0], &objs[0], &objs[0]), -1);    /* the system lookup alone misses it */
        CHECK_INT(flat_img_which(tab, FLAT_IMG_RECS, &objs[0], &sz), -1);
        CHECK_INT(flat_img_record(tab, FLAT_IMG_RECS, &objs[0], 0x0B, 1), 1);
        CHECK_INT(flat_img_record(tab, FLAT_IMG_RECS, &objs[1], 0x0D, 0), 1);
        CHECK_INT(flat_img_record(tab, FLAT_IMG_RECS, &objs[2], 0x0A, 2), 1);
        CHECK_INT(flat_img_record(tab, FLAT_IMG_RECS, &objs[3], 0x0C, 2), 1);
        CHECK_INT(flat_img_which(tab, FLAT_IMG_RECS, &objs[0], &sz), FLAT_W_UP);
        CHECK_INT((int)sz, 1);
        CHECK_INT(flat_img_which(tab, FLAT_IMG_RECS, &objs[1], &sz), FLAT_W_DOWN);
        CHECK_INT(flat_img_which(tab, FLAT_IMG_RECS, &objs[2], &sz), FLAT_W_LEFT);
        CHECK_INT(flat_img_which(tab, FLAT_IMG_RECS, &objs[3], &sz), FLAT_W_RIGHT);
        CHECK_INT((int)sz, 2);
        CHECK_INT(flat_img_record(tab, FLAT_IMG_RECS, &objs[4], 0x00, 1), 0);       /* not an arrow Which: not recorded */
        CHECK_INT(flat_img_record(tab, FLAT_IMG_RECS, &objs[4], FLAT_W_CHECKIMAGE, 1), 1);   /* a check box image is found again at draw time */
        CHECK_INT(flat_img_which(tab, FLAT_IMG_RECS, &objs[4], NULL), FLAT_W_CHECKIMAGE);
        flat_img_forget(tab, FLAT_IMG_RECS, &objs[4]);
        CHECK_INT(flat_img_record(tab, FLAT_IMG_RECS, &objs[4], FLAT_W_PROPKNOB, 0), 1);     /* DOPUS-PROPKNOB: a knob frame is found again at draw time */
        CHECK_INT(flat_img_which(tab, FLAT_IMG_RECS, &objs[4], NULL), FLAT_W_PROPKNOB);
        flat_img_forget(tab, FLAT_IMG_RECS, &objs[4]);
        /* OM_DISPOSE: the entry goes, a new object at the same address is not the old arrow */
        flat_img_forget(tab, FLAT_IMG_RECS, &objs[0]);
        CHECK_INT(flat_img_which(tab, FLAT_IMG_RECS, &objs[0], &sz), -1);
        CHECK_INT(flat_img_which(tab, FLAT_IMG_RECS, &objs[1], &sz), FLAT_W_DOWN);
        /* full table: falls back (0 / -1), nothing overwritten */
        memset(tab, 0, sizeof tab);
        for (i = 0; i < FLAT_IMG_RECS; i++)
            CHECK_INT(flat_img_record(tab, FLAT_IMG_RECS, &objs[i], 0x0B, 1), 1);
        CHECK_INT(flat_img_record(tab, FLAT_IMG_RECS, &objs[FLAT_IMG_RECS], 0x0B, 1), 0);
        CHECK_INT(flat_img_which(tab, FLAT_IMG_RECS, &objs[FLAT_IMG_RECS], &sz), -1);
        CHECK_INT(flat_img_which(tab, FLAT_IMG_RECS, &objs[5], &sz), FLAT_W_UP);
        /* DOPUS-ZOOM-WHICH: a ZOOMIMAGE (Which 1) is recorded and reads back as the zoom glyph; 256 slots */
        memset(tab, 0, sizeof tab);
        CHECK_INT(FLAT_IMG_RECS, 256);
        CHECK_INT(flat_img_record(tab, FLAT_IMG_RECS, &objs[0], 0x01, 1), 1);
        CHECK_INT(flat_img_which(tab, FLAT_IMG_RECS, &objs[0], &sz), FLAT_W_ZOOM);
        CHECK_INT(flat_img_record(tab, FLAT_IMG_RECS, &objs[1], 0x02, 1), 0);       /* other non-arrow Which stays out */
        CHECK_INT(flat_img_record(tab, FLAT_IMG_RECS, &objs[0], 0x0B, 1), 1);       /* same obj replaced */
        CHECK_INT(flat_img_which(tab, FLAT_IMG_RECS, &objs[0], &sz), FLAT_W_UP);
    }
    /* FLAT-C8: the frameiclass decision. Skip only for IM_DRAW / IM_DRAWFRAME, switch on, not held, flat pens. */
    {
        flat_state fs;
        memset(&fs, 0, sizeof fs);
        fs.enabled = 1;
        CHECK_INT(flat_frame_decide(&fs, 0x202UL, 0, 0, 0), FLAT_DRAW_SKIP);
        CHECK_INT(flat_frame_decide(&fs, FLAT_IM_DRAWFRAME, 2, 2, 0), FLAT_DRAW_SKIP);
        CHECK_INT(flat_frame_decide(&fs, 0x202UL, 2, 0, 0), FLAT_DRAW_STOCK);        /* stock pens off a themed screen keep stock bevels */
        /* FONT-PREFS-NOT-FLAT: Font prefs on the 3.1 Workbench (stock pen map SHINE 2, BACK 0) draws flat frames */
        CHECK_INT(flat_frame_decide(&fs, 0x202UL, 2, 0, 1), FLAT_DRAW_SKIP);
        CHECK_INT(flat_frame_decide(&fs, FLAT_IM_DRAWFRAME, 2, 0, 1), FLAT_DRAW_SKIP);
        CHECK_INT(flat_frame_decide(&fs, 0x203UL, 2, 0, 1), FLAT_DRAW_STOCK);     /* IM_ERASE stays stock on a themed screen */
        CHECK_INT(flat_frame_decide(&fs, 0x203UL, 0, 0, 0), FLAT_DRAW_STOCK);        /* IM_ERASE */
        CHECK_INT(flat_frame_decide(&fs, 0x204UL, 0, 0, 0), FLAT_DRAW_STOCK);        /* IM_HITTEST: never skipped */
        CHECK_INT(flat_frame_decide(&fs, 0x0UL, 0, 0, 0), FLAT_DRAW_STOCK);
        /* PALETTE-BUTTONS-BEVELED: a private screen is themed while flat-private-screens is on; an exclude rule wins */
        CHECK_INT(flat_screen_themed(1, 0, 0), 1);
        CHECK_INT(flat_screen_themed(0, 1, 0), 1);
        CHECK_INT(flat_screen_themed(0, 0, 0), 0);
        CHECK_INT(flat_screen_themed(1, 1, 1), 0);
        CHECK_INT(flat_screen_themed(0, 1, 1), 0);
        fs.held = 1;
        CHECK_INT(flat_frame_decide(&fs, 0x202UL, 0, 0, 0), FLAT_DRAW_STOCK);
        CHECK_INT(flat_frame_decide(&fs, 0x202UL, 2, 0, 1), FLAT_DRAW_STOCK);
        fs.held = 0;
        fs.enabled = 0;
        CHECK_INT(flat_frame_decide(&fs, 0x202UL, 0, 0, 0), FLAT_DRAW_STOCK);
        CHECK_INT(flat_frame_decide(&fs, 0x202UL, 2, 0, 1), FLAT_DRAW_STOCK);
        CHECK_INT(flat_frame_decide(0, 0x202UL, 0, 0, 0), FLAT_DRAW_STOCK);
    }
    {
        char fl[64];
        CHECK_INT(flat_frame_status_line(0, 0, 0, 0, 0, fl, sizeof fl) > 0 && !strcmp(fl, "frame=absent"), 1);
        CHECK_INT(flat_frame_status_line(1, 7, 3, 2, 1, fl, sizeof fl) > 0 && !strcmp(fl, "frame=hooked calls=7 flat=3 busy=2 nostk=1"), 1);
        CHECK_INT(flat_frame_status_line(1, 7, 3, 2, 1, fl, 40), 0);
    }
    {   /* FLAT-C8: the flat frame geometry: fill, then four 1 px sides; edges-only has no fill; selected fills pen_line */
        struct frame_rec_t f;
        flat_state fs;
        memset(&fs, 0, sizeof fs);
        fs.enabled = 1; fs.pen_line = 1; fs.pen_back = 0; fs.pen_button = 4; fs.pen_text = 2;
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 8, 6, 1, 0, frame_rec, &f), 1);
        CHECK_INT(f.n, 1);                                                     /* FLAT-BUTTON-FILL: one fill, NO outline */
        CHECK_INT(f.r[0][0] == 10 && f.r[0][1] == 20 && f.r[0][2] == 17 && f.r[0][3] == 25 && f.r[0][4] == 1, 1);   /* S56-PRIMARY-ACCENT-FILL: primary = accent pen_line, not pen_button */
        fs.pen_button = 0;
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 8, 6, 1, 0, frame_rec, &f), 1);
        CHECK_INT(f.n == 1 && f.r[0][4] == 1, 1);                              /* pen_button is no longer the fill: still the accent */
        fs.pen_button = 4;
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 8, 6, 0, 0, frame_rec, &f), 1);
        CHECK_INT(f.n, 4);                                                     /* not fill: the outline only, unchanged */
        CHECK_INT(f.r[0][4] == 1 && f.r[1][4] == 1 && f.r[2][4] == 1 && f.r[3][4] == 1, 1);
        CHECK_INT(f.r[0][0] == 10 && f.r[0][1] == 20 && f.r[0][2] == 17 && f.r[0][3] == 20, 1);
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 1, 10, 20, 8, 6, 1, 0, frame_rec, &f), 1);
        CHECK_INT(f.n == 1 && f.r[0][4] == 2, 1);                              /* pressed: text pen fill (the accent is the unpressed fill), no outline */
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 8, 6, 1, FLAT_ROLE_SECONDARY, frame_rec, &f), 1);
        CHECK_INT(f.n == 1 && f.r[0][4] == 4, 1);                              /* secondary: the face pen_button */
        fs.pen_button = 0; fs.pen_ptext = 0;
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 8, 6, 1, FLAT_ROLE_SECONDARY, frame_rec, &f), 1);
        CHECK_INT(f.n == 1 && f.r[0][4] == 1, 1);                              /* CANCEL-INVISIBLE-FLAT: face = label pen: the accent, never pen_back */
        fs.pen_button = 4;
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 2, 6, 1, 0, frame_rec, &f), 0);   /* too small: stock */
        CHECK_INT(flat_frame_draw(0, 0, 10, 20, 8, 6, 1, 0, frame_rec, &f), 0);
    }
    {   /* FLAT-BUTTON-ROUND: corners take the pen read just outside the button, not a fixed pen */
        struct round_canvas { int pix[40][60]; } *cv = (struct round_canvas *)calloc(1, sizeof *cv);
        flat_state fs;
        int cut[FLAT_ROUND_MAX][4], x, y, n, bg7 = 0, ok = 1;
        memset(&fs, 0, sizeof fs);
        fs.enabled = 1; fs.pen_line = 1; fs.pen_back = 0; fs.pen_button = 4; fs.pen_text = 2;
        for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 7;       /* group box pen 7, not pen 0 */
        CHECK_INT(flat_frame_draw_round(&fs, 0, 10, 10, 20, 8, 1, 0, round_fill, round_read, cv), 1);
        CHECK_INT(cv->pix[10][10] == 7 && cv->pix[10][29] == 7 && cv->pix[17][10] == 7 && cv->pix[17][29] == 7, 1);   /* corners: outside pen */
        CHECK_INT(cv->pix[10][11] == 1 && cv->pix[11][10] == 1 && cv->pix[12][12] == 1, 1);                          /* small: 1 px cut only */
        for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = (x + y) & 3;   /* patterned fill: each corner its own neighbour */
        CHECK_INT(flat_frame_draw_round(&fs, 1, 10, 10, 20, 8, 1, 0, round_fill, round_read, cv), 1);
        CHECK_INT(cv->pix[10][10] == (9 + 9 & 3) && cv->pix[17][29] == (30 + 18 & 3) && cv->pix[10][29] == (30 + 9 & 3), 1);   /* pressed too */
        CHECK_INT(cv->pix[12][12] == 2, 1);                                                                          /* pressed body: pen_text */
        CHECK_INT(flat_button_corners(20, 8, cut), 4);                                                               /* small: 4 cuts */
        CHECK_INT(cut[0][0] == 0 && cut[0][1] == 0 && cut[0][2] == -1 && cut[0][3] == -1, 1);
        CHECK_INT(cut[3][0] == 19 && cut[3][1] == 7 && cut[3][2] == 20 && cut[3][3] == 8, 1);
        CHECK_INT(flat_button_corners(20, 11, cut), 4);
        n = flat_button_corners(20, 12, cut);
        CHECK_INT(n, 12);                                                                                            /* large: corner + two beside each */
        for (x = 0; x < n; x++) {                                                                                   /* inside pixels distinct, all on the box edge ring */
            int ex = cut[x][0] == 0 || cut[x][0] == 19, ey = cut[x][1] == 0 || cut[x][1] == 11, k;
            if (!ex && !ey) ok = 0;
            for (k = 0; k < x; k++) if (cut[k][0] == cut[x][0] && cut[k][1] == cut[x][1]) ok = 0;
        }
        CHECK_INT(ok, 1);
        CHECK_INT(cut[1][0] == 1 && cut[1][1] == 0 && cut[1][2] == 1 && cut[1][3] == -1, 1);                         /* beside the corner, along the top */
        CHECK_INT(cut[2][0] == 0 && cut[2][1] == 1 && cut[2][2] == -1 && cut[2][3] == 1, 1);                         /* beside the corner, down the left */
        CHECK_INT(flat_button_corners(2, 12, cut), 0);
        (void)bg7;
        free(cv);
    }
    {   /* S56-LOCALE-CANCEL-CORNER: a tall secondary (outlined) button keeps a closed outline: each corner cut is bridged by the inner
         * diagonal pixel in pen_text (rig 75 before the fix: Cancel top-left (541,225) pen 0, the outline open at all four corners) */
        struct round_canvas { int pix[40][60]; } *cv = (struct round_canvas *)calloc(1, sizeof *cv);
        flat_state fs;
        int x, y;
        memset(&fs, 0, sizeof fs);
        fs.enabled = 1; fs.pen_text = 2; fs.pen_line = 3; fs.pen_back = 0; fs.pen_button = 4;
        for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 0;
        CHECK_INT(flat_frame_draw_round(&fs, 0, 10, 10, 30, 14, 1, FLAT_ROLE_SECONDARY, round_fill, round_read, cv), 1);
        CHECK_INT(cv->pix[10][10] == 0 && cv->pix[10][11] == 0 && cv->pix[11][10] == 0, 1);           /* corner cut stays */
        /* OWNER-2026-11-08 FLAT-BUTTON-NO-RING: secondary = solid pen_button fill, corner cut stays (surface pen), edge pixel beside it is the fill */
        CHECK_INT(cv->pix[10][12] == 4 && cv->pix[12][10] == 4, 1);                                   /* edge row and column are fill, not outline */
        CHECK_INT(cv->pix[11][11] == 4 && cv->pix[11][38] == 4 && cv->pix[22][11] == 4 && cv->pix[22][38] == 4 && cv->pix[12][12] == 4, 1);
        for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 0;
        CHECK_INT(flat_frame_draw_round(&fs, 0, 10, 10, 30, 14, 1, FLAT_ROLE_PRIMARY, round_fill, round_read, cv), 1);
        CHECK_INT(cv->pix[11][11] == 3, 1);                                                           /* primary: accent body pen_line, no bridge */
        free(cv);
    }
    {   /* S56-LOCALE-KNOB: a SMARTKNOBIMAGE listview knob (frameiclass IM_DRAWFRAME) paints in the Workbench knob pen with round corners */
        struct round_canvas { int pix[40][60]; } *cv = (struct round_canvas *)calloc(1, sizeof *cv);
        flat_state fs;
        int x, y, on, off, obj = 0, other = 0;
        memset(&fs, 0, sizeof fs);
        fs.enabled = 1; fs.pen_text = 2; fs.pen_line = 3; fs.pen_back = 0; fs.pen_outline = 3; fs.pen_button = 4;
        fs.pen_knob = 249; fs.pen_inbar = 250; fs.pen_sel = 4;
        on = flat_knob_state_pen(&fs, 8, 1);
        off = flat_knob_state_pen(&fs, 8, 0);
        CHECK_INT(on != 0 && off != 0 && on == off, 1);   /* S86-ACTIVE-SCROLL-GREY */
        for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 0;
        CHECK_INT(flat_knob_frame_draw(&fs, 8, 1, 10, 10, 10, 18, round_fill, round_read, cv), 1);
        CHECK_INT(cv->pix[14][14] == on && cv->pix[10][14] == on && cv->pix[27][14] == on, 1);   /* box: active knob pen */
        CHECK_INT(cv->pix[10][10] == 0 && cv->pix[27][19] == 0, 1);                               /* corners cut to the track */
        CHECK_INT(flat_knob_frame_draw(&fs, 8, 0, 10, 10, 10, 18, round_fill, round_read, cv), 1);
        CHECK_INT(cv->pix[14][14] == off, 1);                                                      /* inactive window: inactive tone */
        CHECK_INT(flat_smart_knob(1, 0x3d, &obj, &obj), 1);                                       /* Locale listview props: pflags 0x3d */
        CHECK_INT(flat_smart_knob(1, 0x1d, &obj, &obj), 0);                                       /* no SMARTKNOBIMAGE: RectFill path */
        CHECK_INT(flat_smart_knob(1, 0x3d, &other, &obj), 0);                                     /* another frame object */
        CHECK_INT(flat_smart_knob(0, 0x3d, &obj, &obj), 0);
        free(cv);
    }
    {   /* BUTTON-ROLES: rightmost of a row is outlined, others and a lone button are filled; fill pen per depth; pressed is accent */
        struct frame_rec_t f;
        flat_state fs;
        flat_box row[3] = { {10, 5, 40, 12}, {60, 5, 40, 12}, {110, 5, 40, 12} }, other = {10, 40, 40, 12}, lone = {10, 5, 40, 12};
        memset(&fs, 0, sizeof fs);
        fs.enabled = 1; fs.pen_text = 2; fs.pen_line = 3; fs.pen_back = 0; fs.pen_button = 4;
        CHECK_INT(flat_button_role(&row[2], row, 3), FLAT_ROLE_SECONDARY);   /* rightmost button in a row is outlined */
        CHECK_INT(flat_button_role(&row[0], row, 3), FLAT_ROLE_PRIMARY);     /* left button is filled */
        CHECK_INT(flat_button_role(&row[1], row, 3), FLAT_ROLE_PRIMARY);
        CHECK_INT(flat_button_role(&lone, &lone, 1), FLAT_ROLE_PRIMARY);     /* lone button is filled */
        { flat_box two[2]; two[0] = row[2]; two[1] = other;
          CHECK_INT(flat_button_role(&row[2], two, 2), FLAT_ROLE_PRIMARY); } /* a button in another row is no sibling */
        {   /* S62-FLAT-RF-TX-BUDGET: the RectFill hook folds one sibling at a time; the fold equals flat_button_role for every self and set */
            flat_box all[5]; int si, n, j, seen;
            all[0] = row[0]; all[1] = row[1]; all[2] = row[2]; all[3] = other; all[4] = lone;
            for (n = 1; n <= 5; n++)
                for (si = 0; si < n; si++) {
                    seen = 0;
                    for (j = 0; j < n; j++)
                        seen = flat_button_role_add(seen, &all[si], &all[j]);
                    CHECK_INT(flat_button_role_of(seen), flat_button_role(&all[si], all, n));
                }
            seen = 0;
            for (j = 0; j < 3; j++)
                seen = flat_button_role_add(seen, &row[2], &row[j]);
            CHECK_INT(flat_button_role_of(seen), FLAT_ROLE_SECONDARY);
        }
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 8, 6, 1, FLAT_ROLE_SECONDARY, frame_rec, &f), 1);
        CHECK_INT(f.n == 1 && f.r[0][4] == 4, 1);                            /* secondary: one full-box fill in pen_button, no ring */
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 8, 6, 1, FLAT_ROLE_SECONDARY, frame_rec, &f), 1);
        CHECK_INT(f.n == 1 && f.r[0][4] == 4 && f.r[0][0] == 10 && f.r[0][1] == 20 && f.r[0][2] == 17 && f.r[0][3] == 25, 1);   /* covers the whole box */
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 1, 10, 20, 8, 6, 1, FLAT_ROLE_SECONDARY, frame_rec, &f), 1);
        CHECK_INT(f.n == 1 && f.r[0][4] == 2, 1);                            /* pressed secondary gets the text pen fill */
        /* flat_frame_fill: an edges-only frame fills only for a push button (Installer buttons); edges-only non-push stays an outline */
        CHECK_INT(flat_frame_fill(1, 0, 0), 1);
        CHECK_INT(flat_frame_fill(1, 1, 1), 1);
        CHECK_INT(flat_frame_fill(1, 1, 0), 0);
        CHECK_INT(flat_frame_fill(0, 0, 1), 1);
        CHECK_INT(flat_frame_fill(0, 0, 0), 0);
        {   /* S57-BUTTON-ASPECT-BORDERS: a button outline is flat_side_px columns wide left and right, 1 px top and bottom */
            int modes[3][2] = { { 22, 44 }, { 22, 22 }, { 11, 44 } };   /* 640x256 hires, 640x512 interlace, super hires */
            int want[3] = { 2, 1, 4 }, m, k;
            for (m = 0; m < 3; m++) {
                for (k = 0; k < 2; k++) {                                /* 0 = secondary (filled role), 1 = edges-only frame */
                    fs.side = (uint8_t)flat_side_px(modes[m][0], modes[m][1]);
                    memset(&f, 0, sizeof f);
                    CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 60, 12, k == 0, FLAT_ROLE_PRIMARY, frame_rec, &f), 1);
                    CHECK_INT(f.n, k == 0 ? 1 : 4);
                    if (k == 1) {   int o = f.n - 4;                                  /* top, bottom, left, right */
                        CHECK_INT(f.r[o][1] == 20 && f.r[o][3] == 20, 1);              /* top: 1 px */
                        CHECK_INT(f.r[o + 1][1] == 31 && f.r[o + 1][3] == 31, 1);      /* bottom: 1 px */
                        CHECK_INT(f.r[o + 2][0] == 10 && f.r[o + 2][2] == 10 + want[m] - 1, 1);   /* left: side px */
                        CHECK_INT(f.r[o + 3][0] == 69 - want[m] + 1 && f.r[o + 3][2] == 69, 1);   /* right: side px */
                    }
                }
            }
            fs.side = 0;                                                  /* an unset side stays 1 px */
            memset(&f, 0, sizeof f);
            CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 60, 12, 0, 0, frame_rec, &f), 1);
            CHECK_INT(f.r[2][0] == 10 && f.r[2][2] == 10 && f.r[3][0] == 69 && f.r[3][2] == 69, 1);
            CHECK_INT(flat_button_side(4, 6), 2);                         /* never more than a third of a narrow box */
            CHECK_INT(flat_button_side(0, 60), 1);
            fs.side = 0;
        }
        CHECK_INT(flat_button_pen(&fs, 2), 3);                               /* 4-colour primary uses accent fill */
        fs.pen_sel = 4;
        CHECK_INT(flat_button_pen(&fs, 3), 4);                               /* 8 colours: lighter background */
    }
    {   /* S56-PRIMARY-ACCENT-FILL: a primary button (Save) draws the accent pen and its label in the background pen; pressed keeps the background label;
         * secondary is a pen_button fill with the background-pen label (FLAT-SECONDARY-LABEL). Driven through the frame draw and the label arm exactly as flat.c does. */
        struct round_canvas { int pix[40][60]; } *cv = (struct round_canvas *)calloc(1, sizeof *cv);
        flat_state fs; flat_tx t; int surf = 1, task = 2, pressed, role, cases[3][2] = { { 0, FLAT_ROLE_PRIMARY }, { 1, FLAT_ROLE_PRIMARY }, { 0, FLAT_ROLE_SECONDARY } };
        int body[3] = { 5, 6, 5 }, label[3] = { 0, 0, 0 }, i;   /* secondary: no pen_button, face = pen_back = pen_ptext: the accent (CANCEL-INVISIBLE-FLAT) */
        memset(&fs, 0, sizeof fs);
        fs.enabled = 1; fs.pen_text = 6; fs.pen_line = 5; fs.pen_back = 0; fs.pen_ptext = 0; fs.pen_sel = 4;
        for (i = 0; i < 3; i++) {
            pressed = cases[i][0]; role = cases[i][1];
            memset(&t, 0, sizeof t);
            CHECK_INT(flat_frame_draw_round(&fs, pressed, 10, 10, 30, 14, 1, role, round_fill, round_read, cv), 1);
            flat_tx_frame(&t, &surf, &task, flat_tx_arm(pressed, 1, role), 1, 10, 10, 39, 23);
            CHECK_INT(cv->pix[16][20], body[i]);
            CHECK_INT(flat_tx_pen(&fs, &t, &surf, &task, 20, 16), label[i]);
        }
        CHECK_INT(cv->pix[16][20] != fs.pen_sel, 1);                         /* the lighter-background held pen no longer fills the button */
        /* S56-CYCLE-PLAIN: a cycle gadget (label outside or none) is role PLAIN: pen_button fill, label never armed, pressed or not;
         * the push button (label inside) stays primary with the accent */
        fs.pen_button = 7;
        /* S56-PUSH-ROLE: the values measured on rig 88 (3.2 GadTools). Locale/Sound Save, Use, Cancel: GadgetText NULL, 88x14,
         * Activation 0001 -> push button (37b5e3d2 took them for cycles: every Save/Use/Cancel drew PLAIN, no accent) */
        CHECK_INT(flat_push_button(0, 0, 0, 88, 14, 0x0001), 1);              /* Save / Use / Cancel */
        CHECK_INT(flat_push_button(0, 0, 0, 200, 14, 0x0001), 1);             /* Sound "Test Sound" */
        CHECK_INT(flat_push_button(1, 12, 3, 60, 14, 0x0001), 1);             /* a GadTools that keeps the label inside the box */
        CHECK_INT(flat_push_button(1, -96, 3, 180, 14, 0x0001), 0);           /* Sound "Sound Type:" cycle, label left */
        CHECK_INT(flat_push_button(1, 12, -10, 60, 14, 0x0001), 0);           /* cycle, label above */
        /* DOPUS-DIALOG-BUTTON-FILL: values measured on dopus5 5.104 (rig 68): MakeDir / List Format buttons, check boxes, scroller arrows */
        CHECK_INT(flat_dopus_push_gadget(5, 0x0001), 1);                      /* OK / Cancel / Save */
        CHECK_INT(flat_dopus_push_gadget(5, 0x4001), 1);                      /* the 4001 variant */
        CHECK_INT(flat_dopus_push_gadget(5, 0x0101), 0);                      /* List Format check box: TOGGLESELECT */
        CHECK_INT(flat_dopus_push_gadget(5, 0x0000), 0);                      /* scroller arrow */
        CHECK_INT(flat_dopus_push_gadget(1, 0x0001), 0);                      /* not a custom gadget */
        CHECK_INT(flat_dopus_fill_match(160, 71, 47, 17, 161, 72, 205, 86), 1);   /* the measured interior of the first MakeDir button */
        CHECK_INT(flat_dopus_fill_match(160, 71, 47, 17, 160, 71, 206, 87), 1);   /* the full box (no inset) */
        CHECK_INT(flat_dopus_fill_match(160, 71, 47, 17, 163, 72, 205, 86), 0);   /* a narrow strip: not the interior */
        CHECK_INT(flat_dopus_fill_match(160, 71, 47, 17, 161, 72, 205, 90), 0);   /* leaves the box */
        CHECK_INT(flat_dopus_fill_match(160, 71, 47, 17, 100, 72, 120, 86), 0);   /* another place */
        {   /* DOPUS-DIALOG-BUTTON-FILL pending table (agent 43): a stale entry never blocks another owner, a put never refuses */
            flat_dpend_tab dt;
            flat_dpend o;
            int A = 1, B = 2, C = 3, D = 4, E = 5, SA = 0, SB = 0;
            memset(&dt, 0, sizeof dt);
            flat_dpend_put(&dt, &A, &SA, 160, 71, 47, 17, 11);                /* A puts and never takes */
            CHECK_INT(dt.n, 1);
            flat_dpend_put(&dt, &B, &SB, 10, 10, 47, 17, 22);
            CHECK_INT(flat_dpend_take(&dt, &B, &SB, 11, 11, 55, 25, &o), 1);  /* B still works beside A's stale entry */
            CHECK_INT(o.val, 22);
            CHECK_INT(o.x, 10);
            CHECK_INT(dt.n, 1);
            CHECK_INT(flat_dpend_take(&dt, &B, &SB, 11, 11, 55, 25, &o), 0);  /* taken once */
            CHECK_INT(flat_dpend_take(&dt, &A, &SB, 161, 72, 205, 86, &o), 0);   /* wrong surface */
            CHECK_INT(flat_dpend_take(&dt, &A, &SA, 163, 72, 205, 86, &o), 0);   /* a label strip: entry stays */
            CHECK_INT(dt.n, 1);
            CHECK_INT(flat_dpend_take(&dt, &B, &SA, 161, 72, 205, 86, &o), 0);   /* another owner's entry is not B's */
            flat_dpend_put(&dt, &A, &SA, 160, 71, 47, 17, 12);                /* the owner's own put reuses its entry */
            CHECK_INT(dt.n, 1);
            flat_dpend_drop(&dt, &A);
            CHECK_INT(dt.n, 0);
            CHECK_INT(flat_dpend_take(&dt, &A, &SA, 161, 72, 205, 86, &o), 0);   /* drop cleared it */
            flat_dpend_put(&dt, &A, &SA, 0, 0, 40, 20, 1);
            flat_dpend_put(&dt, &B, &SA, 0, 0, 40, 20, 2);
            flat_dpend_put(&dt, &C, &SA, 0, 0, 40, 20, 3);
            flat_dpend_put(&dt, &D, &SA, 0, 0, 40, 20, 4);
            flat_dpend_put(&dt, &E, &SA, 0, 0, 40, 20, 5);                    /* full: the oldest (A) is replaced, never refused */
            CHECK_INT(dt.n, FLAT_DPEND_N);
            CHECK_INT(flat_dpend_take(&dt, &A, &SA, 1, 1, 38, 18, &o), 0);
            CHECK_INT(flat_dpend_take(&dt, &E, &SA, 1, 1, 38, 18, &o), 1);
            CHECK_INT(o.val, 5);
            CHECK_INT(flat_dpend_take(&dt, &B, &SA, 1, 1, 38, 18, &o), 1);
            CHECK_INT(o.val, 2);
        }
        CHECK_INT(flat_push_button(0, 0, 0, 18, 9, 0x0003), 0);               /* Locale scroller arrow: RELVERIFY|IMMEDIATE */
        for (pressed = 0; pressed <= 1; pressed++) {
            memset(&t, 0, sizeof t);
            CHECK_INT(flat_frame_draw_round(&fs, pressed, 10, 10, 30, 14, 1, FLAT_ROLE_PLAIN, round_fill, round_read, cv), 1);
            flat_tx_frame(&t, &surf, &task, flat_tx_arm(pressed, 1, FLAT_ROLE_PLAIN), 1, 10, 10, 39, 23);
            CHECK_INT(cv->pix[16][20], 7);
            CHECK_INT(t.armed, 0);
        }
        CHECK_INT(flat_tx_arm(0, 1, FLAT_ROLE_PRIMARY), 1);
        CHECK_INT(flat_tx_arm(0, 1, FLAT_ROLE_SECONDARY), 1);                /* OWNER-2026-11-08 FLAT-SECONDARY-LABEL: a secondary fill (pen_button) takes the background-pen label */
        CHECK_INT(flat_tx_arm(0, 0, FLAT_ROLE_SECONDARY), 0);
        free(cv);
    }
    {   /* FLAT-BUTTON-FILL: the pressed-label arm and the depth-checked fill pen */
        flat_tx t; flat_state bs; int a, b, c1, c2;
        memset(&t, 0, sizeof t);
        a = 1; b = 2; c1 = 3; c2 = 4;
        CHECK_INT(flat_tx_text(&t, &a, &c1, 12, 22), 0);                      /* not armed */
        flat_tx_frame(&t, &a, &c1, 0, 1, 10, 20, 17, 25);                     /* normal frame: no arm */
        CHECK_INT(t.armed, 0);
        flat_tx_frame(&t, &a, &c1, 1, 0, 10, 20, 17, 25);                     /* pressed but edges-only: no arm */
        CHECK_INT(t.armed, 0);
        flat_tx_frame(&t, &a, &c1, 1, 1, 10, 20, 17, 25);
        CHECK_INT(t.armed, 1);
        CHECK_INT(flat_tx_text(&t, &a, &c1, 12, 22), 1);                      /* match */
        CHECK_INT(flat_tx_text(&t, &a, &c1, 10, 25) && flat_tx_text(&t, &a, &c1, 17, 20), 1);   /* box corners are inside */
        CHECK_INT(flat_tx_text(&t, &b, &c1, 12, 22), 0);                      /* other rastport: untouched */
        CHECK_INT(t.armed, 1);
        CHECK_INT(flat_tx_text(&t, &a, &c2, 12, 22), 0);                      /* other task: untouched */
        CHECK_INT(t.armed, 1);
        CHECK_INT(flat_tx_text(&t, &a, &c1, 9, 22), 0);                       /* outside the box: disarms */
        CHECK_INT(t.armed, 0);
        CHECK_INT(flat_tx_text(&t, &a, &c1, 12, 22), 0);
        memset(&bs, 0, sizeof bs);                                           /* pen_ptext = DK_BG = pen 0: a real pen */
        bs.enabled = 1; bs.pen_ptext = DK_BG;
        flat_tx_frame(&t, &a, &c1, 1, 1, 10, 20, 17, 25);
        CHECK_INT(flat_tx_pen(&bs, &t, &a, &c1, 12, 22), 0);                  /* pressed label: pen 0, not "unset" */
        CHECK_INT(flat_tx_pen(&bs, &t, &a, &c2, 12, 22), -1);                 /* other task: stock */
        bs.held = 1;
        CHECK_INT(flat_tx_pen(&bs, &t, &a, &c1, 12, 22), -1);                 /* held: stock */
        bs.held = 0; bs.enabled = 0;
        CHECK_INT(flat_tx_pen(&bs, &t, &a, &c1, 12, 22), -1);                 /* off: stock */
        flat_tx_frame(&t, &a, &c1, 1, 1, 10, 20, 17, 25);
        flat_tx_frame(&t, &a, &c1, 0, 1, 30, 20, 37, 25);                     /* another frame draw on the rastport: disarms */
        CHECK_INT(t.armed, 0);
        flat_tx_frame(&t, &a, &c1, 1, 1, 10, 20, 17, 25);
        flat_tx_frame(&t, &b, &c1, 0, 1, 30, 20, 37, 25);                     /* a draw on another rastport keeps it */
        CHECK_INT(t.armed, 1);
        {   /* pressed label drawn through a second RastPort of the same window layer (3.2 intuition) still matches */
            int layer, bm1, bm2;
            memset(&t, 0, sizeof t);
            flat_tx_frame(&t, flat_tx_surface(&layer, &bm1), &c1, 1, 1, 10, 44, 109, 59);
            bs.enabled = 1;
            CHECK_INT(flat_tx_pen(&bs, &t, flat_tx_surface(&layer, &bm2), &c1, 40, 53), 0);
            CHECK_INT(flat_tx_surface(0, &bm1) == (const void *)&bm1, 1);       /* no layer: the bitmap */
            CHECK_INT(flat_tx_surface(0, &bm1) != flat_tx_surface(0, &bm2), 1);
        }
        {   int w, h, img = 0, other = 0;
            flat_frame_size(0, 4096, -17232, 30, 12, &w, &h);                 /* IM_DRAW: the Dimensions words are junk */
            CHECK_INT(w == 30 && h == 12, 1);
            flat_frame_size(1, 50, 20, 30, 12, &w, &h);                       /* IM_DRAWFRAME: Dimensions win */
            CHECK_INT(w == 50 && h == 20, 1);
            flat_frame_size(1, 0, 0, 30, 12, &w, &h);
            CHECK_INT(w == 30 && h == 12, 1);
            CHECK_INT(flat_button_gadget(&img, &img, &img), 1);               /* button / cycle: render == select == frame */
            CHECK_INT(flat_button_gadget(&img, 0, &img), 0);                  /* slider / string: no select image */
            CHECK_INT(flat_button_gadget(&img, &other, &img), 0);
            CHECK_INT(flat_button_gadget(&other, &other, &img), 0);           /* another gadget's image */
            CHECK_INT(flat_button_gadget(0, 0, 0), 0);
            /* PALETTE-CYCLE-FLAT: rig 12 Palette 40.2 "Multicolor Settings" cycle, bool gadget 315,17 293x14 */
            CHECK_INT(flat_cycle_frame(0x0001, 0x0001, 315, 17, 293, 14, 315, 17, 293, 14), 1);
            CHECK_INT(flat_cycle_frame(0x1001, 0x0001, 315, 17, 293, 14, 315, 17, 293, 14), 1);   /* GTYP_GADTOOLS bit set */
            CHECK_INT(flat_cycle_frame(0x0001, 0x0001, 315, 17, 293, 14, 316, 18, 291, 12), 0);   /* an inner box (list frame) */
            CHECK_INT(flat_cycle_frame(0x0001, 0x0003, 589, 123, 19, 12, 589, 123, 19, 12), 0);   /* scroller arrow: IMMEDIATE */
            CHECK_INT(flat_cycle_frame(0x0003, 0x0001, 149, 137, 149, 7, 149, 137, 149, 7), 0);   /* prop */
            CHECK_INT(flat_cycle_frame(0x0004, 0x0001, 20, 20, 100, 14, 20, 20, 100, 14), 0);     /* string */
            {   /* PALETTE-CYCLE-PAPER: the cycle face is the accent, the inside is the surface pen (a Patina window) */
                flat_state cs; int in[4], sw;
                memset(&cs, 0, sizeof cs);
                cs.side = 2; cs.pen_line = 7; cs.pen_back = 0; cs.pen_button = 3;
                sw = flat_button_side(cs.side, 293);
                CHECK_INT(flat_cycle_paper(&cs, 315, 17, 293, 14, in), 0);
                CHECK_INT(cs.pen_button, 7);
                CHECK_INT(in[0] == 315 + sw && in[1] == 18 && in[2] == 607 - sw && in[3] == 29, 1);
                CHECK_INT(sw >= 1, 1);
            }
            CHECK_INT(flat_cycle_frame(0x0005, 0x0001, 15, 20, 285, 20, 15, 20, 285, 20), 0);     /* custom (Palette's colour wheel) */
            /* C8-RING: the 3.1 cycle's frame is its render == select image: still a cycle; a push button with the same box is not */
            CHECK_INT(flat_cycle_of(1, 0, 1), 1);
            CHECK_INT(flat_cycle_of(0, 0, 1), 1);
            CHECK_INT(flat_cycle_of(1, 1, 1), 0);
            CHECK_INT(flat_cycle_of(1, 0, 0), 0);
            CHECK_INT(flat_cycle_of(0, 0, 0), 0);
            CHECK_INT(flat_cycle_frame(0x8001, 0x0001, 0, 0, 18, 11, 0, 0, 18, 11), 0);           /* system gadget */
            CHECK_INT(flat_cycle_frame(0x0001, 0x0001, 0, 0, 0, 0, 0, 0, 0, 0), 0);
            /* a slider holder also matches, but noline_frame classes its frame TRACK first and draws nothing when filled */
            CHECK_INT(flat_track_frame(FLAT_FC_TRACK, 0, 1, 5), 1);
        }
        {   /* INSTALLER-BORDER-BUTTON: the 3.2 Installer's buttons (rig 97 gprobe: type 5, flags 0002, render != select, no text, 238x16) */
            int ren = 0, sel = 0;
            flat_box b = { 8, 174, 238, 16 };
            CHECK_INT(flat_border_button(5, 0x0002, &ren, &sel, 0, 238, 16), 1);
            CHECK_INT(flat_border_button(0x0005, 0x0082, &ren, &sel, 0, 238, 16), 1);     /* pressed (GFLG_SELECTED) is still one */
            CHECK_INT(flat_border_button(5, 0x0006, &ren, &sel, 0, 238, 16), 0);          /* GFLG_GADGIMAGE: image renders (frameiclass path) */
            CHECK_INT(flat_border_button(5, 0x0002, &ren, &ren, 0, 238, 16), 0);          /* render == select: a GadTools-style image button */
            CHECK_INT(flat_border_button(5, 0x0002, &ren, 0, 0, 238, 16), 0);             /* no select render */
            CHECK_INT(flat_border_button(5, 0x0002, &ren, &sel, 1, 238, 16), 0);          /* own IntuiText label */
            CHECK_INT(flat_border_button(1, 0x0002, &ren, &sel, 0, 238, 16), 0);          /* GTYP_BOOLGADGET */
            CHECK_INT(flat_border_button(0x8005, 0x0002, &ren, &sel, 0, 238, 16), 0);     /* GTYP_SYSGADGET */
            CHECK_INT(flat_border_button(5, 0x0012, &ren, &sel, 0, 238, 16), 0);          /* GFLG_RELRIGHT */
            CHECK_INT(flat_border_button(5, 0x0000, &ren, &sel, 0, 238, 16), 0);          /* GADGHCOMP: no select render drawn */
            CHECK_INT(flat_border_button(5, 0x0002, &ren, &sel, 0, 2, 16), 0);            /* too small to round */
            CHECK_INT(flat_box_exact(&b, 8, 174, 245, 189), 1);                           /* the measured body erase */
            CHECK_INT(flat_box_exact(&b, 8, 174, 245, 174), 0);                           /* one ring strip */
            CHECK_INT(flat_box_exact(&b, 9, 175, 244, 188), 0);
            CHECK_INT(flat_box_exact(0, 8, 174, 245, 189), 0);
            CHECK_INT(flat_border_button_ids(0x0002), 0);
            CHECK_INT(flat_border_button_ids(0x0082), 1);
        }
        memset(&bs, 0, sizeof bs);
        bs.pen_back = 0; bs.pen_button = 4;
        CHECK_INT(flat_button_pen(&bs, 3), 0);                                /* S33-PREFS-SKIN-FAIL: the role number is not a held pen: pen_back */
        bs.pen_sel = 9;
        CHECK_INT(flat_button_pen(&bs, 3), 0);                                /* 9 is no pen of 3 planes */
        bs.pen_sel = 4;
        CHECK_INT(flat_button_pen(&bs, 3), 4);                                /* 8 colours: the held button pen */
        CHECK_INT(flat_button_pen(&bs, 2), 0);                                /* 4 colours, no accent set: pen_back */
        bs.pen_button = 0; bs.pen_sel = 0;
        CHECK_INT(flat_button_pen(&bs, 3), 0);                                /* unset: pen_back */
    }
    {   /* FLAT-OUTER-EDGE: flat_outline_rects paints exactly the outer 1 px ring of a w x h window, each pixel once, nothing inside */
        static const int sz[][2] = { { 2, 2 }, { 3, 3 }, { 7, 5 }, { 64, 40 }, { 640, 256 } };
        int i, x, y;
        flat_state os;
        memset(&os, 0, sizeof os);
        os.pen_outline = 5;
        for (i = 0; i < 5; i++) {
            int w = sz[i][0], h = sz[i][1], bad = 0;
            static unsigned char cv[640 * 256];
            struct outline_cv_t c;
            memset(cv, 0, sizeof cv); c.cv = cv; c.w = w; c.h = h; c.bad = 0;
            CHECK_INT(flat_outline_rects(&os, w, h, outline_fill, &c), 4);
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++) {
                    int edge = x == 0 || y == 0 || x == w - 1 || y == h - 1;
                    if (cv[y * w + x] != (edge ? 1 : 0))
                        bad++;
                }
            CHECK_INT(bad + c.bad, 0);                                       /* ring only, each pixel once, in the pen, in bounds */
        }
        os.pen_outline = 0;
        {   unsigned char one[16]; struct outline_cv_t c;
            memset(one, 0, sizeof one); c.cv = one; c.w = 4; c.h = 4; c.bad = 0;
            CHECK_INT(flat_outline_rects(&os, 4, 4, outline_fill, &c), 0);   /* unset pen: no rects */
            CHECK_INT(one[0] | one[5] | one[15], 0);
            os.pen_outline = 5;
            CHECK_INT(flat_outline_rects(&os, 1, 9, outline_fill, &c), 0);   /* under 2 x 2: none */
            CHECK_INT(flat_outline_rects(0, 4, 4, outline_fill, &c), 0);
        }
    }
    {   /* FLAT-OUTER-EDGE: the RectFill patch's selector accepts exactly Intuition's four outer strips, nothing else */
        static const int sz[][2] = { { 220, 90 }, { 400, 100 }, { 640, 256 }, { 3, 3 } };
        int i;
        for (i = 0; i < 4; i++) {
            int w = sz[i][0], h = sz[i][1];
            CHECK_INT(flat_outer_edge_match(w, h, 0, 0, w - 1, 0), 1);            /* top */
            CHECK_INT(flat_outer_edge_match(w, h, 0, 1, 0, h - 1), 1);            /* left */
            CHECK_INT(flat_outer_edge_match(w, h, w - 1, 1, w - 1, h - 2), 1);    /* right */
            CHECK_INT(flat_outer_edge_match(w, h, 1, h - 1, w - 1, h - 1), 1);    /* bottom */
            CHECK_INT(flat_outer_edge_match(w, h, 0, 0, w - 2, 0), 0);            /* off by one */
            CHECK_INT(flat_outer_edge_match(w, h, 0, 0, w, 0), 0);
            CHECK_INT(flat_outer_edge_match(w, h, 0, 1, 0, h - 2), 0);
            CHECK_INT(flat_outer_edge_match(w, h, 0, h - 1, w - 1, h - 1), 0);    /* bottom starting at the corner */
            CHECK_INT(flat_outer_edge_match(w, h, 0, 0, w - 1, 1), 0);            /* 2 px thick */
            CHECK_INT(flat_outer_edge_match(w, h, 1, 1, w - 2, 1), 0);            /* inner bevel rects */
            CHECK_INT(flat_outer_edge_match(w, h, 1, 2, 1, h - 2), 0);
            CHECK_INT(flat_outer_edge_match(w, h, 3, 13, w - 18, 13), 0);
        }
        CHECK_INT(flat_outer_edge_match(1, 9, 0, 0, 0, 0), 0);                   /* under 2 x 2 */
    }
    {   /* FLAT-ASPECT-BORDERS: on PAL hires (22 x 44) the window's side lines are 2 px wide, so they look as thick as the top line */
        int r[4];
        CHECK_INT(flat_side_px(22, 44), 2);      /* PAL/NTSC hires non-interlace */
        CHECK_INT(flat_side_px(11, 44), 4);      /* super hires non-interlace */
        CHECK_INT(flat_side_px(22, 22), 1);      /* hires interlace: square */
        CHECK_INT(flat_side_px(44, 44), 1);      /* lores: square */
        CHECK_INT(flat_side_px(44, 22), 1);      /* lores interlace: wide pixels, sides stay 1 */
        CHECK_INT(flat_side_px(0, 0), 1);        /* unknown mode / RTG without DisplayInfo */
        CHECK_INT(flat_side_px(30, 44), 1);      /* 1.47 rounds to 1 */
        {   /* FLAT-ASPECT-SLOTS: each screen keeps its own count, not only the Workbench one */
            flat_aspect_slot t[FLAT_ASPECT_SLOTS];
            int wb, hires, lores, i;     /* addresses only: the table never dereferences them */
            memset(t, 0, sizeof t);
            CHECK_INT(flat_aspect_find(t, FLAT_ASPECT_SLOTS, &wb), 1);              /* unknown screen */
            CHECK_INT(flat_aspect_need(t, FLAT_ASPECT_SLOTS, &wb, 5), 1);
            flat_aspect_put(t, FLAT_ASPECT_SLOTS, &wb, 5, flat_side_px(44, 44));    /* Workbench: lores, 1 */
            flat_aspect_put(t, FLAT_ASPECT_SLOTS, &hires, 7, flat_side_px(22, 44)); /* custom hires screen: 2 */
            flat_aspect_put(t, FLAT_ASPECT_SLOTS, &lores, 9, flat_side_px(11, 44)); /* super hires: 4 */
            CHECK_INT(flat_aspect_find(t, FLAT_ASPECT_SLOTS, &wb), 1);
            CHECK_INT(flat_aspect_find(t, FLAT_ASPECT_SLOTS, &hires), 2);
            CHECK_INT(flat_aspect_find(t, FLAT_ASPECT_SLOTS, &lores), 4);
            CHECK_INT(flat_aspect_find(t, FLAT_ASPECT_SLOTS, NULL), 1);
            CHECK_INT(flat_aspect_known(t, FLAT_ASPECT_SLOTS, &wb), 1);             /* FLAT-HOOK-NO-IBASE: published screens only */
            CHECK_INT(flat_aspect_known(t, FLAT_ASPECT_SLOTS, &lores), 1);
            CHECK_INT(flat_aspect_known(t, FLAT_ASPECT_SLOTS, &i), 0);              /* never published: the hook leaves it alone */
            CHECK_INT(flat_aspect_known(t, FLAT_ASPECT_SLOTS, NULL), 0);            /* an empty slot is not the NULL screen */
            CHECK_INT(flat_aspect_need(t, FLAT_ASPECT_SLOTS, &hires, 7), 0);        /* same mode: cached */
            CHECK_INT(flat_aspect_need(t, FLAT_ASPECT_SLOTS, &hires, 8), 1);        /* mode switched: measure again */
            flat_aspect_put(t, FLAT_ASPECT_SLOTS, &hires, 8, 1);
            CHECK_INT(flat_aspect_find(t, FLAT_ASPECT_SLOTS, &hires), 1);
            for (i = 0; i < FLAT_ASPECT_SLOTS + 2; i++) {                            /* overflow evicts the last, keeps the rest */
                static int more[FLAT_ASPECT_SLOTS + 2];
                flat_aspect_put(t, FLAT_ASPECT_SLOTS, &more[i], 1, 3);
            }
            CHECK_INT(flat_aspect_find(t, FLAT_ASPECT_SLOTS, &lores), 4);
        }
        CHECK_INT(flat_outer_edge_fill(640, 256, 0, 1, 0, 255, flat_side_px(22, 44), r), 1);       /* left: x 0..1 */
        CHECK_INT(r[0], 0); CHECK_INT(r[1], 1); CHECK_INT(r[2], 1); CHECK_INT(r[3], 255);
        CHECK_INT(flat_outer_edge_fill(640, 256, 639, 1, 639, 254, 2, r), 1);   /* right: x 638..639 */
        CHECK_INT(r[0], 638); CHECK_INT(r[1], 1); CHECK_INT(r[2], 639); CHECK_INT(r[3], 254);
        CHECK_INT(flat_outer_edge_fill(640, 256, 0, 0, 639, 0, 2, r), 1);       /* top stays 1 px */
        CHECK_INT(r[0], 0); CHECK_INT(r[1], 0); CHECK_INT(r[2], 639); CHECK_INT(r[3], 0);
        CHECK_INT(flat_outer_edge_fill(640, 256, 1, 255, 639, 255, 2, r), 1);   /* bottom stays 1 px */
        CHECK_INT(r[1], 255); CHECK_INT(r[3], 255);
        CHECK_INT(flat_outer_edge_fill(640, 512, 0, 1, 0, 511, flat_side_px(22, 22), r), 1);       /* interlace: left stays 1 px */
        CHECK_INT(r[0], 0); CHECK_INT(r[2], 0);
        CHECK_INT(flat_outer_edge_fill(3, 3, 0, 1, 0, 2, 4, r), 1);             /* px clamps to w / 2 */
        CHECK_INT(r[2], 0);
        CHECK_INT(flat_outer_edge_fill(640, 256, 1, 1, 1, 254, 2, r), 0);       /* not an outer strip */
        {   /* S32-ARROW-OUTLINE: a gadget box reaching the last column / bottom row overlaps the ring */
            ring_rec rr;
            memset(&rr, 0, sizeof rr);
            CHECK_INT(flat_outer_ring_overlap(640, 256, 2, 622, 214, 639, 224, ring_rec_fill, &rr, 7), 1);     /* up arrow: x 638..639 */
            CHECK_INT(rr.n, 1); CHECK_INT(rr.pen, 7);
            CHECK_INT(rr.r[0][0], 638); CHECK_INT(rr.r[0][1], 214); CHECK_INT(rr.r[0][2], 639); CHECK_INT(rr.r[0][3], 224);
            memset(&rr, 0, sizeof rr);
            CHECK_INT(flat_outer_ring_overlap(640, 256, 2, 622, 236, 639, 255, ring_rec_fill, &rr, 7), 2);     /* size gadget: right + bottom */
            CHECK_INT(rr.n, 2);
            CHECK_INT(rr.r[0][0], 638); CHECK_INT(rr.r[0][3], 254);
            CHECK_INT(rr.r[1][1], 255); CHECK_INT(rr.r[1][0], 622); CHECK_INT(rr.r[1][2], 639);
            memset(&rr, 0, sizeof rr);
            CHECK_INT(flat_outer_ring_overlap(640, 256, 2, 100, 100, 200, 200, ring_rec_fill, &rr, 7), 0);     /* away from the edge */
            CHECK_INT(rr.n, 0);
        }
    }
    CHECK_INT((int)flat_which_alias(5), FLAT_W_DEPTH);
    CHECK_INT((int)flat_which_alias(3), FLAT_W_CLOSE);
}

/* FLAT-SCROLLER: knob = the lighter pen (pen_button), track = the frame pen (pen_back), nothing outside the container, nothing
 * when flat is off or held. Same rule for every state: the function takes no active flag. */
/* MODERN-SLIDER: slider pick, track line and round knob rows */
static void test_slider(void)
{
    int x0, x1, i, w[8], t[4];
    CHECK_INT(flat_prop_slider(0, 2, 6, 7, 1), 1);
    CHECK_INT(flat_prop_slider(0x10, 2, 6, 7, 1), 0);
    CHECK_INT(flat_prop_slider(0, 2, 40, 7, 1), 0);
    CHECK_INT(flat_prop_slider(0, 6, 6, 7, 1), 0);
    CHECK_INT(flat_prop_slider(0, 4, 14, 7, 2), 2);
    CHECK_INT(flat_prop_slider(0, 4, 14, 30, 2), 0);
    for (i = 0; i < 8; i++)
        w[i] = flat_slider_disc_row(1, 10, 20, 6, 7, 20 + i, &x0, &x1) ? x1 - x0 + 1 : 0;
    CHECK_INT(w[0], 4); CHECK_INT(w[1], 6); CHECK_INT(w[4], 6);
    CHECK_INT(w[5], 4); CHECK_INT(w[6], 0);
    CHECK_INT(flat_slider_disc_row(1, 10, 20, 6, 7, 25, &x0, &x1) ? x0 : -1, 11);
    for (i = 0; i < 8; i++)
        w[i] = flat_slider_disc_row(2, 10, 20, 6, 7, 20 + i, &x0, &x1) ? x1 - x0 + 1 : 0;
    /* side 2: min(6 / 2, 7) = 3 rows (22..24) of 6 columns, 3 x 3 visual units; rows 20, 21, 25, 26 miss it */
    CHECK_INT((w[0] == 0) + (w[1] == 0) + (w[5] == 0) + (w[6] == 0), 4);
    CHECK_INT(w[2], 4); CHECK_INT(w[3], 6); CHECK_INT(w[4], 4);
    CHECK_INT(flat_slider_track(1, 1, 100, 50, 149, 7, 0, 0, 999, 999, t), 1);
    CHECK_INT(t[1], 53); CHECK_INT(t[3], 53);
    CHECK_INT(t[0], 100); CHECK_INT(t[2], 248);
    CHECK_INT(flat_slider_track(1, 1, 100, 50, 149, 7, 0, 50, 999, 52, t), 0);
    CHECK_INT(flat_slider_track(1, 1, 100, 50, 149, 7, 120, 0, 130, 999, t) ? t[2] - t[0] : -1, 10);
    CHECK_INT(flat_slider_track(2, 2, 10, 0, 16, 90, 0, 0, 999, 999, t), 1);
    CHECK_INT(t[0], 17); CHECK_INT(t[2] - t[0] + 1, 2);
    {   /* GSLIDER-FLAT: ring repaint only after GM_RENDER of an absolute box on a themed screen with flat on */
        flat_state g;
        memset(&g, 0, sizeof g);
        g.enabled = 1;
        CHECK_INT(flat_gslider_ring(&g, FLAT_GM_RENDER, 1, 0, 18, 91), 1);
        CHECK_INT(flat_gslider_ring(&g, 0x202, 1, 0, 18, 91), 0);        /* not GM_RENDER */
        CHECK_INT(flat_gslider_ring(&g, FLAT_GM_RENDER, 0, 0, 18, 91), 0);  /* stock screen */
        CHECK_INT(flat_gslider_ring(&g, FLAT_GM_RENDER, 1, 0x0010, 18, 91), 0);   /* GFLG_RELRIGHT */
        CHECK_INT(flat_gslider_ring(&g, FLAT_GM_RENDER, 1, 0, 4, 91), 0);   /* nothing inside the ring */
        g.held = 1;
        CHECK_INT(flat_gslider_ring(&g, FLAT_GM_RENDER, 1, 0, 18, 91), 0);
    }
}

static void test_scroller(void)
{
    flat_state s;
    int kn, tr;
    flat_init(&s);
    desk_flat_state(1, 1, 1, &s);
    kn = flat_knob_state_pen(&s, 3, 1);                                  /* S86-ACTIVE-SCROLL-GREY: the active knob shares the inactive grey (was the accent pen_outline) */
    tr = s.pen_back;
    CHECK(kn != tr);
    CHECK(kn != DK_SELECTION);
    CHECK(kn != 0);
    CHECK(kn < 8);
    /* vertical 16 x 100 container at (10,20), body half: knob height 50 */
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), kn);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 70, 25, 119), tr);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0xFFFF, 0, 0x8000, 10, 70, 25, 119), kn);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0xFFFF, 0, 0x8000, 10, 20, 25, 69), tr);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 60, 25, 80), tr);   /* straddles the knob edge */
    /* S56-XCON-KNOB-BOX: rig 74 (3.2) XCON scrollback, container 10 x 195 at (626,12), vpot 0xFFFF, vbody 0x5af2. Intuition's
     * AUTOKNOB Image says the knob is 10 x 70 at container row 125 (rows 137..206); the body formula says 69 rows at 138. The
     * knob comes from the Image box, so Intuition's own knob fill takes the knob pen (it straddled the formula box: track pen,
     * the owner's thin-line knob), and the fill just above it is track. */
    {
        int kx, ky, kw, kh;
        CHECK_INT(flat_scroller_knob_img(626, 12, 10, 195, 1, 0, 125, 10, 70, 0, 1, 0, 0xFFFF, 0, 0x5af2, &kx, &ky, &kw, &kh), 1);
        CHECK_INT(ky, 137);
        CHECK_INT(kh, 70);
        CHECK_INT(flat_scroller_pen_box(&s, 3, 1, 626, 12, 10, 195, kx, ky, kw, kh, 626, 137, 635, 206), kn);
        CHECK_INT(flat_scroller_pen_box(&s, 3, 1, 626, 12, 10, 195, kx, ky, kw, kh, 626, 12, 635, 136), tr);
        CHECK_INT(flat_scroller_knob_img(626, 12, 10, 195, 1, 0, 130, 10, 70, 0, 1, 0, 0xFFFF, 0, 0x5af2, &kx, &ky, &kw, &kh), 1);
        CHECK_INT(ky, 138);                                                                      /* a box leaving the container: the formula */
        CHECK_INT(kh, 69);
        CHECK_INT(flat_scroller_knob_img(626, 12, 10, 195, 0, 0, 125, 10, 70, 0, 1, 0, 0xFFFF, 0, 0x5af2, &kx, &ky, &kw, &kh), 1);
        CHECK_INT(ky, 138);                                                                      /* no AUTOKNOB Image: the formula */
    }
    /* FLAT-SLIDER-ROUND: a fill that is exactly the knob gets the button corner cuts; any other fill gets none */
    {
        int cuts[FLAT_ROUND_MAX][2], n, i, hit = 0;
        n = flat_scroller_cuts(10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69, cuts);   /* knob 16 x 50 at (10,20): tall, 12 cuts */
        CHECK_INT(n, 12);
        for (i = 0; i < n; i++)
            if (cuts[i][0] == 10 && cuts[i][1] == 20) hit = 1;
        CHECK_INT(hit, 1);                                                                       /* top left knob pixel is cut */
        CHECK_INT(flat_scroller_cuts(10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 11, 22, 24, 67, cuts), 0);
        CHECK_INT(flat_scroller_cuts(10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 70, 25, 119, cuts), 0);   /* the track */
        CHECK_INT(flat_scroller_cuts(10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 12, 22, 20, 30, cuts), 0);
    }
    /* RL6: 3.2 paints the knob as several fills (frame strips + interior), never one fill equal to the knob: every strip cuts the
     * corner pixels it contains, the interior cuts none, and across the strips every corner pixel is cut */
    {
        int cuts[FLAT_ROUND_MAX][2], all[FLAT_ROUND_MAX][2], n, i, j, total = 0, seen_tl = 0, seen_br = 0;
        static const int strip[5][4] = { {10, 20, 25, 20}, {10, 69, 25, 69}, {10, 21, 10, 68}, {25, 21, 25, 68}, {11, 21, 24, 68} };
        for (j = 0; j < 5; j++) {
            n = flat_scroller_cuts(10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, strip[j][0], strip[j][1], strip[j][2], strip[j][3], cuts);
            if (j == 4) CHECK_INT(n, 0);                                                         /* the interior has no corner */
            for (i = 0; i < n; i++) {
                CHECK_INT(cuts[i][0] >= strip[j][0] && cuts[i][0] <= strip[j][2] && cuts[i][1] >= strip[j][1] && cuts[i][1] <= strip[j][3], 1);
                all[total][0] = cuts[i][0]; all[total][1] = cuts[i][1]; total++;
                if (cuts[i][0] == 10 && cuts[i][1] == 20) seen_tl = 1;
                if (cuts[i][0] == 25 && cuts[i][1] == 69) seen_br = 1;
            }
        }
        CHECK_INT(seen_tl, 1);
        CHECK_INT(seen_br, 1);
        CHECK_INT(total >= 12, 1);                                                               /* every cut of the whole knob is covered */
    }
    /* a tiny body still gives a 1 px knob */
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 1, 10, 20, 25, 20), kn);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 1, 10, 21, 25, 21), tr);
    /* horizontal 100 x 10, body a quarter (knob width 25), pot at the end: knob x 75..99 */
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 0, 0, 100, 10, 1, 0, 0xFFFF, 0, 0x4000, 0, 75, 0, 99, 9), kn);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 0, 0, 100, 10, 1, 0, 0xFFFF, 0, 0x4000, 0, 0, 0, 74, 9), tr);
    /* outside the container, flat held, flat off: -1, the call stays alone */
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 9, 20, 25, 69), -1);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 26, 69), -1);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 120), -1);
    s.held = 1;
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), -1);
    s.held = 0;
    /* S86-ACTIVE-SCROLL-GREY: active == inactive -> pen_inbar, missing/invalid -> today's knob pen */
    {
        int ko = s.pen_outline, ki = s.pen_inbar;
        s.pen_outline = 5; s.pen_inbar = 6;
        CHECK_INT(flat_knob_state_pen(&s, 4, 1), 6);
        CHECK_INT(flat_knob_state_pen(&s, 4, 0), 6);
        CHECK_INT(flat_scroller_pen(&s, 4, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), 6);   /* through the scroller entry */
        CHECK_INT(flat_scroller_pen(&s, 4, 0, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), 6);
        s.pen_outline = 0; s.pen_inbar = 0;
        CHECK_INT(flat_knob_state_pen(&s, 4, 1), flat_knob_pen(&s, 4));
        CHECK_INT(flat_knob_state_pen(&s, 4, 0), flat_knob_pen(&s, 4));
        s.pen_outline = 20; s.pen_inbar = 20;                            /* not pens of 4 planes */
        CHECK_INT(flat_knob_state_pen(&s, 4, 1), flat_knob_pen(&s, 4));
        CHECK_INT(flat_knob_state_pen(&s, 4, 0), flat_knob_pen(&s, 4));
        s.pen_outline = ko; s.pen_inbar = ki;
    }
    s.pen_outline = 0;                                                   /* below: the knob fallback chain */
    /* under 8 colours the knob takes the accent pen_line (flat_button_pen), never a missing pen */
    CHECK_INT(flat_scroller_pen(&s, 2, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), s.pen_line);
    /* fallback: pen_dimtext 0 or not a pen of the depth keeps flat_button_pen */
    {
        int keep = s.pen_dimtext;
        s.pen_dimtext = 0;
        CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), flat_button_pen(&s, 3));
        s.pen_dimtext = 9;
        CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), flat_button_pen(&s, 3));
        s.pen_dimtext = keep;
    }
    s.enabled = 0;
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), -1);
}

/* ---- FLAT-GLYPH-OS-SHAPES: the clone. A fake 3D close gadget (9 x 9) as the stock image draws it with marker pens: bevel ring
 * (shine top / left, shadow bottom / right), ground, a 3 x 3 glyph in the text marker. The one reachability test: through
 * flat_clone_draw (what flat.c runs) with a render sentinel; the pens are checked on the canvas. */
static int g_render_calls, g_render_fail;
static int g_render_selected;
static int fake_render(void *ctx, unsigned long ids, uint8_t *grid, int w, int h)
{
    int x, y;
    (void)ctx;
    g_render_calls++;
    if (g_render_fail)
        return 0;
    g_render_selected = ids == 1;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int ring = x == 0 || y == 0 || x == w - 1 || y == h - 1;
            int m = FLAT_CLONE_MARK(g_render_selected ? 5 : 7);                 /* ground: FILLPEN selected, BACKGROUNDPEN normal */
            if (ring)
                m = (x == 0 || y == 0) != g_render_selected ? FLAT_CLONE_SHINE : FLAT_CLONE_SHADOW;
            else if (x >= 3 && x <= 5 && y >= 3 && y <= 5)
                m = FLAT_CLONE_MARK(2);                                         /* the glyph */
            grid[y * w + x] = (uint8_t)m;
        }
    return 1;
}

static int blank_render(void *ctx, unsigned long ids, uint8_t *grid, int w, int h)
{
    (void)ctx; (void)ids; (void)grid; (void)w; (void)h;
    g_render_calls++;
    return 1;                                                                   /* OS 3.2 window gadgets: rc 1, nothing drawn */
}

static uint8_t g_scr[FLAT_CLONE_SCRATCH];

/* S31-ARROW-ASPECT: on hires (flat_side_px 2) the owner saw narrow left/right and too-big up/down arrows. Every arrow must have
 * the same 45-degree shape in visual units (a row counts side columns): left/right base (rows * side) == 2 * length (columns),
 * up/down widest row == 2 * side * (rows - 1) + 1; the glyph is centred in its box. Box sizes are the rig's 3.2 hires ones. */
static void arrow_bbox(const flat_state *st, unsigned long w, int bw, int bh, int side, int *x0, int *x1, int *rows, int *top, int *bot)
{
    int x, y, y0 = -1, y1 = -1;
    cv_clear();
    *x0 = CV; *x1 = -1;
    CHECK_INT(flat_glyph_draw(st, w, FLAT_IM_DRAW, 0, 0, 0, bw, bh, st->pen_back, st->pen_text, -1, side, cv_fill, 0), 1);
    for (y = 0; y < bh; y++)
        for (x = 0; x < bw; x++)
            if (cv[y][x] == (char)st->pen_text) {
                if (x < *x0) *x0 = x;
                if (x > *x1) *x1 = x;
                if (y0 < 0) y0 = y;
                y1 = y;
            }
    *rows = y1 - y0 + 1;
    if (top) *top = y0;
    if (bot) *bot = y1;
}

/* S69-ARROW-SPACING: the margin around every arrow is the same rule on every box and pixel aspect: centred on both axes, left
 * and right (top and bottom) margins differ by at most the one odd pixel. */
static void test_arrow_spacing(void)
{
    flat_state st; unsigned long w; int side, i, x0, x1, rows, top, bot;
    int bs[3][2] = { {16,10}, {18,11}, {18,12} };
    flat_init(&st); desk_flat_state(1, 1, 1, &st);
    for (side = 1; side <= 2; side++) for (i = 0; i < 3; i++) for (w = FLAT_W_LEFT; w <= FLAT_W_DOWN; w++) {
        int bw = bs[i][0], bh = bs[i][1];
        arrow_bbox(&st, w, bw, bh, side, &x0, &x1, &rows, &top, &bot);
        CHECK(abs(x0 - (bw - 1 - x1)) <= 1);
        CHECK(abs(top - (bh - 1 - bot)) <= 1);
    }
}

static void test_arrow_aspect(void)
{
    flat_state st;
    unsigned long w;
    int side, x0, x1, rows, top, bot, lw[3] = { 0 }, uh[3] = { 0 }, lb[3] = { 0 }, ub[3] = { 0 };
    flat_init(&st);
    desk_flat_state(1, 1, 1, &st);
    for (side = 1; side <= 2; side++) {
        for (w = FLAT_W_LEFT; w <= FLAT_W_DOWN; w++) {
            if (w == FLAT_W_LEFT || w == FLAT_W_RIGHT) {
                arrow_bbox(&st, w, 16, 10, side, &x0, &x1, &rows, 0, 0);
                CHECK_INT(rows * side, 2 * (x1 - x0 + 1) - side); /* odd base (one-row point): 2 * length - one row, visual units */
                CHECK(abs(x0 - (16 - 1 - x1)) <= 1);              /* centred */
                lw[side] = x1 - x0 + 1;
                lb[side] = rows * side;
            } else {
                arrow_bbox(&st, w, 18, 11, side, &x0, &x1, &rows, &top, &bot);
                CHECK_INT(x1 - x0 + 1, 2 * side * (rows - 1) + 1);
                CHECK(abs(x0 - (18 - 1 - x1)) <= 1);
                uh[side] = rows;
                ub[side] = x1 - x0 + 1;
            }
        }
    }
    CHECK_INT(lw[2], 2 * lw[1]);                                  /* hires: left/right twice as wide */
    CHECK(uh[1] > 0 && uh[2] > 0);
    /* S69-ARROW-EQUAL: on each screen the up/down base and the left/right base match within one row (visual units) */
    for (side = 1; side <= 2; side++)
        CHECK(abs(ub[side] - lb[side]) <= side);
}

/* S69-ARROW-ASPECT: arrows and the size gadget are flat's own glyphs, never the clone of the 2:1 stock shape. At side 1 the arms
 * are at 45 degrees (left/right: one column per row; up/down: two columns per row); at side 2 (hires) the proportion is the
 * hires one. Size: right triangle, one column per row at side 1, two at side 2. */
static int row_len(const flat_state *st, int y, int bw)
{
    int x, n = 0;
    for (x = 0; x < bw; x++) n += cv[y][x] == (char)st->pen_text;
    return n;
}

static int first_row(const flat_state *st, int bw, int bh)
{
    int y;
    for (y = 0; y < bh; y++)
        if (row_len(st, y, bw)) return y;
    return -1;
}

static void test_arrow_s69_shape(void)
{
    flat_state st;
    unsigned long w;
    int side, y, bw = 18, bh = 11;
    flat_init(&st);
    desk_flat_state(1, 1, 1, &st);
    for (w = FLAT_W_LEFT; w <= FLAT_W_DOWN; w++)
        CHECK_INT(flat_clone_route(&st, w, FLAT_IM_DRAW), 0);
    CHECK_INT(flat_clone_route(&st, FLAT_W_SIZE, FLAT_IM_DRAW), 0);
    CHECK_INT(flat_clone_route(&st, FLAT_W_CLOSE, FLAT_IM_DRAW), 1);            /* the other window gadgets keep the clone */
    CHECK_INT(flat_clone_route(&st, FLAT_W_DEPTH, FLAT_IM_DRAW), 1);
    CHECK_INT(flat_clone_route(&st, FLAT_W_ZOOM, FLAT_IM_DRAW), 1);
    CHECK_INT(flat_clone_route(&st, FLAT_W_ICONIFY, FLAT_IM_DRAW), 1);
    for (side = 1; side <= 2; side++) {
        /* left / right: the tip row has side columns, each row toward the middle adds side */
        for (w = FLAT_W_LEFT; w <= FLAT_W_RIGHT; w += FLAT_W_RIGHT - FLAT_W_LEFT) {
            int f;
            cv_clear();
            CHECK_INT(flat_glyph_draw(&st, w, FLAT_IM_DRAW, 0, 0, 0, bw, bh, st.pen_back, st.pen_text, -1, side, cv_fill, 0), 1);
            f = first_row(&st, bw, bh);
            CHECK(f >= 0);
            CHECK_INT(row_len(&st, f, bw), side);
            CHECK_INT(row_len(&st, f + 1, bw), 2 * side);
            CHECK_INT(row_len(&st, f + 2, bw), 3 * side);
        }
        /* up / down: consecutive glyph rows differ by 2 * side columns */
        for (w = FLAT_W_UP; w <= FLAT_W_DOWN; w += FLAT_W_DOWN - FLAT_W_UP) {
            int rows = 0, wprev = 0;
            cv_clear();
            CHECK_INT(flat_glyph_draw(&st, w, FLAT_IM_DRAW, 0, 0, 0, bw, bh, st.pen_back, st.pen_text, -1, side, cv_fill, 0), 1);
            for (y = 0; y < bh; y++) {
                int l = row_len(&st, y, bw);
                if (!l) continue;
                if (rows) CHECK_INT(abs(l - wprev), 2 * side);
                wprev = l; rows++;
            }
            CHECK(rows >= 3);
        }
        /* S69-ARROW-EQUAL: Intuition's boxes on a square-pixel screen (up/down 18x11, left/right 16x10): every arrow gets the
         * same base and depth, 7 x 4 (left/right had a 6-row base before, up/down a 13-column one) */
        if (side == 1) {
            int x, rows = 0, base = 0, depth = 0;
            cv_clear();
            CHECK_INT(flat_glyph_draw(&st, FLAT_W_LEFT, FLAT_IM_DRAW, 0, 0, 0, 16, 10, st.pen_back, st.pen_text, -1, 1, cv_fill, 0), 1);
            for (y = 0; y < 10; y++) {
                int l = row_len(&st, y, 16);
                if (l) { rows++; if (l > depth) depth = l; }
            }
            CHECK_INT(rows, 7);
            CHECK_INT(depth, 4);
            cv_clear();
            CHECK_INT(flat_glyph_draw(&st, FLAT_W_UP, FLAT_IM_DRAW, 0, 0, 0, 18, 11, st.pen_back, st.pen_text, -1, 1, cv_fill, 0), 1);
            rows = 0;
            for (y = 0; y < 11; y++) {
                int l = row_len(&st, y, 18);
                if (l) { rows++; if (l > base) base = l; }
            }
            CHECK_INT(base, 7);
            CHECK_INT(rows, 4);
            (void)x;
        }
        /* size: a right triangle on the lower right; from the top glyph row, each row adds side columns; the last row ends at the
         * inner box's right edge (the corner pixel is filled) */
        {
            int f, l0, l1, l2;
            cv_clear();
            CHECK_INT(flat_glyph_draw(&st, FLAT_W_SIZE, FLAT_IM_DRAW, 0, 0, 0, 16, 10, st.pen_back, st.pen_text, -1, side, cv_fill, 0), 1);
            f = first_row(&st, 16, 10);
            CHECK(f >= 0);
            l0 = row_len(&st, f, 16); l1 = row_len(&st, f + 1, 16); l2 = row_len(&st, f + 2, 16);
            CHECK_INT(l0, side);
            CHECK_INT(l1 - l0, side);
            CHECK_INT(l2 - l1, side);
            {   int x, e0 = -1, e2 = -1;                                   /* right-aligned: every row ends in the same column */
                for (x = 0; x < 16; x++) {
                    if (cv[f][x] == (char)st.pen_text) e0 = x;
                    if (cv[f + 2][x] == (char)st.pen_text) e2 = x;
                }
                CHECK_INT(e0, e2);
            }
        }
    }
}

/* S31 SCROLLER-ARROWS-INVISIBLE: OS 3.2's sysiclass draws nothing offscreen for the arrow images, so the clone fails and the
 * block glyph is what shows. Through the same two steps flat.c runs (clone, then glyph draw) with the pens desk_flat_state
 * gives: the canvas must hold the glyph pen AND a different box pen, for every arrow, state and box size seen on the rig. */
static void test_arrow_glyph_pens(void)
{
    static const int dims[2][2] = { { 16, 10 }, { 18, 11 } };
    static const int states[2] = { 0, 5 };                       /* IDS_NORMAL, IDS_INACTIVEDISABLED */
    flat_state st;
    unsigned long w;
    int d, si, x, y, glyph, box;
    flat_init(&st);
    desk_flat_state(1, 1, 1, &st);
    for (w = FLAT_W_LEFT; w <= FLAT_W_DOWN; w++)
        for (d = 0; d < 2; d++)
            for (si = 0; si < 2; si++) {
                int bw = dims[d][0], bh = dims[d][1], rc;
                cv_clear();
                rc = flat_clone_draw(&st, w, FLAT_IM_DRAW, (unsigned long)states[si], 0, 0, bw, bh, st.pen_back, st.pen_text, -1, 99, -1, blank_render, 0, cv_fill, 0, g_scr);
                CHECK_INT(rc, FLAT_CLONE_NO);                    /* S69-ARROW-ASPECT: arrows are never cloned */
                CHECK_INT(flat_glyph_draw(&st, w, FLAT_IM_DRAW, (unsigned long)states[si], 0, 0, bw, bh, st.pen_back, st.pen_text, -1, 1, cv_fill, 0), 1);
                glyph = box = 0;
                for (y = 0; y < bh; y++)
                    for (x = 0; x < bw; x++) {
                        glyph += cv[y][x] == (char)st.pen_text;           /* S32-ARROW-KNOB-COLOUR: the knob pen, also when disabled */
                        box += cv[y][x] == (char)st.pen_back;
                    }
                CHECK(glyph > 0);                                /* an arrow was drawn */
                CHECK(box > 0);                                  /* and the box shows around it */
                CHECK_INT(glyph + box, bw * bh);                 /* nothing else */
                CHECK(st.pen_text != st.pen_back);
            }
}

static void test_clone_arrows(void)
{
    flat_state st;
    unsigned long w;
    int g, b;
    glyph_state(&st);
    st.enabled = 1; st.held = 0;
    for (w = FLAT_W_LEFT; w <= FLAT_W_DOWN; w++) {
        CHECK_INT(flat_glyph_window_gadget(w), 1);                              /* scroller arrows take the clone path */
        CHECK_INT(flat_clone_route(&st, w, FLAT_IM_DRAW), 0);                    /* S69-ARROW-ASPECT: flat's own glyph, scaled by pixel aspect */
        CHECK_INT(flat_glyph_on_band(w), 0);                                    /* they sit in the border, not the title band */
        CHECK_INT(flat_glyph_known(w), 1);                                      /* OS 3.2 draws nothing offscreen: the block glyph is the arrow */
        flat_glyph_roles(w, FLAT_G_NORMAL, &g, &b);
        CHECK_INT(b, FLAT_ROLE_FRAME);
        CHECK(g != b);                                                          /* glyph pen differs from box pen */
    }
    CHECK_INT(flat_glyph_on_band(FLAT_W_CLOSE), 1);
    CHECK_INT(flat_glyph_on_band(FLAT_W_DEPTH), 1);
    CHECK_INT(flat_glyph_on_band(FLAT_W_ZOOM), 1);
    CHECK_INT(flat_glyph_on_band(FLAT_W_ICONIFY), 1);
    CHECK_INT(flat_glyph_on_band(FLAT_W_SIZE), 0);
    CHECK_INT(flat_clone_route(&st, FLAT_W_LEFT, 0), 0);                        /* only OM_DRAW */
    st.held = 1;
    CHECK_INT(flat_clone_route(&st, FLAT_W_LEFT, FLAT_IM_DRAW), 0);
    st.held = 0; st.enabled = 0;
    CHECK_INT(flat_clone_route(&st, FLAT_W_LEFT, FLAT_IM_DRAW), 0);
}

/* S32-ARROW-KNOB-COLOUR: scroller arrows and the size triangle always draw in the knob state pen (active accent, inactive bar tone);
 * pressed, the box takes that pen and the glyph pen_back; no pen_text anywhere; close and depth keep their old pens. Both paths:
 * flat_clone_draw (blank render -> FAIL, so the clone case uses a filled render) and flat_glyph_draw. */
static void knob_canvas(const flat_state *st, unsigned long w, unsigned long ids, int knob, int clone, int *glyph, int *box)
{
    int x, y, bw = 16, bh = 12, nb = 0, ng = 0, gp = -1, bp = -1;
    cv_clear();
    if (clone)      /* S69-ARROW-ASPECT: arrows and size are never cloned; the glyph path draws them in either case */
        CHECK_INT(flat_clone_draw(st, w, FLAT_IM_DRAW, ids, 0, 0, bw, bh, 'F', knob, -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_NO);
    CHECK_INT(flat_glyph_draw(st, w, FLAT_IM_DRAW, ids, 0, 0, bw, bh, 'F', knob, -1, 1, cv_fill, 0), 1);
    for (y = 0; y < bh; y++)
        for (x = 0; x < bw; x++) {
            int c = cv[y][x];
            if (c == cv[0][0]) { nb++; bp = c; }
            else { ng++; gp = c; }
        }
    CHECK(nb > 0 && ng > 0);
    *glyph = gp; *box = bp;
}

static void test_arrow_knob_colour(void)
{
    flat_state st;
    static const unsigned long ws[5] = { FLAT_W_LEFT, FLAT_W_UP, FLAT_W_RIGHT, FLAT_W_DOWN, FLAT_W_SIZE };
    int wi, clone, g, b;
    glyph_state(&st);
    st.enabled = 1; st.held = 0;
    st.pen_back = 'B'; st.pen_text = 'T'; st.pen_line = 'L';
    for (wi = 0; wi < 5; wi++)
        for (clone = 0; clone < 2; clone++) {
            int act;
            for (act = 0; act < 2; act++) {
                int knob = act ? 'A' : 'I';                     /* the active accent / inactive bar tone */
                knob_canvas(&st, ws[wi], 0, knob, clone, &g, &b);
                CHECK_INT(g, knob);                              /* normal: glyph in the knob pen */
                CHECK_INT(b, 'F');                               /* box on the frame */
                knob_canvas(&st, ws[wi], 2, knob, clone, &g, &b);
                CHECK_INT(g, knob);                              /* disabled / inactive state: still the knob pen */
                knob_canvas(&st, ws[wi], 1, knob, clone, &g, &b);
                CHECK_INT(b, knob);                              /* pressed: box fills with the knob pen */
                CHECK_INT(g, 'B');                               /* glyph in pen_back */
            }
            knob_canvas(&st, ws[wi], 0, 'F', clone, &g, &b);     /* knob pen equals the box pen: the glyph must stay visible */
            CHECK_INT(g, 'F');
            CHECK_INT(b, 'B');
        }
    /* the roles: close and depth are unchanged */
    flat_glyph_roles(FLAT_W_CLOSE, FLAT_G_SELECTED, &g, &b);
    CHECK_INT(g, FLAT_ROLE_BACK); CHECK_INT(b, FLAT_ROLE_TEXT);
    flat_glyph_roles(FLAT_W_DEPTH, FLAT_G_NORMAL, &g, &b);
    CHECK_INT(g, FLAT_ROLE_TEXT); CHECK_INT(b, FLAT_ROLE_FRAME);
    flat_glyph_roles(FLAT_W_LEFT, FLAT_G_SELECTED, &g, &b);
    CHECK_INT(g, FLAT_ROLE_BACK); CHECK_INT(b, FLAT_ROLE_KNOB);
}

/* DOPUS-SCROLLER-LOOK: DOpus draws its lister scroller arrows as LOWRES / MEDRES sysiclass images, whose glyph is fat in a small
 * box. The clone took the most common interior marker as the ground, so a glyph covering over half of the interior swapped ground
 * and glyph: accent box, dark arrow. Every arrow x size, at rest, must give the pens flat_glyph_draw gives the Workbench arrow
 * (box = frame pen, glyph = knob pen), through the real clone entry (flat_clone_draw, render sentinel). Size 0 = MEDRES, 1 =
 * LOWRES, 2 = HIRES (SYSISIZE_*): the boxes and glyph coverage are what each ROM image looks like at that size. */
typedef struct { int bw, bh, gx0, gx1, gy0, gy1; } arrow_size;
static int arrow_render(void *ctx, unsigned long ids, uint8_t *grid, int w, int h)
{
    const arrow_size *a = (const arrow_size *)ctx;
    int x, y;
    (void)ids;
    g_render_calls++;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int ring = x == 0 || y == 0 || x == w - 1 || y == h - 1;
            int m = FLAT_CLONE_MARK(0);                                 /* ground: a pen that is no BACKGROUND hole */
            if (ring)
                m = (x == 0 || y == 0) ? FLAT_CLONE_SHINE : FLAT_CLONE_SHADOW;
            else if (x >= a->gx0 && x <= a->gx1 && y >= a->gy0 && y <= a->gy1)
                m = FLAT_CLONE_MARK(2);
            else if (x == w - 2 && y == h - 2)
                m = FLAT_CLONE_MARK(1);                                 /* a stray shade pixel, so a swapped vote still maps something */
            grid[y * w + x] = (uint8_t)m;
        }
    return 1;
}

static void arrow_pens(int clone, const flat_state *st, unsigned long w, const arrow_size *a, int *glyph, int *box)
{
    int x, y, nb = 0, ng = 0, gp = -1, bp = -1;
    cv_clear();
    if (clone)      /* S69-ARROW-ASPECT: the clone refuses arrows (render never called); both columns are the glyph path */
        CHECK_INT(flat_clone_draw(st, w, FLAT_IM_DRAW, 0, 0, 0, a->bw, a->bh, 'F', 'K', -1, 99, -1, arrow_render, (void *)a, cv_fill, 0, g_scr), FLAT_CLONE_NO);
    CHECK_INT(flat_glyph_draw(st, w, FLAT_IM_DRAW, 0, 0, 0, a->bw, a->bh, 'F', 'K', -1, 1, cv_fill, 0), 1);
    for (y = 0; y < a->bh; y++)
        for (x = 0; x < a->bw; x++) {
            if (cv[y][x] == cv[0][0]) { nb++; bp = cv[y][x]; }
            else { ng++; gp = cv[y][x]; }
        }
    CHECK(nb > 0 && ng > 0);
    *glyph = gp; *box = bp;
}

static void test_clone_arrow_sizes(void)
{
    static const arrow_size sizes[3] = {
        { 12, 10, 5, 6, 2, 7 },         /* MEDRES: thin arrow */
        { 9, 9, 2, 6, 1, 7 },           /* LOWRES: fat arrow, 35 of the 49 interior pixels */
        { 16, 12, 6, 9, 3, 8 }          /* HIRES */
    };
    flat_state st;
    unsigned long w;
    int si, g0, b0, g1, b1, calls;
    glyph_state(&st);
    st.enabled = 1; st.held = 0;
    st.pen_back = 'B'; st.pen_text = 'T'; st.pen_line = 'L';
    for (w = FLAT_W_LEFT; w <= FLAT_W_DOWN; w++)
        for (si = 0; si < 3; si++) {
            calls = g_render_calls;
            arrow_pens(0, &st, w, &sizes[si], &g0, &b0);
            arrow_pens(1, &st, w, &sizes[si], &g1, &b1);
            CHECK_INT(g_render_calls, calls);                           /* S69-ARROW-ASPECT: the system image is never rendered for an arrow */
            CHECK_INT(b1, b0);                                          /* the box pen equals the Workbench arrow's */
            CHECK_INT(g1, g0);                                          /* and so does the glyph pen */
        }
    {   /* the private image is per size: LOWRES and MEDRES at one box size are two images */
        flat_clone_key k[FLAT_CLONE_OBJS];
        k[0].which = FLAT_W_UP; k[0].size = 0; k[0].w = 9; k[0].h = 9;
        CHECK_INT(flat_clone_slot(k, 1, FLAT_CLONE_OBJS, FLAT_W_UP, 0, 9, 9), 0);
        CHECK_INT(flat_clone_slot(k, 1, FLAT_CLONE_OBJS, FLAT_W_UP, 1, 9, 9), 1);
    }
}

/* S32 CLONE-PRIVATE-IMAGE: OS 3.2 sysiclass bakes its pens at OM_NEW and ignores imp_DrInfo at IM_DRAW, so drawing the window's
 * own image with the marker DrawInfo read back its baked screen pens (all 0 under flat pens: 'empty'). The clone keeps one private
 * image per (which, width, height), made with the marker DrawInfo; this is its table. The marker DrawInfo is deep enough for
 * every marker the clone reads back. */
static void test_clone_private_slot(void)
{
    flat_clone_key k[FLAT_CLONE_OBJS];
    int n = 0, i;
    CHECK_INT(flat_clone_slot(k, n, FLAT_CLONE_OBJS, FLAT_W_DEPTH, 0, 23, 11), 0);        /* empty table: append */
    k[n].which = FLAT_W_DEPTH; k[n].size = 0; k[n].w = 23; k[n].h = 11; n++;
    CHECK_INT(flat_clone_slot(k, n, FLAT_CLONE_OBJS, FLAT_W_DEPTH, 0, 23, 11), 0);        /* the same image: reused, not remade */
    CHECK_INT(flat_clone_slot(k, n, FLAT_CLONE_OBJS, FLAT_W_DEPTH, 0, 23, 10), 1);        /* another size: its own image */
    CHECK_INT(flat_clone_slot(k, n, FLAT_CLONE_OBJS, FLAT_W_CLOSE, 0, 23, 11), 1);        /* another gadget: its own image */
    for (i = n; i < FLAT_CLONE_OBJS; i++) {
        k[i].which = FLAT_W_CLOSE; k[i].w = 10 + i; k[i].h = 11;
    }
    CHECK_INT(flat_clone_slot(k, FLAT_CLONE_OBJS, FLAT_CLONE_OBJS, FLAT_W_ZOOM, 0, 23, 11), -1);   /* full: refused, the block glyph */
    CHECK_INT(flat_clone_slot(k, FLAT_CLONE_OBJS, FLAT_CLONE_OBJS, FLAT_W_DEPTH, 0, 23, 11), 0);   /* full, but a hit still answers */
    CHECK_INT((1 << FLAT_CLONE_DEPTH) > FLAT_CLONE_MARK(FLAT_CLONE_MARKS - 1), 1);
}

/* PALETTE-SAMPLE-OUTLINE (FLAT-ARROWS-CORNER): the size box in a Palette sample's corner covers the sample outline with its
 * last column and row; the saved edge cells put the outline back, the rest of the box keeps the box pen */
static void test_pal_sample_edge(void)
{
    unsigned char saved[32];
    int i, x, y, n;
    CHECK_INT(flat_edge_cell(0, 8, 9, &x, &y), 1);
    CHECK_INT(x, 7); CHECK_INT(y, 0);
    CHECK_INT(flat_edge_cell(9, 8, 9, &x, &y), 1);
    CHECK_INT(x, 0); CHECK_INT(y, 8);
    CHECK_INT(flat_edge_cell(15, 8, 9, &x, &y), 1);
    CHECK_INT(x, 6); CHECK_INT(y, 8);                              /* the corner cell is the column's, not twice */
    CHECK_INT(flat_edge_cell(16, 8, 9, &x, &y), 0);
    cv_clear();
    cv_fill(0, 10, 0, 10, 20, 'D');                                  /* the sample's right outline */
    cv_fill(0, 0, 20, 10, 20, 'D');                                  /* and its bottom outline */
    n = 8 + 9 - 1;
    for (i = 0; i < n && flat_edge_cell(i, 8, 9, &x, &y); i++)
        saved[i] = (unsigned char)cv[12 + y][3 + x];
    cv_fill(0, 3, 12, 10, 20, 'F');                                  /* the box fill, as clone_paint does */
    CHECK_INT(flat_edge_restore(saved, n, 8, 9, 3, 12, 'F', cv_fill, 0), 9 + 7);
    for (y = 0; y <= 20; y++)
        CHECK_INT(cv[y][10], 'D');                                   /* the outline column is whole again */
    for (x = 0; x <= 10; x++)
        CHECK_INT(cv[20][x], 'D');                                   /* and the bottom row */
    CHECK_INT(cv[15][5], 'F');                                       /* the box inside keeps its pen */
    cv_clear();                                                      /* no outline under the box: nothing repainted */
    for (i = 0; i < n; i++)
        saved[i] = 'F';
    CHECK_INT(flat_edge_restore(saved, n, 8, 9, 3, 12, 'F', cv_fill, 0), 0);
}

static void test_glyph_clone(void)
{
    flat_state st;
    int x, y, glyph, box;
    glyph_state(&st);
    st.enabled = 1;
    cv_clear();
    g_render_calls = g_render_fail = 0;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 2, 3, 9, 9, 'F', -1, -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    CHECK_INT(g_render_calls, 1);                                               /* the sentinel: the clone path ran */
    glyph = box = 0;
    for (y = 0; y < 9; y++)
        for (x = 0; x < 9; x++) {
            char c = cv[3 + y][2 + x];
            int in = x >= 3 && x <= 5 && y >= 3 && y <= 5;
            CHECK(c == (in ? '#' : 'F'));                                       /* ring gone, box = frame pen, glyph = text pen */
            glyph += c == '#';
            box += c == 'F';
        }
    CHECK_INT(glyph, 9);
    CHECK_INT(box, 72);
    CHECK_INT(cv[2][2], '?');                                                   /* nothing outside the box */
    /* inactive: the glyph takes the line pen, the box stays the frame pen */
    cv_clear();
    CHECK_INT(flat_clone_draw(&st, FLAT_W_ZOOM, FLAT_IM_DRAW, 5, 0, 0, 9, 9, 'F', -1, -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    CHECK_INT(cv[4][4], '+');
    CHECK_INT(cv[0][0], 'F');
    /* inactive on the band (S32-INACTIVE-BAR): box = band pen, glyph = text_pen */
    cv_clear();
    CHECK_INT(flat_clone_draw(&st, FLAT_W_ZOOM, FLAT_IM_DRAW, 5, 0, 0, 9, 9, 'B', -1, 'T', 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    CHECK(cv[0][0] == 'B' && cv[4][4] == 'T');
    /* selected: ground is FILLPEN (the dominant inside marker), ring reversed; glyph in back pen on the text-pen box */
    cv_clear();
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 1, 0, 0, 9, 9, 'F', -1, -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    CHECK_INT(cv[0][0], '#');
    CHECK_INT(cv[4][4], '.');
    CHECK_INT(cv[1][1], '#');
    /* refusals: render failure is reported for the block fallback; menu glyphs, other methods, a held switch, small boxes: not routed, no render */
    {   /* active window: box = band pen, glyph = the title text pen (text_pen overrides the normal glyph only) */
        flat_state b = st;
        cv_clear();
        CHECK_INT(flat_clone_draw(&b, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 9, 9, 'B', -1, 'T', 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
        CHECK(cv[0][0] == 'B' && cv[4][4] == 'T');
        cv_clear();
        CHECK_INT(flat_clone_draw(&b, FLAT_W_CLOSE, FLAT_IM_DRAW, 2, 0, 0, 9, 9, 'B', -1, 'T', 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
        CHECK(cv[4][4] == 'T');                                                 /* inactive takes the text pen too (S32-INACTIVE-BAR) */
        cv_clear();
    }
    g_render_calls = 0; g_render_fail = 1;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 9, 9, 'F', -1, -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_FAIL);
    g_render_fail = 0; g_render_calls = 0;
    cv_clear();                                                                 /* an empty clone is a failure and paints nothing */
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 9, 9, 'F', -1, 'T', 99, -1, blank_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_FAIL);
    CHECK_INT(g_render_calls, 1);
    CHECK(cv[0][0] == '?' && cv[4][4] == '?');
    g_render_calls = 0;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_MENUCHECK, FLAT_IM_DRAW, 0, 0, 0, 9, 9, 'F', -1, -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_NO);
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, 0x203, 0, 0, 0, 9, 9, 'F', -1, -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_NO);
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 6, 9, 'F', -1, -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_NO);
    st.held = 1;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 9, 9, 'F', -1, -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_NO);
    CHECK_INT(g_render_calls, 0);
}

/* FLAT-NO-DIVIDERS: the stock images measured on OS 3.2 (rig 55, glyph-clone-probe-run.md, real pens: the 3.2 sysiclass
 * draws in its own pens whatever DrawInfo we pass). The close image's right columns 18-19 (shadow + shine) are the title bar
 * separator, the depth image's left columns 0-1 likewise; every full-span edge line must go, the glyph must stay whole. */
static const char *const g_os32_close[] = {
    "22222222222222222222", "23333333333333333312", "23333333333333333312", "23333331111133333312",
    "23333331222133333312", "23333331222133333312", "23333331222133333312", "23333331111133333312",
    "23333333333333333312", "23333333333333333312", "21111111111111111111" };
static const char *const g_os32_depth[] = {
    "222222222222222222222222", "123333333333333333333331", "123331111111111133333331", "123331000000000133333331",
    "123331000111111111113331", "123331000122222222213331", "123331111122222222213331", "123333333122222222213331",
    "123333333111111111113331", "123333333333333333333331", "111111111111111111111111" };
/* S32-CLONE-CLIP: the private depth image as rig 23 (3.2, intuition 47.53) drew it with the MARKER pens, dumped 32 wide with
 * the draw origin at column 4; LeftEdge is -1, so image column 0 is dump column 3 (s32-evidence/clone-clip-hunt.md).
 * Marker 5 = SHADOW (right bevel at image column 23), 8 = BACKGROUND (inside the back window). */
#define CLIP_PAD 4
static const char *const g_os32_depth_marker[] = {
    "00044444444444444444444444400000", "00054666666666666666666666500000", "00054666555555555556666666500000",
    "00054666588888888856666666500000", "00054666588855555555555666500000", "00054666588854444444445666500000",
    "00054666555554444444445666500000", "00054666666654444444445666500000", "00054666666655555555555666500000",
    "00054666666666666666666666500000", "00055555555555555555555555500000" };
static int g_clip_le;
/* what clone_render reads: the image drawn at offset flat_clone_offset(le), so image column j lands at grid column off + le + j */
static int clip_render(void *ctx, unsigned long ids, uint8_t *grid, int w, int h)
{
    int x, y, j, off = flat_clone_offset(g_clip_le);
    (void)ctx; (void)ids;
    g_render_calls++;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            j = x - off - g_clip_le;                                    /* image column at grid column x */
            grid[y * w + x] = (uint8_t)(j < 0 || j >= w ? 0 : g_os32_depth_marker[y][CLIP_PAD + g_clip_le + j] - '0');
        }
    return 1;
}
static const char *const *g_grid_rows;
static int grid_render(void *ctx, unsigned long ids, uint8_t *grid, int w, int h)
{
    int x, y;
    (void)ctx; (void)ids;
    g_render_calls++;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            grid[y * w + x] = (uint8_t)(g_grid_rows[y][x] - '0');
    return 1;
}

/* S32-CLONE-CLIP: the depth clone on 3.2 (LeftEdge -1, marker pens): the glyph at its stock columns 5-19, the right bevel
 * (image column 23) never a glyph line at column 22, the back window's BACKGROUND inside a hole, not solid */
static void test_clone_clip_depth_marker(void)
{
    flat_state st;
    int x, y, glyph = 0;
    glyph_state(&st);
    st.enabled = 1;
    CHECK_INT(flat_clone_offset(-1), 1);
    CHECK_INT(flat_clone_offset(0), 0);
    cv_clear();
    g_render_calls = 0;
    g_clip_le = -1;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_DEPTH, FLAT_IM_DRAW, 0, 0, 0, 24, 11, 'B', -1, 'T', 99, -1, clip_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    CHECK_INT(g_render_calls, 1);
    for (y = 0; y < 11; y++)
        for (x = 0; x < 24; x++) {
            char m = g_os32_depth_marker[y][CLIP_PAD - 1 + x];
            int want = x >= 5 && x <= 19 && y >= 2 && y <= 8 && (m == '5' || m == '4');
            CHECK(cv[y][x] == (want ? 'T' : 'B'));
            glyph += cv[y][x] == 'T';
        }
    for (y = 0; y < 11; y++)
        CHECK(cv[y][22] == 'B' && cv[y][23] == 'B');                            /* no bevel line on the right */
    CHECK(cv[3][7] == 'B' && cv[4][8] == 'B' && cv[5][8] == 'B');             /* the back window's inside: a hole */
    CHECK_INT(glyph, 74);                                            /* = the real-pen fixture g_os32_depth */
}

/* S93-INACTIVE-SHAPE: rig 93 (OS 3.1, intuition 40.85) STATUS grid of the inactive depth gadget (IDS_INACTIVENORMAL, 24x14) drawn
 * with the marker pens: one uniform box (marker 10) and two left-corner pixels. No glyph can be mapped from it; the clone must take
 * the IDS_NORMAL render's shape and paint it in the inactive pens, never fail into the solid plan glyph. */
static const char *const g_v40_inactive_depth[14] = {
    "4aaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaa",
    "aaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaa",
    "aaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaa",
    "aaaaaaaaaaaaaaaaaaaaaaaa", "5aaaaaaaaaaaaaaaaaaaaaaa",
};
static int g_v40_ids[4], g_v40_n;
static int v40_render(void *ctx, unsigned long ids, uint8_t *grid, int w, int h)
{
    int x, y;
    if (g_v40_n < 4)
        g_v40_ids[g_v40_n] = (int)ids;
    g_v40_n++;
    if (ids == 0)
        return fake_render(ctx, ids, grid, w, h);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            char c = g_v40_inactive_depth[y][x];
            grid[y * w + x] = (uint8_t)(c >= 'a' ? c - 'a' + 10 : c - '0');
        }
    return 1;
}

static void test_clone_v40_inactive_takes_normal_shape(void)
{
    flat_state st;
    uint8_t grid[24 * 14], map[24 * 14];
    int i;
    glyph_state(&st);
    st.enabled = 1;
    g_v40_n = 0;
    v40_render(0, 5, grid, 24, 14);
    CHECK_INT(flat_clone_map(grid, 24, 14, map, 0), 0);             /* the V40 inactive render holds no glyph */
    for (i = 0; i < 3; i++) {
        unsigned long which = i == 0 ? FLAT_W_DEPTH : i == 1 ? FLAT_W_ZOOM : FLAT_W_CLOSE;
        cv_clear();
        g_v40_n = 0;
        CHECK_INT(flat_clone_draw(&st, which, FLAT_IM_DRAW, 5, 0, 0, 24, 14, 'B', -1, 'T', 99, -1, v40_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
        CHECK_INT(g_v40_n, 2);
        CHECK_INT(g_v40_ids[0], 5);
        CHECK_INT(g_v40_ids[1], 0);                                     /* the retry asks for IDS_NORMAL */
        CHECK(cv[3][3] == 'T' && cv[5][5] == 'T');                      /* the normal shape, in the inactive glyph pen */
        CHECK(cv[1][1] == 'B' && cv[10][20] == 'B' && cv[3][6] == 'B'); /* the rest is the frame, not a solid block */
    }
    /* an active (normal) draw that maps nothing still fails once, no retry */
    g_v40_n = 0;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_DEPTH, FLAT_IM_DRAW, 0, 0, 0, 9, 9, 'B', -1, 'T', 99, -1, blank_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_FAIL);
}

static void test_clone_band_rows_strip(void)
{
    flat_state st;
    int x, y, edge = 0;
    CHECK_INT(flat_clone_band_rows(0, 11, 11), 10);
    CHECK_INT(flat_clone_band_rows(2, 11, 11), 8);
    CHECK_INT(flat_clone_band_rows(0, 11, 30), 11);
    CHECK_INT(flat_clone_band_rows(12, 11, 11), 0);
    glyph_state(&st);
    st.enabled = 1;
    cv_clear();
    g_render_calls = 0;
    g_clip_le = -1;
    /* S32-TITLE-STRIP: row 10 is the title bar's bottom edge (background), not band */
    CHECK_INT(flat_clone_draw(&st, FLAT_W_DEPTH, FLAT_IM_DRAW, 0, 0, 0, 24, 11, 'B', -1, 'T', 10, 'E', clip_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    for (y = 0; y < 11; y++)
        for (x = 0; x < 24; x++) {
            if (y == 10) {
                CHECK(cv[y][x] == 'E' || cv[y][x] == 'T');
                edge += cv[y][x] == 'E';
            } else
                CHECK(cv[y][x] == 'B' || cv[y][x] == 'T');
        }
    CHECK(edge > 0);
}

static void test_clone_no_dividers(void)
{
    flat_state st;
    int x, y, glyph;
    glyph_state(&st);
    st.enabled = 1;
    /* close 20 x 11: glyph = the 5 x 5 square at x 7-11, y 3-7; every other pixel the band pen */
    cv_clear();
    g_render_calls = 0;
    g_grid_rows = g_os32_close;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 20, 11, 'B', -1, 'T', 99, -1, grid_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    CHECK_INT(g_render_calls, 1);
    glyph = 0;
    for (y = 0; y < 11; y++)
        for (x = 0; x < 20; x++) {
            int in = x >= 7 && x <= 11 && y >= 3 && y <= 7;
            CHECK(cv[y][x] == (in ? 'T' : 'B'));
            glyph += cv[y][x] == 'T';
        }
    CHECK_INT(glyph, 25);
    for (y = 1; y < 10; y++)
        CHECK(cv[y][18] == 'B' && cv[y][19] == 'B');                            /* the separator columns: band pen */
    /* depth 24 x 11: the left separator (columns 0-1) and the right edge gone, both overlapping boxes kept */
    cv_clear();
    g_grid_rows = g_os32_depth;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_DEPTH, FLAT_IM_DRAW, 0, 0, 0, 24, 11, 'B', -1, 'T', 99, -1, grid_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    glyph = 0;
    for (y = 0; y < 11; y++)
        for (x = 0; x < 24; x++) {
            int edge = x < 2 || x > 22 || y == 0 || y == 10;
            if (edge)
                CHECK(cv[y][x] == 'B');
            glyph += cv[y][x] == 'T';
        }
    CHECK_INT(glyph, 74);                                                       /* both boxes of the glyph, outline and fill */
    CHECK(cv[2][5] == 'T' && cv[5][10] == 'T' && cv[4][19] == 'T');
}

/* PREFS-NO-OUTLINES: the class of an edges-only frame from its box and the window's gadget boxes (the five boxes measured in
 * Prefs/Time and Prefs/Font), the pen each class takes, and the ground box records the content fills are matched against. */
/* PALETTE-FLAT-PREVIEW: the Palette 40.2 example windows' bevel edges and scroller dithers are told apart from its text,
 * checkmark and arrow calls by call site; an unknown Palette build stays stock */
static void test_palette_preview_sites(void)
{
    static unsigned char code[FLAT_PAL_CODE_LEN];
    const char *home = getenv("HOME");
    char path[512];
    FILE *f;
    CHECK_INT(flat_palette_site(0x54ac), FLAT_PAL_EDGE);
    CHECK_INT(flat_palette_button(0x5fb2), 1);                  /* PALETTE-PREVIEW-BUTTONS: the three sample push button bevels */
    CHECK_INT(flat_palette_button(0x6106), 1);
    CHECK_INT(flat_palette_button(0x6142), 1);
    CHECK_INT(flat_palette_button(0x5a56), 0);                  /* the sample window bevel keeps its edge */
    /* PALETTE-PREVIEW-FRAMES: inner sample-window bevels and the FILLPEN side fills go paper; the outer bevels keep the outline */
    CHECK_INT(flat_palette_button(0x5a88), 1);
    CHECK_INT(flat_palette_button(0x5d12), 1);
    CHECK_INT(flat_palette_button(0x5cde), 0);
    CHECK_INT(flat_palette_button(0x5c44), 1);                  /* PALETTE-SCROLLER-BEVEL: Active, Inactive sample scroller container */
    CHECK_INT(flat_palette_button(0x5e36), 1);
    CHECK_INT(flat_palette_button(0x5c40), 0);                  /* the bsr itself, not its return */
    CHECK_INT(flat_palette_site(0x5ace), FLAT_PAL_GROOVE);
    CHECK_INT(flat_palette_site(0x5ae0), FLAT_PAL_GROOVE);
    CHECK_INT(flat_palette_site(0x5ab8) == FLAT_PAL_GROOVE, 0);  /* the title band keeps its accent */
    CHECK_INT(flat_track_frame(FLAT_FC_TRACK, 1, 0, 5), 1);     /* SLIDER-SCROLLER-NO-FRAME: slider: no box */
    CHECK_INT(flat_track_frame(FLAT_FC_TRACK, 0, 1, 5), 1);     /* filled holder of a scroller: no box, the prop stays */
    CHECK_INT(flat_track_frame(FLAT_FC_TRACK, 0, 0, -1), 1);    /* no input ground: no box either, never the accent ring */
    CHECK_INT(flat_track_frame(FLAT_FC_TRACK, 0, 0, 5), 2);     /* edges-only track with an input ground: the ground box */
    CHECK_INT(flat_track_frame(FLAT_FC_CELL, 0, 1, 5), 0);
    CHECK_INT(flat_palette_site(0x54e4), FLAT_PAL_EDGE);
    CHECK_INT(flat_palette_site(0x5526), FLAT_PAL_EDGE);
    CHECK_INT(flat_palette_site(0x5556), FLAT_PAL_EDGE);
    CHECK_INT(flat_palette_site(0x5418), FLAT_PAL_DITHER);
    CHECK_INT(flat_palette_site(0x5bf2), FLAT_PAL_DITHER);
    CHECK_INT(flat_palette_site(0x5dfa), FLAT_PAL_DITHER);
    {   /* RESTAURANT-GLYPH-THEME-PENS: the row highlight XOR is undone on the flat Amiga key: box = text pen, key = ground */
        flat_state hs;
        memset(&hs, 0, sizeof hs);
        hs.pen_text = 1; hs.pen_back = 2; hs.pen_line = 3;
        CHECK_INT(flat_palette_site(0x5978), FLAT_PAL_HILITE);
        CHECK_INT(flat_pal_hilite_pen(&hs, 5, 2 ^ 5), 1);
        CHECK_INT(flat_pal_hilite_pen(&hs, 5, 3 ^ 5), 2);
        CHECK_INT(flat_pal_hilite_pen(&hs, 5, 4), -1);
        CHECK_INT(flat_pal_hilite_pen(&hs, 0, 2), -1);
        CHECK_INT(flat_pal_hilite_pen(0, 5, 7), -1);
    }
    CHECK_INT(flat_palette_site(0x5c88), FLAT_PAL_KNOB);       /* PALETTE-SAMPLE-SCROLLER: Active knob fill */
    CHECK_INT(flat_palette_site(0x5e68), FLAT_PAL_KNOB);       /* Inactive knob fill */
    CHECK_INT(flat_palette_site(0x34ec), FLAT_PAL_GROOVE);     /* PALETTE-GROOVE: SHADOWPEN row above the samples */
    CHECK_INT(flat_palette_site(0x353e), FLAT_PAL_GROOVE);     /* its SHINEPEN row */
    CHECK_INT(flat_palette_site(0x34e8), FLAT_PAL_STOCK);
    CHECK_INT(flat_palette_site(0x5c84), FLAT_PAL_STOCK);      /* the jsr itself, not its return */
    CHECK_INT(flat_palette_site(0x53da), FLAT_PAL_STOCK);      /* the dither box's ground fill */
    CHECK_INT(flat_palette_site(0x5abc), FLAT_PAL_STOCK);      /* a FILLPEN fill of the sample */
    CHECK_INT(flat_palette_site(0), FLAT_PAL_STOCK);
    {   /* PALETTE-SAMPLE-GLYPHS: Palette's title-bar, size, menu images get a Which record; other programs keep the narrow set */
        static flat_img_rec tab[4];
        static int objs[5];
        memset(tab, 0, sizeof tab);
        CHECK_INT(flat_img_record(tab, 4, &objs[0], FLAT_W_CLOSE, 0), 0);
        CHECK_INT(flat_img_record_pal(tab, 4, &objs[0], FLAT_W_CLOSE, 0), 1);
        CHECK_INT(flat_img_record_pal(tab, 4, &objs[1], FLAT_W_SIZE, 0), 1);
        CHECK_INT(flat_img_record_pal(tab, 4, &objs[2], FLAT_W_AMIGAKEY, 0), 1);
        CHECK_INT(flat_img_record_pal(tab, 4, &objs[3], FLAT_W_CHECKIMAGE, 0), 1);     /* the narrow set still records */
        CHECK_INT(flat_img_record_pal(tab, 4, &objs[4], 0x20, 0), 0);                  /* an unknown Which: none */
        CHECK_INT(flat_img_record_pal(tab, 4, NULL, FLAT_W_DEPTH, 0), 0);
        CHECK_INT(flat_img_which(tab, 4, &objs[0], NULL), FLAT_W_CLOSE);
        CHECK_INT(flat_img_which(tab, 4, &objs[2], NULL), FLAT_W_AMIGAKEY);
        {   /* PALETTE-AMIGAKEY-FLAT: the sample key in pen_line on pen_back; other images keep their pens */
            flat_state ps; int fr = 7, tx = 8;
            memset(&ps, 0, sizeof ps); ps.pen_back = 1; ps.pen_line = 5;
            CHECK_INT(flat_pal_glyph_pens(&ps, FLAT_W_AMIGAKEY, &fr, &tx), 1);
            CHECK_INT(fr, 1); CHECK_INT(tx, 5);
            fr = 7; tx = 8;
            CHECK_INT(flat_pal_glyph_pens(&ps, FLAT_W_MENUCHECK, &fr, &tx), 0);
            CHECK_INT(fr, 7); CHECK_INT(tx, 8);
            CHECK_INT(flat_pal_glyph_pens(NULL, FLAT_W_AMIGAKEY, &fr, &tx), 0);
        }
    }
    memset(code, 0, sizeof code);
    CHECK_INT(flat_palette_id(code, sizeof code), 0);          /* not Palette */
    CHECK_INT(flat_palette_id(NULL, sizeof code), 0);
    CHECK_INT(flat_palette_id(code, 0x1000), 0);               /* too short to hold the sites */
    snprintf(path, sizeof path, "%s/Library/CloudStorage/Dropbox/WB/Prefs/Palette", home ? home : "");
    f = fopen(path, "rb");
    if (!f) {
        printf("note: %s absent, Palette 40.2 accept case not run\n", path);
        return;
    }
    if (fseek(f, 0x24, SEEK_SET) == 0 && fread(code, 1, sizeof code, f) == sizeof code) {
        CHECK_INT(flat_palette_id(code, sizeof code), 1);      /* the 3.1 editor, hunk 0 (no relocs in the checked bytes) */
        code[0x5430] ^= 1;
        CHECK_INT(flat_palette_id(code, sizeof code), 0);      /* another build: stock */
    }
    fclose(f);
}

static void test_no_outlines(void)
{
    flat_state s;
    flat_gr gr;
    const flat_grbox *m;
    int a, t2, out[4];
    flat_gadbox time_g[3] = { { 130, 156, 186, 7, FLAT_GK_PROP }, { 12, 53, 311, 100, FLAT_GK_OTHER }, { 60, 35, 41, 8, FLAT_GK_STRING } };
    flat_gadbox font_g[2] = { { 20, 52, 100, 14, FLAT_GK_OTHER }, { 130, 52, 100, 14, FLAT_GK_OTHER } };
    memset(&s, 0, sizeof s); memset(&gr, 0, sizeof gr);
    s.enabled = 1; s.pen_back = 0; s.pen_text = 2; s.pen_line = 3; s.pen_button = 4; s.pen_sel = 5;
    CHECK_INT(flat_frame_class(126, 154, 194, 11, time_g, 3), FLAT_FC_TRACK);       /* slider frame around its prop gadget */
    CHECK_INT(flat_frame_class(56, 68, 44, 15, time_g, 3), FLAT_FC_CELL);           /* day cell inside the calendar gadget */
    CHECK_INT(flat_frame_class(12, 180, 308, 14, time_g, 3), FLAT_FC_DISPLAY);      /* date row: no gadget relation */
    CHECK_INT(flat_frame_class(14, 46, 376, 35, font_g, 2), FLAT_FC_GROUP);         /* Font Prefs group box holds gadgets */
    CHECK_INT(flat_frame_class(54, 32, 53, 14, time_g, 3), FLAT_FC_STRING);         /* the year input keeps the string path */
    CHECK_INT(flat_frame_class(10, 10, 104, 18, font_g, 0), FLAT_FC_DISPLAY);       /* no gadgets at all */
    CHECK_INT(flat_frame_class(20, 52, 100, 14, font_g, 2), FLAT_FC_BORDERED);      /* borders exactly one other gadget: today's draw */
    {   /* S74-TEXT-BOX: Input Prefs "Test Double-Click" box: a TEXT_KIND gadget (GADGHNONE, no activation) whose bevel is its own box */
        flat_gadbox in_g[2] = { { 221, 88, 155, 14, FLAT_GK_DISPLAY }, { 14, 88, 207, 14, FLAT_GK_OTHER } };
        flat_gadbox lv_g[2] = { { 386, 30, 166, 130, FLAT_GK_DISPLAY }, { 388, 32, 144, 126, FLAT_GK_OTHER } };
        CHECK_INT(flat_display_gadget(0x8007, 0x0000), 1);                          /* GADGHNONE | GADGIMAGE, no activation */
        CHECK_INT(flat_display_gadget(0x8006, 0x0101), 0);                          /* the check box: RELVERIFY | TOGGLESELECT */
        CHECK_INT(flat_display_gadget(0x8007, 0x0001), 0);
        CHECK_INT(flat_frame_class(221, 88, 155, 14, in_g, 2), FLAT_FC_DISPLAY);    /* was BORDERED: the stock outline */
        CHECK_INT(flat_frame_class(386, 30, 166, 130, lv_g, 2), FLAT_FC_BORDERED);  /* a listview border holds its list: unchanged */
        {   /* PALETTE-SWATCH-FRAME: Palette 40.2 on KS 3.1 (rig 12): the PALETTE_KIND swatch grid is custom gadget 15,20 285x20 */
            flat_gadbox pal_g[3] = { { 15, 20, 285, 20, FLAT_GK_CUSTOM }, { 315, 17, 293, 14, FLAT_GK_OTHER }, { 317, 37, 270, 108, FLAT_GK_OTHER } };
            flat_gadbox cyc_g[1] = { { 315, 17, 293, 14, FLAT_GK_OTHER } };
            CHECK_INT(flat_frame_class(15, 20, 285, 20, pal_g, 3), FLAT_FC_SELFDRAWN);    /* its frame: no box, no accent ring */
            CHECK_INT(flat_frame_class(315, 17, 293, 14, cyc_g, 1), FLAT_FC_BORDERED);    /* a bool cycle is not touched */
            CHECK_INT(flat_frame_class(317, 37, 270, 108, pal_g, 3), FLAT_FC_BORDERED);   /* the pen list box is not touched */
            CHECK_INT(flat_frame_class(10, 10, 300, 40, pal_g, 1), FLAT_FC_GROUP);        /* a group box round the palette: unchanged */
            CHECK_INT(flat_frame_class(17, 21, 36, 18, pal_g, 1), FLAT_FC_CELL);          /* a frame inside it still counts as inside */
            CHECK_INT(flat_noline_pen(&s, FLAT_FC_SELFDRAWN, 4, 0), -1);                  /* nothing to fill */
        }
    }
    {   /* S74-CHECKBOX-GROUND: CHECKIMAGE takes the ground box; the tick only in the selected states, in the given pen, inside the box */
        rect_log tl;
        int i, ok = 1;
        CHECK_INT(flat_check_image(0x0E), 1);    /* CHECKIMAGE, NDK imageclass.h */
        CHECK_INT(flat_check_image(0x07), 0);
        CHECK_INT(flat_check_width(26, 11, 1), 11);     /* S74-CHECKBOX-SQUARE: square pixels */
        CHECK_INT(flat_check_width(26, 11, 2), 22);     /* 2 columns per row */
        CHECK_INT(flat_check_width(26, 11, 0), 11);
        CHECK_INT(flat_check_width(26, 11, 4), 26);     /* never wider than the image */
        CHECK_INT(flat_check_image(FLAT_W_MENUCHECK), 0);
        {   /* PALETTE-LEFTOVERS2: no input pen on a themed private screen = outlined box, never the stock bevel */
            flat_state cs;
            int fill = -9, edge = -9;
            memset(&cs, 0, sizeof cs);
            cs.pen_back = 0; cs.pen_outline = 3; cs.pen_sel = 12;
            CHECK_INT(flat_check_look(&cs, 8, 1, &fill, &edge), FLAT_CHECK_GROUND);
            CHECK_INT(fill == 12 && edge == -1, 1);
            CHECK_INT(flat_check_look(&cs, 8, 0, &fill, &edge), FLAT_CHECK_GROUND);     /* a held pen needs no themed flag */
            cs.pen_sel = 0;                                                             /* the Palette private screen: own pens 0 */
            CHECK_INT(flat_check_look(&cs, 8, 1, &fill, &edge), FLAT_CHECK_OUTLINE);
            CHECK_INT(fill == 0 && edge == 3, 1);
            fill = edge = -9;
            CHECK_INT(flat_check_look(&cs, 8, 0, &fill, &edge), FLAT_CHECK_STOCK);      /* flat-private-screens off / excluded */
            CHECK_INT(fill == -9 && edge == -9, 1);
            cs.pen_sel = 12;
            CHECK_INT(flat_check_look(&cs, 2, 1, &fill, &edge), FLAT_CHECK_OUTLINE);    /* 4 colours: pen 12 is no pen there */
            CHECK_INT(flat_check_look(&cs, 1, 1, &fill, &edge), FLAT_CHECK_STOCK);      /* outline pen 3 outside 2 colours */
            cs.pen_outline = 0;
            CHECK_INT(flat_check_look(&cs, 2, 1, &fill, &edge), FLAT_CHECK_STOCK);      /* outline == ground: invisible */
            CHECK_INT(flat_check_look(NULL, 8, 1, &fill, &edge), FLAT_CHECK_STOCK);
        }
        CHECK_INT(flat_check_ticked(0), 0);
        CHECK_INT(flat_check_ticked(5), 0);
        CHECK_INT(flat_check_ticked(1) && flat_check_ticked(6) && flat_check_ticked(8), 1);
        memset(&tl, 0, sizeof tl);
        CHECK_INT(flat_glyph_tick(100, 50, 26, 11, 2, rect_log_fill, &tl) > 0, 1);
        for (i = 0; i < tl.n; i++)
            if (tl.r[i][4] != 2 || tl.r[i][0] < 100 || tl.r[i][2] > 125 || tl.r[i][1] < 50 || tl.r[i][3] > 60) ok = 0;
        CHECK_INT(ok, 1);
        CHECK_INT(flat_mx_image(0x0F), 1);      /* S93-MX-GROUND: MXIMAGE */
        CHECK_INT(flat_mx_image(0x0E), 0);
        memset(&tl, 0, sizeof tl);
        CHECK_INT(flat_glyph_dot(100, 50, 11, 11, 2, rect_log_fill, &tl), 1);
        CHECK_INT(tl.n == 1 && tl.r[0][0] == 102 && tl.r[0][1] == 52 && tl.r[0][2] == 108 && tl.r[0][3] == 58 && tl.r[0][4] == 2, 1);
        CHECK_INT(flat_glyph_dot(0, 0, 3, 3, 2, rect_log_fill, &tl), 0);    /* below FLAT_GLYPH_MIN_SIZE */
    }
    CHECK_INT(flat_noline_pen(&s, FLAT_FC_TRACK, 4, 0), 5);                         /* ground pen = the input ground pen */
    CHECK_INT(flat_noline_pen(&s, FLAT_FC_DISPLAY, 4, 0), 5);
    CHECK_INT(s.pen_button, 4);
    CHECK_INT(flat_noline_pen(&s, FLAT_FC_CELL, 4, 0), 5);                          /* FLAT-GREEN-FACES: the face is the held pen, never the role number 4 (stock green when unowned) */
    CHECK_INT(flat_noline_pen(&s, FLAT_FC_CELL, 4, 3), 3);                          /* the app's selected fill (accent) under the centre */
    s.pen_sel = 0;
    CHECK_INT(flat_noline_pen(&s, FLAT_FC_CELL, 4, 0), 0);                          /* nothing held: the window ground, still not pen 4 */
    s.pen_sel = 5;
    {   /* FLAT-GREEN-FACES: the frame draw state of a screen fills PLAIN and SECONDARY faces with the held pen */
        flat_state look = s, scr = s, st;
        rect_log fr;
        look.pen_sel = 0; look.pen_button = 4;
        scr.pen_sel = 12; scr.pen_dim = 13; scr.pen_knob = 14; scr.pen_inbar = 15;
        flat_frame_state(&st, &look, &scr, 2, 4);
        CHECK_INT(st.pen_button, 12);
        CHECK_INT(st.pen_sel, 12);
        CHECK_INT(st.pen_dim, 13);
        CHECK_INT(st.pen_knob, 14);
        CHECK_INT(st.pen_inbar, 15);
        CHECK_INT(st.side, 2);
        memset(&fr, 0, sizeof fr);
        CHECK_INT(flat_frame_draw(&st, 0, 0, 0, 40, 12, 1, FLAT_ROLE_SECONDARY, rect_log_fill, &fr), 1);
        CHECK_INT(fr.r[0][4], 12);                                                  /* Cancel: not the stock pen 4 */
        memset(&fr, 0, sizeof fr);
        CHECK_INT(flat_frame_draw(&st, 0, 0, 0, 40, 12, 1, FLAT_ROLE_PLAIN, rect_log_fill, &fr), 1);
        CHECK_INT(fr.r[0][4], 12);                                                  /* cycle gadget */
        scr.pen_sel = 0;
        flat_frame_state(&st, &look, &scr, 0, 4);
        CHECK_INT(st.pen_button, look.pen_back);                                    /* nothing held: the window ground */
        CHECK_INT(st.side, 1);
        scr.pen_sel = 12;
        flat_frame_state(&st, &look, &scr, 1, 2);
        CHECK_INT(st.pen_button, look.pen_line);                                    /* 4 colours: the accent line pen */
    }
    CHECK_INT(flat_noline_pen(&s, FLAT_FC_GROUP, 4, 0), -1);                        /* group: nothing is drawn */
    CHECK_INT(flat_noline_pen(&s, FLAT_FC_BORDERED, 4, 0), 5);                      /* C8-LOOK: the listview box takes the input ground, no pen_line outline */
    CHECK_INT(flat_noline_pen(&s, FLAT_FC_BORDERED, 2, 0), -1);                     /* 4 colours: no ground pen, the stock outline stays */
    s.pen_sel = 0;
    CHECK_INT(flat_noline_pen(&s, FLAT_FC_TRACK, 4, 0), -1);                        /* no input pen on the screen */
    s.pen_sel = 5;
    /* ring: record, match, miss, drop */
    flat_gr_add(&gr, &a, 1, 12, 180, 319, 193, 5);
    m = flat_gr_match(&s, &gr, &a, 14, 182, 100, 191);
    CHECK_INT(m != 0, 1);
    CHECK_INT(m ? m->pen : -1, 5);
    CHECK_INT(flat_gr_match(&s, &gr, &a, 14, 182, 100, 195) != 0, 0);          /* rect leaves the box */
    CHECK_INT(flat_gr_match(&s, &gr, &t2, 14, 182, 100, 191) != 0, 0);         /* another surface */
    s.held = 1;
    CHECK_INT(flat_gr_match(&s, &gr, &a, 14, 182, 100, 191) != 0, 0);          /* held: stock */
    s.held = 0;
    flat_gr_drop(&gr, &a, 300, 190, 330, 200);                                 /* another frame overlaps the box */
    CHECK_INT(flat_gr_match(&s, &gr, &a, 14, 182, 100, 191) != 0, 0);
    flat_gr_add(&gr, &a, 1, 12, 180, 319, 193, 5);
    flat_gr_drop(&gr, &a, 400, 400, 410, 410);                                 /* a disjoint frame keeps it */
    CHECK_INT(flat_gr_match(&s, &gr, &a, 14, 182, 100, 191) != 0, 1);
        /* PREFS-NO-OUTLINES: a record belongs to a surface region, not to a task */
    memset(&gr, 0, sizeof gr);
    flat_gr_add(&gr, &a, 1, 126, 154, 319, 164, 5);                                    /* slider track recorded by the Prefs task */
    m = flat_gr_match(&s, &gr, &a, 130, 156, 137, 162);                             /* knob erase drawn by input.device during a drag still lands on the track ground */
    CHECK_INT(m != 0, 1);
    CHECK_INT(m ? m->pen : -1, 5);
    CHECK_INT(flat_gr_match(&s, &gr, &t2, 130, 156, 137, 162) != 0, 0);             /* the same rect on another surface misses */
/* the content fill keeps the corner squares (radius 2) the frame drew */
    {
        flat_grbox b;
        b.surf = &a; b.x0 = 12; b.y0 = 180; b.x1 = 319; b.y1 = 193; b.pen = 5;
        b.ix0 = 12; b.iy0 = 180; b.ix1 = 319; b.iy1 = 193;                          /* inset box = program box (FLAT-SPACING too small) */
        CHECK_INT(flat_gr_clip(&b, 2, 12, 180, 319, 193, 0, out) && out[0] == 14 && out[1] == 180 && out[2] == 317 && out[3] == 181, 1);   /* top band without the corners */
        CHECK_INT(flat_gr_clip(&b, 2, 12, 180, 319, 193, 1, out) && out[0] == 12 && out[1] == 182 && out[2] == 319 && out[3] == 191, 1);   /* middle rows whole */
        CHECK_INT(flat_gr_clip(&b, 2, 12, 180, 319, 193, 2, out) && out[0] == 14 && out[1] == 192 && out[2] == 317 && out[3] == 193, 1);
        CHECK_INT(flat_gr_clip(&b, 2, 20, 184, 100, 190, 0, out), 0);                 /* an interior rect lies in the middle band only */
        CHECK_INT(flat_gr_clip(&b, 2, 20, 184, 100, 190, 2, out), 0);
        CHECK_INT(flat_gr_clip(&b, 2, 20, 184, 100, 190, 1, out) && out[0] == 20 && out[1] == 184 && out[2] == 100 && out[3] == 190, 1);
    }
}

/* FLAT-SPACING (owner 2026-11-11: modern spacing between elements): two filled flat elements whose program boxes touch show window
 * ground between them. Inset 1 row top and bottom, side columns left and right; neighbours show 2 visual pixels of ground. */
static void test_flat_spacing(void)
{
    struct sp_canvas { int pix[40][60]; } *cv = (struct sp_canvas *)calloc(1, sizeof *cv);
    int in[4], in2[4], box[4], o[4], x, y, k, n, bad, side, blended;
    flat_state fs;
    flat_gr gr;
    const flat_grbox *m;
    int a, t2;
    /* the inset box: side 1 and side 2 */
    CHECK_INT(flat_inset(1, 10, 10, 53, 23, in) && in[0] == 11 && in[1] == 11 && in[2] == 52 && in[3] == 22, 1);
    CHECK_INT(flat_inset(2, 10, 10, 53, 23, in) && in[0] == 12 && in[1] == 11 && in[2] == 51 && in[3] == 22, 1);
    CHECK_INT(flat_inset(0, 10, 10, 53, 23, in) && in[0] == 11 && in[2] == 52, 1);          /* side below 1 counts 1 */
    /* too small: an inset under FLAT_INSET_MIN wide or high leaves the box unchanged */
    CHECK_INT(flat_inset(1, 0, 0, 6, 6, in) && in[0] == 1 && in[3] == 5, 1);                 /* 7x7: inset 5x5, the minimum */
    CHECK_INT(flat_inset(1, 0, 0, 5, 10, in), 0);                                            /* 6 wide: inset 4 */
    CHECK_INT(in[0] == 0 && in[1] == 0 && in[2] == 5 && in[3] == 10, 1);
    CHECK_INT(flat_inset(1, 0, 0, 20, 5, in), 0);                                            /* 6 high: inset 4 */
    CHECK_INT(flat_inset(2, 0, 0, 7, 10, in), 0);                                            /* side 2 on 8 wide: inset 4 */
    CHECK_INT(flat_inset(2, 0, 0, 8, 10, in), 1);                                            /* side 2 on 9 wide: inset 5 */
    /* the four bands cover box minus inset exactly once, nothing inside the inset or outside the box */
    box[0] = 10; box[1] = 10; box[2] = 53; box[3] = 23;
    flat_inset(2, box[0], box[1], box[2], box[3], in);
    for (y = 0, bad = 0; y < 40; y++)
        for (x = 0; x < 60; x++) {
            int want = x >= box[0] && x <= box[2] && y >= box[1] && y <= box[3] && !(x >= in[0] && x <= in[2] && y >= in[1] && y <= in[3]);
            for (k = 0, n = 0; k < 4; k++)
                if (flat_inset_band(box, in, box, k, o) && x >= o[0] && x <= o[2] && y >= o[1] && y <= o[3])
                    n++;
            bad += n != want;
        }
    CHECK_INT(bad, 0);
    in2[0] = 0; in2[1] = 0; in2[2] = 11; in2[3] = 11;                                        /* a clip rect over the top-left corner */
    CHECK_INT(flat_inset_band(box, in, in2, 0, o) && o[0] == 10 && o[1] == 10 && o[2] == 11 && o[3] == 10, 1);
    CHECK_INT(flat_inset_band(box, in, in2, 2, o) && o[0] == 10 && o[1] == 11 && o[2] == 11 && o[3] == 11, 1);
    CHECK_INT(flat_inset_band(box, in, in2, 3, o), 0);
    /* two 44-wide boxes at x and x + 44 (touching): 2 * side columns of ground between their fills; rows alike, 2 rows */
    for (side = 1; side <= 2; side++) {
        flat_inset(side, 10, 10, 53, 23, in);
        flat_inset(side, 54, 10, 97, 23, in2);
        CHECK_INT(in2[0] - in[2] - 1, 2 * side);
        flat_inset(side, 10, 24, 53, 37, in2);
        CHECK_INT(in2[1] - in[3] - 1, 2);
    }
    /* through raa_frame_draw: two filled buttons whose program boxes touch, on a backdrop of pen 3, show 2 columns of pen_back (0) */
    memset(&fs, 0, sizeof fs);
    fs.enabled = 1; fs.pen_line = 1; fs.pen_back = 0; fs.pen_button = 4; fs.pen_text = 2; fs.side = 1;
    for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 3;
    CHECK_INT(raa_frame_draw(&fs, 0, 5, 5, 22, 16, 1, 0, round_fill, round_read, raa_test_rgb, 5, cv, &blended), 1);
    CHECK_INT(raa_frame_draw(&fs, 0, 27, 5, 22, 16, 1, 0, round_fill, round_read, raa_test_rgb, 5, cv, &blended), 1);
    CHECK_INT(cv->pix[12][26] == 0 && cv->pix[12][27] == 0, 1);                              /* the gap: window ground */
    CHECK_INT(cv->pix[12][25] != 0 && cv->pix[12][28] != 0 && cv->pix[12][25] != 3, 1);       /* the fills either side */
    CHECK_INT(cv->pix[5][15] == 0 && cv->pix[20][15] == 0 && cv->pix[12][5] == 0, 1);         /* the band all round */
    CHECK_INT(cv->pix[4][15] == 3 && cv->pix[21][15] == 3 && cv->pix[12][4] == 3, 1);         /* nothing outside the program box */
    for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 3;
    CHECK_INT(raa_frame_draw(&fs, 0, 5, 5, 22, 16, 0, 0, round_fill, round_read, raa_test_rgb, 5, cv, &blended), 1);
    CHECK_INT(cv->pix[5][15] != 0, 1);                                                       /* outline only (fill 0): no inset */
    fs.side = 2;                                                                             /* non-square pixels: 2 columns per side */
    for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 3;
    CHECK_INT(raa_frame_draw(&fs, 0, 5, 5, 22, 16, 1, 0, round_fill, round_read, raa_test_rgb, 5, cv, &blended), 1);
    CHECK_INT(cv->pix[12][5] == 0 && cv->pix[12][6] == 0 && cv->pix[12][7] != 0 && cv->pix[12][26] == 0 && cv->pix[12][25] == 0, 1);
    {   /* C8-RING: the cycle ring is on the program box 5..48 x 5..18 (accent 1), paper (0) inside, corners left as the backdrop (3) */
        flat_state cs = fs;
        for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 3;
        CHECK_INT(flat_cycle_frame_draw(&cs, 5, 5, 44, 14, round_fill, round_read, cv), 1);
        CHECK_INT(cv->pix[5][27] == 1 && cv->pix[18][27] == 1 && cv->pix[12][5] == 1 && cv->pix[12][6] == 1 && cv->pix[12][48] == 1, 1);
        CHECK_INT(cv->pix[12][27] == 0 && cv->pix[6][27] == 0 && cv->pix[12][7] == 0, 1);
        CHECK_INT(cv->pix[5][5] == 3 && cv->pix[4][27] == 3 && cv->pix[19][27] == 3, 1);
    }
    free(cv);
    /* ground record: the program-box erase matches, its fill is clipped to the inset box, the band part is ground */
    memset(&gr, 0, sizeof gr);
    fs.side = 0;
    flat_gr_add(&gr, &a, 1, 12, 180, 319, 193, 5);
    m = flat_gr_match(&fs, &gr, &a, 12, 180, 319, 193);                                      /* the program erases its full box */
    CHECK_INT(m != 0, 1);
    if (m) {
        CHECK_INT(m->ix0 == 13 && m->iy0 == 181 && m->ix1 == 318 && m->iy1 == 192, 1);
        CHECK_INT(flat_gr_clip(m, 2, 12, 180, 319, 193, 0, o) && o[0] == 15 && o[1] == 181 && o[2] == 316 && o[3] == 182, 1);
        CHECK_INT(flat_gr_clip(m, 2, 12, 180, 319, 193, 1, o) && o[0] == 13 && o[1] == 183 && o[2] == 318 && o[3] == 190, 1);
        CHECK_INT(flat_gr_clip(m, 2, 12, 180, 319, 193, 2, o) && o[0] == 15 && o[1] == 191 && o[2] == 316 && o[3] == 192, 1);
        box[0] = m->x0; box[1] = m->y0; box[2] = m->x1; box[3] = m->y1;
        in[0] = m->ix0; in[1] = m->iy0; in[2] = m->ix1; in[3] = m->iy1;
        CHECK_INT(flat_inset_band(box, in, box, 0, o) && o[0] == 12 && o[1] == 180 && o[2] == 319 && o[3] == 180, 1);
        o[0] = 0; o[1] = 0; o[2] = 100; o[3] = 400;                                          /* rf_gring: an erase clipped at x 100 */
        CHECK_INT(flat_gr_band(m, 3, o), 0);                                                  /* right column is past the clip */
        o[0] = 0; o[1] = 0; o[2] = 100; o[3] = 400;
        CHECK_INT(flat_gr_band(m, 2, o) && o[0] == 12 && o[1] == 181 && o[2] == 12 && o[3] == 192, 1);
    }
    CHECK_INT(flat_gr_match(&fs, &gr, &t2, 12, 180, 319, 193) != 0, 0);                      /* a fill on another surface misses */
}

void suite_flat_core_patina(void)
{
    test_arrow_glyph_pens();
    test_arrow_knob_colour();
    test_arrow_spacing();
    test_arrow_aspect();
    test_arrow_s69_shape();
    test_clone_arrows();
    test_glyph_clone();
    test_pal_sample_edge();
    test_clone_private_slot();
    test_clone_arrow_sizes();
    test_clone_no_dividers();
    test_clone_clip_depth_marker();
    test_clone_v40_inactive_takes_normal_shape();
    test_clone_band_rows_strip();
    test_scroller();
    test_slider();
    test_which_from_gtype();
    test_dopus_box_outline();
    test_dopus_no_ring();
    test_lock_glyph();
    test_band_glyphs_take_text_pen();
    test_glyph_keeps_window_ring();
    test_dopus_image_which();
    test_dopus_arrow();
    test_glyph_goldens();
    test_glyph_solid_and_state_equal();
    test_glyph_gadgets_distinct();
    test_glyph_states();
    test_glyph_frame_bg();
    test_glyph_route();
    test_round_aa();
    test_round_cut_at();
    test_knob_aspect();
    test_round_partial_mask();
    test_round_pen_byte();
    test_no_outlines();
    test_palette_preview_sites();
    test_flat_spacing();
}
