# boot-console-capture-hunt (rig 150, 2026-11-05)

Question: why does Startup-Sequence output never reach BOOTX:boot.console, and which capture route works?
Prior evidence: boot-console-live.md in the main checkout (0-byte file, even Echo in User-Startup not captured).

## Root cause
`C:Execute DH0:S/Startup-Sequence >BOOTX:boot.console`: on 2.0+ Execute does not run the script itself. It puts the
script in front of the calling shell's input (cli_CurrentInput) and returns at once. The shell's `>` redirect lives
only for the Execute command, so the shell closes the file (0 bytes, created) when Execute exits, and then reads the
script's lines with its own cli_StandardOutput: the boot console. Not a flush problem: the handle is closed at once.

## Route 1: `C:Run >BOOTX:boot.console C:Echo "boot console open" + / C:Execute DH0:S/Startup-Sequence` - FAILED
- boot.console = 8 bytes: `[CLI 3]` (Run's own message). The child CLI's Echo did not land in the file.
- The child printed into a console window instead: start failed on IPrefs' System Request
  ("windows were open while IPrefs reset the screen", RIG-IPREFS-SYSREQ). So `Run >file` redirects Run alone.

## Route 2 (chosen): BOOTX:bootcap - tools/rig/bootcap.c
`BOOTX:bootcap BOOTX:boot.console C:Execute DH0:S/Startup-Sequence`: SystemTags(cmd, SYS_Input NIL:, SYS_Output
the file, SYS_UserShell, synchronous), so the CLI that splices the Startup-Sequence has the file as its standard
output; the handle closes when that CLI ends (EndCLI). It writes `boot console open` first; start fails when the marker
is missing (boot_console_captured), so a 0-byte file can no longer pass as "clean".
Staged every start by stage_bootcap (make build/amiga/bootcap, copied to RIG/boot/bootcap).

## Live results
- (b) clean boot, fresh start: rc 0, "up", boot.console 18 bytes = `boot console open`. PASS.
- (a) appended to S:User-Startup (after ;END UP-Term) `Echo BOOTMARK-user-startup` and `C:Type RAM:nosuchfile`,
  `rig.py reboot` (same sys.hdf): boot.console = `boot console open` / `BOOTMARK-user-startup` /
  `TYPE can't open RAM:nosuchfile` / `object not found`; start exit 1 with
  `[ERROR] rig: boot console: TYPE can't open RAM:nosuchfile`, `[ERROR] rig: boot console: object not found`,
  `rig: the boot printed 2 error line(s)`. PASS: User-Startup output now reaches the file, and the gate fires.
- No IPrefs System Request on the bootcap boots (output never opens a console window).
- Rig 150 stopped, build/rig150 removed.

## Not covered
- A DOS requester (e.g. `List NOSUCHVOL:` "Please insert volume") still blocks the boot instead of printing a line;
  that is a requester, not console output, and stays for the requester watch.
- Run'd children of the Startup-Sequence without their own redirect get no console now (the CLI's input is NIL:);
  the clean boot showed no effect.
