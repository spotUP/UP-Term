---
date: 2026-10-16
topic: gcc m68k drops the tst of a DImode high word after add.l/addx.l (__builtin_mul_overflow)
status: fixed and installed; submitted 2026-10-16 as https://codeberg.org/bebbo/gcc/pulls/15
branch: fix/m68k-addx-cc in /Users/spot/Code/UP-Term/amiga-gcc/projects/gcc, commit d4d9db8f6 on amiga6 8c1fd39db
installed: dc36f9750 on fix/ixemul-startfile-sret in /Users/spot/Code/UP-Term/cpython-amiga/build/gcc/src
---

# Cause

`ov(size_t n){ptrdiff_t c; return __builtin_mul_overflow(n,4,&c);}` at -O1+ emits
`add.l d3,d3; addx.l d2,d2` twice (`*ashldi3_const1`, split from `ashift:DI 2`), then `jne`
with no `tst.l d2`. addx.l clears Z only for a non-zero result, so Z is the low word's.
ov(31) returned 1; gnulib xalloc_oversized failed in hash.c (coreutils "memory exhausted").

Not notice_update_cc and not the pattern: `-fbbb=-` emits the tst, `-fbbb=e` alone drops it.

1. bbb-opts.c `track_regs` keeps one 32-bit mask per hard register. For `(set (reg:DI d2)
   (ashift:DI d2 1))` the shift branch stored `mask(d2) << 1` = 0xfffffffe for d2 alone,
   losing the carry addx brings in from d3. `opt_elim_dead_assign` then attached
   `REG_BIT_MASK 0xfffffffe` to `tst.l d2`.
2. final.c (bebbo's clause in the cc0 compare elimination) deletes `compare (reg) 0` before
   EQ/NE when `REGNO(cc_status.value1) == REGNO(src2)` and the mask is below
   `1 << (8*GET_MODE_SIZE(GET_MODE(src2)))`. value1 was `(reg:DI d2)`: the flags describe
   value1's mode (and a multi-word set only its last word op), not src2's; and `1 << 32` is
   undefined for SImode.

Fix (both parts, one commit): track_regs clears the masks of every register of a src-op set
wider than UNITS_PER_WORD; final.c requires value1 to fit one word and the mask to fit
`GET_MODE_MASK(value1) & GET_MODE_MASK(src2)`.

Build: the old obj dir cpython-amiga/build/gcc/obj is configured with the stale path
/Users/spot/Code/cpython-amiga/... (fails). Use the scratch obj dir drvobj (configured from
/Users/spot/Code/UP-Term/cpython-amiga/build/gcc/src): `make -C <scratchpad>/drvobj all-gcc`.
Old cc1 backed up as scratchpad/bak-addx/cc1 and libexec/.../cc1.pre-addx-2026-10-16.

Regression test: `make test ONLY=gcc_mul_overflow_cc` (tests/test_gcc_mul_overflow_cc.py):
no conditional branch directly after addx in ov() for none/-m68020/-m68060, and the vamos run
of the linked program exits 0. Old cc1: 3 [ERROR] (vamos exit 2 = ov(31)). New cc1: 3 [OK]. Torture test under vamos: old cc1 aborts, new passes -O1 -O2 -Os -O3. Not run: dejagnu, rig.

# PR title

Keep the test of a register's high word after a DImode add.l/addx.l

# PR body

## Bug

`__builtin_mul_overflow (size_t n, 4, &ptrdiff_t c)` at -O1 and up (any CPU flag) emits

```
	moveq #0,d0
	move.l (8,a5),d3
	move.l d0,d2
	add.l d3,d3
	addx.l d2,d2
	add.l d3,d3
	addx.l d2,d2
	jne .L7
	tst.l d3
	jlt .L8
```

The `jne` has no `tst.l d2`. addx.l clears Z only for a non-zero result and otherwise leaves
it, so Z comes from `add.l d3,d3`: ov (31) returns 1. gnulib's xalloc_oversized uses this
(coreutils cp/du/ls: "memory exhausted"). -O0 and `-fbbb=-` are correct; `-fbbb=e` alone
drops the tst.

## Cause

- bbb-opts.c track_regs keeps one bit mask per 32-bit register. For a DImode shift on d2:d3
  it shifted d2's mask and stored it, losing the carry addx.l brings in from d3 (mask
  0xfffffffe, but bit 0 can be set). opt_elim_dead_assign put that mask as a REG_BIT_MASK
  note on `tst.l d2`.
- final.c deletes a compare with zero when cc_status.value1 is the same hard register and the
  REG_BIT_MASK note fits the compared operand's mode. The flags describe value1's mode
  ((reg:DI d2) here, whose flags are those of the last word op), and `1 << 32` is undefined
  for SImode.

## Fix

- track_regs: an operation wider than one register clears the masks of all its registers.
- final.c: require value1 to fit one word and the mask to fit both value1's and the compared
  operand's mode (GET_MODE_MASK).

## Test

gcc.c-torture/execute/mul-overflow-addx-1.c aborts without the fix and passes with it.

# Affected binaries (scan 2026-10-16, old cc1 builds)

Scanner: objdump -d, flag any `addx` whose next instruction is a Z branch (beq/bne/jeq/jne).
This finds only the add.l/addx.l shape; other `tst` deletions from the same REG_BIT_MASK
fault (sub-word sets) are not covered. 104 of 3666 files hit; vtcon's own handler and tools
are not in the list. upterm-ports kit/userland/bin (33): bsdcat bsdunzip chgrp chmod chown chroot cp cpio diff dir du env find grep iconv install ln ls lua luac mv numfmt patch readlink realpath rm shred shuf sort stat tar vdir xargs 
The rig2 userland copy and upterm-ports build/obj (coreutils lib hash.o, mbsstr.o) hit the same.
Rebuild these with the new cc1, then drop the tools/m68k-cc link-flag workaround in upterm-ports
and re-run cp/du/ls on a rig.
