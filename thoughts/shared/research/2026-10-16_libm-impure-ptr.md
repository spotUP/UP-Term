---
date: 2026-10-16
topic: -lm links newlib's libm.a (undefined _impure_ptr) on ixemul ports
status: fixed on branch fix/libm (worktree ~/Code/UP-Term/ixemul-vtcon-libm), not installed, not pushed
---
# Cause
- tools/m68k-cc runs `m68k-amigaos-gcc -mcrt=ixemul`. Link line: `-L ixemul/lib ... -L m68k-amigaos/lib/ ... -lm -( -lc -lamiga -lgcc -lm -)`.
- ixemul/lib has libc.a and libixcompat.a but NO libm.a, so `-lm` falls through to m68k-amigaos/lib/libm.a: newlib's libm (346 members, 48 reference `_impure_ptr` for errno). ixemul.library does not export `_impure_ptr`: link fails (jq config.log 5592-5601, w_sqrt/w_log).
- ixemul's own libc.a (740 symbols) already defines the classic math set: acos asin atan ceil cos cosh exp fabs floor frexp isinf isnan ldexp log log10 modf pow sin sincos sinh sqrt tan tanh. Plain -lm is therefore needless for those, but the `-lm` the build systems add pulls newlib's archive first.
- Missing from ixemul libc.a and only in newlib libm (double, non-complex): log2 exp2 fmod(now libixcompat) hypot cbrt trunc rint lround copysign fmax fmin expm1 log1p erf tgamma ... (list: scratchpad nl.txt minus ix.txt). libixcompat already has fmod, round, atan2.
- Same cause: awk and joe recipes drop -lm by hand (comments name _impure_ptr); Lua's LUA_GAPS log2 macro exists because log2 is only in newlib's libm.
- Specs/search order: spec cannot fix it (`%{lm:-lm}` just adds -lm); user -L wins over the default dirs, ixemul/lib wins over m68k-amigaos/lib. So a libm.a in ixemul/lib is the right shadow.

# Fix (level: ixemul-vtcon compat, installed into ixemul/lib)
Branch fix/libm, commit 6e93e42 in /Users/spot/Code/UP-Term/ixemul-vtcon-libm:
- compat/c99.c: log2, exp2 (per-function members like fmod/round).
- compat/Makefile: libm.a = c99_fmod c99_round c99_log2 c99_exp2; `make install` copies it to $(SDK)/lib/libm.a; test/libm_link (host-only link + nm assert: no _impure_ptr, log2 present).
- Verified: without -L. the same source shows 6 `_impure_ptr` undefined refs; with it the link passes, nm clean.
Not done: more C99 functions (hypot cbrt trunc rint lround copysign fmax fmin expm1 log1p ...) - add to C99_FUNCS/LIBM_FUNCS when a recipe needs them; float (`f`) variants.

# Install step (NOT run; needs approval, writes outside the repo)
    make -C /Users/spot/Code/UP-Term/ixemul-vtcon-libm/compat install
(copies libixcompat.a and libm.a to ~/opt/amiga/m68k-amigaos/ixemul/lib). Then re-run the affected configures.

# Recipes that change after install
- lua (fp4c pkgs/lua/recipe.mk:18-19): drop the `log2(x)=...` macro from LUA_GAPS (log2 now links; mind that the macro's replacement was log(x)/ln2, identical to the new c99 log2).
- awk (pkgs/awk/recipe.mk:16) and joe (pkgs/joe/recipe.mk:31): the "drop -lm" workaround and comments can go; check other pkgs that hand-edit LIBS.
- jq (upterm-ports/pkgs/jq/recipe.mk has no -lm handling; its configure checks fail on libm today): its configure `checking for sqrt in -lm` etc. now pass, so HAVE_ math defines (HAVE_LIBM, HAVE_SQRT, HAVE_POW, ...) become 1; jq then uses libm paths it had been compiling out: expect behaviour change, retest.
- grep of upterm-ports and fp4c pkgs for -lm, log2, LUA_GAPS, ac_cv_lib_m, HAVE_LOG2/SQRT/POW/LIBM found only the files above (grep proves spelling only).

## C99 completion

Worktree ixemul-vtcon-libm, branch fix/libm. libm.a now holds the whole C99 <math.h> set: 57 functions x (double, f, l) = 171 names (test/libm_names.txt), minus the 27 ixemul libc.a exports.
- Missing list: C99 names minus `nm` of ixemul/lib/libc.a (722 T/W symbols). No newlib or musl source is on disk (only the compiled toolchain libm.a), so the fdlibm members were taken as objects.
- Source of each: (a) 81 members of the toolchain's newlib libm.a (Sun fdlibm, permissive): cbrt, trunc, rint, lround, copysign, fmax, fmin, expm1, log1p, erf, ... chosen by closing undefined symbols over that archive and rejecting any path to _impure_ptr / __fdlib_version / matherr (LIBM_NEWLIB in compat/Makefile). (b) compat/libm.c: hypot, acosh, atanh, remainder, lgamma, tgamma on newlib's __ieee754_* kernels; 17 float forms newlib wraps with errno (acosf, expf, powf, ...) via the double function; nexttoward(f/l). (c) 57 long double forms via the double ones (96-bit on m68k; result has double precision; fabsl and copysignl exact). sqrtl is `sqrt(x)` in libm.c like the other long double forms (newlib's sqrtl parses the x86 layout and returns garbage on m68k).
- Tests: `make test/libm_link` links all 171 names, no _impure_ptr, every symbol present: PASS. test/libm_num (166 checks: 0, -0, inf, nan, negative cbrt) built for the Amiga but NOT run: vamos stops at "Need at least version 47 of ixemul.library". Its expected values were checked by compiling the same file against the host libm: 166 checks, 0 failures.
- Not done: j0 j1 jn y0 y1 yn gamma scalb drem exp10 (not C99). Long double has no extra precision. The older c99.c fmod (x - n*y) and round (floor(x+0.5)) stay: fmod is inexact for large quotients, round(0.49999999999999994) is wrong. libm.a is the 68000 build only (no 020/060 variants).
