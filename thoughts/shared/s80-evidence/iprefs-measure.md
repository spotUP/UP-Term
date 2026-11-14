# s80 IPREFS theme switch: measurement and option A status

## Step 1, rig measurement: NOT DONE
The worktree had no build/amiga, so a rig needed `make amiga` first. All builds go through buildq (owner rule); the queue was 4-10 jobs
deep for the whole session (other sessions' selecta/tsc/test builds), so no rig was started. Nothing about `reset` /
desk_dos_wb_closes per apply, or a font reset on switch 2, is measured. Counters to read once a rig runs (published by lf_gt_select
after each pick): PATINA_WBCLOSES (+1 per apply that closed windows for a reset), PATINA_DESKMASK, plus the new PATINA_DEFERRED.
Existing driver: tools/rig/patina_desk_rig.py pick() (menu path, Keep requester clicked), needs rig `--os 32` per its header, DOpus via `rig.py dopus`.

## Option A: built ahead of the measurement (premise still unverified)
Code reading only (the premise risk from iprefs-theme-switch.md stands: s75 saw the requester on switch 2, which a pure pen-map
change on switch 1 would not explain; a font write is the other trigger, and option A also defers the font).
- patina/desk_core.c desk_defer_map(): pure. Foreign windows and a reset needed -> returns the pen map in force (0 stock, 1 flat) to write
  colours for, or DESK_DEFER_SKIP when the live pens match neither map. Otherwise DESK_DEFER_NONE.
- patina/desk_dos.c dk_apply: after closing our windows, counts non-backdrop windows on the Workbench screen (dk_foreign_windows);
  when deferring: no reset wait, font dropped from the mask, palette written for the live map, our windows reopened,
  desk_dos_deferred++ and sentinel PATINA_DEFERRED (the one status notice; no modal).
- tests/test_desk.c test_desk_defer_map (ONLY=desk).
Status of checks: host test and `make amiga` queued in buildq, see the report that carries this file for results.
Gap: desk_dos.c start path (line ~1790, dk_flat_pens ~907) has the same reset+close pattern and is NOT changed.

## Rig measurement
Rig 6, `--os 32`, DOpus 5.104 (`rig.py dopus`), Patina from this branch (22d64b8b build, `patina_gui_rig --setup` with patina-menu left out of
SETUP_NAMES: that file was stale/absent and the drawn menu is patina-gui; `wbpens`/`dripens` copied from the main checkout, same source).
Driver: scratch script around `patina_desk_rig.pick()`, desktop-theme = on, flat off, fresh Patina process (counters from 0), 12 s settle per pick.
The "Keep?" requester click failed on every pick (`no window titled 'Patina desktop theme'`): no Keep requester appeared at all.
DrawInfo pens read with dripens before/after; the Workbench screen address never changed.

| pick | situation | PATINA_DESKS | DESKMASK | WBCLOSES | DEFERRED | pens changed | screen reset | requester after |
|---|---|---|---|---|---|---|---|---|
| none1 tokyo-night -> vantablack | no Shell; DOpus lister + button windows on screen | 1 | 6 | 1 | 1 | no | no | none |
| none2 -> white | same | 2 | 6 | 2 | 2 | no | no | none |
| shell1 -> vantablack | 2 Shells + DOpus lister | 1 | 6 | 1 | 1 | no | no | IPrefs "System Request" Retry/Cancel (see note) |
| shell2 -> white | same | 2 | 6 | 2 | 2 | no | no | same requester, still there |
| shell3 | not run: menu timed out (rig left with the stuck requester) | - | - | - | - | - | - | - |

Answers:
1. Pen-map reset on each switch: NO as observed. Pens identical (`0 1 2 2 253 3 0 0 2 2 0 253`) before and after every pick, screen address constant. The premise is
   not shown; the pens never changed because the hook deferred (DEFERRED +1 each pick), so whether an undeferred apply resets each time is not measured here.
2. Counters: table above. DESKMASK 6 (not 15) on every pick, so the palette and font bits were dropped.
3. Requester: with Shells open, an IPrefs System Request (Retry|Cancel, no label text readable) stood on the Workbench screen. It was already listed
   in the window list right after the Patina restart, BEFORE the first pick of that run (the Patina start path, desk_dos.c ~1790, is the unchanged
   reset+close path), so it cannot be attributed to the picks. Not seen in the no-Shell runs.
4. No foreign window: NOT TESTED. Under DOpus the Workbench screen always carries a lister (and a button-bar window), which dk_foreign_windows counts, so the
   hook deferred (DEFERRED 1 and 2) in the "none" runs: the normal full reset did not run. A true no-window run needs a rig started with `--no-dopus`.

Verdict: INCONCLUSIVE, with one defect: desk_defer_map defers whenever any non-backdrop window stands on the screen, including DOpus listers, so on a DOpus
Workbench the full reset/palette path never runs (mask 6, pens unchanged) even with no user window open. Whether Patina's own windows ("Patina bar") are
excluded was not isolated. Not measured: picks on a no-DOpus Workbench, the undeferred baseline, the system-request source.

## Rig measurement (no DOpus)
Rig 6, `--os 32 --no-dopus --fresh`, Patina from this branch (build/amiga, `patina_gui_rig --setup`), prefs: desktop-theme on, flat off, welcome-shown on
(the Welcome window is a separate patina-gui window and would count as foreign), fresh Patina process (counters from 0), Keep clicked, 10 s settle.
Driver: scratch script around `patina_desk_rig.pick()`. Earlier attempts this session were contaminated (a leftover Welcome window, leftover
Shell and IPrefs requester, and a concurrent second driver) and are discarded; the table is the last clean run on a freshly reset rig.
Windows on the Workbench screen before the first pick (read via read_ui): `Patina bar` and the `Workbench` backdrop only.

| pick | situation | DESKS | DESKMASK | WBCLOSES | DEFERRED | pens changed | screen reset | IPrefs System Request |
|---|---|---|---|---|---|---|---|---|
| p1 tokyo-night -> vantablack | no Shell, no DOpus | 1 | 6 | 1 | 1 | no | no | none |
| p2 -> white | same | 2 | 6 | 2 | 2 | no | no | none |
| p3 -> vantablack | one Shell (MeasureShell) open | 3 | 6 | 3 | 3 | no | no | YES, Retry|Cancel, appeared after this pick |

Pens `0 1 2 2 253 3 0 0 2 2 0 253` and font.prefs length (518) identical after every pick; screen address constant.

Answers:
1. No foreign user window: the pick does NOT take the undeferred path. DEFERRED +1 on every pick, DESKMASK 6 (not 15), no pen/palette/font change,
   no reset. So the deferral trigger is not DOpus: with only `Patina bar` + the Workbench backdrop on the screen, dk_foreign_windows still reports
   a foreign window (or the defer decision is made on another input). Suspect: Patina's own bar window (or the backdrop) is not excluded.
2. With one Shell: DEFERRED +1 as in the no-Shell case (no difference), and an IPrefs System Request (Retry|Cancel) appeared after the pick. It was
   absent after p1 and p2, so with a Shell open the requester is attributable to the pick (it stands on the Workbench screen, title "System Request").
   Open question for the fix: the deferred path still wrote something that made IPrefs ask to close the Shell (palette file written for the live map?).

Verdict: FAILS. Option A's defer is taken even with no foreign windows, so the normal reset/palette/font path never runs on this rig, and the
requester with a Shell open is not avoided. Next: make dk_foreign_windows ignore Patina's own windows (check by window title/owner) and re-measure.
Not measured: the undeferred baseline (never reached).

## Re-measure after 9b27ae98
Result: FAIL for case 1 (no foreign window still defers), case 2 (one Shell defers) as expected, but the IPrefs requester still appears.
Build: build/amiga/Patina was STALE (13:13, before win_core.c 13:23); rebuilt with `~/.claude/bin/buildq --purpose "s80 vtcon iprefs: rig build" -- make amiga`
(Patina 13:27, patina-gui 13:28) before measuring. Rig 6: `UPTERM_RIG=6 tools/rig/rig.py start --os 32 --no-dopus --fresh --allow-stale-base "..." --purpose "s80 vtcon session: IPREFS no-dopus measure"`,
then `patina_gui_rig.py --setup`. Same driver as before (scratch copy of the earlier one; prefs desktop-theme on, flat off, welcome-shown on; Keep click fails
with "no window titled" as before, no Keep requester), 12 s settle per pick. Rig stopped afterwards.

| case | pick | windows before | DESKS | DESKMASK | WBCLOSES | DEFERRED | pens changed | IPrefs System Request |
|---|---|---|---|---|---|---|---|---|
| 1 no foreign window | tokyo-night -> vantablack | `Patina bar`, backdrop | 1 | 6 | 1 | 1 | no | none |
| 1 | -> white | same | 2 | 6 | 2 | 2 | no | none |
| 2 one Shell | -> vantablack | `AmigaShell`, `Patina bar` | 1 | 6 | 1 | 1 | no | YES (Retry/Cancel, after the pick) |
| 2 | -> white | same | 2 | 6 | 2 | 2 | no | YES (still/again) |

Pens `0 1 2 2 253 3 0 0 2 2 0 253` and screen address unchanged throughout. Case 2 run died at pick 3 (theme list timeout, stuck requester); not needed.
Case 1: FAIL (expected DESKMASK 15, DEFERRED 0; got 6 / +1 per pick). Case 2: defers as expected, but the requester still appears, so deferral does not avoid it.
Observation for the next fix: dk_foreign_windows runs AFTER dk_windows_close() (desk_dos.c ~505-507), so the bar is already closed when counting; the
title exclusion cannot be what keeps the count above 0. Some other window on the Workbench screen at that moment (not "Patina"/"Patina bar"/backdrop;
e.g. a menu/prefs/untitled window, or a window the bar close leaves behind) is counted. Identify it by logging title+flags of each counted window.
