/* amarchy-flat, the portable core (plan 2026-10-29-amarchy-flat-ui-patch.md FLAT-C0; OD-F8 separate resident
 * program). The state block that the resident program and Amarchy share through the public semaphore
 * "Amarchy.Flat": a versioned MEMF_PUBLIC record. Amarchy writes the switch and the glyph pens, the program
 * counts every hook call (the sentinel that proves the hook ran) and reports whether it is still chained.
 * No OS calls: tests/test_flat_core.c pins every offset and the refusal of a block of another layout.
 *
 * Every field is a fixed-width integer at its natural alignment, so the layout is the same on the Amiga (vbcc)
 * and on the host, and a reader built from an older header can read version and size first and refuse. */
#ifndef AMARCHY_FLAT_CORE_H
#define AMARCHY_FLAT_CORE_H

#include <stddef.h>
#include <stdint.h>

#define FLAT_SEM_NAME "Amarchy.Flat"
#define FLAT_VERSION 5

typedef struct flat_state {
    uint16_t version;           /*  0 FLAT_VERSION; first, so a reader of another layout can refuse before the rest */
    uint16_t size;              /*  2 sizeof(flat_state) of the writer */
    uint16_t enabled;           /*  4 1 = glyph hook draws flat (written by Amarchy) */
    uint16_t style;             /*  6 reserved, ignored: glyphs are always solid blocks (FLAT-SOLID-FILL); the slot keeps the offsets */
    uint16_t held;              /*  8 0 = running; else the FLAT_HELD_* reason a kill switch kept it inert (FLAT-C4) */
    uint16_t still_chained;     /* 10 1 = a foreign hook sits on top of ours, so we cannot unload (FLAT-C1) */
    uint32_t calls;             /* 12 hook calls so far: the sentinel */
    uint8_t pen_text;           /* 16 glyph line pen */
    uint8_t pen_line;           /* 17 thin outline pen */
    uint8_t pen_back;           /* 18 background pen */
    uint8_t pen_outline;        /* 19 theme accent pen of the 1 px window outer edge (FLAT-OUTER-EDGE); 0 = none (pen 0 is the background, never an edge) */
    uint8_t pen_button;         /* 20 raised fill of a GadTools button, cycle and scroller arrow (FLAT-BUTTON-FILL): the theme lighter_background (DK_SELECTION); 0 or a pen the screen lacks = pen_back */
    uint8_t pen_ptext;          /* 21 label pen of a pressed button, dark on the accent fill (DK_BG) */
    uint8_t pen_dim;            /* 22 FLAT-BUTTON-DISABLED-DIM: solid fill of a disabled button, a dim tone of pen_button; obtained by amarchy-flat itself (ObtainBestPen), 0 = none */
    uint8_t pen_dimtext;        /* 23 label pen of a disabled button (DK_MUTED) */
    uint8_t pen_knob;           /* 24 KNOB-PEN: scroller knob fill, the surface moved FLAT_KNOB_PCT (60) percent toward the muted text colour; obtained by amarchy-flat itself (ObtainBestPen), 0 = none */
    uint8_t pen_inbar;          /* 25 S32-INACTIVE-BAR: bar, gadget boxes, ring, arrows and knob of an inactive window; obtained by amarchy-flat itself (ObtainBestPen) from inbar_rgb, 0 = none (the muted pen takes over) */
    uint8_t inbar_rgb[3];       /* 26 the colour of pen_inbar (R, G, B) written by Amarchy from the theme role GR_INACTIVE; all zero = derive it from the screen */
    uint8_t pen_sel;            /* 29 S33-PREFS-SKIN-FAIL: the pen holding the button fill; obtained by amarchy-flat itself (ObtainBestPen) from sel_rgb, 0 = none (pen_back takes over). pen_button is only the theme role number and is never drawn with */
    uint8_t sel_rgb[3];         /* 30 the colour of pen_sel (R, G, B) written by Amarchy from the theme role GR_SELECTION; all zero = no held pen */
    uint8_t side;               /* 33 S57-BUTTON-ASPECT-BORDERS: columns of a button's left and right outline lines (flat_side_px of the screen); 0 or 1 = 1 px. Set by amarchy-flat on its draw copy, never written by Amarchy */
    uint8_t pad[2];             /* 34 reserved, zero */
    /* TE-6.1 (version 5): the theme look and the colours the editor previews live. All written by Amarchy from the theme model
     * (desk_flat_look); 0 = the theme does not say, so amarchy-flat keeps its built-in value. A reader of version 4 refuses this
     * block by its version word (flat_check) before it reads anything else. */
    uint8_t corner_radius;      /* 36 gt_look corner_radius, px */
    uint8_t border_width;       /* 37 gt_look border_width, px */
    uint8_t knob_shape;         /* 38 gt_look knob_shape */
    uint8_t knob_min;           /* 39 gt_look knob_min, px */
    uint8_t glyph_set;          /* 40 gt_look glyph_set */
    uint8_t ghost_style;        /* 41 gt_look ghost_style */
    uint8_t hover;              /* 42 gt_look hover_on */
    uint8_t pad5;               /* 43 reserved, zero */
    uint8_t knob_rgb[3];        /* 44 the Scroll Knob colour (R, G, B) from the theme element GTE_SCROLL_KNOB; all zero = derive it */
    uint8_t pad6;               /* 47 reserved, zero (keeps the block a multiple of 4 bytes) */
} flat_state;                   /* 48 bytes */

#define FLAT_STATE_SIZE 48

/* flat_check results */
enum { FLAT_OK = 0, FLAT_ERR_VERSION = -1, FLAT_ERR_SIZE = -2 };

/* A fresh block: current version and size, everything else zero (off, not held, not chained). */
void flat_init(flat_state *s);

/* Is the block at p (avail bytes readable) a block of this layout? Reads only version and size, in that order,
 * so a block of another layout is refused before any other field is trusted. Another version is
 * FLAT_ERR_VERSION (checked first); a size field other than FLAT_STATE_SIZE, or fewer than FLAT_STATE_SIZE
 * readable bytes, is FLAT_ERR_SIZE. */
int flat_check(const void *p, size_t avail);

/* The STATUS word: "absent" (no block), "held", "on", "off". */
const char *flat_status_word(const flat_state *s, int found);

/* The whole STATUS line, "flat=<word>" plus " calls=N chained=0|1" for a valid block; a block of another
 * layout reads "flat=absent bad=version|size". Returns the length, 0 when cap < 48. */
int flat_status_line(const void *p, size_t avail, int found, char *out, size_t cap);

/* Can the program unload on Ctrl-C? 1 when nothing is chained on top of it. */
int flat_can_exit(const flat_state *s);

/* ---- FLAT-C1: the sysiclass hook swap, as a pure model -------------------------------------------------------
 * The OS part (flat.c) swaps the h_SubEntry slot of sysiclass's cl_Dispatcher (never h_Entry: one routine serves
 * five classes, research section 10). The model sees only the slot's value (an address as uintptr_t), so a fake
 * Hook in a test drives the same code the program runs. */
typedef struct flat_swap {
    uintptr_t saved;            /* the h_SubEntry we found and must hand back */
    uintptr_t ours;             /* the trampoline we put there */
    int installed;
    int inert;                  /* 1 after a refused remove: the program does nothing more to the hook */
} flat_swap;

/* flat_swap_remove results */
enum { FLAT_REMOVED = 0, FLAT_STILL_CHAINED = 1, FLAT_NOT_INSTALLED = -1 };
/* flat_swap_install results besides FLAT_OK */
enum { FLAT_ERR_DOUBLE = -3, FLAT_ERR_ARG = -4 };

void flat_swap_init(flat_swap *w);

/* Save *slot and put ours there. A second install (while installed, or a slot that already holds ours, or ours == 0
 * or ours == the saved value) is refused and changes nothing. The caller holds Forbid() around it. */
int flat_swap_install(flat_swap *w, uintptr_t *slot, uintptr_t ours);

/* Put the saved value back ONLY when *slot is still ours. Anything else on top (a foreign patcher chained on our
 * trampoline) is FLAT_STILL_CHAINED: *slot is left untouched (our saved value is never written over a foreign one),
 * st->still_chained = 1 and the swap goes inert. FLAT_REMOVED clears still_chained. st may be NULL. */
int flat_swap_remove(flat_swap *w, uintptr_t *slot, flat_state *st);

/* The trampoline: 24 bytes of 68000 code + two longs, position independent.
 *   move.l a0,-(sp) / movea.l cnt(pc),a0 / addq.l #1,(a0) / movea.l (sp)+,a0 / move.l saved(pc),-(sp) / rts
 * It bumps the call counter and tail-jumps to the saved routine with every register and the whole stack as the
 * caller left them (a0..a2 and a6 on 3.2, C stack arguments on 3.1: the same code serves both conventions, so a
 * pass-through needs no per-OS case). Big-endian bytes, as the Amiga reads them. out must hold FLAT_TRAMP_SIZE. */
#define FLAT_TRAMP_SIZE 24
void flat_tramp_build(unsigned char *out, uint32_t counter_addr, uint32_t saved_addr);

/* The calling convention of the saved routine by intuition.library version: 3.1 (40) passes hook / object / message
 * on the C stack, 3.2 (47) in a0 / a2 / a1. FLAT_CONV_STACK below 44, else FLAT_CONV_REG. C2's glyph code picks its
 * entry by this; the C1 trampoline is valid for both. */
enum { FLAT_CONV_STACK = 0, FLAT_CONV_REG = 1 };
int flat_conv(unsigned intuition_version);

/* ---- FLAT-D4: the runtime DOpus flat patch, as a pure model ---------------------------------------------------
 * Plan 2026-10-29-amarchy-flat-ui-patch.md FLAT-D4 (OD-F9 / OD-F10), research section 14. Only the dopus5.library
 * entries that DOpus calls through the jump table can be patched this way: DrawBox (LVO -696, Library/gui.c:27: toolbar
 * buttons, column header boxes, file-list border) and DrawFieldBox (LVO -702, gui.c:60). The patched slot is the long
 * operand of the entry's JMP abs.l, swapped with the C1 model (flat_swap): same restore-if-ours, same still_chained.
 * A build is patched only when its version, revision and the checksum of the first FLAT_FUNC_BYTES bytes of each
 * routine match the table; any other build is inert and STATUS says why. */
enum { FLAT_EL_DRAWBOX = 0, FLAT_EL_FIELDBOX = 1, FLAT_EL_COUNT = 2 };
#define FLAT_LVO_TEXT (-60)         /* graphics.library Text (the pressed-button label patch, FLAT-BUTTON-FILL) */
#define FLAT_LVO_RECTFILL (-306)    /* graphics.library RectFill (the outer-edge patch) */
#define FLAT_LVO_DRAWBOX (-696)
#define FLAT_LVO_FIELDBOX (-702)
#define FLAT_FUNC_BYTES 64

typedef struct flat_dopus_build {
    uint16_t version, revision;             /* lib_Version / lib_Revision of dopus5.library */
    uint32_t sum[FLAT_EL_COUNT];            /* flat_sum of the routine's first FLAT_FUNC_BYTES; 0 = not measured yet: never matches */
} flat_dopus_build;

/* gate reasons */
enum { FLAT_GATE_OK = 0, FLAT_GATE_UNKNOWN = 1, FLAT_GATE_UNMEASURED = 2, FLAT_GATE_SUM = 3 };

/* The built-in table (DOpus 5.104 = library 73.8, sums still to be measured on the rig) and its length. */
const flat_dopus_build *flat_dopus_table(int *n);

/* Sum of the big-endian 32-bit words of len bytes (len a multiple of 4; a trailing remainder is ignored). Never 0:
 * a computed 0 reads as 1, so 0 stays free for "not measured". */
uint32_t flat_sum(const void *p, size_t len);

/* The address a JMP abs.l entry (4E F9 + long) jumps to; 0 when the entry is anything else (already a foreign patch,
 * an unexpected build). */
/* flat_sum over opcode words only: absolute-long operands (relocations) are skipped, so the value is the same at every load address. */
uint32_t flat_sum_reloc(const void *p, size_t len);
uint32_t flat_jmp_target(const unsigned char *entry);

/* Which elements may be patched for this library. Bit (1 << FLAT_EL_*) per element; *reason says why a bit is missing. */
int flat_dopus_gate(const flat_dopus_build *tab, int n, unsigned version, unsigned revision,
                    const uint32_t sums[FLAT_EL_COUNT], int *reason);

/* The flat-draw decision for one call of a patched element. FLAT_DRAW_SKIP: draw nothing (under flat pens the shine
 * half equals the background and the shadow half would remain as a right/bottom L; owner 2026-10-30: draw flat, no
 * shadow L). FLAT_DRAW_STOCK: call the saved routine. Skip only when the switch is on, not held, and the screen pens
 * really are flat; otherwise stock pens keep stock bevels. Flat means SHINEPEN == BACKGROUNDPEN (the flat signal of
 * OD-F4) or, B10 (a) owner 2026-11-07, two different pens that carry the same colour: DOpus clones a screen whose
 * pen numbers differ while the theme gives both one colour. rgb_shine/rgb_back are 0xRRGGBB, or -1 when the colour
 * map is unknown; then only the pen numbers decide. */
enum { FLAT_DRAW_STOCK = 0, FLAT_DRAW_SKIP = 1 };
int flat_dopus_decide(const flat_state *s, int element, int recessed, int pen_shine, int pen_back,
                      long rgb_shine, long rgb_back);

/* FLAT-C8: frameiclass (every GadTools bevel and ridge: buttons, strings, checkboxes, cycles, listviews, DrawBevelBox).
 * The same decision for one method call of the class: SKIP only for IM_DRAW (0x202) and IM_DRAWFRAME (0x206), when the
 * switch is on, not held, and the screen pens are flat (shine == background). Any other method (OM_GET, IM_ERASE,
 * IM_HITFRAME, ...) is STOCK. A missing state is STOCK. */
#define FLAT_IM_DRAWFRAME 0x206UL
int flat_frame_decide(const flat_state *s, unsigned long method, int pen_shine, int pen_back);
/* STATUS line of the frameiclass hook: "frame=absent" or "frame=hooked calls=N flat=N" (flat = draws skipped). */
int flat_frame_status_line(int installed, unsigned long calls, unsigned long flat, char *out, size_t cap);

/* STATUS extra line for the DOpus patch: "dopus=absent", "dopus=73.8 inert unknown|unmeasured|sum" or
 * "dopus=73.8 patched=<mask> calls=N flat=N". Appends " sums=<hex>,<hex>" (the measured routine sums, NULL = none) when found. Returns the length, 0 when cap < 96. */
int flat_dopus_status_line(int found, unsigned version, unsigned revision, int reason, int mask, int installed,
                           unsigned long calls, unsigned long flat, const uint32_t sums[FLAT_EL_COUNT], char *out, size_t cap);

/* SYSIA_Which of a system gadget from its GadgetType & GTYP_SYSTYPEMASK (0xF0): WDEPTH 0x40 -> 0, WZOOM 0x60 -> 1,
 * SIZING 0x10 -> 2, CLOSE 0x80 -> 3. -1 for every other type. Used where GetAttr(SYSIA_Which) fails (Kickstart 3.1). */
int flat_which_from_gtype(unsigned gadget_type);

/* FLAT-C7: the SYSIA_Which of a system gadget (window or screen: GTYP_SYSGADGET set) when one of its two render
 * pointers is `image`, else -1. Screen gadgets (Screen->FirstGadget, type 0xc055 for the depth gadget) have no window,
 * so flat.c walks the screen's list with this too. */
int flat_gadget_which(unsigned gadget_type, const void *gadget_render, const void *select_render, const void *image);

/* SYSIA_Which values that draw the glyph of another one (SDEPTHIMAGE 5 -> DEPTHIMAGE 0). */
unsigned long flat_which_alias(unsigned long which);

/* ---- FLAT-C4: kill switches ------------------------------------------------------------------------------------
 * Plan 2026-10-29-amarchy-flat-ui-patch.md FLAT-C4 (OD-F5, OD-F7). The program starts INERT (no hook swapped, held
 * = the reason, STATUS flat=held why=...) when any switch is set. Priority, first match wins:
 *   1 crash marker ENVARC:Amarchy/flat-starting present (written before the swap, deleted FLAT_MARKER_SECS later:
 *     a start that crashed the machine leaves it, so the next boot starts inert),
 *   2 both mouse buttons held at boot (OD-F7),
 *   3 VisualPrefs or Birdie running, by task name, or a foreign hook already on the sysiclass slot (OD-F5; P1 pens
 *     still apply because they are not part of this program). */
#define FLAT_MARKER_PATH "ENVARC:Amarchy/flat-starting"
#define FLAT_MARKER_HEAD "flat-starting "       /* + the start count and a newline: "flat-starting 1\n" */
#define FLAT_MARKER_STRIKES 2                   /* this many early deaths in a row hold the next start inert */
#define FLAT_MARKER_SECS 60

enum { FLAT_HELD_NONE = 0, FLAT_HELD_MARKER = 1, FLAT_HELD_GESTURE = 2, FLAT_HELD_VISUALPREFS = 3,
       FLAT_HELD_BIRDIE = 4, FLAT_HELD_HOOK = 5 };

/* Which foreign patcher a task name belongs to: FLAT_HELD_VISUALPREFS, FLAT_HELD_BIRDIE or FLAT_HELD_NONE.
 * Case-insensitive prefix match ("VisualPrefs", "VisualPrefs Handler", "Birdie", "BirdieD"). */
int flat_other_task(const char *name);

/* A hook entry address already on the sysiclass slot: outside the intuition.library ROM span [lo, hi) is
 * FLAT_HELD_HOOK, inside is FLAT_HELD_NONE. */
int flat_other_hook(unsigned long entry, unsigned long lo, unsigned long hi);

/* The marker's start count from its file text (buf, len bytes, not NUL terminated): "flat-starting N\n" -> N, capped at
 * FLAT_MARKER_STRIKES. Anything else (empty, garbled, N < 1) counts 1: a damaged marker is one strike, not two. */
int flat_marker_parse(const char *buf, unsigned len);

/* The marker text for count (1..FLAT_MARKER_STRIKES) into out (>= 24 bytes); returns its length. */
unsigned flat_marker_format(int count, char *out);

/* The start decision. marker: the start count in the marker file (0 = no file, 1 = one earlier start died before
 * FLAT_MARKER_SECS, FLAT_MARKER_STRIKES = two in a row: held). lmb / rmb: that button is down. other: a FLAT_HELD_* from
 * flat_other_task / flat_other_hook (a task name wins over the hook; the caller passes the first nonzero).
 * Returns FLAT_HELD_NONE (start) or the reason held. */
int flat_start_decide(int marker, int lmb, int rmb, int other);

/* hook_install found no sysiclass: the Workbench screen is not open yet (ipd_lock_wb_screen never opens it). */
enum { FLAT_ERR_NOCLASS = -100 };

/* S32-START-LOOK: amarchy-flat BOOT runs from S:User-Startup, before LoadWB opens the Workbench screen, so the first
 * hook_install finds no class. 1: retry the install now (not held, not installed, the last try failed only for want
 * of the class); 0: an installed hook, a kill switch or any other failure (a foreign swap, memory) is final. */
int flat_install_due(int held, int installed, int last_rc);

/* Seconds the marker still has to live: FLAT_MARKER_SECS minus the age (now and written are seconds on any clock,
 * unsigned arithmetic so a wrapped clock still works), 0 when expired. */
unsigned flat_marker_remaining(unsigned long written, unsigned long now);

/* The held reason as one word ("marker", "gesture", "VisualPrefs", "Birdie", "hook"; "" for NONE). */
const char *flat_held_name(unsigned reason);

/* The prefs status text (shows in Amarchy's settings page): flat_status_word, plus for a held block the cause
 * by name, "held by VisualPrefs" / "held by crash marker" / "held by both mouse buttons" / "held by another
 * hook on the gadget class". Returns the length, 0 when cap is too small (cap >= 40 needed). */
int flat_status_text(const flat_state *s, int found, char *out, size_t cap);

/* ---- FLAT-BUTTON-FILL: the pressed-label arm, as a pure model ------------------------------------------------
 * A pressed filled frame arms {rastport, task, box}; the graphics Text patch recolours the label pen only for a Text
 * call on that rastport, from that task, whose current pen position lies inside the box. Any other frame draw on the
 * rastport, and any Text call on it outside the box, disarms. */
typedef struct flat_tx {
    const void *surf, *task;    /* surf: flat_tx_surface of the frame's rastport */
    int armed, x0, y0, x1, y1;
} flat_tx;

/* The drawing surface a rastport draws on: its layer, else its bitmap. Not the RastPort pointer: on 3.2 intuition draws
 * the pressed frame through one RastPort and the label Text through another RastPort of the same window layer
 * (rig 8 diag 2026-10-31: arms=1, Text on another rastport of the same layer=1, hits=0). */
const void *flat_tx_surface(const void *layer, const void *bitmap);

/* A frame was drawn on surface surf by task: disarm when surf is the armed one; with pressed && fill, arm for box
 * (x0,y0)-(x1,y1). */
/* S56-PRIMARY-ACCENT-FILL: does a frame draw arm the Text patch for its label (pen_ptext, the background colour)? A filled button that is
 * pressed or primary does (accent fill); a filled secondary button keeps its stock label. Pass the result as flat_tx_frame's "pressed". */
int flat_tx_arm(int pressed, int fill, int role);
void flat_tx_frame(flat_tx *t, const void *surf, const void *task, int pressed, int fill, int x0, int y0, int x1, int y1);

/* Does a Text call at pen position (cx, cy) on surface surf from task get the pressed-label pen? 1 yes; a call on the
 * armed surface outside the box disarms (0). Another surface or task: 0, state untouched. */
int flat_tx_text(flat_tx *t, const void *surf, const void *task, int cx, int cy);

/* The pen a patched Text call draws with: pen_ptext when flat is on and not held and flat_tx_text matches, else -1 (stock
 * pen). pen_ptext is DK_BG = pen 0, a real pen: 0 never means "unset" here. */
int flat_tx_pen(const flat_state *s, flat_tx *t, const void *surf, const void *task, int cx, int cy);

/* ---- FLAT-TITLE-ACCENT: a window title drawn by Intuition (one Text call on the border rastport) ---------------------
 * flat_title_decide: 1 and *t filled when the call is a window title under flat (enabled, not held, title string prefix
 * match of str/count against the Window title, pen y inside bar_h = BorderTop). Active: band (0,0)-(w-1,bar_h-1) in
 * pen_outline (band_pen -1 = none: pen_outline unset or not a pen of the depth) and the title text in pen_ptext. Inactive (S32-INACTIVE-BAR): the same
 * with flat_inactive_pen as the band, text pen_ptext on it, flat_dimtext_pen when no inactive pen exists.
 * bg_pen = band_pen when a band is drawn (the title Text call is JAM2: its cell box repaints BgPen, so BgPen must be the band pen
 * for that call or the band is erased behind the glyphs), else -1 (BgPen untouched). Inactive: no band, title text in flat_dimtext_pen (dark_foreground). 0: not a title, stock Text untouched. */
typedef struct flat_title { int band_pen, bx0, by0, bx1, by1, text_pen, bg_pen; } flat_title;
/* flat_frame_pen: the pen of an active window's title band (pen_outline) when flat is on, not held, and pen_outline is a pen of the
 * depth; else -1 (caller uses its stock frame pen). flat_title_decide and the window gadget boxes both take the band from here. */
int flat_frame_pen(const flat_state *s, int depth, int active);
/* S32-INACTIVE-BAR: the pen of everything an inactive window shows in the bar tone: pen_inbar when it is a pen of the depth (16+ colours, 4..8 planes),
 * else the muted text pen (flat_dimtext_ok), else -1 (no inactive look). flat_frame_pen(inactive), the ring, the arrows and the knob take it. */
int flat_inactive_pen(const flat_state *s, int depth);
/* The colour of pen_inbar from the screen's surface and text colours (0xRRGGBB): gt_mix(surface, text, GT_INACTIVE_PCT), the default of GR_INACTIVE. */
unsigned long flat_inbar_colour(unsigned long surface, unsigned long text);
/* FLAT-OUTER-EDGE: the pen of the window's 1 px ring: pen_outline when active (or when there is no inactive pen), else flat_inactive_pen. */
int flat_edge_pen(const flat_state *s, int depth, int active);
/* flat_side_fill_pen: S57-LOCALE-LIST-NO-SELECT. The flat DrawInfo map now keeps FILLPEN = the accent so ListBrowser and GadTools
 * listviews show a selected row. Intuition also fills an active window's left, right and bottom border strips with FILLPEN, and the
 * owner rule FLAT-TITLE-ACCENT keeps those sides dark. For a fill in pen fg == pen_outline that lies wholly in the left, right or
 * bottom border strip (window ww x wh, borders bl/bt/br/bb) and reaches below the title bar, returns pen_back and sets out to the
 * rows of it below the title bar (a full-height strip keeps its title rows in the accent); else -1 (draw as asked). */
int flat_side_fill_pen(const flat_state *s, int fg, int ww, int wh, int bl, int bt, int br, int bb, int x0, int y0, int x1, int y1,
                       int out[4]);
/* FLAT-GADGETS-RIG8: the band spans only the title area between the system gadgets (bx0..bx1, from flat_title_span), so it never
 * wipes the close / depth / zoom gadgets Intuition draws once and does not redraw. */
typedef struct flat_gadrect { int x, y, w, h, relright, relwidth, drag; } flat_gadrect;
/* flat_title_span: *x0 / *x1 = the title area of a bar_h high, win_w wide title bar. Gadgets in the title rows (y < bar_h,
 * y + h > 0) other than the drag bar are resolved against win_w (relright adds win_w to x, relwidth to w); one centred in the
 * left half pushes x0 past its right edge, one in the right half pulls x1 before its left edge. No gadgets: 0 .. win_w-1. */
void flat_title_span(const flat_gadrect *g, int n, int win_w, int bar_h, int *x0, int *x1);
int flat_title_decide(const flat_state *s, int depth, int active, const char *title, int win_w, int bar_h,
                      const char *str, int count, int cp_y, int bx0, int bx1, flat_title *t);

/* ---- FLAT-BUTTON-DISABLED-DIM: the ghost fill of a disabled gadget, as a pure model ---------------------------------
 * Intuition draws a disabled gadget (frame, label), then ghosts it with one pattern RectFill (rp->AreaPtrn set, AreaPtSz != 0)
 * over the gadget box, called from Intuition's ROM. The frame hook records every filled primary button box per surface and task, the Text
 * patch records the label drawn inside it. A pattern fill that lands inside a recorded box is replaced by a solid fill in the
 * dim pen and the label is redrawn in the dim label pen; any other fill is untouched. */
#define FLAT_GH_N 8
typedef struct flat_gbox {
    const void *surf, *task;
    int x0, y0, x1, y1;
    const char *str;            /* the label: pointer, length and pen position of its Text call; str 0 = none seen */
    int count, lx, ly;
} flat_gbox;
typedef struct flat_gh {
    flat_gbox box[FLAT_GH_N];
    int next;
} flat_gh;
/* A filled frame was drawn: pressed or unfilled frames drop a record for the box, a filled
 * primary non-pressed frame records it (a ring of FLAT_GH_N, the same box replaces its record). */
void flat_gh_frame(flat_gh *g, const void *surf, const void *task, int pressed, int fill, int x0, int y0, int x1, int y1);
/* A Text call at pen position (cx, cy): stored as the label of the record whose box holds the position. */
void flat_gh_text(flat_gh *g, const void *surf, const void *task, int cx, int cy, const char *str, int count);
/* The dim fill pen of a button on a screen of this depth: pen_dim when it is a pen of the depth (depth >= 3 and 0 < pen_dim < 1 << depth),
 * else (a screen under 8 colours cannot hold a third tone, or no dim pen) the plain button pen flat_button_pen. */
int flat_dim_pen(const flat_state *s, int depth);
int flat_dimtext_pen(const flat_state *s, int depth);
/* KNOB-PEN: the scroller knob mix. The surface colour moved FLAT_KNOB_PCT percent toward the muted text colour (0xRRGGBB, per channel):
 * vantablack (000000, 7A7A7A) gives 494949 (about 4A4A4A: gt_scale truncates). Shared by amarchy-flat (screen colours) and the host tests (theme colours). */
#define FLAT_KNOB_PCT 60
unsigned long flat_knob_colour(unsigned long surface, unsigned long muted);
/* The scroller knob pen: pen_knob when it is a pen of the depth and the screen has 16+ colours (depth 4..8: a free pen exists),
 * else pen_dimtext (DK_MUTED; flat_button_pen under 8 colours or when pen_dimtext is 0 or not a pen of the depth). */
int flat_knob_pen(const flat_state *s, int depth);
/* A fill (x0,y0)-(x1,y1) on surface surf by task with area pattern size ptsz: the record it ghosts, or 0 (not enabled, held,
 * no pattern, no record of that surface and task containing the rect). */
const flat_gbox *flat_gh_match(const flat_state *s, const flat_gh *g, const void *surf, const void *task, int ptsz, int x0, int y0, int x1, int y1);

/* S32-GHOST-MENU: Intuition's menu ghost on 3.2 (ROM RectFill, AreaPtrn {0x4444, 0x1111}, AreaPtSz 1, JAM1, FgPen = the menu fill pen)
 * is drawn OVER the finished item text, into the off-screen menu bitmap (rp->Layer NULL) for items and the separator bar, and into
 * the screen bar layer (Layer->Window NULL) for a disabled title. Instead of the dither every non-background pixel of the rect
 * (label, shortcut, checkmark, separator line) takes the dim label pen and background pixels stay: no item lookup is needed.
 * flat_gm_pen: the pen to recolour with, or -1 for the stock dither (not enabled, held, not a ROM caller, a Window layer, another
 * pattern or size, the dim pen unusable on this depth (flat_dimtext_ok: 8..256 colours) or equal to the fill pen bg). */
#define FLAT_GM_PAT0 0x4444u
#define FLAT_GM_PAT1 0x1111u
int flat_gm_pen(const flat_state *s, int depth, int rom, int window, int ptsz, unsigned pat0, unsigned pat1, int bg);
/* The recolour core over planar data (standard or interleaved: plane p row y at planes[p] + y * bpr): in (x0,y0)-(x1,y1), clipped
 * to width by rows (bpr is the row modulo, which spans every plane in an interleaved bitmap), every pixel whose pen is not bg becomes pen dim. Refused (-1, nothing written) when the rect is empty or
 * fewer than half its pixels are bg (not a menu fill: leave it to the stock call); else the count of pixels recoloured. */
long flat_gm_recolour(uint8_t *const *planes, int depth, int bpr, int width, int rows, int x0, int y0, int x1, int y1, int bg, int dim);

/* FLAT-BUTTON-FILL: the size of a frame draw. Only IM_DRAWFRAME carries Dimensions (a 0 side falls back to the image's);
 * IM_DRAW has no such field, its message words past the state are junk (3.1: 4096 x -17232). */
void flat_frame_size(int draw_frame, int dim_w, int dim_h, int img_w, int img_h, int *w, int *h);

/* FLAT-BUTTON-FILL, 3.1 (frameiclass does not answer OM_GET): is this window gadget a button or cycle drawn by image? GadTools
 * BUTTON and CYCLE have GadgetRender == SelectRender == the frame; STRING, LISTVIEW, SLIDER have no select image. */
int flat_button_gadget(const void *gadget_render, const void *select_render, const void *image);

/* The PRIMARY button fill pen for a screen of the given depth: under 8 colours (depth < 3) the accent pen_line; from 8 colours the
 * theme lighter background pen_button, or pen_back when it is 0 or not a pen of that depth. */
int flat_button_pen(const flat_state *s, int depth);

/* BUTTON-ROLES: a button box in gadget coordinates. */
typedef struct { int x, y, w, h; } flat_box;
enum { FLAT_ROLE_PRIMARY = 0, FLAT_ROLE_SECONDARY = 1, FLAT_ROLE_PLAIN = 2 };   /* PLAIN: a filled frame that is no push button (scroller knob, cycle gadget): filled with pen_button, stock label, never the accent */
/* S56-CYCLE-PLAIN / S56-PUSH-ROLE: is this boolean frame gadget (render == select) a push button? 1 = push button.
 * Measured on 3.2 GadTools (rig 88, s56-evidence/push-role-fix.md): a BUTTON_KIND gadget has GadgetText NULL (GadTools draws
 * the PLACETEXT_IN label itself) and Activation RELVERIFY (0001); a labelled CYCLE_KIND has its label IntuiText outside the box
 * (Sound Type: le=-96 on a 180x14 box); a scroller arrow has no text and Activation RELVERIFY|IMMEDIATE (0003). So: IMMEDIATE =
 * not a push button; no text or a label inside the box = push button; a label outside = cycle. An unlabelled cycle is not told
 * apart (no field differs) and stays a push button. lx, ly: IntuiText offsets; activation: Gadget.Activation. */
/* OWNER-2026-11-08 FLAT-BUTTON-NO-RING: does a flat frame draw fill its box? answered: frameiclass answered OM_GET IA_EdgesOnly with edges_only;
 * push: the frame is the render+select image of a push button gadget of the window. A frame that is not edges-only fills. An edges-only frame
 * fills only for a push button (the Installer draws its buttons edges-only: the ring was all they got); string fields, listviews and bevel
 * boxes stay outlines. Not answered (3.1): fill when it is a gadget image of the window (push = that test). */
int flat_frame_fill(int answered, int edges_only, int push);
int flat_push_button(int has_text, int lx, int ly, int w, int h, int activation);
/* Amiga style guide: in a row of buttons the rightmost is secondary (Cancel, a solid pen_button fill: no ring, OWNER-2026-11-08), every other button
 * primary (filled); a lone button is primary. A row = siblings whose vertical span overlaps this box. sib lists every button of
 * the window, the box itself included (skipped by equal box). */
int flat_button_role(const flat_box *self, const flat_box *sib, int n);

/* FLAT-SCROLLER: a window scroller (prop gadget). The pen for one fill inside the gadget's container (cx,cy,cw,ch in the
 * rastport's coordinates): a fill inside the knob rectangle takes flat_knob_pen (its own pen_knob from 16 colours, else the theme muted text pen); the
 * rest of the container is track and takes the frame pen (pen_back: the flat map sets FILLPEN and BACKGROUNDPEN to it). The knob
 * is laid out as Intuition does: size = body * container / 0xFFFF (at least 1 px), position = pot * (container - size) / 0xFFFF.
 * Returns -1 when flat is off or held, or the fill is not inside the container (the caller leaves the call alone).
 * horiz / vert: the PropInfo FREEHORIZ / FREEVERT flags; depth: the screen depth. */
int flat_scroller_pen(const flat_state *s, int depth, int active, int cx, int cy, int cw, int ch, int horiz, int vert,
                      unsigned hpot, unsigned vpot, unsigned hbody, unsigned vbody, int x0, int y0, int x1, int y1);
/* S32-KNOB-STATE: the scroller knob pen by window state: active = pen_outline (accent) when it is a pen of the depth, inactive = flat_inactive_pen (pen_inbar);
 * either one missing falls back to flat_knob_pen (pen_knob, else the muted text pen). Never -1. */
int flat_knob_state_pen(const flat_state *s, int depth, int active);
/* S32-KNOB-NONE-FRESH: is this window gadget a prop gadget whose SpecialInfo is a PropInfo? gadgettype = Gadget.GadgetType & GTYP_GTYPEMASK.
 * Plain GTYP_PROPGADGET (0x0003) with SpecialInfo: yes. GTYP_CUSTOMGADGET (0x0005, a boopsi object such as XCON's propgclass gadget) with
 * SpecialInfo: yes only when is_propgclass (the object's class is propgclass or a subclass); a strgclass SpecialInfo is a StringInfo. */
int flat_gadget_is_prop(unsigned gadgettype, int is_propgclass, int has_specialinfo);
/* S56-XCON-KNOB-BOX: flat_scroller_pen with the knob rectangle given (kx,ky,kw,kh, absolute) instead of computed from pots and bodies. */
int flat_scroller_pen_box(const flat_state *s, int depth, int active, int cx, int cy, int cw, int ch, int kx, int ky, int kw, int kh,
                          int x0, int y0, int x1, int y1);
/* S56-XCON-KNOB-BOX: the knob rectangle Intuition itself draws. An AUTOKNOB prop gadget's GadgetRender Image holds the knob box
 * Intuition laid out (ile,ite relative to the container, iw x ih). 3.2 sizes it differently from body * container / 0xFFFF
 * (XCON scrollback: body 0x5af2 of 195 rows is a 70 row knob at row 125, the formula says 69 at row 126), so every knob fill
 * straddled the computed rectangle and took the track pen. img_ok = the gadget has an AUTOKNOB Image; a box that is empty or
 * leaves the container falls back to flat_scroller_knob. 0 when the container is empty. */
int flat_scroller_knob_img(int cx, int cy, int cw, int ch, int img_ok, int ile, int ite, int iw, int ih, int horiz, int vert,
                           unsigned hpot, unsigned vpot, unsigned hbody, unsigned vbody, int *kx, int *ky, int *kw, int *kh);
/* The knob rectangle (the layout flat_scroller_pen uses) inside container (cx,cy,cw,ch); 0 when the container is empty. */
int flat_scroller_knob(int cx, int cy, int cw, int ch, int horiz, int vert, unsigned hpot, unsigned vpot, unsigned hbody, unsigned vbody,
                       int *kx, int *ky, int *kw, int *kh);


/* ---- S32-STALE-HELD-PENS: the three shared pens amarchy-flat obtains itself (dim, knob, inbar) ----
 * A pen number belongs to the ColorMap it was obtained on. After a Workbench reopen the held numbers name pens of the OLD screen:
 * on the new one other clients own them (wrong colours in the hook), and ReleasePen on one never obtained there corrupts the
 * PalExtra free list (iconpen_core.h ip_holder_verify). So the pens remember the screen AND its epoch (ipd_epoch, which also
 * changes when a reopen lands at the same address) and are released only on that identity; on any other screen they are
 * forgotten without ReleasePen. */
#define FLAT_DIM_PCT 60
typedef struct flat_pens {
    const void *screen;             /* the screen the pens were obtained on, 0 = none held */
    unsigned epoch;                 /* its ipd_epoch */
    int dim, knob, inbar, sel;      /* pen numbers, -1 = none */
} flat_pens;
typedef struct flat_pen_ops {
    void *ctx;
    int (*obtain)(void *ctx, unsigned long rgb);        /* ObtainBestPen on the screen being synced; -1 = none */
    void (*release)(void *ctx, int pen);                /* ReleasePen on that screen */
    unsigned long (*rgb)(void *ctx, int pen);           /* GetRGB32 of a pen, 0xRRGGBB */
} flat_pen_ops;
void flat_pens_init(flat_pens *p);
/* Release (on the identity they were obtained on) and re-obtain the three pens on `screen` at `depth`; stores the pen numbers in
 * s->pen_dim/pen_knob/pen_inbar. Called with no screen it only forgets. */
void flat_pens_refresh(flat_pens *p, const flat_pen_ops *o, flat_state *s, const void *screen, unsigned epoch, int depth);
/* Let go of the pens: ReleasePen only when `screen`/`epoch` is the identity they were obtained on, else forget them. */
void flat_pens_release(flat_pens *p, const flat_pen_ops *o, flat_state *s, const void *screen, unsigned epoch);
/* The 1 s Workbench watch step, after the icon pens synced: `want` 0 (held, kill switch) releases/forgets; the screen is gone
 * (0) or another identity forgets without ReleasePen; a new screen with `want` obtains the three pens on it. Returns 1 when it
 * obtained. */
int flat_pens_follow(flat_pens *p, const flat_pen_ops *o, flat_state *s, int want, const void *screen, unsigned epoch, int depth);
/* S57-ICL-PINK-SHADE: call before flat_pens_follow. Same screen and epoch is not proof that the pens are still ours: a Workbench
 * reopen at the same address leaves both equal (the epoch moves only in ipd pens_sync, and not at all when ipd holds no pens). Asks
 * the ColorMap (ip_any_free over the held pens); if any is free the pens are forgotten WITHOUT ReleasePen and 1 is returned, so the
 * follow step re-obtains. Else 0. */
int flat_pens_verify(flat_pens *p, flat_state *s, int (*is_free)(void *ctx, int pen), void *ctx, const void *screen, unsigned epoch);

/* TE-10.6a: pens per screen slot. Colours per screen, look global (D10-7): `s` is the one look block (pen roles, rgb overrides), the
 * pen numbers a refresh obtains go to `store` when it is not 0 (the Workbench slot passes the flat_state the draw hook reads) and
 * are only kept in the flat_pens otherwise. flat_pens_release and flat_pens_verify take s == 0 as "store nothing"; flat_pens_refresh
 * and flat_pens_follow above are the _on form with store == s. */
void flat_pens_refresh_on(flat_pens *p, const flat_pen_ops *o, const flat_state *s, flat_state *store, const void *screen, unsigned epoch,
                          int depth);
int flat_pens_follow_on(flat_pens *p, const flat_pen_ops *o, const flat_state *s, flat_state *store, int want, const void *screen,
                        unsigned epoch, int depth);

/* The ONE lookup rule of every per-screen table (flat_pens_tab, icl_pentab): in n records of `stride` bytes whose screen pointer
 * sits at scr_off and whose epoch (unsigned) at epoch_off, the index of the record holding (screen, epoch); else, with
 * allow_free, the first record that holds no screen; else -1 (also for screen 0). It never releases or forgets anything. */
int flat_ident_slot(const void *base, size_t stride, size_t scr_off, size_t epoch_off, int n, const void *screen, unsigned epoch,
                    int allow_free);

/* Equal to IP_SCREENS (iconpen_core.h, the pen holder table of Amarchy main); tests/test_flat_core.c checks it. */
#define FLAT_PENS_SLOTS 40
typedef struct flat_pens_tab {
    flat_pens slot[FLAT_PENS_SLOTS];
} flat_pens_tab;
void flat_pens_tab_init(flat_pens_tab *t);
/* The slot to obtain / refresh on: the one holding (screen, epoch), else a free one; 0 when the table is full or screen is 0. */
flat_pens *flat_pens_tab_for(flat_pens_tab *t, const void *screen, unsigned epoch);
/* What the draw hook reads: the slot holding exactly that identity, else 0. */
const flat_pens *flat_pens_tab_find(const flat_pens_tab *t, const void *screen, unsigned epoch);
/* Forget (WITHOUT release: the ColorMap went with the screen) every slot whose identity is_open says is gone; returns the count. */
int flat_pens_tab_reap(flat_pens_tab *t, int (*is_open)(void *ctx, const void *screen, unsigned epoch), void *ctx);

/* TE-10.6b: the state a draw hook works with for a window on `screen` (epoch = that screen's slot epoch): *out is the global look `look`
 * with the pen numbers (dim, knob, inbar, sel) of the slot holding exactly (screen, epoch); a screen with no slot or other epoch has
 * no pens (all four 0, the "none" the pen functions already fall back from). Returns 1 when a slot was found. Never obtains. */
int flat_state_for(const flat_state *look, const flat_pens_tab *t, const void *screen, unsigned epoch, flat_state *out);

/* TE-10.6b budget fix: the draw table. The main task of amarchy-flat keeps it; a draw hook (any task, on its own small stack) only looks
 * up a pointer into it: no flat_state copy on the hook's stack and no iconpen call (FLAT-C3, tests/test_flat_budget.py). */
typedef struct flat_draw_slot {
    const void *screen;   /* 0 = free */
    unsigned epoch;       /* the pen record epoch of the screen at the last sync */
    flat_state st;        /* the look with the four own pens of this screen (0 when it holds none) */
} flat_draw_slot;

typedef struct flat_draw_tab {
    const void *wb;       /* the Workbench screen at the last sync, 0 = unknown */
    unsigned wb_epoch;
    flat_state plain;     /* the look with the four own pens 0: a screen the table does not list */
    flat_draw_slot slot[FLAT_PENS_SLOTS];
} flat_draw_tab;

typedef struct flat_draw_scr { const void *screen; unsigned epoch; } flat_draw_scr;

void flat_draw_init(flat_draw_tab *d);
/* Main task, hooks excluded by the caller (Forbid): plain = look with the four pens 0; one slot per listed screen (not the Workbench),
 * st = flat_state_for(look, pens, screen, epoch). A screen already in the table keeps its slot (a hook may hold the pointer); a
 * screen not listed frees its slot. */
void flat_draw_sync(flat_draw_tab *d, const flat_state *look, const flat_pens_tab *pens, const flat_draw_scr *list, int n, const void *wb,
                    unsigned wb_epoch);
/* Hook: screen 0 or the Workbench screen = live (the shared state, no lag on the Workbench); a listed screen = its slot state; any other
 * screen = plain. Pointer compares only, no copy. */
const flat_state *flat_draw_state(const flat_draw_tab *d, const flat_state *live, const void *screen);
/* Hook, label path: the mirrored epoch (Workbench = wb_epoch, a listed screen = its epoch, any other = 0). */
unsigned flat_draw_epoch(const flat_draw_tab *d, const void *screen);

/* S57-BUTTON-ASPECT-BORDERS: the width in pixels of a button's left and right outline lines: side (flat_side_px; below 1 counts 1),
 * never more than a third of the box width w. Top and bottom lines stay 1 px. */
int flat_button_side(int side, int w);

/* FLAT-ASPECT-BORDERS: columns per row of visual height on a mode whose DisplayInfo Resolution is rx x ry ticks per pixel
 * (also the width in pixels of a vertical line that looks as thick as a 1 px horizontal one). Non-square modes with taller
 * pixels (PAL/NTSC hires non-interlace 22 x 44, super hires 11 x 44) give ry / rx rounded (2, 4). Square modes, wide-pixel
 * modes, RTG and an unknown mode (rx or ry 0) give 1. Shared by the flat patch and the Amarchy GUI (welcome checkbox). */
int flat_side_px(int rx, int ry);

/* FLAT-ASPECT-SLOTS: the side-line column count per screen. A small table filled on the main task (which can look at every
 * screen); the draw hook only reads it (flat_aspect_find), so no DisplayInfo call or frame lands on the draw task's stack. */
#define FLAT_ASPECT_SLOTS 8
typedef struct flat_aspect_slot {
    const void *scr;            /* the screen (compared, never dereferenced); NULL = free */
    unsigned long mode;         /* its mode ID when px was measured */
    int px;                     /* flat_side_px of that mode's Resolution */
} flat_aspect_slot;
/* px of scr, 1 when scr is NULL or has no slot. */
int flat_aspect_find(const flat_aspect_slot *t, int n, const void *scr);
/* 1 when scr has no slot yet or its slot was measured for another mode: the caller then measures and calls flat_aspect_put. */
int flat_aspect_need(const flat_aspect_slot *t, int n, const void *scr, unsigned long mode);
/* store px for scr: its own slot, else a free one, else the last (evicted). */
void flat_aspect_put(flat_aspect_slot *t, int n, const void *scr, unsigned long mode, int px);

#endif
