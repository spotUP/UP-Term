## RMB screen bar repro

Rig 68, purpose "s69 session: RMB screen bar garbage repro". Workbench screen 640x256 on uaegfx via a hand-made ScreenMode prefs
(0x50041303, 640x256, depth 4 requested) -- the screen came up as depth=24 (uitree "S 0 640x256 depth=24"), NOT depth 4. No DCTelnet on
rig 68; app window = an UP-Term NewShell window "probe" (CON:300/60/300/120). amarchy-flat = kit binary e7dafba8dd50
(from ../vtcon-dopus-env-0c/build/amiga/amarchy-flat, verified by hash of UP-Term:bin/amarchy-flat read back from the rig). amarchy-menu/gui/Amarchy = local main build.
Method: scratchpad rmb.py: wasabi mouse move into the window, right button down (MOUSE action 2, button 1), pixel grab at 1.0 s and 2.5 s while held,
top 28 grab rows compared with the idle grab; uitree/menus read while held; button up; whole-screen diff vs idle.

| step | config | held: top-28-row pixels changed | colours in top 28 rows (idle/held) | verdict |
|---|---|---|---|---|
| 1 | flat e7da + amarchy-menu (menu-style=text), SolidWin not started; DOpus5 and gui processes still resident (boot) | 862 | 7 / 4 | drawn correctly (menu titles UP-Term/Edit/View/Settings/Help listed by MENUS) |
| 2 | + SolidWin 1.12 (C:SolidWin, process running) | 862 | 7 / 4 | same as step 1 |
| 3 | + menu-style=graphical (amarchy-gui style) with DOpus5 running | 862 | 7 / 4 | same; after release 23-42 px differ from idle (clock/title), screen recovers |

Result: NO garbage at any step on this screen. Not reproduced. Missing vs rig 1: screen is depth 24 not depth 4 (uaegfx ignored depth 4 from the prefs),
and the window is an UP-Term window, not DCTelnet. A depth-4 uaegfx public screen needs an app that opens it (DCTelnet or a test program).
Caveat: verdict is from pixel statistics (few colours, same palette subset), no image was viewed.
Side effect: one ami.py call against rig 1 (read-only exec) triggered rig.py ensure_current, which refreshed rig 1's VTC: with this checkout's
build (amarchy-flat, upprefs, vtcon-handler, 00:1x). Wrapper in scratchpad (amin.py) now disables that gate for rig 68.
