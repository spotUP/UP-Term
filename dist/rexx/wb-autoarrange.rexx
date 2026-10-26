/* wb-autoarrange.rexx: Auto-arrange Icons for the active Workbench drawer or volume window
 * Workbench 3.2 offers this as a WBStartup menu item (TOOLS.AUTOARRANGEICONS, added by SYS:WBStartup/AutoArrangeIcons),
 * which workbench.library does not know: MENU INVOKE on it is rc 10 in every state. This runs the same four menu
 * actions on the active window: update, sort the icons by name, resize the window to fit them, snapshot all icons.
 * No Workbench port, or no drawer/volume window active (nothing, or the Workbench root): exit 5, nothing is touched.
 * Plan 2026-10-25-amarchy-window-groups.md, B2. */
OPTIONS RESULTS
IF ~SHOW('P', 'WORKBENCH') THEN EXIT 5
ADDRESS WORKBENCH
'GETATTR OBJECT WINDOWS.ACTIVE'
IF RC ~= 0 THEN EXIT 5
win = STRIP(RESULT)
IF win = '' | TRANSLATE(win) = 'ROOT' THEN EXIT 5
DO i = 1 TO 4
  PARSE VALUE WORD('WINDOW.UPDATE WINDOW.CLEANUPBY.NAME WINDOW.RESIZETOFIT WINDOW.SNAPSHOT.ALL', i) WITH item
  'MENU WINDOW "' || win || '" INVOKE ' || item
  IF RC ~= 0 THEN EXIT RC
END
EXIT 0
