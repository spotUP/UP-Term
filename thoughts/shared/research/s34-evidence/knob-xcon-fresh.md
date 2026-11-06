---
date: 2026-11-04
topic: S32-KNOB-NONE-FRESH - why the XCON prop knob misses the amarchy-flat scroller gate (3.2)
status: PAUSED before any rig measurement (coordinator: rig 96 R3 runs alone). No verdict yet.
---

# S32-KNOB-NONE-FRESH, s34 hunt (paused)

## Where it stands

- No rig measurement was taken. Rig 65 fresh-all was started, then aborted mid-Installer on the
  coordinator's PAUSE ([ABORT] fresh-all: 143, no verdict written). Rig 65 stopped and cleared
  (build/rig65 gone). Its disk was mid-write when stopped; it was cleared, so nothing to recover.
- Failing condition NOT yet identified. The code-read candidates below are ranked; only a rig read decides.

## Blockers found on the way (fix before the next run)

1. `make dist` on main fails: `dist: upterm-ports/build/kit/userland was staged before the stamp: make kit-stage
   in upterm-ports`. Not run here (other repo's build state). The existing build/dist/up-term kit is
   from 2026-11-03 23:37 and is OLDER than main's flat code (c245126f 23:48, S33-PREFS-SKIN-FAIL, flat_core.c):
   kit amarchy-flat md5 a694b2a2, main build/amiga/amarchy-flat md5 2f92eb62. A "fresh install from main"
   needs `make kit-stage` in upterm-ports first (owner of that repo's session), or install the old kit and
   copy build/amiga/amarchy-flat over UP-Term:bin/amarchy-flat + reboot (flat.c gate code is unchanged).
2. Local main dec2222f lacks origin/main ae859c52 (UPDemo only) -> rig.py refuses (RIG-STALE-BASE).
   Working start: `UPTERM_RIG=65 UPTERM_RIG_FLAGS="--os 32 --max --allow-stale-base missing-commit-is-updemo-only"
   INSTALLER_KEEP=1 python3 -I tools/rig/installer_rig.py ONLY fresh-all --purpose "Amarchy s34: XCON scroller knob hunt (3.2)"`.
   (`--os 32` on the installer command line plus UPTERM_RIG_FLAGS passes it twice; harmless, the flags form is enough.)

## Code facts (read, not measured)

- Gate: amarchy/flat.c:700-701 (`rp->Layer->Window` set and `ret` inside irom_lo..irom_hi), then
  `scroller_pen` flat.c:618-640: skips any gadget with `(g->GadgetType & GTYP_GTYPEMASK) != GTYP_PROPGADGET`
  or no SpecialInfo (flat.c:625); needs fill inside the gadget box (flat.c:627-632) and inside the
  container gx+LeftBorder, gy+TopBorder, CWidth, CHeight (flat.c:633 -> flat_core.c:597).
- The math of flat_scroller_pen / flat_scroller_knob (flat_core.c:574-605) gives the knob pen for a
  full-body FREEVERT prop (VertBody 0xFFFF, kh = ch), so a host test of that function alone will PASS
  on current code: the failure is in what scroller_pen reads from the live gadget, not in flat_core math.
- XCON prop (handler/sbar_gad.c:106-121): NewObject(PROPGCLASS) with GA_RelRight 5-sw, GA_RelHeight,
  GA_RightBorder, PGA_Borderless, PGA_NewLook, Total=Visible=1. The box formula in flat.c:627-630
  (RELRIGHT: LeftEdge + Width - 1; RELHEIGHT: Height + w->Height) matches Intuition's convention.
- shits rose at boot (24, s33 evidence) so some window's props DO pass; the XCON prop's fills do not
  (shits unchanged after opening an XCON window).

## Ranked candidates

1. GadgetType of the boopsi propgclass object on 3.2 is not GTYP_PROPGADGET (e.g. GTYP_CUSTOMGADGET 5),
   so flat.c:625 skips it. Drawer windows would pass only if workbench uses a native/other prop.
2. PropInfo CWidth/CHeight/LeftBorder/TopBorder of the borderless prop are 0 or stale when the knob is
   filled (flat_core.c:597 `cw < 1 || ch < 1` returns -1; also flat.c:633 adds LeftBorder/TopBorder).
3. The knob fill's caller is outside the intuition resident range (3.2 propgclass or knob draw in a
   separate module) or the fill goes to a rastport with no Layer->Window (flat.c:700-701).
4. The NewLook borderless knob is not a single RectFill (e.g. BltPattern / frame image), so the gate never sees it.

## Exact rig steps still needed (one rig, ~10 tool calls)

1. Fresh-all on own rig per blocker 2 (after blocker 1 is settled), reboot.
2. `ami.py put` thoughts/shared/research/s34-evidence/gadprobe.c built for 68020
   (`vc +build/vbcc-aos68k.cfg -Ivendor/ndk-3.2r4-Include_H -cpu=68020 -O2 -o <scratch>/gadprobe gadprobe.c`)
   to RAM:, open `NewShell XCON:20/30/400/150/KnobShell`, open a drawer (SYS:), run
   `RAM:gadprobe KnobShell` and `RAM:gadprobe System` (prints IROM lo/hi, every gadget's type/flags/box and PropInfo).
   - type != 0x0003 on the XCON prop and 0x0003 on the drawer -> candidate 1 confirmed (flat.c:625).
   - CWidth/CHeight 0 -> candidate 2 (flat_core.c:597).
3. If both look right: debug amarchy-flat in a worktree that rings every irom-or-not fill inside a
   prop box (ret, Layer->Window, rect, chosen gadget, pen result) in STATUS (next to the sfill ring,
   flat.c:933) -> separates candidate 3 from 4.
4. Pixel-check the prop column (x406-415, y45-170 in KnobShell) before/after.

## Fix and test plan (per confirmed candidate)

- Candidate 1: accept a boopsi propgclass gadget in scroller_pen (and the sring walk, flat.c:752) by
  identifying it through its class (or treating GTYP_CUSTOMGADGET with a PropInfo SpecialInfo the same),
  extracted into a pure predicate in flat_core.c (e.g. flat_is_prop(gadgettype, has_specialinfo, ...)).
  Host test in tests/test_flat_core.c next to the scroller tests: the XCON gadget's measured GadgetType
  must be accepted -> fails before (predicate rejects it), passes after.
- Candidate 2: compute the container from the gadget box when PGA_Borderless leaves CWidth/CHeight 0
  (pure helper in flat_core.c); host test: borderless full-body prop with cw=ch=0 and the gadget box ->
  knob pen, fails before (returns -1).

## Scratch

- Deleted: build/rig65 (rig.py clear). Kept: this file and gadprobe.c (small). Scratchpad holds
  gadprobe binary + install/build logs (small). No worktree created.
