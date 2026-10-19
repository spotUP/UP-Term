---
date: 2026-10-18
topic: Amiga IDE spike findings - Scintilla 5 + Lexilla + Scinterm on 68k
status: draft (numbers and go/no-go pending S5)
source: thoughts/shared/research/2026-10-12_amiga-ide-brainstorm.md (section "Spike")
ledger: thoughts/shared/plans/2026-10-18-ide-spike.progress.md
---

# IDE spike findings

Question: can Scintilla 5 + Lexilla + Scinterm be built with our Amiga toolchain and
run well enough on an 040?

## Answer

PENDING S5 (020 floor timing on rig 1). Build question: yes, with shims, no Scintilla source patched.

## Build result (S1-S3): builds

- Scintilla 5.6.7, Lexilla 5.5.4, Scinterm 6.0 (tag scinterm_6.0). Work dir `~/Code/UP-Term/ide-spike/` (throwaway).
- gcc 16.1.1b (`~/opt/amiga16`): `-m68040 -O2 -fno-builtin -fno-tree-loop-distribute-patterns -std=c++17`,
  exceptions and RTTI on (Scintilla throws, Lexilla uses dynamic_cast). gcc 6.5 cannot build C++17.
- gcc 16 has no ixemul multilib (`-mcrt=ixemul` silently falls back to its own libc and crt0).
  Build uses `-nostdinc` / `-nostdlib`: ixemul crt0.o and headers from the gcc 6.5 install,
  libstdc++/libgcc from gcc 16, our ncursesw port.
- Shims needed (all in `ide-spike/compat/` and `patches/`):
  - `ixcompat_cxx.h` forced include: C99 and wide-char declarations ixemul headers lack.
  - single-thread `<mutex>`, `<thread>`, `<future>` (no gthreads; Scintilla layout threads run inline).
  - `ixlink.cxx`: link stubs libstdc++ 16 expects from a newlib-style libc.
  - `-DPTRDIFF_DOESNT_ALIAS_INT` (ixemul ptrdiff_t is long).
  - `ixcxxrt.c`: C++ runtime start-up for ixemul (see next section). Without it the binary crashes at start.
- Binary size, stripped: all 125 lexers 4,786,560 B; 9-lexer IDE build ("lite") 2,593,060 B.

## Start-up crash and fix

- Symptom: both variants died at start with ixemul "EMT trap", nothing drawn.
- Root cause: ixemul crt0 never runs C++ global constructors and never registers EH frames.
  bebbo's ld collects `.list___CTOR_LIST__`, `.list___DTOR_LIST__`, `.list___EH_FRAME_BEGINS__`,
  `.dlist___EH_FRAME_OBJECTS__`, but the list headers, terminators and walkers live in the libnix
  crt0 / newlib cxxglue.o only. Statics stayed zero; the first call through one jumped into garbage.
- Fix (necessary, not sufficient: crash persists after it, second fault under hunt): `ide-spike/compat/ixcxxrt.c`, linked right after crt0.o with `-Wl,--wrap=main`. It supplies the
  list headers and terminators, registers the EH frames, runs ctors in libnix order, and registers dtors with atexit.
- Consequence for any C++ on our ixemul stack (not only the IDE): this runtime file is required. It belongs
  in the toolchain / ports tree, not in each app.

## Compiler bug: gcc 16.1.1b C++ does not save callee-saved registers

- Same source compiled with `-x c` saves d2; with `-x c++` it writes d2 (divsl.l) and never saves it.
  Independent of -O level, exceptions, RTTI, unwind tables, frame pointer and bbb. Verified in the main session.
- Every gcc16 C++ object is affected, including the prebuilt libstdc++.a (static scanner
  `ide-spike/tools/scan_csr.py`: libscintilla 697 of 2701 functions flagged, libstdc++ 2086 of 7136).
- Effect seen: a libstdc++ static constructor clobbered d2, so main got argc 0. Wild jump to 0x10000838
  inside `scintilla_new` is suspected to be the same bug (not yet tied to one call).
- Owner decision 2026-10-19: fix in the gcc16 source and rebuild gcc16 + libstdc++ (in progress).

## Numbers (S4/S5)

Harness: `scidemo --bench FILE` on `vtcon/shell/sh_expand.c` (100510 B). Times measured to doupdate()
return; UP-Term render after that is excluded. Machine: rig 1 `--a1200` (68020, real speed) = floor.
Floor passes means an 040 passes. Floor fails means real-hardware numbers are needed (no scaling guess).

| Measure | Pass bar | full (125 lexers) | lite (9 lexers) | Verdict |
|---|---|---|---|---|
| Open + highlight 100 KB C | <= 2 s | PENDING | PENDING | PENDING |
| Keystroke to refresh | <= 100 ms | PENDING | PENDING | PENDING |
| Page scroll | record | PENDING | PENDING | - |
| RAM (AvailMem drop) | <= 4 MB | PENDING | PENDING | PENDING |

Medians of 3 runs. Raw lines: `ide-spike/results/<variant>-a1200.txt`.

## Lexer coverage (S6)

| Language | Lexilla 5.5.4 | Status |
|---|---|---|
| C / C++ | LexCPP | present |
| 68k asm, Motorola syntax | LexA68k | present |
| Generic asm | LexAsm | present |
| bash / sh | LexBash | present |
| Python | LexPython | present |
| Lua | LexLua | present |
| Markdown | LexMarkdown | present |
| JSON | LexJSON | present |
| Makefile | LexMake | present |
| ARexx | none | to write |
| AmigaDOS script | none (LexBatch is MS-DOS only) | to write |
| AmigaGuide | none | to write |

## Go / no-go

PENDING S5.
