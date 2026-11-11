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
