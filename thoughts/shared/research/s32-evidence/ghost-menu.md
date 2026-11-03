# S32-GHOST-MENU: rig probe of disabled menu ghosting on 3.2 (rig 37, --os 32)

Status: step (a) partly done, (b)-(d) NOT started. Blocked: the menu ITEM ghost is not a ROM pattern RectFill
into a layer without a Window, so the planned second gate would not see it. No code committed.

## Probe
TEMP log in flat_rf_c (amarchy/flat.c, after `r->rcalls++`): ROM caller (ret in [irom_lo, irom_hi)), rp->Layer set,
rp->Layer->Window NULL, rp->AreaPtSz != 0. Logged caller offset, AreaPtrn[0], AreaPtSz, rect, layer bounds, FgPen
into a 16-entry ring in flatshared, printed by `amarchy-flat STATUS` while the menu was held open.
Probe patch: /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/286924bf-47a4-4018-b024-45e49a009a2e/scratchpad/ghost-probe.patch (reverted from the tree).
Binary: build/amiga/amarchy-flat md5 a3aef7f29ccf5ab8a3813bb02eabb2ec, run from RAM: with BOOT (rig had no kit).
Menu held open: mouse button held over the bar, title "Window" (inverted at x 91-147), pointer at (100,16).

## Result (one line for the whole menu open, rectfill calls=60)
    mghost 1 102120 17476 1 159 0 206 9 0 0 639 10 2
    = ROM offset 102120, AreaPtrn 0x4444, AreaPtSz 1, rect (159,0)-(206,9), layer = screen bar (0,0)-(639,10), FgPen 2
This is a disabled menu TITLE in the bar (x 159-206, right of "Window"; bar rows 1-7 show its text dotted).

## Menu items: ghosted in pixels, but NOT logged
Screenshot ~/Desktop/upterm-shots/vtcon16-ghostmenu-probe1.png, measured by script (no image read), Window menu box x 92-240:
- rows 23-27 (first item, disabled "New drawer"): black text pixels on a period-4 grid (row 24 "abbbabbbabbb"),
  the 0x4444-style dither. Same at rows 57-60 (a later disabled item).
- rows 39-54: enabled items, solid text.
- Amiga-key glyph column x 222-240 (aaaaaa): period-4 dither on rows 30-37 and 57-60, solid on rows 39-55.
So the item ghost IS a pattern, but no matching RectFill reached the probe.

## Meaning for the planned gate (hypotheses, not tested)
The item ghost escapes one probe condition: (1) rp->Layer NULL (3.2 may render the menu box into an off-screen
BitMap, then blit), (2) the item layer has a Window, (3) caller outside the ROM range, or (4) BltPattern/blit, not
RectFill. Next probe: also log AreaPtSz != 0 RectFills with rp->Layer NULL, with a Window, and with a non-ROM
caller; design the gate from that line. The bar-title ghost is caught by the probed condition today and could be
served by a bar-title box match.

## Not done
(b) gate design, (c) flat_core helper + tests + revert output, (d) pixel proof after the fix.
