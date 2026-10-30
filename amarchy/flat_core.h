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

/* style (OD-F4: a thin line glyphs in the text pen; b filled blocks) */
enum { FLAT_STYLE_LINE = 0, FLAT_STYLE_BLOCK = 1 };

typedef struct flat_state {
    uint16_t version;           /*  0 FLAT_VERSION; first, so a reader of another layout can refuse before the rest */
    uint16_t size;              /*  2 sizeof(flat_state) of the writer */
    uint16_t enabled;           /*  4 1 = glyph hook draws flat (written by Amarchy) */
    uint16_t style;             /*  6 FLAT_STYLE_* */
    uint16_t held;              /*  8 1 = kept inert by a kill switch (crash marker, boot gesture, other patcher) */
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

#endif
