# BAR-STRIP-RESERVE rig evidence (rig 24, AmigaOS 3.2.2, 060, screen 640x256, bar top 246)

Design: thoughts/shared/research/2026-11-01-bar-strip-reserve-design.md. Owner answers: D1 leave snapshotted icons, D2 bar stays behind.

## Probe Z0 (stock, no patch): default alternate box of a sizable window with no WA_Zoom
struttest 10 11 300 200 sz        -> BOX 10 11 300 200, ZIP 10 11 60 40, ZIP 10 11 300 200
struttest 0 11 640 245 sz         -> BOX 0 11 640 245,  ZIP 0 11 60 40,  ZIP 0 11 640 245
Result: the default alternate box is 60x40 at the window's top-left. It reaches the strip only if the window itself did, and the open
clamp prevents that. Decision: no WA_Zoom injection (STRUT_INJECT_ZOOM 0); only a caller-given WA_Zoom is clamped.

## Probe I0 (icons after the root window is shortened)
UNVERIFIED. Verified only that the Workbench root window ends at the strip top (246) while the bar is up and at 256 after BAR OFF / quit.
Not probed: whether WB relayouts icons on NEWSIZE, and CleanUp placement (step 3, "new icon lands above the strip").

## Rig rows (tools/rig/amarchy_bar_rig.py, UPTERM_RIG=24), final run, 5 of 5
ok strut_open: box [100,96,200,150] (requested top 156), CLAMPED 0 -> 1, OPENS 0 -> 1
ok strut_zoom: WA_Zoom full screen; BOX and both ZIP boxes end <= 246 (heights 235), ZOOM 0 -> 1
ok strut_desk: Workbench root ends 246, AMARCHY_STRUT_DESK = 1
ok strut_off: BAR OFF -> box top 106 (reaches strip), CLAMPED unchanged 1, root ends 256; BAR ON -> box top 96, CLAMPED 1 -> 2, root ends 246
Step 6 (quit): AMARCHY_STRUT_INSTALLED 1 -> 0, open of the same box gives 106 again, root back to 256, no crash.
Step 4 (Shell, Stack 4096): struttest has its own 64 KB stack cookie, so this does not test a 4 KB caller. UNVERIFIED at 4096.

## Revert check (install line commented out, rebuilt, restaged)
FAIL strut_open: box [100,106,200,150], CLAMPED 9 -> 9
FAIL strut_zoom: ZIP box [0,11,640,245] (enters strip), ZOOM unchanged
FAIL strut_desk: Workbench ends 256
(start ok.) Steps 1-3 fail without the patch; they pass with it.

## Guard mutation results (host test, ONLY=win)

Each guard in win_strut_class removed in turn; the test must fail.
- exact bar box guard removed: FAIL (test_win.c:389, class 2 want 0).
- fullscreen terminal guard removed: FAIL (test_win.c:399, class 1 want 0).
- borderless popup guard removed: FAIL (test_win.c:393, class 2 want 0).
- bottom<=0 guard removed: no failure. Equivalent mutant: with no strip, win_strut returns 0 for every box and the exact-strip test (== bottom == 0) passes the same way. The guard is a fast path only.
- Not mutated: the bottom<=0 guard inside win_strut_clamp.

Final host runs: win 272 checks 0 failed; wbreset 75 checks 0 failed.
