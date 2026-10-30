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

#endif
