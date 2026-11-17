---
date: 2026-11-11
topic: FLAT-SPACING / FLAT-WIDE-INPUT side 2 proof on AGA PAL hires (non-square pixels)
branch: main
commit: 578b6dfc
rig: 69
---

# Side 2 proof on AGA PAL hires, rig 69

Measure only. No source edits, no builds, no commits. Pixels read as text through
`ami.grab` (helper scripts runs.py / census.py copied to the session scratchpad
folder side2/). No screenshots viewed.

## Expected numbers (stated before measuring)

Source: thoughts/shared/s72-evidence/flat-spacing-design.md ("1 pixel inset per side,
scaled for the pixel aspect"; tests: "gap of 2 * side columns"; rig proof line:
"4 columns on a non-square side-2 screen") and the code:
- `flat_side_px(rx, ry)` (amarchy/flat_core.c:1405) = round(ry / rx), 1 when ry <= rx.
  PAL hires non-interlaced pixels are twice as tall as wide, so side = 2.
- `side_px(r, scr)` in amarchy/flat.c:618 is the one per-screen source.
- `flat_inset(side, ...)` (flat_core.c:1463): inset box = x0 + side .. x1 - side,
  y0 + 1 .. y1 - 1.

So on side 2: each filled box loses 2 columns left and right, 1 row top and bottom.
Two touching boxes show a horizontal gap of 4 columns and a vertical gap of 2 rows of
window ground. Fitted box (FLAT-WIDE-INPUT): the box ends `side` (2) columns past the
text bound; fit tolerance 2 * side = 4.

## Rig steps and events

1. `df -h /`: 5.4 GiB free before start.
2. `UPTERM_RIG=69 rig.py setup`, then `rig.py start --max --idle-minutes 0 --purpose
   "s73: side 2 proof on AGA PAL hires"` (rc 0). No `[BUILD] stale` warning. Refreshed
   pty-handler 13156, upprefs 199380, vtcon-handler 610512. `List UP-Term:bin/amarchy-flat`
   = 239792 bytes (14:13:34). Warning: DOpus 5.104 not installed in VTC: (as on rig 68).
3. `Copy ENV:Sys/palette.prefs ENVARC:Sys/palette.prefs CLONE` (rc 0), then `rig.py aga`.
   The aga reboot never answered: "auth failed: the Amiga took the connection but its
   daemon did not answer for 150 s", then "stopped". retry.log: "saved ScreenMode is
   native (DisplayID 0x00029000)" (PAL monitor + HIRES key).
4. `rig.py reboot` (cold, keeps DH0:): rig answered. Screen 640x256 depth 4.
5. ENVARC:Sys/palette.prefs was missing again after the aga switch.
6. `Run >NIL: SYS:Prefs/Time`: window "Time Preferences" 0,14 332x203 on screen
   S 0 640x256 depth=4 "Workbench Screen". STATUS read (see Q1).
7. Copied palette.prefs to ENVARC: again (434 bytes, 14:22:48), `ami.py reboot` (warm):
   "back after 10 s", then the daemon never answered again (ami connect timeout).
8. Two more cold `rig.py reboot`: both failed ("the Amiga stopped answering while Assign
   was running" / "daemon did not answer for 150 s"). Serial log reaches the vsh prompt
   (BOOTX: %) on these boots, so the Amiga boots; only wasabid stays silent. fs-uae log:
   2 "CPU reset" lines, no guru/alert, one "FPU EXCEPTION 1 ... PC=00F80C94" (ROM).
9. Hypothesis test: `rig.py stop`, fresh `start`, `aga` (hung again, "stopped"), cold
   `reboot` (failed again, same Assign message). So the missing palette.prefs is NOT the
   cause: on AGA, rig 69 answered on 1 boot of 6. Cause not investigated (budget).

## Q1 flat=on and side=2 on AGA PAL hires: PASS
`ami.py uitree`: `S 0 640x256 depth=4 "Workbench Screen"`, `W 0 0 14 332x203 active
"Time Preferences"`. rig retry.log: native DisplayID 0x00029000.
`UP-Term:bin/amarchy-flat STATUS` with Time Preferences open:
- line 2: `flat=on calls=57 chained=0`
- line 9: `rectfill installed=yes calls=2255 hits=30 shits=2 mhits=0 bhits=0 gboxes=38 gfit=1 gfitbusy=0 side=2 looks=2`
- also: `pens-held 0 missing 8 9 10 11 12 13 14 15` (16-colour screen: pens 8..15 not obtained;
  read before the Time window opened). Not followed up.

## Q2 gaps: NOT MEASURED
## Q3 date row box fit / refit: NOT MEASURED
## Q4 inset width horizontal vs vertical: NOT MEASURED (expected 2 columns vs 1 row)
## Q5 colours / 7f007f / 00ff9f: NOT MEASURED
Reason (attempt 1): the rig stopped answering before any pixel grab (step 7 onwards).

### Attempt 2 (2026-11-11 15:1x-15:56, same commit 578b6dfc, every rig command with
`UPTERM_RIG=69 UPTERM_REQUESTER_WATCH=0`, wrapped in `timeout 170`)
No pixel was grabbed. Boots: 3 of the 3 allowed; all 3 answered the first ping, 2 froze later.
1. `df -h /`: 7.8 GiB free. `rig.py setup` rc 0, `rig.py start --max --idle-minutes 0 --purpose
   "s73: side 2 (AGA PAL hires) flat spacing measurements"`: up (boot 1, RTG).
2. `Copy ENV:Sys/palette.prefs ENVARC:Sys/palette.prefs CLONE` rc 0; `rig.py aga` rc 0, "up",
   Shell-Startup read (boot 2, AGA). `rig.py status`: `wasabid 0.11b2`. The first `ami.py uitree`
   returned 0 bytes; the next exec and a second uitree timed out (rc 124); `rig.py status`: `down`.
   So with the watcher off the daemon still froze, on the FIRST UI-tree request (wasabid cmd_ui),
   a few seconds after "up".
3. `rig.py reboot` (cold, boot 3): "up". Waited 45 s, ping answered. `Run >NIL: SYS:Prefs/Time`
   rc 0. `VTC:amarchy-flat STATUS` and `UP-Term:bin/amarchy-flat STATUS`: `flat=absent` (no
   amarchy-flat process in `Status`; attempt 1 had flat=on on its answered boot).
   `Break 9 C` did not close Time Preferences. `Run >NIL: UP-Term:bin/amarchy-flat BOOT`: process
   12 runs, STATUS below. Then `ami.click_px(276, 219)` (Cancel) called `pointer_scale()` ->
   `list_windows()`: socket timeout; `rig.py status`: `down`. Third boot used: stopped.

STATUS on boot 3 after the manual `amarchy-flat BOOT` (Time Preferences already open, opened
before flat started):
```
flat=on calls=0 chained=0
loop beat=0 stage=none refused=none refusals=0
chain 1 task=10269AD8 Background CLI state=4 sigwait=00000010 sem=1000B926 private owner=100F1328 « IPrefs » nest=1 queue=4
stuck state=4 sigwait=00000010 sem=1000B926 private owner=100F1328 « IPrefs » nest=1
chain 2 task=100F1328 « IPrefs » state=4 sigwait=00000010 sem=00000000  owner=00000000  nest=0 queue=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
dopus=absent
rectfill installed=no calls=0 hits=0 shits=0 mhits=0 bhits=0 gboxes=0 gfit=0 gfitbusy=0 side=0 looks=0
text installed=no calls=0 hits=0 labels=0 lskip=0 cmin=0 ctask= chw=0 cstk=16384 cnostk=0
cprobe fill256=0 deep=0 guard=0 hdr=0 b0=0 b4=0
clone draws=0 fallback=0
clone fallback busy=0 nodri=0 nobm=0 empty=0 which=0
clone empty ids=0 last which=0 size=0 nonzero=0 marks=0
pens-held 0 refs=0 screens=0 applies=0 dim=0 knob=0 inbar=0
frame=absent
```
`side=2 looks=2` NOT confirmed on this boot: side=0 looks=0, rectfill not installed.

Reading (not measured further): amarchy-flat's install is stuck waiting on private semaphore
1000B926 owned by IPrefs (IPrefs itself in Wait, sigwait 00000010, nest=1), and every wasabid
request that walks screens/windows (uitree, list_windows) hangs on this AGA boot. One held lock
taken by IPrefs on the native-mode boot would explain both; which semaphore 1000B926 is (IntuitionBase
lock or another) is NOT measured. This replaces the "requester watcher fires too early" reading
of rig-aga-wasabid-silent.md as the next thing to check: the watcher was off here and the
daemon still froze.

Also learned: on 640x256 `pointer_scale()` reports grab rows = screen rows x row_scale (2), so a
grab of this screen is 512 rows tall; vertical runs from runs.py must be halved to screen rows.

## End state
- rig 69 stopped (`rig.py stop` rc 0); build/rig69 removed (checked: no such directory).
- build/rig-saved/69 holds 6 saves, 55 MB total: 20261111-143729, -144542 (attempt 1),
  -152213, -152742, -153635 (the wasabid-silent investigation), -155647 (this attempt, 48 files,
  9.5 MB). Left in place.
- No screenshots taken. Scratchpad side2b/ 28 KB (logs, status0.txt), side2/ 72 KB. Nothing over
  10 MB, nothing deleted.
- `df -h /` at end: 5.2 GiB free.

## Next step suggestion
Find why wasabid does not answer after an AGA boot on rig 69 (the boot reaches the vsh prompt
on serial; the step after "Assign" stalls). Possibly AGA-mode boot timing or amarchy-flat on a
16-colour screen (pens 8..15 missing). Then repeat Q2-Q5 with the runs.py / census.py method.
