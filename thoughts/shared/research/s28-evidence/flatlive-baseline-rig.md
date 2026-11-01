---
title: s28 FLATLIVE-BASELINE-RIG, rig 10 (OS 3.2, aga256, depth 8), commit 2cfb772c
---
Setup: ENV:Sys and ENVARC:Sys Palette.prefs = saved palette 0:102030 1:a0b0c0 2:405060 3:d0e0f0 4-7:111111 222222 333333 444444
(entries 4-7 land on pens 252-255 at depth 8, read back as such). Flat switches via ENV:Amarchy/prefs, theme pick through the
drawn menu (amarchy_desk_rig.py pick, wbpens). wbpens F marker now reads Amarchy.Flat st.enabled.

| state                     | 0      | 1      | 2      | 3      | 4      | 5      | 6      | 7      |
|---------------------------|--------|--------|--------|--------|--------|--------|--------|--------|
| before flat ON            | 000000 | a0b0c0 | 405060 | d0e0f0 | 0000ff | ff00ff | 00ffff | ffffff |
| flat ON, vantablack (1)   | 000000 | 222222 | bebebe | 8d8d8d | 0000ff | ff00ff | 00ffff | ffffff |
| flat OFF (1)              | 000000 | a0b0c0 | 405060 | d0e0f0 | 0000ff | ff00ff | 00ffff | ffffff |
| flat ON, starship (2)     | 000000 | 0e0e10 | f5f5f7 | ff9f0a | 0000ff | ff00ff | 00ffff | ffffff |
| flat OFF (2)              | 000000 | a0b0c0 | 405060 | d0e0f0 | 0000ff | ff00ff | 00ffff | ffffff |
| flat ON, vantablack (3)   | 000000 | 222222 | bebebe | 8d8d8d | 1a1a1a | 222222 | b6b6b6 | a4a4a4 |
| flat OFF (3)              | 000000 | a0b0c0 | 405060 | d0e0f0 | 0000ff | ff00ff | 00ffff | ffffff |

Rows 1-2 ran in one driver pass from flat OFF; row 3 is a separate pass (pick with a different menu cursor). Pens 252-255 read 111111 222222 333333 444444 throughout.

Verdict: FAIL on pen 0 only. After flat OFF pens 1-3 equal the saved palette and 4-7 equal 0000ff ff00ff 00ffff ffffff in all three
cycles, independent of the cursor/direction in the open menu. Pen 0 reads 000000, saved palette says 102030; ENV:Sys/Palette.prefs and
T:amarchy-revert/flat-palette both hold 102030 for entry 0, so the file is right and the live pen 0 is not restored (pen 0 was already
000000 before the first flat ON in this run, so a pen 0 set earlier by the theme/backdrop route is never put back).
Unexpected: in cycles 1 and 2 pens 4-7 stayed stock after flat ON (not the theme colours); in cycle 3 they took theme colours
(1a1a1a 222222 b6b6b6 a4a4a4). Whether a Workbench reset after dk_live_pens overwrites them in the OFF->ON transition is not established.
