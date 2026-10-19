/* render/painter: text into bitplanes at any bit phase, against a pixel by
 * pixel model. */
#include "harness.h"
#include "../render/painter.h"

#define BPR 12
#define H 3
static vp_u8 pl[4][BPR * 4] __attribute__((aligned(4))), want[4][BPR * 4];
static vp_u8 glyphs[256 * H];

static void model(int x, int y, const vp_u8 *chars, int n, int fg, int bg, int mask)
{
    int p, r, i, b;
    for (p = 0; p < 4; p++) {
        int fb = (fg >> p) & 1, bb = (bg >> p) & 1;
        if (!((mask >> p) & 1))
            continue;
        for (r = 0; r < H; r++)
            for (i = 0; i < n; i++)
                for (b = 0; b < 8; b++) {
                    int on = (glyphs[chars[i] * H + r] >> (7 - b)) & 1;
                    int bit = on ? fb : bb;
                    int px = x + i * 8 + b;
                    vp_u8 *d = &want[p][(y + r) * BPR + (px >> 3)];
                    if (bit)
                        *d |= (vp_u8)(0x80 >> (px & 7));
                    else
                        *d &= (vp_u8)~(0x80 >> (px & 7));
                }
    }
}

static void every_phase_length_and_pen_pair_matches_the_pixels(void)
{
    static const vp_u8 chars[10] = { 'A', 'b', 0, 255, 'q', ' ', 'Z', '7', 200, '!' };
    vp_u8 *planes[4];
    int x, n, fg, bg, mask, p, k, bad = 0;
    for (k = 0; k < 256 * H; k++)
        glyphs[k] = (vp_u8)(k * 37 + (k >> 3) * 11);
    for (p = 0; p < 4; p++)
        planes[p] = pl[p];
    for (x = 0; x < 16; x++)
        for (n = 1; n <= 10; n++)
            for (fg = 0; fg < 16; fg += 5)
                for (bg = 0; bg < 16; bg += 3)
                    for (mask = 0x0F; mask >= 0x05; mask -= 0x0A) {
                        for (p = 0; p < 4; p++)
                            for (k = 0; k < BPR * 4; k++)
                                pl[p][k] = want[p][k] = (vp_u8)(0x5A ^ (k * 13) ^ p);
                        vp_span(planes, 4, BPR, x, 1, glyphs, H, chars, n, fg, bg, mask);
                        model(x, 1, chars, n, fg, bg, mask);
                        for (p = 0; p < 4; p++)
                            for (k = 0; k < BPR * 4; k++)
                                if (pl[p][k] != want[p][k])
                                    bad++;
                    }
    CHECK_INT(bad, 0);
}

/* The long painter (CTerm's design) at every bit phase 0-31 (so every byte
 * phase and every long straddle), n 1..10 cells (spans inside one long,
 * across two, three), several pen pairs including inverse video (fg/bg
 * swapped), clipped at both edges of the row (x 0 and the last cell end at
 * the row's last bit), planes masked: every pixel as the model says, the
 * pixels around untouched, and the long path ran (sentinel). */
static void the_long_painter_matches_the_pixels_at_every_phase_and_edge(void)
{
    static const vp_u8 chars[10] = { 'A', 'b', 0, 255, 'q', ' ', 'Z', '7', 200, '!' };
    static const int pens[6][2] = { { 1, 0 }, { 0, 1 }, { 15, 9 }, { 6, 6 }, { 3, 12 }, { 8, 7 } };
    vp_u8 *planes[4];
    int x, n, pp, mask, p, k, bad = 0, notrun = 0;
    for (k = 0; k < 256 * H; k++)
        glyphs[k] = (vp_u8)(k * 37 + (k >> 3) * 11);
    for (p = 0; p < 4; p++)
        planes[p] = pl[p];
    for (x = 0; x <= 8 * BPR - 8; x++)
        for (n = 1; n <= 10 && x + 8 * n <= 8 * BPR; n++)
            for (pp = 0; pp < 6; pp++)
                for (mask = 0x0F; mask >= 0x05; mask -= 0x0A) {
                    unsigned long before = vp_long_spans;
                    for (p = 0; p < 4; p++)
                        for (k = 0; k < BPR * 4; k++)
                            pl[p][k] = want[p][k] = (vp_u8)(0x5A ^ (k * 13) ^ p);
                    if (!vp_span_long(planes, 4, BPR, x, 1, glyphs, H, chars, n, pens[pp][0], pens[pp][1], mask))
                        notrun++;
                    if (vp_long_spans != before + 1)
                        notrun++;
                    model(x, 1, chars, n, pens[pp][0], pens[pp][1], mask);
                    for (p = 0; p < 4; p++)
                        for (k = 0; k < BPR * 4; k++)
                            if (pl[p][k] != want[p][k])
                                bad++;
                }
    CHECK_INT(bad, 0);
    CHECK_INT(notrun, 0);
}

static void the_long_painter_refuses_a_row_that_is_not_long_aligned(void)
{
    static const vp_u8 chars[1] = { 'A' };
    vp_u8 *planes[4];
    int p;
    for (p = 0; p < 4; p++)
        planes[p] = pl[p];
    CHECK_INT(vp_span_long(planes, 4, 10, 3, 0, glyphs, H, chars, 1, 1, 0, 15), 0);
    planes[2] = pl[2] + 2;
    CHECK_INT(vp_span_long(planes, 4, BPR, 3, 0, glyphs, H, chars, 1, 1, 0, 15), 0);
}

void suite_painter(void)
{
    the_long_painter_matches_the_pixels_at_every_phase_and_edge();
    the_long_painter_refuses_a_row_that_is_not_long_aligned();
    every_phase_length_and_pen_pair_matches_the_pixels();
}
