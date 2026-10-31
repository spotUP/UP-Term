/* iconfile_core -- see iconfile_core.h. */
#include <string.h>
#include "iconfile_core.h"

#define DO_SIZE     78          /* DiskObject without its Gadget's trailing images */
#define OLD_DRAWER  56          /* OldDrawerData: NewWindow 48 + CurrentX/Y */
#define IMG_HDR     20
#define OFF_GW      12
#define OFF_GH      14
#define OFF_GFLAGS  16
#define OFF_GREND   22
#define OFF_GSEL    26
#define OFF_DEFTOOL 50
#define OFF_TT      54
#define OFF_DRAWER  66
#define OFF_TOOLWIN 70
#define NEWICON_HDR "*** DON'T EDIT THE FOLLOWING LINES!! ***"

static unsigned rd16(const uint8_t *p) { return (unsigned)p[0] << 8 | p[1]; }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

static size_t img_bytes(unsigned w, unsigned h, unsigned depth)
{
    return (size_t)depth * h * ((w + 15) / 16) * 2;
}

/* A length-prefixed string at pos: its end, or 0 when it runs past len. */
static size_t skip_str(const uint8_t *b, size_t len, size_t pos)
{
    size_t n;
    if (pos + 4 > len) return 0;
    n = rd32(b + pos);
    if (n > len - pos - 4) return 0;
    return pos + 4 + n;
}

static int read_image(const uint8_t *b, size_t len, size_t pos, li_image *im, size_t *end)
{
    if (pos + IMG_HDR > len) return LI_E_SHORT;
    im->left = (int16_t)rd16(b + pos);
    im->top = (int16_t)rd16(b + pos + 2);
    im->width = (uint16_t)rd16(b + pos + 4);
    im->height = (uint16_t)rd16(b + pos + 6);
    im->depth = (uint16_t)rd16(b + pos + 8);
    im->imagedata = rd32(b + pos + 10);
    im->planepick = b[pos + 14];
    im->planeonoff = b[pos + 15];
    im->next = rd32(b + pos + 16);
    if (im->depth > 8 || im->width > 1024 || im->height > 1024) return LI_E_LAYOUT;
    pos += IMG_HDR;
    im->data = b + pos;
    im->dlen = im->imagedata ? img_bytes(im->width, im->height, im->depth) : 0;
    if (im->dlen > len - pos) return LI_E_SHORT;
    *end = pos + im->dlen;
    return 0;
}

int li_parse(const uint8_t *buf, size_t len, li_file *f)
{
    size_t pos, e;
    int r, i;
    uint32_t n;

    memset(f, 0, sizeof *f);
    f->buf = buf;
    f->len = len;
    f->ntt = -1;
    if (len < DO_SIZE) return LI_E_SHORT;
    if (rd16(buf) != 0xE310) return LI_E_MAGIC;
    f->gad_w = (uint16_t)rd16(buf + OFF_GW);
    f->gad_h = (uint16_t)rd16(buf + OFF_GH);
    f->gad_flags = (uint16_t)rd16(buf + OFF_GFLAGS);
    f->gad_render = rd32(buf + OFF_GREND);
    f->gad_select = rd32(buf + OFF_GSEL);
    f->tt_raw = rd32(buf + OFF_TT);
    f->is_drawer = rd32(buf + OFF_DRAWER) != 0;
    pos = DO_SIZE;
    if (f->is_drawer) {
        if (len < pos + OLD_DRAWER) return LI_E_SHORT;
        pos += OLD_DRAWER;
    }
    f->img_off = pos;
    if (f->gad_render) {
        if ((r = read_image(buf, len, pos, &f->img[f->nimg], &e)) != 0) return r;
        f->nimg++;
        pos = e;
    }
    if (f->gad_select) {
        if ((r = read_image(buf, len, pos, &f->img[f->nimg], &e)) != 0) return r;
        f->nimg++;
        pos = e;
    }
    f->img_end = pos;
    if (rd32(buf + OFF_DEFTOOL)) {
        if (!(pos = skip_str(buf, len, pos))) return LI_E_SHORT;
    }
    f->tt_off = f->tt_end = pos;
    if (f->tt_raw) {
        if (pos + 4 > len) return LI_E_SHORT;
        n = rd32(buf + pos);
        if (n < 4 || n % 4 || n > len - pos) return LI_E_LAYOUT;
        n = n / 4 - 1;
        if (n > LI_MAXTT) return LI_E_TOOMANY;
        e = pos + 4;
        for (i = 0; i < (int)n; i++) {
            size_t nx = skip_str(buf, len, e);
            if (!nx) return LI_E_SHORT;
            f->tt[i].off = e;
            f->tt[i].len = rd32(buf + e);
            e = nx;
        }
        f->ntt = (int)n;
        f->tt_end = e;
        pos = e;
    }
    f->rest_off = pos;
    if (rd32(buf + OFF_TOOLWIN)) {
        if (!(pos = skip_str(buf, len, pos))) return LI_E_SHORT;
    }
    /* drawer data extension (6 bytes), then the FORM ICON chunk to the end of the file */
    if (f->is_drawer && len - pos >= 6 && memcmp(buf + pos, "FORM", 4) != 0) pos += 6;
    if (len - pos >= 12 && memcmp(buf + pos, "FORM", 4) == 0 && memcmp(buf + pos + 8, "ICON", 4) == 0) {
        f->form_off = pos;
        f->form_len = len - pos;
    }
    return 0;
}

static int is_newicon(const li_file *f, int i)
{
    const uint8_t *s = f->buf + f->tt[i].off + 4;
    size_t n = f->tt[i].len;
    if (n >= sizeof NEWICON_HDR && memcmp(s, NEWICON_HDR, sizeof NEWICON_HDR - 1) == 0) return 1;
    return n >= 4 && (memcmp(s, "IM1=", 4) == 0 || memcmp(s, "IM2=", 4) == 0);
}

int li_newicon_count(const li_file *f, int *first)
{
    int i, n = 0;
    if (first) *first = -1;
    for (i = 0; i < f->ntt; i++)
        if (is_newicon(f, i)) {
            if (n == 0 && first) *first = i;
            n++;
        }
    return n;
}

void li_edit_apply(li_edit *e, const li_file *src, const li_image *normal, const li_image *selected)
{
    memset(e, 0, sizeof *e);
    e->nimg = selected ? 2 : 1;
    e->img[0] = *normal;
    if (selected) e->img[1] = *selected;
    e->gad_w = normal->width;
    e->gad_h = normal->height;
    e->gad_flags = (uint16_t)((src->gad_flags & ~3u) | (selected ? 2u : 0u));
    e->gad_render = src->gad_render ? src->gad_render : 1;
    e->gad_select = selected ? (src->gad_select ? src->gad_select : 1) : 0;
    e->strip_newicon = 1;
    e->drop_form = 1;
}

void li_edit_restore(li_edit *e, const li_file *cur, const li_file *backup)
{
    int first;
    (void)cur;
    memset(e, 0, sizeof *e);
    e->nimg = backup->nimg;
    e->img[0] = backup->img[0];
    e->img[1] = backup->img[1];
    e->gad_w = backup->gad_w;
    e->gad_h = backup->gad_h;
    e->gad_flags = backup->gad_flags;
    e->gad_render = backup->gad_render;
    e->gad_select = backup->gad_select;
    if (li_newicon_count(backup, &first) > 0) {
        e->ins_src = backup;
        e->ins_at = first;
    }
    if (backup->form_len) e->form_src = backup;
    e->tt_raw = backup->tt_raw;
}

typedef struct wr { uint8_t *out; size_t cap, pos; int over; } wr;

static void put(wr *w, const uint8_t *p, size_t n)
{
    if (n > w->cap - w->pos) { w->over = 1; return; }
    memcpy(w->out + w->pos, p, n);
    w->pos += n;
}

static void put16(wr *w, unsigned v)
{
    uint8_t b[2];
    b[0] = (uint8_t)(v >> 8);
    b[1] = (uint8_t)v;
    put(w, b, 2);
}

static void put32(wr *w, uint32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)(v >> 24);
    b[1] = (uint8_t)(v >> 16);
    b[2] = (uint8_t)(v >> 8);
    b[3] = (uint8_t)v;
    put(w, b, 4);
}

typedef struct ent { const li_file *f; int i; } ent;

size_t li_splice(const li_file *src, const li_edit *e, uint8_t *out, size_t cap)
{
    wr w;
    ent list[2 * LI_MAXTT];
    int n = 0, i, k, ins = 0;
    size_t tail_start, at;
    uint32_t ttraw;
    const uint8_t *b = src->buf;

    w.out = out; w.cap = cap; w.pos = 0; w.over = 0;
    for (i = 0; i < src->ntt; i++)
        if (!(e->strip_newicon && is_newicon(src, i))) { list[n].f = src; list[n].i = i; n++; }
    if (e->ins_src) {
        ent add[LI_MAXTT];
        int na = 0;
        for (i = 0; i < e->ins_src->ntt; i++)
            if (is_newicon(e->ins_src, i)) { add[na].f = e->ins_src; add[na].i = i; na++; }
        ins = e->ins_at < 0 ? 0 : e->ins_at > n ? n : e->ins_at;
        if (n + na > 2 * LI_MAXTT) return 0;
        for (k = n - 1; k >= ins; k--) list[k + na] = list[k];
        for (k = 0; k < na; k++) list[ins + k] = add[k];
        n += na;
    }
    ttraw = src->ntt >= 0 ? src->tt_raw : e->tt_raw;
    if (n == 0 && src->ntt != 0) ttraw = 0;   /* an empty array the source already had is kept as it was */
    else if (n > 0 && !ttraw) ttraw = 1;

    put(&w, b, OFF_GW);
    put16(&w, e->gad_w);
    put16(&w, e->gad_h);
    put16(&w, e->gad_flags);
    put(&w, b + OFF_GFLAGS + 2, OFF_GREND - OFF_GFLAGS - 2);
    put32(&w, e->gad_render);
    put32(&w, e->gad_select);
    put(&w, b + OFF_GSEL + 4, OFF_TT - OFF_GSEL - 4);
    put32(&w, ttraw);
    put(&w, b + OFF_TT + 4, src->img_off - OFF_TT - 4);
    for (i = 0; i < e->nimg; i++) {
        const li_image *m = &e->img[i];
        put16(&w, (unsigned)(uint16_t)m->left);
        put16(&w, (unsigned)(uint16_t)m->top);
        put16(&w, m->width);
        put16(&w, m->height);
        put16(&w, m->depth);
        put32(&w, m->imagedata);
        put(&w, &m->planepick, 1);
        put(&w, &m->planeonoff, 1);
        put32(&w, m->next);
        put(&w, m->data, m->dlen);
    }
    put(&w, b + src->img_end, src->tt_off - src->img_end);
    if (n > 0 || src->ntt == 0) {
        put32(&w, (uint32_t)(n + 1) * 4);
        for (k = 0; k < n; k++) {
            const li_tt *t = &list[k].f->tt[list[k].i];
            put(&w, list[k].f->buf + t->off, 4 + t->len);
        }
    }
    tail_start = src->form_len ? src->form_off : src->len;
    at = src->ntt >= 0 ? src->tt_end : src->tt_off;
    put(&w, b + at, tail_start - at);
    if (e->form_src && e->form_src->form_len)
        put(&w, e->form_src->buf + e->form_src->form_off, e->form_src->form_len);
    else if (src->form_len && !e->drop_form)
        put(&w, b + src->form_off, src->form_len);
    return w.over ? 0 : w.pos;
}

int li_pick_variant(unsigned screen_w, unsigned screen_h)
{
    return screen_h > 0 && screen_w >= 2 * screen_h;
}

int li_dat_open(const uint8_t *buf, size_t len, li_dat *d)
{
    unsigned i;
    memset(d, 0, sizeof *d);
    if (len < 16 || memcmp(buf, "LUCI", 4) != 0 || rd16(buf + 4) != 1) return -1;
    d->buf = buf;
    d->len = len;
    d->count = rd16(buf + 6);
    d->depth = rd16(buf + 8);
    d->variants = rd16(buf + 10);
    if (d->variants == 0 || d->variants > 4 || d->depth == 0 || d->depth > 8) return -1;
    d->index_off = 16 + 4 * (size_t)d->variants;
    if (d->index_off + 52 * (size_t)d->count > len) return -1;
    for (i = 0; i < d->variants; i++) {
        d->vw[i] = rd16(buf + 16 + 4 * i);
        d->vh[i] = rd16(buf + 18 + 4 * i);
    }
    return 0;
}

int li_dat_find(const li_dat *d, const char *name, int v, li_image *normal, li_image *selected)
{
    unsigned lo = 0, hi = d->count;
    size_t nl = strlen(name), off, sz;
    int k;

    if (v < 0 || (unsigned)v >= d->variants || nl == 0 || nl > 48) return -1;
    while (lo < hi) {
        unsigned mid = (lo + hi) / 2;
        const uint8_t *ent = d->buf + d->index_off + 52 * (size_t)mid;
        int c = strncmp((const char *)ent, name, 48);
        if (c == 0) {
            off = rd32(ent + 48);
            for (k = 0; k < v; k++) off += 2 * img_bytes(d->vw[k], d->vh[k], d->depth);
            sz = img_bytes(d->vw[v], d->vh[v], d->depth);
            if (off > d->len || 2 * sz > d->len - off) return -1;
            memset(normal, 0, sizeof *normal);
            normal->width = (uint16_t)d->vw[v];
            normal->height = (uint16_t)d->vh[v];
            normal->depth = (uint16_t)d->depth;
            normal->planepick = (uint8_t)((1u << d->depth) - 1);
            normal->imagedata = 1;
            normal->data = d->buf + off;
            normal->dlen = sz;
            *selected = *normal;
            selected->data = d->buf + off + sz;
            return 0;
        }
        if (c < 0) lo = mid + 1; else hi = mid;
    }
    return -1;
}

int li_backup_path(const char *icon_file, char *out, size_t cap)
{
    static const char prefix[] = "SYS:Storage/Icons/Amarchy/";
    const char *colon = strchr(icon_file, ':');
    size_t n = strlen(icon_file), pl = sizeof prefix - 1;
    const char *sfx;

    if (!colon || colon == icon_file || colon[1] == 0 || memchr(icon_file, '/', (size_t)(colon - icon_file))) return -1;
    if (n < 6) return -1;
    sfx = icon_file + n - 5;
    if (!((sfx[0] == '.') && (sfx[1] | 32) == 'i' && (sfx[2] | 32) == 'n' && (sfx[3] | 32) == 'f' && (sfx[4] | 32) == 'o')) return -1;
    if (pl + n + 1 > cap) return -1;
    memcpy(out, prefix, pl);
    memcpy(out + pl, icon_file, (size_t)(colon - icon_file));
    out[pl + (size_t)(colon - icon_file)] = '/';
    memcpy(out + pl + (size_t)(colon - icon_file) + 1, colon + 1, n - (size_t)(colon - icon_file));
    return 0;
}
