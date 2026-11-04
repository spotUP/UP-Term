/* amarchy-flat core: see flat_core.h. */
#include <string.h>
#include <stdio.h>
#include "flat_core.h"
#include "gthemes_core.h"
#include "../device/romspan.h"

void flat_init(flat_state *s)
{
    memset(s, 0, sizeof *s);
    s->version = FLAT_VERSION;
    s->size = FLAT_STATE_SIZE;
}

int flat_check(const void *p, size_t avail)
{
    const flat_state *s = (const flat_state *)p;
    if (!p || avail < 4)
        return FLAT_ERR_SIZE;
    if (s->version != FLAT_VERSION)
        return FLAT_ERR_VERSION;
    if (s->size != FLAT_STATE_SIZE || avail < FLAT_STATE_SIZE)
        return FLAT_ERR_SIZE;
    return FLAT_OK;
}

const char *flat_status_word(const flat_state *s, int found)
{
    if (!found || !s)
        return "absent";
    if (s->held)
        return "held";
    return s->enabled ? "on" : "off";
}

int flat_status_line(const void *p, size_t avail, int found, char *out, size_t cap)
{
    int rc = found ? flat_check(p, avail) : FLAT_OK, n;
    const flat_state *s = (const flat_state *)p;
    if (cap < 48)
        return 0;
    if (!found || rc != FLAT_OK)
        n = snprintf(out, cap, "flat=absent%s", !found ? "" : rc == FLAT_ERR_VERSION ? " bad=version" : " bad=size");
    else
        n = snprintf(out, cap, "flat=%s calls=%lu chained=%d%s%s", flat_status_word(s, 1), (unsigned long)s->calls,
                     s->still_chained ? 1 : 0, s->held ? " why=" : "", s->held ? flat_held_name(s->held) : "");
    return n > 0 && (size_t)n < cap ? n : 0;
}

int flat_can_exit(const flat_state *s)
{
    return !s || !s->still_chained;
}

void flat_swap_init(flat_swap *w)
{
    memset(w, 0, sizeof *w);
}

int flat_swap_install(flat_swap *w, uintptr_t *slot, uintptr_t ours)
{
    if (!w || !slot || !ours)
        return FLAT_ERR_ARG;
    if (w->installed || *slot == ours)
        return FLAT_ERR_DOUBLE;
    w->saved = *slot;
    w->ours = ours;
    *slot = ours;
    w->installed = 1;
    w->inert = 0;
    return FLAT_OK;
}

int flat_swap_remove(flat_swap *w, uintptr_t *slot, flat_state *st)
{
    if (!w || !slot || !w->installed)
        return FLAT_NOT_INSTALLED;
    if (*slot != w->ours) {                 /* a foreign hook sits on top: never write our saved value over it */
        if (st)
            st->still_chained = 1;
        w->inert = 1;
        return FLAT_STILL_CHAINED;
    }
    *slot = w->saved;
    w->installed = 0;
    w->inert = 0;
    if (st)
        st->still_chained = 0;
    return FLAT_REMOVED;
}

static void put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

void flat_tramp_build(unsigned char *out, uint32_t counter_addr, uint32_t saved_addr)
{
    static const unsigned char code[16] = {
        0x2F, 0x08,             /*  0 move.l a0,-(sp) */
        0x20, 0x7A, 0x00, 0x0C, /*  2 movea.l 16(pc),a0   (ext word at 4, data at 16) */
        0x52, 0x90,             /*  6 addq.l #1,(a0) */
        0x20, 0x5F,             /*  8 movea.l (sp)+,a0 */
        0x2F, 0x3A, 0x00, 0x08, /* 10 move.l 20(pc),-(sp) (ext word at 12, data at 20) */
        0x4E, 0x75              /* 14 rts: jumps to the saved routine, returns to the original caller */
    };
    memcpy(out, code, sizeof code);
    put32(out + 16, counter_addr);
    put32(out + 20, saved_addr);
}

int flat_conv(unsigned intuition_version)
{
    return intuition_version < 44 ? FLAT_CONV_STACK : FLAT_CONV_REG;
}

/* ---- FLAT-D4 ---- */
static const flat_dopus_build dopus_table[] = {
    { 73, 8, { 0x780fd46bu, 0xba2cf172u } },   /* DOpus 5.104 (LIB_VERSION 73, LIB_REVISION 8), measured on rig 5 (OS 3.2) by STATUS "sums=" (flat_sum_reloc, same at two load addresses) */
};

const flat_dopus_build *flat_dopus_table(int *n)
{
    if (n)
        *n = (int)(sizeof dopus_table / sizeof dopus_table[0]);
    return dopus_table;
}

uint32_t flat_sum(const void *p, size_t len)
{
    const unsigned char *b = (const unsigned char *)p;
    uint32_t sum = 0;
    size_t i;
    for (i = 0; i + 4 <= len; i += 4)
        sum += ((uint32_t)b[i] << 24) | ((uint32_t)b[i + 1] << 16) | ((uint32_t)b[i + 2] << 8) | b[i + 3];
    return sum ? sum : 1;
}

/* A routine's bytes carry absolute addresses (relocations) that differ with every load address, so a plain sum of the
 * first bytes changes from boot to boot. Skip the long that follows an opcode word with an absolute-long operand
 * (source EA 7/1, or the destination EA of a MOVE), and fingerprint the rest. */
uint32_t flat_sum_reloc(const void *p, size_t len)
{
    const unsigned char *b = (const unsigned char *)p;
    uint32_t sum = 0;
    size_t i = 0;
    while (i + 2 <= len) {
        unsigned op = ((unsigned)b[i] << 8) | b[i + 1];
        sum = (sum << 5 | sum >> 27) ^ op;
        i += 2;
        if ((op & 0x3f) == 0x39 || (op < 0x4000 && ((op >> 6) & 7) == 7 && ((op >> 9) & 7) == 1))
            i += 4;                     /* absolute-long operand: relocated, not part of the fingerprint */
    }
    return sum ? sum : 1;
}

uint32_t flat_jmp_target(const unsigned char *e)
{
    if (!e || e[0] != 0x4E || e[1] != 0xF9)
        return 0;
    return ((uint32_t)e[2] << 24) | ((uint32_t)e[3] << 16) | ((uint32_t)e[4] << 8) | e[5];
}

int flat_dopus_gate(const flat_dopus_build *tab, int n, unsigned version, unsigned revision,
                    const uint32_t sums[FLAT_EL_COUNT], int *reason)
{
    int i, el, mask = 0, measured = 0;
    int why = FLAT_GATE_UNKNOWN;
    for (i = 0; tab && i < n; i++) {
        if (tab[i].version != version || tab[i].revision != revision)
            continue;
        why = FLAT_GATE_UNMEASURED;
        for (el = 0; el < FLAT_EL_COUNT; el++) {
            if (!tab[i].sum[el])
                continue;
            measured = 1;
            if (sums && sums[el] == tab[i].sum[el])
                mask |= 1 << el;
        }
        if (measured)
            why = mask ? FLAT_GATE_OK : FLAT_GATE_SUM;
        break;
    }
    if (reason)
        *reason = why;
    return mask;
}

int flat_dopus_decide(const flat_state *s, int element, int recessed, int pen_shine, int pen_back)
{
    (void)recessed;                         /* a recessed box swaps the two halves; the shadow half is the L either way */
    if (!s || element < 0 || element >= FLAT_EL_COUNT)
        return FLAT_DRAW_STOCK;
    if (!s->enabled || s->held)
        return FLAT_DRAW_STOCK;
    return pen_shine == pen_back ? FLAT_DRAW_SKIP : FLAT_DRAW_STOCK;
}

int flat_frame_decide(const flat_state *s, unsigned long method, int pen_shine, int pen_back)
{
    if (!s || !s->enabled || s->held)
        return FLAT_DRAW_STOCK;
    if (method != 0x202UL && method != FLAT_IM_DRAWFRAME)
        return FLAT_DRAW_STOCK;
    return pen_shine == pen_back ? FLAT_DRAW_SKIP : FLAT_DRAW_STOCK;
}

int flat_dopus_status_line(int found, unsigned version, unsigned revision, int reason, int mask, int installed,
                           unsigned long calls, unsigned long flat, const uint32_t sums[FLAT_EL_COUNT], char *out, size_t cap)
{
    int n, k;
    if (cap < 96)
        return 0;
    if (!found)
        n = snprintf(out, cap, "dopus=absent");
    else if (!installed)
        n = snprintf(out, cap, "dopus=%u.%u inert=%s", version, revision,
                     reason == FLAT_GATE_UNKNOWN ? "unknown" : reason == FLAT_GATE_UNMEASURED ? "unmeasured" : "sum");
    else
        n = snprintf(out, cap, "dopus=%u.%u patched=%d calls=%lu flat=%lu", version, revision, mask, calls, flat);
    if (found && sums && n > 0 && (size_t)n < cap) {        /* the measured routine sums: how the table is filled */
        k = snprintf(out + n, cap - (size_t)n, " sums=%08lx,%08lx", (unsigned long)sums[0], (unsigned long)sums[1]);
        n = k > 0 ? n + k : -1;
    }
    return n > 0 && (size_t)n < cap ? n : 0;
}

int flat_frame_status_line(int installed, unsigned long calls, unsigned long flat, char *out, size_t cap)
{
    int n;
    if (cap < 48)
        return 0;
    if (!installed)
        n = snprintf(out, cap, "frame=absent");
    else
        n = snprintf(out, cap, "frame=hooked calls=%lu flat=%lu", calls, flat);
    return n > 0 && (size_t)n < cap ? n : 0;
}

int flat_which_from_gtype(unsigned gadget_type)
{
    switch (gadget_type & 0xF0u) {
    case 0x40: return 0;        /* GTYP_WDEPTH */
    case 0x50: return 0;        /* GTYP_SDEPTH: the screen title bar's depth gadget (type 0xc055), FLAT-C7 */
    case 0x60: return 1;        /* GTYP_WZOOM */
    case 0x10: return 2;        /* GTYP_SIZING */
    case 0x80: return 3;        /* GTYP_CLOSE */
    default: return -1;
    }
}

int flat_gadget_which(unsigned gadget_type, const void *gadget_render, const void *select_render, const void *image)
{
    if (!(gadget_type & 0x8000u) || !image || (gadget_render != image && select_render != image))   /* GTYP_SYSGADGET */
        return -1;
    return flat_which_from_gtype(gadget_type);
}

unsigned long flat_which_alias(unsigned long which)
{
    return which == 0x05 ? 0x00 : which;    /* SYSIA_Which SDEPTHIMAGE draws the depth glyph */
}

/* ---- FLAT-C4: kill switches (see flat_core.h) ---- */
static int ci_prefix(const char *name, const char *pre)
{
    for (; *pre; name++, pre++) {
        char c = *name;
        if (!c)
            return 0;
        if (c >= 'A' && c <= 'Z')
            c = (char)(c + 32);
        if (c != *pre)
            return 0;
    }
    return 1;
}

int flat_other_task(const char *name)
{
    if (!name)
        return FLAT_HELD_NONE;
    if (ci_prefix(name, "visualprefs"))
        return FLAT_HELD_VISUALPREFS;
    if (ci_prefix(name, "birdie"))
        return FLAT_HELD_BIRDIE;
    return FLAT_HELD_NONE;
}

int flat_other_hook(unsigned long entry, unsigned long lo, unsigned long hi)
{
    return rom_addr_outside(entry, lo, hi) ? FLAT_HELD_HOOK : FLAT_HELD_NONE;
}

int flat_marker_parse(const char *buf, unsigned len)
{
    unsigned hl = (unsigned)strlen(FLAT_MARKER_HEAD);
    int n = 1;
    if (buf && len > hl && memcmp(buf, FLAT_MARKER_HEAD, hl) == 0 && buf[hl] >= '1' && buf[hl] <= '9')
        n = buf[hl] - '0';
    return n > FLAT_MARKER_STRIKES ? FLAT_MARKER_STRIKES : n;
}

unsigned flat_marker_format(int count, char *out)
{
    unsigned hl = (unsigned)strlen(FLAT_MARKER_HEAD);
    if (count < 1)
        count = 1;
    if (count > FLAT_MARKER_STRIKES)
        count = FLAT_MARKER_STRIKES;
    memcpy(out, FLAT_MARKER_HEAD, hl);
    out[hl] = (char)('0' + count);
    out[hl + 1] = '\n';
    out[hl + 2] = 0;
    return hl + 2;
}

int flat_start_decide(int marker, int lmb, int rmb, int other)
{
    if (marker >= FLAT_MARKER_STRIKES)
        return FLAT_HELD_MARKER;
    if (lmb && rmb)
        return FLAT_HELD_GESTURE;
    return other;
}

int flat_install_due(int held, int installed, int last_rc)
{
    return !held && !installed && last_rc == FLAT_ERR_NOCLASS;
}

unsigned flat_marker_remaining(unsigned long written, unsigned long now)
{
    unsigned long age = (now - written) & 0xFFFFFFFFul;
    if (age >= FLAT_MARKER_SECS)
        return 0;
    return (unsigned)(FLAT_MARKER_SECS - age);
}

const char *flat_held_name(unsigned reason)
{
    switch (reason) {
    case FLAT_HELD_MARKER: return "marker";
    case FLAT_HELD_GESTURE: return "gesture";
    case FLAT_HELD_VISUALPREFS: return "VisualPrefs";
    case FLAT_HELD_BIRDIE: return "Birdie";
    case FLAT_HELD_HOOK: return "hook";
    default: return "";
    }
}

int flat_status_text(const flat_state *s, int found, char *out, size_t cap)
{
    const char *w = flat_status_word(s, found), *by = "";
    int n;
    if (cap < 40)
        return 0;
    if (found && s && s->held) {
        switch (s->held) {
        case FLAT_HELD_MARKER: by = " by crash marker"; break;
        case FLAT_HELD_GESTURE: by = " by both mouse buttons"; break;
        case FLAT_HELD_VISUALPREFS: by = " by VisualPrefs"; break;
        case FLAT_HELD_BIRDIE: by = " by Birdie"; break;
        default: by = " by another hook on the gadget class"; break;
        }
    }
    n = snprintf(out, cap, "%s%s", w, by);
    return n > 0 && (size_t)n < cap ? n : 0;
}

const void *flat_tx_surface(const void *layer, const void *bitmap)
{
    return layer ? layer : bitmap;
}

void flat_tx_frame(flat_tx *t, const void *surf, const void *task, int pressed, int fill, int x0, int y0, int x1, int y1)
{
    if (t->armed && t->surf == surf)
        t->armed = 0;
    if (pressed && fill) {
        t->surf = surf; t->task = task;
        t->x0 = x0; t->y0 = y0; t->x1 = x1; t->y1 = y1;
        t->armed = 1;
    }
}

int flat_tx_arm(int pressed, int fill, int role)
{
    return fill && role != FLAT_ROLE_PLAIN && (pressed || role == FLAT_ROLE_PRIMARY);
}

int flat_tx_text(flat_tx *t, const void *surf, const void *task, int cx, int cy)
{
    if (!t->armed || t->surf != surf || t->task != task)
        return 0;
    if (cx < t->x0 || cx > t->x1 || cy < t->y0 || cy > t->y1) {
        t->armed = 0;
        return 0;
    }
    return 1;
}

int flat_tx_pen(const flat_state *s, flat_tx *t, const void *surf, const void *task, int cx, int cy)
{
    if (!s->enabled || s->held || !flat_tx_text(t, surf, task, cx, cy))
        return -1;
    return s->pen_ptext;
}

void flat_gh_frame(flat_gh *g, const void *surf, const void *task, int pressed, int fill, int secondary, int x0, int y0, int x1, int y1)
{
    int i, slot = -1;
    for (i = 0; i < FLAT_GH_N; i++) {
        flat_gbox *b = &g->box[i];
        if (b->surf == surf && b->task == task && b->x0 == x0 && b->y0 == y0 && b->x1 == x1 && b->y1 == y1) {
            slot = i;
            break;
        }
    }
    if (pressed || !fill || secondary) {
        if (slot >= 0)
            g->box[slot].surf = 0;
        return;
    }
    if (slot < 0) {
        slot = g->next;
        g->next = (g->next + 1) % FLAT_GH_N;
    }
    g->box[slot].surf = surf; g->box[slot].task = task;
    g->box[slot].x0 = x0; g->box[slot].y0 = y0; g->box[slot].x1 = x1; g->box[slot].y1 = y1;
    g->box[slot].str = 0; g->box[slot].count = 0;
}

void flat_gh_text(flat_gh *g, const void *surf, const void *task, int cx, int cy, const char *str, int count)
{
    int i;
    for (i = 0; i < FLAT_GH_N; i++) {
        flat_gbox *b = &g->box[i];
        if (b->surf && b->surf == surf && b->task == task && cx >= b->x0 && cx <= b->x1 && cy >= b->y0 && cy <= b->y1) {
            b->str = str; b->count = count; b->lx = cx; b->ly = cy;
            return;
        }
    }
}

int flat_dim_pen(const flat_state *s, int depth)
{
    if (depth >= 3 && depth <= 8 && s->pen_dim && s->pen_dim < (1 << depth))
        return s->pen_dim;
    return flat_button_pen(s, depth);
}

/* pen_dimtext (DK_MUTED) is usable: set, 8+ colours, and a pen of the depth. Shared by the dim text and the scroller knob. */
static int flat_dimtext_ok(const flat_state *s, int depth)
{
    return depth >= 3 && depth <= 8 && s->pen_dimtext && s->pen_dimtext < (1 << depth);
}

int flat_dimtext_pen(const flat_state *s, int depth)
{
    if (flat_dimtext_ok(s, depth))
        return s->pen_dimtext;
    return s->pen_text;
}

unsigned long flat_knob_colour(unsigned long surface, unsigned long muted)
{
    return (unsigned long)gt_scale((uc_u32)surface, 100 - FLAT_KNOB_PCT) + (unsigned long)gt_scale((uc_u32)muted, FLAT_KNOB_PCT);
}

int flat_knob_pen(const flat_state *s, int depth)
{
    if (depth >= 4 && depth <= 8 && s->pen_knob && s->pen_knob < (1 << depth))
        return s->pen_knob;
    return flat_dimtext_ok(s, depth) ? s->pen_dimtext : flat_button_pen(s, depth);   /* KNOB-MUTED: the theme muted text pen (DK_MUTED), flat_button_pen under 8 colours */
}

int flat_gm_pen(const flat_state *s, int depth, int rom, int window, int ptsz, unsigned pat0, unsigned pat1, int bg)
{
    if (!s->enabled || s->held || !rom || window || ptsz != 1 || (pat0 & 0xFFFFu) != FLAT_GM_PAT0 || (pat1 & 0xFFFFu) != FLAT_GM_PAT1)
        return -1;
    if (!flat_dimtext_ok(s, depth) || bg < 0 || bg >= (1 << depth) || s->pen_dimtext == bg)
        return -1;
    return s->pen_dimtext;
}

static int gm_pen_at(uint8_t *const *planes, int depth, int bpr, int x, int y)
{
    int p, pen = 0;
    unsigned off = (unsigned)y * (unsigned)bpr + (unsigned)(x >> 3), bit = 0x80u >> (x & 7);
    for (p = 0; p < depth; p++)
        if (planes[p][off] & bit)
            pen |= 1 << p;
    return pen;
}

long flat_gm_recolour(uint8_t *const *planes, int depth, int bpr, int width, int rows, int x0, int y0, int x1, int y1, int bg, int dim)
{
    int x, y, p;
    long total, nbg = 0, done = 0;
    if (!planes || depth < 1 || depth > 8 || bpr <= 0 || width <= 0 || width > bpr * 8 || rows <= 0)
        return -1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= width) x1 = width - 1;     /* bpr is the row modulo: interleaved bitmaps span every plane */
    if (y1 >= rows) y1 = rows - 1;
    if (x1 < x0 || y1 < y0)
        return -1;
    total = (long)(x1 - x0 + 1) * (long)(y1 - y0 + 1);
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (gm_pen_at(planes, depth, bpr, x, y) == bg)
                nbg++;
    if (nbg * 2 < total)                 /* not a menu fill under the rect: refuse, the caller runs the stock call */
        return -1;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            unsigned off = (unsigned)y * (unsigned)bpr + (unsigned)(x >> 3);
            uint8_t bit = (uint8_t)(0x80u >> (x & 7));
            if (gm_pen_at(planes, depth, bpr, x, y) == bg)
                continue;
            for (p = 0; p < depth; p++) {
                if (dim & (1 << p))
                    planes[p][off] |= bit;
                else
                    planes[p][off] &= (uint8_t)~bit;
            }
            done++;
        }
    return done;
}

const flat_gbox *flat_gh_match(const flat_state *s, const flat_gh *g, const void *surf, const void *task, int ptsz, int x0, int y0, int x1, int y1)
{
    int i;
    if (!s->enabled || s->held || !ptsz || x1 < x0 || y1 < y0)
        return 0;
    for (i = 0; i < FLAT_GH_N; i++) {
        const flat_gbox *b = &g->box[i];
        if (b->surf && b->surf == surf && b->task == task && x0 >= b->x0 && y0 >= b->y0 && x1 <= b->x1 && y1 <= b->y1)
            return b;
    }
    return 0;
}

int flat_push_button(int has_text, int lx, int ly, int w, int h, int activation)
{
    if (activation & 0x0002)                     /* GACT_IMMEDIATE: a scroller arrow (act 0003), not a push button */
        return 0;
    return !has_text || (lx >= 0 && ly >= 0 && lx < w && ly < h);
}

int flat_button_role(const flat_box *self, const flat_box *sib, int n)
{
    int i, left = 0;
    for (i = 0; self && sib && i < n; i++) {
        const flat_box *o = &sib[i];
        if (o->x == self->x && o->y == self->y && o->w == self->w && o->h == self->h)
            continue;
        if (o->y < self->y + self->h && self->y < o->y + o->h) {   /* same row */
            if (o->x > self->x)
                return FLAT_ROLE_PRIMARY;                          /* a button further right: not rightmost */
            left = 1;
        }
    }
    return left ? FLAT_ROLE_SECONDARY : FLAT_ROLE_PRIMARY;
}

int flat_button_pen(const flat_state *s, int depth)
{
    if (depth >= 1 && depth < 3 && s->pen_line)
        return s->pen_line;
    if (!s->pen_sel || depth < 3 || depth > 8 || s->pen_sel >= (1 << depth))
        return s->pen_back;                                  /* S33-PREFS-SKIN-FAIL: only the held pen, never the bare role number pen_button */
    return s->pen_sel;
}

void flat_frame_size(int draw_frame, int dim_w, int dim_h, int img_w, int img_h, int *w, int *h)
{
    *w = draw_frame && dim_w ? dim_w : img_w;
    *h = draw_frame && dim_h ? dim_h : img_h;
}

int flat_button_gadget(const void *gadget_render, const void *select_render, const void *image)
{
    return image && gadget_render == image && select_render == image;
}

/* pen_outline when it is a pen of the depth (non-zero, below 1 << depth), else 0. */
static int flat_outline_of_depth(const flat_state *s, int depth)
{
    if (s->pen_outline && depth >= 1 && depth <= 8 && s->pen_outline < (1 << depth))
        return s->pen_outline;
    return 0;
}

int flat_knob_state_pen(const flat_state *s, int depth, int active)
{
    if (active) {
        int o = flat_outline_of_depth(s, depth);
        return o ? o : flat_knob_pen(s, depth);
    }
    return flat_inactive_pen(s, depth) >= 0 ? flat_inactive_pen(s, depth) : flat_knob_pen(s, depth);
}

int flat_scroller_knob(int cx, int cy, int cw, int ch, int horiz, int vert, unsigned hpot, unsigned vpot, unsigned hbody, unsigned vbody,
                      int *kx_out, int *ky_out, int *kw_out, int *kh_out)
{
    int kx = cx, ky = cy, kw = cw, kh = ch;
    if (cw < 1 || ch < 1)
        return 0;
    if (horiz) {
        kw = (int)(((unsigned long)hbody * (unsigned long)cw) / 0xFFFFUL);
        if (kw < 1) kw = 1;
        kx = cx + (int)(((unsigned long)hpot * (unsigned long)(cw - kw)) / 0xFFFFUL);
    }
    if (vert) {
        kh = (int)(((unsigned long)vbody * (unsigned long)ch) / 0xFFFFUL);
        if (kh < 1) kh = 1;
        ky = cy + (int)(((unsigned long)vpot * (unsigned long)(ch - kh)) / 0xFFFFUL);
    }
    *kx_out = kx; *ky_out = ky; *kw_out = kw; *kh_out = kh;
    return 1;
}

int flat_scroller_knob_img(int cx, int cy, int cw, int ch, int img_ok, int ile, int ite, int iw, int ih, int horiz, int vert,
                           unsigned hpot, unsigned vpot, unsigned hbody, unsigned vbody, int *kx, int *ky, int *kw, int *kh)
{
    if (cw < 1 || ch < 1)
        return 0;
    if (img_ok && iw >= 1 && ih >= 1 && ile >= 0 && ite >= 0 && ile + iw <= cw && ite + ih <= ch) {
        *kx = cx + ile; *ky = cy + ite; *kw = iw; *kh = ih;
        return 1;
    }
    return flat_scroller_knob(cx, cy, cw, ch, horiz, vert, hpot, vpot, hbody, vbody, kx, ky, kw, kh);
}

int flat_scroller_pen(const flat_state *s, int depth, int active, int cx, int cy, int cw, int ch, int horiz, int vert,
                      unsigned hpot, unsigned vpot, unsigned hbody, unsigned vbody, int x0, int y0, int x1, int y1)
{
    int kx = cx, ky = cy, kw = cw, kh = ch;
    if (cw >= 1 && ch >= 1)
        flat_scroller_knob(cx, cy, cw, ch, horiz, vert, hpot, vpot, hbody, vbody, &kx, &ky, &kw, &kh);
    return flat_scroller_pen_box(s, depth, active, cx, cy, cw, ch, kx, ky, kw, kh, x0, y0, x1, y1);
}

int flat_scroller_pen_box(const flat_state *s, int depth, int active, int cx, int cy, int cw, int ch, int kx, int ky, int kw, int kh,
                          int x0, int y0, int x1, int y1)
{
    if (!s->enabled || s->held || cw < 1 || ch < 1 || x0 < cx || y0 < cy || x1 >= cx + cw || y1 >= cy + ch || x1 < x0 || y1 < y0)
        return -1;
    if (x0 >= kx && x1 < kx + kw && y0 >= ky && y1 < ky + kh)
        return flat_knob_state_pen(s, depth, active);
    return s->pen_back;
}

int flat_inactive_pen(const flat_state *s, int depth)
{
    if (depth >= 4 && depth <= 8 && s->pen_inbar && s->pen_inbar < (1 << depth))
        return s->pen_inbar;
    return flat_dimtext_ok(s, depth) ? s->pen_dimtext : -1;      /* under 16 colours: the muted text pen */
}

unsigned long flat_inbar_colour(unsigned long surface, unsigned long text)
{
    return (unsigned long)gt_mix((uc_u32)surface, (uc_u32)text, GT_INACTIVE_PCT);
}

int flat_edge_pen(const flat_state *s, int depth, int active)
{
    int p = active ? -1 : flat_inactive_pen(s, depth);
    return p >= 0 ? p : s->pen_outline;
}

int flat_frame_pen(const flat_state *s, int depth, int active)
{
    if (s->enabled && !s->held)
        return active ? (flat_outline_of_depth(s, depth) ? s->pen_outline : -1) : flat_inactive_pen(s, depth);
    return -1;
}

void flat_title_span(const flat_gadrect *g, int n, int win_w, int bar_h, int *x0, int *x1)
{
    int i;
    *x0 = 0;
    *x1 = win_w - 1;
    for (i = 0; i < n; i++) {
        int l, w, r;
        if (g[i].drag || g[i].y >= bar_h || g[i].y + g[i].h <= 0)
            continue;
        l = g[i].x + (g[i].relright ? win_w : 0);
        w = g[i].w + (g[i].relwidth ? win_w : 0);
        if (w <= 0)
            continue;
        r = l + w - 1;
        if (l + r < win_w) {
            if (r + 1 > *x0) *x0 = r + 1;
        } else if (l - 1 < *x1) {
            *x1 = l - 1;
        }
    }
}

int flat_title_decide(const flat_state *s, int depth, int active, const char *title, int win_w, int bar_h,
                      const char *str, int count, int cp_y, int bx0, int bx1, flat_title *t)
{
    int i;
    if (!s->enabled || s->held || !title || !str || count <= 0 || bar_h <= 0 || win_w < 2 || cp_y < 0 || cp_y >= bar_h || bx0 > bx1)
        return 0;
    for (i = 0; i < count; i++)
        if (!title[i] || title[i] != str[i])
            return 0;
    t->band_pen = -1;
    t->bg_pen = -1;
    t->bx0 = bx0; t->by0 = 0; t->bx1 = bx1; t->by1 = bar_h - 1;
    if (active) {
        t->band_pen = flat_frame_pen(s, depth, 1);
        t->bg_pen = t->band_pen;
        t->text_pen = t->band_pen >= 0 ? s->pen_ptext : -1;
    } else {
        t->band_pen = flat_frame_pen(s, depth, 0);      /* S32-INACTIVE-BAR: a visible dragbar in the inactive tone */
        t->bg_pen = t->band_pen;
        t->text_pen = t->band_pen >= 0 ? s->pen_ptext : flat_dimtext_pen(s, depth);
    }
    return 1;
}

/* ---- S32-STALE-HELD-PENS (flat_core.h): the dim / knob / inbar pens follow the screen identity ---- */
void flat_pens_init(flat_pens *p)
{
    p->screen = 0;
    p->epoch = 0;
    p->dim = p->knob = p->inbar = p->sel = -1;
}

static int pens_here(const flat_pens *p, const void *screen, unsigned epoch)
{
    return screen && p->screen == screen && p->epoch == epoch;
}

static void pens_forget(flat_pens *p, flat_state *s)
{
    p->screen = 0;
    p->epoch = 0;
    p->dim = p->knob = p->inbar = p->sel = -1;
    s->pen_dim = 0;
    s->pen_knob = 0;
    s->pen_inbar = 0;
    s->pen_sel = 0;
}

void flat_pens_release(flat_pens *p, const flat_pen_ops *o, flat_state *s, const void *screen, unsigned epoch)
{
    if (pens_here(p, screen, epoch)) {
        if (p->inbar >= 0)
            o->release(o->ctx, p->inbar);
        if (p->dim >= 0)
            o->release(o->ctx, p->dim);
        if (p->knob >= 0)
            o->release(o->ctx, p->knob);
        if (p->sel >= 0)
            o->release(o->ctx, p->sel);
    }
    pens_forget(p, s);
}

void flat_pens_refresh(flat_pens *p, const flat_pen_ops *o, flat_state *s, const void *screen, unsigned epoch, int depth)
{
    flat_pens_release(p, o, s, screen, epoch);
    if (!screen)
        return;
    p->screen = screen;
    p->epoch = epoch;
    if (depth >= 4) {
        unsigned long c;
        if (s->inbar_rgb[0] || s->inbar_rgb[1] || s->inbar_rgb[2])
            c = ((unsigned long)s->inbar_rgb[0] << 16) | ((unsigned long)s->inbar_rgb[1] << 8) | s->inbar_rgb[2];
        else
            c = flat_inbar_colour(o->rgb(o->ctx, s->pen_back), o->rgb(o->ctx, s->pen_text));
        p->inbar = o->obtain(o->ctx, c);
        if (p->inbar >= 0)
            s->pen_inbar = (uint8_t)p->inbar;
    }
    if (depth >= 4 && s->pen_dimtext) {
        unsigned long c = flat_knob_colour(o->rgb(o->ctx, s->pen_back), o->rgb(o->ctx, s->pen_dimtext));
        p->knob = o->obtain(o->ctx, c);
        if (p->knob >= 0)
            s->pen_knob = (uint8_t)p->knob;
    }
    if (depth >= 3 && (s->sel_rgb[0] || s->sel_rgb[1] || s->sel_rgb[2])) {
        p->sel = o->obtain(o->ctx, ((unsigned long)s->sel_rgb[0] << 16) | ((unsigned long)s->sel_rgb[1] << 8) | s->sel_rgb[2]);
        if (p->sel >= 0)
            s->pen_sel = (uint8_t)p->sel;
    }
    if (depth >= 3) {
        unsigned long c = (unsigned long)gt_scale((uc_u32)o->rgb(o->ctx, flat_button_pen(s, depth)), FLAT_DIM_PCT);
        p->dim = o->obtain(o->ctx, c);
        if (p->dim >= 0)
            s->pen_dim = (uint8_t)p->dim;
    }
}

int flat_pens_follow(flat_pens *p, const flat_pen_ops *o, flat_state *s, int want, const void *screen, unsigned epoch, int depth)
{
    if (!want || !screen) {
        flat_pens_release(p, o, s, screen, epoch);
        return 0;
    }
    if (pens_here(p, screen, epoch))
        return 0;
    pens_forget(p, s);                  /* another screen: the pens went with the old ColorMap, nothing to release */
    flat_pens_refresh(p, o, s, screen, epoch, depth);
    return 1;
}

/* S32-KNOB-NONE-FRESH: see flat_core.h. */
int flat_gadget_is_prop(unsigned gadgettype, int is_propgclass, int has_specialinfo)
{
    if (!has_specialinfo)
        return 0;
    if (gadgettype == 0x0003u)                  /* GTYP_PROPGADGET */
        return 1;
    return gadgettype == 0x0005u && is_propgclass;   /* GTYP_CUSTOMGADGET: only a propgclass object has a PropInfo */
}

int flat_side_px(int rx, int ry)
{
    int px;
    if (rx <= 0 || ry <= rx)
        return 1;
    px = (ry + rx / 2) / rx;
    return px < 1 ? 1 : px;
}

int flat_aspect_find(const flat_aspect_slot *t, int n, const void *scr)
{
    int i;
    if (scr)
        for (i = 0; i < n; i++)
            if (t[i].scr == scr)
                return t[i].px;
    return 1;
}

int flat_aspect_need(const flat_aspect_slot *t, int n, const void *scr, unsigned long mode)
{
    int i;
    for (i = 0; i < n; i++)
        if (t[i].scr == scr)
            return t[i].mode != mode;
    return 1;
}

void flat_aspect_put(flat_aspect_slot *t, int n, const void *scr, unsigned long mode, int px)
{
    int i, at = n - 1;
    for (i = 0; i < n; i++)
        if (t[i].scr == scr) { at = i; break; }
    if (at == n - 1)
        for (i = 0; i < n; i++)
            if (!t[i].scr) { at = i; break; }
    t[at].scr = scr; t[at].mode = mode; t[at].px = px;
}
