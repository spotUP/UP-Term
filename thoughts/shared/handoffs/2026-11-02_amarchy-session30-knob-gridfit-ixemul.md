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

## Open, in order
1. AMARCHY-MENU-IXEMUL-BOOT-RACE (ixemul48-snoop.md): SYS:Libs has no ixemul; amarchy-menu starts
   from User-Startup before async BOOTX:go adds VTCX:ixp6 to LIBS:. Next: find where the installer
   "libs" step (dist/Install.installer ~line 659, P_STEP "libs") really copies ixemul; it probably
   resolves LIBS: to the first multi-assign dir. Fix at the installer or make the Amarchy start
   wait for the BOOTX assigns; pick after reading the copy target. Boot-time open not snooped yet.
2. Stale kit: build/dist/up-term/Files/Amarchy 11:03 (md5 56830d1a) is pre-deadlock-fix. A
   fresh-all kit install brings the hang back until make dist + kit-stage (upterm-ports) run.
3. Rest of queue as in the ledger: FLAT-SLIDER-LOOK (a) arrows (c) knob radius,
   FLAT-ASPECT-BORDERS, ICONTEXT/SELECTA-AA-ICONTEXT, colour audit 3-8, AM-ICONS-DISK-ALL, F11-31,
   AMARCHY_WBCLOSES reads 0, fs-uae 3.2 no Amarchy autostart, 3.2 sysiclass IM_DRAW offscreen,
   SolidWin open points, installer rig IconLib case, RIG-OS32-NO-RTG, RIG-APPICONS-TEARDOWN-REQ,
   INSTALL-AMISSL-STALL, DIST-STAGED-FROM, bar_open retry with no screenmode.prefs.

## Blocked on owner
- Look at s30-knob-muted.png and s30-icon-grid-fit.png.
- Owner of build/rig27 and of the upterm-ports checkout on port/htop.
- Thomas Rapp reply pending.

## Rules carried
- A1200 down: final checks on Amiberry OS 3.2; never boot 3.1 on Amiberry.
- Own rigs: stop then clear at once; others' rigs: owner OK first.
- No rule keeps rig 8 up.
- Not ours: thoughts/shared/research/s28-evidence/fp3-os32.md, tools/rig/*.py edits, ixemul-vtcon WIP.
