---
date: 2026-11-09
topic: Bug 0, Amarchy desktop theme requester bottom border (s66)
status: not reproduced
---

# Bug 0: theme requester bottom border

Owner report (640x256, topaz 8, KS 3.2): the "Amarchy desktop theme" confirm
requester is cut off at the bottom, or the bottom border is missing / overwritten
after the theme applies. Same owner: the DOpus Environment window bottom is
chopped (window ends at row 222/256, bottom border in stock state).

## Setup

- Rig 166, KS 3.2, 060 --max, 640x256x8, topaz 8 (ENV/ENVARC Sys/font.prefs removed).
- main at 8f7a8a50 (HEAD now f7d1250c, other sessions committed meanwhile).
- Prefs: flat on, desktop-theme on. Pick tokyo-night -> vantablack via
  amarchy_desk_rig.pick(keep=False) with g.close_menu patched to a no-op so the
  requester stays open (unpatched, close_menu answers it: CONFIRM_REVERTS_USER=1).
- Frames from ami.req(0x07), 640x256 RGB; window geometry from read_ui (row_scale 2).

## Measurements

- Requester (amarchy/desk_confirm_dos.c:32-45, BuildEasyRequestArgs, 5 lines,
  Keep|Revert, 15 s timeout) opens at 0,0, size 434x92; seen 29-37 s after pick start.
- Pens: 8d8d8d = outline/accent and interior; 000000 = border back.
- Bottom: rows 88-90 black border fill with 2 px grey sides (x0-1, x432-433);
  row 91 full-width grey outline. Top border rows 10-12 symmetric with bottom.
  Buttons rows 74-87.
- Maps at t+33.7, t+35.4, t+37.2: bottom rows identical; only text rows 25-33 change.
- Bottom ring 1 px vs side 2 px matches design (flat_glyph.c outer_strip).

## Verdict

NOT reproduced on this setup: outer ring and bottom border complete after apply.
No source change, no commit.

## Open

- Requester state before ~t+29 s (during the apply redraw) not sampled.
- DOpus Environment window not measured; s65 found FLAT-D4 d4_drawbox /
  d4_fieldbox return SKIP under flat pens (flat.c ~817-835) and draw nothing.
  Prime suspect for the "stock-state bottom border at row 222".
- Owner's exact theme / DOpus config not tried.

## Side findings

- Rig 1's build/rig folder and its fs-uae vanished mid-session (not from s66 commands).
- tools/rig/rig.py seed_rig_vtc crashes when rig 1's VTC: is missing; s66 used a
  scratchpad wrapper that skips the seed.
- tools/rig/rig.py has another session's uncommitted edits; left untouched.
