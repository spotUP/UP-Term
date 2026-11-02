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

Ledger row AMIBERRY-DEFAULT (todos.md near the top) holds hunts 2 to 8. Research file (untracked, in the
worktree): ../vtcon-s48-amiberry-default/thoughts/shared/research/2026-11-02-amiberry-assign-hang.md, Steps 1 to 8.

Ruled out, in order (each was once believed and then disproved, so do not repeat them):
1. A DOS handler not answering Assign LIST.
2. wasabid's UI-tree read (requester watcher). UPTERM_REQUESTER_WATCH=0 does not help.
3. macOS App Nap / CPU starvation. The 0 s CPU reading came from host load about 108.
4. Generated .uae config: equals main 4254f87d's (rig 20, known good in s29) apart from serial and screenshot lines;
   removing those lines did not help.

Hunt 6 (rig 37, Amiberry) failed with 4254f87d's tools/rig, with the older 31 Oct sys.hdf as DH0, and with
KIT_SHADOWS emptied: rig code, DH0 base and kit shadows are ruled out.

Hunt 7 (rig 37, fs-uae): 4 of 4 good boots on current main (--ram 256, --max, AGA, restart). fs-uae is fine; the
rig 35 auth failure at 07:39 was a one-off. Only Amiberry hangs. `--emu fsuae` is the workaround for everyone.

Hunt 8 (rig 37, Amiberry): JIT off (cachesize=0) is worse (stops at "go started", C:Wait 15 never returns,
Amiberry at 0.1% CPU). The JIT is not the cause. The "FPU UNIMPLEMENTED INSTRUCTION" line at PC=00f80c96 is
Kickstart's own FPU probe and appears under fs-uae too.

Next (hunt 9), cheapest first:
- (b) JIT on with cpu_speed=max and cpu_idle=0 instead of cpu_speed=real (hypothesis: idle throttling stops
  emulated time).
- (a) Step 6 boot A in a scratch worktree with build/amiga from the extracted build/up-term.lha (1 Nov 12:46).
  Never swap main's build/amiga.
Each Amiberry start clones a 1.5 GB sys.hdf; disk was 2.0 GB free on 2026-11-02, so check df first.
`rig.py stop` leaves Amiberry processes alive; kill them by hand.

Branch s48-amiberry-default (Amiberry as default emulator) stays unmerged until Amiberry boots answer.
rig 2 belongs to musicplayer-16; never touch it.

## Icon stems

Rig 35 read 6 of 8 icons SOFT. The host hunt (a0aaeb96, research/2026-11-02-icon-stem-soft.md) shows snap_stems
is correct: rig 35's maps equal the unfitted render, so its mounted XCON: ran a stale handler. gridfit_rig.py also
drops the top row of each cell (faint E0B0 anchor row). F013 and F1C0 stay SOFT by font design (curves).
In flight at handoff: gridfit_rig row fix (host) and rig 35 rerun after install plus reboot.

## Owner decisions still open

- OK to delete build/rig33 (later rig37/40/41).
- Whether --a500 / --stock stay blocked under Amiberry.

## Queue after the hang

Kitty T-E (needs owner D12/D13), Kitty T-F/T-G/T-H, timg port, chafa plus GLib, nnn kitty preview, picture tools
P3-P5, `make dist` so icat is in the kit, rest of AMIBERRY-RIGS-GAPS.
