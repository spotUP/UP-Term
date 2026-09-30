---
date: 2026-09-30
topic: UP-Term session handoff -- GNU screen finished, tmux runs, ixemul fixes, the run-shell/#() job hang
tags: [vtcon, up-term, tmux, screen, ixemul, libevent, handoff]
status: draft
---

# Session handoff, 2026-09-30

The Claude CLI was started from `/Users/spot/Code/Up_Rough_Demo_System` (the demo
system dir). All the work is in other repos, by absolute path:

| Repo | What | Branch/state |
|---|---|---|
| `~/Code/vtcon` | UP-Term: XCON:/PTY: handlers, vsh, kit, rig tools, ledger | main, committed except the items under "Uncommitted" |
| `~/Code/ixemul-vtcon` | patched ixemul 48.2 (own git) | committed except the execve probes |
| `~/Code/tmux-amiga` | tmux 3.6a + libevent 2.1.12 port (own git, new today) | committed except the vspawn probes |
| `~/Code/screen-amiga/src` | GNU screen 4.9.1 port (git root is `src/`) | committed |

Read first: `~/Code/vtcon/RULES.md`, the ledger `~/Code/vtcon/thoughts/shared/plans/2026-09-28-vtcon.md`
(sections P7.1, P7.2), memory `vtcon_owner_delegates_design_goal_is_unix_zsh_terminal.md`
(owner delegates all design decisions; goal: a Unix/zsh-level terminal).

## Task in progress (stopped mid-investigation)

**P7.2 T2.6: `tools/rig/tmux_rig.py` passes 5 of 6.** tmux runs in an XCON: window:
status line, vsh in a pane, typing/echo, the 256-colour cube exact in a pane, Ctrl-B %
split, detach and `tmux attach` all pass. **FAIL: the `#()` status job** (and
`tmux run-shell`, which hangs waiting for it).

What is known about the job hang (measured, not guessed):
- tmux runs jobs with `/bin/sh` (not default-shell). ixemul maps `/bin/sh` to `BIN:sh`;
  on the rig `BIN:` is assigned to `VTC:bin` holding a copy of vsh as `sh`
  (tmux_rig.py sets that up; the kit does not yet -- see Next steps).
- The job child (amiga/vspawn.c `amiga_vspawn`) gets through every step up to `execv`
  (probe markers in RAM:vspawn.log: child, signals, chdir, fds, close_fd, closefrom, exec),
  then sticks inside ixemul's `execve` -- the process keeps the name `tmux` (a program that
  reached SetProgramName would show `sh`), yet the tmux server keeps answering (`tmux ls`
  works), so the vfork parent was released.
- First execve probe run: RAM:exec.log did not appear at all for the job (not even the
  "resume" marker at the parent release). The second, finer probe build (markers at execve
  entry and after `__load_seg`) was built but its rig run was cut off when the Mac ran out
  of RAM. **Next: rerun exactly that** (commands below).
- The same exec works outside tmux: `VTC:ixpipeprobe D` (socketpair stdio into
  `/bin/sh -c "VTC:forkprobe ..."`) and `E 1..31` (every tmux child step, alone and all
  together) all pass. So it is something in the tmux server process's context.
- Also: after a stuck job the whole rig later hung (amiagent timed out) -- consistent with
  a child stuck inside a Forbid/Disable section of execve. Reboot the rig after each try.

Commands to resume (rig must be up; `python3 tools/rig/rig.py start` from ~/Code/vtcon):
```
cd ~/Code/vtcon/tools/rig && python3 -c "
import ixpty_rig; from install_rig import run; import time
ixpty_rig.use_ixemul()
run('Mount PTY: FROM VTC:ptymount'); run('Mount IXPIPE: FROM VTC:ixpipemount'); run('Assign BIN: VTC:bin')
print(run('VTC:vsh -c \"VTC:tmux -f /VTC/tmux.conf new -d 2>&1\"', 60))
run('Delete RAM:exec.log RAM:rs.txt RAM:vspawn.log QUIET')
print(run('VTC:vsh -c \"VTC:tmux run-shell -b \'echo builtin-ran >RAM:rs.txt\' 2>&1\"', 30))
time.sleep(6)
print(run('Type RAM:exec.log')[1]); print(run('Type RAM:vspawn.log')[1]); print(run('Type RAM:rs.txt'))"
```
`use_ixemul()` copies `~/Code/ixemul-vtcon/build295/library/68020/68881/amigaos/ixemul.library`
(currently the build WITH the xstep probes) to VTC:ixp6 and puts it first in LIBS:.
Copy `~/Code/tmux-amiga/build/tmux-bin` to `build/rig/vtc/tmux` first if tmux was rebuilt.

## Uncommitted (must be cleaned before committing)

- `~/Code/ixemul-vtcon/stdlib/execve.c`: TEMPORARY `xstep()` probe (writes RAM:exec.log via
  DOS Open/Seek/Write) at execve entry, after __load_seg, native sp, resume, entry,
  basename, setprogramname, compatible_startup, io, runcommand. Remove all `xstep` lines and
  the function, rebuild (`sh docker/build.sh`), before any commit.
- `~/Code/tmux-amiga/amiga/vspawn.c`: TEMPORARY `STEP()` macro (RAM:vspawn.log). Remove.
- `~/Code/vtcon/tools/rig/tmux_rig.py`: new, the T2.6 check (6 checks; sets up BIN:sh).
  Commit once it passes.
- `~/Code/vtcon/tests/amiga/ixpipeprobe.c`: cases D (socketpair stdio into /bin/sh -c
  VTC:forkprobe) and E (tmux's child steps as bits 1,2,4,8,16). Worth committing (it is the
  job-path regression probe); the D command currently runs VTC:forkprobe.
- `~/Code/vtcon/tests/streams/*.bin` (9 files) show modified: NOT from this session; leave.
- `~/Code/vtcon/tests/amiga/mallocbench.c` untracked, older; leave.

## Done this session (committed)

vtcon: 5f7a811 screen_rig (256 colours in screen), e9bf823 Uninstall restores TERMINFO,
5a7814c, b601d8d, 3ef11c0 vsh -c / script mode / $-, f133881 kit IXPIPE:, f816cb2 vsh
foreground reads a piped stdin, 1bb7293 ixpipe_rig, f49e0ef ixemul 80.1 decision (stay on
48.2), 1f64cad engine/vtwidth.h, c24b051, 87636f4 terminal answers are termios input +
ACTION_VTCON_NREAD (FIONREAD), 337c6d1 rig tests + rig.py amiagent copy fix, bef4233
console.device research+plan (by a helper agent), 899252c ledger.

ixemul-vtcon: 8836946 unmounted IXPIPE: no requester, 5c7bd59 socketpair + socklen_t +
libixcompat, bb620d4 SDK for gcc 6 (va_start via __builtin_next_arg, C99 snprintf size 0,
headers wchar/libgen/langinfo, POSIX constants, libixcompat C99 functions, ix_self_path),
78d2934 AF_UNIX (descriptors per message, reader/writer wait slots, connect ENOENT),
3073a40 select (write-only wakes, EINTR from ixnet, console stdin read/write), 9c75798
FIONREAD via NREAD, printf z/t/j/hh.

tmux-amiga: 100fdb7 pristine, e7a6dbc libevent + evprobe, 7ce59d9 tmux runs.
screen-amiga: 4c07872 backtick/printcmd/blanker/lock via vfork, 702e873 ix_self_path.

## Learnings / gotchas

- `make` rules for rig test programs do not depend on the ixemul SDK headers: after
  changing headers, delete the binary or `touch` the source (a stale ixc99 fooled me once).
- amiagent's text typing turns `[` into `(`: put bracketed text in a file and `source` it.
- vsh: exit a nested vsh with `exit`, the AmigaDOS Shell with `endcli`.
- Kill tmux sessions by exiting pane shells; `tmux kill-server` leaves the server because
  vsh (native) ignores SIGHUP (open item).
- `ixps` (an ixemul process lister) hung the rig when tmux ran; deleted, not committed.
- tmux `-vv` logging is so slow the server needs ~2 minutes to start.
- SnoopDos on the rig: `Run >NIL: SnoopDos HIDE`, then ARexx scripts VTC:snoop.rexx
  (OpenLog RAM:snoop.log + MonitorPackets) and VTC:snoopclose.rexx (CloseLog); an ARexx
  script must start with a comment line.
- Owner's host Mac ran out of RAM during the last run (rebooted); keep FS-UAE to one
  instance and stop it when idle (`python3 tools/rig/rig.py stop`).

## Next steps (in order)

1. Rerun the execve probe (above); find where the job's execve sticks; fix at the root in
   ixemul or amiga/vspawn.c; add a rig regression (extend ixpipeprobe or tmux_rig).
2. Remove both probes, rebuild ixemul + tmux, rerun: tmux_rig.py (6/6), screen_rig.py,
   ixpty_rig.py, ixpipe_rig.py (fresh boot), `VTC:ixsock pair`/`pairwake`, `VTC:ixc99`,
   `VTC:ixreply` (in an XCON window). Commit per repo.
3. Kit (P7.2 T2.6 rest): C:tmux, terminfo tmux/tmux-256color (or keep screen-256color),
   a system tmux.conf (ENV:tmux.conf is in TMUX_CONF), `/bin/sh` = vsh with a BIN: assign
   only when none exists (GG installs have their own), Uninstall removes it; install_rig.
4. vsh: react to SIGHUP-equivalent so `tmux kill-server` ends panes; Ctrl-Z in pipelines.
5. ixemul size_t (unsigned long) vs gcc __SIZE_TYPE__ (unsigned int): -Wformat noise.
6. Then Phase D per `thoughts/shared/plans/2026-09-30-console-device.md`; irssi and Neovim last
   (owner's order).

## Owner-facing state

The owner watched tmux start working on the rig ("it echoed"). They asked for an agent on
the todos (done: console.device research/plan). The rig is stopped (host reboot).
