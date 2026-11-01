/* iconpen_core: see iconpen_core.h. */
#include <stdio.h>
#include <string.h>
#include "iconpen_core.h"

static uint8_t mix(unsigned from, unsigned to, int pct)
{
    return (uint8_t)((from * (unsigned)(100 - pct) + to * (unsigned)pct + 50) / 100);
}

static void blend(uint8_t out[3], uc_u32 from, uc_u32 to, int pct)
{
    out[0] = mix((from >> 16) & 255, (to >> 16) & 255, pct);
    out[1] = mix((from >> 8) & 255, (to >> 8) & 255, pct);
    out[2] = mix(from & 255, to & 255, pct);
}

void ip_colours(const gtheme *t, uint8_t rgb[IP_PENS][3])
{
    uc_u32 bg = desk_role(t, DK_BG), fg = desk_role(t, DK_FG), ac = desk_role(t, DK_ACCENT);
    uc_u32 mu = desk_role(t, DK_MUTED), sel = desk_role(t, DK_SELECTION);
    int i;
    for (i = 0; i < IP_PENS; i++)
        blend(rgb[i], bg, bg, 0);
    blend(rgb[IP_FG33], bg, fg, 33);
    blend(rgb[IP_FG66], bg, fg, 66);
    blend(rgb[IP_FG100], bg, fg, 100);
    blend(rgb[IP_ACCENT33], bg, ac, 33);
    blend(rgb[IP_ACCENT66], bg, ac, 66);
    blend(rgb[IP_ACCENT100], bg, ac, 100);
    blend(rgb[IP_MUTED33], bg, mu, 33);
    blend(rgb[IP_MUTED66], bg, mu, 66);
    blend(rgb[IP_MUTED100], bg, mu, 100);
    blend(rgb[IP_SELFG33], sel, fg, 33);
    blend(rgb[IP_SELFG66], sel, fg, 66);
    blend(rgb[IP_SELFG100], sel, fg, 100);
    blend(rgb[IP_SELBG], sel, sel, 0);
}

static unsigned long dist(const uint8_t c[3], uc_u32 role)
{
    long dr = (long)c[0] - (long)((role >> 16) & 255);
    long dg = (long)c[1] - (long)((role >> 8) & 255);
    long db = (long)c[2] - (long)(role & 255);
    return (unsigned long)(dr * dr + dg * dg + db * db);
}

int ip_fold(const gtheme *t, int slot, int depth)
{
    uint8_t rgb[IP_PENS][3];
    int pen, best = 0, last;
    unsigned long bd = 0, d;
    if (slot < 0 || slot >= IP_PENS)
        return 0;
    if (depth >= 6)
        return IP_FIRST + slot;
    if (depth >= 4 && slot >= 8)
        return slot;
    ip_colours(t, rgb);
    last = depth <= 2 ? DK_ACCENT : DK_MUTED;
    for (pen = 0; pen <= last; pen++) {
        d = dist(rgb[slot], desk_role(t, pen));
        if (pen == 0 || d < bd) {
            bd = d;
            best = pen;
        }
    }
    return best;
}

void ip_holder_init(ip_holder *h)
{
    memset(h, 0, sizeof *h);
}

int ip_holder_release(ip_holder *h, const ip_ops *o)
{
    int i, n = 0;
    for (i = 0; i < IP_PENS; i++)
        if (h->have[i]) {
            o->release(o->ctx, h->first + i);
            h->have[i] = 0;
            n++;
        }
    h->held = 0;
    h->screen = NULL;            /* nothing is obtained on any screen now: the next sync obtains again */
    h->depth = 0;
    return n;
}

int ip_holder_obtain(ip_holder *h, const ip_ops *o, const void *screen, int depth, const uint8_t rgb[IP_PENS][3])
{
    desk_pen_plan p;
    int i;
    ip_holder_release(h, o);
    desk_pen_plan_for(depth, &p);
    h->screen = screen;
    h->depth = depth;
    h->slot0 = p.icon_slot;
    h->first = p.icon_first;
    h->count = p.icon_count;
    for (i = 0; i < p.icon_count; i++) {
        const uint8_t *c = rgb[p.icon_slot + i];
        if (o->obtain(o->ctx, p.icon_first + i, c[0], c[1], c[2]) == p.icon_first + i) {
            h->have[i] = 1;
            h->held++;
        }
    }
    return h->held;
}

int ip_holder_stale(const ip_holder *h, const void *screen, int depth)
{
    return h->screen != screen || h->depth != depth;
}

int ip_status_line(const ip_holder *h, char *out, size_t cap)
{
    int n = snprintf(out, cap, "pens-held %d", h->held), i;
    if (n <= 0 || (size_t)n >= cap)
        return 0;
    if (h->screen && h->held < h->count) {       /* name the pens another user holds */
        int w = snprintf(out + n, cap - (size_t)n, " missing");
        if (w <= 0 || (size_t)(n + w) >= cap)
            return 0;
        n += w;
        for (i = 0; i < h->count; i++)
            if (!h->have[i]) {
                w = snprintf(out + n, cap - (size_t)n, " %d", h->first + i);
                if (w <= 0 || (size_t)(n + w) >= cap)
                    return 0;
                n += w;
            }
    }
    return n;
}

int ip_sync_decide(const ip_holder *h, int enabled, const void *screen, int depth)
{
    if (!enabled || !screen)
        return h->screen ? IP_SYNC_RELEASE : IP_SYNC_NONE;
    return ip_holder_stale(h, screen, depth) ? IP_SYNC_OBTAIN : IP_SYNC_NONE;
}

unsigned long ip_scale32(unsigned c)
{
    return (unsigned long)(c & 0xff) * 0x01010101UL;
}

int ip_screen_depth(int planes, int ehb)
{
    return (ehb && planes == 6) ? 5 : planes;
}
