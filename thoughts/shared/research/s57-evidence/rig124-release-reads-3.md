# Rig 124 release reads 3 (2026-11-05)

Rig 124: 3.2 full install, kept folder, idle 0. Build: Amarchy 8535ce7f, amarchy-flat a7f57fa0, amarchy-gui 6ee56f9b (md5 equal to build/amiga; main since b193312c changed only rig tools and evidence). Shots in ~/Desktop/upterm-shots (s57r3-*.ppm). No product edits.

| ID | Verdict | Measured |
|----|---------|----------|
| RL3 | PASS | tokyo-night backdrop 0e0e14 (142377 px) = GR_BACKGROUND darker tone = bar colour; window surface 1a1b26. After pick vantablack: backdrop 222222 (143713 px), the black-ground lift. Backdrop follows theme. Caveat: flat prefs were off at first read. |
| RL15 | PASS (caveat) | flat on + pick vantablack (47 s), flat off + pick vantablack (42 s), pick white: after each, windows only Amarchy bar + root, requester list empty. Caveat: gap between flat on and off was about 42 s, not within 5 s. |
| RL8 dim text | PASS | tokyo-night: menu bg 1a1b26, dim 545a77 (264 px), contrast 260 (floor 220). white: bg ffffff, dim 676767, contrast 231. |
| RL8 pointer | PASS on bytes, pixels NOT MEASURABLE | Screen grab excludes the sprite. ENV:Sys/pointer.prefs: 41 PNTR chunks, depth 2, colours [a9b1d6, 1a1b26, 7aa2f7] = tokyo-night text, surface, accent; colour-3 pixels 0. |
| RL20 menu row | PASS | selected row fill 7aa2f7 (26,37)-(345,58); all four corners run 1a1b26, 31354d, 6783c6, 31354d before full fill: blended, mirrored. |
| RL20 welcome Close | PASS | Corner rows show blend pixels (3x3 corner has intermediate colours 333344/414868/858baa) at all four corners of the filled 7aa2f7 button, x 40-107, y 171-190 (window coords). |
| RL20 welcome Open Help | PASS | Outline button x 123-~292, same y; all four corners show intermediate pixels, no square corner. |

amarchy_richaa_rig.py stub not completed; reads done by hand from the grabs.

State changes: deleted ENV:Amarchy/prefs and ENVARC:Amarchy/prefs to force the Welcome window (welcome-shown line gone); closed it via Close. Theme back to tokyo-night. Locale Preferences and stale DiskCopy windows cancelled. Rig left up; no window of mine open.
