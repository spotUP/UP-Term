---
date: 2026-10-27
session: vtcon-a1 (Amarchy only)
branch: prefs-inline
worktree: .claude/worktrees/prefs-a1
---

# Amarchy prefs-inline, session 7

## State at session end

- Plan thoughts/shared/plans/2026-10-26-amarchy-prefs-inline.md: 18 of 19 done. Open: R2 (owner test on the A1200).
- R1 passed 7 of 7 rows on rig 7 (driver fixes 77415a6a, e20a4a2a, a82bc826). Evidence in
  thoughts/shared/research/2026-10-27-prefs-r1-run.md (Re-run 2, Re-run 3).
- M3-followup done and rig-checked (639e4b11, 02cee8b0, 93be7dc8). A1200 still runs 564d9dc3 (no long-label fix).
  Its rig check (row m3-right in tools/rig/amarchy_prefs_rig.py) was running in an agent on rig 7 at session end;
  its result goes under "M3 Right check" in the R1 research file. Tick plan line 158 from that section.
- R2 deploy done 2026-10-27 from build 564d9dc3, every file backed up as <path>.bak-20261027 (list under R2 in
  the plan). Handler sources equal main's. 12 unrelated files still DIFF (vsh, sh, UP-Term-Prefs, sz, rz, hl, mdv,
  upgetty, uptelnet, UPConsole, up-console.device).
- Text-menu Right needs UI_HOTKEY_RIGHT in libupkgui: thoughts/spot/todos.md, section UPKGUI-HOTKEY-RIGHT
  (UP-Term session).
- The drawn-menu rows of the R1 driver still wait for the Amarchy reload with a fixed sleep(3).

## Owner test for R2 (A1200, after a reboot for the new handlers)

1. Reboot, open UP-Term, start Amarchy.
2. Left Amiga + Space: the drawn menu opens.
3. Settings > Cursor blinks: the cursor starts or stops blinking.
4. Style > Font: the font requester opens on the menu's screen, not on Workbench.
5. Right on a leaf row does nothing; Right on a group enters it.
6. A long Font row keeps its value on screen.
7. Style > Amarchy theme, pick one, reopen the menu: the pick is kept.

Pass on all 7: tick R2. Then the roadmap (thoughts/shared/plans/2026-10-26-amarchy-roadmap.md) starts.

## Note on AMARCHY_FIT_<label> (93be7dc8)

put_keep in amarchy/gui.c sets ENV:AMARCHY_FIT_<label> for every settings row it draws. This was checked against
the existing sentinels: AMARCHY_ROWS, AMARCHY_DRAWS, AMARCHY_BLITS, AMARCHY_BEAM_WAITS and AMARCHY_AA_GLYPHS are
all set unconditionally with GVF_GLOBAL_ONLY (ENV: only, RAM, never ENVARC:). AMARCHY_FIT follows the same pattern,
so no gate was added. It writes only when the menu redraws, not every frame.

## Update 2026-10-27 (later, same session)
- R2 ticked: owner test passes steps 1-5, 7, 8 and Esc. Step 6 failed only in terminal Amarchy (no Right there yet).
  The prefs-inline plan is complete (19 of 19). The font requester guru is closed as a one-off (research file runs 2 and 3).
- The A1200 runs the HEAD 93be7dc8 debug build of amarchy-gui; the old binary is kept as amarchy-gui.bak-guru-20261027.
- Roadmap now has Phase J (shared job view, before AMARCHY-UHC), Phase L (logo, three font sizes, About; after
  AMARCHY-UHC, before P1) and Phase X (toggle rows, add-launcher wizard, reminders, disk speed test). Decisions 13-19.
- Open, in order: AMARCHY-TEXT-SPEED (terminal Amarchy takes seconds to open; a measurement agent writes
  thoughts/shared/research/2026-10-27-amarchy-text-speed.md), AMARCHY-TEXT-RIGHT, AMARCHY-MENU-STALE-PORT, then Phase J.
