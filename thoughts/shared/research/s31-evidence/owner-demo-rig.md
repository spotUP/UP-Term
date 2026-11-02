# Owner full-experience demo rig 96 (session 31)

Built from origin/main 08dcd937 (make dist, make ports-kit-check, make rig-helpers all rc 0). Rig 96: OS 3.2, --max, fs-uae,
idle watchdog off (--idle-minutes 0), purpose "Amarchy session 31: owner full-experience demo". RIG-STALE-BASE was overridden
(origin/main moved 7 kitty-only commits ahead during the build).

Install: `INSTALLER_KEEP=1 installer_rig.py ONLY fresh-all FORCE` (real Installer, every option ticked, reboot, kit kept).
One check of the case failed: "AmiSSL installed: LIBS:amisslmaster.library is version 5 or newer: object not found" (the
AmiSSL: drawer files exist; the check reads LIBS: before the reboot). After the reboot: AMARCHY STATUS
`hits=0 rows=69 enabled=1 active=0 wins=0 snaps=0 bar=1 wbpens=0 wbrefused=0 shots=0 notes=0 ticks=140`; Left Amiga+Space opens the menu.

Finding 1: the install does NOT turn the flat look on. ENVARC:Amarchy/prefs holds only the header line. The startup block
`amarchy-flat BOOT` is installed, but flat stays off until the prefs say flat-system-look / flat-window-gadgets = on.

Finding 2: after put_flat_prefs(True) + theme pick, SYS:Prefs/Workbench (the apply step) shows a "Program Error" requester
(button OK) and the apply never finishes (AMARCHY_WBCLOSES not written, 60 s). The requester does not go away with ui click OK
(4 tries) or Return; it was still open at the end. The first pick also changed the theme tokyo-night to vantablack; a second
pick returned it (prefs now carry no theme key, the default).

State at the end: ENV:Amarchy/prefs flat-system-look = on, flat-window-gadgets = on; `UP-Term:bin/amarchy-flat STATUS`
`flat=on calls=329 chained=0`; SYS:C open (WB ARexx WINDOW 'SYS:C' OPEN, rc 0); backdrop tone (26,27,38) = tokyo-night.

Pixel numbers (~/Desktop/upterm-shots/s31-demo-3.png), taken while the Program Error requester was open, so unreliable:
- backdrop region: (26,27,38) 5351 px; flat tone present.
- close, depth, zoom boxes of window C: each ONE colour (170,170,170), equal to the title band; no glyph in the box (expected 2 colours): FAIL as measured.
- scroller arrows: up arrow box 1 colour (no glyph); others 2-3 colours: not proven.
- icons and the clean STATUS check were not completed.
