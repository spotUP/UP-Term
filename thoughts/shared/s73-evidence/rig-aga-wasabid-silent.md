---
date: 2026-11-11
topic: RIG-AGA-WASABID-SILENT - why wasabid stops answering after an AGA boot on rig 69
rig: 69
status: narrowed, not proven
---

# RIG-AGA-WASABID-SILENT, rig 69

Measurement and reading only. No source edits, no builds, no commits.

## Verdict

Narrowed to one interaction: the rig tool's requester watcher (tools/rig/ami.py:186-188 `requesters()` ->
wasabi `read_ui(every_screen=True)` -> wasabid `cmd_ui`, build/wasabi/src/wasabid.c:4034) fired in the first
seconds after "up" on an AGA boot, while the Amarchy parts (Amarchy block / SolidWin) are coming up on the freshly
reopened native Workbench. With the watcher off, or with Amarchy and SolidWin off, every AGA boot answered.
Which Amarchy part, and whether wasabid deadlocks inside LockIBase or is just held off, is NOT measured.

Not the cause:
- (c) CPU starvation: fs-uae host CPU was 70-91 % on answered and silent boots alike (table). --a1200 not run.
- palette.prefs (ruled out before, side2-aga-rig69.md).
- the boot itself: boot.log reaches "wasabid started" and the serial log the vsh prompt on every boot, silent ones
  included; the daemon answered the first ping (rig.py printed "up") on every boot.
- the IPrefs RTG switch: retry.log on every boot: "saved ScreenMode is native (DisplayID 0x00029000): no RTG switch
  to wait for".

## Method

Script: scratchpad agahang/hunt.py (resumable, verdicts in results.tsv). Per condition: `rig.py stop`, `setup`,
`start <flags>` (RTG boot), `aga <flags>` (AGA boot 1), `reboot <flags>` twice (AGA boots 2, 3). All with
`--idle-minutes 0 --purpose "s73: AGA boot hang hunt"`. After each boot: `ami.py ping`, wait 30 s, `ami.py ping`;
answered = both pings ok. Also fs-uae CPU (ps), serial.log size before/after the 30 s, boot.log tail, copies of
boot.log, boot.console, retry.log, serial tail, fs-uae log tail.

Flow in rig.py start after "up" (tools/rig/rig.py:2458-2476): `boot_assigns_missing()` runs EXEC "Assign LIST"
(rig.py:2531) through ami.req, whose requester watcher thread (ami.py:225-240, on unless
UPTERM_REQUESTER_WATCH=0) calls `requesters()` = UI read of every screen on a second connection; then
`guard_shell_startup()` reads S:Shell-Startup. The two error texts of the defect are these two steps:
"stopped answering while Assign was running" = Assign LIST, "S:Shell-Startup not read: auth failed ... did not
answer for 150 s" = guard_shell_startup.

## Boots per condition (AGA PAL hires, DisplayID 0x00029000)

| condition | flags | answered | silent |
|---|---|---|---|
| previous agent (side2-aga-rig69.md) | --max | 1 | 5 |
| A default | --max | 2 | 1 (+1 stall) |
| D watcher off | --max, env UPTERM_REQUESTER_WATCH=0 | 3 | 0 |
| B no Amarchy | --max --no-amarchy-block --no-solidwin | 3 | 0 |
| C a1200 | --a1200 | not run (budget) | |

RTG boot before each condition: answered 3 of 3.

Detail, condition A:
- boot 1 (aga, 85 s): the watcher's read_ui timed out (`socket.timeout` in ami.py:237 _watch -> 188 requesters ->
  wasabi read_ui 3254 -> drain_to_end), then `[WARN] S:Shell-Startup not read: auth failed: the Amiga took the
  connection but its daemon did not answer ...`. 30-60 s later both probe pings answered: a stall, not a death.
- boot 2 (reboot, 222 s): "up", then both probe pings `auth failed ... did not answer for 150 s`; fs-uae CPU 84 / 83 %,
  serial.log unchanged (12339 bytes, Amiga quiet at the vsh prompt), boot.log ends "wasabid started".
- boot 3 (reboot, 34 s): answered.
Conditions D and B: every boot 33-105 s, no read_ui timeout, no WARN, all pings ok.

## Reading (not measured)

`cmd_ui` (wasabid.c:4034) runs `mui3_prescan()` (memory scan, wasabid.c:3370) and then walks every screen and window
under `LockIBase()`, calling `ui_items` -> MUI/BOOPSI `GetAttr`/`DoMethodA` on window objects (wasabid.c:3546-3970)
while Intuition is locked. Calling into another program's objects under LockIBase is the documented deadlock
pattern (wasabi's own audit3.md item 1). Right after an AGA boot the Workbench has just been reopened in the native
mode and the Amarchy parts are opening windows and patching (amarchy-flat RectFill patch, wbreset patch of
CloseWorkBench/OpenWorkBench, the Amarchy commodity's windows): the likely window for a lock-order conflict between
wasabid's walk and an Amarchy part. Unproven.

## Next decisive measurement

Boot AGA with UPTERM_REQUESTER_WATCH=0 (answers reliably), then in the first 60 s after "up" run
`ami.py requesters` in a loop (one connection, 30 s timeout) and record freezes; 3 boots each with:
all Amarchy parts on; only `--no-solidwin`; Amarchy commodity off but amarchy-flat on; amarchy-flat off. A freeze
on one subset names the part. On a frozen boot, an fs-uae debugger/`rig.py dbg` task list showing wasabid's PC
inside LockIBase / ObtainSemaphore would prove the deadlock and its partner.

## End state

rig 69 stopped by the hunt's final `rig.py stop` (build/rig69 removed). rig.py stop saved
build/rig-saved/69/20261111-152213, -152742, -153635 (about 9.5 MB each; with the two older ones the folder is
46 MB). Scratchpad agahang/ 284 KB. `df -h /`: 7.8 GiB free at the end. No screenshots taken.
