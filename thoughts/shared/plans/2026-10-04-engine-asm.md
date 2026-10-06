---
date: 2026-10-04
topic: ASM1 engine -- the engine's hot C paths cut down or moved to 68k asm, by vamos instruction counts
tags: [s1, asm1, speed, engine, 68k, stock-a1200]
status: implemented
---

# ASM1 (engine part): find the slow C, make it fast

Owner: "find all slow c code and replace it with super optimized asm". Scope: engine/vtengine.c,
engine/vtengine_68k.s, tests/amiga/engbench.c (+ host tests). Not render/, handler/, the rig.
Branch: feature/engine-asm (from main 91d5c42). No push, no merge.

Done = every workload's profile shows nothing left above ~5% that can be cut; plain text per
78-char line at or under creep's 863 instructions; host tests, engbench asm check (each new
routine proven able to fail), make amiga clean, all green at every commit.

Method: tools/prof68k.py (instructions under vamos -C 68020, engbench REPS 1 ONLY w PERS p).
Runs go through a scratch wrapper (scratchpad/prof: snap.py builds a snapshot of binary +
sources, runall.sh runs workloads one at a time under a lock, resumable, table.py makes the
table). Engine instructions = the program's total less engbench's own functions (the buffer
build, main's loop); vt_new is in it (5425, noise).

Workloads: 0 plain lines 65280 B (816 lines of 78 + CRLF), 1 newlines 12000, 2 colour a char
24102, 3 256-colour pair a cell 41729, 4 frame repaint 35820, 5 ins/del line 3600.

## Constraints / decisions
- 68000-safe asm (vtengine_68k.s is assembled without -m68020; DCTelnet builds the engine for
  a 68000): no long/word reads at odd addresses, no 020-only instructions.
- ONE vamos at a time (owner's rule: at most 2 emulators on the machine, the rig is one). The
  first baseline started 8 in parallel and overheated the Mac; the coordinator killed them.
  Profiling is now serial and resumable (a finished workload's file is skipped), the wrapper
  runs in its own process group and kills its vamos on a signal (no orphans); the asm checks
  take the same lock.
- C stays the reference (host tests); asm behind VT_ASM; sizeof(vt_cell) / layout unchanged.
- vt_line.chonly (the old spare byte): cells [0, used) differ from the default blank in ch
  only. Set by line_clear to the default blank, cleared by mark() and by every direct `used`
  raise (line_new, rewrap, reflow). VT_CHECK_USED checks it in the host tests.
- An erase to the row's end in the default blank marks only [x0, used) (the rest was that blank
  already); rows with images keep the full mark (the image must be redrawn away).

## Instruction table (engine instructions an input byte)

(filled from the profiles; see "Results")

## Found on the way
- main's engbench asm check fails: `asm: WRONG (41)` = vp_span_fast (render/painter_68k.s, the
  renderer's BFINS painter) differs from vp_span's C at bit phase x=1 under vamos 68020. Built
  from an untouched export of 91d5c42: same result. Not engine scope; engbench now reports the
  renderer's checks apart ("render asm: ...") so the engine numbers are not blocked by it.
- prof68k's "vtengine:?" bucket is the code at line 1: function prologues and epilogues
  (link/movem/rts). It is call overhead; fewer calls shrink it.
- engbench under ONLY 0-5 no longer runs the checks (they were 0.7-6.5 M instructions of a
  2-3 M instruction profile); ONLY 9 runs the checks alone.
- The first vt_asm_csi check missed a planted bug (6554 for 6553): no test value had 6553 before
  its last digit. Added 65530 / 65531; the plant then fails it (81).

## Checklist
- [x] B0 baseline profiles
- [x] P1 vt_asm_put_ch four bytes a long (17 instructions / 4 chars, was 36); check 60-67 -- d69344e
- [x] P2 chonly lines: a clear of plain text resets ch only (vt_asm_ch_blank, 6 / 4 cells);
      check 68 -- 6680b31
- [x] P3 put_ascii_run: mark() / row_cols inline; vt_feed tests the ground state once -- 6680b31
- [x] P4 scroll_up: pend_prepare / ovf_drop only when there is something to do -- 6680b31
- [x] E1 csi_fast: parameter in a register, no clear_params call, SGR / CUP / EL / ED direct -- 2c50afc
- [x] E2 sgr: style_index only for a rare style -- 2c50afc; 38;5;n / 48;5;n in place -- 8abe0b1
- [x] E3 erase_cells to the row's end: only [x0, used); unwide an inline test -- 2c50afc
- [x] E4 vt_asm_csi: the CSI parameter scan in asm, 8 instructions a digit; check 80/81 -- 38a8041
- [x] W1 put_ascii_run does the last column and the wrap -- 8abe0b1
- [x] U1 fast_cp: a whole UTF-8 / Latin-1 / CP437 character of A0 and up to put_char -- 8abe0b1
- [x] F1 vt_feed keeps "may text run / plain colours" across text, CR, LF -- 8abe0b1
- [x] C1 put_char marks one cell in place; scroll_up hoists the scrollback test, no % -- 8abe0b1
- [x] N1 scroll_up: line_clear's empty-line case in place, vacated_default once a scroll; CR LF
      in one turn -- 68079a5
- [x] V1 `#pragma opt 991`: vbcc's -O2 spilled hoisted field addresses (19.4 -> 16.5 / byte on
      plain lines) -- f4aaf4f
- [x] D1 dbra in rows_up/down, cells_move, fill (two instructions a row pointer) -- b02ca89
- [x] N2 pend_prepare / vacated_default tests in place (macros, call only for the rare case);
      text-mode test or-ed -- 25e5722
- [x] K1 a lone coloured character written in place; one-colour SGR without the loop; pen_ext
      -- 5541615
- [x] K2 a whole-row erase from column 0 makes the row chonly (frame repaint's rows never
      scroll); param/param0 macros; move_to in place -- 6bc1e22
- [x] E5 vt_asm_csi v2: a2/a3 point at the parameter being read (separator ~20, digit 10)
      -- 8cf7347
- [x] W2 put_ascii_run runs on across CR / LF (newlines() shared with vt_feed) -- 281fbcd
- [x] measure s5 (281fbcd) on all workloads; next targets from its profiles
