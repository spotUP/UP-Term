# small-todos-burn (s33, host-only, read-only triage)

## Task 1: s31 ENV-AMARCHY-DRAWER (ledger line 3750): NO CODE CHANGE, no commit
Every Amarchy prefs writer already makes its drawer before writing:
- prefs_save_at (prefs/prefs_core.c:830) calls dir(p,t) before each target; Amarchy's dir = prefs_dos_dir (prefs/prefs_dos.c:63): CreateDir then Lock, for ENV:Amarchy and ENVARC:Amarchy. Used by amconf_save_with, amconf_migrate_with, leaf, gthemes.
- Other writers create inline: clipwatch.c:77 (ENV:Amarchy), tile.c:150, desk_dos.c:941/1129, icon_gfx.c:326, index.c:347, flat.c:1218, icons.c make_dirs.
- Installer already creates both: dist/install.dos:513 `MakeDir >NIL: ENVARC:Amarchy ENV:Amarchy`.
- The string "cannot create the temporary file" does not exist anywhere in the repo (grep, all of amarchy/, prefs/, dist/). Upprefs says "Cannot create <drawer>" / "<path>: write failed".
- Existing host coverage: tests/test_amconf.c:525-530 (note_dir proves dir() is called for both Amarchy targets before the write).
Verdict: no gap found host-side; I could not name the failing writer, so I did not invent a fix. Needs the exact on-screen text + which program (Prefs or Amarchy menu) from the rig; if it recurs on a fresh 3.2, probe CreateDir("ENVARC:Amarchy") IoErr there. Row is likely STALE or from a path outside this repo's writers.

## Task 2: triage, open rows lines 3250-3760 (18 rows)
Verdict DONE = code/commit exists on main; the ledger row stays open only for rig/owner verification where noted.
| Row | Line | Verdict | Proof |
|---|---|---|---|
| THEME-ENTER-BACK-ONE | 3255 | OPEN | no commit matches |
| FLAT-31-GEOMETRY | 3258 | DONE (research) | 4a61cb59 research note; the 3.1 route itself not found |
| FLAT-31-PALETTE-HANG | 3264 | OPEN | OS-side verdict, no fix commit |
| AMARCHY-THEME-EDITOR | 3290 | OPEN | gated, no commit |
| FLAT-C7 depth gadget | 3297 | DONE (code) | a22ea52e |
| FLAT-C8 frameiclass | 3298 | DONE (code) | 69621727, 077c8684 (3.1 deferred) |
| FLAT-ON-SWITCH | 3305 | STALE | superseded by 295ef667 desktop theme default + 61760273/d94a609c apply at start |
| FLAT-OUTLINE | 3341 | STALE | aa60f51d reverted by 23a7edf8, replaced by FLAT-OUTER-EDGE b1b6d940 |
| AM-WBRESET-RECUR | 3342 | OPEN | related fixes 1fb65b1a/fe80eec4, recurrence not confirmed fixed |
| AM-THEME-BOOT | 3343 | DONE (code) | cf0d408e, 31ce61a5 apply palette at start |
| FLAT-OUTER-EDGE | 3344 | DONE (code) | b1b6d940 |
| THEME-TEXT-GREY | 3354 | DONE | 5e55c7fe |
| FLAT-BUTTON-FILL | 3355 | DONE (code) | 4ef708a6, 614b0954 (rig verification pending) |
| ROUND-CORNERS | 3380 | OPEN | deferred by owner |
| AMARCHY-31-PALUSE-REBOOT | 3382 | OPEN | OS-side, no fix commit |
| WBRESET-EXT | 3389 | DONE | 439ccceb probe, 1fb65b1a fix |
| FLAT-SCROLLER | 3390/3399 | DONE (code) | 3b94169c, d3a5346c (rig-unverified) |
| FLAT-PENS-NOT-LIVE | 3394 | DONE (research)/OPEN | 603edf8b cause note; 295ef667 turned desktop-theme on by default, rig recheck needed |
Counts: DONE 9 (incl. 2 research-only), STALE 2, OPEN 7 (3 of them wait on a rig recheck even where code landed).
