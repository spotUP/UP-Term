# S56-PUSH-ROLE: every GadTools push button drew as FLAT_ROLE_PLAIN after 37b5e3d2

Date: 2026-11-04. Rig 88 (FS-UAE, 060 max, OS 3.2, depth 8 via aga256, flat on).
Branch s56-pushfix, based on origin/main 0292023f.

## Root cause (measured with gadprobe on Locale and Sound prefs)

- GadTools 3.2 BUTTON_KIND Save/Use/Cancel: type 0101, flags 8006, act 0001, 88x14
  (Locale le=12/232/452 te=170). GadgetText = NULL. GadgetRender == SelectRender,
  a BOOPSI frame image (depth -1) that carries the label itself.
- 37b5e3d2 added flat_push_label(has_text, ...) to tell a cycle gadget from a push
  button by an IntuiText inside the box. It required has_text, so every 3.2 push
  button (no IntuiText) failed and role_of returned FLAT_ROLE_PLAIN.
- Sound "Sound Type:" cycle (G10): 180x14 at 216,69, flags 8106 (disabled), act 0001,
  GadgetText le=-96 te=3 (label left of the box).
- Locale scroller arrows: 18x9 / 18x8, GadgetText NULL, act 0003 (GACT_IMMEDIATE).
- Checkboxes: GadgetText le=33 (right of the 26-wide box).
- Test Sound 200x14 and Locale G23 172x14: flags 8106, GadgetText NULL.

## Fix

amarchy/flat_core.c: flat_push_label replaced by
`flat_push_button(has_text, lx, ly, w, h, activation)`:
- GACT_IMMEDIATE set: scroller arrow, not a push button (0).
- No GadgetText: push button (1).
- GadgetText inside the box: push button (1); outside (cycle, checkbox label): 0.
amarchy/flat.c role_of passes g->Activation. Prototype and measured values in flat_core.h.
Known limit: an unlabelled cycle gadget cannot be told apart and stays a push button.

## Validation

- test-one SUITE=flat_core: 2367 checks, 0 failed. With the old rule restored the
  suite fails 2 checks (test_flat_core.c lines 789, 790); fix restored after.
- make flat-budget: PASS, _c2_disp_reg deepest 308 (headroom 76), _c2_disp_stk 292 (headroom 92).
- make amiga: rc 0. amarchy-flat md5 4f3feeb2003b41b72b98efc0cdfa9ca0.

## Pixels (rig 88 screenshots, box census; palette PAL 0 13a907, 2 ffffff, 3 6688bb, 4 3e4254)

Before (old build), Sound: Save/Use/Cancel each 3e4254 (pen4) 1136 + ffffff 84: all plain.

After (fixed build), ~/Desktop/upterm-shots/s56-push88-locale-after.png and s56-push88-sound-after.png:
- Locale Save 12,181: 6688bb 1136 / 13a907 96 -> pen3 fill, pen0 label: PASS.
- Locale Use 232,181: 6688bb 1155 / 13a907 77: PASS.
- Locale Cancel 452,181: 13a907 933 / ffffff 299 -> pen0 fill + outline: secondary PASS.
- Locale arrow 150,60: 3e4254 140 -> pen4 plain: PASS.
- Sound Save/Use/Cancel: same counts as Locale: PASS.
- Sound Type 216,80: 252732 2427 / 55dd55 93, same as before. Not a palette pen;
  the gadget is disabled (flags 8106), so this is the ghosted look, not a role bug.
  Cycle fill on an enabled cycle gadget is not yet measured.
