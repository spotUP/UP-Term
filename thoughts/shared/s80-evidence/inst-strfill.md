---
date: 2026-11-15
topic: Installer askdir string field invisible under Patina flat theme
status: WIP - root cause narrowed, no code change yet
---

# Installer string field fill (s82-inst-strfill)

Owner bug: "the dir input in the dopus installer has the same bg color as the installer bg so its not visible".

## Reproduction (rig 183, main build e0d51826, flat=on)

- DOpus5 archive unpacked to the rig, `Installer "Install DOpus5" NOLOG` driven to the askdir page
  (driver: `tools/rig` modules dopus_rig / installer_rig / install_rig).
- UITREE: `W 0 152 192 496x216 active "Install DOpus5"`, `G 92 164 331 472x12 string selected "" "SYS:"`.
- Pixel dump rows 327..346, x 158..641: every pixel is window background `1a1b26`, except the text
  "SYS:" (`a9b1d6`) and the cursor block at the left. No fill, no frame. Bug reproduced.
- patina-flat STATUS: pens text=2 line=3 back=0 outline=3 button=4; frame=hooked calls=104 flat=26.

## Violated invariant

INPUT-GROUND (OWNER-2026-11-10): every string input is drawn on the input ground pen (`flat_input_pen`),
so it stands apart from the window background. The Installer field breaks it.

## Root-cause layer: patina/flat.c INPUT-GROUND reach (not the Installer, not the pens)

Static read of Installer 43.3 (`build/rig-inputs/Installer`):
- No gadtools gadgets: the only GadToolsBase calls are GetVisualInfo, FreeVisualInfo and two
  DrawBevelBoxA calls, both in the push-button render paths (0x7598 / 0x7712, gadget box, recessed when
  GFLG_SELECTED). So no CreateGadgetA STRING_KIND and no frameiclass frame for the string field.
- Gadgets are plain Intuition structs built by hand (button builder at code 0xbd5a: Border lists in
  pens 5980(a4) / 5978(a4), which are SHINE / SHADOW; under flat both are pen 0 = background).
- The string field is therefore a plain GTYP_STRGADGET whose frame is a Border list (invisible: pen 0)
  and whose interior Intuition's own string code paints in pen 0.

Every INPUT-GROUND entry point misses it:
- `string_ground` (flat.c:848) runs only from a frameiclass IM_DRAWFRAME (c8_draw, flat.c:1061) or a
  DOpus FieldBox (flat.c:1271). The Installer draws neither around the field.
- `g9_ground` (flat.c:1191) runs only from strgclass GM_RENDER. Plain GTYP_STRGADGET is rendered by
  Intuition's internal string code, not through the strgclass dispatcher.
- Both also require StringExtend (`flat_string_ext`, flat_core.c:353). Not yet confirmed whether the
  Installer gadget sets GACT_STRINGEXTEND (no gadget-memory peek tool in tools/rig/ami.py; the creation
  site of the string gadget was not found in the disassembly within budget).

## Open: fix options (design choice, no precedent for a plain string gadget)

1. (Recommended if the gadget has StringExtend) New trigger for plain GTYP_STRGADGET: in the gfx
   RectFill hook, when a pen-0 fill covers a string gadget box of rp's window (`string_gadget_at` with
   the box test), call `string_ground` and paint `ground_box`. Same two-pen write, no pointer kept.
2. (If no StringExtend) Pen remap in the RectFill and Text hooks for the string gadget box: pen 0 fill
   and JAM2 text background become `flat_input_pen`. Touches two hot hooks; needs a per-window box cache.
3. Draw only a visible outline (outline pen 3) around plain string gadgets from the Border render,
   leaving the interior pen 0. Smallest, but breaks the INPUT-GROUND look used by DOpus and Prefs.

## Remaining steps

1. Confirm StringExtend on gadget 92: add a peek (e.g. a `patina-flat GADGETS` debug line printing
   GadgetType / Flags / Activation / Extension for the active window) or find the creation site.
2. Pick option (owner if 2 or 3). Load skill fixing-bugs; add the pure predicate to flat_core.c with a
   host test that fails before (plain GTYP_STRGADGET + RectFill box not grounded) and passes after.
3. `buildq --purpose "up-term-e8: installer string field fill" -- make test ONLY=<flat suite>`, then the
   Amiga build, rig check with the same driver, compare rows 327..346 (field fill must not be 1a1b26).
4. Owner eye check owed after the fix: Installer, DOpus5 install, "Where to install" page, field visible.

## Probe 2 (rig 183, 2026-11-15, subagent up-term-e8)

- Reached the askdir page again by script (gadgets 89..100 present, 92 = string). Rows 327..346 x 158..641
  still all 1a1b26 apart from the "SYS:" text (a9b1d6, e5e4d9) and cursor (564e29). No fill, no frame. Repro holds.
- StringExtend on gadget 92: NOT YET MEASURED. The ARexx IMPORT peek of ActiveWindow->FirstGadget returned
  nothing on the first try (rexxsupport.library not loaded for SHOWLIST); the second try, with ADDLIB, left the
  rig's wasabid unresponsive ("daemon did not answer for 15 s"), so rig 183 was stopped. Probable cause: the
  IMPORT walk read a bad address. Next try: read the gadget list from the host (UITREE gives id 92 at
  164,331 472x12) via a wasabid memory read instead of ARexx, or peek only Gadget+12/+14 (Flags, Activation)
  of gadget 92 itself and its SpecialInfo+24 (Extension).
- Driver kept at the session scratchpad (askdir.py: start rig, unpack DOpus archive, run Installer, answer to
  askdir, peek, row histogram).

## Remaining steps

1. Measure StringExtend on gadget 92 (GFLG_STRINGEXTEND 0x0400 in Flags or GACT_STRINGEXTEND 0x2000 in
   Activation, StringInfo->Extension != 0). Expectation from Installer 43.3 building gadgets by hand for 1.3:
   absent.
2. If absent (option 2): string_gadget_at (flat.c:822) rejects the gadget because it requires flat_string_ext.
   Add a pure predicate in flat_core.c for a plain GTYP_STRGADGET box (no StringExtend) that qualifies for the
   input ground, plus host test in tests/test_flat_core.c (fails before, passes after). Record the box with
   flat_gr_add when Installer erases it with a pen-0 RectFill (bb_claim pattern, flat.c ~2414) so rf_gbox and
   flat_tx_c paint input pen fill and text background; draw the frame from the same record. No ground_box /
   raa_* from the rf hook (stack budget, S71-C8-PRIVATE-STACK); run make flat-budget.
3. If present (option 1): string_ground + ground_box path, but deferred through the gr record for the same
   stack reason.
4. Rig check: rows 327..346 x 158..641 show input fill != 1a1b26 plus frame.
