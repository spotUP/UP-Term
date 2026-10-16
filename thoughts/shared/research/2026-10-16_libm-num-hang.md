---
date: 2026-10-16
topic: libm_num hangs the Amiga (rig 3, 68060)
status: cause proven, fixed on ixemul-vtcon-libm fix/libm a1a4b6d (not installed)
---

# libm_num hang: sqrtf calls itself

## Hanging row

`sqrtf(9)` (row 167 of 179). Found with `-DLIBM_NUM_TRACE="VTC:libm_trace.txt"`
(fflush per row into rig 3's shared dir `build/rig3/vtc/`): the last line on
every hanging run was `167 sqrtf(9)`. Any sqrtf argument hangs.

## Cause

`compat/libm.c:209-214` is `float sqrtf(float x) { return (float)sqrt(x); }`.
Compiled by `compat/Makefile` rule `libm_%.o` without `-fno-builtin`, gcc 6.5
(bebbo) narrows `(float)sqrt((double)x)` to `sqrtf(x)`, so the member is:

    00000000 <_sqrtf>:
       0: linkw a5,#0
       4: movel a5@(8),sp@-
       8: jsr %pc@(0 <_sqrtf>)     ; itself
       c: unlk a5
       e: rts

Endless recursion; 4 bytes + frame per level runs the stack down through other
memory (no MMU on the 060 rig), so the whole machine dies: wasabid stops, ping
fails. Not an FPU issue: libm.a and the test are 68000 soft-float (the
disassembly has no FPU opcodes), nothing traps to 68060.library.

## Stack probe

`Stack 262144` then `VTC:libm_num`: still hangs (unbounded recursion eats any
stack). Stack is not the cause; no `__stack` cookie needed.

## Second defect found on the way

`test/libm_num` linked with `-L. -lm` resolved `-lm` to the INSTALLED
`~/opt/amiga/m68k-amigaos/ixemul/lib/libm.a` (ld --trace-symbol), not the
freshly built `compat/libm.a`, so a rebuilt archive was never tested. The rule
now links `libm.a` by path.

## Fix (a1a4b6d)

- `LIBFLAGS = -fno-builtin` on the `libm_%.o`, `c99_%.o` and `%.o` rules.
- `make selfcall` (in `make test`): `tools/selfcall_check.py libm.a`.
  Old archive: flags `_sqrtf`. New archive: `[OK]`. libixcompat.a is left
  out: `_trecurse` (newlib tdelete, intended recursion) and `_casemap`
  (uclass.c, a loop whose head is the entry) are true/false positives there.
- `test/libm_num.c`: `-DLIBM_NUM_TRACE="file"` row tracer.

## Rig 3 result after the fix

179 checks, 25 failures, exits normally, wasabid answers afterwards.
Failures are real numeric bugs, separate from the hang:
- 19 rows `= Inf, want Inf` (and -Inf): `got == want` false for infinities in
  the soft-float compare used by `same()`; suspect `__eqdf2`/`__nedf2` from the
  ixemul libc or libgcc on infinities. Not investigated.
- `tgamma(5) = 2.6489e10` (= e^24), `tgamma(0.5) = 5.885` (= e^1.7725),
  `tgammaf(4) = 403.4` (= e^6): tgamma applies exp() to a value that is already
  gamma; libm.c's tgamma wrapper over newlib's kernels.
- `fdim(INF_, INF_) = Inf, want 0`.
- `sqrtl(2.25L) = 6.9e77`: the long double argument/return convention in the
  wrapper is wrong (96-bit extended vs double).
- `powf(0, 0) = NaN, want 1`.

The Mac "179/179" ran the table against macOS libm, so it never tested this code.

## Rig notes

wasabid stopped listening ~1 s into each hanging run (the hang itself, not the
1-2 min quirk). One start (01:47) came up "up" but wasabid refused at once,
with a Retry/Cancel requester in retry.log; a stop+start fixed it. The host
disk was full (ENOSPC) around 01:46, likely related.

## Install (not done)

    make -C /Users/spot/Code/UP-Term/ixemul-vtcon-libm/compat install
