# R1: which sysroot libs need an FPU build, and libm (2026-10-27)

Plan: thoughts/shared/plans/2026-10-27-ports-fpu-variants.md. Read-only host research. Toolchain: ~/opt/amiga, m68k-amigaos-gcc 6.5.0b, `-mcrt=ixemul` (tools/m68k-cc adds it).

## Verdict
1. Linking FPU objects against the libs the compiler picks today is NOT ABI-safe. `-m68060 -m68881` selects the "." (68000 soft-float) multilib: libm.a, libc.a, libgcc.a are all soft-float. The FPU callee ABI returns double/float in fp0; the soft libm returns in d0/d1. Silent wrong results, not a link error.
2. The installed toolchain already ships a 68881 set, but only for 68020: `libm020/libm881/` (ixemul libc.a, libm.a, libixcompat.a; gcc libgcc.a). Selected by `-m68020 -m68881`. It runs on a 68040/060 only if the 060 has an FPU and the code uses only opcodes the 68060 hardware implements. Transcendentals (fsin, fpow helpers etc.) are 68060 line-F traps, emulated by the 68060.library/SoftFloat support. So it works but is not 060 optimal.
3. Minimal set needing an FPU variant (everything that has float code or whose callers pass double):
   - libm (ixemul): required. 4117 FPU opcodes in the 881 libm, 0 soft calls; the plain libm has 378 soft calls, 1 FPU opcode.
   - libgcc: required (the 881 variant has 1162 FPU opcodes; plain has 87 soft calls). Provides __floatsidf/__fixdfsi replacements and the like.
   - libc (ixemul): required. Has float code (printf/strtod/qsort-style): 881 libc has 30 FPU opcodes and 20 soft calls vs plain 1 and 26.
   - libixcompat (ixemul) and the port's libixcompat (sysroot/UP-Term/lib/libixcompat.a, appended by tools/m68k-cc): the ixemul copy has 76 FPU opcodes in the 881 variant vs 23 soft calls in the plain one. The wrapper's own copy needs the same variant treatment.
   - Port libs (zlib, ncurses, libarchive, ...): only if their sources have float/double code; zlib and ncurses have almost none, but any that does must be rebuilt with the same flags (they are in the plan's second sysroot anyway).
   - libamiga/libnix not involved with -mcrt=ixemul (libamiga.a linked, no float).
4. libm does come with it, from the same multilib: no need to build a libm from libsrc if a 68020/881 set is acceptable. A true 68060 set needs the ixemul build.

## Evidence
- Multilibs (`m68k-amigaos-gcc -print-multi-lib`): ".", libb, libm020, libm020/libm881, libb/libm020(+libm881), libb32/libm020(+libm881). The only FPU multilib is mcpu=68020 + m68881. There is no 040/060 multilib.
- `-print-multi-directory` and `-print-file-name=libm.a` (with -mcrt=ixemul):
  - `-m68020`: libm020, .../ixemul/lib/libm020/libm.a? (it resolved to m68k-amigaos/lib/libm020/libm.a path form; libgcc libm020/libgcc.a)
  - `-m68020 -m68881`: libm020/libm881, ixemul/lib/libm020/libm881/libm.a, libgcc libm020/libm881/libgcc.a
  - `-m68060 -m68881` (also -m68040 -m68881): ".", ixemul/lib/libm.a, libgcc.a, i.e. SOFT-FLOAT.
  - `-m68060 -m68881 -dM -E` defines __HAVE_68881__.
- Opcode counts, `m68k-amigaos-objdump -d | grep -c` (FPU opcodes / soft calls like __adddf3):
  ixemul/lib/libm.a 1/378; libm020/libm881/libm.a 4117/0; libc.a 1/26 vs 881 30/20; libixcompat.a 0/23 vs 881 76/0; libgcc.a 0/87 vs libm020/libm881/libgcc.a 1162/23; libm020/libgcc.a 0/87.
- ABI: compiling `double f(double x){return sin(x)*2.0;}` -O2: `-m68020` -> `jsr _sin; ... jsr ___adddf3` (result in d0/d1). `-m68060 -m68881 -D__NO_MATH_INLINES` -> `jsr _sin; fdadd.x fp0,fp0` (caller reads the result from fp0; args stay on the stack). Soft libm sin never writes fp0. This is the classic trap, confirmed.
- Wrapper trap 1: tools/m68k-cc, a single-step compile+link (`m68k-cc -m68060 -m68881 x.c -lm`) strips -m680[0-6]0 for the WHOLE call (link=1), so the assembler runs at 68000 and fails ("fmove.d ... needs 68020"). Recipes that compile with -c separately are fine.
- Wrapper trap 2 (measured): `m68k-cc -O2 -m68060 -m68881 -c m.c`, then `m68k-cc -m68060 -m68881 m.o -lm` links ixemul/lib/libc.a (45 members), ixemul/lib/libm.a (2), libgcc.a: the soft set, no error. By contrast, direct `m68k-amigaos-gcc -mcrt=ixemul -m68060 -m68881` single-step linked libm020/libm881/libc.a and libm.a (45+2 members), so the multilib choice differs between compile-driver and link-driver paths; the wrapper (which drops -m68060 at link but keeps -m68881) gives the soft set. Do not rely on either; pin the library path explicitly.
- ixemul-vtcon builds FPU variants: /Users/spot/Code/UP-Term/ixemul-vtcon/Makefile.in:74-77 `CPU-FPU-TYPES = 68000.soft-float notrap.soft-float 68020.soft-float 68020.68881 68040.soft-float 68040.68881 68060.68881`. So a 68060.68881 libc/libm exists as a build target, but the installed toolchain only has the 68020.68881 result in libm020/libm881 (ixemul/lib/libm020/libm881, dated 19 Oct); there is no 040/060 directory and gcc has no multilib to select one.
- Sysroot today: sysroot dir /Users/spot/Code/UP-Term/upterm-ports/build/sysroot does not exist in this checkout (not built here); libs link from ~/opt/amiga/m68k-amigaos/ixemul/lib/libc.a and libm.a (68000 soft, the "." set) and, for -m68020, libm020. The wrapper adds build/sysroot/UP-Term/lib/libixcompat.a when present.

## Consequence for the plan (decision for the owner, not taken here)
- `-m68881` must NOT simply stay off the link line while `-m68060` is dropped: either (a) link with explicit `-L` to a prepared FPU lib dir (libc/libm/libgcc/libixcompat FPU builds) ahead of the default search, which the second `$(SYSROOT)-fpu` tree can hold, or (b) use the existing libm020/libm881 set by compiling and linking with `-m68020 -m68881` and `-m68060`-only tuning (`-mtune=68060`), losing 060 opcode use, or (c) build 68060.68881 from ixemul-vtcon Makefile.in into a copy and install it as the FPU tree's lib dir. (c) matches the plan's "never edit ixemul-vtcon in place".
- Whatever is chosen, test_m68k_cc.sh needs a case asserting that an FPU link resolves libm/libc/libgcc to the FPU set (map-file check), plus a runtime check `pow(2.5,2)` on the 060 rig, because a mismatch gives no link error.
- Not tested: running m_fpu on a rig (no rigs by task rule). The runtime result of the d0/d1 vs fp0 mismatch is derived from the generated assembly, not executed.
