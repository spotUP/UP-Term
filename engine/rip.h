/* RIPscrip 1.54 core, pure C (no Amiga headers, no vt_term): the byte parser
 * (rip_parse.c), the RIP palette (rip_pal.c) and the indexed canvas (canvas.c).
 * DCTelnet vendors these files and compiles them as separate translation units
 * (plan 2026-11-09-graphics-protocols D18), so this header includes nothing but
 * the C library's stddef.
 *
 * rip_feed takes the raw byte stream of a session. Text goes to sinks->text, a
 * parsed command to sinks->cmd. The parser holds its position in `struct
 * rip_state` between calls, so a command may be split at any byte. */
#ifndef RIP_H
#define RIP_H

#define RIP_MAXARGS 1040 /* a 512-point polygon: count + 1024 coordinates */
#define RIP_TEXT_MAX 256 /* the text tail of a command, bytes kept */

/* One parsed command. level and sub are the level digits before the letter
 * (`|1U` level 1, `|15L` level 1 sub-level 5, `|c` level 0); ch is the command
 * character (any non-digit, ESC for the 1<ESC> query and the level-9 transfer).
 * arg[] holds the base-36 numbers in the order of the command's width table;
 * for P, p and l arg[0] is the point count and arg[1..2*count] the pairs. text
 * is the variable tail (T, @, 1U, 1M, file names ...), with \| \! \\ undone and
 * continuations removed. */
struct rip_cmd {
    int level, sub, ch;
    int nargs;
    long arg[RIP_MAXARGS];
    int ntext;
    unsigned char text[RIP_TEXT_MAX + 1];
    int truncated; /* text longer than RIP_TEXT_MAX: the rest was dropped */
};

struct rip_sinks {
    void *ud;
    /* plain text (also the ANSI of the BBS menu text), in order, any split */
    void (*text)(void *ud, const unsigned char *p, long n);
    /* a complete, valid command; the pointer is valid during the call only */
    void (*cmd)(void *ud, const struct rip_cmd *c);
    /* may be NULL: asked after every delivered command; nonzero = stop there, rip_feed returns
     * the bytes consumed up to and including the byte that ended the command */
    int (*halt)(void *ud);
};

struct rip_state {
    int mode, phase, bs, eol;
    int sohch;
    const char *wid;      /* the width table of the current command */
    int wi, wk, pairs;
    long val;
    int ndig;
    int bad;
    long n_cmd, n_err, n_unknown;
    struct rip_cmd c;
};

void rip_init(struct rip_state *st);
/* feed bytes; sinks->text and sinks->cmd may be NULL */
long rip_feed(struct rip_state *st, const unsigned char *p, long n, const struct rip_sinks *sk);
/* commands delivered, malformed commands dropped (short, bad digit, over
 * RIP_MAXARGS), well-formed commands with a (level, letter) the table lacks */
long rip_commands(const struct rip_state *st);
long rip_errors(const struct rip_state *st);
long rip_unknown(const struct rip_state *st);
/* 1 when the parser sits between commands (text or line start): the adapter may
 * look at an escape sequence there. rip_bol puts it at a line start. */
int rip_in_text(const struct rip_state *st);
void rip_bol(struct rip_state *st);
/* one line "L<level>[.sub] <ch> a,b,c|text" for tests and golden files; returns length */
int rip_dump(const struct rip_cmd *c, char *out, int max);

/* ---- RIP palette (rip_pal.c, RIP-3) ---- */
#define RIP_NPAL 16
/* master EGA colour (6 bits rgbRGB: bit0 blue 2/3, bit1 green 2/3, bit2 red 2/3,
 * bit3 blue 1/3, bit4 green 1/3, bit5 red 1/3) to 0xRRGGBB with levels 00/55/AA/FF */
unsigned long rip_ega_rgb(int master);
/* the default palette: index -> master colour (RIP-0 closure: 6 is brown 0x14) */
void rip_pal_default(unsigned char master[RIP_NPAL]);

/* ---- canvas (canvas.c, RIP-3) ---- */
#define CV_RIP_W 640
#define CV_RIP_H 350

struct cv_rect_s { int x0, y0, x1, y1; }; /* x1,y1 exclusive; empty when x1 <= x0 */

/* An indexed canvas: one byte per pixel, pal[] 0xRRGGBB. The host reads pix/pal and the
 * dirty rectangle; cv_take_dirty returns it and clears it. serial is bumped by every
 * cv_flush that had something dirty (a renderer's cache key). */
struct cv {
    unsigned char *pix;
    unsigned long pal[256];
    int w, h, npal;
    struct cv_rect_s dirty;
    long serial;
    int own; /* pix was malloc'd by cv_open and cv_close frees it */
};

/* alloc with malloc; NULL when the size is zero, over 1024x1024, or malloc fails.
 * Pixels start at index 0, the palette at black. */
struct cv *cv_open(int w, int h, int npal);
/* The same over pixels the caller owns (the vt adapter hands in the image slot's buffer):
 * cleared to index 0, never freed by cv_close. */
struct cv *cv_open_pix(int w, int h, int npal, unsigned char *pix);
void cv_close(struct cv *c);
void cv_set_pal(struct cv *c, int idx, unsigned long rgb);
int cv_get(const struct cv *c, int x, int y);           /* -1 outside */
void cv_pixel(struct cv *c, int x, int y, int idx);
void cv_hspan(struct cv *c, int x0, int x1, int y, int idx); /* x1 inclusive */
void cv_rect(struct cv *c, int x0, int y0, int x1, int y1, int idx); /* both corners inclusive */
/* the dirty rectangle since the last take; cleared */
/* Copy w x h indexed pixels (row major) onto the canvas at x, y with a write mode (the RIP
 * 1P/1I modes: copy, xor, or, and, not-of-source); only pixels inside the box cx0..cx1,
 * cy0..cy1 (inclusive) and on the canvas are written. */
#define CV_COPY 0
#define CV_XOR 1
#define CV_OR 2
#define CV_AND 3
#define CV_NOT 4
void cv_blit(struct cv *c, int x, int y, const unsigned char *src, int w, int h, int minterm,
             int cx0, int cy0, int cx1, int cy1);
struct cv_rect_s cv_take_dirty(struct cv *c);
/* grow *d to cover r (an empty r changes nothing; an empty *d becomes r) */
void cv_rect_union(struct cv_rect_s *d, const struct cv_rect_s *r);
/* bump serial when something is dirty (call once per feed); returns 1 if it bumped */
int cv_flush(struct cv *c);

/* Logical canvas to window (O7): the canvas is drawn 1:1 at the grid origin; it is
 * scaled down, keeping the aspect, only when the window (win_w x win_h pixels) is
 * smaller than the canvas. out_w/out_h get the drawn size, scale_num/scale_den the
 * factor (1/1 when it fits). Returns 0 for a zero-sized window. */
struct cv_map_s { int out_w, out_h, num, den; };
int cv_map(int cv_w, int cv_h, int win_w, int win_h, struct cv_map_s *m);

/* RIP palette commands against a canvas (RIP-3): Q sets 16 entries from master colours,
 * a sets one, reset restores the default. The canvas palette entries 0..15 follow. */
struct rip_pal {
    unsigned char master[RIP_NPAL];
};
void rip_pal_reset(struct rip_pal *p, struct cv *c);
/* c is a parsed cmd: ch 'Q' (level 0) or 'a'; returns 1 if it was a palette command and applied */
int rip_pal_exec(struct rip_pal *p, struct cv *c, const struct rip_cmd *cmd);

/* ---- drawing model (rip_draw.c, RIP-4) ---- */
struct rip_gfx {
    int color;                  /* c: drawing colour */
    int wmode;                  /* W: 0 copy, 1 XOR */
    int lstyle;                 /* =: 0 solid 1 dotted 2 centre 3 dashed 4 user */
    unsigned lpat;              /* = user pattern, 16 bits */
    int thick;                  /* = thickness 1 or 3 */
    int fpat, fcolor;           /* S: fill pattern 0..11 (12 user), fill colour */
    unsigned char upat[8];      /* s: user 8x8 pattern, row 0 first, bit 0x80 leftmost */
    int px, py;                 /* m: current position */
    int vx0, vy0, vx1, vy1;     /* v: the graphics viewport, corners inclusive; every pixel clips to it */
    int font, fdir, fsize;      /* Y: font number (1..4 draw Hershey strokes when stroke is set, all else the 8x8 bitmap), 0 across or 1 up, scale */
    int stroke;                 /* host option (RIP-3b): 0 8x8 bitmap for every font (default), 1 Hershey strokes for fonts 1..4 */
};
void rip_gfx_init(struct rip_gfx *g);
/* 8x8 bitmap font (O19): glyph `ch` (CP437) row r, bit 0x80 leftmost */
unsigned char rip_font8_row(int ch, int r);
/* Hershey stroke fonts (RIP-3b, rip_font.c): BGI fonts 1 triplex, 2 small, 3 sans-serif, 4 gothic.
 * rip_font_is_stroke: 1 for a font number that has strokes. rip_font_glyph feeds the segments of
 * glyph ch at size s (1..10) from the cell origin ox, oy (up: a quarter turn counter-clockwise) to
 * seg and returns the advance in pixels. rip_font_stroke_glyphs counts the glyphs drawn (sentinel). */
int rip_font_is_stroke(int font);
int rip_font_glyph(int font, int ch, int s, int up, int ox, int oy,
                   void (*seg)(void *ud, int x0, int y0, int x1, int y1), void *ud);
long rip_font_stroke_glyphs(void);
/* 1 when rip_gfx_exec acts on this command (it needs a canvas) */
int rip_gfx_handles(const struct rip_cmd *c);
/* run a level-0 drawing or state command; returns 1 if handled */
int rip_gfx_exec(struct rip_gfx *g, struct cv *cv, const struct rip_cmd *c);

/* ---- clipboard and icons (rip_icon.c, RIP-7) ---- */
#define RIP_ICON_FILE_MAX 131072L /* a .ICN file over this is refused (640 x 350 x 4 planes is 112000) */
/* The clipboard: indexed pixels of 1C and 1I (clip flag); malloc'd, freed by rip_clip_free. */
struct rip_clip {
    unsigned char *pix;
    int w, h;
};
/* How an icon file is read; the host's file hook sits behind load. name is the sanitised file
 * name only (never a path); load returns 1 with a buffer of n bytes that release frees. */
struct rip_icon_src {
    void *ud;
    int (*load)(void *ud, const char *name, unsigned char **buf, long *n);
    void (*release)(void *ud, unsigned char *buf);
};
/* The 1I file text as a file name: one plain component (no '/', '\\', ':', control characters;
 * not "." or ".."), ".ICN" appended when it has no dot. 1 and out filled, or 0 = refused. */
int rip_icon_name(const unsigned char *text, int ntext, char *out, int max);
/* .ICN: u16 width, u16 height (little endian), then per scanline the four bit planes blue,
 * green, red, intensity, each (width+7)/8 bytes, bit 0x80 leftmost. 1 with *pix malloc'd
 * (width x height indices 0..15), or 0 for a bad or oversized file. */
int rip_icon_decode(const unsigned char *d, long n, unsigned char **pix, int *w, int *h);
int rip_clip_handles(const struct rip_cmd *c); /* 1 for 1C 1P 1G 1I */
int rip_clip_exec(struct rip_clip *clip, const struct rip_gfx *g, struct cv *cv,
                  const struct rip_cmd *c, const struct rip_icon_src *src);
void rip_clip_free(struct rip_clip *clip);
long rip_icons_drawn(void); /* sentinel: icons loaded and drawn since start */

/* ---- mouse fields and buttons (rip_mouse.c, RIP-6) ---- */
#define RIP_MOUSE_MAX 128 /* fields and buttons together; the 129th is rejected */
#define RIP_MCMD 128      /* host command bytes kept per field */

struct rip_mfield {
    int x0, y0, x1, y1;          /* pixels, corners inclusive */
    int clk, clr;                /* 1M flags: invert on click (stored, not drawn), reset windows first */
    int hotkey;                  /* 1U: an ASCII code, 0 none */
    int ncmd;
    unsigned char cmd[RIP_MCMD]; /* the host command as sent, before expansion */
};
struct rip_bstyle { int wid, hgt, bevel, dfore, dback, bright, dark, surface; }; /* 1B */
struct rip_mouse {
    int n;
    long rejected;               /* fields refused for the limit */
    struct rip_bstyle st;
    struct rip_mfield f[RIP_MOUSE_MAX];
};
void rip_mouse_init(struct rip_mouse *m);
/* 1 for a level-1 M, K, B or U */
int rip_mouse_handles(const struct rip_cmd *c);
/* run it; g and cv draw a button (cv may be NULL: the field is kept, nothing drawn) */
int rip_mouse_exec(struct rip_mouse *m, struct rip_gfx *g, struct cv *cv, const struct rip_cmd *c);
/* index of the last defined field holding the pixel, -1 none */
int rip_mouse_at(const struct rip_mouse *m, int px, int py);
/* the expanded host command of the field hit (byte count, 0 for an empty command), -1 for no hit.
 * out holds up to max (RIP_MCMD is always enough); *clr (may be NULL) gets the field's clear flag */
int rip_mouse_click(const struct rip_mouse *m, int px, int py, unsigned char *out, int max, int *clr);
int rip_mouse_key(const struct rip_mouse *m, int key, unsigned char *out, int max, int *clr);
/* host command expansion: ^x control character, backquote and $NAME$ recognised (no-op) */
int rip_expand(const unsigned char *src, int n, unsigned char *out, int max);

#endif
