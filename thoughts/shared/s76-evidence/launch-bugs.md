---
title: Patina rich-UI launch bugs (Claude row, UP-Term without logo)
date: 2026-11-13
status: partial (bug 2 confirmed from source + rig config; bug 1 cause partly unverified)
---

# Launch bugs from patina-gui / patina-menu

## Rows involved (rig 7, ENVARC:up-term/keymap; same in dist/keymap)
    178 apps.upterm  = UP-Term  | open:uterm
    181 apps.claude  = Claude   | menuid:MENU_CLAUDE
    191 dopus.shell  = Open UP-Term here | open:uterm
(`search ENVARC:up-term/keymap claude` on rig 7 -> lines 181, 204, 219)

Both gui.c:1028/1867/2013 and menu.c:523 run rows through lf_run (patina/leaf.c).

## Bug 2: UP-Term from the rich UI has no ANSI logo -- CONFIRMED (source + rig)
- The logo is the vsh banner: dist/vshrc:205-209 `Type ENV:vsh/banner` only in an interactive vsh.
- Workbench icon SYS:System/UP-Term: default tool UP-Term:bin/vsh, tooltype WINDOW=XCON:... (tools/mkicon.py:3,17,145).
  So the icon starts vsh -> vshrc -> banner.
- open:uterm -> patina/leaf.c:691-697 open_uterm(): `system("NewShell \"XCON:0/20/640/300/UP-Term/CLOSE\"")`
  (LF_UTERM_SPEC patina/leaf.h:14). Same in patina/act.c:249-257 act_open_uterm (hotkey path).
  NewShell without FROM runs the AmigaDOS Shell with S:Shell-Startup (rig 7 file: Prompt, aliases, upkg alias; no vsh).
  No vsh -> no vshrc -> no banner, and no bash shell either.
- Not a rename regression: `git show 7d6bdb09^:amarchy/leaf.c` has the identical open_uterm (line 691) and LF_UTERM_SPEC.
- Fix (patina layer, one place): open_uterm/act_open_uterm must start vsh the way the icon does: reuse
  run_in_uterm's pattern (leaf.c:59-88: Open the XCON spec, SystemTags upassign_vsh() with SYS_Input=window,
  pl_stack) or NewShell <spec> FROM a script that runs upassign_vsh() (the handler's tab_spawner does exactly this,
  handler/vtcon_handler.c:7559-7585). Share one helper between leaf.c and act.c (they are already duplicates).

## Bug 1: Claude row opens a shell, Claude does not run -- cause partly UNVERIFIED
- Path: menuid:MENU_CLAUDE, no terminal present -> lf_plan_t (patina/leaf_core.c:45-48) -> LF_OPEN_THEN_REQUEST
  -> open_then_request (leaf.c:700-714): open_uterm() (the AmigaDOS Shell window of bug 2), poll
  upactive_terminal_present up to 5 s, then run_request -> upreq_send menuid MENU_CLAUDE -> handler
  vtcon_handler.c:7962-7965 tab_spawn(TAB_CLAUDE) -> tab_prog_script (7507-7530) writes T:UP-Term-claude
  ("FailAt..., Stack 32768, C:Claude, If WARN Ask...") and runs it in a NEW TAB.
- The window the owner sees is exactly bug 2's AmigaDOS Shell window: same root path.
- Rig 7 evidence: `list T:UP-Term-#?` -> empty; `type T:UP-Term-claude` -> "object not found" rc 10.
  tab_prog_script always leaves that file when it gets as far as spawning, so on this boot the
  MENU_CLAUDE request never reached tab_spawn (or the owner did not try it on this boot: UNVERIFIED).
  Likely: upreq_send went to no window / was refused (the 5 s poll, or self_in_mru selection), or the
  Claude tab opened behind / in another window. Not reproduced: needs a GUI click on a rig; budget stopped here.
- Even when it works, the design is wrong for the user: Claude lands in a second tab behind an AmigaDOS Shell tab.
- Not a rename regression: MENU_CLAUDE row from 16b75e36; rename diff (7d6bdb09) for tabprog.h/keymap_act.c/
  config/keymap.c only renames KA_AMARCHY->KA_PATINA and include paths. No amarchy string left in
  handler/, patina/, claude/, dist/install*.dos (grep -rIi).
- Fix recommendation (patina layer): with no terminal present, a menuid program row (MENU_CLAUDE, MENU_PI,
  MENU_DEMO, MENU_TUTORIAL, MENU_AGENT) should open a window whose first program IS the program
  (run_in_uterm with the tabprog command line, Stack 32768 cookie), or the window opened must be vsh
  (fix of bug 2) and the request must target that new window; verify on a rig by clicking Apps > Claude
  with no UP-Term window open and checking T:UP-Term-claude appears and the /login prompt shows.

## Next probe (not done)
Own rig (rig.py start --max --purpose "s77 Patina session: launch bug hunt"), no terminal open, run
patina-gui, click Apps > Claude; then `list T:UP-Term-#?` and `UPTerm ACTIVE screen FILE RAM:s.txt`.
