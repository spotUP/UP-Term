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
