---
date: 2026-10-19
topic: gcc16 (m68k-amigaos 16.1.1b) C++ objects write callee-saved registers without saving them
status: fixed; cc1/cc1plus/cc1obj/lto1 and all 11 libstdc++/libgcc multilibs rebuilt with the fix (2026-10-19 07:13)
---

# gcc16 C++ callee-saved omission: root cause and fix

## Root cause

It is not a C++ front-end difference. The installed toolchain was a mixed build:

| binary in ~/opt/amiga16/libexec/gcc/m68k-amigaos/16.1.1b | mtime | has commit e32636be50? |
|---|---|---|
| cc1     | 5 Oct 21:39 | yes (`strings` shows `float-return-d0`) |
| cc1plus | 5 Oct 18:11 | no |
| cc1obj, lto1, collect2, driver | 5 Oct 18:11 | no |

The bug is in bebbo's m68k backend: `m68k_save_reg` (gcc/config/m68k/m68k.cc, before commit e32636be50 around
line 1040) returned false for a call-saved register when `m68k_is_reg_written(regno)` (m68k.cc ~line 941,
added by bebbo) found no non-artificial def in `DF_REG_DEF_CHAIN`. Those def chains are not rescanned after
register allocation, so a hard register that LRA/reload assigned (d2 as a scratch for `divsl.l`) had no def in
the chain. It was then used without being saved. Violated invariant: every call-saved hard register the
function writes must be in the prologue save mask, and the answer `m68k_save_reg` gives while reload lays out
the frame must not change as allocation proceeds.

The fix was already in the source tree as spotUP's commit `e32636be50` (2026-10-05, "m68k: save every live
call-saved register; -mfloat-return-d0"). It removes `m68k_is_reg_written` and leaves `df_regs_ever_live_p`
to decide, as stock gcc does. But only `cc1` was rebuilt and installed after that commit. cc1plus, cc1obj and
lto1, plus every target library (libstdc++, libsupc++, libgcc, built 5 Oct 18:58), came from the pre-fix
build. That is why `-x c` saved d2 and `-x c++` did not.

Proof that the language does not matter: on the old install, `g++ -x objective-c` (cc1obj, also pre-fix)
dropped the d2 save exactly like `-x c++`. With the freshly built cc1plus/cc1obj from the same source, all
three languages save d2.

Source tree: /Users/spot/Code/UP-Term/amiga-gcc15/projects/gcc (moved from ~/Code/amiga-gcc15; HEAD
e32636be50 on top of bebbo a280712478 "bump version", origin https://franke.ms/git/bebbo/gcc). No new clone
was needed.

Patch (the existing commit, as a file):
/private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/8399206b-d6b5-4592-96c6-fbacfc05ffaa/scratchpad/gcc/m68k-save-every-live-call-saved-reg.patch
(scratchpad, not durable; the commit itself is e32636be50 in the tree above).

## Build and install

- Backup of the whole old install: ~/opt/amiga16.bak-2026-10-19 (288M, cp -a, contains the pre-fix cc1plus
  dated 5 Oct 18:11). This is the only backup copy.
- Objdir: ~/Code/UP-Term/amiga-gcc15/build/gcc16-obj. Same configure line as `gcc -v`, except
  `--with-headers=<scratchpad copy of ~/opt/amiga16/m68k-amigaos/sys-include>` (the original newlib path is
  gone) and host `CFLAGS=CXXFLAGS=-O2` (no -g) to save disk.
- `gmake -j6 all-gcc && gmake install-gcc`. This replaced cc1, cc1plus, cc1obj, lto1, the driver and
  plugin headers. The version string is still "16.1.1b 20260816114048".
- The default multilib "." (the one `-m68040` uses, i.e. ide-spike) was built with the default -g -O2,
  `MULTIDO=true` in m68k-amigaos/libgcc and m68k-amigaos/libstdc++-v3, then `gmake MULTIDO=true install-strip`
  in each. The old installed libs had no debug symbols either (0 stabs), so stripped output matches.
- The first attempt at all 11 multilibs with -g ran the disk down to 996 MiB (about 650 MB per multilib) and
  was stopped.

## Verification

- Repro (`struct T{int tab;int start;}; int h(...)`), `g++ -x c++ -O2 -m68040 -S`: now
  `move.l d2,-(sp) ... move.l (sp)+,d2`, same as `-x c`.
- ~/Code/UP-Term/ide-spike/tools/test_csr_cxx.sh: exit 1 before, exit 0 after install.
- scan_csr.py, default multilib:

| file | before (flagged/total) | after |
|---|---|---|
| libstdc++.a | 2086/7136 | 1176/7136 |
| libgcc.a | 62/169 | 7/169 |
| libscintilla.a (rebuilt) | 697/2701 | 287/2701 |
| liblexillalite.a (rebuilt, full-lexer archive) | 911/2621 (same-shape archive, bbb_off.txt) | 543/2621 |
| scidemo.o | 1/3 | 0/3 |

- The remaining hits are scanner false positives, checked by hand:
  1. Data scanned as code. scan_csr treats every symbol as a function entry. In libstdc++, 1170 of the 1176
     hits are in `.rodata.*` sections (static const members such as `std::ios_base::end` in ios.o, 173 in
     limits.o, typeinfo names `__ZTS*`, `ctype_base::blank`). Only 6 hits are symbols in `.text*` sections.
  2. `_d_find_pack` (cp-demangle.o): the disassembly from 0x63c to rts never touches d2 or a2. The reported
     addresses (0xd2..) fall outside the function. False positive.
  3. `__GLOBAL__sub_I__ZN14__gnu_parallel9_Settings3getEv` (parallel_settings.o): `movel #1000,a4` and
     `clrl d4` are stores to absolute addresses 0xa4/0xd4 (relocated symbol addresses printed as bare hex).
     The scanner parses them as registers a4/d4. False positive.
  4. Also `__ZNSt12_Vector_baseIiSaIiEED2Ev.isra.0` (CellBuffer.o): the label sits in the middle of another
     function's loop, so the "entry" is not a real entry. False positive.
  The scanner bugs (rodata sections; bare-hex absolute operands that read as d0-d7/a0-a7; mid-function
  labels) need fixing before its counts mean anything. Before the fix, the scanner already over-counted for
  the same reasons.
- ide-spike: build/sci, build/lex, build/lexlite objects and the archives were deleted, then
  `gmake -j6 out/scidemo-lite` rebuilt them with the fixed cc1plus and linked against the fixed default
  libstdc++/libgcc (rc=0).

## Multilibs

The other 10 multilibs (libb, libb32, libm020, libm060, the m881 variants) were rebuilt without -g
(`CFLAGS_FOR_TARGET=CXXFLAGS_FOR_TARGET=-O2`; -g never changes gcc codegen, and the install strips) and
installed with `install-strip-target-libgcc install-strip-target-libstdc++-v3`. Outcome: see the
"Multilib result" line below.

## Still unverified / open

- scidemo-lite on rig 4 (BENCH line): not run. Rig use was excluded from this task.
- Other target libraries in ~/opt/amiga16 built with the pre-fix compilers: libobjc, libatomic, libgcov
  (all of them), and anything else compiled with the old cc1plus/cc1obj or with cc1 before 5 Oct 21:39. That
  includes libnix if it was built then (not checked), upterm-ports' libncursesw/libixcompat (C; timestamps
  not checked) and every C++ object outside ide-spike built with gcc16 before today.
- The scan_csr.py false-positive filters were done ad hoc (section filter) and are not in the tool.

## 2026-10-19 07:13: all multilibs rebuilt

All 11 libstdc++ multilibs and libgcc were rebuilt with the fixed cc1plus and installed into ~/opt/amiga16 (libstdc++.a dated 07:11-07:13). The rebuild ran with `enable_libstdcxx_pch=no`: the first attempt filled the disk writing precompiled headers, which the installed toolchain never shipped. The `std.cc ... has not been declared in 'std'` errors in the build are from the C++ module file and are ignored by make.

Verification:
- tools/test_csr_cxx.sh: exit 0 ("C and C++ callee-save masks agree").
- scan_csr.py on libm060/libstdc++.a: 937 flagged functions of the pre-fix build are gone, 0 new ones appear. Every remaining hit in the default, libm020 and libm060 multilibs is also in the default-multilib set checked earlier, whose code hits were all false positives (data decoded as code, absolute addresses read as registers).
