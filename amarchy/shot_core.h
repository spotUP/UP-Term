/* shot_core: the screenshot file writer, portable C89 (host-tested by the vttest_host suite "shot_core";
 * the screen read is shot.c). A picture is written as IFF ILBM: FORM, BMHD, CMAP, CAMG, BODY. Rows are
 * streamed through a sink as they are read, so there is no whole-image buffer; the FORM and BODY sizes
 * are written as 0 first and patched by shot_end() through sink->patch32 (a file sink Seeks; a memory
 * sink pokes). Input rows are chunky: one byte per pixel, the colour register number, which is what
 * ReadPixelArray8 returns. ByteRun1 packed, depth 1..8 (or 24 deep, below). */
#ifndef SHOT_CORE_H
#define SHOT_CORE_H

typedef struct shot_sink {
    void *ud;
    /* append n bytes; 0 = ok, nonzero = failed */
    int (*write)(void *ud, const void *buf, long n);
    /* overwrite the big-endian 32-bit value at byte offset from the start of the file; 0 = ok */
    int (*patch32)(void *ud, long offset, unsigned long value);
} shot_sink;

typedef struct shot_writer {
    const shot_sink *sink;
    long width, height, rows_done;
    int depth;
    long rowbytes;               /* one plane of one row: ((width + 15) / 16) * 2 */
    long pos;                    /* bytes written so far */
    long body_size_at;           /* offset of the BODY chunk's size field */
    long body_len;               /* BODY data bytes written */
    int deep;                    /* 24 planes, no CMAP, rows come from shot_row_argb */
    int failed;                  /* a sink call failed; later calls do nothing */
    unsigned char *planes;       /* depth * rowbytes */
    unsigned char *packed;       /* worst case for one plane row */
} shot_writer;

/* ByteRun1: pack n bytes (n >= 1) into out, which holds at least shot_pack_max(n). Returns the length. */
long shot_pack_max(long n);
long shot_pack(const unsigned char *in, long n, unsigned char *out);

/* One chunky row of width pixels to depth bitplanes, plane p at out + p * rowbytes. */
void shot_planes(const unsigned char *pix, long width, int depth, unsigned char *out);

/* Write the headers. cmap holds 3 bytes (R, G, B 0..255) for each of 1 << depth registers. camg is the
 * screen's ModeID (0 = no CAMG chunk). Returns 0, or -1 for a bad size/depth, no memory or a sink failure. */
int shot_begin(shot_writer *w, const shot_sink *sink, long width, long height, int depth,
               const unsigned char *cmap, unsigned long camg);
/* Truecolour: 24-bit deep ILBM (BMHD nPlanes 24, no CMAP, planes R0-7, G0-7, B0-7, ByteRun1). Rows are
 * ARGB, 4 bytes per pixel (A, R, G, B), via shot_row_argb. Same shot_end. */
int shot_begin_deep(shot_writer *w, const shot_sink *sink, long width, long height, unsigned long camg);
int shot_row_argb(shot_writer *w, const unsigned char *argb);
void shot_planes_argb(const unsigned char *argb, long width, unsigned char *out);
/* Append one row (width bytes). Exactly height rows must follow shot_begin. Returns 0 or -1. */
int shot_row(shot_writer *w, const unsigned char *pix);
/* Patch BODY and FORM sizes, pad the BODY to even, free the buffers. Returns 0 only when every row was
 * written and no sink call failed. */
int shot_end(shot_writer *w);

/* The file name for a DateStamp (days since 1978-01-01, minutes, ticks of 1/50 s): "shot-YYYYMMDD-HHMMSS.iff"
 * into out (at least 24 bytes). */
void shot_stamp_name(long days, long minute, long tick, char *out);

#endif
