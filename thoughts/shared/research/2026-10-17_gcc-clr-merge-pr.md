---
date: 2026-10-17
topic: PR for bebbo/gcc (amiga6) - m68k clr-merge peephole2 mixes modes
status: ready, NOT submitted (codeberg.org down 2026-10-17: HTTPS 503, SSH closed)
---

Branch: /Users/spot/Code/UP-Term/amiga-gcc/projects/gcc `fix/m68k-clr-merge-mode`, commit 5a4cfa02d
(cherry-pick of aee333a76 from cpython-amiga build/gcc/src fix/zlib-o2) on amiga6 8c1fd39db.
Owner approved the upstream PR 2026-10-17.

Submit (same AGit flow as PR 13 and 15):

    G=/Users/spot/Code/UP-Term/amiga-gcc/projects/gcc
    B64=$(sed -n '/^## PR body/,$p' <this file> | tail -n +2 | base64 | tr -d '\n')
    GIT_SSH_COMMAND="ssh -i $HOME/.ssh/codeberg_ed25519 -o IdentitiesOnly=yes" git -C $G push \
      ssh://git@codeberg.org/bebbo/gcc.git fix/m68k-clr-merge-mode:refs/for/amiga6/m68k-clr-merge-mode \
      -o "title=m68k: merge two clr stores only when both have the same mode" -o "description={base64}$B64"

## PR body
The peephole2 "combine clr if possible" matched two zero stores
x(aN) and y(aN) with mode-less mem patterns and checked only the
first store's mode. "clr.w 12(a0); clr.l 14(a0)" passed the test
(12 + 2 == 14) and became "clr.l 12(a0)", which clears four of the
six bytes and leaves the low word of the int at 14 unwritten.

zlib's bi_windup does "s->bi_buf = 0; s->bi_valid = 0;" (ush then
int, adjacent), so bi_valid kept a stale value and deflate wrote a
stray 0x00 before the adler32 trailer at -O2/-Os.

Require the second store to have the first store's mode and to be
non-volatile as well. Test: gcc.c-torture/execute/clr-merge-mode-1.c
aborts without the fix.

