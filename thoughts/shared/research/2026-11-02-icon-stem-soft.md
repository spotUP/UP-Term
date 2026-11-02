---
date: 2026-11-02
topic: HANDLER-ICON-GRID-FIT step 2, why rig 35 saw 6 of 8 icons SOFT
status: answered, no C fix
---

# Icon stems SOFT on rig 35: not a render/ttf.c bug

## Question

Rig 35 (fs-uae, AGA hires PAL 1:2) judged 6 of 8 Nerd Font icons SOFT with
`gridfit_rig.py stem_verdict`. Candidates: pair not detected, sub-pixel stem not
widened, horizontal stems not snapped, rasterizer splits rows after the snap.

## Answer

None of the four. On the host, current main's `render/ttf.c` (snap_stems,
ttf.c:618, called at ttf.c:780-786) snaps the icons. The rig pixel maps match
the UNFITTED render of the same font at the same geometry, so rig 35 ran a
handler without step 2 (c376883e) on its mounted XCON:. Not proven which
binary L: held (in the rig's disk; rig not touched per brief).
build/rig35/vtc/vtcon-handler equals build/amiga/vtcon-handler (built
07:01:15, step 2 committed 07:01:55); a mounted XCON: keeps the old handler
until a reboot (gridfit_rig.py header, memory feedback-handler-install-reload).

## Host measurement

Harness: scratchpad stemh/h.c (bench_ttf's file io + ttf_place(TTF_ICON, 16, 8,
base 6, rx 1, ry 2) + ttf_render_fit with snap 0 and pl.snap), font
build/symbolsnerd/SymbolsNerdFontMono-Regular.ttf (same bytes as
build/third_party/f1/SymNerdMono.ttf). Verdict = gridfit_rig.stem_verdict on the
coverage ('#' 255, '.' 0, 'o' between).

| icon | pairs | raw   | fit     |
|------|-------|-------|---------|
| F07B | 3     | SOFT  | SNAPPED |
| F15B | 3     | SOFT  | SNAPPED |
| F0C9 | 3     | SOFT  | SNAPPED (bars on whole rows 0, 3, 6 at h 8) |
| F00C | 0     | none  | none    |
| E0A0 | 3     | SOFT  | SNAPPED |
| F013 | 0     | SOFT  | SOFT (no axis-aligned on-curve pair: cog teeth) |
| F023 | 4     | SOFT  | SNAPPED |
| F1C0 | 1     | SOFT  | SOFT (rows are ellipse arcs; sides already on boundaries; fit == raw) |

F0C9 fit is crisp for every h 7..9 and base 5..7 tried.

## Rig maps vs host

The rig's rows are cell rows 1..7: gridfit_rig.py takes the anchor (E0B0)
top as the first non-background row, and E0B0's cell row 0 coverage (~0.3) is
below the first AA shade, so cell row 0 is cropped and the cell reads 5x7
instead of 8x8. With that shift:
- E0B0 rig rows match host fit rows 1..7 column for column.
- F013 rig rows match host rows 1..7 (raw == fit for the cog).
- F0C9 rig rows 0 and 3 (14 'o') are host raw rows 1 and 4 (coverage 0.1-0.3);
  host fit has those rows empty. Rig rows 2 and 5 are raw rows 3 and 6.
- E0A0 rig col 2 7/7 is the raw column; the fit puts the stem on two whole
  columns with only the font's notch soft (rows 3-5). The earlier E0A0 hunt
  (3 pairs snap, soft pens from the notch) stands.

## Follow-ups (not done here)

1. Rig re-check after `rig.py install` + reboot (or a fresh XCON name), and
   check /about or the handler date before the shot.
2. gridfit_rig.py: the anchor's top row is missed when its coverage is below
   the first shade; take the cell top from the jbmono U+2588 geometry
   (`again x0,y0,cw,ch`) or the window's text row, not the anchor's first ink.
3. F013 and F1C0 SOFT on host are font design at 8 rows (no straight pairs /
   curved bands), not snap failures; stem_verdict counts curved rows as stems.
