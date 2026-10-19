/*
 * kitty.c - kitty's graphics protocol (APC G keys ; base64 ST) for the
 * engine: a picture arrives as RGB, RGBA or a PNG, is made an indexed
 * image (the same vt_image a sixel makes, so the renderer draws it
 * unchanged), and sits on the cells like text.
 *
 * Ported from CTerm's src/vt.c (kitty_keys, kitty_reply, kitty_find, the
 * a= actions, pic_room / pic_fits / pic_gc) and src/pics.c (the 6x6x6
 * colour cube and the 4x4 ordered dither).
 * Copyright (c) 2026 Tobias Karlsson, MIT licence (github.com/creep-ltx/cterm).
 * Changes for vtcon: the bytes stream into the decoder as they arrive (no
 * second copy of the base64), the pictures go through vtcon's image store
 * and its VT_IMAGE_MEMORY, replies go through the engine's reply path.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions: the above copyright notice and this permission
 * notice shall be included in all copies or substantial portions of the
 * Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
 *
 * Supported: a=t (transmit), a=T (transmit and show), a=p (show by id),
 * a=d (delete: a A i I n N c C), a=q (query); f=24, 32, 100 (PNG through
 * the host's decoder); o=z; m=1 chunks; t=d only (no files); i= I= p=;
 * q=1/2; c= r= x= y= w= h= C=1. Not supported, each answered with an
 * error: z<0 below the text (drawn on top), U=1 placeholders, animation,
 * t=f/t/s. Included by vtengine.c (one translation unit with the engine).
 */
#include "inflate.c"

#define KIT_RAW_MAX (4L * 1024L * 1024L) /* the bytes of one picture, base64 decoded */
#define KIT_KEYS 192                     /* the longest key list */
#define KV(k, c) ((k)->kv[(c) >= 'a' ? (c) - 'a' + 26 : (c) - 'A'])

typedef struct vt_kitty {
    vt_u32 kv[52];   /* the keys of the first chunk, a-z then A-Z */
    vt_u8 *raw;      /* the data so far */
    long len, cap;
    vt_u32 acc;      /* the base64 quad being read */
    int nacc;
    int nkeys;
    int in_data;     /* past the ';' of this chunk */
    int more;        /* m=1: the picture goes on in the next APC */
    int full;        /* the data passed KIT_RAW_MAX: dropped */
    char keys[KIT_KEYS];
} vt_kitty;

/* The picture as the actions use it: bpp 1 indices into pal (a stored
 * one), 3 R G B, 4 R G B A, 0 the decoder's 0xAARRGGBB words. */
typedef struct kit_src {
    const vt_u8 *px;
    const vt_u32 *argb, *pal;
    int bpp, w, h;
    vt_u8 *own;      /* the inflated copy, freed with the source */
    vt_u32 *dec;     /* the decoder's buffer */
    vt_image_release dfree;
} kit_src;

static const vt_u8 kit_bayer[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

/* ---- palette: entry 0 unset, 1-216 the 6x6x6 cube, 217-255 greys ---- */

static vt_u32 kit_color(int i)
{
    int v, r, g, b;
    if (i >= 217) {
        v = (i - 217) * 255 / 38;
        return ((vt_u32)v << 16) | ((vt_u32)v << 8) | (vt_u32)v;
    }
    i--;
    r = i / 36 * 51;
    g = i / 6 % 6 * 51;
    b = i % 6 * 51;
    return ((vt_u32)r << 16) | ((vt_u32)g << 8) | (vt_u32)b;
}

/* a level 0..n of v (0..255) with the ordered dither at (x, y) */
static int kit_level(int v, int n, int x, int y)
{
    int q = v * n / 255, rem = v * n % 255;
    return q + ((rem * 16) / 255 > kit_bayer[((y & 3) << 2) | (x & 3)] ? 1 : 0);
}

/* 0xAARRGGBB to a palette index; alpha under half is index 0 */
static vt_u8 kit_quant(vt_u32 p, int x, int y)
{
    int a = (int)(p >> 24), r = (int)(p >> 16) & 255, g = (int)(p >> 8) & 255, b = (int)p & 255;
    int mx = r > g ? r : g, mn = r < g ? r : g;
    if (a < 128)
        return 0;
    if (b > mx)
        mx = b;
    if (b < mn)
        mn = b;
    if (mx - mn <= 12 && g % 51) {
        int l = kit_level(g, 38, x, y);
        return (vt_u8)(217 + l);
    }
    return (vt_u8)(1 + kit_level(r, 5, x, y) * 36 + kit_level(g, 5, x, y) * 6 + kit_level(b, 5, x, y));
}

static vt_u32 kit_px(const kit_src *s, long i)
{
    const vt_u8 *q;
    if (s->bpp == 0)
        return s->argb[i];
    if (s->bpp == 1)
        return s->px[i] ? 0xFF000000u | s->pal[s->px[i]] : 0;
    q = s->px + i * s->bpp;
    return ((vt_u32)(s->bpp == 4 ? q[3] : 255) << 24) | ((vt_u32)q[0] << 16) | ((vt_u32)q[1] << 8) | q[2];
}

/* ---- memory (D7): stored pictures that are not on the screen go first ---- */

static int kit_room(vt_term *t, long bytes)
{
    int i, old;
    if (bytes > VT_IMAGE_MEMORY)
        return 0;
    while (t->img_mem + bytes > VT_IMAGE_MEMORY) {
        old = -1;
        for (i = 0; i < VT_IMG_SLOTS; i++)
            if (t->imgs[i] && t->imgs[i]->pix && t->imgs[i]->kind == 2 &&
                (old < 0 || t->imgs[i]->serial < t->imgs[old]->serial))
                old = i;
        if (old >= 0)
            image_free(t, old);
        else if (!image_evict(t))
            return 0;
    }
    return 1;
}

/* The part (x0, y0, w, h) of src as an indexed image of tw x th pixels
 * (nearest pixel, dithered), its colours packed to the ones in use.
 * 0 when there is no memory. */
static int kit_make(vt_term *t, const kit_src *s, int x0, int y0, int w, int h, int tw, int th,
                    vt_u8 **pix, vt_u32 *pal, int *npal)
{
    vt_u8 *o, map[256];
    int x, y, n = 1, i;
    long bytes = (long)tw * th;
    if (!kit_room(t, bytes))
        return 0;
    o = (vt_u8 *)VT_MALLOC(bytes);
    if (!o)
        return 0;
    memset(map, 0, sizeof(map));
    for (y = 0; y < th; y++) {
        long sy = y0 + (long)y * h / th;
        for (x = 0; x < tw; x++) {
            long sx = x0 + (long)x * w / tw;
            vt_u8 q = kit_quant(kit_px(s, sy * s->w + sx), x, y);
            if (q && !map[q])
                map[q] = (vt_u8)n++;
            o[(long)y * tw + x] = q ? map[q] : 0;
        }
    }
    pal[0] = 0;
    for (i = 1; i < 256; i++)
        if (map[i])
            pal[map[i]] = kit_color(i);
    *pix = o;
    *npal = n;
    return 1;
}

/* ---- the chunks ---- */

static void kitty_abort(vt_term *t)
{
    if (!t->kit)
        return;
    if (t->kit->raw)
        VT_FREE(t->kit->raw);
    VT_FREE(t->kit);
    t->kit = 0;
}

static void kit_parse(const char *s, int n, vt_u32 *kv)
{
    const char *e = s + n;
    while (s < e) {
        int k = *s;
        if (s + 1 < e && s[1] == '=' && ((k >= 'a' && k <= 'z') || (k >= 'A' && k <= 'Z'))) {
            vt_u32 val = 0;
            s += 2;
            if (s < e && *s == '-')
                s++; /* z<0: under the text (it is not) */
            if (s < e && (*s < '0' || *s > '9'))
                val = (vt_u8)*s++;
            else
                while (s < e && *s >= '0' && *s <= '9')
                    val = val * 10 + (vt_u32)(*s++ - '0');
            kv[k >= 'a' ? k - 'a' + 26 : k - 'A'] = val;
        }
        while (s < e && *s != ',')
            s++;
        s++;
    }
}

/* APC G seen: a decoder takes the bytes until ST. A chunk after m=1
 * goes on with the picture so far. */
static void kitty_begin(vt_term *t)
{
    vt_kitty *k = t->kit;
    if (!k) {
        k = (vt_kitty *)VT_MALLOC(sizeof(vt_kitty));
        if (!k) {
            t->str_len = VT_STR_MAX - 1; /* no memory: swallowed as any string */
            return;
        }
        memset(k, 0, sizeof(*k));
        t->kit = k;
    } else if (!k->more) {
        if (k->raw)
            VT_FREE(k->raw);
        memset(k, 0, sizeof(*k));
    }
    k->nkeys = 0;
    k->in_data = 0;
    k->acc = 0;
    k->nacc = 0;
    t->state = S_KITTY;
    t->str_esc = 0;
}

static void kit_data(vt_kitty *k, vt_u8 b)
{
    if (k->full)
        return;
    if (k->len >= k->cap) {
        long nc = k->cap ? k->cap * 2 : 4096;
        vt_u8 *n;
        if (nc > KIT_RAW_MAX)
            nc = KIT_RAW_MAX;
        if (k->len >= nc) {
            k->full = 1;
            return;
        }
        n = (vt_u8 *)VT_MALLOC(nc);
        if (!n) {
            k->full = 1;
            return;
        }
        if (k->raw) {
            memcpy(n, k->raw, k->len);
            VT_FREE(k->raw);
        }
        k->raw = n;
        k->cap = nc;
    }
    k->raw[k->len++] = b;
}

static void kit_b64(vt_kitty *k, vt_u8 c)
{
    int v;
    if (c >= 'A' && c <= 'Z')
        v = c - 'A';
    else if (c >= 'a' && c <= 'z')
        v = c - 'a' + 26;
    else if (c >= '0' && c <= '9')
        v = c - '0' + 52;
    else if (c == '+' || c == '-')
        v = 62;
    else if (c == '/' || c == '_')
        v = 63;
    else
        return; /* '=' and anything else */
    k->acc = (k->acc << 6) | (vt_u32)v;
    if (++k->nacc == 4) {
        kit_data(k, (vt_u8)(k->acc >> 16));
        kit_data(k, (vt_u8)(k->acc >> 8));
        kit_data(k, (vt_u8)k->acc);
        k->acc = 0;
        k->nacc = 0;
    }
}

/* the bytes of the keys, then (after ';') the base64 */
static void kitty_byte(vt_term *t, vt_u8 c)
{
    vt_kitty *k = t->kit;
    if (k->in_data) {
        kit_b64(k, c);
    } else if (c == ';') {
        k->in_data = 1;
    } else if (k->nkeys < KIT_KEYS) {
        k->keys[k->nkeys++] = (char)c;
    }
}

/* ---- replies (D3) ---- */

static int kit_put(char *b, int n, const char *s)
{
    while (*s)
        b[n++] = *s++;
    return n;
}

static void kit_reply(vt_term *t, const vt_kitty *k, vt_u32 id, const char *err)
{
    char b[112];
    int n = 0, q = (int)KV(k, 'q');
    if ((!id && !KV(k, 'I')) || q >= (err ? 2 : 1))
        return;
    n = kit_put(b, n, "\033_G");
    if (id) {
        n = kit_put(b, n, "i=");
        n = fmt_uint(b, n, (long)id);
    }
    if (KV(k, 'I')) {
        n = kit_put(b, n, id ? ",I=" : "I=");
        n = fmt_uint(b, n, (long)KV(k, 'I'));
    }
    if (KV(k, 'p')) {
        n = kit_put(b, n, ",p=");
        n = fmt_uint(b, n, (long)KV(k, 'p'));
    }
    b[n++] = ';';
    n = kit_put(b, n, err ? err : "OK");
    n = kit_put(b, n, "\033\\");
    reply(t, b, n);
}

/* ---- the picture of the data ---- */

static void kit_src_free(kit_src *s)
{
    if (s->own)
        VT_FREE(s->own);
    if (s->dec && s->dfree)
        s->dfree(s->dec);
    memset(s, 0, sizeof(*s));
}

/* The data as a source; NULL on success, else the error text. */
static const char *kit_load(vt_term *t, const vt_kitty *k, kit_src *s)
{
    int fmt = KV(k, 'f') ? (int)KV(k, 'f') : 32, bpp = fmt == 24 ? 3 : 4;
    int w = (int)KV(k, 's'), h = (int)KV(k, 'v'), z = KV(k, 'o') == 'z';
    long need;
    memset(s, 0, sizeof(*s));
    if (KV(k, 't') && KV(k, 't') != 'd')
        return "ENOTSUP:only direct data (t=d)";
    if ((fmt != 24 && fmt != 32 && fmt != 100) || (KV(k, 'o') && !z) || (z && fmt == 100))
        return "EINVAL:format not supported";
    if (k->full)
        return "ENOMEM:too big";
    if (!k->len)
        return "ENODATA:no data";
    if (fmt == 100) {
        vt_u32 *argb = 0;
        if (!t->decode)
            return "EINVAL:no PNG decoder";
        if (!t->decode(k->raw, k->len, &argb, &w, &h) || !argb)
            return "EBADPNG:not a picture";
        s->dec = argb;
        s->dfree = t->decode_free;
        s->argb = argb;
        s->bpp = 0;
        if (w <= 0 || h <= 0 || (long)w * h > KIT_RAW_MAX / 4) {
            kit_src_free(s);
            return "ENOMEM:too big";
        }
    } else {
        if (w <= 0 || h <= 0)
            return "EINVAL:no size (s=, v=)";
        if ((long)w * h > KIT_RAW_MAX / bpp)
            return "ENOMEM:too big";
        need = (long)w * h * bpp;
        if (z) {
            s->own = (vt_u8 *)VT_MALLOC(need);
            if (!s->own)
                return "ENOMEM:too big";
            if (vt_zinflate(k->raw, k->len, s->own, need) != need) {
                kit_src_free(s);
                return "EINVAL:broken zlib data";
            }
            s->px = s->own;
        } else {
            if (k->len < need)
                return "ENODATA:too little data";
            s->px = k->raw;
        }
        s->bpp = bpp;
    }
    s->w = w;
    s->h = h;
    return 0;
}

/* ---- showing ---- */

/* The part of src the keys ask, as a new image on the screen at the
 * cursor (C=1: the cursor stays), tagged with the picture's ids. NULL, or
 * the error text. */
static const char *kit_show(vt_term *t, const vt_kitty *k, const kit_src *s, vt_u32 id)
{
    int x0 = (int)KV(k, 'x'), y0 = (int)KV(k, 'y'), w = (int)KV(k, 'w'), h = (int)KV(k, 'h');
    int c = (int)KV(k, 'c'), r = (int)KV(k, 'r'), tw, th, npal, slot;
    int cw = t->cell_w > 0 ? t->cell_w : 8, ch = t->cell_h > 0 ? t->cell_h : 16;
    int left = (t->cols - t->cx) * cw;
    vt_u8 *pix;
    vt_u32 pal[256];
    vt_image *im;
    if (x0 < 0 || y0 < 0 || x0 >= s->w || y0 >= s->h)
        x0 = y0 = 0;
    if (w <= 0 || w > s->w - x0)
        w = s->w - x0;
    if (h <= 0 || h > s->h - y0)
        h = s->h - y0;
    if (c > t->cols)
        c = t->cols;
    if (r > 256)
        r = 256;
    tw = c ? c * cw : r ? (int)((long)w * r * ch / h) : w;
    th = r ? r * ch : c ? (int)((long)h * tw / w) : h;
    if (tw < 1)
        tw = 1;
    if (th < 1)
        th = 1;
    if (tw > left && left > 0) { /* cut at the right edge */
        w = (int)((long)w * left / tw);
        tw = left;
    }
    if (tw > VT_SIXEL_MAX_W) {
        w = (int)((long)w * VT_SIXEL_MAX_W / tw);
        tw = VT_SIXEL_MAX_W;
    }
    if (th > VT_SIXEL_MAX_H) {
        h = (int)((long)h * VT_SIXEL_MAX_H / th);
        th = VT_SIXEL_MAX_H;
    }
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;
    if (id && KV(k, 'p')) { /* the same placement again: the new one */
        int i;
        for (i = 0; i < VT_IMG_SLOTS; i++)
            if (t->imgs[i] && t->imgs[i]->pix && t->imgs[i]->kind == 1 && t->imgs[i]->kid == id &&
                t->imgs[i]->pid == KV(k, 'p'))
                image_free(t, i);
    }
    if (!kit_make(t, s, x0, y0, w, h, tw, th, &pix, pal, &npal))
        return "ENOMEM:no room for the picture";
    slot = image_install(t, pix, (long)tw * th, tw, th, npal, pal);
    if (slot < 0)
        return "ENOMEM:no room for the picture";
    im = t->imgs[slot];
    im->pal[0] = vt_default_color(t, 1);
    im->kind = 1;
    im->kid = id;
    im->knum = KV(k, 'I');
    im->pid = KV(k, 'p');
    image_place(t, slot, KV(k, 'C') == 1 ? 2 : 3);
    return 0;
}

/* The whole picture kept under its id (stored, not on the screen). */
static const char *kit_store(vt_term *t, const kit_src *s, vt_u32 id, vt_u32 num)
{
    vt_u8 *pix;
    vt_u32 pal[256];
    int npal, slot, i;
    if ((long)s->w * s->h > VT_IMAGE_MEMORY / 2)
        return "ENOMEM:too big to keep";
    for (i = 0; i < VT_IMG_SLOTS; i++)
        if (t->imgs[i] && t->imgs[i]->pix && t->imgs[i]->kind == 2 && t->imgs[i]->kid == id)
            image_free(t, i); /* the same id again: the new one */
    if (!kit_make(t, s, 0, 0, s->w, s->h, s->w, s->h, &pix, pal, &npal))
        return "ENOMEM:no room for the picture";
    slot = image_install(t, pix, (long)s->w * s->h, s->w, s->h, npal, pal);
    if (slot < 0)
        return "ENOMEM:no room for the picture";
    t->imgs[slot]->kind = 2;
    t->imgs[slot]->kid = id;
    t->imgs[slot]->knum = num;
    return 0;
}

/* the stored picture of id, else the newest of number num */
static int kit_find(const vt_term *t, vt_u32 id, vt_u32 num)
{
    int i, best = -1;
    for (i = 0; i < VT_IMG_SLOTS; i++) {
        const vt_image *im = t->imgs[i];
        if (im && im->pix && im->kind == 2 && (id ? im->kid == id : num && im->knum == num) &&
            (best < 0 || im->serial > t->imgs[best]->serial))
            best = i;
    }
    return best;
}

/* a=d: d=a all on the screen, i by id (and p), n by number, c at the
 * cursor; the capital letters free the stored pictures too. */
static void kit_delete(vt_term *t, const vt_kitty *k)
{
    int d = KV(k, 'd') ? (int)KV(k, 'd') : 'a', i;
    int up = d >= 'A' && d <= 'Z', lo = up ? d + 32 : d;
    vt_u32 id = KV(k, 'i'), num = KV(k, 'I');
    if (lo == 'n') {
        int f = kit_find(t, 0, num);
        id = f >= 0 ? t->imgs[f]->kid : 0;
        if (!id)
            return;
        lo = 'i';
    }
    if (lo == 'c') {
        const vt_line *l = t->scr[t->cy];
        vt_u16 e;
        for (e = l->img; e; e = t->pl[e].next) {
            vt_image *im = place_image(t, &t->pl[e]);
            if (im && im->kind == 1 && t->cx >= t->pl[e].col0 && t->cx < t->pl[e].col0 + image_cols(im)) {
                id = im->kid;
                if (!id)
                    image_free(t, t->pl[e].img);
                break;
            }
        }
        if (!id)
            return;
        lo = 'i';
    }
    if (lo != 'a' && lo != 'i')
        return;
    for (i = 0; i < VT_IMG_SLOTS; i++) {
        vt_image *im = t->imgs[i];
        if (!im || !im->pix || !im->kind)
            continue;
        if (im->kind == 1 && (lo == 'a' || (im->kid == id && (!KV(k, 'p') || im->pid == KV(k, 'p')))))
            image_free(t, i);
        else if (im->kind == 2 && up && (lo == 'a' || im->kid == id))
            image_free(t, i);
    }
}

/* the picture complete (or the command without data): what the keys ask */
static void kitty_run(vt_term *t, vt_kitty *k)
{
    int a = KV(k, 'a') ? (int)KV(k, 'a') : 't';
    vt_u32 id = KV(k, 'i'), num = KV(k, 'I');
    const char *err = 0;
    kit_src s;
    int f;
    if (KV(k, 'U') && (a == 'T' || a == 'p')) {
        err = "EINVAL:no unicode placeholders";
        a = 0;
    }
    if (a == 't' || a == 'T' || a == 'q') {
        err = kit_load(t, k, &s);
        if (!err) {
            if (a == 'q') {
                /* only asked */
            } else {
                if (num && !id)
                    id = 0x40000000u | (++t->kit_next & 0x3FFFFFFF);
                if (id)
                    err = kit_store(t, &s, id, num);
                if (!err && a == 'T')
                    err = kit_show(t, k, &s, id);
            }
            kit_src_free(&s);
        }
    } else if (a == 'p') {
        f = kit_find(t, id, num);
        if (f < 0) {
            err = "ENOENT:no such image";
        } else {
            vt_image *im = t->imgs[f];
            memset(&s, 0, sizeof(s));
            s.px = im->pix;
            s.pal = im->pal;
            s.bpp = 1;
            s.w = im->w;
            s.h = im->h;
            id = im->kid;
            err = kit_show(t, k, &s, id);
        }
    } else if (a == 'd') {
        kit_delete(t, k);
        return; /* kitty does not answer a delete */
    } else if (!err) {
        err = "EINVAL:not supported";
    }
    kit_reply(t, k, id, err);
}

/* ST (or the end of an empty command): the chunk is in. */
static void kitty_end(vt_term *t)
{
    vt_kitty *k = t->kit;
    vt_u32 nv[52];
    int i;
    if (!k)
        return;
    if (k->nacc == 2) {
        kit_data(k, (vt_u8)(k->acc >> 4));
    } else if (k->nacc == 3) {
        kit_data(k, (vt_u8)(k->acc >> 10));
        kit_data(k, (vt_u8)(k->acc >> 2));
    }
    k->acc = 0;
    k->nacc = 0;
    memset(nv, 0, sizeof(nv));
    kit_parse(k->keys, k->nkeys, nv);
    if (k->more) { /* a later chunk brings m= (and q=) only */
        KV(k, 'm') = nv['m' - 'a' + 26];
        if (nv['q' - 'a' + 26])
            KV(k, 'q') = nv['q' - 'a' + 26];
    } else {
        for (i = 0; i < 52; i++)
            k->kv[i] = nv[i];
    }
    k->more = KV(k, 'm') == 1;
    if (k->more)
        return;
    kitty_run(t, k);
    kitty_abort(t);
}
