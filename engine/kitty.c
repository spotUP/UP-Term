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
 * the host's decoder); o=z; m=1 chunks; t=d, t=f, t=t (the host's file reader); i= I= p=;
 * q=1/2; c= r= x= y= X= Y= w= h= C=1; P= Q= H= V= relative placements (at the
 * parent's top left plus H columns, V rows; the cursor stays; deleted with
 * the parent; ENOPARENT, ECYCLE, ETOODEEP past 8 deep); z= draw order (z<0
 * under the text, which stays; z <= -1073741824 also under a non-default
 * cell background). Not supported, each answered with an
 * error: U=1 placeholders, (a=f frames K34, a=a and a=c K35 are supported),
 * t=s (answered EBADF). t=f/t go to the host's file reader. Included by vtengine.c (one translation unit with the engine).
 */
#include "inflate.c"

#define KIT_RAW_MAX (4L * 1024L * 1024L) /* the bytes of one picture, base64 decoded */
#include "png.c"
#include "iterm.c"
#define KIT_EBADF "EBADF:Failed to read image file" /* the one answer for any unreadable medium */
#define KIT_PATH_MAX 1024
#define KIT_KEYS 192                     /* the longest key list */
#define KV(k, c) ((k)->kv[(c) >= 'a' ? (c) - 'a' + 26 : (c) - 'A'])
#define KU(k, c) ((vt_u32)KV(k, c)) /* the ids: unsigned, up to 4294967295 */

typedef struct vt_kitty {
    long kv[52];     /* the keys of the first chunk, a-z then A-Z (z= may be negative) */
    vt_u8 *raw;      /* the data so far */
    long len, cap;
    vt_u32 acc;      /* the base64 quad being read */
    int nacc;
    int nkeys;
    int in_data;     /* past the ';' of this chunk */
    int more;        /* m=1: the picture goes on in the next APC */
    int full;        /* the data passed KIT_RAW_MAX: dropped */
    char iname[64];  /* inline=0: the sanitised file name */
    iterm_args ia;   /* OSC 1337 File=: its arguments (the data goes through the same base64 and cap) */
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
    vt_u8 *fbuf;     /* the bytes a file reader gave (t=f, t=t), freed through dfree */
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
    if (bytes > t->img_budget)
        return 0;
    while (t->img_mem + bytes > t->img_budget) {
        old = -1;
        for (i = 0; i < VT_IMG_SLOTS; i++)
            if (t->imgs[i] && t->imgs[i]->pix && t->imgs[i]->kind == 2 &&
                (old < 0 || t->imgs[i]->ktrans > t->imgs[old]->ktrans ||
                 (t->imgs[i]->ktrans == t->imgs[old]->ktrans && t->imgs[i]->serial < t->imgs[old]->serial)))
                old = i;
        if (old >= 0)
            image_free(t, old);
        else if (!image_evict(t))
            return 0;
    }
    return 1;
}

/* The part (x0, y0, w, h) of src as indices into a palette of tw x th
 * pixels in o (nearest pixel, dithered), its colours packed to the ones in
 * use. The one maker of index pictures: kit_make and the RGB-off conversion. */
static void kit_index(const kit_src *s, int x0, int y0, int w, int h, int tw, int th, vt_u8 *o, vt_u32 *pal,
                      int *npal)
{
    vt_u8 map[256];
    int x, y, n = 1, i;
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
    *npal = n;
}

/* The part (x0, y0, w, h) of src as an indexed image of tw x th pixels.
 * 0 when there is no memory. */
static int kit_make(vt_term *t, const kit_src *s, int x0, int y0, int w, int h, int tw, int th,
                    vt_u8 **pix, vt_u32 *pal, int *npal)
{
    vt_u8 *o;
    long bytes = (long)tw * th;
    if (!kit_room(t, bytes))
        return 0;
    o = (vt_u8 *)VT_MALLOC(bytes);
    if (!o)
        return 0;
    kit_index(s, x0, y0, w, h, tw, th, o, pal, npal);
    *pix = o;
    return 1;
}

/* The RGB twin (vt_set_image_rgb on): the same nearest-pixel scale, 4 bytes a
 * pixel R G B A in memory order with the source's own alpha byte (owner O2:
 * true alpha). *opaque is 1 when no pixel is below 255. 0 when there is no memory. */
static int kit_make_rgb(vt_term *t, const kit_src *s, int x0, int y0, int w, int h, int tw, int th,
                        vt_u8 **pix, int *opaque)
{
    vt_u8 *o, *q;
    int x, y, op = 1;
    long bytes = (long)tw * th * 4;
    if (!kit_room(t, bytes))
        return 0;
    o = (vt_u8 *)VT_MALLOC(bytes);
    if (!o)
        return 0;
    q = o;
    for (y = 0; y < th; y++) {
        long sy = y0 + (long)y * h / th;
        for (x = 0; x < tw; x++, q += 4) {
            long sx = x0 + (long)x * w / tw;
            vt_u32 p = kit_px(s, sy * s->w + sx);
            q[0] = (vt_u8)(p >> 16);
            q[1] = (vt_u8)(p >> 8);
            q[2] = (vt_u8)p;
            q[3] = (vt_u8)(p >> 24);
            if (q[3] != 255)
                op = 0;
        }
    }
    *pix = o;
    *opaque = op;
    t->kit_rgb_made++;
    return 1;
}

/* Install what kit_make or kit_make_rgb made: the format of the terminal decides. */
static int kit_picture(vt_term *t, const kit_src *s, int x0, int y0, int w, int h, int tw, int th)
{
    vt_u8 *pix;
    vt_u32 pal[256];
    int npal, slot, op;
    if (t->image_rgb) {
        if (!kit_make_rgb(t, s, x0, y0, w, h, tw, th, &pix, &op))
            return -1;
        slot = image_install(t, pix, (long)tw * th * 4, tw, th, 0, pal);
        if (slot >= 0) {
            t->imgs[slot]->fmt = 1;
            t->imgs[slot]->opaque = (vt_u8)op;
        }
        return slot;
    }
    if (t->image_copy && kit_room(t, (long)tw * th * 4)) { /* a screen short of pens: the picture's own colours too */
        vt_u8 *cp = (vt_u8 *)VT_MALLOC((long)tw * th * 3);
        if (cp && kit_make(t, s, x0, y0, w, h, tw, th, &pix, pal, &npal)) {
            long i, n = (long)tw * th;
            for (i = 0; i < n; i++) {
                int sx = (int)(i % tw), sy = (int)(i / tw);
                vt_u32 p = kit_px(s, (y0 + (long)sy * h / th) * s->w + x0 + (long)sx * w / tw);
                cp[i * 3] = (vt_u8)(p >> 16);
                cp[i * 3 + 1] = (vt_u8)(p >> 8);
                cp[i * 3 + 2] = (vt_u8)p;
            }
            slot = image_install(t, pix, n, tw, th, npal, pal);
            if (slot >= 0) {
                t->imgs[slot]->rgb = cp;
                t->imgs[slot]->bytes += n * 3;
                t->img_mem += n * 3;
                t->kit_copy_made++;
            } else {
                VT_FREE(cp);
            }
            return slot;
        }
        if (cp) { /* the indices have no room: the plain path below says so */
            VT_FREE(cp);
        }
    }
    if (!kit_make(t, s, x0, y0, w, h, tw, th, &pix, pal, &npal))
        return -1;
    return image_install(t, pix, (long)tw * th, tw, th, npal, pal);
}

/* vt_set_image_rgb off: every RGB picture becomes cube indices in place (T6),
 * at each pixel's own (x, y), the palette packed as kit_make does; img_mem
 * shrinks by what was freed. A picture with no room for its indices (cannot
 * happen: they are a quarter of the size) keeps its RGBA. */
static void kit_to_index(vt_term *t)
{
    int i;
    for (i = 0; i < VT_IMG_SLOTS; i++) {
        vt_image *im = t->imgs[i];
        kit_src s;
        vt_u8 *o;
        if (!im || !im->pix || im->fmt != 1)
            continue;
        o = (vt_u8 *)VT_MALLOC((long)im->w * im->h);
        if (!o)
            continue;
        memset(&s, 0, sizeof(s));
        s.px = im->pix;
        s.bpp = 4;
        s.w = im->w;
        s.h = im->h;
        kit_index(&s, 0, 0, im->w, im->h, im->w, im->h, o, im->pal, &im->npal);
        if (im->anim) { /* K39: the frame canvases go back to cube indices too */
            int f;
            for (f = 0; f < im->anim->n; f++) {
                long n = (long)im->w * im->h, j;
                vt_u8 *c = (vt_u8 *)VT_MALLOC(n);
                if (!c)
                    break; /* cannot happen in practice: a quarter of the size; the loop below frees what it has */
                for (j = 0; j < n; j++) {
                    kit_src fs;
                    memset(&fs, 0, sizeof(fs));
                    fs.px = im->anim->fr[f];
                    fs.bpp = 4;
                    c[j] = kit_quant(kit_px(&fs, j), (int)(j % im->w), (int)(j / im->w));
                }
                VT_FREE(im->anim->fr[f]);
                im->anim->fr[f] = c;
                t->anim_mem -= n * 3;
            }
        }
        VT_FREE(im->pix);
        im->pix = o;
        t->img_mem -= im->bytes - (long)im->w * im->h;
        im->bytes = (long)im->w * im->h;
        im->fmt = 0;
        im->opaque = 0;
        if (im->kind)
            im->pal[0] = vt_default_color(t, 1);
        im->serial = ++t->img_serial; /* the pen cache sees a new picture */
    }
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

static void kit_parse(const char *s, int n, long *kv)
{
    const char *e = s + n;
    while (s < e) {
        int k = *s;
        if (s + 1 < e && s[1] == '=' && ((k >= 'a' && k <= 'z') || (k >= 'A' && k <= 'Z'))) {
            vt_u32 val = 0;
            int neg = 0;
            s += 2;
            if (s < e && *s == '-') {
                neg = 1; /* z<0: under the text */
                s++;
            }
            if (s < e && (*s < '0' || *s > '9'))
                val = (vt_u8)*s++;
            else
                while (s < e && *s >= '0' && *s <= '9')
                    val = val * 10 + (vt_u32)(*s++ - '0');
            kv[k >= 'a' ? k - 'a' + 26 : k - 'A'] = neg ? -(long)val : (long)val;
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

static void kit_reply(vt_term *t, const vt_kitty *k, vt_u32 id, const char *err, int bare)
{
    char b[112];
    int n = 0, q = (int)KV(k, 'q');
    if ((!id && !KV(k, 'I') && !bare) || q >= (err ? 2 : 1))
        return;
    n = kit_put(b, n, "\033_G");
    if (id) {
        n = kit_put(b, n, "i=");
        n = fmt_uint(b, n, (long)id);
    }
    if (KV(k, 'I')) {
        n = kit_put(b, n, id ? ",I=" : "I=");
        n = fmt_uint(b, n, (long)KU(k, 'I'));
    }
    if (KV(k, 'p')) {
        n = kit_put(b, n, ",p=");
        n = fmt_uint(b, n, (long)KU(k, 'p'));
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
    if (s->fbuf && s->dfree)
        s->dfree((vt_u32 *)s->fbuf);
    memset(s, 0, sizeof(*s));
}

/* The data as a source; NULL on success, else the error text. */
static const char *kit_load(vt_term *t, const vt_kitty *k, kit_src *s)
{
    int fmt = KV(k, 'f') ? (int)KV(k, 'f') : 32, bpp = fmt == 24 ? 3 : 4;
    int w = (int)KV(k, 's'), h = (int)KV(k, 'v'), z = KV(k, 'o') == 'z';
    long need;
    const vt_u8 *raw = k->raw;
    long rlen = k->len;
    memset(s, 0, sizeof(*s));
    if (KV(k, 't') == 's')
        return KIT_EBADF; /* D10: no named shared memory on AmigaOS or in ixemul */
    if (KV(k, 't') && KV(k, 't') != 'd' && KV(k, 't') != 'f' && KV(k, 't') != 't')
        return "ENOTSUP:unknown transmission medium";
    if (KV(k, 't') == 'f' || KV(k, 't') == 't') {
        char path[KIT_PATH_MAX];
        long want = KV(k, 'S'), off = KV(k, 'O');
        vt_u8 *buf = 0;
        long n = 0;
        if (!t->file_read || !t->decode_free || k->full || k->len <= 0 || k->len >= KIT_PATH_MAX || off < 0 ||
            want < 0)
            return KIT_EBADF;
        memcpy(path, k->raw, k->len);
        path[k->len] = 0;
        if (want == 0 || want > KIT_RAW_MAX)
            want = KIT_RAW_MAX;
        if (!t->file_read(path, off, want, &buf, &n) || !buf)
            return KIT_EBADF;
        s->fbuf = buf;
        s->dfree = t->decode_free;
        if (n <= 0 || n > KIT_RAW_MAX) {
            kit_src_free(s);
            return KIT_EBADF;
        }
        /* K26: a temp file is removed once it is read; only when the name says it is kitty's */
        if (KV(k, 't') == 't' && t->file_unlink && strstr(path, "tty-graphics-protocol"))
            t->file_unlink(path);
        raw = buf;
        rlen = n;
    }
    if ((fmt != 24 && fmt != 32 && fmt != 100) || (KV(k, 'o') && !z) || (z && fmt == 100))
        return "EINVAL:format not supported";
    if (k->full)
        return "ENOMEM:too big";
    if (!rlen)
        return "ENODATA:no data";
    if (fmt == 100) {
        vt_u32 *argb = 0;
        int own = png_decode(raw, rlen, &argb, &w, &h);
        if (own == 0)
            return "EBADPNG:not a picture"; /* a PNG that is broken: the datatypes cannot do better */
        if (own == -2)
            return "ENOMEM:too big";
        if (own == 1) {
            s->own = (vt_u8 *)argb; /* freed with the source */
            t->kit_png_own++;
        } else {
            if (!t->decode)
                return "EINVAL:no PNG decoder";
            if (!t->decode(raw, rlen, &argb, &w, &h) || !argb)
                return "EBADPNG:not a picture";
            s->dec = argb;
            s->dfree = t->decode_free;
        }
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
            if (vt_zinflate(raw, rlen, s->own, need) != need) {
                kit_src_free(s);
                return "EINVAL:broken zlib data";
            }
            s->px = s->own;
        } else {
            if (rlen < need)
                return "ENODATA:too little data";
            s->px = raw;
        }
        s->bpp = bpp;
    }
    s->w = w;
    s->h = h;
    return 0;
}

/* ---- showing ---- */

/* The slot of the newest placement i=id, p=pid on the screen, or -1. */
static int kit_placement(const vt_term *t, vt_u32 id, vt_u32 pid)
{
    int i, best = -1;
    for (i = 0; i < VT_IMG_SLOTS; i++) {
        const vt_image *im = t->imgs[i];
        if (im && im->pix && im->kind == 1 && im->kid == id && im->pid == pid &&
            (best < 0 || im->serial > t->imgs[best]->serial))
            best = i;
    }
    return best;
}

/* The screen cell of placement `slot`'s top left: its first tile row's
 * line less that row. 0 when none of it is on the screen. */
static int kit_corner(const vt_term *t, int slot, int *col, int *row)
{
    const vt_image *im = t->imgs[slot];
    int y;
    for (y = 0; y < t->rows; y++) {
        vt_u16 i;
        for (i = t->scr[y]->img; i; i = t->pl[i].next)
            if (place_image(t, &t->pl[i]) == im) {
                *col = t->pl[i].col0;
                *row = y - t->pl[i].row;
                return 1;
            }
    }
    return 0;
}

/* A relative placement's parent (P=, Q=): its slot, and the cell the
 * child's top left goes to (the parent's top left plus H= columns, V=
 * rows). -1 and the error text when the parent is missing, the chain
 * would loop back to the placement being made, or runs past 8 deep. */
static int kit_parent(const vt_term *t, const vt_kitty *k, vt_u32 id, int *col, int *row, const char **err)
{
    vt_u32 pk = KU(k, 'P'), pp = KU(k, 'Q'), p = KU(k, 'p');
    int slot, at, n;
    if (id && p && pk == id && pp == p) {
        *err = "ECYCLE:the parent chain loops";
        return -1;
    }
    slot = kit_placement(t, pk, pp);
    if (slot < 0 || !kit_corner(t, slot, col, row)) {
        *err = "ENOPARENT:no such parent placement";
        return -1;
    }
    for (at = slot, n = 0; at >= 0 && t->imgs[at]->pkid && n <= 8; n++) {
        const vt_image *im = t->imgs[at];
        if (id && p && im->pkid == id && im->ppid == p) {
            *err = "ECYCLE:the parent chain loops";
            return -1;
        }
        at = kit_placement(t, im->pkid, im->ppid);
    }
    if (t->imgs[slot]->kdepth + 1 > 8) {
        *err = "ETOODEEP:more than 8 parents deep";
        return -1;
    }
    *col += (int)KV(k, 'H');
    *row += (int)KV(k, 'V');
    if (*col < 0)
        *col = 0; /* left of the screen: its first column */
    if (*col >= t->cols)
        *col = t->cols - 1;
    return slot;
}

/* Image `slot` with its top left at (col, row), the rows off the screen
 * cut; no scrolling, the cursor untouched (a relative placement). */
static void kit_place_at(vt_term *t, int slot, int col, int row)
{
    vt_image *im = t->imgs[slot];
    int rows = (im->py0 + im->h + im->ch - 1) / im->ch, k;
    for (k = 0; k < rows; k++)
        if (row + k >= 0 && row + k < t->rows)
            sixel_place_row(t, slot, row + k, col, k);
    if (im->pix && !im->refs)
        image_free(t, slot); /* nowhere on the screen */
}

/* The part of src the keys ask, as a new image on the screen at the
 * cursor (C=1: the cursor stays), or relative to a parent placement (P=:
 * the cursor stays whatever C= says), tagged with the picture's ids. NULL,
 * or the error text. */
static const char *kit_show(vt_term *t, const vt_kitty *k, const kit_src *s, vt_u32 id)
{
    int x0 = (int)KV(k, 'x'), y0 = (int)KV(k, 'y'), w = (int)KV(k, 'w'), h = (int)KV(k, 'h');
    int c = (int)KV(k, 'c'), r = (int)KV(k, 'r'), tw, th, slot;
    int cw = t->cell_w > 0 ? t->cell_w : 8, ch = t->cell_h > 0 ? t->cell_h : 16;
    int px0 = (int)KV(k, 'X'), py0 = (int)KV(k, 'Y');
    int left, parent = -1, depth = 0, col = t->cx, row = t->cy;
    int virt = KV(k, 'U') == 1; /* K31: a virtual placement, shown by placeholder cells */
    vt_image *im;
    if (KU(k, 'P')) {
        const char *err = 0;
        parent = kit_parent(t, k, id, &col, &row, &err);
        if (parent < 0)
            return err;
        depth = t->imgs[parent]->kdepth + 1; /* kept: the parent may give way below */
    }
    if (px0 < 0)
        px0 = 0;
    if (px0 >= cw)
        px0 = cw - 1; /* an offset past the cell: the cell's last pixel */
    if (py0 < 0)
        py0 = 0;
    if (py0 >= ch)
        py0 = ch - 1;
    left = virt ? 0x7FFFFFFF : (t->cols - col) * cw - px0; /* a virtual one is on no row: no edge */
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
    /* IMAGE-ASPECT-HIRES: a cell count is in screen pixels already; a size taken from the source pixels
     * (no c=, r=) or from one count keeps the picture's look on tall pixels: heights shrink by rx/ry */
    tw = c ? c * cw : r ? (int)((long)w * r * ch / h) : w;
    if (!c && r && t->px_rx && t->px_ry)
        tw = (int)((long)tw * t->px_ry / t->px_rx);
    th = r ? r * ch : c ? vt_aspect_rows((int)((long)h * tw / w), t->px_rx, t->px_ry)
                         : vt_aspect_rows(h, t->px_rx, t->px_ry);
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
            if (t->imgs[i] && t->imgs[i]->pix && t->imgs[i]->kind == (virt ? 3 : 1) && t->imgs[i]->kid == id &&
                t->imgs[i]->pid == KU(k, 'p'))
                image_free(t, i);
    }
    slot = kit_picture(t, s, x0, y0, w, h, tw, th);
    if (slot < 0)
        return "ENOMEM:no room for the picture";
    im = t->imgs[slot];
    im->pal[0] = vt_default_color(t, 1);
    im->kind = virt ? 3 : 1;
    im->kid = id;
    im->knum = KU(k, 'I');
    im->pid = KU(k, 'p');
    if (virt) { /* stored scaled to its cells; nothing on the screen, the cursor stays */
        im->vc = (tw + cw - 1) / cw;
        im->vr = (th + ch - 1) / ch;
        return 0;
    }
    im->sx0 = x0; /* K35: what a frame switch rescales from */
    im->sy0 = y0;
    im->sw = w;
    im->sh = h;
    im->kz = KV(k, 'z');
    im->px0 = px0;
    im->py0 = py0;
    t->kit_last_z = KV(k, 'z');
    if (parent >= 0) {
        im->pkid = KU(k, 'P');
        im->ppid = KU(k, 'Q');
        im->kdepth = depth;
        kit_place_at(t, slot, col, row);
    } else {
        image_place(t, slot, KV(k, 'C') == 1 ? 2 : 3);
    }
    t->kit_placed++;
    return 0;
}

/* The whole picture kept under its id (stored, not on the screen). */
static const char *kit_store(vt_term *t, const kit_src *s, vt_u32 id, vt_u32 num, int trans)
{
    int slot, i;
    if ((long)s->w * s->h * (t->image_rgb ? 4 : 1) > t->img_budget / 2)
        return "ENOMEM:too big to keep";
    for (i = 0; i < VT_IMG_SLOTS; i++)
        if (t->imgs[i] && t->imgs[i]->pix && t->imgs[i]->kind && t->imgs[i]->kid == id)
            image_free(t, i); /* the same id again: the new one, and its placements go */
    slot = kit_picture(t, s, 0, 0, s->w, s->h, s->w, s->h);
    if (slot < 0)
        return "ENOMEM:no room for the picture";
    t->imgs[slot]->kind = 2;
    t->imgs[slot]->kid = id;
    t->imgs[slot]->knum = num;
    t->imgs[slot]->ktrans = (vt_u8)trans; /* N=1: the first to go when room is needed */
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

/* d=p q x y z r (and capitals): the kitty images whose placement is at
 * cell (x, y) (1-based; p), the same with z (q), in column x, in row y, at
 * z-index z, or whose id is x..y (r). The capitals also free the stored
 * picture of each when no placement of it is left. */
static void kit_delete_where(vt_term *t, const vt_kitty *k, int lo, int up)
{
    char hit[VT_IMG_SLOTS];
    long x = KV(k, 'x'), y = KV(k, 'y'), z = KV(k, 'z');
    int i, row;
    memset(hit, 0, sizeof(hit));
    if (lo == 'r' || lo == 'z') {
        for (i = 0; i < VT_IMG_SLOTS; i++) {
            const vt_image *im = t->imgs[i];
            if (im && im->pix && (im->kind == 1 || (im->kind == 3 && lo == 'r')) &&
                (lo == 'z' ? im->kz == z : y >= x && im->kid >= (vt_u32)x && im->kid <= (vt_u32)y))
                hit[i] = 1;
        }
    } else {
        for (row = 0; row < t->rows; row++) {
            vt_u16 e;
            if ((lo == 'p' || lo == 'q' || lo == 'y') && y != row + 1)
                continue;
            for (e = t->scr[row]->img; e; e = t->pl[e].next) {
                const vt_image *im = place_image(t, &t->pl[e]);
                if (!im || im->kind != 1)
                    continue;
                if (lo != 'y' && (x <= t->pl[e].col0 || x > t->pl[e].col0 + image_cols(im)))
                    continue;
                if (lo == 'q' && im->kz != z)
                    continue;
                hit[t->pl[e].img] = 1;
            }
        }
    }
    for (i = 0; i < VT_IMG_SLOTS; i++)
        if (hit[i])
            image_free(t, i);
    if (lo == 'r' && up) {
        for (i = 0; i < VT_IMG_SLOTS; i++)
            if (t->imgs[i] && t->imgs[i]->pix && t->imgs[i]->kind == 2 && y >= x &&
                t->imgs[i]->kid >= (vt_u32)x && t->imgs[i]->kid <= (vt_u32)y)
                image_free(t, i);
    } else if (up) { /* stored pictures left without a placement */
        int j, used, was;
        for (i = 0; i < VT_IMG_SLOTS; i++) {
            const vt_image *st = t->imgs[i];
            if (!st || !st->pix || st->kind != 2 || !st->kid)
                continue;
            used = was = 0;
            for (j = 0; j < VT_IMG_SLOTS; j++) {
                if (!t->imgs[j] || t->imgs[j]->kind != 1 || t->imgs[j]->kid != st->kid)
                    continue;
                if (t->imgs[j]->pix)
                    used = 1;
                else if (hit[j])
                    was = 1; /* a placement of it was just deleted */
            }
            if (!used && was)
                image_free(t, i);
        }
    }
}

/* a=d: d=a all on the screen, i by id (and p), n by number, c at the
 * cursor; the capital letters free the stored pictures too. */
static void kit_delete(vt_term *t, const vt_kitty *k)
{
    int d = KV(k, 'd') ? (int)KV(k, 'd') : 'a', i;
    int up = d >= 'A' && d <= 'Z', lo = up ? d + 32 : d;
    vt_u32 id = KU(k, 'i'), num = KU(k, 'I');
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
    if (lo == 'f') { /* K34: the frames of the picture go; F frees the picture when nothing shows it */
        int f = kit_find(t, id, num);
        if (f < 0)
            return;
        anim_free(t, t->imgs[f]);
        if (up) {
            for (i = 0; i < VT_IMG_SLOTS; i++)
                if (t->imgs[i] && t->imgs[i]->pix && t->imgs[i]->kind != 2 && t->imgs[i]->kid == t->imgs[f]->kid &&
                    t->imgs[i]->kind)
                    return;
            image_free(t, f);
        }
        return;
    }
    if (lo == 'p' || lo == 'q' || lo == 'x' || lo == 'y' || lo == 'z' || lo == 'r') {
        kit_delete_where(t, k, lo, up);
        return;
    }
    if (lo != 'a' && lo != 'i')
        return;
    for (i = 0; i < VT_IMG_SLOTS; i++) {
        vt_image *im = t->imgs[i];
        if (!im || !im->pix || !im->kind)
            continue;
        if (im->kind == 1 && (lo == 'a' || (im->kid == id && (!KV(k, 'p') || im->pid == KU(k, 'p')))))
            image_free(t, i);
        else if (im->kind == 2 && up && (lo == 'a' || im->kid == id))
            image_free(t, i);
        else if (im->kind == 3 && lo == 'i' && im->kid == id && (!KV(k, 'p') || im->pid == KU(k, 'p')))
            image_free(t, i); /* d=a leaves the virtual placements (K31) */
    }
}

/* ---- K34: animation frames (a=f) ---- */

/* the cube index of a stored picture's packed palette entry */
static void kit_root_map(const vt_image *im, vt_u8 *map)
{
    int i, j;
    memset(map, 0, 256);
    for (i = 1; i < im->npal; i++)
        for (j = 1; j < 256; j++)
            if (kit_color(j) == im->pal[i]) {
                map[i] = (vt_u8)j;
                break;
            }
}

/* bytes a pixel of a canvas of this picture: 4 (R G B A, K39) on an RGB picture, else a cube index */
static long kit_cbpp(const vt_image *im)
{
    return im->fmt ? 4 : 1;
}

/* the root's picture as a canvas (cube indices; an RGBA picture is copied as it is), or 0 for no memory */
static vt_u8 *kit_root_canvas(const vt_image *im)
{
    vt_u8 map[256], *c;
    long i, n = (long)im->w * im->h;
    c = (vt_u8 *)VT_MALLOC(n * kit_cbpp(im));
    if (!c)
        return 0;
    if (im->fmt) {
        memcpy(c, im->pix, n * 4);
        return c;
    }
    kit_root_map(im, map);
    for (i = 0; i < n; i++)
        c[i] = map[im->pix[i]];
    return c;
}

/* a new canvas: a copy of frame `base` (1-based) or, for 0, filled with Y= */
static vt_u8 *kit_new_canvas(const vt_image *im, const vt_kitty *k, int base)
{
    long n = (long)im->w * im->h;
    vt_u8 *c = (vt_u8 *)VT_MALLOC(n * kit_cbpp(im));
    if (!c)
        return 0;
    if (base) {
        memcpy(c, im->anim ? im->anim->fr[base - 1] : im->pix, n * kit_cbpp(im));
    } else {
        vt_u32 y = KU(k, 'Y'); /* RGBA */
        if (im->fmt) {
            long i;
            for (i = 0; i < n; i++) {
                c[i * 4] = (vt_u8)(y >> 24);
                c[i * 4 + 1] = (vt_u8)(y >> 16);
                c[i * 4 + 2] = (vt_u8)(y >> 8);
                c[i * 4 + 3] = (vt_u8)y;
            }
        } else {
            vt_u32 p = ((y & 255u) << 24) | (y >> 8);
            memset(c, kit_quant(p, 0, 0), n);
        }
    }
    return c;
}

/* the sent rectangle onto a canvas: X=1 overwrites, else a pixel under half alpha keeps the base (D16) */
static const char *kit_blit(vt_u8 *c, const vt_image *im, const vt_kitty *k, const kit_src *s)
{
    long x0 = KV(k, 'x'), y0 = KV(k, 'y');
    int x, y;
    if (x0 < 0 || y0 < 0 || x0 + s->w > im->w || y0 + s->h > im->h)
        return "EINVAL:frame outside the picture";
    for (y = 0; y < s->h; y++)
        for (x = 0; x < s->w; x++) {
            vt_u32 p = kit_px(s, (long)y * s->w + x);
            if (KV(k, 'X') == 1 || (p >> 24) >= 128) {
                if (im->fmt) { /* K39: the exact bytes, alpha with them */
                    vt_u8 *q = c + ((y0 + y) * im->w + x0 + x) * 4;
                    q[0] = (vt_u8)(p >> 16);
                    q[1] = (vt_u8)(p >> 8);
                    q[2] = (vt_u8)p;
                    q[3] = (vt_u8)(p >> 24);
                } else {
                    c[(y0 + y) * im->w + x0 + x] = kit_quant(p, (int)x0 + x, (int)y0 + y);
                }
            }
        }
    return 0;
}

static void kit_anim_show(vt_term *t, vt_image *st);

/* a=f: a new frame (r= 0) or an edit of frame r= */
static const char *kit_frame(vt_term *t, const vt_kitty *k, const kit_src *s, vt_image *im)
{
    long r = KV(k, 'r'), c = KV(k, 'c'), z = KV(k, 'z'), per = (long)im->w * im->h * kit_cbpp(im);
    int n = im->anim ? im->anim->n : 1;
    vt_u8 *root = 0, *cv;
    const char *err;
    if (r < 0 || r > n || c < 0 || c > n)
        return "ENOENT:no such frame";
    if (!r && n >= VT_ANIM_FRAMES)
        return "ENOSPC:too many frames";
    if (t->anim_mem + (im->anim ? 0 : per) + (r ? 0 : per) > t->anim_budget)
        return "ENOSPC:no room for frames";
    if (!im->anim) {
        root = kit_root_canvas(im);
        if (!root)
            return "ENOMEM:no room for frames";
        if (im->fmt)
            t->kit_rgba_frames++;
    }
    if (r && root) { /* editing the root: its canvas is the one written */
        cv = root;
    } else if (r) {
        cv = im->anim->fr[r - 1];
    } else {
        cv = kit_new_canvas(im, k, (int)c);
        if (cv && c && !im->anim) { /* the base is the root: copy its canvas, not its packed indices */
            memcpy(cv, root, per);
        }
    }
    if (!cv) {
        if (root)
            VT_FREE(root);
        return "ENOMEM:no room for frames";
    }
    if (!r && im->fmt)
        t->kit_rgba_frames++;
    err = kit_blit(cv, im, k, s);
    if (err) {
        if (cv != root && !r)
            VT_FREE(cv);
        if (root)
            VT_FREE(root);
        return err;
    }
    if (root) {
        image_drop_rgb(t, im); /* the frames are indices: the copy would show the first one */
        im->anim = (vt_anim *)VT_MALLOC(sizeof(vt_anim));
        if (!im->anim) {
            VT_FREE(root);
            if (!r)
                VT_FREE(cv);
            return "ENOMEM:no room for frames";
        }
        memset(im->anim, 0, sizeof(vt_anim));
        im->anim->fr[0] = root;
        im->anim->gap[0] = 40;
        im->anim->n = 1;
        im->anim->cur = 1;
        im->anim->state = 1;
        t->anim_mem += per;
    }
    if (!r) {
        im->anim->fr[im->anim->n] = cv;
        im->anim->gap[im->anim->n] = 40;
        r = ++im->anim->n;
        t->anim_mem += per;
    }
    if (z)
        im->anim->gap[r - 1] = z;
    if (r == im->anim->cur)
        kit_anim_show(t, im); /* the frame on view changed */
    return 0;
}

/* ---- K35: animation control (a=a), compose (a=c) and the clock ---- */

/* the placements of stored picture `st` show its current frame again: one w x h copy each, nearest
 * pixel (kit_make without the quantiser), on the fixed cube palette; their rows are marked */
static void kit_anim_show(vt_term *t, vt_image *st)
{
    int i, y, shown = 0;
    const vt_u8 *src;
    if (!st->anim)
        return;
    src = st->anim->fr[st->anim->cur - 1];
    for (i = 0; i < VT_IMG_SLOTS; i++) {
        vt_image *p = t->imgs[i];
        int x, c;
        if (!p || !p->pix || p->kind != 1 || p->kid != st->kid || p->sw <= 0 || p->sx0 + p->sw > st->w ||
            p->sy0 + p->sh > st->h)
            continue;
        if (p->fmt) { /* K39: RGBA to RGBA, no palette; the sent alpha bytes decide opaque */
            long n = (long)p->w * p->h, i;
            const vt_u8 *a;
            for (y = 0; y < p->h; y++) {
                long sy = p->sy0 + (long)y * p->sh / p->h;
                for (x = 0; x < p->w; x++)
                    memcpy(p->pix + ((long)y * p->w + x) * 4,
                           src + (sy * st->w + p->sx0 + (long)x * p->sw / p->w) * 4, 4);
            }
            p->opaque = 1;
            for (a = p->pix, i = 0; i < n; i++)
                if (a[i * 4 + 3] != 255) {
                    p->opaque = 0;
                    break;
                }
            p->serial = ++t->img_serial;
        } else if (p->npal != 256) { /* first switch: the packed palette becomes the cube; the pen cache sees a new serial */
            for (c = 1; c < 256; c++)
                p->pal[c] = kit_color(c);
            p->npal = 256;
            p->serial = ++t->img_serial;
        }
        for (y = 0; y < (p->fmt ? 0 : p->h); y++) {
            long sy = p->sy0 + (long)y * p->sh / p->h;
            for (x = 0; x < p->w; x++)
                p->pix[(long)y * p->w + x] = src[sy * st->w + p->sx0 + (long)x * p->sw / p->w];
        }
        image_mark(t, i);
        shown = 1;
    }
    /* S56-UD2-GLOBE-BAND: the new frame is one picture, a frame of its own unless a ?2026 frame is open (its end
     * counts it): the host composes it off screen and shows it at once. A clock switch outside a frame was drawn in
     * place, the rows' background before their pixels -- a band on an AGA screen, a cell row tall. */
    if (shown && !(t->modes & VT_MODE_SYNC))
        t->sync_frames++;
}

/* make frame `f` (1-based) the one shown */
static void kit_anim_goto(vt_term *t, vt_image *im, int f)
{
    im->anim->cur = f;
    im->anim->elapsed = 0;
    t->kit_switches++;
    kit_anim_show(t, im);
}

/* a=a: s= run state, v= loops, c= current frame, r= with z= a frame's gap */
static const char *kit_control(vt_term *t, const vt_kitty *k, vt_image *im)
{
    vt_anim *an = im->anim;
    long s = KV(k, 's'), v = KV(k, 'v'), c = KV(k, 'c'), r = KV(k, 'r'), z = KV(k, 'z');
    if (!an)
        return "ENOENT:the picture has no frames";
    if (c < 0 || c > an->n || r < 0 || r > an->n)
        return "ENOENT:no such frame";
    if (s < 0 || s > 3)
        return "EINVAL:bad animation state";
    if (r && z)
        an->gap[r - 1] = z;
    if (v > 0)
        an->loops = v;
    if (s == 1) { /* stopping resets the loop count */
        an->state = 1;
        an->done = 0;
    } else if (s) {
        an->state = (int)s;
    }
    if (c && c != an->cur)
        kit_anim_goto(t, im, (int)c);
    return 0;
}

/* a=a done: frame r= onto frame c=, the rectangle w x h from X,Y to x,y; C=1 overwrites, else a clear
 * source pixel keeps the base (D16) */
static const char *kit_compose(vt_term *t, const vt_kitty *k, vt_image *im)
{
    vt_anim *an = im->anim;
    long fr = KV(k, 'r'), to = KV(k, 'c'), sx = KV(k, 'X'), sy = KV(k, 'Y'), dx = KV(k, 'x'), dy = KV(k, 'y');
    long w = KV(k, 'w'), h = KV(k, 'h'), x, y;
    const vt_u8 *a;
    vt_u8 *b;
    int bpp = (int)kit_cbpp(im);
    if (!an || fr < 1 || fr > an->n || to < 1 || to > an->n)
        return "ENOENT:no such frame";
    if (w <= 0)
        w = im->w - (sx > dx ? sx : dx);
    if (h <= 0)
        h = im->h - (sy > dy ? sy : dy);
    if (sx < 0 || sy < 0 || dx < 0 || dy < 0 || w <= 0 || h <= 0 || sx + w > im->w || dx + w > im->w ||
        sy + h > im->h || dy + h > im->h)
        return "EINVAL:rectangle outside the picture";
    if (fr == to && sx < dx + w && dx < sx + w && sy < dy + h && dy < sy + h)
        return "EINVAL:rectangles overlap";
    a = an->fr[fr - 1];
    b = an->fr[to - 1];
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            const vt_u8 *p = a + ((sy + y) * im->w + sx + x) * bpp;
            if ((bpp == 4 ? p[3] >= 128 : *p != 0) || KV(k, 'C') == 1)
                memcpy(b + ((dy + y) * im->w + dx + x) * bpp, p, bpp);
        }
    if (to == an->cur)
        kit_anim_show(t, im);
    return 0;
}

/* the next frame after cur the clock shows: gapless frames are skipped; 0 when the run ends here */
static int kit_anim_next(vt_anim *an)
{
    int f = an->cur, tries;
    for (tries = 0; tries < an->n; tries++) {
        f++;
        if (f > an->n) {
            if (an->state == 2)
                return 0; /* waits for a frame at the end */
            if (an->loops > 1 && ++an->done >= an->loops - 1) {
                an->state = 1; /* v=n played its n-1 loops: stops on the last frame, the count reset */
                an->done = 0;
                return 0;
            }
            f = 1;
        }
        if (an->gap[f - 1] >= 0)
            return f == an->cur ? 0 : f;
    }
    return 0;
}

long vt_anim_tick(vt_term *t, long us)
{
    long due = -1;
    int i;
    if (us < 0)
        us = 0;
    if (t->anim_off)
        return -1;
    for (i = 0; i < VT_IMG_SLOTS; i++) {
        vt_image *im = t->imgs[i];
        vt_anim *an;
        if (!im || !im->pix || im->kind != 2 || !im->anim || im->anim->state == 1)
            continue;
        an = im->anim;
        an->elapsed += us;
        /* kitty scan_active_animations: at most one switch per wake and image (kit_anim_next skips gapless frames),
         * and the switch time is "now" (current_frame_shown_at), so lateness is dropped, never caught up */
        if (an->state != 1 && an->gap[an->cur - 1] >= 0 && an->elapsed >= an->gap[an->cur - 1] * 1000L) {
            int f = kit_anim_next(an);
            if (!f) {
                if (an->state != 1)
                    an->elapsed = an->gap[an->cur - 1] * 1000L; /* waits, ready when a frame arrives */
                else
                    an->elapsed = 0;
            } else {
                kit_anim_goto(t, im, f);
                an->elapsed = 0;
            }
        }
        if (an->state != 1 && an->cur < an->n + (an->state == 3)) {
            long d = an->gap[an->cur - 1] * 1000L - an->elapsed;
            if (d < 0)
                d = 0;
            if (due < 0 || d < due)
                due = d;
        }
    }
    return due;
}

void vt_set_animation(vt_term *t, int on)
{
    if (t)
        t->anim_off = on ? 0 : 1;
}

unsigned long vt_kitty_frame_switches(const vt_term *t)
{
    return t->kit_switches;
}

/* the picture complete (or the command without data): what the keys ask */
static void kitty_run(vt_term *t, vt_kitty *k)
{
    int a = KV(k, 'a') ? (int)KV(k, 'a') : 't';
    vt_u32 id = KU(k, 'i'), num = KU(k, 'I');
    const char *err = 0;
    kit_src s;
    int f;
    int bare = 0;
    if (id && num) {
        err = "EINVAL:i and I together";
        a = 0;
    } else if (a == 'p' && !id && !num) {
        err = "EINVAL:no image id";
        bare = 1; /* the one error that is answered without an id */
        a = 0;
    } else if (KV(k, 'U') && KU(k, 'P') && (a == 'T' || a == 'p')) {
        err = "EINVAL:a virtual placement has no parent";
        a = 0;
    }
    if (a == 't' || a == 'T' || a == 'q') {
        err = kit_load(t, k, &s);
        if (err)
            kit_src_free(&s); /* a file's bytes read before a later refusal */
        if (!err) {
            if (a == 'q') {
                /* only asked */
            } else {
                if (num && !id)
                    id = 0x40000000u | (++t->kit_next & 0x3FFFFFFF);
                if (id)
                    err = kit_store(t, &s, id, num, KV(k, 'N') == 1);
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
            vt_u32 cube[256];
            int ci;
            memset(&s, 0, sizeof(s));
            s.px = im->pix;
            s.pal = im->pal;
            if (im->anim) { /* the picture shown is the current frame (the root's pix is not edited, K34) */
                for (ci = 0; ci < 256; ci++)
                    cube[ci] = kit_color(ci);
                s.px = im->anim->fr[im->anim->cur - 1];
                s.pal = cube;
            }
            s.bpp = im->fmt ? 4 : 1; /* an RGBA root reads back through kit_px as it is */
            s.w = im->w;
            s.h = im->h;
            id = im->kid;
            err = kit_show(t, k, &s, id);
        }
    } else if (a == 'f') {
        f = kit_find(t, id, num);
        if (f < 0) {
            err = "ENOENT:no such image";
        } else {
            err = kit_load(t, k, &s);
            if (!err)
                err = kit_frame(t, k, &s, t->imgs[f]);
            kit_src_free(&s);
            id = t->imgs[f]->kid;
        }
    } else if (a == 'a' || a == 'c') {
        f = kit_find(t, id, num);
        if (f < 0) {
            err = "ENOENT:no such image";
        } else {
            err = a == 'a' ? kit_control(t, k, t->imgs[f]) : kit_compose(t, k, t->imgs[f]);
            id = t->imgs[f]->kid;
        }
    } else if (a == 'd') {
        kit_delete(t, k);
        return; /* kitty does not answer a delete */
    } else if (!err) {
        err = "EINVAL:not supported";
    }
    kit_reply(t, k, id, err, bare);
}

/* ST (or the end of an empty command): the chunk is in. */
static void kitty_end(vt_term *t)
{
    vt_kitty *k = t->kit;
    long nv[52];
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
    if (k->more && nv['a' - 'a' + 26] == 'd') { /* a delete ends the open upload */
        if (k->raw)
            VT_FREE(k->raw);
        memset(k, 0, sizeof(*k));
    }
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

/* ---- iTerm2 inline images (OSC 1337 ; File=...) -------------------------------
 * The header (str, up to the ':') was parsed by iterm_parse; the base64 after
 * it streams through the kitty chunk's own decoder, buffer and cap (kit_b64,
 * kit_data, KIT_RAW_MAX), and ST / BEL decodes it as a PNG (or through the host
 * decoder) and places it with kit_show, as a kitty a=T would. inline=0 is a
 * download: its bytes are swallowed and nothing is kept. */

static void iterm_begin(vt_term *t, const char *hdr, int n)
{
    iterm_args a;
    vt_kitty *k;
    kitty_abort(t); /* a half-sent kitty upload does not survive an OSC */
    t->osc52 = 4;
    iterm_parse(hdr, n, &a);
    if (!a.inl && !t->dl_sink && !t->file_write)
        return; /* nobody to save it: bytes swallowed, t->kit stays 0 */
    k = (vt_kitty *)VT_MALLOC(sizeof(vt_kitty));
    if (!k)
        return;
    memset(k, 0, sizeof(*k));
    k->ia = a;
    if (!a.inl)
        iterm_name(t->iname, t->iname_n, k->iname, (int)sizeof(k->iname));
    t->kit = k;
}

static void iterm_byte(vt_term *t, vt_u32 c)
{
    if (t->kit && c < 0x80)
        kit_b64(t->kit, (vt_u8)c);
}

static void iterm_end(vt_term *t)
{
    vt_kitty *k = t->kit;
    kit_src s;
    int cw, ch, c, r;
    if (!k)
        return;
    if (!k->ia.inl) { /* inline=0: a file to keep (O5), under the 4 MB cap (O10) */
        char path[160];
        if (k->nacc == 2) {
            kit_data(k, (vt_u8)(k->acc >> 4));
        } else if (k->nacc == 3) {
            kit_data(k, (vt_u8)(k->acc >> 10));
            kit_data(k, (vt_u8)(k->acc >> 2));
        }
        if (k->full) {
            if (t->cb.notify)
                t->cb.notify(t->user, "iTerm2", "download over 4 MB dropped");
        } else if (k->len > 0 && t->dl_sink) {
            if (!t->dl_sink(vt_download_host(t), k->iname, k->raw, k->len) && t->cb.notify)
                t->cb.notify(t->user, "iTerm2", "download could not be saved");
        } else if (k->len > 0 && t->file_write) {
            iterm_path(path, (int)sizeof(path), t->dl_host, k->iname);
            if (!t->file_write(path, k->raw, k->len) && t->cb.notify)
                t->cb.notify(t->user, "iTerm2", "download could not be saved");
        }
        kitty_abort(t);
        return;
    }
    if (k->nacc == 2) {
        kit_data(k, (vt_u8)(k->acc >> 4));
    } else if (k->nacc == 3) {
        kit_data(k, (vt_u8)(k->acc >> 10));
        kit_data(k, (vt_u8)(k->acc >> 2));
    }
    k->acc = 0;
    k->nacc = 0;
    KV(k, 'f') = 100;
    if (!k->full && k->len > 0 && !kit_load(t, k, &s)) {
        cw = t->cell_w > 0 ? t->cell_w : 8;
        ch = t->cell_h > 0 ? t->cell_h : 16;
        c = iterm_cells(k->ia.wu, k->ia.wv, cw, t->cols);
        r = iterm_cells(k->ia.hu, k->ia.hv, ch, 256);
        if (c && r && k->ia.keep) { /* both given: the picture fits the box, one count decides */
            if ((long)s.w * r * ch > (long)s.h * c * cw)
                r = 0;
            else
                c = 0;
        }
        KV(k, 'c') = c;
        KV(k, 'r') = r;
        KV(k, 'C') = k->ia.nomove ? 1 : 0;
        if (!kit_show(t, k, &s, 0))
            t->iterm_placed++;
        kit_src_free(&s);
    }
    kitty_abort(t); /* too big, unreadable or placed: the bytes go in every case */
}

unsigned long vt_iterm_placed(const vt_term *t)
{
    return t->iterm_placed;
}

unsigned long vt_kitty_placeholders(const vt_term *t)
{
    return t->kit_ph;
}

unsigned long vt_kitty_frames(const vt_term *t)
{
    unsigned long n = 0;
    int i;
    for (i = 0; i < VT_IMG_SLOTS; i++)
        if (t->imgs[i] && t->imgs[i]->pix && t->imgs[i]->anim)
            n += (unsigned long)t->imgs[i]->anim->n;
    return n;
}

long vt_kitty_frame_px(const vt_term *t, unsigned long id, int frame, int x, int y)
{
    int f = kit_find(t, (vt_u32)id, 0);
    const vt_image *im;
    if (f < 0)
        return -1;
    im = t->imgs[f];
    if (!im->anim || frame < 1 || frame > im->anim->n || x < 0 || y < 0 || x >= im->w || y >= im->h)
        return -1;
    if (im->fmt) { /* K39: 0xAARRGGBB (on a 32-bit long an opaque white reads as -1) */
        const vt_u8 *q = im->anim->fr[frame - 1] + ((long)y * im->w + x) * 4;
        return (long)(((vt_u32)q[3] << 24) | ((vt_u32)q[0] << 16) | ((vt_u32)q[1] << 8) | q[2]);
    }
    return im->anim->fr[frame - 1][(long)y * im->w + x];
}

unsigned long vt_kitty_placed(const vt_term *t)
{
    return t->kit_placed;
}

long vt_kitty_last_z(const vt_term *t)
{
    return t->kit_last_z;
}

unsigned long vt_kitty_png_own(const vt_term *t)
{
    return t->kit_png_own;
}

/* ---- unicode placeholders (U=1, K31) ------------------------------------- */

#define KIT_PLACEHOLDER 0x10EEEEUL

/* kitty's rowcolumn-diacritics.txt (gen/rowcolumn-diacritics.txt, 297 code
 * points, counted from the file twice: data lines and distinct values), in
 * file order, which is ascending: a mark's index is its row, column or id
 * high byte number. */
static const vt_u32 kit_diacritics[297] = {
    0x0305, 0x030D, 0x030E, 0x0310, 0x0312, 0x033D, 0x033E, 0x033F,
    0x0346, 0x034A, 0x034B, 0x034C, 0x0350, 0x0351, 0x0352, 0x0357,
    0x035B, 0x0363, 0x0364, 0x0365, 0x0366, 0x0367, 0x0368, 0x0369,
    0x036A, 0x036B, 0x036C, 0x036D, 0x036E, 0x036F, 0x0483, 0x0484,
    0x0485, 0x0486, 0x0487, 0x0592, 0x0593, 0x0594, 0x0595, 0x0597,
    0x0598, 0x0599, 0x059C, 0x059D, 0x059E, 0x059F, 0x05A0, 0x05A1,
    0x05A8, 0x05A9, 0x05AB, 0x05AC, 0x05AF, 0x05C4, 0x0610, 0x0611,
    0x0612, 0x0613, 0x0614, 0x0615, 0x0616, 0x0617, 0x0657, 0x0658,
    0x0659, 0x065A, 0x065B, 0x065D, 0x065E, 0x06D6, 0x06D7, 0x06D8,
    0x06D9, 0x06DA, 0x06DB, 0x06DC, 0x06DF, 0x06E0, 0x06E1, 0x06E2,
    0x06E4, 0x06E7, 0x06E8, 0x06EB, 0x06EC, 0x0730, 0x0732, 0x0733,
    0x0735, 0x0736, 0x073A, 0x073D, 0x073F, 0x0740, 0x0741, 0x0743,
    0x0745, 0x0747, 0x0749, 0x074A, 0x07EB, 0x07EC, 0x07ED, 0x07EE,
    0x07EF, 0x07F0, 0x07F1, 0x07F3, 0x0816, 0x0817, 0x0818, 0x0819,
    0x081B, 0x081C, 0x081D, 0x081E, 0x081F, 0x0820, 0x0821, 0x0822,
    0x0823, 0x0825, 0x0826, 0x0827, 0x0829, 0x082A, 0x082B, 0x082C,
    0x082D, 0x0951, 0x0953, 0x0954, 0x0F82, 0x0F83, 0x0F86, 0x0F87,
    0x135D, 0x135E, 0x135F, 0x17DD, 0x193A, 0x1A17, 0x1A75, 0x1A76,
    0x1A77, 0x1A78, 0x1A79, 0x1A7A, 0x1A7B, 0x1A7C, 0x1B6B, 0x1B6D,
    0x1B6E, 0x1B6F, 0x1B70, 0x1B71, 0x1B72, 0x1B73, 0x1CD0, 0x1CD1,
    0x1CD2, 0x1CDA, 0x1CDB, 0x1CE0, 0x1DC0, 0x1DC1, 0x1DC3, 0x1DC4,
    0x1DC5, 0x1DC6, 0x1DC7, 0x1DC8, 0x1DC9, 0x1DCB, 0x1DCC, 0x1DD1,
    0x1DD2, 0x1DD3, 0x1DD4, 0x1DD5, 0x1DD6, 0x1DD7, 0x1DD8, 0x1DD9,
    0x1DDA, 0x1DDB, 0x1DDC, 0x1DDD, 0x1DDE, 0x1DDF, 0x1DE0, 0x1DE1,
    0x1DE2, 0x1DE3, 0x1DE4, 0x1DE5, 0x1DE6, 0x1DFE, 0x20D0, 0x20D1,
    0x20D4, 0x20D5, 0x20D6, 0x20D7, 0x20DB, 0x20DC, 0x20E1, 0x20E7,
    0x20E9, 0x20F0, 0x2CEF, 0x2CF0, 0x2CF1, 0x2DE0, 0x2DE1, 0x2DE2,
    0x2DE3, 0x2DE4, 0x2DE5, 0x2DE6, 0x2DE7, 0x2DE8, 0x2DE9, 0x2DEA,
    0x2DEB, 0x2DEC, 0x2DED, 0x2DEE, 0x2DEF, 0x2DF0, 0x2DF1, 0x2DF2,
    0x2DF3, 0x2DF4, 0x2DF5, 0x2DF6, 0x2DF7, 0x2DF8, 0x2DF9, 0x2DFA,
    0x2DFB, 0x2DFC, 0x2DFD, 0x2DFE, 0x2DFF, 0xA66F, 0xA67C, 0xA67D,
    0xA6F0, 0xA6F1, 0xA8E0, 0xA8E1, 0xA8E2, 0xA8E3, 0xA8E4, 0xA8E5,
    0xA8E6, 0xA8E7, 0xA8E8, 0xA8E9, 0xA8EA, 0xA8EB, 0xA8EC, 0xA8ED,
    0xA8EE, 0xA8EF, 0xA8F0, 0xA8F1, 0xAAB0, 0xAAB2, 0xAAB3, 0xAAB7,
    0xAAB8, 0xAABE, 0xAABF, 0xAAC1, 0xFE20, 0xFE21, 0xFE22, 0xFE23,
    0xFE24, 0xFE25, 0xFE26, 0x10A0F, 0x10A38, 0x1D185, 0x1D186, 0x1D187,
    0x1D188, 0x1D189, 0x1D1AA, 0x1D1AB, 0x1D1AC, 0x1D1AD, 0x1D242, 0x1D243,
    0x1D244,
};

/* a mark's number, -1 when it is none of the 297 */
static int kit_mark(vt_u32 cp)
{
    int lo = 0, hi = (int)(sizeof(kit_diacritics) / sizeof(kit_diacritics[0])) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (kit_diacritics[mid] == cp)
            return mid;
        if (kit_diacritics[mid] < cp)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

/* a colour as the id it names: a 256-colour index, or R<<16|G<<8|B */
static vt_u32 kit_color_id(vt_color c)
{
    if (c & VT_COLOR_RGB)
        return VT_RGB_OF(c);
    return c < 256 ? (vt_u32)c : 0;
}

/* One placeholder cell's own marks and colours: 1 when it is one. row,
 * col, hi are -1 for a mark it does not carry. */
static int kit_ph_cell(const vt_term *t, const vt_cell *c, int *row, int *col, int *hi, vt_u32 *id, vt_u32 *pid)
{
    vt_u32 cp[VT_CLUSTER_CPS];
    int n, i, m;
    int *slot[3];
    if (c->ch < VT_CLUSTER_FIRST || c->ch > VT_CLUSTER_LAST)
        return 0;
    n = vt_cell_text(t, c, cp);
    if (n < 1 || cp[0] != KIT_PLACEHOLDER)
        return 0;
    slot[0] = row;
    slot[1] = col;
    slot[2] = hi;
    *row = *col = *hi = -1;
    for (i = 1; i < n && i <= 3; i++) {
        m = kit_mark(cp[i]);
        if (m < 0)
            break;
        *slot[i - 1] = m;
    }
    *id = kit_color_id(c->fg);
    *pid = kit_color_id(vt_cell_underline_color(t, c));
    return 1;
}

int vt_cell_placeholder(const vt_term *t, int row, int col, vt_image_view *v, int *tile_col, int *tile_row)
{
    int n, s, x, r = 0, cc = 0, h = 0, best = -1, i;
    const vt_cell *cells;
    vt_u32 id = 0, pid = 0;
    const vt_image *im;
    if (row < 0 || row >= t->rows || col < 0 || col >= t->cols)
        return 0;
    cells = vt_row(t, row, &n);
    if (!cells || col >= n)
        return 0;
    {
        int r0, c0, h0;
        vt_u32 i0, p0;
        if (!kit_ph_cell(t, &cells[col], &r0, &c0, &h0, &i0, &p0))
            return 0;
    }
    /* back to the first cell of the run that shares this cell's colours, then
     * forward, each cell taking what it lacks from the one before (a cell
     * the run starts on lacks nothing it could take: it gets 0) */
    for (s = col; s > 0; s--) {
        int a, b, c2;
        vt_u32 i1, p1, i2, p2;
        if (!kit_ph_cell(t, &cells[s - 1], &a, &b, &c2, &i1, &p1))
            break;
        kit_ph_cell(t, &cells[col], &a, &b, &c2, &i2, &p2);
        if (i1 != i2 || p1 != p2)
            break;
    }
    for (x = s; x <= col; x++) {
        int mr, mc, mh;
        kit_ph_cell(t, &cells[x], &mr, &mc, &mh, &id, &pid);
        if (x == s) {
            r = mr < 0 ? 0 : mr;
            cc = mc < 0 ? 0 : mc;
            h = mh < 0 ? 0 : mh;
        } else {
            int nr = mr < 0 ? r : mr;
            cc = mc >= 0 ? mc : (nr == r ? cc + 1 : 0);
            r = nr;
            h = mh < 0 ? h : mh;
        }
    }
    id |= (vt_u32)h << 24;
    if (!id)
        return 0;
    for (i = 0; i < VT_IMG_SLOTS; i++) {
        im = t->imgs[i];
        if (im && im->pix && im->kind == 3 && im->kid == id && (!pid || im->pid == pid) &&
            (best < 0 || t->imgs[best]->serial < im->serial))
            best = i;
    }
    if (best < 0)
        return 0;
    im = t->imgs[best];
    if (cc >= im->vc || r >= im->vr)
        return 0;
    v->pix = im->pix;
    v->w = im->w;
    v->h = im->h;
    v->pal = im->pal;
    v->npal = im->npal;
    v->fmt = im->fmt;
    v->opaque = im->opaque;
    v->rgb = im->rgb;
    v->kitty = 1;
    v->bg0 = 0;
    v->cw = im->cw;
    v->ch = im->ch;
    v->col0 = 0;
    v->px0 = v->py0 = v->py = 0;
    v->serial = im->serial;
    v->z = 0;
    *tile_col = cc;
    *tile_row = r;
    ((vt_term *)t)->kit_ph++; /* a test sentinel on an otherwise read-only query */
    return 1;
}
