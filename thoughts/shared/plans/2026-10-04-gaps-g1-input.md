---
date: 2026-10-04
topic: Gap list G1 -- input (keys, mouse, wheel, focus, selection, paste)
tags: [plan, progress, input, xterm, mouse, keyboard, gaps]
status: implemented
---

# G1 input gaps: progress ledger

Source: `thoughts/shared/research/2026-10-04_modern-terminal-gaps.md` (the audit, in the
main checkout), items #1 #3 #5 #6 #7 #8 #12 (key part) #14, bracketed-paste sanitising, and
the input-side notes. Branch `feature/gaps-g1-input`. Done = each item has a host test that
failed before the fix, `make amiga` builds clean, and the rig-only parts are listed as
manual checks (the rig is busy with the speed campaign: not used here).

## Decisions (do not re-litigate)

- Host input *policy* (pixel to cell, wheel routing, motion filtering, click counting,
  word/line bounds, which keys are the console's, paste filtering) moves out of
  `render/vtwin.c` (Intuition-only, not host-compiled) into a pure module
  `render/vtinput.c`, host-tested as suite `input`. `vtwin.c` keeps the Intuition calls
  and executes what vtinput decides. `vr_cell_at` delegates to `vti_cell_at` (one copy
  of the math).
- Encoders stay in the engine (`vt_encode_*`): focus; alternate scroll uses
  `vt_encode_key` arrows; modified Return/Tab/Backspace/Escape.
- ?1007 default OFF, as xterm (`alternateScroll` resource false); a program turns it on.
  VTE/kitty/foot default on -- the owner may want a profile switch later (not done).
- Shift+PgUp/PgDn: UP-Term's scrollback on the main screen with no mouse mode; the
  program's (`CSI 5;2~` / `CSI 6;2~`, terminfo kPRV/kNXT) on the alternate screen or
  with a mouse mode on.
- Modified Return/Tab/Backspace/Escape: without modifyOtherKeys Alt adds ESC, Ctrl+Backspace
  is BS (xterm), Shift+Tab is `CSI Z`, other modifiers change nothing. modifyOtherKeys 1:
  Ctrl or Shift (except Shift+Tab, Ctrl+Backspace) -> `CSI 27;m;code~`; level 2: any
  modifier except Shift+Tab alone -> `CSI 27;m;code~`.
- modifyOtherKeys never reports Shift alone on a character (xterm: the character says
  it). The host passes Ctrl/Shift with the keymap's character *without* Ctrl only while
  modifyOtherKeys is on; at level 0 the keymap's own Ctrl stays (unchanged behaviour).
- Multi-click selections are final on the click (no word/line-unit drag extension).
- Rectangular selection: NOT done -- `selected()` in `render/amiga_render.c` is the
  per-cell draw test and `vt_copy_text` is linear; both sit in the speed campaign's hot
  path, which this job must not restructure.

## Items

| ID | Item | Status | Commit |
|----|------|--------|--------|
| K1 | #6 Alt/Ctrl/Shift with Return, Tab, Backspace, Escape | done | 95e48a9 |
| K2 | #7 modifyOtherKeys for Ctrl combinations (host + Shift-alone rule) | done | 10e7ba9 |
| K3 | #5 focus events ?1004 (engine encoder, handler dispatch) | done | d662d3a |
| M1 | #1 wheel reports in cells (vtinput module; vr_cell_at removed, vti_cell_at the one copy) | done | 1d79826 |
| M2 | #8 alternate scroll ?1007 | done | e9f55ae |
| M3 | #3 ?1002 / ?1003 motion reports | done | 24b604f |
| M4 | #14 middle button and Ctrl/Meta modifiers in mouse reports | done | b9986e1 |
| S1 | #14 double-click word, triple-click line | done | ae945e3 |
| S2 | #14 middle-click paste | done | 2af09a0 |
| S3 | #14 rectangular selection | blocked (hot path, see decisions) | -- |
| K4 | #12 Shift+PgUp/PgDn reach the program on the alternate screen / in mouse mode | done | 990b1ac |
| P1 | bracketed paste: ESC and other controls stripped inside ?2004 | done | 4170f2a |
| F1 | RIS resets modifyOtherKeys | done | f71a00b |
| F2 | DECRQM ?66 answers DECNKM | done | f71a00b |
| F3 | conformance matrix 5.4 updated with each input change | done | with each commit |

14 of 15 done, 1 blocked (S3).

## Verification

- Each host test failed before its fix (missing symbol, or the old behaviour put back
  temporarily: pixels in vti_wheel, no click counting) and passes after.
- `make test` (all 21 suites, landing run) green; `make amiga` (68020) and the 68000
  engine object build with warnings as errors, clean.
- Not run: the rig (busy with the speed campaign). Manual checks below.

## Manual checks (rig or real Amiga, `make amiga`, install the handler)

1. Wheel in tmux with `set -g mouse on`, two panes side by side: the wheel over the
   right pane scrolls the right pane.
2. `vim` with `set mouse=a ttymouse=sgr`: drag-select in visual mode follows the
   pointer (?1002); Ctrl+click arrives (`:map <C-LeftMouse>`); middle button pastes in
   vim only if vim maps it -- in a shell (no mouse mode) the middle button pastes the
   clipboard. Confirms MIDDLEDOWN/MIDDLEUP reach IDCMP_MOUSEBUTTONS (unconfirmed).
3. `printf '\e[?1003h'; cat -v`, move the pointer: `^[[M#..` reports per cell;
   `printf '\e[?1003l'` stops them.
4. `printf '\e[?1004h'; cat -v`, click another window and back: `^[[O` then `^[[I`.
5. bash: type `foo bar`, Left Amiga+Backspace deletes `bar` (ESC DEL).
6. `printf '\e[>4;2m'; cat -v`: Ctrl+; shows `^[[27;5;59~`, Ctrl+Return `^[[27;5;13~`.
7. `less` on a long file after `printf '\e[?1007h'`: the wheel pages the file.
8. In `less`, Shift+PgUp scrolls less (not the window); in the shell it still scrolls
   the window's scrollback.
9. Double-click a path in `ls -l` output: the path is selected; triple-click a wrapped
   line: the whole line.
10. With bracketed paste on (bash 5.1+ default), paste text containing an ESC byte: it
    arrives without the ESC.
