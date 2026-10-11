---
date: 2026-10-11
topic: Budget stop - state of amiga-pi phases, kit, upupdate, amiagent 0.13.2
tags: [handoff, amiga-pi, amissl, kit, budget]
status: final
---

# Start here (2026-10-11 late, budget stop)

25% of the weekly budget went in one day (5 parallel agents, 3 on Opus, plus a long
Opus main session). Rule from now: at most 2 agents, a cost line before any extra,
Opus only for judgement (design, audits, root cause), Sonnet for implementation.

## Merged to local main (nothing pushed)

- vtcon main: amiga-pi Phase E (merge d6e323f), upupdate stack fix (32aeaa8) and its
  audit follow-ups (sanitizer-free stack driver tests/updiff_stack_drv.c bounded at 3072,
  `long __stack = 16384` in install/upupdate.c, 80000004 = illegal instruction comments),
  rig.py --stock finds amiagent's NP_StackSize by tag (554163a).
- amiga-pi main: Phase E (2dd72b1). Phase D parity 15/15 was already on main.

## Branches (WIP, not merged) - each has a pause note in its ledger

| Work | Branch / worktree | State |
|---|---|---|
| Phase F model switch | amiga-pi `phase-e` (build/wt-phase-e), vtcon `feature/amiga-pi-phase-e` | stopped; also owns E7's thinking-level restore (audit finding 3) |
| Phase G instructions/trust | amiga-pi `phase-g` (build/wt-phase-g), vtcon `feature/amiga-pi-phase-g` | stopped early |
| Phase K MCP | amiga-pi `phase-k` (build/wt-phase-k), vtcon `feature/amiga-pi-phase-k` | finishing (6 of 8 at 23:11) |
| Phase E audit fixes | amiga-pi `e-audit-fixes` (build/wt-e-audit) | finishing: (1) list reads only - done; (2) compaction trigger point as pi |
| Kit + AmiSSL | vtcon `feature/amissl-bundle` (.claude/worktrees/amissl) | stopped; upupdate fix cherry-picked; LIBS:AmiSSL probe reworked |

Read the final reports of K and the audit fixes in the session that ran them, or their
ledger sections. Each phase gets an Opus audit before merge (Phase E audit found 3 medium
bugs that the Sonnet ledger had ticked).

## Open, owner

- E11: on the Amiga `amiga-pi -p hello`, copy the session file from
  `UP-Term:var/amiga-pi/sessions/<dir>/` to the Mac, `pi --session <file>`. PASS: pi shows
  the conversation.

## Open, queued

- amiagent 0.13.2 installed in main build/rig, rig2, rig3 (old kept as amiagent-0.13.0);
  worktree rig copies still 0.13.0. 0.13.2 runs commands with pr_WindowPtr -1, so rig
  checks for "no requester" (install_rig.py:430, installer_rig.py:311) pass vacuously.
  Needed before rig 1 moves to 0.13.2: a tiny tests/amiga tool running a command with
  pr_WindowPtr 0, used by those checks.
- Phase H (print/JSON/RPC) after F; I/J after G; L, M, N, O later.
- Kit: remaining rig cases (amissl-present, novice, dest, update on 3.2.2 and 3.1), make
  test, kit to Desktop. Guest dies mid-install ~3 of 8 runs (undiagnosed).
- `make test` in a vtcon worktree needs UPTERM_ROOT=/Users/spot/Code.
