---
title: RL17 Installer recheck (default case, OS 3.2)
status: BLOCKED, not run
---

Commit under test: origin/main eb17a7e4 (worktree branch worktree-agent-a57f03ea92143ba93).

Result: not run. No rig was started (91 was free; nothing touched).

Block: `make dist` fails in dist-base:
`dist: /Users/spot/Code/UP-Term/upterm-ports/build/kit/userland was staged before the stamp: make kit-stage in upterm-ports`.
- build/kit/userland has no STAGED-FROM stamp.
- The ports checkout is on branch port/htop (not main) with untracked worktrees; the stamp check
  (Makefile PORTS_KIT_FRESH) also requires HEAD to be on ports origin/main, so restaging needs a branch change in
  another repository that other sessions use.
- The main checkout's build/dist/up-term (built 2026-11-02 12:54) is stale: its Install differs from
  dist/Install.installer at origin/main (cmp differs at byte 2332), and 89 commits landed since 12:50.
  Using it would not test current origin/main.

| check | verdict |
|---|---|
| 4,5,8,10,11,30 | not run |

Needed from the owner: allow `make kit-stage` in upterm-ports (on main), or name a ports commit/kit to use.
Then: `make wasabi dist rig-helpers`, and
`UPTERM_RIG=91 UPTERM_RIG_FLAGS="--os 32" python3 -I tools/rig/installer_rig.py ONLY default --purpose "Amarchy session 31: Installer recheck"`
(installer_rig.py has no --help; it takes ONLY/FAILED/FORCE/--purpose; --os 32 goes through UPTERM_RIG_FLAGS).
