---
date: 2026-10-27
session: vtcon-a1 (Amarchy only)
branch: prefs-inline (worktree .claude/worktrees/prefs-a1)
ledger: thoughts/spot/todos.md (main checkout) rows AMARCHY-GUI-WARM, AMARCHY-LAUNCH-FIELD7
---
# Handoff: drawn menu warm server (11 of 12) and Install > Packages (3 of 8)

## AMARCHY-GUI-WARM, plan thoughts/shared/plans/2026-10-27-amarchy-gui-warm.md
- G1-G11 done: 4dfa3c1c (server), 11d4947d (rig --drawn timing, font/icons marks), a9525fb5 (ticks).
- Rig 7: warm and cold both median 140 ms; G10 exemption written (font 20-40 ms, icons ~0 ms).
- Open: G12 A1200. Deploy Amarchy and amarchy-gui only to UP-Term:bin (back up first; do not touch UHCBIN or
  UHC state until up-uhc 1.4 lands). SetEnv AMARCHY_TIMING 1, `ami.py key 40 40` (hotkey), `key 45 0` (Escape),
  read RAM:amarchy.timing, then unset the variable and delete the file. Fill Results.

## AMARCHY-LAUNCH-FIELD7, plan thoughts/shared/plans/2026-10-27-amarchy-packages-submenu.md
- P1-P3 done in a9525fb5: amarchy/pkgs_core.{c,h} (pk_parse, pk_row_path, pk_path_launch), tests/test_pkgs.c,
  ix_add_pkgs + IX_KIND_PKG + IX_PKGS_NODE "install.packages" + IX_PKG_NONE in index_core, keymap row and
  dist/menu-icons entry, test_keymap install_packages_lists_the_package_rows.
- Next P4: DOS loader (SystemTags `upkg list-installed --tsv` or `upkg --tsv` into a T: file, read, delete, pk_parse,
  ix_add_pkgs last) beside gt_dos_load_rows in menu.c (~:186, :341) and gui.c (~:661, :1210). Add new files to
  the Amarchy, amarchy-menu (Makefile:734) and amarchy-gui source lists.
- P5: extract the seg_stack start from act.c srv_start_proc (act.c:158-166, config/segstack.h:42) into one helper;
  `pkg:` branch in lf_open_index (leaf.c:312) uses it (IX_PKG_NONE row does nothing).
- P6: confirm `list-installed --tsv` on upterm-ports feat/uhc-repo carries field 7.
- P7 rig 7 reachability both styles with a sentinel; P8 A1200 after up-uhc 1.4.

## Gotchas
- amarchy-menu needs UI_HOTKEY_RIGHT (unmerged upterm-ports feat/ui-hotkey-right): build with
  UPKGUI_SYS=/Users/spot/Code/UP-Term/upterm-ports-right/build/sysroot/UP-Term.
- Every new [menu] node needs a dist/menu-icons entry or test_keymap fails ("no icon for menu node").
- Drawn menu closes on a second hotkey press, not a typed Ctrl-C.
- Leave alone: tools/rig/rig.py modification in the main checkout, gtheme-keymap-user research, menusrv probes.

## Update 2026-10-27 (late)
- P7 first run on rig 7: PASS in both styles (sentinel `Loaded as command: UP-Term:bin/nano`, stack 262144), but a
  detached start gives nano NIL: and no window. Owner chose: package rows start in an UP-Term window like run: rows.
  Done in prefs-inline f44cf618 (run_in_uterm; vsh now gets its own cookie stack via pl_stack). Built:
  amarchy-menu md5 e43ab89e..., amarchy-gui md5 42549511... (menu built with UPKGUI_SYS=<scratchpad>/sys/UP-Term,
  the Makefile default sysroot is stale).
- P7 DONE (run 3, rig 7, owner allowed the up-nano reinstall from the local feed): text and graphical PASS, nano
  opens in its own UP-Term window, vsh and nano stacks 262144. Plan ticked (7 of 8, P8 waits for up-uhc 1.4).
- Phase J job view: owner scope 2026-10-27 (output-only, MT_OK/MT_FAIL roles, both styles); plan
  thoughts/shared/plans/2026-10-27-amarchy-job-view.md (0 of 11 at writing). RJ.6 (job_core) and RJ.4 (theme roles,
  branch job-theme-roles in worktree .claude/worktrees/jobtheme) were given to sonnet agents; check the plan ticks.
- Menu root has 9-10 groups, 8 visible (MN_OSD_MAX_ENTRIES 8); Install is off-screen: design question for owner.
