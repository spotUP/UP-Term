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

## Part 2 (2026-11-01): current origin/main (2c684f70) deployed to the A1200, measured read-only
- Deployed UP-Term:bin Amarchy, amarchy-gui, amarchy-menu, amarchy-index, amarchy-flat, amarchy-icons (md5 on A1200 == build). Backups beside them: *.bak-s28bar2. Old md5: Amarchy 1e8ee28a, gui 9bce11e9, menu 32619899, index c4dd5372 (same as new), flat bf3f320a, icons 7baa92c2. Restarted: AMARCHY QUIT via ARexx, then Run >NIL: UP-Term:bin/Amarchy as User-Startup does.
- Screen: Workbench 680x270, id 0x29000, DClip = TxtOScan 0,0..679,269 (full), ViewPort dx=0 dy=0, DWidth 680 DHeight 270. NO clip of the bottom rows. Screen font helvetica.font/13.
- Bar window 'Amarchy bar' at 0,255 680x15: flush with the screen bottom (255+15 = 270). Height 15 = screen font YSize 13 + 2 (bar.c:479).
- Bar window RastPort font is topaz.font/8 (probe), NOT the screen font. tw_text draws at rp->TxBaseline+1 in topaz 8: text occupies bar rows 1..8; rows 9..14 (6 rows) are plain MT_BG.
- Screenshot (wasabi grab, 680x270): rows 255..269 all bar background; text pixels only in rows 256..263; rows 264..269 solid (11,12,22). So the "gap under the bar" is 6 empty bar-coloured rows below 8 px text, not an unpainted strip.
- Also bar.c text_w() measures in g_scr->RastPort (helvetica 13) but tw_text draws in the window font (topaz 8): slot widths and drop order are computed for a different font than the one drawn. On the rig the screen font is topaz 8, so both agree and h = 10.
- Cause: window font != screen font. Fix direction: SetFont(g_win->RPort, g_scr->RastPort.Font) after tw_open in bar_open (bar-local), so height, measure and draw use one font. Not landed: no host test can reach this (SetFont on a real window); needs a rig run with a 13 px screen font (UpTerm font prefs) to prove.
