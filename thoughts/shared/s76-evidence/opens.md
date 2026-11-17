# s76 open rows: UPKG-REMOVE-NOOP and IPREFS-REQ-ON-THEME-SWITCH

Rig 5 (OS 3.2, Kickstart 47.115), real upkg (opkg 0.9.0 + patches 0001..0061), base kit at Work:UP-Term.

## UPKG-REMOVE-NOOP
- Rig: `upkg install VTC:up-git.ipk` rc 0, status holds `Package: up-git` 1.0 (Status: install user installed).
  `upkg remove up-git` rc 0, "Removing up-git (1.0) from root... up-git removed.", status emptied. Repeat after
  `install --force-reinstall`: same. So remove works when up-git is installed.
- Second `upkg remove up-git` with nothing installed: stderr "up-git is not installed." then "No packages removed.", rc 5.
- Likely cause of the s75 no-op: up-git was not in status; the reason line is on stderr and only stdout was kept. Unproven:
  s75's exact state was not reproduced. A refusal ERROR (dependents) was not observed.
- Code: upterm-ports build/src/opkg/libopkg/solvers/internal/opkg_action.c, opkg_solver_remove (message ~line 152). The final
  "No packages removed." is shared by every skip path, so it never said why.
- Fix (not merged, not pushed): worktree /Users/spot/Code/UP-Term/upterm-ports-opkg-msg, branch opkg-remove-reason,
  commit 5fa49b1: patch 0062 makes the line "No packages removed: <reason>."; check/remove_reason.sh; recipe.mk note.
  recipe_lint ok. Host build and check run: see report (status at hand-back).

## IPREFS-REQ-ON-THEME-SWITCH
Not reproduced on rig 5 (no DOpus installed, budget used on row 2). Leads, unproven:
- s75 evidence (s75-evidence/dopus-toolbar-themeswitch.md): during `AMARCHY RUN 1` (gtheme:white) an IPrefs "System Request"
  (Retry|Cancel) stalled the commodity; requester text not recorded.
- Path: amarchy.c do_row KA_GTHEME -> gt_dos_select (gthemes_dos.c:217) -> desk_dos pens/palette write -> IPrefs Workbench reset.
- desk_dos.c:863 (RL15-IPREFS-SYSREQ) already documents this class: a pen-map change makes IPrefs call CloseWorkBench; with
  windows open it fails and the Retry/Cancel request stays up. The fix there closes only OUR windows (dk_windows_close).
  A DOpus lister/toolbar window on the Workbench screen is not ours, so CloseWorkBench can still fail for it.
  Probable cause, unverified. Proposed fix (not applied): before a pen-reset apply, close or iconify the bar and
  detect foreign windows on the WB screen (or skip the pen change for them), then wait for IPrefs idle.
- To reproduce: `rig.py dopus`, keymap rows gtheme:nord/white/gruvbox, Amarchy running, ARexx `AMARCHY RUN n`; read
  the requester text from the intuition window title/IDCMP via the rig tools. Reference: tools/rig/amarchy_dopus_toolbar_rig.py.
