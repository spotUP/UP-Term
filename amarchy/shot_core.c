/* shot_core: IFF ILBM writer, see shot_core.h. */
#include <stdlib.h>
#include <string.h>
#include "shot_core.h"

long shot_pack_max(long n)
{
    return n + n / 128 + 2;
}

long shot_pack(const unsigned char *in, long n, unsigned char *out)
{
    long i = 0, o = 0, r, j;

    while (i < n) {
        r = 1;
        while (i + r < n && r < 128 && in[i + r] == in[i])
            r++;
        if (r >= 3) {                                   /* a run: 257 - r, one byte */
            out[o++] = (unsigned char)(257 - r);
            out[o++] = in[i];
            i += r;
            continue;
        }
        j = i + 1;                                      /* literals up to the next run of 3 */
        while (j < n && j - i < 128 && !(j + 2 < n && in[j] == in[j + 1] && in[j] == in[j + 2]))
            j++;
        out[o++] = (unsigned char)(j - i - 1);
        memcpy(out + o, in + i, (size_t)(j - i));
        o += j - i;
        i = j;
    }
    return o;
}

void shot_planes(const unsigned char *pix, long width, int depth, unsigned char *out)
{
    long rb = ((width + 15) / 16) * 2, x;
    int p;

    memset(out, 0, (size_t)(rb * depth));
    for (p = 0; p < depth; p++) {
        unsigned char *pl = out + p * rb;
        for (x = 0; x < width; x++)
            if ((pix[x] >> p) & 1)
                pl[x >> 3] |= (unsigned char)(0x80 >> (x & 7));
    }
}

/* One ARGB row (4 bytes per pixel: A, R, G, B) to 24 planes: R bits 0..7 are planes 0..7, G 8..15, B 16..23. */
void shot_planes_argb(const unsigned char *argb, long width, unsigned char *out)
{
    long rb = ((width + 15) / 16) * 2, x;
    int c, b;

    memset(out, 0, (size_t)(rb * 24));
    for (x = 0; x < width; x++)
        for (c = 0; c < 3; c++) {
            unsigned char v = argb[4 * x + 1 + c];
            for (b = 0; b < 8; b++)
                if ((v >> b) & 1)
                    out[(c * 8 + b) * rb + (x >> 3)] |= (unsigned char)(0x80 >> (x & 7));
        }
}

static void put16(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

static void put32(unsigned char *p, unsigned long v)
{
    put16(p, v >> 16);
    put16(p + 2, v & 0xFFFF);
}

static int emit(shot_writer *w, const void *buf, long n)
{
    if (w->failed)
        return -1;
    if (w->sink->write(w->sink->ud, buf, n) != 0) {
        w->failed = 1;
        return -1;
    }
    w->pos += n;
    return 0;
}

static void release(shot_writer *w)
{
    free(w->planes);
    free(w->packed);
    w->planes = w->packed = NULL;
}

static int begin(shot_writer *w, const shot_sink *sink, long width, long height, int depth,
                 const unsigned char *cmap, unsigned long camg)
{
    unsigned char h[12 + 8 + 20], c[8], g[12], b[8];
    long ncol = 1L << (depth > 0 && depth <= 8 ? depth : 1);

    memset(w, 0, sizeof *w);
    w->sink = sink;
    w->deep = depth == 24;
    if (width < 1 || width > 65535 || height < 1 || height > 65535 || !(depth == 24 || (depth >= 1 && depth <= 8))
        || (depth != 24 && cmap == NULL))
        return -1;
    w->width = width;
    w->height = height;
    w->depth = depth;
    w->rowbytes = ((width + 15) / 16) * 2;
    w->planes = (unsigned char *)malloc((size_t)(w->rowbytes * depth));
    w->packed = (unsigned char *)malloc((size_t)shot_pack_max(w->rowbytes));
    if (w->planes == NULL || w->packed == NULL) {
        release(w);
        return -1;
    }
    memcpy(h, "FORM", 4);
    put32(h + 4, 0);                                    /* patched by shot_end */
    memcpy(h + 8, "ILBM", 4);
    memcpy(h + 12, "BMHD", 4);
    put32(h + 16, 20);
    put16(h + 20, (unsigned long)width);
    put16(h + 22, (unsigned long)height);
    put16(h + 24, 0);                                   /* x, y */
    put16(h + 26, 0);
    h[28] = (unsigned char)depth;
    h[29] = 0;                                          /* masking: none */
    h[30] = 1;                                          /* compression: ByteRun1 */
    h[31] = 0;
    put16(h + 32, 0);                                   /* transparent colour */
    h[34] = 10;                                         /* aspect 10:11, a PAL/NTSC pixel */
    h[35] = 11;
    put16(h + 36, (unsigned long)width);                /* page size */
    put16(h + 38, (unsigned long)height);
    if (emit(w, h, 12 + 8 + 20) != 0)
        goto fail;
    if (depth != 24) {                                  /* a deep picture has no CMAP */
        memcpy(c, "CMAP", 4);
        put32(c + 4, (unsigned long)(3 * ncol));
        if (emit(w, c, 8) != 0 || emit(w, cmap, 3 * ncol) != 0)
            goto fail;
    }
    if (camg != 0) {
        memcpy(g, "CAMG", 4);
        put32(g + 4, 4);
        put32(g + 8, camg);
        if (emit(w, g, 12) != 0)
            goto fail;
    }
    memcpy(b, "BODY", 4);
    put32(b + 4, 0);                                    /* patched by shot_end */
    w->body_size_at = w->pos + 4;
    if (emit(w, b, 8) != 0)
        goto fail;
    return 0;
fail:
    release(w);
    return -1;
}

int shot_begin(shot_writer *w, const shot_sink *sink, long width, long height, int depth,
               const unsigned char *cmap, unsigned long camg)
{
    if (depth < 1 || depth > 8) {
        memset(w, 0, sizeof *w);
        return -1;
    }
    return begin(w, sink, width, height, depth, cmap, camg);
}

int shot_begin_deep(shot_writer *w, const shot_sink *sink, long width, long height, unsigned long camg)
{
    return begin(w, sink, width, height, 24, NULL, camg);
}

static int put_row(shot_writer *w)
{
    long rb = w->rowbytes, n;
    int p;

    for (p = 0; p < w->depth; p++) {
        n = shot_pack(w->planes + p * rb, rb, w->packed);
        if (emit(w, w->packed, n) != 0)
            return -1;
        w->body_len += n;
    }
    w->rows_done++;
    return 0;
}

int shot_row(shot_writer *w, const unsigned char *pix)
{
    if (w->failed || w->deep || w->rows_done >= w->height)
        return -1;
    shot_planes(pix, w->width, w->depth, w->planes);
    return put_row(w);
}

int shot_row_argb(shot_writer *w, const unsigned char *argb)
{
    if (w->failed || !w->deep || w->rows_done >= w->height)
        return -1;
    shot_planes_argb(argb, w->width, w->planes);
    return put_row(w);
}

int shot_end(shot_writer *w)
{
    static const unsigned char zero = 0;
    int ok = !w->failed && w->planes != NULL && w->rows_done == w->height;

    release(w);
    if (!ok)
        return -1;
    if (w->body_len & 1) {                              /* chunk data is padded, the size is not */
        if (emit(w, &zero, 1) != 0)
            return -1;
    }
    if (w->sink->patch32(w->sink->ud, w->body_size_at, (unsigned long)w->body_len) != 0)
        return -1;
    if (w->sink->patch32(w->sink->ud, 4, (unsigned long)(w->pos - 8)) != 0)
        return -1;
    return 0;
}

void shot_stamp_name(long days, long minute, long tick, char *out)
{
    long z = days + 2922L + 719468L;          /* days since 0000-03-01; 1978-01-01 is 2922 days after 1970-01-01 */
    long era = z / 146097L, doe = z - era * 146097L;
    long yoe = (doe - doe / 1460L + doe / 36524L - doe / 146096L) / 365L;
    long y = yoe + era * 400L, doy = doe - (365L * yoe + yoe / 4L - yoe / 100L);
    long mp = (5L * doy + 2L) / 153L, d = doy - (153L * mp + 2L) / 5L + 1L, m = mp < 10 ? mp + 3 : mp - 9;
    long sec = tick / 50L, v[6];
    int i, k = 0;
    static const char pre[] = "shot-";
    if (m <= 2)
        y++;
    v[0] = y; v[1] = m; v[2] = d; v[3] = minute / 60L; v[4] = minute % 60L; v[5] = sec;
    for (i = 0; pre[i]; i++)
        out[k++] = pre[i];
    for (i = 0; i < 6; i++) {
        long n = v[i], dig = i == 0 ? 1000L : 10L;
        for (; dig > 0; dig /= 10) {
            out[k++] = (char)('0' + (n / dig) % 10);
        }
        if (i == 2)
            out[k++] = '-';
    }
    out[k++] = '.'; out[k++] = 'i'; out[k++] = 'f'; out[k++] = 'f'; out[k] = 0;
}
