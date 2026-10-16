---
topic: libm_num rig 3 - root cause of the 25 failures (desk analysis, nothing run on the rig)
date: 2026-10-16
---
# 25 libm_num failures: causes and fix layers

Build facts: test and libm.a are 68000 soft-float (`-mcrt=ixemul`, no -m68881; libgcc 6.5.0b). Every double
compare is a call to `__eqdf2/__gtdf2/__ltdf2/__nedf2` in libc.a, which are 8-byte stubs
(`move.l _ixemulbase,a0; jmp -1416(a0)`) into ixemul.library. The compare therefore runs inside the library
(ixemul-vtcon-gaps/general/arith.c), not in libm.a. Failure list: scratchpad run6.fails (25 rows).

## Group A: 19 rows "Inf, want Inf" / "-Inf, want -Inf" (test/libm_num.c:35-41 same(), rows 63-99)
Rows: hypot(INF,NAN), cbrt(+-INF), trunc(INF), fmin(-INF,3), expm1(INF), log1p(-1), log2(0), acosh(INF),
atanh(+-1), tgamma(0,-0,INF), lgamma(0,INF), logb(0,-INF), logf(0). Every row whose WANT is +-Inf fails; every
row with Inf as input but finite want (fmax(-INF,-5), expm1(-INF), exp2(-INF), erf(-INF), fmod(5.5,INF)) passes.
So the libm functions are right (the printed got is Inf); same() judges them wrong.
Mechanism (verified in disassembly, `m68k-amigaos-gcc -O2 -fno-builtin -S test/libm_num.c`): same() is inlined
with a constant want = +Inf; gcc folds `got == Inf` into `got > DBL_MAX` (pushes 0x7fefffff,0xffffffff, calls
`___gtdf2`, jumps to FAIL on result <= 0; num.s lines ~6700). The -Inf rows do the same with `___ltdf2` against
-DBL_MAX. So the rows fail iff ixemul.library's `__gtdf2(Inf, DBL_MAX)` returns <= 0 (and `__ltdf2(-Inf,-DBL_MAX)` >= 0).
Library side (general/arith.c): two builds. The `__HAVE_68881__` build uses C `a > b` (FPU compare, correct for
Inf). The soft build (lines 373-644) forwards to Amiga `IEEEDPCmp` via ieeedpcmp(); mathieeedoubbas has no Inf
(exponent 0x7ff is just a big number) and the likely behaviour is Inf comparing equal/less-than DBL_MAX.
UNVERIFIED which build the rig's ixemul.library is, and what IEEEDPCmp returns; nothing was run.
Discriminating probe (rig, 10 lines): call `__gtdf2(INF, DBL_MAX)`, `__eqdf2(INF, INF)`, `__ltdf2(-INF,-DBL_MAX)` via
volatile doubles; print results. Also would show that `x == INFINITY`, isinf() (fabs(x) > DBL_MAX) are broken for
every program on that library, not only this test.
Fix layer: ixemul.library general/arith.c soft compare: do the double compare on the bits (NaN: exponent 0x7ff and
nonzero mantissa gives unordered result; otherwise sign-magnitude integer compare) instead of IEEEDPCmp, rebuild the library,
reinstall on the rig. NOT a libm.c or test fix: reworking same() to dodge the compare (bit test) would hide a library bug that
breaks isinf() for users. A test-only supplement is fine (one row that asserts `__builtin_isinf` / `x > DBL_MAX` for Inf).
Cheaper first step: the probe above, to decide between "soft build" and "other".

## Group B: tgamma (3 rows: tgamma(5)=2.6489e10, tgamma(0.5)=5.885, tgammaf(4)=403.4)
compat/libm.c:86-103. The wrapper does `l = __ieee754_gamma_r(x,&sign); return sign<0 ? -exp(l) : exp(l)`.
newlib's er_gamma.o is `__ieee754_gamma_r(x,s) { return __ieee754_exp(__ieee754_lgamma_r(x,s)); }`
(disassembled: calls lgamma_r, then tail-jumps to __ieee754_exp), i.e. it already returns |Gamma(x)|.
exp() on top gives e^24, e^1.7725, e^6 exactly as observed. (The comment at libm.c:87 assumed gamma_r returns lgamma.)
Fix (libm.c, one line): `g = __ieee754_gamma_r(x,&sign); return sign < 0 ? -g : g;`. tgammaf/tgammal go through tgamma and follow.
The tgamma(0.0), (-0.0), (INF_) rows in Group A are the compare bug, not this.

## Group C: fdim(INF_, INF_) = Inf, want 0 (test line 86)
fdim comes from newlib's s_fdim.o (LIBM_NEWLIB, Makefile:79). Disassembly: after the NaN checks, `fpclassify(x) == FP_INFINITE`
jumps to a block returning +Inf (0x7ff00000) regardless of y (newlib returns HUGE_VAL if x is infinite). C99/glibc: x > y ? x-y : +0,
so fdim(Inf,Inf) = 0, fdim(Inf,-Inf) = Inf. newlib bug. sf_fdim.o (fdimf) has the same shape; fdiml in libm.c:561 calls fdim.
Fix (libm.c + Makefile): add LIBM_fdim (and fdimf) to libm.c: NaN passthrough, `return x > y ? x - y : 0.0`; add fdim/fdimf to LIBM_FUNCS,
remove s_fdim/sf_fdim from LIBM_NEWLIB. With Group A fixed the x>y compare on Inf must also be right (Inf > Inf false).
Add fdimf(INF,INF) to libm_num.c.

## Group D: sqrtl(2.25L) = 6.9e77 (test line 97)
sqrtl is newlib's lib_a-sqrtl.o, a generic software long-double sqrt written for x86 extended layout: it reads the exponent word at
byte offset 8 of the 12-byte operand (`move.w sp@(148)` of a stored copy at sp@(140), then `lsrw #1`, compare 0x7fff, `btst #0,sp@(149)`).
On m68k the sign/exponent word is at offset 0 of the 96-bit value, so it parses garbage. Not an ABI mismatch in libm.c (hypotl etc. in the same
archive pass/return via a0 the same way).
Fix (libm.c + Makefile): all other long double forms there are `return f((double)x)`; add `sqrtl(long double x){ return sqrt(x); }`
LIBM_sqrtl, add sqrtl to LIBM_FUNCS, remove sqrtl from LIBM_NEWLIB. Precision is double, like the siblings; test tolerance 0 holds for 2.25.
(2026-10-16_libm-impure-ptr.md line 35 notes "sqrtl is newlib's"; update it.)

## Group E: powf(0, 0) = NaN (test line 99)
powf (libm.c:249) is `(float)pow(x,y)`; pow is NOT in libm.a: it resolves to libc.a pow.o = stub into ixemul.library (LVO -1848),
which is compiled from include/math-68881.h:374: `x == 0` with `y <= 0` returns NaN and sets EDOM, so pow(0,0) = NaN
(C99: 1) and pow(0,-1) = NaN (C99: +Inf). Same for powl (libm.c:609).
Fix, either: (1) libm.c defines pow on newlib's `__ieee754_pow` (lib_a-e_pow.o; its undefined refs are only soft-float helpers,
fabs, nan, scalbn, __ieee754_sqrt: no _impure_ptr) and adds pow to LIBM_FUNCS; compat libm.a precedes libc.a so it wins, consistent with
tgamma/hypot. Or (2) fix pow in math-68881.h / ixemul.library (x==0: y==0 -> 1, y<0 -> +-Inf). (1) fits this archive's pattern and needs no
library rebuild; (2) also fixes classic users that skip -lm. Recommended: (1) now. Add pow(0,0), pow(0,-1), pow(-8,1/3.)=NaN rows to libm_num.c.

## Count check
A 19 + B 3 + C 1 + D 1 + E 1 = 25. A counted from the run6.fails rows with Inf want (19; 11 listed first in the file plus tgamma x3, lgamma x2, logb x2, logf).
Counted from the table: the test has exactly 19 rows with want = +-INF_ (excluding fdim, whose want is 0).

## Order
1. B, C, D, E in compat (libm.c/Makefile/test rows), build, selfcall check; 2. rig probe for A; 3. fix arith.c, rebuild ixemul.library.

## Results: fixes B-E (ixemul-vtcon-gaps fix/libc-gaps 5ea2c29), rig 3, 2026-10-16
- Test table now 182 rows (added fdimf(INF,INF), pow(0,0), pow(0,-1)). Same source, rig 3 (68060), old installed libm.a
  (backup in scratchpad bak-libm2/): 28 failures; new libm.a (installed to ixemul/lib): 22 failures.
- Fixed on the rig: tgamma(5), tgamma(0.5), tgammaf(4), sqrtl(2.25L), powf(0,0), pow(0,0). Host evidence: old libm.a defines no _pow
  (pow came from the libc stub), newlib's _fdim and _sqrtl; new libm.a defines _pow, _powf, _fdim, _fdimf, _sqrtl, and test/libm_link
  resolves pow/powf from it. make test, selfcall check and cleancheck pass.
- 22 remaining, all the soft-float Inf compare bug (group A): the 19 original "Inf, want Inf" rows, plus pow(0,-1) = Inf, want Inf
  (value is correct, the compare lies), plus fdim(INF,INF) and fdimf(INF,INF) = Inf: their `x > y` is __gtdf2(Inf, Inf), which is
  also wrong, so C/D-style fdim cannot pass until arith.c is fixed. No workaround added.
- Probe test/dfcmp_probe.c (rig 3, 68000 soft-float):
  __gtdf2(INF, DBL_MAX) = 1; __eqdf2(INF, INF) = 1 (should be 0); __ltdf2(-INF, -DBL_MAX) = -1; __eqdf2(NaN, NaN) = 1;
  x == INFINITY: 1; isinf(x): 1. So __eqdf2 on equal infinities returns nonzero; the compare helper in ixemul arith.c is the bug.
