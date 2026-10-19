---
date: 2026-10-18
topic: Amiga IDE spike findings - Scintilla 5 + Lexilla + Scinterm on 68k
status: draft (go/no-go pending perf root cause)
source: thoughts/shared/research/2026-10-12_amiga-ide-brainstorm.md (section "Spike")
ledger: thoughts/shared/plans/2026-10-18-ide-spike.progress.md
---

# IDE spike findings

Question: can Scintilla 5 + Lexilla + Scinterm be built with our Amiga toolchain and
run well enough on an 040?

## Answer

Provisional (2026-10-19). Builds: yes, with shims, no Scintilla source patched. Runs correctly: yes, after
the C++ start-up runtime, the gcc16 callee-saved-register fix and a `__stack` definition.
Performance: no-go as built. On a 68060 (rig 4) a keystroke takes 2.8 s against a 100 ms bar (about 30-45x over),
so an 040 or 020 is worse. The final go/no-go depends on whether the perf hunt finds a fixable per-repaint
pathology (see the placeholder under Numbers).

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
- Fix (necessary): `ide-spike/compat/ixcxxrt.c`, linked right after crt0.o with `-Wl,--wrap=main`. It supplies the
  list headers and terminators, registers the EH frames, runs ctors in libnix order, and registers dtors with atexit.
- Consequence for any C++ on our ixemul stack (not only the IDE): this runtime file is required. It belongs
  in the toolchain / ports tree, not in each app.
- Second fault (solved 2026-10-19): a stack overflow. ixemul crt0 (`__stkext_startup`) gives main a stack of at
  least `__stack` bytes, but without a `__stack` definition main ran on the caller's small stack.
  - ncurses `initscr` (terminfo buffers on the stack) ran about 100 KB deep and overwrote the tail of the code
    hunk (libstdc++ codecvt code and vtables, text+0x244328..0x24662c). scidemo-lite crashed in a later virtual call.
  - The full build survived only by layout: its code hunk is 2.2 MB larger.
  - Fix: `extern "C" unsigned long __stack = 262144;` in `ide-spike/scidemo.cxx`. Any ncurses program on our
    ixemul stack needs this.
  - `Stack` in the calling shell does not reach the child started by vsh, so it cannot replace the definition.

## Compiler bug: gcc 16.1.1b C++ does not save callee-saved registers

- Same source compiled with `-x c` saves d2; with `-x c++` it writes d2 (divsl.l) and never saves it.
  Independent of -O level, exceptions, RTTI, unwind tables, frame pointer and bbb. Verified in the main session.
- Every gcc16 C++ object is affected, including the prebuilt libstdc++.a (static scanner
  `ide-spike/tools/scan_csr.py`: libscintilla 697 of 2701 functions flagged, libstdc++ 2086 of 7136).
- Effect seen: a libstdc++ static constructor clobbered d2, so main got argc 0.
- Fixed (2026-10-19) in the gcc16 source (patch e32636be, `df_regs_ever_live_p`), installed in `~/opt/amiga16`,
  with all 11 multilibs rebuilt. `ide-spike/tools/test_csr_cxx.sh` exits 0. The scanner shows the 937 pre-fix
  hits gone and 0 new.

## Numbers (S4/S5)

Harness: `scidemo --bench FILE` on `vtcon/shell/sh_expand.c` (100510 B). Times measured to doupdate()
return; UP-Term render after that is excluded. Machine: rig 1 `--a1200` (68020, real speed) = floor.
Floor passes means an 040 passes. Floor fails means real-hardware numbers are needed (no scaling guess).

| Measure | Pass bar | full (125 lexers) | lite (9 lexers) | Verdict |
|---|---|---|---|---|
| Open + highlight 100 KB C | <= 2 s | not measured, see below | not measured, see below | not measured, see below |
| Keystroke to refresh | <= 100 ms | not measured, see below | not measured, see below | not measured, see below |
| Page scroll | record | not measured, see below | not measured, see below | - |
| RAM (AvailMem drop) | <= 4 MB | not measured, see below | not measured, see below | not measured, see below |

020 run on rig 1 (A1200): lite did not finish within 1800 s, so there is no BENCH line. The full run is invalid
(the hung lite run held RAM:b.err). Not rerun, because the 060 already fails. Files `ide-spike/results/*-020.txt`
are invalid as noted.

### 68060 (rig 4, FS-UAE 68060, `--max`)

One run each, file `vtcon/shell/sh_expand.c` (100510 B). Values are ms per operation (key and scroll averaged
over 50 and 20 steps). Raw lines: `ide-spike/results/scidemo-060.txt` (full) and
`ide-spike/results/scidemo-lite-060.txt` (lite).

| Measure | Pass bar | full (125 lexers) | lite (9 lexers) | Verdict |
|---|---|---|---|---|
| Open + highlight 100 KB C | <= 2 s | 3170 ms | 3036 ms | fail |
| Keystroke to refresh | <= 100 ms | 2785 ms | 2767 ms | fail (about 30-45x over) |
| Page scroll | record | 2911 ms | 2609 ms | - |
| RAM (AvailMem drop) | <= 4 MB | 634 KB (drop_from_start 904 KB) | 635 KB (drop_from_start 919 KB) | pass |

Lite with split timers (a later run):

| Measure | Total | Paint | Terminal |
|---|---|---|---|
| Open | 3963 ms | 3933 ms | 30 ms |
| Key | 4464 ms | 4464 ms | 0 ms |
| Scroll | 4192 ms | 4192 ms | 0 ms |

"Paint" is `scintilla_send_key` + `scintilla_noutrefresh` (Scintilla edit, layout, Scinterm draw into the curses
buffer). "Terminal" is `doupdate`.

The cost is inside Scintilla, not the terminal. Open, key and scroll cost about the same, which suggests a
per-repaint constant rather than work that scales with the operation.

PENDING: performance root cause (agent perf hunt, scratchpad spike/perf.md).

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

Provisional: no-go as built. It builds and runs correctly, but a keystroke costs 2.8-4.5 s on a 68060 against a
100 ms bar (about 30-45x over), so an 040 or 020 is worse. The final go/no-go depends on whether the perf hunt
finds a fixable per-repaint pathology.

PENDING: performance root cause (agent perf hunt, scratchpad spike/perf.md).
