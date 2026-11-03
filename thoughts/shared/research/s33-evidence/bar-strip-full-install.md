# BAR-STRIP-RESERVE on a full install (rig 61, AmigaOS 3.2 --os 32 --max, 2026-11-03)

Setup: rig 61 fresh, `installer_rig.py ONLY fresh-all` with INSTALLER_KEEP=1 -> [PASS], kit stays installed, rebooted by the case.
Kit: build/dist/up-term (20:56). Installed UP-Term:bin/Amarchy md5 4d98af93180fe3906a297ab906c28271 = build/dist/up-term/Files/Amarchy;
build/amiga/Amarchy (20:47) md5 aad585f9... differs (same size 295988; dist is a later rebuild; no strut change since 664f90db).
DOpus 5 not installed on rig 61 (rig.py WARN), so no lister was tested. Screen 640x256, bar window "Amarchy bar" 0,246 640x10: strip top 246.

## State after boot
BAR STATUS: bar=1 strut_opens=6 strut_clamped=1 strut_zoom=0. Patch is installed and live.
Windows after boot (uitree): f1host 0,20 600x200 (ends 220), drawer Empty-12MB-FFS 96,109 462x103 (212), drawer Work 159,55 400x100 (155),
WB root 0,11 640x235 (ends 246). None in the strip on this rig: the boot drawers are short.

## Opens while the bar is up: all clamped (PASS)
CON:0/0/640/256 -> 0,11 640x235 (246). CON:20/150/300/100 -> 20,146 300x100 (246). XCON:0/30/640/226 -> 0,20 640x226 (246).
XCON:0/0/640/256 -> 0,11 640x235 (246). Counters opens 6->11, clamped 1->5.

## Window open BEFORE the bar comes up: NOT clamped (FAIL, the cause)
BAR OFF (bar window gone, root 640x245) -> `NewShell "CON:20/40/600/216/Pre2"` opens 20,40 600x216 (ends 256), opens unchanged 11 (hook passes, strip=0)
-> BAR ON -> bar back at 246, root shrinks to 235, Pre2 STAYS 20,40 600x216: rows 246..255 lie behind the bar. Counters unchanged.

## Cause
amarchy/strut_patch.c:343-370 strut_publish() reflows only DESKTOP windows (bar_is_desktop: backdrop or full-width WBENCHWINDOW,
filter at :355-358). Every bordered window already open when the bar starts keeps its box. The open hooks (tags_hook :213,
plain_hook :246) act only while g_pub->strip > 0, and strip is set only in strut_publish, called from bar_open (amarchy/bar.c:517).
On a full install the bar opens late: Amarchy is Run from S:User-Startup before LoadWB and holds all its windows until the
Workbench is in its final mode (amarchy/amarchy.c:613-624, W43 g_wb_hold / wb_gate). Everything Workbench opens at boot in that gap
(snapshotted drawers, WBStartup / boot Shells, UPDemo XCON, DOpus listers when DOpus is the desktop) opens unclamped and is never fixed.
Same path: any BAR OFF -> ON (theme reload, WB reset via wbr_patch -> bar_close/bar_open) leaves windows opened while the bar was down.
Which windows overlapped on rig 96 itself: UNVERIFIED (rig 96 not touched); on rig 61 the boot drawers happened to end above 246.
Not covered either (not measured here): later ChangeWindowBox/SizeWindow/ZipWindow by apps or the size gadget (only opens are patched).

## Proposed fix (root cause level: strut_publish)
In strut_publish, after the desktop pass, walk scr->FirstWindow (under LockIBase, skip Amarchy's own and desktop windows) and for each
window build a win_strut_req from its live box + Flags, run win_strut_class + win_strut_clamp (same classifier as the open hook, so
popups/fullscreen terminals/the bar stay PASS), and apply the result with ChangeWindowBox (move up first, shrink only below minh logic
already in win_strut_clamp). Count them in g_pub->clamped (new sentinel AMARCHY_STRUT_REFLOW optional). Do not restore them in
strut_clear (a moved window staying put is harmless; desktop windows keep their restore). Optionally also publish the strip
(g_pub->strip/sw/sh/scr) as soon as the bar height is known at W43 hold time, so boot opens are clamped by the hook directly.

## Regression test idea
Host: extract the per-window decision into win_core (e.g. win_strut_reflow(list of boxes+flags, screen) -> boxes) and test bordered
window 20,40 600x216 -> 20,30 600x216, popup unchanged, bar unchanged, fullscreen terminal unchanged.
Rig row in tools/rig/amarchy_bar_rig.py "strut_prebar": BAR OFF, open CON:20/40/600/216/Pre2, BAR ON, assert Pre2 bottom <= strip top
and clamped counter +1; fails on current main (measured above: Pre2 ends 256).
