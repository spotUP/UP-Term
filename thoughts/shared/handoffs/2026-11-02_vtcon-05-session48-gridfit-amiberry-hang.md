---
date: 2026-11-02
session: vtcon-05 (session 48, cross-session name vtcon-ee)
status: in progress
---

# Session 48 handoff: grid-fit landed, Amiberry rig hang still open

## Landed on main (pushed)

- eafa4c01 + 62f99ddd: grid-fit step 1 (TTF_FILL stem snapping) and tools/rig/gridfit_rig.py.
- 4c8c7953 (c376883e): grid-fit step 2, icon-font stem pairing (TTF_ICON, snap_stems, TTF_SNAP_STEMS).
- a11bb5bd: gridfit_rig icon and icon-flood modes.
- f01013d7: gridfit_rig stem_verdict() judges a stem by its longest ink run; tests/test_gridfit_stem_verdict.py.
- U+E0A0 hunt: stems snap correctly; the remaining soft pens are the font's own notch (no code change).

Ledger rows: HANDLER-ICON-GRID-FIT (todos.md top), GRIDFIT-RIG-FALSE-SOFT (done).

## Open: Amiberry rigs stop answering after boot

Ledger row AMIBERRY-DEFAULT (todos.md near the top) holds hunts 2 to 6. Research file (untracked, in the
worktree): ../vtcon-s48-amiberry-default/thoughts/shared/research/2026-11-02-amiberry-assign-hang.md, Steps 1 to 6.

Ruled out, in order (each was once believed and then disproved, so do not repeat them):
1. A DOS handler not answering Assign LIST.
2. wasabid's UI-tree read (requester watcher). UPTERM_REQUESTER_WATCH=0 does not help.
3. macOS App Nap / CPU starvation. The 0 s CPU reading came from host load about 108.
4. Generated .uae config: equals main 4254f87d's (rig 20, known good in s29) apart from serial and screenshot lines;
   removing those lines did not help.

Current state: wasabid accepts the TCP connect but never replies; the Amiga runs busy (4-7 s CPU per 8 s).
Hunt 6 (opus) was running at handoff time: boot rig 37 with 4254f87d's tools/rig, then bisect the boot drawer
(upgetty, vtcon-handler, up-console.device, pty-handler, Amarchy) or the DH0 clone.

Branch s48-amiberry-default (Amiberry as default emulator) stays unmerged until Amiberry boots answer.
Workaround for everyone: --emu fsuae. rig 2 belongs to musicplayer-16; never touch it.

## Owner decisions still open

- Look at the shots in ~/Desktop/upterm-shots: gridfit-box-glyphs.png, gridfit-powerline-glyphs.png (shaded tips
  OK?), gridfit-icons.png, r6a3-edge-pens.png, kitty-td-relative.png, kitty-tc-files.png.
- OK to delete build/rig33 (later rig37/40/41).
- Whether --a500 / --stock stay blocked under Amiberry.

## Queue after the hang

Kitty T-E (needs owner D12/D13), Kitty T-F/T-G/T-H, timg port, chafa plus GLib, nnn kitty preview, picture tools
P3-P5, `make dist` so icat is in the kit, rest of AMIBERRY-RIGS-GAPS, rig 35 rerun of icon stems under the new
stem_verdict rule.
