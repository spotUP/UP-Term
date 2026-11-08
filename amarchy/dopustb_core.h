/* dopustb_core: the Directory Opus flat toolbar's per-screen size set and its blended brush colours (plan
 * 2026-11-08-dopus-toolbar-size-blend.md DT-2, DT-3). Pure: no Amiga calls. */
#ifndef DOPUSTB_CORE_H
#define DOPUSTB_CORE_H
#include <stddef.h>
#include <stdint.h>
#include "gthemes_core.h"

#define DT_PENS 8          /* brush depth 3: ground, glyph 33/66/100 percent, accent 33/66/100 percent, ground */

typedef struct {
    unsigned size;         /* brush width in pixels: 16, 24 or 32 */
    int tall;              /* 1 = tall-pixel set, half the height (a 2:1 pixel aspect) */
    int outline;           /* DT-9: 1 = the Lucide outline family (icon-style = outline), 0 = the Phosphor filled family, the default */
} dt_set;

/* The public screen Directory Opus runs on, from the reply of the ARexx command `dopus screen` ("<name> <w> <h> <depth> ...", the name
 * of a public screen has no blanks): the first word is copied to out (NUL ended). Returns 1 with the name; 0 for an empty or too long
 * reply, out is then "" and the caller uses the default public screen. */
int dt_screen_name(const char *reply, char *out, unsigned cap);

/* DT-3: the set for a DOpus screen whose font is font_h pixels high (Screen->Font->ta_YSize; 0 = unknown, gives 24) and whose
 * pixels are rx : ry (DisplayInfo Resolution via menu_screen_aspect; 0 = unknown, square). The icon height is about 2 x the font
 * height, snapped to the nearest of 16/24/32 (a tie goes to the smaller). Tall when li_pick_variant says the pixel is twice as
 * high as wide. DT-9: outline is the family (amconf_icon_outline), stored as given. */
void dt_pick_set(unsigned font_h, int rx, int ry, int outline, dt_set *out);

/* The set's name as gen_dopus_toolbar.py set_name writes it ("filled-24", "outline-24t": family, then size and "t" when tall), NUL
 * ended; returns its length, 0 when cap is too small (DT_SET_NAME_MAX bytes always fit). */
#define DT_SET_NAME_MAX 12
unsigned dt_set_name(const dt_set *s, char *out, unsigned cap);

/* DT-2: the CMAP of every brush: ip_colours (iconpen_core.c, the Workbench icons' blend) on the lister's ground (GR_SURFACE);
 * pen 0 and 7 ground, 1-3 foreground 33/66/100, 4-6 accent 33/66/100. */
void dt_cmap(const gtheme *t, uint8_t rgb[DT_PENS][3]);

/* Writes rgb over the CMAP chunk of the IFF ILBM in buf (DT_PENS * 3 bytes). Returns 1 when the CMAP chunk is there with that
 * size and its bytes now equal rgb; 0 when buf is no ILBM, has no CMAP, or the CMAP has another size (buf is then unchanged);
 * 2 when it was already equal (nothing changed). */
int dt_cmap_patch(uint8_t *buf, size_t len, const uint8_t rgb[DT_PENS][3]);
#endif
