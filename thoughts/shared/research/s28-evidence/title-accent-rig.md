# FLAT-TITLE-ACCENT rig check (rig 11, OS 3.2) -- FAIL, not landed

Build 3cc60dd0 amarchy-flat, flat on via theme pick (vantablack row), pens text=2 line=3 back=0 outline=3 ptext=0.
Palette read on the rig: pen0 aaaaaa, pen1 000000, pen2 ffffff, pen3 6688bb (the stock-like palette, not the 8d8d8d one).
Probe: titleprobe.c, driver titleprobe-driver.py, log titleprobe-32.log. Window 240x90, BorderTop=11.

| Region | Expected | Measured |
|---|---|---|
| Active title band, empty part (130..180,1..9) | all pen3 (459 px) | A: 363 pen3 + 96 pen0; B: 235 pen3 + 224 pen0 (partial band) |
| Active title text zone (24..120,1..9) | pen3 band, text pen0 | 728 pen0, 145 pen3: band missing behind text, glyphs drawn pen3 |
| Active side border (1..3,15..84) | unchanged dark | PASS: 210 pen0 |
| Inactive title text | pen_dimtext (dark_foreground) | FAIL: glyphs pen2 (white), 164-174 px; band pen0 |
| Body | unchanged | PASS: 14718 pen0 |

Reading: the hook fires only partly. Band reaches some columns, not behind the text, so the order assumption
(border, then title Text) is wrong for this OS path or the JAM2 text box repaints pen0 over the band. ptext=0 may also skip
flat_tx_c (flat.c requires pen_ptext truthy, same limit as FLAT-BUTTON-FILL). Inactive dim not applied for the same reason.
Open: check flat_tx_c early-out on pen_ptext==0, then BgPen/DrawMode of the title Text call.

## Iteration 2 (92cb7629, rig 11, OS 3.2, screen depth 8)

Cause of the missing band behind the text: the title Text call is JAM2; its cell box repaints BgPen (pen 0) over the band
(728 px pen0 in iteration 1). No early-out on pen_ptext==0 exists in flat_tx_c (text_pen>=0 is tested, 0 passes). Fix: BgPen = band pen
for that call (flat_title.bg_pen), restored after. Host test pins bg_pen; reverting the assignment fails test_flat_core.c:1034.
Log: titleprobe-32-iter2.log.

| Region | Expected | Measured |
|---|---|---|
| Active text zone (24..120,1..9) | band pen3, glyphs pen0 | 709 pen3 + 164 pen0 (glyphs) PASS |
| Active empty band (130..180,1..9) | 459 pen3 | 430 pen3 + 29 pen0 (6% residual, unexplained) |
| Active side border | dark | 70 pen1 + 140 pen3 (line pens, not accent band) PASS |
| Body | unchanged | 14718 pen0 PASS |
| Inactive glyphs | dark_foreground RGB | pen5 (DK_MUTED), 174 px; but pen5 on the WB screen is ff00ff: the vantablack palette is NOT loaded into the WB screen (pens 0-3 stay aaaaaa/000000/ffffff/6688bb), so the RGB is wrong. FAIL on RGB |
