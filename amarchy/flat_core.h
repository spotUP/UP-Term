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
#define FLAT_VERSION 1

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
    uint8_t reserved;           /* 19 zero */
} flat_state;                   /* 20 bytes */

#define FLAT_STATE_SIZE 20

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
 * really are flat (SHINEPEN == BACKGROUNDPEN, the flat signal of OD-F4); otherwise stock pens keep stock bevels. */
enum { FLAT_DRAW_STOCK = 0, FLAT_DRAW_SKIP = 1 };
int flat_dopus_decide(const flat_state *s, int element, int recessed, int pen_shine, int pen_back);

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
#define FLAT_MARKER_HEAD "flat-starting 1\n"
#define FLAT_MARKER_SECS 60

enum { FLAT_HELD_NONE = 0, FLAT_HELD_MARKER = 1, FLAT_HELD_GESTURE = 2, FLAT_HELD_VISUALPREFS = 3,
       FLAT_HELD_BIRDIE = 4, FLAT_HELD_HOOK = 5 };

/* Which foreign patcher a task name belongs to: FLAT_HELD_VISUALPREFS, FLAT_HELD_BIRDIE or FLAT_HELD_NONE.
 * Case-insensitive prefix match ("VisualPrefs", "VisualPrefs Handler", "Birdie", "BirdieD"). */
int flat_other_task(const char *name);

/* A hook entry address already on the sysiclass slot: outside the intuition.library ROM span [lo, hi) is
 * FLAT_HELD_HOOK, inside is FLAT_HELD_NONE. */
int flat_other_hook(unsigned long entry, unsigned long lo, unsigned long hi);

/* The start decision. marker: the marker file exists. lmb / rmb: that button is down. other: a FLAT_HELD_* from
 * flat_other_task / flat_other_hook (a task name wins over the hook; the caller passes the first nonzero).
 * Returns FLAT_HELD_NONE (start) or the reason held. */
int flat_start_decide(int marker, int lmb, int rmb, int other);

/* Seconds the marker still has to live: FLAT_MARKER_SECS minus the age (now and written are seconds on any clock,
 * unsigned arithmetic so a wrapped clock still works), 0 when expired. */
unsigned flat_marker_remaining(unsigned long written, unsigned long now);

/* The held reason as one word ("marker", "gesture", "VisualPrefs", "Birdie", "hook"; "" for NONE). */
const char *flat_held_name(unsigned reason);

/* The prefs status text (shows in Amarchy's settings page): flat_status_word, plus for a held block the cause
 * by name, "held by VisualPrefs" / "held by crash marker" / "held by both mouse buttons" / "held by another
 * hook on the gadget class". Returns the length, 0 when cap is too small (cap >= 40 needed). */
int flat_status_text(const flat_state *s, int found, char *out, size_t cap);

#endif
