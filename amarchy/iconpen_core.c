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

uc_u32 ip_ground(const gtheme *t, int flat, unsigned mask)
{
    (void)mask; /* the tone does not depend on who writes the tile */
    return desk_pen_colour(t, desk_backdrop_pen(), flat);
}

void ip_colours(const gtheme *t, uc_u32 ground, uint8_t rgb[IP_PENS][3])
{
    /* TE-2.2: Icon Foreground, Icon Accent, Icon Muted and Icon Selection Plate override their slot colour when the model gives them a ref */
    uc_u32 bg = ground, fg = gt_elem_rgb(t, GTE_ICON_FOREGROUND, desk_role(t, DK_FG));
    uc_u32 ac = gt_elem_rgb(t, GTE_ICON_ACCENT, desk_role(t, DK_ACCENT));
    uc_u32 mu = gt_elem_rgb(t, GTE_ICON_MUTED, desk_role(t, DK_MUTED)), sel = gt_elem_rgb(t, GTE_ICON_SELECTION_PLATE, desk_role(t, DK_SELECTION));
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

int ip_fold(const gtheme *t, int slot, int depth)
{
    uint8_t rgb[IP_PENS][3];
    if (slot < 0 || slot >= IP_PENS)
        return 0;
    if (depth >= 6)
        return IP_FIRST + slot;
    if (depth >= 4 && slot >= 8)
        return slot;
    ip_colours(t, desk_role(t, DK_BG), rgb);   /* the fold picks among the role pens, which are drawn on the panel tone */
    return gt_nearest_role_pen(t, ((uc_u32)rgb[slot][0] << 16) | ((uc_u32)rgb[slot][1] << 8) | rgb[slot][2], depth);
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
    for (i = 0; i < h->rcount; i++)      /* S57-PENS47-UNOWNED: the role pens go where the icon pens go */
        if (h->rhave[i]) {
            o->release(o->ctx, h->rpen[i]);
            h->rhave[i] = 0;
            n++;
        }
    h->rcount = 0;
    for (i = 0; i < h->pcount; i++) {    /* TE-10.5b: and the plan pens */
        o->release(o->ctx, h->ppen[i]);
        n++;
    }
    h->pcount = 0;
    h->held = 0;
    h->recheck = 0;
    h->screen = NULL;            /* nothing is obtained on any screen now: the next sync obtains again */
    h->depth = 0;
    return n;
}

static int ip_pen_is_ours(const ip_holder *h, int pen)
{
    int i;
    if (pen >= h->first && pen < h->first + h->count && h->have[pen - h->first])
        return 1;
    for (i = 0; i < h->rcount; i++)
        if (h->rpen[i] == pen && h->rhave[i])
            return 1;
    return 0;
}

int ip_plan_sync(ip_holder *h, const ip_ops *o, const gt_plan_pen *plan, int n)
{
    int i, j, k;
    for (i = 0; i < h->pcount; ) {       /* release what the plan no longer holds exclusive */
        int keep = 0;
        for (j = 0; j < n && !keep; j++)
            keep = plan[j].exclusive && plan[j].pen == h->ppen[i];
        if (keep) {
            i++;
            continue;
        }
        o->release(o->ctx, h->ppen[i]);
        for (k = i + 1; k < h->pcount; k++) {
            h->ppen[k - 1] = h->ppen[k];
            h->prgb[k - 1] = h->prgb[k];
        }
        h->pcount--;
    }
    for (j = 0; j < n; j++) {
        uc_u32 c = plan[j].rgb;
        if (!plan[j].exclusive || ip_pen_is_ours(h, plan[j].pen))
            continue;
        for (i = 0; i < h->pcount && h->ppen[i] != plan[j].pen; i++)
            ;
        if (i < h->pcount) {
            if (h->prgb[i] != c) {
                h->prgb[i] = c;
                if (o->set_rgb)
                    o->set_rgb(o->ctx, plan[j].pen, c);
            }
        } else if (h->pcount < GTE_COUNT &&
                   o->obtain(o->ctx, plan[j].pen, (c >> 16) & 255, (c >> 8) & 255, c & 255) == plan[j].pen) {
            h->ppen[h->pcount] = plan[j].pen;
            h->prgb[h->pcount] = c;
            h->pcount++;
        }
    }
    return h->pcount;
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
        memcpy(h->rgb[i], c, 3);
        if (o->obtain(o->ctx, p.icon_first + i, c[0], c[1], c[2]) == p.icon_first + i) {
            h->have[i] = 1;
            h->held++;
        }
    }
    return h->held;
}

int ip_recolour(ip_holder *h, const ip_ops *o)
{
    gtheme th;
    uint8_t rgb[IP_PENS][3];
    int flat = 0, i, n = 0;
    unsigned mask = 0;
    if (!h->screen || !o->theme || !o->set_rgb)
        return 0;
    memset(&th, 0, sizeof th);
    o->theme(o->ctx, &th, &flat, &mask);
    ip_colours(&th, ip_ground(&th, flat, mask), rgb);
    for (i = 0; i < h->count; i++) {
        const uint8_t *c = rgb[h->slot0 + i];
        if (!h->have[i] || !memcmp(h->rgb[i], c, 3))
            continue;
        memcpy(h->rgb[i], c, 3);
        o->set_rgb(o->ctx, h->first + i, ((uc_u32)c[0] << 16) | ((uc_u32)c[1] << 8) | c[2]);
        n++;
    }
    if (h->rcount > 0 && (flat || ((mask >> DT_PALETTE) & 1))) {   /* the held role pens follow the theme (desk_live_follow's gate) */
        for (i = 0; i < h->rcount; i++) {
            uc_u32 c = desk_pen_colour(&th, h->rpen[i], flat);
            if (!h->rhave[i] || h->rrgb[i] == c)
                continue;
            h->rrgb[i] = c;
            o->set_rgb(o->ctx, h->rpen[i], c);
            n++;
        }
    }
    return n;
}

int ip_role_claim(ip_holder *h, int pen, uc_u32 rgb)
{
    int i;
    if (!h->screen)
        return 0;
    for (i = 0; i < h->rcount; i++)
        if (h->rpen[i] == pen && h->rhave[i]) {
            h->rrgb[i] = rgb;
            return 1;
        }
    return 0;
}

int ip_any_free(int (*is_free)(void *ctx, int pen), void *ctx, const int *pens, int n)
{
    int i;
    if (!is_free)
        return 0;
    for (i = 0; i < n; i++)
        if (is_free(ctx, pens[i]))
            return 1;
    return 0;
}

int ip_holder_stale(const ip_holder *h, const void *screen, int depth)
{
    return h->screen != screen || h->depth != depth || h->recheck;
}

int ip_holder_verify(ip_holder *h, const ip_ops *o, const void *screen, int depth)
{
    int i, lost = 0, gained = 0;
    if (!o->is_free || !h->screen || h->screen != screen)
        return 0;
    if (h->depth == depth)
        for (i = 0; i < h->count; i++) {
            int fr = o->is_free(o->ctx, h->first + i);
            if (fr && h->have[i])
                lost = 1;
            else if (fr)
                gained = 1;
        }
    if (h->depth == depth)
        for (i = 0; i < h->rcount; i++) {
            int fr = o->is_free(o->ctx, h->rpen[i]);
            if (fr && h->rhave[i])
                lost = 1;
            else if (fr)
                gained = 1;
        }
    if (h->depth == depth)
        for (i = 0; i < h->pcount; i++)  /* TE-10.5b2: a plan pen free on the ColorMap is lost too */
            if (o->is_free(o->ctx, h->ppen[i]))
                lost = 1;
    if (h->depth != depth || lost) {     /* reopened at the same address: our pens went with the old ColorMap */
        ip_holder_init(h);
        return 1;
    }
    if (gained) {
        h->recheck = 1;
        return 1;
    }
    return 0;
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

int ip_sync_decide(const ip_holder *h, int hold, const void *screen, int depth)
{
    if (!hold || !screen)
        return h->screen ? IP_SYNC_RELEASE : IP_SYNC_NONE;
    return ip_holder_stale(h, screen, depth) ? IP_SYNC_OBTAIN : IP_SYNC_NONE;
}

int ip_sync_new_identity(const void *old_screen, const void *new_screen, int act, int lost)
{
    if (act == IP_SYNC_NONE)
        return 0;
    if (old_screen != new_screen)
        return 1;
    return act == IP_SYNC_OBTAIN && lost;
}

int ip_sync(ip_holder *h, const ip_ops *o, int hold, const void *screen, int depth, int planes)
{
    int act, lost;
    const void *old = h->screen;
    ip_holder_verify(h, o, screen, depth);
    lost = old && !h->screen;            /* verify forgets the pens only when the same address was reopened */
    act = ip_sync_decide(h, hold, screen, depth);
    if (act == IP_SYNC_OBTAIN && o->theme_ready && !o->theme_ready(o->ctx))
        return IP_SYNC_NEED_THEME;   /* FLAT-BOOT-FREEZE: nothing obtained, set or flagged yet. The caller loads the theme and calls
                                      * again. The verify above is idempotent: if it forgot the pens (same-address reopen) the
                                      * second call sees old == NULL, lost == 0, which ip_sync_new_identity maps to 1 for a
                                      * non-NULL screen just as old == screen, lost == 1 does. A changed screen or depth gives the
                                      * same answer from both calls. */
    if (act == IP_SYNC_RELEASE) {
        ip_holder_release(h, o);
    } else if (act == IP_SYNC_OBTAIN) {
        gtheme th;
        uint8_t rgb[IP_PENS][3];
        int flat = 0, lp[4], k, n;
        unsigned mask = 0;
        uc_u32 lc[4];
        memset(&th, 0, sizeof th);
        o->theme(o->ctx, &th, &flat, &mask);
        ip_colours(&th, ip_ground(&th, flat, mask), rgb);
        ip_holder_obtain(h, o, screen, depth, rgb);   /* releases the old ones first */
        n = desk_live_follow(&th, flat, mask, planes, lp, lc);
        h->rcount = n;                   /* S57-PENS47-UNOWNED: own the role pens before writing them (a pen another client holds is skipped) */
        for (k = 0; k < n; k++) {
            h->rpen[k] = lp[k];
            h->rrgb[k] = lc[k];
            h->rhave[k] = (uint8_t)(o->obtain(o->ctx, lp[k], (lc[k] >> 16) & 255, (lc[k] >> 8) & 255, lc[k] & 255) == lp[k]);
            if (h->rhave[k] && o->set_rgb)
                o->set_rgb(o->ctx, lp[k], lc[k]);
        }
    }
    h->new_identity = ip_sync_new_identity(old, screen, act, lost);
    return act;
}

void ip_holder_plan_mask(const ip_holder *h, uc_u32 mask[GT_MASK_WORDS])
{
    int i;
    for (i = 0; i < h->pcount; i++) {
        int p = h->ppen[i];
        if (p >= 0 && p < GT_MASK_WORDS * 32)
            mask[p >> 5] |= (uc_u32)1 << (p & 31);
    }
}

int ip_sync_plan(ip_holder *h, const ip_ops *o, int hold, const void *screen, int depth)
{
    int act, lost;
    const void *old = h->screen;
    ip_holder_verify(h, o, screen, depth);
    lost = old && !h->screen;            /* verify forgets the pens only when the same address was reopened */
    act = ip_sync_decide(h, hold, screen, depth);
    if (act == IP_SYNC_OBTAIN && o->theme_ready && !o->theme_ready(o->ctx))
        return IP_SYNC_NEED_THEME;       /* nothing obtained or flagged yet; the caller loads the theme and syncs again (as ip_sync) */
    if (act == IP_SYNC_RELEASE) {
        ip_holder_release(h, o);
    } else if (act == IP_SYNC_OBTAIN) {
        gt_plan_pen plan[GTE_COUNT];
        int n = o->plan ? o->plan(o->ctx, depth, plan, GTE_COUNT) : 0, i, want = 0;
        if (n < 0)
            n = 0;
        if (h->screen != screen || h->depth != depth)
            ip_holder_release(h, o);     /* another screen or depth (the holder was not forgotten): release what it holds first */
        ip_plan_sync(h, o, plan, n);
        for (i = 0; i < n; i++)
            want += plan[i].exclusive != 0;
        h->screen = screen;
        h->depth = depth;
        h->recheck = h->pcount < want;   /* a refused pen is retried by the next sync */
    }
    h->new_identity = ip_sync_new_identity(old, screen, act, lost);
    return act;
}

int ip_hold_wanted(int quit, int held, int gfx_v39)
{
    return !quit && !held && gfx_v39;
}

unsigned long ip_scale32(unsigned c)
{
    return (unsigned long)(c & 0xff) * 0x01010101UL;
}

int ip_screen_depth(int planes, int ehb)
{
    return (ehb && planes == 6) ? 5 : planes;
}

void ip_shared_init(ip_shared *s)
{
    s->owners = 0;
    s->epoch = 0;
    ip_holder_init(&s->h);
}

int ip_shared_set(ip_shared *s, unsigned owner, int on)
{
    if (on)
        s->owners |= owner;
    else
        s->owners &= ~owner;
    return ip_shared_refs(s);
}

int ip_shared_refs(const ip_shared *s)
{
    int n = 0;
    unsigned o;
    for (o = s->owners; o; o >>= 1)
        n += (int)(o & 1u);
    return n;
}

int ip_shared_hold(const ip_shared *s, int gfx_v39)
{
    return s->owners != 0 && gfx_v39;
}

int ip_main_step(int applied, int owned)
{
    if (applied)
        return owned ? IP_MAIN_SYNC : IP_MAIN_TAKE;
    return owned ? IP_MAIN_DROP : IP_MAIN_NONE;
}

/* ---- TE-10.3: the pen holder per public screen ---- */
static int ips_lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

static int ips_eq(const char *a, const char *b)
{
    while (*a && ips_lower((unsigned char)*a) == ips_lower((unsigned char)*b)) { a++; b++; }
    return ips_lower((unsigned char)*a) == ips_lower((unsigned char)*b);
}

static void ips_set_name(char *dst, const char *src)
{
    strncpy(dst, src, IP_PSN_NAME - 1);
    dst[IP_PSN_NAME - 1] = 0;
}

void ip_screens_init(ip_screens *t)
{
    memset(t, 0, sizeof *t);
    t->count = 1;
    ips_set_name(t->slot[0].name, "Workbench");
    ip_shared_init(&t->slot[0].sh);
}

int ip_screens_find(const ip_screens *t, const char *name)
{
    int i;
    for (i = 0; i < t->count; i++)
        if (ips_eq(t->slot[i].name, name)) return i;
    return -1;
}

int ip_screens_add(ip_screens *t, const char *name)
{
    int i = ip_screens_find(t, name);
    if (i >= 0) return i;
    if (!name[0] || strlen(name) >= IP_PSN_NAME || t->count >= IP_SCREENS) return -1;
    memset(&t->slot[t->count], 0, sizeof t->slot[0]);
    ips_set_name(t->slot[t->count].name, name);
    ip_shared_init(&t->slot[t->count].sh);
    return t->count++;
}

int ip_screens_drop(ip_screens *t, const char *name)
{
    int i = ip_screens_find(t, name);
    if (i < 0) return 0;
    if (i == 0) return -1;
    for (; i + 1 < t->count; i++) t->slot[i] = t->slot[i + 1];
    t->count--;
    return 1;
}

static int ips_snap_find(const ip_snap *s, const char *name)
{
    int i;
    for (i = 0; i < s->count; i++)
        if (ips_eq(s->e[i].name, name)) return i;
    return -1;
}

void ip_screens_diff(const ip_snap *old_s, const ip_snap *new_s, ip_diff *out)
{
    int i, j;
    out->count = 0;
    for (i = 0; i < new_s->count; i++) {
        const ip_snap_entry *n = &new_s->e[i];
        ip_diff_entry *d = &out->e[out->count++];
        j = ips_snap_find(old_s, n->name);
        ips_set_name(d->name, n->name);
        d->flags = n->flags;
        d->screen = n->screen;
        d->depth = n->depth;
        if (j < 0) d->kind = IP_DIFF_APPEARED;
        else if (old_s->e[j].screen != n->screen || old_s->e[j].depth != n->depth) d->kind = IP_DIFF_REOPENED;
        else d->kind = IP_DIFF_SAME;
    }
    for (i = 0; i < old_s->count; i++) {
        if (ips_snap_find(new_s, old_s->e[i].name) >= 0) continue;
        ips_set_name(out->e[out->count].name, old_s->e[i].name);
        out->e[out->count].kind = IP_DIFF_GONE;
        out->e[out->count].flags = old_s->e[i].flags;
        out->e[out->count].screen = old_s->e[i].screen;
        out->e[out->count].depth = old_s->e[i].depth;
        out->count++;
    }
}

int ip_screens_apply(ip_screens *t, const ip_diff *d)
{
    int i, changed = 0;

    for (i = 0; i < d->count; i++) {
        const ip_diff_entry *e = &d->e[i];
        int s = ip_screens_find(t, e->name);
        ip_holder *h;

        if (s == 0) continue;                                    /* Workbench has its own path */
        if (e->kind == IP_DIFF_GONE) {                           /* closed, private or not: the pens went with it */
            if (s < 0) continue;
            ip_holder_init(&t->slot[s].sh.h);
            t->slot[s].owed = 0;
            changed++;
            continue;
        }
        if (e->flags & IP_PSN_PRIVATE) continue;                 /* not open yet: no slot, and a slot stays as it is */
        if (s < 0) {                                             /* first public sighting, whatever the diff calls it */
            if (ip_screens_add(t, e->name) >= 0) changed++;
            continue;
        }
        h = &t->slot[s].sh.h;
        if (h->screen ? (h->screen == e->screen && h->depth == e->depth) : e->kind == IP_DIFF_SAME) continue;
        ip_holder_init(h);                                       /* the pens went with the ColorMap: forget, never release */
        t->slot[s].epoch++;
        t->slot[s].owed = 0;
        changed++;
    }
    return changed;
}

/* ---- TE-10.4: the per-screen sync ---- */
int ip_snap_add(ip_snap *s, const char *name, const void *screen, unsigned flags, int depth, int ehb)
{
    ip_snap_entry *e;
    if (!name || !name[0] || strlen(name) >= IP_PSN_NAME || s->count >= IP_SCREENS) return 0;
    e = &s->e[s->count++];
    ips_set_name(e->name, name);
    e->screen = screen;
    e->flags = flags;
    e->depth = depth;
    e->ehb = ehb;
    return 1;
}

int ip_screens_sync(ip_screens *t, const ip_snap *snap, const ip_ops *o, const int *hold, ip_slot_ctx_fn select, void *ctx, int *need)
{
    int i, needs = 0;
    for (i = 0; i < t->count; i++) {
        ip_screen_slot *sl = &t->slot[i];
        int j = ips_snap_find(snap, sl->name), act, planes;
        const ip_snap_entry *e = j >= 0 ? &snap->e[j] : 0;
        if (need) need[i] = 0;
        if (i > 0 && (!e || (e->flags & IP_PSN_PRIVATE))) {   /* not open yet, or gone (already forgotten) */
            sl->owed = 0;
            continue;
        }
        if (select) select(ctx, i, e);
        planes = e ? ((e->ehb && e->depth == 5) ? 6 : e->depth) : 0;
        if (i == 0) {
            act = ip_sync(&sl->sh.h, o, hold[i], e ? e->screen : 0, e ? e->depth : 0, planes);
        } else {                                                  /* TE-10.5b2: another screen holds only its pen plan */
            const void *old = sl->sh.h.screen;
            act = ip_sync_plan(&sl->sh.h, o, hold[i], e->screen, e->depth);
            if (act == IP_SYNC_NEED_THEME) {
                if (old && old == e->screen && !sl->sh.h.screen)
                    sl->owed = 1;                                 /* the same-address reopen was forgotten; ip_screens_apply did not see it */
            } else if (sl->sh.h.new_identity && ((old && old == e->screen) || sl->owed)) {
                sl->epoch++;                                      /* apply bumps a changed screen; the sync bumps only a reopen at the same address */
                sl->owed = 0;
            }
        }
        if (act == IP_SYNC_NEED_THEME) {
            if (need) need[i] = 1;
            needs++;
        } else if (i == 0 && sl->sh.h.new_identity)
            sl->sh.epoch++;
    }
    return needs;
}

int ip_screens_held(const ip_screens *t)
{
    int i, n = 0;
    for (i = 1; i < t->count; i++)
        if (t->slot[i].sh.h.held > 0 || t->slot[i].sh.h.pcount > 0) n++;
    return n;
}

unsigned ip_screens_epoch(const ip_screens *t, const char *name)
{
    int i = ip_screens_find(t, name);
    if (i < 0) return 0;
    return i == 0 ? t->slot[0].sh.epoch : t->slot[i].epoch;
}
