---
date: 2026-10-17
topic: finish-open-plans run, session 7
ledger: thoughts/shared/plans/2026-10-15-finish-open-plans.md
previous: thoughts/shared/handoffs/2026-10-16_finish-run-session-6.md
---

# Finish-open-plans run, session 7

Ledger is the source of truth; grep `^- \[ \]`. vtcon main is ahead of origin, not pushed.

## Done this session

- FP4.1 batch A and FP4.2 batch B pass on rig 2 (ledger ac8169c6, 40b9452c; joe man fix in upterm-ports d4bf289).
- ASM1 painter profile: prof68k `build-paint` / `run-paint` (ba9f4d1a). Plane loop 70-76 %, C glue 21-25 %; short-run per-call glue is the ASM1 candidate. Row composition not profiled (needs graphics.library).
- FP1.10 iTerm2 cell comparison (4091749c, 73c916c2): engine matches iTerm2 on text and attributes in screen, tmux and chat-log scenarios. The first tmux mismatch was a harness ONLCR artifact, fixed with `stty -opost` plus a pty test. Owner verdict pending.
- Cursor pass split (dfc8f936): on 060 the pass is 0.07 ms/frame; the 2.7 ms figure came from stock 020 and is not comparable.

## Open, in order

1. Owner: FP1.10 "good enough" verdict.
2. Owner: cursor pass, re-measure on 020 (needs the OS3.2 tree that conbench_rig.py expects; not found on this Mac) or park.
3. Owner: approve FP4.4 batch D (vim, rsync, git, perl; no recipes yet; one sonnet rig agent on rig 2, large).
4. FP1.9 when the Mac is quiet (load < 5).
5. Owner-blocked: FP3.9, FP3.8, FP4.7, FP0.6. Phase 5 needs a cost checkpoint and an approved plan.

## Gotchas

- iTerm2 API: use build/iterm2-venv; wrap awaits in timeouts, run in the foreground. A hung connection stalled one agent for 2.7 h.
- Two agents in one checkout must not both switch branches: give rig/code agents their own worktree.
- phase_rig/conbench default flags are `--stock --os32`; for rig 2 runs set `UPTERM_RIG_FLAGS=--max`.
