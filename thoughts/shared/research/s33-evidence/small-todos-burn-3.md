# Small todos burn 3 (read-only triage, 2026-11-03)

Evidence: `git log --oneline -5000` dump grep + source grep. Not covered by burn 1/2. Ledger: thoughts/spot/todos.md.

| Ledger line | ID | Verdict | Evidence |
|---|---|---|---|
| 215 | NEXT KIT (rebuild ixemul build295) | STALE | Superseded by line 3788 `[x] R9` and 3154 `[x] IXEMUL-SIGRC-DEPLOY`: build295 libs built after HEAD 0926fd3 (ixemul-vtcon). |
| 1842 | TEXT-MENU-BEEP | STALE (done) | Line 1865 `[x] TEXT-MENU-BEEP` -- root cause fixed in 7f5ec51d (new XCON: process no longer moves the screen). Row 1842 is the duplicate open row. |
| 1997 | test_dist_manifest base-hands-paths fails on main | DONE | 82a65473 "test_dist_manifest: handed-paths test passes its own package rows"; test at tests/test_dist_manifest.py:193. (Test run not repeated here: unverified green.) |
| 998 | upterm-ports script/upkg-ui need ?, -h, --help | OPEN | vtcon suite commands got it (6d3ad47c, c1fb5680 for rig.py) but the row names the upterm-ports repo (script, upkg-ui); no evidence there. |
| 1578 | AMARCHY-MENU-RIG-FAILS step 1 (rig 6, app_index 27/27) | STALE | Line 1529: CLOSED 2026-10-25, main ad8aab8f, app_index 31/31 on rig 6 and rig 7. Steps 2/3 only apply on a new failure (1579, 1580). |
| 1579 | AMARCHY-MENU-RIG-FAILS step 2 (only if menu closes again) | STALE | Condition never recurred: 1529 CLOSED, 31/31 on rigs 6 and 7. |
| 1580 | AMARCHY-MENU-RIG-FAILS step 3 (rig 7 aga256 check) | STALE | Same: closed 2026-10-25; rig 7 passed with --max --fresh. |
| 2107 | Code follow-ups D13/D12/D9 | DONE (partial: v5.104 image, own-prefs U/E/X not found) | D13 joe launcher + D12 uterm-shell: 929a5bf8; D13 filetype launchers: a8108aff; D9 --no-dopus: d56b95c3. Remaining sub-items v5.104 rig image and own-prefs U/E/X: no commit found -> keep row OPEN, shrink to those two. |
| 2113 | Text menu scroll cue | OPEN | mg_more() exists (amarchy/menu_geom.c:189, used amarchy/gui.c:375, test tests/test_keymap.c:2177) for the drawn/GUI menu; no marker found in the text menu (menu_core.c, menu_tree.c grep empty). Add marker + test. |
| 40 | SHELL-TUTORIAL | RIG-ONLY | dist/tutorial + dist/tutorial-lessons/*.lesson on main; lessons 12/13/14 fixes 423bfd46, aa09ca3d. Remaining: tutorial_rig on a rig to get all 27 checks (step 3 gives up on rig 4, 9af97d6a WIP) and L-R owner run. |
| 54 | SYNCTERM-PORT | OPEN | No SyncTERM port in repo (git ls-files empty; only ANSI music borrowing, c21dc2ae). Real work. |
| 37 | cursor leak: inverted char in cursor on rig | OPEN | "not reproduced"; no commit; needs a repro on a rig. |
| 1084 | NEWSHELL-BEEP | OPEN | Cause unknown; no commit; needs the A1200 (VPN off). Related 7f5ec51d fixed only the XCON screen flash, not the NewShell beep (unverified). |
| 1086 | INSTALL-PROGRESS | OPEN | No download-percent / unpack-progress code found (grep for percent in amarchy/dist empty; no commit). |
| 1368 | IXEMUL-FGETWC | OPEN | No fgetwc commit in vtcon log; ixemul-vtcon grep not conclusive (shell glob error). Treat as open. |
| 795 | Duplicate upgetty + wasabid after boot | OPEN | Cause unknown; commits 88d26e3c/69656aa1 only handle upgetty in rigs. Needs A1200 `ps` check. |
| 1011 | Feed line -> v1.2-test2 + ignore_uid, rerun install of 7 on A1200 | OPEN | No commit mentioning ignore_uid / v1.2-test; A1200 run. |
| 886 | Upload make dist set to v1.2 --clobber | OPEN | Release upload needs owner OK (memory: release upload still asks). |
| 1316 | R3 --nofpu rig boot + soft-float ixemul check | RIG-ONLY | Rig run: boot `rig.py start --max` with --nofpu, check ixemul soft-float; no commit proves it. (Could not confirm the later outcome in this pass.) |

Counts: DONE 2 (2107 partial), STALE 5, RIG-ONLY 2, OPEN 10 = 19 rows.
Not covered by this pass: whether lines 2056/2075/2062 (upkg startup/WB UI) closed later; not examined.
