---
date: 2026-09-29
topic: vtcon pcansi against DCTelnet's term-engine on BBS art (T1 pre-check 1)
tags: [dctelnet, pcansi, differential]
status: final
---

# pcansi vs term-engine on BBS art

Tool: `tools/te_diff` (`make te-diff`). It includes DCTelnet's
`src/third_party/retro32-term/term-engine.c` (dctelnet-v2 @ 8ab9e2e) on the host
with a small Amiga shim, runs its direct planar path into host bitplanes, draws
vtcon's pcansi grid into a second set by the same glyph rule, and compares pixels.
The synthetic font gives every code its own bitmap, so a differing cell decodes back
to code, fg and bg. `-f` feeds byte by byte and names the first diverging byte.

Corpus: `~/Code/amiexpress-doorserver/bbs_ads`. There are 1544 `*.ans` files, plus 2326 other
files that contain `ESC[` (under 300 KB each): 3870 in total, at 80x25.

## Result

| Stage | Files that differ |
|---|---|
| First run, `*.ans` | 169 of 1544 |
| With term-engine's CUP bug patched in a scratch copy | 7 of 1544 |
| After the vtcon fixes below, all 3870 | 8 of 3870 |

## vtcon pcansi bugs found and fixed (each has a test in tests/test_pcansi.c)

1. An erase or scroll after `ESC[5m` filled with the plain background, not the iCE
   bright one (HTF-SHES.ANS, HTF-MADH.ANS).
2. An erase under `ESC[7m` did not swap colours. ANSI.SYS erases with the whole
   attribute byte (ICEHOUSE_84TTk2M.TXT). The pcansi blank now carries fg and
   bold/blink/inverse only when they are visible, so the default blank stays
   canonical for the renderer's scroll contract.
3. VT (0x0B) moved the cursor down a line and shifted the art below it (IPH-PLT.ANS,
   GOODBYE.ANS). It is now ignored, as term-engine does. DOS would print the male
   sign glyph.
4. `CSI s` / `CSI u` saved and restored colours (DECSC). ANSI.SYS SCP/RCP handle the
   position only (LSP-CS.ANS, LSP-NZ.ANS).
5. `CSI 1;1 T` did not scroll (the xterm guard for mouse highlight). pcansi now scrolls
   by the first parameter, as term-engine and SyncTERM do (__z9hnNn8).

## term-engine bugs (DCTelnet, not fixed there; the adoption removes them)

- **`ESC[nH` with one parameter takes a stale column.** `term_csi_begin` initialises
  `p_params[1]` lazily, so `pn1(1)` reads the previous sequence's value. This is 162 of the
  169 first-run diffs. ANSI.SYS puts the cursor at column 1. Visible in DCTelnet today
  on common art (STRONG.ANS, CIA.ANS, CALVIN.ANS...).
- A C0 control right after ESC, or after ESC plus an intermediate, is swallowed, not
  executed (UBU.ANS: CR lost, "4 Gigs" lands at column 78 instead of 31; Program.doc,
  FLYING_RULEZ.displayme, -LOGIC-.CMT). ESC inside ESC-intermediates does not restart
  the sequence (DeATHRoW.DiSPLaYMe).
- `CSI I` (CHT) is ignored (bplogga).

## Remaining differences (8 of 3870)

The files above where term-engine is the one that deviates, plus MORBID.DI: bytes 0xA0-0xFF inside a
CSI started by a stray 0x9B in CP437 text. That is a one-file edge case, left as is.
