# KIT-WASABI-DOUBLE evidence (rig 91, 2026-11-03)

Fix: wasabid claims public port WASABI.daemon (non-trial only); a second start prints
"wasabid: already running" and returns 5. wasabi PR https://github.com/creep-ltx/wasabi/pull/11,
merge commit 959c33bd5493ca78d787c249d4155073049d54ab (tools/nas/wasabi.rev).
Rig: S:User-Startup block ";BEGIN rig-wasabid" runs BOOTX:wasabid-up before the kit blocks; GO start stays as fallback.

## Rig 91 proof (rig started with --purpose "vtcon-ee: KIT-WASABI-DOUBLE one wasabid check")
The full kit install could not run: `make dist-base` stops with
"dist: .../upterm-ports/build/kit/userland was staged before the stamp: make kit-stage in upterm-ports"
(other repo, not touched). Stand-in: kit binary copied to C:wasabid and the kit's exact block appended
to S:User-Startup (";BEGIN UP-Term wasabi" / "Run >NIL: C:wasabid" / ";END UP-Term wasabi"), then `ami.py reboot`.

After reboot:
- S:User-Startup lines: 47 ";BEGIN rig-wasabid" before 97 ";BEGIN UP-Term wasabi" / 98 "Run >NIL: C:wasabid"
- `Status COM C:wasabid` -> rc 5 (no C:wasabid process: the kit's start exited)
- `Status 3 FULL` -> "Process  3: stk 262144, gv 150, pri   1 Loaded as command: BOOTX:wasabid" (the only daemon;
  the other BOOTX:wasabid entry in Status is the 16384-stack wasabi-runner of the exec call itself)
- `lsof -nP -iTCP:1234 -sTCP:LISTEN` filtered to rig 91's fs-uae pid 21929: 0 lines
- `lsof -nP -iTCP:1324 -sTCP:LISTEN` -> "fs-uae 21929 ... TCP *:1324" (rig answers on its own port; ami.py exec works)
- manual `C:wasabid` -> "wasabid: already running", rc 5
- Observed before the fix on another rig: fs-uae pid 85957 (rig 96) listens on *:1234.

## Regression test
tests/test_rig_wasabid_block.py: 6 failed on origin/main code (AttributeError: module 'rig' has no attribute
'user_startup_with_rig_blocks'), 6 passed with the change.

## Not proven
- wasabid restart (g_restart relaunch after singleton_release) not exercised on the rig.
- install_rig case_wasabi / installer_rig stand_in edits are untested (kit not buildable here).
