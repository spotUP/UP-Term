# Splits baseline: tmux on the Amiga (NS0.1)

Rig 4, FS-UAE, installed handler and tmux in `build/rig4/vtc` (not the branch build; this measures tmux). Driver: reuses `tmux_rig.py` helpers (`open_vsh`, `typeline`, `ctrl_b`, `lines`) with a throwaway script; 3 runs each, host-timed.
Raw: `build/rig/splits/baseline.txt` (git-ignored).

| Figure | Runs | Mean | Spread |
|---|---|---|---|
| tmux startup to usable prompt | 9.70, 10.61, 8.73 s | 9.7 s | 8.7-10.6 s (poll +-1 s) |
| tmux idle CPU, 2 panes | 7.2, 6.5, 7.6 % | 7.1 % | 6.5-7.6 % |
| same, no tmux (plain vsh) | 0.2, 0.7, 0.2 % | 0.4 % | |
| 138 KB `Type` in a tmux pane | 3.00, 3.00, 3.12 s | 3.04 s (~45 KB/s) | 3.00-3.12 s |
| same, no tmux | 1.67, 1.72, 1.76 s | 1.72 s (~80 KB/s) | 1.67-1.76 s |

Native splits must beat: startup < 9.7 s, idle < 7.1 % (target near the 0.4 % no-tmux floor), pane throughput better than 45 KB/s (tmux costs 1.77x the plain time).

Method
- Startup: typed `VTC:tmux -f /VTC/tmux.conf`, then `Echo done >RAM:up` (buffered until the pane shell reads it); timer stops when the host sees the file (poll 1/s).
- Idle: after Ctrl-B %, 10 s wait, `VTC:top -b -d 10 -n 1`, "CPU busy" value. tmux.conf has a `#()` status job every 5 s, which is part of the idle load.
- Throughput: `Type VTC:big.txt` (137989 B) then `Echo done >RAM:tp_done`; wall time to the marker, includes up to 1 s poll latency.
- engbench (`tests/amiga/engbench.c`) is engine-only (no console/tmux path), so the fixed `Type` was used instead.
- Caveat: the plan's "frames/s of engbench 7" figure is not obtainable this way; NS0.1's file assertion needs adjusting.
