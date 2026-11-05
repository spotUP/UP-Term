/* themeprev_core -- TE-5 the Theme Editor's preview pane, pure part (plan 2026-11-05-amarchy-theme-editor.md, TE-5).
 * themeprev_layout places the boxes of a mock desktop (an active and an inactive window with title bar, gadgets in the chosen
 * order, scroll bar and knob; a menu strip with a selected and a disabled item; a requester button; a text field with a cursor;
 * one icon with its label) from the look of the model being edited. themeprev_rgb gives a box its colour from gt_resolve, so
 * a field the reset has not reached yet (bar height, radius) shows before any reset. amarchy/gui.c theme_preview_view draws the
 * boxes; no AmigaOS calls here (tests/test_themeprev.c). Coordinates are relative to the pane's top left. */
#ifndef AMARCHY_THEMEPREV_CORE_H
#define AMARCHY_THEMEPREV_CORE_H

#include "gthemes_core.h"

enum {
    TPV_DESKTOP,
    TPV_ACT_FRAME, TPV_ACT_BAR, TPV_ACT_TITLE, TPV_ACT_BODY, TPV_ACT_GADGET0, TPV_ACT_GADGET1, TPV_ACT_GADGET2,
    TPV_ACT_SCROLL, TPV_ACT_KNOB,
    TPV_INA_FRAME, TPV_INA_BAR, TPV_INA_TITLE, TPV_INA_BODY, TPV_INA_GADGET0, TPV_INA_GADGET1, TPV_INA_GADGET2,
    TPV_MENU, TPV_MENU_HEADER, TPV_MENU_SEL, TPV_MENU_DISABLED,
    TPV_BUTTON, TPV_FIELD, TPV_CURSOR, TPV_ICON, TPV_ICON_LABEL,
    TPV_COUNT
};

/* Stock values a look field of 0 stands for (the Workbench look the reset restores). */
#define TPV_STOCK_BAR 12
#define TPV_STOCK_BORDER 2
#define TPV_STOCK_SCROLL 10
#define TPV_MARGIN 6
#define TPV_MIN_W 150      /* a pane narrower than this, or shorter than TPV_MIN_H, shows the boxes that fit */
#define TPV_MIN_H 130
#define TPV_WIDTH 180      /* the width the editor asks for beside the rows */

typedef struct tpv_item {
    int x, y, w, h;        /* w == 0 or h == 0: does not fit the pane, not drawn */
    unsigned char elem;    /* GTE_* the colour comes from */
    unsigned char state;   /* GT_ST_* */
    unsigned char radius;  /* the look's corner radius when this box is rounded, else 0 */
    unsigned char text;    /* 1: the box is a text drawn in the colour, not a fill */
    const char *label;     /* text of a text box */
} tpv_item;

typedef struct themeprev {
    int w, h;
    tpv_item it[TPV_COUNT];
} themeprev;

/* The boxes in a w x h pane for `look`. The gadget order (0 left, 1 right) puts the three gadgets at the bar's left or right end. */
void themeprev_layout(const gt_look *look, int w, int h, themeprev *out);
/* The colour of a box: the model's resolved element colour for its state (0xRRGGBB). */
uc_u32 themeprev_rgb(const gt_model *m, const tpv_item *it);

enum { TPV_BESIDE, TPV_TOGGLE };
/* D4: beside the rows when the screen holds the rows and the pane, else a full view on a key. */
int themeprev_placement(int screen_w, int rows_w);

#endif
