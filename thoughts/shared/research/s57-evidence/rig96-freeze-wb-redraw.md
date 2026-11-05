---
date: 2026-11-05
author: opus agent (vtcon-c1 brief), read-only on rig 96
row: S57-RIG96-FREEZE
build: rig 96 = ../vtcon-s58-rig96 at main 92b29ff8, amarchy-flat full install, OS 3.2, 640x256x8
verdict: freeze state lost (rig rebooted by vtcon-8c before this read); red-in-AA cause found and reproduced on the live boot
---

# Rig 96 freeze after a Workbench redraw, with red pixels in the antialias

## 1. The freeze itself: state gone, no task dump

- The freeze was not live when this agent read the rig. fs-uae pid 58039 started 12:30:49; vtcon-8c restarted rig 96 at
  12:28:51 as part of its planned 10-boot series (`../vtcon-s58-rig96/thoughts/shared/research/s58-evidence/rig96-rebuild-3.md`).
  At 12:32 wasabid refused port 1329 (still booting), and answered `health` normally by 12:34. No ps or task dump was
  taken on the fresh boot (brief: do not hunt on a fresh boot).
- What survives from before 12:30:49 (build/rig96/wasabi/debug.log, the only log that spans reboots):
  - 12:10:53 `[wasabi: 127.0.0.1 connected]` x3, then 12:11:26 `nothing heard for 20 s - the machine is frozen`;
    connection lost only at 12:15:38 (the series' reboot). = boot 7, `workbench_up=no`.
  - 12:23:41 connected x3, 12:24:24 `nothing heard for 20 s`, lost 12:28:48 (reboot at 12:28:51). = boot 9, `workbench_up=no`.
  - No KPrintF text at all in either window; no Guru (series checks `[WARN] Guru before reset`: 0, `CPU reset PC=`: 0).
  - The series' own "task states (ami.py exec ps)" capture is EMPTY for both hangs: wasabid itself stopped answering.
- serial.log and logs/debug.uae are rewritten per boot (current boot only: vsh banner, fs-uae config). The previous
  fs-uae log tail shows only a normal SDL_QUIT and one ROM `FPU EXCEPTION ... PC=00F80D6C` at boot (normal 060 trap).
- So: stuck task, wait object and lock holder are UNKNOWN for this freeze. 2 of 9 recorded boots hung on 92b29ff8.

### Match with known leads

- S57-BOOT5-HANG (hang-fix-proof-10boots.md): same signature. No Guru, fs-uae alive, wasabid silent, during the boot /
  post-start step, on a build that already has the icl pen fix 351e9c0e. This is the matching lead.
- start-pointer-hang.md / flat-lock-map.md (Workbench queued on the dead PalExtra semaphore of a closed WB ColorMap,
  IPrefs then blocked on the WB LayerInfo): those hangs left wasabid ALIVE (task dumps were taken). Here wasabid is silent
  too, so this is either a wider stall or something wasabid itself blocks on. Not proven to be the same lock.
- S57-PALETTE-USE-CRASH (Exception 3 at 10031f6a after Palette USE, 640x256x8): same screen mode and same trigger family
  (palette / prefs change makes Workbench redraw). Unverified link; an Exception 3 would normally leave a Guru, none seen.

## 2. Red pixels in the grey antialias: reproduced on the current boot

Screenshot (wasabi `grab`, 12:34:23): `~/Desktop/upterm-shots/rig96-freeze-wb-redraw.png`, 640x256 rows doubled.
Measured by script (scratchpad red.py / red2.py), 12 distinct colours on screen:

- Label text (169,177,214); AA shades (84,90,119) 45 px, (116,122,148) 98 px, (133,139,170) 8 px.
- 38 pixels of (247,118,142) = #f7768e, the theme red. One single colour, so one pen.
- Positions (screen coords, x,y): bbox x 9..35, y 35..181, in the left icon-label column. Per row:
  y35:5 y36:4 y37:4 y38:1 y39:3 y40:1 y72:1 y74:2 y107:1 y108:2 y110:2 y111:3 y143:1 y176:2 y178:2 y179:2 y181:2.
  First pixels: (12,35) (15,35) (20,35) (24,35) (26,35) (9,36) (10,36) (17,36) (18,36) (9,37) (10,37) (17,37).
- Character map rows 35-41 (`#` text, `1` AA shade, `R` red):

```
 35        #####R  R####R ##R R##
 36        ##RR##  ##RR## ###1###
 37        ##RR##  ##RR## #######
 38        #####R  ###### ##1#1##
 39        ##1##R  ##RR## ##   ##
```

  The R pixels sit exactly at the glyph corner and inner-notch positions where the icon-label blend writes shade
  pens; other glyphs in the same label keep the correct shade `1`. So the label was painted with a correct shade pen,
  and that pen NUMBER later changed colour to red.
- This is on a fresh boot (after 12:30:49), so the red artefact does not need the freeze; it comes with the boot-time
  sequence of Workbench redraws.

### Cause (code)

amarchy/flat.c icl text path (pen screens, lines ~806-823): `icl_pen_for` obtains each shade with `ObtainBestPen`
(PRECISION_IMAGE), the label is written with `WritePixelArray8`, then since 351e9c0e every obtained pen is released in
the same call (`icl_cache_drain` + `ReleasePen`). The pixels stay on screen in a pen the patch no longer owns. A pen
with no users is free in the PalExtra; the next client that needs a colour with no close match (Amarchy's theme pens,
amarchy-flat's dim/knob/inbar/sel pens, colour icon pens, IPrefs/Palette on the next redraw) gets that pen number and
SetRGBs it, here to the theme red. Every label pixel drawn with it turns red until the label is redrawn. The last
redraw of the boot sequence is the one where a red-needing allocation lands after the labels; earlier redraws repaint
labels after the allocation, which is why only the final one shows it.

351e9c0e fixed the hang half of flat-lock-map (ReleasePen on a dead ColorMap) by releasing early, and introduced this
recolour: releasing per call is only correct for pens whose pixels do not outlive the call.

## Proposed fix (not done in this brief)

Hold the icon-label shade pens for the life of the screen, keyed by screen identity, the same pattern as
`flat_pens` (amarchy/flat_core.h S32-STALE-HELD-PENS: screen + ipd_epoch; ReleasePen only on the identity the pens
were obtained on, forget without ReleasePen on any other). Concretely: move `icl_pc` to a per-identity holder, drop the
per-call `icl_cache_drain` release, and drain from the 1 s Workbench watch step (`flat_pens_follow` call site) and on
unload: release when the identity still matches, forget when the screen is gone or reopened. That keeps both
properties: no ReleasePen on a freed ColorMap (the s57 boot hang) and no free pen under on-screen label pixels (the red).
Regression test: host test in tests/test_iclabel.c that a shade pen stays held across two calls on one identity and is
forgotten (not released) after an epoch change; replace tests/test_flat_icl_no_cross_call_pens.py, which asserts the
per-call release that causes this bug.

For the freeze: the next capture must be a task dump taken WHILE hung. Since wasabid goes silent, that needs the
fs-uae debugger route (rig started with `--debugger`, as the S57-BOOT5-HANG rig 142 hunt) rather than wasabi ps.

Confidence: red-in-AA cause high (pixel positions + code path). Freeze cause: unknown, matches S57-BOOT5-HANG by
signature only; low confidence on any lock.

## Fix run (S57-ICL-PEN-RECOLOUR, commit 703365f0 on top of df369b89)

- Hole closed: icl_pen_for no longer calls ObtainBestPen when the 32-slot cache has no room for the key (new
  icl_cache_has_room); it takes the non-obtained choice (nearest of fg/bg), so no drawn pen is ever released. Host test
  t_cache_full_no_obtain (tests/test_iclabel.c) + wiring test in tests/test_flat_icl_pens_persist.py. `make test ONLY=iclabel`:
  184 checks, 0 failed; ONLY=flat_icl_pens OK. `make build/amiga/Amarchy` and `build/amiga/amarchy-flat` (vc -warnings-as-errors) rc=0.
- RGB-changed same-key path (icl_cache_put hands back old_pen, caller releases it): CAN in principle recolour, because pixels
  drawn with the old pen may still be on screen after a palette change and the released pen can be reallocated. It needs a
  palette change of the fg/bg pen first, and the labels are repainted by the redraw that follows a palette change. NOT fixed
  here (no repro; a fix would hold the old pen until the next bind, i.e. a deliberate leak): open question for the owner.
- Rig 144 (OS 3.2, 060 max, 640x256x8, base-staleness override), Amarchy + amarchy-flat (flat-system-look on, labels=30 AA drawn),
  Workbench drawer open/close, grab. Exact (247,118,142) pixels: boot0 0, boot1 0, boot2 0 (INVALID: flat=held, labels=0, no AA
  drawn), boot3 0. Boot hangs: 0 of 3 reboots (wasabid answered each time).
- Caveat, unverified as proof: this rig has no client that allocates the theme red, so 0 is weak evidence (the before-fix 38
  came from rig 96 full install). Boot1 showed the two AA shade pens (93 px and 14 px) as pink (255,119,238)/(255,85,170)
  instead of grey (83,87,108)/(112,117,144) seen on boot0 and boot3: the shade pens were recoloured by another client in that
  boot although held. Cause not investigated (budget). Needs a hunt: who SetRGBs pens 4+ after the labels (Amarchy palette
  apply?) and why ObtainBestPen shares an existing pen that a client then recolours.
- Shots: ~/Desktop/upterm-shots/s57-icl-fix-rig144-boot{0,1,2,3}.png. Rig 144 stopped and cleared.
