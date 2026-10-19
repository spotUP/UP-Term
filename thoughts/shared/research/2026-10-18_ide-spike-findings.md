---
date: 2026-10-18
topic: Amiga IDE spike findings - Scintilla 5 + Lexilla + Scinterm on 68k
status: final (go on 060; 020 + 68882 fails; 040 needs real hardware)
source: thoughts/shared/research/2026-10-12_amiga-ide-brainstorm.md (section "Spike")
ledger: thoughts/shared/plans/2026-10-18-ide-spike.progress.md
---

# IDE spike findings

Question: can Scintilla 5 + Lexilla + Scinterm be built with our Amiga toolchain and
run well enough on an 040?

## Answer

Builds: yes, with shims, no Scintilla source patched. Runs correctly: yes, after the C++ start-up runtime,
the gcc16 callee-saved-register fix and a `__stack` definition.
Performance: go on a 68060, no-go on a 68020 + 68882, open on an 040 (needs a real-hardware measurement). The first slow numbers (2.8 s per key on a 68060) came from a
soft-float build: gcc16 `-m68040` emits double arithmetic as ixemul.library calls, and Scintilla lays out in
double. Built with `-m68020 -m68881`, a key costs 23 ms on the 68060, under the 100 ms bar, and 645 ms on a
68020 + 68882 at real speed, over it; two Scinterm patches brought the 020 key to 76 ms (pass), but open
(9.5 s) and scroll (1.6 s) still fail there. Without an FPU
(stock A1200) the IDE stays at soft-float speed and is not usable.

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

### Root cause: soft-float through ixemul.library (2026-10-19)

The spike was built with `-m68040`. gcc16 m68k-amigaos treats `-m68040` (and `-m68060`) as soft-float: every
double operation becomes a libcall (`___adddf3`, `___muldf3`, ...), and in our link those resolve to stubs
that jump through ixemul.library vectors. Scintilla's layout type `XYPOSITION` is `double`, so every
position, width and wrap calculation of every repaint went through a library call.

Evidence:

- The paint cost is real CPU work, not a wait: with the JIT off it rises from 2.9 s to 7.7 s per key.
- The same source built natively on macOS paints a key in under 1 ms, with a flat profile, so the
  pathology is in the Amiga build, not in Scintilla.
- `nm` on the Scintilla objects shows `U ___adddf3` / `___muldf3` / `___divdf3`; `-m68040 -mhard-float` or
  `-m68020 -m68881` emits FPU instructions instead.
- Rebuilt with `-m68020 -m68881`, no other change, same rig, back to back (rig 4, `--max`, 68060 + JIT):

| Build | Open | Key | Scroll | Key paint |
|---|---|---|---|---|
| soft-float (`-m68040`) | 3279 ms | 3223 ms | 3241 ms | 3223 ms |
| FPU (`-m68020 -m68881`) | 280 ms | 23 ms | 60 ms | 23 ms |

Key cost drops 140x, under the 100 ms bar on the 060. Results: `ide-spike/results/scidemo-lite-060.txt`
(FPU) and `scidemo-lite-sf-060.txt` (soft-float).

Two toolchain issues the FPU build exposed:

- gcc16 installs its own `math-68881.h` in its include dir, older than ixemul's (no `log10f` and the other
  2026 additions). It comes first on the include path, so `<cmath>` fails under `-m68881`. ixemul's
  `math.h` includes it with `<>`; including `"math-68881.h"` would make ixemul's copy win. The spike
  worked around it with an include dir ahead of gcc16's that links to ixemul's header.
- `round` in libixcompat.a is soft-float compiled and returns its double in d0/d1; an FPU caller reads fp0
  and gets garbage, silently. The spike's `compat/ixcompat_cxx.h` defines `round` inline (fintrz) under
  `__HAVE_68881__`. Any libixcompat function returning double has the same problem for FPU callers.

Both fixed in the toolchain on 2026-10-19 (ixemul-vtcon f4e9aa4 and 9b2be03, not pushed): ixemul's math.h now
includes its own math-68881.h, and FPU archives (installed in the libm020/libm881 multilib dirs) give every
double-returning ixemul call and libixcompat/libm function an fp0 result. The spike now links them (env.mk
IXLIB/IXCOMPAT) and the inline `round` is gone.

Rig 4 `--a1200` check of the FPU-archive build (2026-10-19 ~11:40): compat `test/fpret_test` 13 of 14 OK; the one
failure, `log2(8) = 2.9999999999999996`, was a rounding error in log2 itself (log(x)/ln 2), not a wrong return
register, and is fixed by ixemul-vtcon 44264b3 (rig rerun open). scidemo-lite BENCH, 3 runs: open
9453/9338/9662, key 77/78/76, scroll 1537/1537/1533 ms, the same as before the archives (9610/74/1536), so the
archives fix correctness, not speed (`ide-spike/results/scidemo-lite-a1200.rig4.txt`).

Input lag on 020 + 68882 (rig 4 `--a1200`): key 645 ms down to 74 ms with two Scinterm patches (partial
repaint; fast fill and ASCII width in PlatCurses), see `ide-spike/results/input-lag.md`.

Consequence: an IDE built this way needs an FPU (68881/68882, 040, 060). A stock A1200 (68020, no FPU)
would need the soft-float build, at the speed above.

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

Built with `-m68020 -m68881` (FPU required):

| Machine | Open (<= 2 s) | Key (<= 100 ms) | Scroll | RAM drop (<= 4 MB) | Verdict |
|---|---|---|---|---|---|
| 68060 + JIT (rig 4 `--max`) | 280 ms | 23 ms | 60 ms | pass | go |
| 68020 + 68882, real speed (rig 2 `--a1200`, 2026-10-19) | 9523 ms | 76 ms | 1568 ms | 762 KB | no-go (open, scroll) |
| 68040 | not measured | not measured | not measured | - | needs real hardware |
| no FPU (soft-float build) | 3.0-3.3 s on a 060 | 2.8-4.5 s on a 060 | - | - | no-go |

020 split (11:43 FPU-archive rebench; `ide-spike/results/scidemo-lite-a1200.txt` was overwritten at 12:05 by a rig 3 probed run, see the ledger): key paint 52 ms + terminal 24 ms;
open paint 7848 ms + terminal 1675 ms; scroll paint 438 ms + terminal 1130 ms. The key passes. Scroll is now
dominated by the terminal (the screen visibly draws line by line); open is dominated by Scintilla paint.
Before the input-lag patches: key paint 612 + terminal 33 ms (`scidemo-lite-a1200.fixABC-roundinline.txt`).

The floor (020) fails, so by the rule under "Numbers" the 040 verdict needs a measurement on a real 040, not a
scaling guess.

Before the IDE is built for real, fix the two toolchain issues under "Root cause" in the toolchain itself,
not with the spike's workarounds: the stale gcc16 `math-68881.h`, and libixcompat functions that return
double in d0/d1 to FPU callers.
