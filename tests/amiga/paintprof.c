/* paintprof: the painter on the conbench colour shapes, for tools/prof68k.py
 * (ledger S1, ASM1). render/amiga_render.c's painter_run needs graphics.library
 * (LockLayer, planes_ok), so the row composition is not run; the painter is
 * driven with the runs it gets for each shape of tests/amiga/phaseprobe.c:
 *   sgr-colour   8 runs of 8 cells, pens 0-7 on 0 (the SGR 30+j sequence)
 *   sgr-perchar  40 runs of 1 cell, pens j%8 on 0
 *   pen1on0      one run of 77 cells, pen 1 on 0 (the one-plane vp_asm_plane path)
 * usage: paintprof SHAPE PLANES [ROWS]   (SHAPE 0 sgr-colour, 1 sgr-perchar, 2 pen1on0)
 * ROWS (default 2000) rows of 8 pixel lines at bit phase 4, as paintbench does. */
#include <stdlib.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include "../../render/painter.h"

int main(int argc, char **argv)
{
    static vp_u8 glyphs[256 * 8], chars[80];
    vp_u8 *planes[4];
    int shape, depth, p, k, j, i, rows = 2000;
    if (argc < 3)
        return 10;
    if (argc > 3)
        rows = atoi(argv[3]);
    shape = atoi(argv[1]);
    depth = atoi(argv[2]);
    if (depth < 1 || depth > 4)
        return 10;
    for (k = 0; k < 256 * 8; k++)
        glyphs[k] = (vp_u8)(k * 37);
    for (k = 0; k < 80; k++)
        chars[k] = (vp_u8)('A' + k % 26);
    for (p = 0; p < 4; p++) {
        planes[p] = (vp_u8 *)AllocMem(80 * 256, MEMF_CHIP | MEMF_CLEAR);
        if (!planes[p])
            return 20;
    }
    for (i = 0; i < rows; i++) {
        long y = (i % 30) * 8;
        int mask = (1 << depth) - 1;
        if (shape == 0) {
            for (j = 0; j < 8; j++)
                vp_span_fast(planes, depth, 80, 4 + j * 64, y, glyphs, 8, chars + j * 8, 8, j & mask, 0, mask);
        } else if (shape == 1) {
            for (j = 0; j < 40; j++)
                vp_span_fast(planes, depth, 80, 4 + j * 8, y, glyphs, 8, chars + j, 1, (j % 8) & mask, 0, mask);
        } else {
            vp_span_fast(planes, depth, 80, 4, y, glyphs, 8, chars, 77, 1, 0, 1);
        }
    }
    for (p = 0; p < 4; p++)
        FreeMem(planes[p], 80 * 256);
    return 0;
}
