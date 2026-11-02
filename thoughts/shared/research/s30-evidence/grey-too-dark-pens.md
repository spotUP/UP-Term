# GREY-TOO-DARK pen probe (rig 46, fs-uae, OS 3.2, HEAD bf57df5a, build/amiga 08:33)

Setup: Amarchy started, prefs desktop-theme/flat-system-look/flat-window-gadgets = on, theme picked (amarchy_desk_rig.pick).
Palette read with T:dripens Workbench (tests/amiga/dripens.c) and amarchy-flat STATUS; shot pixels by PIL.

## DrawInfo (dri_Pens slots 0..11, themed)
pens: 0 1 2 0 0 0 2 0 2 2 0 1  (DETAIL0 BLOCK1 TEXT2 SHINE0 SHADOW0 FILL0 FILLTEXT2 BG0 HILITETEXT2 BARDETAIL2 BARBLOCK0 BARTRIM1)
palette: pen0 000000, pen1 222222, pen2 BEBEBE, pen3 8D8D8D, pen4 1A1A1A, pen5 7A7A7A, pen6 B6B6B6
No dri_Pens slot holds pen 3, 4 or 5: anything drawn in those is a hard-coded pen number or Amarchy's own pen, not a DrawInfo role.

## Table
| element | pen | RGB | proof |
|---|---|---|---|
| list-view drawer names | 3 (fixed number, in no DrawInfo slot) | 8D8D8D | Shot lv.png: root-drawer list window text pixels all 8d8d8d; pen 3 = 8D8D8D; no slot = 3 |
| list-view file names | 1 (fixed, = BLOCKPEN value) | 222222 | NOT re-measured this run (no file rows reached: SYS:S/SYS:Prefs did not show as list); from owner shot s29 + ledger. UNVERIFIED here |
| sizes / dates | 2 (TEXTPEN) | BEBEBE | ledger measurement; pen 2 = BEBEBE = TEXT slot. Not re-measured here |
| screen backdrop | 5-ish tile pen | 222222 | shot: 222222 fills the screen outside windows (84260 px) |
| scroller track/knob region | 4 | 1A1A1A | amarchy-flat STATUS "button=4" (flat_button_pen = 4); pen 4 = 1A1A1A; shot: 1a1a1a bbox x3..457,y12..100 in window = prop gadget strips (3022 px) on a 000000 body. So knob follows Amarchy's flat path (pen 4 = DK_SELECTION = desk_selection(t) = 1A1A1A), not a WB bypass. Pen 4 is already a dark SELECTION value: it is dark by theme choice, independent of pen 1 |
| title text 7A7A7A | 5 (DK_MUTED) | 7A7A7A | pen 5 palette = 7A7A7A (not re-traced to the draw call) |

NOT done: live SetRGB32 pen-1 change to prove names follow (no existing rig tool: tests/amiga has palprobe/rgbprobe read-only; no setrgb tool). Budget spent on getting the list view up. UNVERIFIED that file names are pen 1 by direct change; ledger inference stands.

## (c) DK_DARKER / desk_darker users (non-test, amarchy/)
- desk_core.c:87 desk_role(DK_DARKER) = desk_darker(t) = pen 1 colour (222222 = bg*scale)
- desk_core.c:150 DK_BLOCK -> DK_DARKER (BLOCKPEN: slot 1; stock icons draw outlines/black with it)
- desk_core.c:153 DK_SHADOW -> DK_DARKER (overridden to DK_BG by desk_pens_flat:  SHADOW=BG)
- desk_core.c:160 DK_BARTRIM -> DK_DARKER (bar trim line)
- desk_core.c:217 icon-pen backdrop below 3 planes = DK_DARKER (same tone as backdrop)
- desk_core.c:360 backdrop colour = desk_darker(t) when flat (root tile, pen 5/backdrop pen is recoloured to it, not pen 1 itself)
- desk_core.c:426 comment: pen 1 = BLOCKPEN/BARTRIMPEN, "black in stock icons" -> ICON OUTLINES in pen 1 would turn light if pen 1 = fg
- desk_core.c:674 wbfont FP_BACK = DK_DARKER (label backdrop behind outlined icon text; would go light = bad)
- desk_core.c:709 desk_check contrast test (darker vs fg) - logic, not a draw
- desk_dos.c:688 desk_palette_set_rgb for backdrop pen with desk_darker
- desk_core.h:13 enum, :35 decl, :66,:112 comments
Would turn light if pen 1 became fg: everything drawn with fixed pen 1 (list file names - the goal; stock icon outlines/black pixels; BLOCKPEN users; BARTRIM line; wbfont back if it indexes pen 1 rather than a copied RGB). DK_BLOCK/DK_BARTRIM (slots 1, 11) read pen 1 live and would go light too unless remapped to DK_BG as ledger proposes.
Shot copy: ~/Desktop/upterm-shots/s30-grey-listview.png
