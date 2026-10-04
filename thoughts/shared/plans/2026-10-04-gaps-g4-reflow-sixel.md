---
date: 2026-10-04
topic: gaps G4 -- reflow on resize by default (#11, ledger W1) and DEC sixel graphics (#20)
tags: [plan, ledger, reflow, sixel, graphics, gaps]
status: draft
---

# G4: reflow + sixel -- progress ledger

Source: `thoughts/shared/research/2026-10-04_modern-terminal-gaps.md` items 11 and 20;
`thoughts/shared/plans/2026-09-28-vtcon.md` W1. Branch `feature/gaps-g4-reflow-sixel`.
Done = host tests through `vt_write`/`vt_resize` (engine) and the profile/slash/prefs
suites green, `make amiga` with zero warnings, one full `make test` at the end. Rig checks
are the owner's (the rig is busy with the speed benchmark).

## Decisions (do not re-litigate)

- Reflow model: the whole history is one text. Scrollback + rows a resize pushed out +
  primary screen are re-wrapped together at the new width (a logical line that started in
  the scrollback joins its rest on the screen). The screen is the bottom of the result,
  rows below the cursor dropped first; rows above it go back to the scrollback, and a
  grow pulls them down again (iTerm2 / Terminal.app / Alacritty). The alternate screen
  is cut or padded (its program redraws). Personalities without scrollback (amiga, the
  console units) keep the old screen-plus-overflow behaviour.
- Out of memory for the whole-history pass: the screen-only reflow; out of memory for
  that: the plain cut-or-pad. Never a half-done layout.
- The selection is cleared when a reflow changes the width (Alacritty); mapping it onto
  the re-wrapped text is not done.
- Host default: reflow on for every UP-Term/XCON window; profile `reflow = on | off`,
  Settings > Reflow on resize, `/reflow on|off`, Prefs checkbox.
- Sixel storage: `vt_cell.pad` bit 0 (VT_CELL_IMAGE) marks a cell that shows an image
  tile; text or an erase over it clears the bit for free (the cell is rewritten whole).
  Which image and which tile: a per-line list of placements (`vt_line.img`, an index into
  a pool in the term), moved with the line by every scroll, kept in the scrollback,
  carried by reflow, dropped when the line is cleared or freed. sizeof(vt_cell) unchanged.
- Images: 8-bit indices into a compact per-image palette (entry 0 = unset pixels,
  the default background); up to 1024 x 1024 pixels; all images together at most
  2 MB (oldest evicted; a stale placement draws nothing).
- Renderer: one `vt_images()` test per draw_rows call; palette screens obtain pens per
  colour (ObtainBestPen PRECISION_IMAGE, nearest when the screen is full) and draw with
  WriteChunkyPixels; true-colour screens with cybergraphics WriteLUTPixelArray (LVO -198,
  from AROS cybergraphics.conf) straight from the image's indices and palette.
- OSC 1337 / kitty graphics: not implemented (see the section below).

## Checklist

- [x] R1 engine: scrollback reflowed with the screen (wide chars, wrapped at the last
      column, cursor on a wrapped line, scrollback full, shrink-grow round trip)
- [x] R2 host: reflow on by default, profile key, Settings menu, /reflow, Prefs checkbox,
      selection cleared on a reflowing resize; matrix/README/conf docs
- [ ] X1 engine: streaming sixel decoder (DCS P1;P2;P3 q ... ST), raster attributes,
      colour registers RGB + HLS, repeat, aspect, memory cap
- [ ] X2 engine: image store and placement (cursor/scroll semantics, ?80 DECSDM, ?1070),
      scroll/clear/overwrite/scrollback/reflow carry, RIS
- [ ] X3 engine: DA1 ;4, XTSMGRAPHICS (CSI ? Pi;Pa;Pv S), DECRQM ?80/?1070, matrix
- [ ] X4 renderer: image tiles drawn (palette pens / WriteLUTPixelArray), zero cost
      without images
- [ ] X5 OSC 1337 / kitty graphics: documented out of scope with reasons
- [ ] F  full `make test`, `make amiga` zero warnings

## Progress

- R1 81c643d: whole-history reflow; also fixed re-wrapped rows that never raised
  vt_line.used (a later clear left text behind). Suite `reflow` (66 checks), proven red
  on the old engine (14 failures) and the used fix proven by an abort without it.
- R2 587c243: host default on + profile/menu/slash/Prefs; le_resized keeps the line
  editor on its line (test red without it); selection cleared on a reflowing resize.
