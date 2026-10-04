/* paintbench: render/painter's cost on the 68020 (ledger S1) -- 2000 rows of
 * 77 cells, 8 pixels high, at bit phase 4, into a chip-RAM bitmap that is
 * not on screen, one and three planes; ticks of 1/50 s. What a row costs
 * before the display's DMA and the blitter are in it. */
#include <string.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "../../render/painter.h"
void vp_asm_plane(vp_u8 *dst, long bpr, long s, const vp_u8 **gp, long n, long h, long pattern, long constant);

static long now(void) { struct DateStamp ds; DateStamp(&ds); return ds.ds_Minute * 3000L + ds.ds_Tick; }

int main(void)
{
    static vp_u8 glyphs[256 * 8], chars[77];
    vp_u8 *planes[4];
    long t0, i;
    int p, k;
    for (k = 0; k < 256 * 8; k++)
        glyphs[k] = (vp_u8)(k * 37);
    for (k = 0; k < 77; k++)
        chars[k] = (vp_u8)('a' + k % 26);
    for (p = 0; p < 4; p++) {
        planes[p] = (vp_u8 *)AllocMem(80 * 256, MEMF_CHIP | MEMF_CLEAR);
        if (!planes[p])
            return 20;
    }
    for (k = 1; k <= 3; k += 2) {
        t0 = now();
        for (i = 0; i < 2000; i++)
            vp_span_fast(planes, 4, 80, 4, (i % 30) * 8, glyphs, 8, chars, 77, 1, 0, k == 1 ? 1 : 7);
        Printf((STRPTR)"%ld plane(s): 2000 rows in %ld ticks\n", (LONG)k, now() - t0);
    }
    {
        /* the 68k plane loop alone, the glyph pointers made once */
        static const vp_u8 *gp[80];
        for (k = 0; k < 77; k++)
            gp[k] = glyphs + chars[k] * 8;
        t0 = now();
        for (i = 0; i < 2000; i++)
            vp_asm_plane(planes[0] + (i % 30) * 8 * 80, 80, 4, gp, 77, 8, 0, 0);
        Printf((STRPTR)"asm plane loop alone: 2000 rows in %ld ticks\n", now() - t0);
        t0 = now();
        for (i = 0; i < 2000; i++)
            for (k = 0; k < 77; k++)
                gp[k] = glyphs + chars[k] * 8;
        Printf((STRPTR)"the C glyph-pointer loop alone: 2000 rows in %ld ticks\n", now() - t0);
    }
    for (p = 0; p < 4; p++)
        FreeMem(planes[p], 80 * 256);
    return 0;
}
