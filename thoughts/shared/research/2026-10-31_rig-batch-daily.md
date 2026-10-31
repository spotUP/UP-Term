# RIG-BATCH-DAILY run, s39, rig 3 (2026-10-31)

Items (4) and (3) of the queue, one rig 3 (`start --max`), patched ixemul.library (ixemul-vtcon feature/ixemul-80, build295, 201256 bytes FPU, refreshed into ixp6 at start) and current vtcon-handler.

## (4) WATCHDOG: PASS
`UPTERM_RIG=3 rig.py start --max --idle-minutes 2`, no commands sent. Rig process gone at 21:54, start was 21:51.
build/rig3/watchdog.log: `2026-10-31 21:53:47 idle 2 min: stopping rig 3`

## (3) WINDOW-CLOSE-KILLS-APP: PASS (nano and vsh, separate runs)
- `UPTERM_RIG=3 python3 tools/rig/hangup_rig.py nano`: `hangupnano: running before close ['Process  2: Loaded as command: UP-Term:bin/nano']; window gone True; left after close []: ok`
- `UPTERM_RIG=3 python3 tools/rig/hangup_rig.py vsh`: `hangupvsh: running before close ['Process 11: Loaded as command: VTC:vsh']; window gone True; left after close []: ok`
- mg skipped (not installed).
Note: first boot of the second start needed one retry (`no wasabid answer`, host load); second boot came up.
Note: build/wasabi HEAD is 50231b33, tools/nas/wasabi.rev pins 83acd194; `make wasabi` was not run, rig and ami.py worked anyway.

## (1), (2): not run
Need upterm-ports builds (upkg, slow server), out of scope of this agent.
