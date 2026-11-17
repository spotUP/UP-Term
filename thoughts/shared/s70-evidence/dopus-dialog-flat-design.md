# DOpus dialog gadgets flat: design draft (s71 main thread, 2026-11-10)

Status: DRAFT. Waits for agent 41's measurements (reference looks, Environment map) in `dopus-dialog-callers.md`.

## Owner rulings (final)
- DOpus dialog buttons: filled and rounded like the other flat buttons, same on every dialog.
- Inputs: no outline, own background colour "like our other inputs" (reference input still to be named by agent 41).
- No raw DOpus dialog ships: release blocker.

## Violated invariant
One gadget kind has one look on every window under the patcher. DOpus push buttons draw through dopus5's internal
bevel (hook `d4_bevelbody`, flat.c) which ends in `flat_dopus_box_draw`: a square 1 px outline, no fill. Every other push
button ends in `raa_frame_draw` / `raa_frame_box` + `raa_frame_corners`: rounded solid fill, label recoloured by the Text
patch (`flat_tx_arm`). Root cause level: the bevel hook has no gadget-kind decision, it treats every bevel as a box.

## Reuse: the INSTALLER-BORDER-BUTTON pattern (flat.c `bb_claim` / `bb_draw` / `bb_done`, dispatched from `flat_rf_c`)
The Installer draws erase RectFill, label, then frame. The patcher claims the erase RectFill of the exact gadget box
(`border_button_at`), paints the rounded fill there (`bb_draw`, static `bb_job`, off the deep stack path) and registers the
box with the ghost and label trackers (`bb_done`: `flat_gh_frame`, `flat_tx_frame`). DOpus draws border (bevel), interior
RectFill, label (agent 39). So the same mechanism fits with the claim moved to the bevel call:

1. `d4_bevelbody`: when the call is a push button (decision below), draw NO outline; record a pending button
   {task, surface (`flat_tx_surface`), box, pressed = recessed == 1} in one static slot claimed under Forbid like `bb_task`.
   A busy slot means the existing outline path runs (counted, STATUS).
2. `flat_rf_c`: a RectFill from the pending task on the pending surface whose rect lies inside the pending box (the
   interior fill; exact inset to be measured) becomes `bb_draw` over the FULL box with ids from pressed and role from
   `flat_button_role` over the window's DOpus buttons (or PLAIN when siblings are unknown), then `bb_done`. The slot is
   released there. A pending slot that never sees its RectFill is released at the next bevel call of the same task.
3. Label: `flat_tx_frame` armed by `bb_done` recolours the label as for every other filled button.

No new draw code: `bb_draw`, `bb_done`, `raa_frame_box`, `raa_frame_corners`, `flat_tx_arm` are reused. New code is the
pending slot, the push-button decision (pure, in flat_core.c, host tested) and the RectFill match (pure, host tested).

## Push-button decision
Caller offset 0x2c9e (5.104) serves push buttons AND check boxes (26x15), so the offset alone is not enough. Preferred:
the gadget under the box (`rp->Layer->Window` gadget list, exact box match as `border_button_at` does) and its class /
GadgetType / Activation (toggle select = check box). Fallback only if dopus gadget boxes do not match the bevel rect:
per-build table row carries the button call site offset plus a pure size rule. Decide after agent 41's map.

## Check boxes, scroller arrows, fields
- Check boxes: same pending-slot path with the flat check box look measured by agent 41 on a Workbench Prefs window.
- Scroller arrows (caller 0x4308): look of the flat Workbench arrow buttons; reuse the glyph draw of the lister arrows.
- Fields (`dopusstrgclass`, FieldBox): no ring (done, a417366d). Ground colour: strgclass repaints with its StringExtend
  pens, so the ground is set there (pens at gadget creation or at the FieldBox call), never by a fill at FieldBox time.

## Budget and safety
- `d4_bevelbody` keeps its 384-byte stack budget: it only writes the static slot. The draw runs in `flat_rf_c`'s helper
  frames, already measured by `make flat-budget`.
- FLAT-UNLOAD-INFLIGHT: the slot is released on unload (same place `bb_task` is).
- Per-build: 5.104 only has table rows today; other dopus5 builds keep the outline path.

## Proof plan
- Host: decision and match functions in `tests/test_flat_core*`; regression test fails on 7241d8d1 (outline), passes after.
- Rig 68: pixel census of Environment Cancel/Save/Use and MakeDir OK/Cancel: fill = button pen, rounded corners, no
  7aa2f7 outline; STATUS counter for claimed buttons moves; traffic and unload checks from agent 24's list.
- Diagnostic commit 839b4526 reverted by a plain revert commit before the fix lands.
