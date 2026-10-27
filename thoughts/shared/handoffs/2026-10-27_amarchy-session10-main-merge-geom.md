---
date: 2026-10-27
session: Amarchy session 10
branches: prefs-inline ff66b1cd, pg8-text2 521cb5b1 (both pushed; pg8-text2 contains prefs-inline)
rigs: 7 and 3 only (8 = musicplayer-7f, 1/2/9/10 other sessions)
ledger: thoughts/spot/todos.md row AGENTS-S10 and its "S10 progress" / "S10 guirig" sub-entries
---

# Amarchy session 10 handoff

## Done
- keymap host suite on prefs-inline ba954450: 12183 checks, 0 failed.
- Both branches were 4 behind origin/main (fe7eda9f watchdog, 970905e0, 14d1c108 no-AmiTCP, bc6ecabf).
  prefs-inline merged main (926a5f73; rig.py install() wraps _install() in paths.long_run()), pg8-text2 merged
  prefs-inline (ecb282a6). Both pushed.
- Stuck window in rows_over_terminal: the old rig.py assigned AmiTCP:, ramlib asked "Please insert volume AmiTCP"
  (research/2026-10-27-s10-stuckwin.md). Gone with the main merge.
- 63eaac72: rows_no_terminal expects Style (27eda4b7 always-visible Terminal group); rows_over_terminal sets
  menu-in-terminal = graphical for its read and restores both stores; run_row keeps the table going past one
  aborted row (tests/test_menu_rig_row_abort.py, fails when reverted).
- origin/am-idx deleted (owner approved; its patches live on main). Local am-idx is checked out in another
  worktree, untouched.
- upkg-s10 (9045fcb) is STALE; use kit-main (see end of file).

## Rig results so far (first half; the geometry FAIL below was the helper, see next section)
- guirig rig 3, pg8-text2 231c66c9 (research/2026-10-27-s10-guirig.md): default FAIL on geometry, root window
  372x206 at 134,25 vs menu_geom.c (140,27,360,202), apps level wants 140,70,360,116; on a fresh boot also
  "amarchy-gui exited after Esc", later cases cascade. --pkg-all PASS except pkg_upgrades (KNOWN-UPKG: two up-pi
  rows, ran without the new upkg). --pkg-tags PASS 9.
- At handoff time two agents were running: geometry hunt (answer in research/2026-10-27-s10-geom.md) and menu rig
  --fresh on prefs-inline 63eaac72 rig 7 (research/2026-10-27-s10-menurig-r2.md). Read both files first; if a
  file is missing, the agent did not finish: re-brief from this handoff.

## Later in session 10 (after the first handoff)
- Geometry: the rig helper was wrong, not the product (research/2026-10-27-s10-geom-r2.md). Fixed on prefs-inline:
  bbf7bb29 (zero mg_screen, --dopus, --apps-rows, apps keeps the root width, rig reads the WORKBENCH and DOPUS.1
  ports), ff66b1cd (stale host test now checks rows over Workbench per level). guirig default geometry ok on rig 3.
- The "Please insert volume -C" requester the owner saw on rig 7 reads "AmiTCP". ixnet.library opened
  AmiTCP:libs/usergroup.library at every ixemul start; 14d1c108 removed the rig's AmiTCP: assign, so amarchy-menu
  and tmux hung on it (research/2026-10-27-s10-menurig-r3.md in prefs-a1). vtcon-05 fixed it: ixemul PR 5,
  build295 d1fa642 is the rig default, rig.py refuses the old ixnet (bead969b). Kit make dist still pending; vtcon-05
  messages when it lands.
- 4c815d82: the menu rig fails a row by name when a system requester is open.
- pg8-text2 merged prefs-inline (717c6931, 4e127ec4), pushed.
- Reruns started with the fixed ixnet: menu rig rig 7 --fresh (research/2026-10-27-s10-menurig-r4.md in
  pg8-text2), gui rig rig 3 --fresh (geom-r2.md, section "r4 rerun"). Missing file = agent did not finish.

## Late session 10 results
- gui rig r4 rig 3 (geom-r2.md "r4 rerun"): default 45 ok 0 FAIL (geometry passes). run_tmux: fs-uae
  "CPU reset PC=f800d0" after Apps > 3rd row Return, rig died at c.run('tmux ls'). --pkg-tags: FAIL "the Packages
  level has a Refresh row (rows: ['Browse','Search','Installed'])".
- 6c353c79 (pushed): open_apps_row(label) walks Apps rows by label for run_tmux/run_editor (the Apps level merges
  index apps with tab:/menuid:/slash: rows; counted Down presses hit the wrong row).
- 521cb5b1 (pushed): tests/test_keymap.c loaded-index test checks the 5 packages rows (research/2026-10-27-s10-pkgs-refresh.md).
- Packages 3-of-5 rows: NOT a stale keymap. rig.py ROOT is the running checkout, build/rig3/vtc/keymap has all 5 rows;
  mn_row_needs does not hide job: rows. Hunt running: research/2026-10-27-s10-pkgs-level-rows.md (pg8-text2).
- Rig 7 "-c:" volume requester (owner report): gone by the time it was checked; LIBS:ixnet.library is the fixed
  one, AmiTCP: unassigned, ami.requesters() []. Source unknown. menu rig r4 FAILs enter_slash / enter_th: theme
  requester shown, handler km-hits +0 (1 expected). Evidence: research/2026-10-27-s10-menurig-r4.md (pg8-text2).
- Agents at this point: menu rig r4 rig 7 (menurig-r4.md), run_tmux/run_editor r5 rig 3 (geom-r2.md "r5 run_tmux by
  label"), packages level hunt (host only). Missing section = agent did not finish.

## End of session 10 (pg8-text2)
- da26371f: ami.requesters() sees AssignWedge "Volume Request" (Retry|Assign|Deny|Cancel); test_rig_ami.py types
  through the keymap table again.
- 5c2d2576: close_vsh types `exit` only into an active window; open_host closes a same-title leftover first
  (tests/test_host_window_leak.py). No fallback close by window address yet.
- "-C" volume box (research/2026-10-27-s10-volume-c.md): owner AssignWedge, requester likely a VTC:vsh, path not
  captured. Likely cause: typed lines landed in leaked menu-host windows (fixed by 5c2d2576, unverified). If it
  recurs: fresh rig, snoop started before amarchy_menu_rig.py, pkg_text* rows one at a time.
- tmux reset (research/2026-10-27-s10-tmux-reset.md): only via amarchy-gui Apps > Tmux (leaf.c run_in_uterm,
  XCON + SystemTags vsh -c tmux). Plain `tmux new -d` / `tmux ls` fine. Section "r2 launcher probe" = rig 7 agent.
- PG.8 text-menu run on rig 4: research/2026-10-27-s10-pg8-text-r1.md. Missing section = agent did not finish.
- UHC: vtcon-05 says up-uhc 1.5 and U8 are done. Use UPTERM_PORTS=/Users/spot/Code/UP-Term/upterm-ports/.claude/worktrees/kit-main
  (5bfe71d; upkg-s10 is stale). Real UHC on a rig: up-uhc 1.5 ipk from an http feed under vsh, then
  setenv UPKG_UHC_DIR SYS:UHC/Packages. Next: guirig --pkg-tags/--pkg-all/--pkg-upgrades against real UHC (rig 4/7).
- Owner rule: always one implementing agent plus one debugging agent.

## Open, in order
1. Read menurig-r4.md and geom-r2.md "r4 rerun". Fix real FAILs at the root, with a regression test.
2. "ERR: rename into place failed" (wasabid put deletes the target, then renames over it). The target is not
   confirmed; suspects are ENV:up-term/up-term, ENV:Amarchy/prefs, ENVARC:Amarchy/apps, held open by Amarchy.
   wasabid source is in /Users/spot/Code/wasabi-eval/wasabi (upstream creep-ltx/wasabi), outside this repo: ask the
   owner before editing it.
3. pg8-text2 text-menu cases (Search, tags, rc 12/13, AMARCHY_ROWS) on rig 7; pass -> merge pg8-text2 into
   prefs-inline, push.
4. --pkg-upgrades with UPTERM_PORTS (see Done); warmsrv text-menu case (tools/rig/amarchy_warmsrv_rig.py); RJ.7
   B3/A3 cancel timing.
5. Tick PG.8 (plans/2026-10-27-amarchy-packages-group.md ~line 110) only when every item passes in both styles.
- Dropped: the "rig.py install fallback [WARN]" item. It is by design (rig.py:934-941). A worktree without
  its own build takes the main checkout's handler and says so; run make worktree-ready, then make amiga, to silence it.

## Gotchas
- tests/test_menu_rig_*.py are not wired into make test; run them by hand.
- Rig start can warn "N file(s) in VTC: differ from the build": run amarchy_gui_rig.py --setup (or stage) first.
- Verdicts live in the main checkout's build/rig<N>/amarchy/.
