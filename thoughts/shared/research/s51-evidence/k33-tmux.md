---
date: 2026-11-02
topic: TMUX-PANE-CONSOLE and K33 phtmux
status: fixed (tmux-amiga branch s51-pane-console, not merged)
---

# TMUX-PANE-CONSOLE: native commands in a tmux pane print nowhere

## Symptom
In a tmux pane, C:Type, C:List, C:Version run by vsh printed nothing; vsh
builtins and ixemul programs (cat, echo) printed. `>*`, `>/dev/tty` and
`>CONSOLE:` went to the outer XCON window tmux was started from.

## Evidence (rig 86, before the fix)
- tests/amiga/inwho in a pane: `in=4118f97/... out=41162c1/... clin=41162c1
  clout=41162c1 ... console=103b0af4:XCON`: vsh's Input() and Output() are
  the same DOS FileHandle on the pty slave; pr_ConsoleTask is the outer XCON.
- wasabi snoop: C:Type and C:Version run and return 0, no Open("*"): they
  write to Output().
- A probe in the pane: unbuffered `Write(Output())` shows; `FPuts(Output())`
  + Flush and `VPrintf` + Flush are lost. Output()'s DOS buffer was in read
  state: `pos=0 end=196`.

## Root cause
tmux-amiga amiga/vspawn.c opened the pane tty once and made fd 1 and 2 with
dup(0). ixemul's execve hands a native command u_ofile[0]->f_fh and
u_ofile[1]->f_fh as pr_CIS / pr_COS: dup'd fds share one FileHandle, so vsh
(native, the pane's shell) got Input() == Output(). vsh's buffered line reads
leave that shared handle's buffer in read state, and every DOS buffered write
on it (FPuts, VPrintf, FWrite: what Type, List, Version use) is dropped. A
Shell window gives each stream its own handle. pr_ConsoleTask was never set,
so `*` and CONSOLE: kept the console of the process that started tmux.

## Fix (tmux-amiga, branch s51-pane-console)
- amiga/vspawn.c: open the tty once per stream (fd 0, 1, 2), no dup.
- amiga/console.c (new): amiga_console_from_fd(0) sets the pane process's
  console task to the slave handle's port (SetConsoleTask). Own file because
  proto/dos.h and ixemul's sys/time.h both define struct timeval.

## Proof (rig 86, after the fix)
- Pane running `inwho; C:Type; C:List; C:Version; echo S-STAR >*`: screen
  shows the file lines, `kd.txt / 1 file - 2 blocks used`,
  `Kickstart 40.68, Workbench 40.42`, `S-STAR`. inwho: in and out are
  distinct handles, `console=10e6fa0e:PTY`.
- `UPTERM_RIG=86 python3 tools/rig/kitty_z_rig.py <out> phtmux`: PASS (four
  boxes), shot ~/Desktop/upterm-shots/s51-k33-phtmux-pass.png.

## Regression test
No host test: the defect is DOS FileHandle buffering under ixemul, which has
no host model. The rig run above is the regression check (failed before:
Type output lost; passes after).

## Open
- ixemul execve still gives one DOS handle to fd 0 and fd 1 when they are
  dup'd. Any other ixemul parent that dup's a tty and execs vsh (GNU screen,
  ptyrun) can hit the same lost output. Not changed here (ixemul repo out of
  scope for this task).
