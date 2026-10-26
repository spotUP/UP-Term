---
date: 2026-10-26
topic: gcc m68k bbb propagate_moves hoists a DImode loop copy-back out of the loop (sqlite3 hang)
status: fixed and installed; no torture test (see Status)
branch: fix/m68k-bbb-di-loop in /Users/spot/Code/UP-Term/cpython-amiga/build/gcc/src, off f88c4beae
installed: ~/opt/amiga 2026-10-26 (cc1 cmp-identical to build); backup ~/opt/amiga/libexec/gcc/m68k-amigaos/6.5.0b/cc1.pre-diloop-2026-10-26 and scratchpad bak-diloop/cc1
commit: 1e025c364 on fix/m68k-bbb-di-loop (gcc/bbb-opts.c, 21+ 5-)
---

# Cause

sqlite3.c:177022 (sqlite3WhereBegin) `Bitmask b = colUsed; for(; b; b=b>>1, n++){}` with b in
a0:a1 at -m68020 -O2. RTL after bbro (the pass before bbb):

    L:  (set (reg:DI d1) (reg:DI a0))                       ; REG_DEAD a0
        (set (reg:DI d1) (lshiftrt:DI (reg:DI d1) 1))
        (set (reg:DI a0) (reg:DI d1))                       ; REG_DEAD d1
        (set (reg:SI d0) (plus d0 1))
        (jump_insn (parallel [(set pc (if_then_else (ne (reg:DI a0) 0) L pc)) (clobber scratch:SI)]))

Bisect of `-fbbb=<letter>` (sqlite3.i, asm check of the loop): only `p` (opt_propagate_moves)
gives the bad loop; - a b c e f h i l m n r s 0 z alone are correct. `-fbbb=+v` logs
"(p) propagate_moves condition met, moving regs a0, d1" in sqlite3WhereBegin.

Violated invariant: propagate_moves may hoist `move src,x` / sink `move x,src` only if src is
not read anywhere in the loop after the copy-back, including the back-edge jump. Its checks use
is_reg_touched_between (regno, from, to), which scans from+1 .. to-1, so the jump at `index`
is never scanned. With cc0 the compare is a separate insn inside the range; the m68k DImode
cbranch (`*cbranchdi` parallel, eq/ne against 0) reads (reg:DI a0) inside the jump itself.
Second fault, same checks: only REGNO(src)/REGNO(dst) were tested, so the second register of a
DImode pair (a1, d2) was never checked.

# Fix

bbb-opts.c: new is_reg_rtx_touched_between (rtx, from, to, to_inclusive) after
is_reg_touched_between: loops REGNO .. END_REGNO; the "move x,src ... jcc" src check includes
the jump. All five checks in opt_propagate_moves use it.

# Build / install

The 16 Oct scratch obj dir (drvobj) is gone with that session's scratchpad. New obj dir:
<scratchpad>/gobj, configured from cpython-amiga/build/gcc/src with the same configure line as
build/gcc/obj/config.log (prefix ~/opt/amiga). `nice -n 19 make -j2 all-gcc`.

# Status

- Bisect: `-fbbb=-` and every single letter except `p` give a correct loop; `-fbbb=p` (opt_propagate_moves)
  alone reproduces the hang pattern. `-fbbb=+v` logs "(p) propagate_moves condition met, moving regs a0, d1".
- Violated invariant: propagate_moves may hoist `move src,x` / sink `move x,src` only when src is not read
  anywhere in the loop, including by the back-edge jump. The check `is_reg_touched_between(regno, from, to)`
  scans from+1..to-1, so the jump (where the m68k DImode cbranch keeps its compare) was skipped; it also
  checked REGNO only, so the second hard reg of a DImode pair (a1, d2) was never looked at.
- Fix: gcc/bbb-opts.c:2678 new `is_reg_rtx_touched_between(rtx, from, to, to_inclusive)` walking
  REGNO..END_REGNO; used at 3210-3214, the src check up to the jump includes the jump.
- Head match: a clean build of f88c4beae differs from the installed cc1 in bytes (Mach-O header/paths)
  but gives byte-identical asm for all of sqlite3.i at -O2, so the installed cc1 is that head.
- Asm after fix (_sqlite3WhereBegin, -m68020 -O2, bbb on):

      .L39683: move.l a0,d1 / move.l a1,d2 / lsr.l #1,d1 / roxr.l #1,d2
               move.l d1,a0 / move.l d2,a1 / addq.l #1,d0
               tst.l a0 / jne .L39683 / tst.l a1 / jne .L39683

  Before: copy-back after the loop, tests on a0/a1 that never change (infinite loop).
- Install verified: cmp installed cc1 == build cc1, and the asm check run with the default
  (installed) driver gives OK.
- Torture test: NOT done. Standalone reductions (S/bbb/t1.c..t6.c: register pressure, "+a" asm
  constraints) never put the u64 in address registers, so propagate never fires. cvise/creduce are not
  installed; the reduction needs one of them (asm-pattern check: S/bbb/chk.sh). Open item.
- Other ports at risk: any m68k build at -O2 with bbb on where a 64-bit value lives in a register pair
  that is copied, changed and copied back inside a loop whose exit test is the DImode cbranch. Cannot be
  told cheaply from source; rebuild suspect ports (anything with u64 bitmask/shift loops, e.g. sqlite)
  with the new cc1.

## Regression test

gcc commit 4c4793bbf on fix/m68k-bbb-di-loop: gcc/testsuite/gcc.c-torture/execute/bbb-propagate-moves-di-loop-1.c (69 lines).
Method: stripped sqlite3.i to sqlite3WhereBegin plus its direct-call closure (2.2 MB; other function bodies dropped, the
inlined callees must stay or the allocation changes), then creduce --n 2 with an interestingness check that needs:
host clang accepts it (implicit declarations, return-type and uninitialised use are errors), the pre-fix cc1
(cc1.pre-diloop-2026-10-26 via -B) emits a lsr/roxr loop that tests registers it never writes, the fixed cc1 does not.
Reduced to 1.1 KB, then hand-made valid C and executable (main sets the data, aborts if the shift count is not 31
or the h() sum is wrong). Two details decide the shape: `struct o *n` must be a pointer (not an array), and h() must
be noinline (inlined, the register allocation changes and the bug disappears).
Proof: -m68020 -O2 -S with the pre-fix cc1 gives the BAD loop (tst a0/a1, copy-back after the loop); with the fixed cc1
the copy-back stays inside the loop. Execution NOT shown: vamos here returns rc 20 even for an empty main (no libs set up),
so the hang itself was not run; the host build of the test passes (rc 0). Check script: scratchpad bbb/red/interest.sh.

## Second bbb bug: elim_dead_assign2 (fts5)

With the DI-loop fix, sqlite fts5 still failed (8 of 9). A per-function gate on the bbb `e` pass found
`_sqlite3CompleteInsertion.isra.551`. Rule `opt_elim_dead_assign2` (log "e0: eliminate superfluous clear of dX")
deletes a SImode constant load when the next insn writes the same register in QImode/HImode and the upper bits
are dead or known zero. It does not check that the next insn reads the register, so `moveq #32,d4; or.b #1,d4`
lost the moveq (flags 1, not 33). Same pattern in adjustOrderByCol (`moveq #0,d0; addq.w #1,d0`).
Fix: also require `!jj.is_myuse(regno)`; strict_low_part destinations do not set the use bit, so the intended
`clr; move.b` case still fires. gcc commits ab34f2880 (fix) and 9691feda3 (test bbb-elim-clr-partial-or-1.c) on
fix/m68k-bbb-di-loop; separate codeberg PR branch spotup/m68k-bbb-e0-partial-or.
