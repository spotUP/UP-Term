# BAR-BOTTOM-GAP-A1200 (s28) - unresolved, no fix

Status: cause NOT found. No code fix landed. Budget spent.

## Rig 15 repro (A1200 prefs regenerated: screenmode 0x29000 depth 8, overscan 680x270, ViewPos 113,35)
- Needs `rig.py start --max --os 32 --overscan` (without --overscan the rig window crops to 640x512 and hides overscan).
- Workbench 680x270 depth 8 as on A1200. Bar window at top 260, 680x10 (grab space doubles rows), flush with screen bottom. NO gap.
- Temporary BAR GEOM probe (not kept): scr=680x270 vp=680x270 dx=0 dy=0 id=29000 nom=640x256 txt=0,0,679,269 std=0,0,681,269 vid=-40,-6,695,276 bar=0,260 680x10 fonty=8
- BAR TEXT on rig: everything fits at 680; net dropped (6@0+0). Drop order works on rig.

## A1200 (read-only window list, old Amarchy build)
- Workbench 680x270 depth 8, row_scale 2. Bar window left 0, top 510, w 680, h 30 (grab space) => screen rows top 255, height 15, ends at 270: flush, no gap in window geometry.
- Height 15 vs 10 on rig: A1200 bar is 5 rows taller -> different font/bar height than rig. Not explained.
- bar.c fills the whole window with MT_BG (bar.c:420), so a visible gap is not unpainted bar window.

## Hypotheses left
1. Visible area (Replay FPGA / monitor clip) is smaller than the 680x270 screen: bottom rows and right end clipped. Explains cut "Syste..." and "gap" together. Test: read A1200 BAR TEXT slots with the current build and compare to what is visible; ask owner what the monitor shows at screen edges.
2. A1200 font/bar height 15 differs from rig 10: check ENVARC:Sys/font.prefs, screen font YSize on A1200 (read-only).
3. Old Amarchy build on A1200 predates drop-order fix: deploy current build and recheck (needs owner OK, A1200 write).

Next: owner to confirm whether A1200 runs a current Amarchy build.
