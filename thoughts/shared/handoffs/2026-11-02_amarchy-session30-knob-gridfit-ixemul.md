---
date: 2026-11-02
session: amarchy 30
topic: knob DK_MUTED and icon grid-fit merged, OS 3.1 boot proof, ixemul boot race found
status: handoff
---

# Amarchy session 30: state at end

Ledger: thoughts/spot/todos.md (rows ICON-GRID-FIT, GREY-TOO-DARK, BOOT-31-WB-DEADLOCK,
AMARCHY-MENU-IXEMUL-BOOT-RACE). Evidence: thoughts/shared/research/s30-evidence/.

## Landed (pushed to origin/main)
- 39152f89 knob DK_MUTED: flat_scroller_pen knob uses pen_dimtext at depth 3..8 via shared
  flat_dimtext_ok(). Rig 52 proof PASS from pixels (knob 7A7A7A on 000000). Shot
  ~/Desktop/upterm-shots/s30-knob-muted.png, evidence knob-muted-proof.md.
- 51e6b597 ICON-GRID-FIT: y-only stem snapping for the 32x16 bake (tools/gen_menu_icons.py
  fit_stems_y/raster_polys, test tests/test_icon_grid_fit.py). Mockup
  ~/Desktop/upterm-shots/s30-icon-grid-fit.png. Rig shot still due after a kit refresh.

## Proven
- BOOT-31-WB-DEADLOCK fix: rig 50 fs-uae 3.1, 0 hangs in 3 cold + 1 warm boot (boot31-fix-proof.md).
  vtcon-ee closed FSUAE-WASABID-NO-ANSWER; AMIBERRY-DEFAULT stays open (Amiberry 3.1 unproven,
  s48 unmerged).
- rig.py stop removes build/rigN by design (8df99f0b). Not a bug.

## Landed later in session 30 (pushed)
- 82746511 + ee6e1361 AMARCHY-MENU-IXEMUL-BOOT-RACE: early S:User-Startup block assigns LIBS: VTCX:ixp6
  before the Amarchy block. Rig 57 proof PASS, 2 cold boots (ixemul-boot-race-proof.md).
- 8941cc55 FLAT-GADGET-NO-DIVIDERS: flat_clone_map peels full-span edge lines (edge_line helper). Pixels
  PASS on 3.1 and 3.2 (flat-gadget-no-dividers.md); owner PASS 2026-11-02.
- 2cf9de4f + 23d38c23 (peer fix: bare Echo, no C:Echo exists): rig STARTUP truncates boot.log once; go appends.
- 54dd15e4 rig.py refuses a bare rig number; 726f684d clear refuses rig 1 without --rig1.
- Glyph clone on 3.2 works clean and under the desktop (GLYPH-CLONE-32 closed, 58616e98, 870c1f1c).
- build/rig (rig 1) was deleted by an agent's bare `clear 55`: RIG1-REBUILD open (rebuild on next need).

## Open, in order
1. KIT-AMARCHY-FLAT: kit has no amarchy-flat; amarchy_gui_rig.py --setup stages only Amarchy,
   amarchy-menu, amarchy-gui. Rigs never test the boot start of UP-Term:bin/amarchy-flat.
2. Stale kit: vtcon-ee runs make kit-stage + make dist from upterm-ports kit-main worktree.
3. AM-ICON-NOBORDER branch worktree-agent-a5868f0ddde7bcf3e (360d8455, 87c1f697): rig 3.2 check.
4. INSTALLER-CHECK10: installer check 10 failed twice on rig 57 (not investigated).
5. Rest of queue as in the ledger: remote s28-button-round unchecked, FLAT-SLIDER-LOOK (a)(c),
   FLAT-ASPECT-BORDERS, ICONTEXT/SELECTA-AA-ICONTEXT, colour audit 3-8, AM-ICONS-DISK-ALL, F11-31,
   AMARCHY_WBCLOSES reads 0, fs-uae 3.2 no Amarchy autostart, SolidWin open points, installer rig IconLib
   case, RIG-OS32-NO-RTG, RIG-APPICONS-TEARDOWN-REQ, INSTALL-AMISSL-STALL, bar_open retry, ICON-GRID-FIT
   rig shot after kit refresh, 222222 body fill, knob depth-3 fallback host-only.

## Blocked on owner
- Is wasabi/pin-f59c839 still needed?
- Thomas Rapp reply pending.

## Rules carried
- A1200 down: final checks on Amiberry OS 3.2; never boot 3.1 on Amiberry.
- Own rigs: stop then clear at once; others' rigs: owner OK first.
- No rule keeps rig 8 up.
- Not ours: thoughts/shared/research/s28-evidence/fp3-os32.md, tools/rig/*.py edits, ixemul-vtcon WIP.
