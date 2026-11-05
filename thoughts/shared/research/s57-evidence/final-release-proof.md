---
date: 2026-11-05
topic: S57 final Amarchy release proof on main 500984a1
status: BLOCKED at item 1 (fresh-all FAIL 34, twice); items 2-5 not measurable
---
# Final release proof, rig 135 (3.2, 060 max, --idle-minutes 0)

Build: worktree at 500984a1 (contains 9f9ea2dd), `make worktree-ready`, `make amiga`,
`UPTERM_PORTS=.../upterm-ports/.claude/worktrees/kit-main make dist`.
Rig started with `--allow-stale-base` (origin/main c6d4a04a, one commit newer than 500984a1, was missing; the briefed commit was tested).
Amarchy md5 a8c7c65535b2b73cc2c9d9f9d931fd10 is identical in build/amiga, build/dist/up-term/Files, the rig's VTC: and UP-Term:bin,
and the binary is dated after desk_dos.c.

## 1. Fresh-all: FAIL (2 of 2 runs)

`INSTALLER_KEEP=1 installer_rig.py ONLY fresh-all`, both runs (the second after rig stop + clear + fresh start):
- ok 30..33 (AmiSSL), then `FAIL 34 after the reboot: the AMARCHY ARexx port answers STATUS: no port`
- then `[ABORT] fresh-all: auth failed: the Amiga took the connection but its daemon did not answer for 15 s`.
- `ami.py ping` afterwards: daemon did not answer for 30 s. The serial log shows the vsh prompt (system booted; wasabid and Amarchy dead or stuck).
- Steps 35 and 36 were never reached. Host load quiet (only rig 96 fs-uae about 112%).
- Differs from the rig 134 PASS (127b9fb7 + the fix, pre-merge). Open: whether the merge 500984a1 differs from that tree, or
  the race is timing (start-pointer-hang.md "Open").
- On-screen requester text: not readable (daemon silent, no screenshot path without it).

## 2-5: NOT MEASURED

The Amarchy menu, the ARexx port and the wasabi daemon are unavailable after the failed install, so RL15, RL19 at depth 8,
the Locale theme-pick pixels and GADGET-BLACK / UPDemo 15 could not be run. No pixel data, no shots taken.
