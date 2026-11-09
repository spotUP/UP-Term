---
date: 2026-11-04
session: s57
topic: 3.1-safe palette.prefs gave magenta pen 5
---

# 3.1-safe palette.prefs: pen 5 magenta

Q1: YES. On a 3.1 rig (rig 87) at depth 8, the raw 3.1-safe palette.prefs in ENVARC:Sys made IPrefs 40.7 read literal pen 5 as screen pen 5, which is ff00ff at depth 8. Cause: the 8-colour list kept literal pen numbers 4-7.

Palette 40.2 stores entries 4-7 as 0xFEFC + (entry-4) (0xFEFD for entry 5 measured; 4, 6, 7 extrapolated). Intuition resolves these to the last four pens (253 at depth 8). Slot 11 is 2 in both lists on Palette 40.2.

Fix: amarchy/desk_core.c pal_list_31 maps entries 4-7 to 0xFEFC.. in the 8-colour list and sets slot 11 to 2. Used only below OS 47. 3.2 output unchanged.

Test: tests/test_desk.c test_desk_palette_31_pen5 (measured bytes). Fix in place: desk suite 2293 checks, 0 failed. Fix reverted: 3 FAIL (test_desk.c:1364, 1413, 1415). Restored: pass.

Rig re-check (rig 87, 3.1, same boot path, final fix): DrawInfo pens `0 1 2 2 253 3 0 0 2 2 0 2`, no FF00FF in rgb row. Before fix pen 5 was ff00ff.

Not measured: entries 4, 6, 7 on the stock editor.
