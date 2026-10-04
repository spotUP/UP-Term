/* render/painter: text into bitplanes at any bit phase, against a pixel by
 * pixel model. */
#include "harness.h"
#include "../render/painter.h"

#define BPR 12
#define H 3
static vp_u8 pl[4][BPR * 4], want[4][BPR * 4];
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

/* the rows a blitter scroll vacated: what vp_span draws in one pen, any
 * glyph, at every phase -- and the pixels beside the cells kept */
static void a_fill_is_a_span_in_one_pen_at_every_phase(void)
{
    static const vp_u8 chars[10] = { 'A', 'b', 0, 255, 'q', ' ', 'Z', '7', 200, '!' };
    vp_u8 *planes[4];
    int x, n, pen, mask, p, k, bad = 0;
    for (k = 0; k < 256 * H; k++)
        glyphs[k] = (vp_u8)(k * 37 + (k >> 3) * 11);
    for (p = 0; p < 4; p++)
        planes[p] = pl[p];
    for (x = 0; x < 16; x++)
        for (n = 1; n <= 10; n++)
            for (pen = 0; pen < 16; pen += 3)
                for (mask = 0x0F; mask >= 0x05; mask -= 0x0A) {
                    for (p = 0; p < 4; p++)
                        for (k = 0; k < BPR * 4; k++)
                            pl[p][k] = want[p][k] = (vp_u8)(0x5A ^ (k * 13) ^ p);
                    vp_fill(planes, 4, BPR, x, 1, H, n, pen, mask);
                    model(x, 1, chars, n, pen, pen, mask);
                    for (p = 0; p < 4; p++)
                        for (k = 0; k < BPR * 4; k++)
                            if (pl[p][k] != want[p][k])
                                bad++;
                }
    CHECK_INT(bad, 0);
}

void suite_painter(void)
{
    every_phase_length_and_pen_pair_matches_the_pixels();
    a_fill_is_a_span_in_one_pen_at_every_phase();
}
