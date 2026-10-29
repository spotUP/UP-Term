/* pen_cache -- the pens menu_text holds for the colours it draws in on a palette screen, apart from AmigaOS so the
 * host tests it (tests/test_gthemes.c). A colour asked for twice shares its pen. The cache is bounded; a theme
 * change (the Style > Theme list previews a theme on every cursor move) drops every pen at once, as gui.c drops
 * its own, because the next draw asks for the new theme's colours. Without that drop the cache filled after about
 * four previewed themes, every later colour got no pen and the text fell to pen 1 on pen 0: black glyphs in light
 * boxes, and a selected label in black on the dark selection bar (Amarchy s25, A1200). */
#ifndef PEN_CACHE_H
#define PEN_CACHE_H

#include "../render/pen_rule.h"

#define PC_MAX 24

typedef struct {
    long pen[PC_MAX];
    unsigned long rgb[PC_MAX];
    int n;
} pen_cache;

/* the pen for rgb: the held one, else best() through vr_obtain_rule (exact, then nearest); -1 when none or full */
static long pc_of(pen_cache *c, vr_best_fn best, void *u, unsigned long rgb)
{
    int i;
    long p;
    for (i = 0; i < c->n; i++)
        if (c->rgb[i] == rgb)
            return c->pen[i];
    if (c->n >= PC_MAX)
        return -1;
    p = vr_obtain_rule(best, u, rgb);
    if (p < 0)
        return -1;
    c->pen[c->n] = p;
    c->rgb[c->n++] = rgb;
    return p;
}

/* the theme changed: release every held pen (release(u, pen)) and forget the colours */
static void pc_theme_changed(pen_cache *c, void (*release)(void *u, long pen), void *u)
{
    int i;
    for (i = 0; i < c->n; i++)
        release(u, c->pen[i]);
    c->n = 0;
}

#endif
