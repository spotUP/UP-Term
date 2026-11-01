---
title: Amiberry key mapping evidence (session 29)
status: incomplete - key table not obtained
---
# Amiberry key mapping, rig 20

## Task 1: stop/status bug (fixed, commit on branch s29-amiberry-keys)
Cause: the emulator choice lived only in `--emu` / `UPTERM_RIG_EMU`. `rig.py stop` and `status` given neither looked for
fs-uae (`mine()` matched nothing), printed "stopped" and never reached the kill, while Amiberry ran. `mine()` itself matches
the Amiberry command line correctly (checked live: it returned the `-G --log -f .../rig20/win/<title>.uae` line, other rigs
not matched). Fix: `start` records the emulator in `<rig>/emu`; `paths.EMU` falls back to it for every command except
start and setup. Host test `StopFindsAmiberry` (stand-in `Amiberry` processes for rig 97 and rig 98, stop without a flag
must end rig 97's and leave rig 98's): FAILS with the old paths.py (errors=1), passes with the fix (10 tests OK).
Live: rig 20 `stop` without flag ended the real Amiberry, no process under `rig20/win` left.

## Task 2: key mapping (NOT done)
Fs-uae map (tools/rig/rig.py HOST_KEYS): lsuper and rsuper -> Right Amiga, ralt -> Left Amiga, left Option -> Alt.
Amiberry: strings of the binary show `RCtrl = RAmiga` (option rctrl_as_ramiga) and `inactive_*`/`active_*` pause options;
no per-key remap table was found in the binary. Not verified further.

Attempt: lcmdprobe on rig 20, JXA CGEvent keys (HID tap) with Amiberry forced frontmost. Result: NO line reached
RAM:lcmdprobe.log, not for Mac keys and not for `ami.py key 20 0` (wasabid key injection) either. So the probe/log path
did not work on this rig run (cause not found; the probe window was open and active, wasabid answered). Not a Mac-key
finding. Other observations that matter for the next attempt:
- the emulation stalled for ~8 minutes after launch while Amiberry was in the background (mount lines in the Amiberry log
  are stamped 17:39 for a 17:31 launch); suspect App Nap / `inactive_*` options; keep Amiberry frontmost or set
  `inactive_priority`/`inactive_pause=no`.
- iTerm2 took the foreground twice during the loop (the script aborted correctly).
- Two probe instances ran at once because the timers ran longer than 60 s real time.
Next: check `Echo >>RAM:x` append and the probe with fs-uae-known-good steps, then rerun the 12 chords one probe at a time.
