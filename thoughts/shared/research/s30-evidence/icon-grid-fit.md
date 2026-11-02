
## Step 2 (implemented, branch s30-gridfit)
- gen_menu_icons.fit_stems_y(polys, rows) next to raster; raster(font, cp, fit_rows=None) and new raster_polys(polys, fit_rows). gen_lucide_icons.coverage passes h only when h != w, so 32x32 never enters the fit.
- Stem = near-horizontal edge pair (dy <= 1% em, run >= 5% em), opposite direction, gap <= 1.5 target px, filled between (winding test). Even snapped heights centre on a row boundary, odd on a pixel centre (a pixel-centre rule on a 2 px stem would half-cover rows). Stems that would cross or share an earlier anchor are skipped.
- tests/test_icon_grid_fit.py (Makefile ONLY=icon_grid_fit): synthetic bar [0.5, 0.5] -> [1.0]; metric 187 -> 146 partial rows over both sets; fails with fit disabled.
- Mockup: ~/Desktop/upterm-shots/s30-icon-grid-fit.png (unchecked by eye); top = before, bottom = after.
