# VSH-BG-IGNORES-TERM notes (no fix made)
- shell/ (vsh) has no SIG_IGN/sigaction anywhere (grep shell/*.c): vsh itself never sets ignore masks. Its os_signal sends INT/QUIT as break bits, others via ixkill -> kill().
- On rig 9, bare `sh` is not a command; VTC:bin/sh = 63032 bytes dated 30 Sep (NOT vsh, 240068); GG:bin/sh = vsh. Earlier agent's `sh -c` target unknown: may have been the stale VTC:bin/sh.
- Repro today: `VTC:bin/sh -c "RAM:kp &"` -> pid 271665600, ixkill -0 rc0; then `C:ixkill -TERM 271665600` -> wasabid connection closed, rig dead (not "survived"). 
- `GG:bin/sh -c "RAM:kp &"` (vsh): no kp.pid appeared; then rig died again (my exec of `RAM:kp &` passed '&' as argv -> kill(0,0)); rig crashes unrelated to the vsh path.
- ixemul: siginit(kern_sig.c:430) sets defaultignmask; p_sigignore inherited via exec is the only place ignore could come from.

## s21 (rig 9, 060)
- `GG:bin/sh -c "RAM:kp &"` -> kp ("vsh job (RAM:kp)") runs; kp.pid=271568736=0x102fcf60 (wasabi ps: that task). ixkill -0 rc0; -TERM rc0; -INT rc0; second -TERM rc0. Target STILL alive each time (wasabi ps/Status), rig alive (ping ok). No requester seen.
- Control: `Run >NIL: RAM:kp` (no shell parent): ixkill -TERM -> target ends, rig alive.
- So vsh background job ignores TERM/INT (signals not delivered or ignored) but does not crash the rig with GG:bin/sh (vsh). Earlier rig deaths used VTC:bin/sh (63032 bytes, 30 Sep).
- Part B: VTC:bin/sh has no writer in current code. program_pairs (tools/rig/rig.py:1212) lists only build/amiga files + userland (no `sh`); stage_upterm rig.py:1095 writes UP-Term bin only; vsh goes to VTC:vsh and gg/bin/sh (rig.py:1284-1290). Leftover from before 695288d5 (2026-10-21) in every build/rig*/vtc/bin/sh (30 Sep, identical size). Stale drift, not checked by build_drift.
