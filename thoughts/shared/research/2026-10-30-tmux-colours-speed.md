# tmux draws colors.sh: speed (s30, 2026-10-30)

Question: tmux ~5x slower than plain XCON (25 s vs 5 s, rig 1, earlier).

Setup: rig 8, 68060 (--max), tests/amiga/colors.sh, XCON window running vsh; tmux per tools/rig/tmux_rig.py
(default-terminal screen-256color, vsh as shell). Time = typed command until the window checksum
(wasabi sum --window) stops changing for 6 s; 3 runs each. Host: 4-5 fs-uae rigs at 90-110% CPU each, nothing else.
Verified the tmux window was the one drawn (green status row, 245 colours in the grab).

- plain XCON: 1.8 / 1.8 / 1.5 s (script finishes ~1.2-1.8 s by RAM:done marker; XCON draws as it goes)
- inside tmux: 2.2 / 3.2 / 2.2 s (script returns in 1.2 s; tmux finishes the redraw ~1-2 s later)
- tmux task state while drawing: 22 of 23 samples "wait" (not CPU-bound in tmux), PTY handler wait.

Result: not reproduced. Ratio now ~1.5x, absolute 2-3 s, not 25 s. XCON is the console here
(tmux_rig uses XCON:, vtcon handler/engine is not in this path), so the engine/handler cannot be the cause.
Likely the old 25 s included tmux_rig's cube_rig.shoot_check screenshot cadence, an older tmux/pty-handler build
or a loaded host. No code change, no tmux.conf change proposed.
Not measured: 68020 (--a1200) and vtcon: handler as console under tmux; byte counts of tmux output.
