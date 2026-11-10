/* The public record of amarchy-flat ("Amarchy.Flat" semaphore), shared by the resident program (flat.c) and
 * whoever reads it (UP-Term Prefs: the status row of the Amarchy page). Only st is a stable, versioned layout
 * (flat_core.h); the rest is the program's own. */
#ifndef AMARCHY_FLAT_SHARED_H
#define AMARCHY_FLAT_SHARED_H

#include <exec/semaphores.h>
#include <exec/tasks.h>
#include <exec/libraries.h>
#include <stdint.h>
#include "flat_core.h"
#include "iconpen_core.h"

/* The public record. ss is first so AddSemaphore's node is the allocation's start (as ixshared, ixwatch.h). */
typedef struct flatshared {
    struct SignalSemaphore ss;
    char name[16];
    struct Task *task;          /* the resident program; only compared, never dereferenced alone */
    char tname[32];
    flat_state st;
    flat_swap sw;
    uintptr_t *slot;            /* &sysiclass->cl_Dispatcher.h_SubEntry while installed */
    /* FLAT-C8: the frameiclass hook (second slot) and its sentinels */
    flat_swap fsw;
    uintptr_t *fslot;
    ULONG fcalls, fflat;        /* every frameiclass dispatch / every IM_DRAW(FRAME) that drew flat */
    /* FLAT-D4 */
    struct Library *dlib;       /* dopus5.library, opened while patched so it cannot expunge under the patch */
    flat_swap dsw[FLAT_EL_COUNT];
    uintptr_t *dslot[FLAT_EL_COUNT];
    /* 0c body hook: the body entry hooked per element (0 = the LVO jump-table swap above), the bytes it held, and the
     * trampoline (relocated prologue + JMP back into the body) the replacement calls when it draws nothing */
    unsigned char *dbody[FLAT_EL_COUNT];
    unsigned char dorig[FLAT_EL_COUNT][FLAT_PRO_MAX];
    int dplen[FLAT_EL_COUNT];
    uint16_t dtramp[FLAT_EL_COUNT][FLAT_BODY_TRAMP_MAX / 2];
    unsigned dver, drev;
    int dreason, dmask, dfound;
    uint32_t dsum[FLAT_EL_COUNT]; /* flat_sum of each routine at the last scan (printed by STATUS) */
    ULONG dcalls, dflat;        /* every patched call / every call that drew flat (the sentinels) */
    ULONG dbevel;               /* s66: calls that entered the internal bevel body (FLAT_EL_BEVEL), part of dcalls */
    /* FLAT-OUTER-EDGE: the graphics.library RectFill patch (LVO -306) that paints Intuition's outer border edges in pen_outline */
    flat_swap rsw;
    uintptr_t *rslot;
    uintptr_t irom_lo, irom_hi; /* Intuition's ROM code range (FindResident rt_MatchTag .. rt_EndSkip): the caller gate */
    int knob_side;              /* S69-KNOB-ASPECT: side_px of the scroller fill being painted (set by knob_thin, read by the cap rule; keeps the drawing frames small) */
    int sring[16][9];           /* RL6 diagnostic: last 16 scroller fills: fill x0 y0 x1 y1, knob x y w h, cuts made */
    ULONG sring_n;              /* scroller fills logged so far (ring index = n % 16) */
    ULONG rcalls, rhits, shits; /* every patched RectFill call / every outer-edge fill recoloured / every scroller fill recoloured (the sentinels) */
    /* FLAT-BUTTON-FILL: the graphics.library Text patch (LVO -60) that draws a pressed button's label in pen_ptext */
    flat_swap tsw;
    uintptr_t *tslot;
    flat_tx tx;                 /* the armed pressed frame (flat_core.h) */
    flat_gh gh;                 /* FLAT-BUTTON-DISABLED-DIM: the recorded filled button boxes and labels (flat_core.h) */
    ULONG ghits;                /* ghost fills replaced by the solid dim fill (the sentinel) */
    ULONG ttitle;               /* window titles redrawn by FLAT-TITLE-ACCENT (the sentinel) */
    ULONG tcalls, thits;
    ULONG labels, lskip;        /* RL21: icon labels antialiased / label calls drawn stock (semaphore busy, nothing to blend) */        /* every patched Text call / every pressed label recoloured (the sentinels) */
    ULONG cmin, chw, cstk, cnostk;  /* S63 clone stack: least free stack of the drawing task at c2_clone, private stack high-water bytes, its size, clones refused for want of it */
    ULONG cb256, cb8a, cb8b, chdr, cguard, cdeep;  /* S63 stack probe: fill bytes left in the bottom 256, the first 8 stack bytes, AllocVec size longword, guard bytes intact (of 16), high-water ignoring the bottom 256 */
    char ctask[16];             /* name of the task at cmin */
    ULONG cdraws, cfall;        /* FLAT-GLYPH-OS-SHAPES sentinels: gadgets drawn by cloning the system image / clone attempts that fell back to the block glyph */
    ULONG cf_busy, cf_nodri, cf_nobm, cf_empty;    /* cfall split by cause: cl_ss held by another task / no DrawInfo / no offscreen bitmap / image drew no pixel */
    ULONG cf_mask;              /* bit (which & 31) set for every image that fell back (SYSIA_Which after flat_which_alias) */
    ULONG cf_ids;               /* bit (imp_State & 31) set for every empty clone */
    ULONG cf_lwhich, cf_lsize, cf_lnz, cf_lmarks;   /* the last empty clone: which, w << 8 | h, non-zero grid pixels, bit per mark value seen */
    /* FLAT-ASPECT-BORDERS: the cached side-line width for one screen (flat.c side_px) */
    flat_aspect_slot aspect_slot[FLAT_ASPECT_SLOTS];
    int aspect_px;              /* the Workbench screen's count (the status line) */
    ULONG aspect_looks;         /* DisplayInfo lookups done (the sentinel: one per screen and mode) */
    ULONG mhits;                /* S32-GHOST-MENU: menu ghost fills recoloured to the dim label pen (the sentinel) */
    ULONG bhits;                /* INSTALLER-BORDER-BUTTON: Border-render button erases drawn as the rounded flat fill (the sentinel) */
    ULONG dface;                /* DOPUS-BUTTON-NO-RING sentinel: raised Program DrawBox calls whose ring was painted in the face pen */
    ULONG dnopix;               /* DOPUS-BUTTON-NO-RING: face reads that failed (ReadPixel < 0), border fell back to the background pen */
    /* DIAG s71 agent 27 (temporary): info-frame shaped d4 box draws (height 11..15, width > 150) */
    ULONG dg_n, dg_acc;         /* shaped calls; all d4 draws whose final pen_line equals the state's accent line pen */
    LONG dga[8];                /* the last accent draw: el, rec, x0, y0, x1, y1, ring, fp */
    LONG dg_el, dg_rec, dg_x0, dg_y0, dg_x1, dg_y1, dg_fp, dg_pl, dg_side, dg_rgb, dg_ring; /* the last shaped call */
    flat_swap d5sw;             /* DOPUS-LOCK-ICONIFY: the dopusiclass h_Entry swap (lock / iconify images) */
    uintptr_t *d5slot;
    ULONG d5draws;              /* sentinel: lock / iconify images drawn by flat */
    /* FLAT-LOOP-BEAT: the main loop trace (flat_loop_line): passes after Wait, the FLAT_STG_* step it is in, the last FLAT_REF_* refusal */
    ULONG beat;
    UWORD stage, refuse;
    ULONG nrefuse;
    /* the icon pens are no longer here: the shared holder record IPD_SEM_NAME owns them (iconpen_dos.h, F14) */
} flatshared;

#endif
