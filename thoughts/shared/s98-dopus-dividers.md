---
topic: DOPUS-GADGET-DIVIDERS (v1.3 blocker), frames around "<", popup arrow and "Off" in the DOpus lister strip
status: NO CODE CHANGE. Draw site found; current flat code already removes the frame. Rig check UNVERIFIED.
date: 2026-11-24
session: up-term-4c (s98)
---
# Draw site (DOpus Magellan open source, github MrZammler/opus_magellan, same layout as 5.104)
- Program/lister_display.c:275-278 (lister refresh): `display_gui_border()` on name_area, command_area (popup down
  arrow), parent_area ("<") and status_area ("Off"). Only when the lister is NOT in icon mode (name mode lister).
- Program/display.c:705-717 `display_gui_border`: rect grown 1 px each side, then `do3dbox(rp, &border, 0)`.
- Program/graphics.c:27-30 `do3dbox` = `DrawBox(rp, rect, GUI->draw_info, recessed)`: the dopus5.library DrawBox LVO,
  which Patina already patches (patina/flat.c d4_drawbox -> d4_flat -> d4_flat_pens, FLAT_EL_DRAWBOX). No inline
  Move/Draw; no new patch point needed.
- Interiors: Program/lister_title.c lister_show_status + lister_title_pens: RectFill of the inner rect only, pen =
  BACKGROUNDPEN for an "Off" lister, env source_col/dest_col for SRCE/DEST. Areas are 3 px apart, so neighbouring
  borders touch: the "dividers" are two adjacent 1 px frame columns.

# Why the owner's screenshot shows frames, and why the code already fixes it
- Owner shot ~/Pictures/claude-inbox/CleanShot 2026-11-10 at 01.18.23@2x.png (taken 2026-11-10 01:18). Pixel runs: block
  interiors 1a1b25 (ground), frame rows/columns 82a1f1 (accent) on top, bottom, left of the arrow and between
  arrow / "<" / "Off". That is the old full accent ring of a raised DrawBox.
- 49d0373c (2026-11-10 07:23, six hours AFTER the shot) "DOpus raised buttons lose their ring (DOPUS-BUTTON-NO-RING)",
  e8c0dc0c (14:14) "never fall back to the accent pen", f998ac0a (11-11) FLAT-SPACING.
- Current path for these boxes: raised DrawBox -> flat_dopus_ring(FLAT_EL_DRAWBOX, 0) == 0 (render/flat_core.c:337,
  pinned tests/test_flat_core.c:708 and :717 for the 0x22000000 d0) -> patina/flat.c:1421 no-ring branch; the box is
  20 x (font+2) so flat_inset succeeds (FLAT_INSET_MIN 5) and the ring pen = BACKGROUNDPEN (patina/flat.c:1440).
  "Off" interior is BACKGROUNDPEN too, so the frame is invisible: no frame, no dividers.
- SRCE/DEST listers: interior = configured source/dest colour, ring = ground: a 1 px ground band per side (2 px between
  neighbours). That is the FLAT-SPACING gap every flat DOpus button gets, not an accent frame. Owner may veto at the eye
  check; the fix then is to give the ring the face pen for DrawBox (drop the flat_inset override for FLAT_EL_DRAWBOX),
  which would also change every other raised DOpus button.

# Unverified (needs a rig with DOpus5 + Patina flat on)
- That the strip now draws without frames. Earlier rig attempt (strip-dividers.md) saw no strip because the
  DeviceList lister is not a name-mode file lister. Recipe: start DOpus, `rx "ADDRESS DOPUS.1; lister new SYS:"`,
  make sure it is in name mode (`lister set <handle> mode name`), grab the strip row: expect no 82a1f1/accent columns
  between arrow, "<" and "Off"; check square and non-square pixel modes (side_px 1 and 2).
- Patina pens: assumed SHINEPEN and BACKGROUNDPEN share a colour so flat_dopus_decide returns SKIP (flat path taken).
