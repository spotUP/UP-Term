/* See inst_core.h. Offsets are Installer 43.3 hunk-0 offsets (C:Installer md5 d23d5e8b2ae23b97685cb3e811aa9abd);
 * the ranges hold no relocations (all a4-relative), so the bytes are fixed. */
#include "inst_core.h"

#define SITE1 0xc0ee    /* single-button row (Help): move.w a4@(24014),d1; sub.w d0,d1; subq.w #1,d1; move.w d1,a2@(6) */
#define SITE2 0xc274    /* two-button row (Proceed / Abort): same, then move.w d1,a3@(6) */
#define SUBQ 6          /* offset of the subq.w word inside both sites */
#define OPENW 0x441c    /* movea.l a4@(60),a6; jsr -204(a6): the plain OpenWindow the hook sees */
#define SIZE 0x2ca      /* addq #1,d0; move.l d0,d1; lsl.w #4,d1; add.w a0@(20),d1; addq.w #7,d1: Height = 17*Y + 23 */

static const unsigned char s1[] = { 0x32, 0x2c, 0x5d, 0xce, 0x92, 0x40, 0x53, 0x41, 0x35, 0x41, 0x00, 0x06 };
static const unsigned char s2[] = { 0x32, 0x2c, 0x5d, 0xce, 0x92, 0x40, 0x53, 0x41, 0x35, 0x41, 0x00, 0x06, 0x37, 0x41, 0x00, 0x06 };
static const unsigned char so[] = { 0x2c, 0x6c, 0x00, 0x3c, 0x4e, 0xae, 0xff, 0x34 };
static const unsigned char sz[] = { 0x52, 0x40, 0x22, 0x00, 0xe9, 0x41, 0xd2, 0x68, 0x00, 0x14, 0x5e, 0x41 };

#define SUBQ_W_D1(n) (0x5141 | (((n) & 7) << 9))  /* subq.w #n,d1 */

/* 1 when code[at..] equals sig, with the subq word allowed to be subq #want instead. */
static int same(const unsigned char *code, unsigned long at, const unsigned char *sig, unsigned n, int sub, int want)
{
    unsigned i;
    unsigned w = SUBQ_W_D1(want);
    for (i = 0; i < n; i++) {
        unsigned char b = sig[i];
        if (sub && i == SUBQ)
            b = (unsigned char)(w >> 8);
        if (code[at + i] != b)
            return 0;
    }
    return 1;
}

int inst_sig(const unsigned char *code, unsigned long size)
{
    if (!code || size < INST_CODE_MIN)
        return INST_SIG_UNKNOWN;
    if (!same(code, OPENW, so, sizeof so, 0, 0) || !same(code, SIZE, sz, sizeof sz, 0, 0))
        return INST_SIG_UNKNOWN;
    if (same(code, SITE1, s1, sizeof s1, 1, 1) && same(code, SITE2, s2, sizeof s2, 1, 1))
        return INST_SIG_ORIG;
    if (same(code, SITE1, s1, sizeof s1, 1, INST_K) && same(code, SITE2, s2, sizeof s2, 1, INST_K))
        return INST_SIG_PATCHED;
    return INST_SIG_UNKNOWN;
}

void inst_apply(unsigned char *code)
{
    code[SITE1 + SUBQ] = (unsigned char)(SUBQ_W_D1(INST_K) >> 8);
    code[SITE2 + SUBQ] = (unsigned char)(SUBQ_W_D1(INST_K) >> 8);
}

int inst_row_margin(const unsigned char *code)
{
    unsigned w = ((unsigned)code[SITE1 + SUBQ] << 8) | code[SITE1 + SUBQ + 1];
    int n;
    if ((w & 0xf1ff) != 0x5141)
        return 0;
    n = (w >> 9) & 7;
    return n ? n : 8;
}

int inst_font_y(int height)
{
    int y;
    if (height < 23 || (height - 23) % 17)
        return 0;
    y = (height - 23) / 17;
    return y >= 8 && y <= 100 ? y : 0;
}

int inst_grow(void)
{
    return INST_ROWS * (INST_K - 1);
}

int inst_box(int top, int height, int scr_h, int *ntop, int *nh)
{
    int h = height + inst_grow();
    int t = top - inst_grow() / 2;
    if (h > scr_h)
        return 0;
    if (t + h > scr_h)
        t = scr_h - h;
    if (t < 0)
        t = 0;
    *ntop = t;
    *nh = h;
    return 1;
}
