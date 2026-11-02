---
date: 2026-11-02
topic: K33 kitty placeholder under tmux, rig check
status: rig run open
---
# K33: why `kitty_z_rig.py <out> phtmux` FAILed

## Host side (Mac tmux 3.6a, same stream, TERM=vtcon from terminfo/vtcon.terminfo)
tmux with `allow-passthrough on` re-emits the upload unwrapped to the outer terminal and keeps the cells:
  apc 1, dcs-tmux 0, U+10EEEE 12, U+0305 6, U+030D 6
  outer bytes: `\e_Ga=T,U=1,i=7,f=24,s=2,v=2,c=2,r=2,q=2;/wAAAP8AAAD///8A\e\\`
  cells: `\e[3;3H\e[37m <ph> BS <ph><0305> BS <ph><0305><0305> ...` (base, backspace, base+marks)
So the protocol path is sound; the failure is in the rig run.

## Rig 83, by hand, window text (screen_text of window kz)
`tmux -f VTC:kz.conf new-session "Type VTC:kz-stream.bin; sleep 60"` from the XCON NewShell (AmigaDOS CLI, prompt `11.BOOTX:>`):
  vsh: sleep: not found (it lives in [server exited unexpectedly]
  [0] 0:Type*        "BMS-MAC001" 19:00 02-Nov-26
A second tmux start in the same window crashed the machine:
  rig.py status: crashed: Illegal instruction: 10c8 at 10C8B3B0 -> 00F80AD2, 88 CPU reset(s)

## Cause (rig tool, not vtcon, not the tmux port)
The phtmux mode started tmux from the bare AmigaDOS CLI of a NewShell. The kit tmux carries `$STACK: 262144`
(strings of build/dist/up-term/Files/tmux, same size as tmux-amiga/build/tmux-bin, 1095068), but only vsh and the
kit launchers honour that cookie; the CLI ran it on its own small stack (tmux-amiga 0d30fa7 records the same
overflow under a 16000-byte stack). The working tools/rig/tmux_rig.py starts tmux from vsh (vshwin.open_vsh).
Second, smaller fault: the pane command `sleep 60` is not found in the pane's vsh (no PATH to the kit drawer),
so the pane ended at once; `vsh -c "sleep 1"` from wasabi's runner does find it.

## Fix (tools/rig/kitty_z_rig.py, phtmux only)
Open the window with vshwin.open_vsh (vsh sets TERM, honours $STACK); pane command `Type ...; C:Wait 60`;
after the shot `tmux ls` + `tmux kill-server` through vsh, then `exit` (keys typed while tmux runs reach the pane).

## Rig verdict: OPEN
Rig 83 restart after the crash booted to the vsh prompt on serial, but wasabid never answered
("auth failed: ... did not answer for 150 s"; status stayed `down` 8+ min) with rigs 80, 84, 90 at 70-107% CPU.
Rig stopped and cleared. Next: on a quiet host, `UPTERM_RIG=<n> python3 tools/rig/kitty_z_rig.py <out> phtmux`;
PASS = four colour boxes, `tmux ls` shows the session. Unproven: that the stack is the whole cause of the
server death (probe: same run with `Stack 262144` typed first in the bare CLI).
