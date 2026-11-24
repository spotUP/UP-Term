# DOPUS-ARROW-SPACING (S69)

Measured with the test canvas (flat_glyph_draw into a box, bbox of glyph pen; margins in px, box = Intuition gadget size):

| arrow (box)        | before L R T B | after |
|--------------------|----------------|-------|
| left/right 16x10   | 6 6 1 2        | unchanged |
| up 18x11 (side 1)  | 5 6 5 2        | 5 6 3 4 |
| down 18x11 (side 1)| 5 6 2 5        | 5 6 3 4 |
| up 18x11 (side 2)  | 2 3 5 2        | centred |

Root cause (algorithm level): left/right were centred in their box, up sat on the bottom and down on the top of theirs
(S31-ARROW-GAP), so margins differed by orientation (1-2 px vs 2-5 px). Fix: one rule in g_arrow, every arrow centred on both
axes, odd leftover pixel to the trailing side. Test: test_arrow_spacing (margins within 1 px, 3 box sizes, side 1 and 2).
Old S31 gap check removed (it asserted the hugging layout). Test fails on old code (11 FAIL), passes on new; flat-budget PASS.
NOT rig-measured (host canvas only); the real hires box heights differ from the test boxes.
