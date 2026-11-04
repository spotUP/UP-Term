# amarchy-flat (active) vs wasabid on fresh 3.2 rig 66 (s54)

Date 2026-11-04 09:29-09:45. Verdict: FLAT FREEZES. The freeze only happens when flat installs at BOOT time. Once the
Workbench is open, a live start works.
The exact statement that hangs is NOT isolated. The suspects are below.

## Host load
- 09:29: fs-uae rig67 114 %, node (another repo's vitest) 84 %, tsc 30 %; load average 62/72/74 (8 cores).

## Control: live start, WB already open (load 158 -> 92)
- Fresh `start --max --os 32`, wasabid 0.11b2 answers, `flat=absent`, ENVARC:Amarchy missing.
- 09:33:24 `Run >NIL: VTC:amarchy-flat BOOT` via exec, rc 0.
- Ping every 10 s, 09:33:28-09:35:31: 13 of 13 `wasabid 0.11b2`. Host load average was 158 at the start of the window.
- 09:35:46 STATUS: `flat=on calls=0 chained=0`, `rectfill installed=yes`, `text installed=yes`, `frame=hooked`,
  `pens-held 16 refs=1 dim=249 knob=250 inbar=251`. Status lists `VTC:amarchy-flat`. `Directory "ENVARC:Amarchy" is empty`:
  the 60 s marker was deleted, so flat's main loop ran.

## Boot A: same image, flat line in User-Startup (after the rig-wasabid block), load 18-40
- 09:36:10 `stop --keep`; appended `Run >NIL: VTC:amarchy-flat BOOT`; `start --keep-sys --max --os 32`.
- The start's requester watch (ami.py:237 `_watch` -> wasabi read_ui -> drain_to_end -> `_fill`) died with
  `TimeoutError: timed out`. wasabid had accepted the connection and was in the middle of the UI walk when it stopped
  answering. Then `[WARN] S:Shell-Startup not read: auth failed: the Amiga took the connection but its daemon did not
  answer for 150 s` and `auth failed ... 150 s`. ping at 09:43:12: timeout (rc 124).
- Decisive: host file `os-32/Prefs/Env-Archive/Amarchy/flat-starting` written 09:36:48. It still existed at 09:44:46,
  8 min later. Flat deletes it 60 s after install_all (flat.c:1513-1515). In the control run it was gone within 2 min.
  So flat's own task never got back to its Wait loop after the boot-time install_all, and wasabid hung at the same time.
- Load average was 40 at the failure, far below the 158 the control survived. Host starvation does not explain it.
- fs-uae CPU was 96-104 % in both runs (the --max 060 runs flat out), so CPU does not tell a spin from idle.
  fs-uae.log ends at `clipboard task init` (saved to the scratchpad as fs-uae-bootA.log; nothing after the mount lines).
- Tally with Runs 4 and 5: active flat at boot lost wasabid in 4 of 4 boots. The flat=held boot and the boots without
  flat answered.

## Rig 96 (read-only)
- `rig.py status` = down, so no live read. Its User-Startup has the Amarchy-Flat block (line 48) after rig-wasabid
  (line 22). The earlier eye-check (rig96-eyecheck-32.md) took wasabi reads and shots with amarchy-flat running after a
  reboot. That makes it a counterexample only if flat was active there, which is not known.

## Where
Boot path. At boot the Workbench screen is not open yet, so main()'s install_all returns FLAT_ERR_NOCLASS. The 1 s
screen watch then retries the install when LoadWB opens the screen (flat.c:1506-1508). The sequence is marker_write
(flat.c:1390, 09:36:48), then hook_install (sysiclass + frameiclass swaps, flat.c:1077), rf_install, tx_install. In the
same loop pass it runs pens_sync and pens_run (flat.c:1509-1510; ipd_lock_wb_screen and ObtainPen on a screen that has
just opened). The live start only differs in timing: it installs on a settled screen, and STATUS `calls=0` shows that
no image draw went through the hook.
Suspects, most likely first:
1. flat.c:103 `c7_screen_which` calls LockIBase(0) from inside the sysiclass dispatcher (called at flat.c:283). It runs
   in whatever task draws the screen's title-bar gadget (Intuition/LoadWB, holding layer locks) while the screen opens.
   wasabid's UI walk holds LockIBase (wasabid.c:2591) and then needs layers, so the two can deadlock (ABBA). That
   matches "wasabid stalls mid read_ui" and "flat's loop stalls". Caveat: this path only runs when
   GetAttr(SYSIA_Which) fails. Whether 3.2 sysiclass answers it is unverified.
2. pens_run / ipd_lock_wb_screen + ObtainPen from flat's task while LoadWB is still opening the screen (flat.c:1349).
3. Minor: amconf_load() does DOS Open/Lock inside Forbid at flat.c ~1455-1458 (AddSemaphore block). Wait breaks the
   Forbid, so this is not a deadlock on its own, but it is wrong.
Next probe: boot with flat in User-Startup but put `Wait 20` before it, so flat installs on a settled WB. If wasabid
survives, the bug is in the screen-open window. Then stub the c7 LockIBase (or count c7 calls in STATUS) to separate
suspect 1 from suspect 2.

## Files
Scratchpad: startA.log, fs-uae-bootA.log, marker-ls.txt, probe1.log, User-Startup.orig (1.2 MB total). Rig 66 stopped
and cleared.
