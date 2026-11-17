---
date: 2026-11-09
topic: DCTelnet RMB screen bar bisect (rig 68), amarchy-flat 6d3df261 vs 7b99fe6e
---

# Result: NOT reproduced in any of the three runs

Setup: rig 68 (rebooted, purpose "s69 session: DCTelnet RMB screen bar bisect"), DCTelnet.020 from DCTelnet-b004b5b.lha
copied to RAM:DCT. flat-starting deleted before each flat start. Boot flat (main's installed build) was stopped with
Break; each test flat started as RAM:flat6d / RAM:flat7b, then DCT launched. Screen: 640x256 depth 4, title "DCTelnet 2.0-UP/X ...",
one window 640x256 (the screen mode requesters did not appear: prefs were already saved from the first launch via
the information/mode requesters, mode = default PAL hires; picked by clicking OK on the preselected entry).
IPrefs "System Request" (Retry/Cancel) open since boot on the Workbench screen; not touched, not on DCT's screen.
Test: pointer to (320,100) in the window, right button down held 60 ticks, full grab, compare top 24 rows to idle.

| Run | Build | RMB held: screen top 24 rows |
|---|---|---|
| control | flat absent (STATUS "absent") | rows 0-9 = bar (colour a9b1d6 with 414868 text), 10 rows differ from idle; text at x=8.. reads as menu/title strip glyphs; no stray pixels |
| flat 6d3df261 | amarchy-flat built from 6d3df261 (STATUS flat=on) | identical bar: 5613 bar pixels, 10 rows differ from idle |
| flat 7b99fe6e | amarchy-flat built from 7b99fe6e (STATUS flat=on calls=44) | identical to 6d3df261, pixel counts equal |

Evidence is pixel counts and ASCII dumps of rows 0-13 (no screenshots kept). The bar glyph text was judged by shape
only (starts "DC Telnet" then a second word at x~100, consistent with a menu strip: unverified which strings).
Not checked: the right end of the bar (x 560-640) in the control and 6d runs (the 7b dump showed only the bar fill, no depth gadget).

Conclusion: no regression between 6d3df261 and 7b99fe6e on rig 68. The owner's failure on rig 1 depends on something
rig 68 does not have (different screen mode: rig 68 Workbench is RTG 800x600; flat config/theme; Workbench palette; or the
bar fill in rig 1 being drawn at a moment when flat's saved state differed). Next step: compare rig 1's Amarchy prefs
(theme, flat on/off), Workbench screen mode, and the exact DCTelnet screen depth/mode picked.
