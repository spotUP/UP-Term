/* inst_core: the pure part of the Installer 43.3 margin patch (A5, strut_patch.c plain_hook). Installer anchors its
 * button rows to the window bottom with `subq.w #1,d1` at two sites, which leaves a 1 px margin under the Help row
 * and a 1 px gap between rows. Its own side margin (BorderLeft + 4) and the gap between Proceed and Abort are 4 px,
 * so the patch turns both immediates into #4 and grows the window by the pixels that adds, so nothing above the
 * buttons is squeezed. Byte signatures are checked exactly; anything else is left alone (fail closed, as the
 * dopus5 body patch in flat_core.c). Host-tested (tests/test_inst.c). */
#ifndef AMARCHY_INST_CORE_H
#define AMARCHY_INST_CORE_H

#define INST_K 4                    /* margin and row gap after the patch: Installer's own side margin / button gap */
#define INST_ROWS 2                 /* bottom-anchored rows: Help, Proceed/Abort */
#define INST_CODE_MIN 0xc284UL      /* hunk-0 bytes the signatures need */

#define INST_SIG_UNKNOWN 0
#define INST_SIG_ORIG 1
#define INST_SIG_PATCHED 2

/* Installer 43.3 hunk 0 (code), size bytes: original, already patched (resident / shared code) or unknown. */
int inst_sig(const unsigned char *code, unsigned long size);
/* Write #INST_K into both row-placement immediates. Call only when inst_sig says INST_SIG_ORIG. */
void inst_apply(unsigned char *code);
/* The bottom margin / row gap the code produces (the subq immediate at the single-row site), 0 when not subq.w. */
int inst_row_margin(const unsigned char *code);
/* tf_YSize when height is Installer's own window height 17*Y + 23 (Y 8..100), else 0. */
int inst_font_y(int height);
/* Pixels the patch adds to the window: INST_ROWS * (INST_K - 1). */
int inst_grow(void);
/* Grow a top/height box by inst_grow(), keeping it centred and on a screen scr_h tall. 1 = fits, *ntop / *nh set;
 * 0 = the grown window does not fit (do not patch). */
int inst_box(int top, int height, int scr_h, int *ntop, int *nh);

#endif
