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
void rip_feed(struct rip_state *st, const unsigned char *p, long n, const struct rip_sinks *sk);
/* commands delivered, malformed commands dropped (short, bad digit, over
 * RIP_MAXARGS), well-formed commands with a (level, letter) the table lacks */
long rip_commands(const struct rip_state *st);
long rip_errors(const struct rip_state *st);
long rip_unknown(const struct rip_state *st);
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
};

/* alloc with malloc; NULL when the size is zero, over 1024x1024, or malloc fails.
 * Pixels start at index 0, the palette at black. */
struct cv *cv_open(int w, int h, int npal);
void cv_close(struct cv *c);
void cv_set_pal(struct cv *c, int idx, unsigned long rgb);
int cv_get(const struct cv *c, int x, int y);           /* -1 outside */
void cv_pixel(struct cv *c, int x, int y, int idx);
void cv_hspan(struct cv *c, int x0, int x1, int y, int idx); /* x1 inclusive */
void cv_rect(struct cv *c, int x0, int y0, int x1, int y1, int idx); /* both corners inclusive */
/* the dirty rectangle since the last take; cleared */
struct cv_rect_s cv_take_dirty(struct cv *c);
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

#endif
