# S85k: DOpus close/reopen options for a theme apply (source read, 2026-11-15)

Source: ~/Code/Reference/dopus5/source. Nothing here is rig-verified.

## DOpus's own CloseWorkBench path (what fails today)
- Program/event_loop.c:614-625: on DN_CLOSE_WORKBENCH, DOpus acts only if !GUIF_CLOSE_PENDING, !GUI->screen,
  GUI->window set, and get_our_pubscreen() == "Workbench". Then it starts process dopus_closewb.
- Program/misc_proc.c:1111-1114: dopus_closewb sends MAIN_CLOSEWB with check_closescreen(WScreen) (misc.c:441).
- Program/event_loop.c:481-501: flags TRUE: close_display(CLOSE_ALL | CLOSE_STOREPOS), GUIF_CLOSED, CloseWorkBench()
  itself, reopen timer 1 s. flags FALSE: only clears GUIF_CLOSE_PENDING; DOpus does nothing.
- event_loop.c:632-642: DN_OPEN_WORKBENCH clears CLOSE_PENDING only if GUIF_CLOSED. So a close notify whose
  dopus_closewb has not answered yet, followed by an open notify, leaves the flag to MAIN_CLOSEWB (normally fine).
- Run 6 census (PATINA_WBR_BLOCKER) tells which window makes check_closescreen FALSE.

## Option H: ARexx Hide before the reset, Reveal after
- function_special.c:50-60: Hide = IPC_HIDE -> hide_display(); Reveal = IPC_SHOW -> display_open.
- display.c hide_display(): RunScript(SCRIPT_HIDE), kills clock, close_display(CLOSE_ALL | CLOSE_STOREPOS) (the
  same close as the closewb path), then shows a hide marker per settings.hide_method:
  HIDE_CLOCK (DEFAULT, Library/config_default.c:166) opens a hidden-clock WINDOW, task dopus_clock, on the
  Workbench screen -> that window itself blocks CloseWorkBench. HIDE_ICON: AppIcon (falls back to menu).
  HIDE_MENU: AppMenuItem.
- Risk: depends on the user's hide_method; with the default the reset still fails. Would need the clock closed
  or hide_method changed for the duration. Also hide_display returns early if the patterns process runs.
- ARexx form for running an internal command (`'Hide'` sent to DOPUS.1) not verified.

## Option Q: Quit FORCE, relaunch
- function_special.c:33-46: Quit with an argument = IPC_Quit(&main_ipc) at once; without = dopus_quit process
  (may ask). Relaunch DOpus5:DirectoryOpus after OpenWorkBench. Lister state lost unless DOpus layout is saved
  and opened at start. Slow (DOpus start takes seconds), heavy.

## Not a reset fix, but the redraw half: `dopus refresh all`
- rexx_dopus.c:947 -> rexx_send_reset(CONFIG_CHANGE_DISPLAY) -> main_reset.c:40-45: close_display(CLOSE_ALL |
  CLOSE_STOREPOS) then display_open(0) at once. This is what DOpus's own themes module runs after a theme
  (Modules/themes/themes.c:636). Reopens with new pens, but immediately, so it does not free the Workbench.
