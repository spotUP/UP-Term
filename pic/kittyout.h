/* kittyout -- the one encoder of kitty graphics commands (APC G) for the picture tools
 * (icat, and later the fetch tool and the file manager preview). Portable C89, no OS calls:
 * the bytes go to a sink. The engine's parser (engine/kitty.c) is the other half; the host test
 * tests/test_kittyout.c feeds one into the other. */
#ifndef KITTYOUT_H
#define KITTYOUT_H

typedef void (*kout_put)(void *u, const char *s, long n);

typedef struct kout_opts {
    int cols; /* c=: scale to this many cells wide, 0 = natural size */
    int rows; /* r=: scale to this many cells high, 0 = natural size */
    int quiet; /* q=: 0, 1 or 2 (2 hides every reply) */
} kout_opts;

/* raw pixels: bpp 3 (f=24 RGB) or 4 (f=32 RGBA), shown at the cursor (a=T), in 4096-byte
 * base64 chunks (m=1 ... m=0). 1 when sent, 0 for a bad size. */
int kout_pixels(kout_put put, void *u, const unsigned char *px, int w, int h, int bpp, const kout_opts *o);

/* a PNG file's bytes as they are (f=100), same chunks. 1 when sent. */
int kout_png(kout_put put, void *u, const unsigned char *png, long n, const kout_opts *o);

/* the detection query (a=q, i=31, a 1 x 1 RGB pixel): a terminal with kitty graphics answers
 * ESC _ G i=31 ; OK ESC \ */
void kout_query(kout_put put, void *u);

/* 1 when buf (n bytes read after kout_query) holds the OK answer to the query */
int kout_query_ok(const char *buf, long n);

/* nearest-neighbour copy of src (w x h, bpp bytes a pixel) into dst (dw x dh). */
void kout_scale(const unsigned char *src, int w, int h, int bpp, unsigned char *dst, int dw, int dh);

/* the size (dw, dh) to send so that w x h stays in max_px pixels in all (the engine takes up to
 * 1,000,000 pixels in one picture) and in max_w pixels across; never above the input. */
void kout_fit(int w, int h, long max_px, int max_w, int *dw, int *dh);

#endif
