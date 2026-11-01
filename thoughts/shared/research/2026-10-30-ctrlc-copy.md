---
date: 2026-10-30
topic: CTRLC-COPY - Ctrl-C does not stop Copy in an UP-Term tab (owner A1200 2026-10-23)
status: not reproduced on rig 5; no code change
---

# CTRLC-COPY rig hunt, 2026-10-30 (rig 5, HEAD a276b6f5, installed handler/vsh match the build)

## Result: not reproduced (third agent)

Every case below: Ctrl-C ~0.2 s after Return on `Copy C: TO RAM:ccx` (C: = 244-246 files; the whole
of C: copies in under 1 s at rig max speed, so 1 s is too late to see a break).

| Case | Copied | Tab text |
|---|---|---|
| fresh tab (vsh), `tools/rig/sessions/ctrlc_copy_session.py` | 35 of 244 | `CPU..***Break` / `Destination file "CPU" removed.` |
| fresh tab, manual | 127 of 246 | `xBench..***Break` / `Destination file "xBench" removed.` |
| inside tmux in the tab | 132 of 246 | (stopped) |
| after tmux exit + nvim + `:q` | 67 of 246 | (stopped) |

Rig gotcha: `#` typed through ami.req 0x08 arrives as the keymap's pound sign, so `Copy C:#?` typed on
the rig becomes `Copy C:£?` and copies nothing. Use `Copy C:` (same files, same per-file break check).

## CTRLC-RAW: reproduced and fixed (s30 agent, rig 5, 2026-10-30)

Hypothesis 1 holds. Path: any program that calls SetMode(1) and ends without SetMode(0) (an error path, a
crash, a forgotten restore). The handler keeps `c->raw` per console until someone sends SetMode(0)
(handler/vtcon_handler.c ACTION_SCREEN_MODE); program exit resets nothing. vsh only sent SetMode(0) after a
`fg` that restored termios (shell/vsh.c os_wait). So the prompt read in raw mode: no echo, Return (CR) does
not end vsh's FGets line, and `le_break_key` (handler/lineedit.c) makes ^C data when raw && xterm.

Rig (tools/rig/sessions/ctrlc_copy_session.py with CTRLC_RAW=1; VTC:rawleave = tests/amiga/rawleave.c,
SetMode(1) then exit): with Return the Copy never even ran (0 files); with Ctrl-J ending the line (LF ends
FGets) Copy ran and Ctrl-C 0.2 s later did nothing: RAM:ccx 244 of 244 files. Owner's symptom exactly.
`wasabi kill --force` of a raw program is NOT a usable path: vsh then waits forever for the dead job's reply.

Invariant: vsh's interactive prompt reads in the console's cooked mode; a mode a finished command left
ends at the next prompt (readline puts its mode in per prompt; the 3.2 Shell sets its mode per prompt).
Fix: `sh_prompt_mode` (shell/sh_exec.c) = os.echo(in, 1) = SetMode(Input(), 0), called by vsh before every
interactive prompt read. Rejected: handler drops raw at process end (mode is per console, not per process;
packets name no owner); ^C a break in raw xterm (programs that want ^C as data, e.g. telnet, would lose it).
SetMode(0) also runs tty_leave, so a lingering termios owner with ISIG off (hypothesis 2) is dropped at the
prompt too.

After the fix (host test a_prompt_takes_the_line_mode_back_after_a_program_left_the_console_raw: 4 checks fail
with the fix reverted, 570/570 pass with it; rig 5 with the new vsh): CTRLC_RAW=1 with Return now runs the Copy
(the line ends) and it stops early, 9 and 71 of 244 files, versus 244 of 244 before. OPEN: the second run's
tab text shows no "***Break" line (last line "ftpd..copied." then the prompt), so it is not yet proven that
the Copy was broken rather than cut by something else; the plain case passes (85 of 244, ***Break shown).

## Code paths checked (handler/vtcon_handler.c, handler/brk.c, shell/vsh.c)
- `typed()`: without an active termios owner, 0x03 goes straight to `key_in`; `le_break_key` returns
  LE_BRK_NONE only when `raw && xterm`. With a live termios owner, ISIG decides (`tty_signal`).
- `brk_send` signals `b->owner` (set by ACTION_CHANGE_SIGNAL) when alive, else the opener. No sender of
  ACTION_CHANGE_SIGNAL exists in vtcon or ixemul-vtcon sources.
- vsh `os_wait` forwards CTRL_C/D/E/F to the job process (`job_signal`), clears CTRL_C before PutMsg.
- Ctrl-X is not a break key anywhere (by design); Ctrl-D waits in the type-ahead queue (by design).

## Remaining hypotheses (none reproduced)
1. A program left the console raw (ACTION_SCREEN_MODE) with xterm personality: vsh never resets raw at
   its prompt, so ^C would become data. tmux and nvim did NOT leave it so on the rig.
2. A still-live background process owning termios with ISIG off (tty_active stays true).
3. Something specific to the A1200 session state on 2026-10-23 (R8); owner's R9 rerun passed.

## Next step if it recurs
On the A1200, capture a VTCON_DEBUG handler log (`break to` / `tty signal` lines) while pressing Ctrl-C,
plus `wasabi ps` for any termios owner; note what ran in that tab before the Copy.
