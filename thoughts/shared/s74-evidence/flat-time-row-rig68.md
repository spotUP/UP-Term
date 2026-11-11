---
date: 2026-11-11
topic: Time preferences year + month row full width -- rig 68 proof of the CreateGadgetA glue (s74 step 3)
status: FAIL -- hook installed and safe, but the row never changes: gadtools calls CreateGadgetA recursively, the core ends the sequence on the nested call
branch: s73-flat-time-row-full-width
---

# Time row full width: rig 68 run

Binary: worktree build `build/amiga/amarchy-flat` at 861b24bc (241188 bytes, md5 60a50becba966733f9e6c3c53471f9df),
run as `RAM:amarchy-flat` after `Break` of the resident main-checkout copy. Rig 68 (OS 3.x, Time 39.1), started by the
main checkout. Time opened with `Run >NIL: SYS:Prefs/Time` (CLI command name, FilePart "Time"). A Time window left open
from the s73 measurement (started by RAM:wbrun before the patch was loaded) was closed first with Cancel.

## Q1 STATUS after a fresh open -- FAIL (changed=0)

    time row installed=yes calls=18 changed=0 reason=nobutton integer=54,18,53,14,14 cycle=142,18,178,14,15 frame=0

## Q2 FLAT_TROW_FRAME_OUT -- PASS, kept 0

Raw INTEGER NewGadget is left 54 width 53 (the frame box, not the 60/41 gadget box), so 0 is right. No rebuild for it.

## Q3 uitree -- FAIL (stock layout)

    W 0 0 14 332x203 active "Time Preferences"
    G 14 60 35 41x8 integer - "" "2026"
    G 15 142 32 178x14 button - "November"

Identical to the s73 measurement (60,35 41x8 and 142,32 178x14): nothing moved.

## Root cause (measured with an uncommitted trace build)

A diagnostic build (not committed; patch kept as scratchpad `trdiag.patch`) logged every CreateGadgetA call from Time as
depth.kind.id, depth = nesting inside our own hook:

    0.0.0 0.1.2 1.0.2 0.1.8 1.0.8 0.1.9 1.0.9 0.3.14 1.12.14 2.0.14 0.7.15 1.0.15 0.11.13 1.0.13 1.0.13 0.11.12 1.0.12 1.0.12

gadtools builds each kind by calling CreateGadgetA through its own library vector: BUTTON 2 -> nested GENERIC(0) id 2,
INTEGER 14 -> STRING(12) -> GENERIC, CYCLE -> GENERIC, SLIDER -> GENERIC x2 (CreateContext also arrives as a top-level
GENERIC 0.0.0). The core sees the nested GENERIC right after the first BUTTON, in phase 0, and ends the slot with
`order`; the INTEGER then finds no slot (`nobutton`). 18 calls = 7 program gadgets + CreateContext + 10 nested.
Disassembly of Time 39.1 agrees: one CreateGadgetA site (0x2092) in a wrapper (0x1fc2) reached from 7 call sites,
buttons (descriptors 0x200/0x218/0x230) from 0x12e0, then year/month/sliders (0x248..0x290) from 0x170e.

Fix level: the glue must feed the core only the program's own (outermost) calls, or the core must skip a gadtools
sub-gadget (GENERIC/STRING with the same id as the call it is nested in). A per-task depth is needed for the first;
a single global depth counter (as in the trace build) is not task safe. Not implemented here: a design choice.

## Q4 click areas -- NOT CHECKED (layout never changed)
## Q5 month change redraw -- NOT CHECKED for the new layout

Stock layout after a month click: `G 15 142 32 178x14 button - "December"`, boxes unchanged.

## Q6 unload safety -- PASS (for the installed hook; no moved layout to test)

With Time open: `Break` of the RAM copy -> `amarchy-flat STATUS` = `flat=absent`. Month click, Cancel, reopen:
`G 14 60 35 41x8`, `G 15 142 32 178x14` (stock). `ami.py ping` = wasabid 0.11b2, no requester, no guru. Reload of the
RAM copy: `time row installed=yes`. Also unloaded and reloaded twice more during the run (diag swap), no fault.

## Q7 other gadtools program -- PASS (changed=0)

Input Preferences with the patcher loaded: `time row installed=yes calls=39 changed=0 reason=ok`; window
564x195, Save 12,190 / Use 238,190 / Cancel 465,190 as created. No unpatched Input baseline was taken; `changed=0`
means the hook passed every NewGadget through untouched.

## End state

Rig 68 running, resident `UP-Term:bin/amarchy-flat` (main checkout build) started again with `Run`, Time and Input
closed, `RAM:amarchy-flat` and `RAM:amfdiag` deleted.

# Second proof (s74 step 3b)

Fix: commit bac27b87, `flat_trow_feed` passes through (returns 0, touches no state) every GENERIC_KIND call and a STRING_KIND
call of the year id while the slot holds the moved INTEGER. Host: `make test ONLY=flat_core` [OK] 4531 checks, 0 failed (new
`test_trow_nested_trace` replays the rig trace, `test_trow_nested_inert`); `make flat-budget` PASS (`_flat_cg_c` headroom 68);
`make amarchy-flat` built, 241224 bytes, md5 65ed61e06be4bf92a2dfd4cd85c993a2. Run on rig 68 as `RAM:amarchy-flat` (put with
`ami.py put`) after `Break 9` of the resident main-checkout copy; Time opened with `Run >NIL: SYS:Prefs/Time`. Clicks by screen
pixel (`ami.click_px`), keys by `ami.key`, text by `ami.py type`.

## Q1 STATUS after a fresh open -- PASS

    time row installed=yes calls=18 changed=1 reason=ok integer=54,18,53,14,14 cycle=142,18,178,14,15 frame=0

`changed` counts rows (one row = the INTEGER and the CYCLE both moved), so 1 per Time window is the expected value.
18 calls as traced; the nested calls no longer end the sequence, and `reason=ok` stays after the sliders' nested calls.

## Q2 uitree boxes -- PASS

Patched:

    W 0 0 14 332x203 active "Time Preferences"
    G 2 12 198 87x14 button - "Save"
    G 8 122 198 87x14 button - "Use"
    G 9 233 198 87x14 button - "Cancel"
    G 14 18 35 41x8 integer - "" "2026"
    G 15 65 32 255x14 button - "November"
    G 13 130 156 186x7 prop - ""
    G 13 126 154 194x11 button - "Hours"
    G 12 130 168 186x7 prop - ""
    G 12 126 166 194x11 button - "Minutes"
    G 11 12 53 311x100 custom - ""

Stock (patcher unloaded, same session): `G 14 60 35 41x8`, `G 15 142 32 178x14`, every other line identical to the above.
Year: string box 18 = frame left 12 + 6 (stock 60 = 54 + 6), frame 12..64. Month 65..319 (width 255), right edge 319 unchanged.
The frame-to-month spacing (frame right 64, month left 65) is what the core's flat_trow_month produces for side 1 (host check
`trow_ground` = 2 ground columns after flat_inset); the drawn gap itself is a pixel question, not checked here (text only).

## Q3 click areas -- PASS

- Month new left part, click (80,39): "November" -> "December", again -> "January".
- Year at its new place, click (22,38): `G 14 ... integer selected`; Amiga-X, type `2030`: `"2030"`; Return: deselected, `"2030"`.
  x 22 lies outside the stock year box (60..100), so the click reached the moved gadget.
- Where the year used to be, click (90,38): month changed "January" -> "February" (that area belongs to the month now); the year
  stayed "2030", not activated.

## Q4 boxes after several month changes -- PASS

After clicks at x 300, 150, 200, 70: `G 14 18 35 41x8 integer - "" "2030"`, `G 15 65 32 255x14 button - "June"`, all other
lines unchanged. Text cannot show: whether the redrawn cycle label, the cycle glyph and the frames are drawn at the new boxes
without leftovers of the stock frames (pixels), or the flat-look gap between the two boxes.

## Q5 unload with the moved layout open -- PASS

`Break` of the RAM copy (process 9) with Time open: `UP-Term:bin/amarchy-flat STATUS` = `flat=absent`. Month click on the open
moved window: "June" -> "July", boxes still 18/65. Cancel at (270,205): window gone. Reopen: `G 14 60 35 41x8 "2026"`,
`G 15 142 32 178x14 "November"` (stock, the Cancel discarded 2030). `ami.py ping` = wasabid 0.11b2, `ami.py requesters` empty, no
guru. Reload RAM copy, Cancel, reopen: STATUS `calls=18 changed=1 reason=ok`, `G 14 18 35 41x8`, `G 15 65 32 255x14` (moved again).

## Q6 other gadtools programs -- PASS

Input Preferences (564x195, has a STRING gadget) and ScreenMode Preferences (604x198, INTEGER gadgets ids 13 and 14: Width
116,126 56x8, Height 116,142 56x8) opened with the patcher loaded and again unloaded: window gadget lines identical both ways
(`diff` empty for both). STATUS after the loaded pass: `calls=89 changed=1 reason=ok` (the 1 is the earlier Time row; the
ScreenMode INTEGER id 14 took no slot because the program is not Time).

## End state

Rig 68 running (fs-uae pid 29938), resident `UP-Term:bin/amarchy-flat` (main checkout build) started again with `Run`
(`flat=on`), Time, Input and ScreenMode closed with Cancel, `RAM:amarchy-flat` deleted.
