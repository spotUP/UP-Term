---
date: 2026-10-12
topic: amiga-pi queue run after P11 (Q1-Q21)
tags: [amiga-pi, parity, pi-1.1.0, handoff]
status: final
---

# Handoff: queue run after P11

## Task(s)
Ledger thoughts/shared/plans/2026-10-07-amiga-pi.progress.md, section "## Queue run 2026-10-12 (after P11)": Q1-Q12 and Q15-Q20 done, merged, gated and pushed (PI d81da82, VT main). Open: Q13, Q14 (owner rig checks on rig 3), Q21.

## Critical References
- Ledger sections "## Queue run 2026-10-12 (after P11)" and "## Merged 2026-10-12 queue run" (the open list there).
- Rig 3 (UPTERM_RIG=3) holds the current build; agent dirs VTC:pichk (default), VTC:pichk0 (outputPad 0), session VTC:pichk/took.jsonl (copy of amiga-pi tests/pi/tui/took_session.jsonl).

## Learnings
- AmigaDOS Shell ignores `2>`: capture the error stream through VTC:vsh with FailAt 21 (rig step 14e).
- typewin.click_in does not reliably move focus between several UP-Term windows, and keys typed right after an exit drop characters ('V', '/'). Open one window per check, or let the owner type.
- bashdiff vars/seconds flakes under load (~44): re-run the probe alone; a failed gate skips `bashdiff.py --leak`, run it by hand.
- pi 1.1.0 prints nothing for an unknown --tools name; argument errors are "Error: <msg>" on stderr, rc 1.

## Next Steps
1. Owner rig results for Q13/Q14 (steps in the ledger Q13/Q14 lines); fix any FAIL.
2. Q21 (--thinking bad is a warning in pi).
3. Open items under "## Merged 2026-10-12 queue run": bash tool_execution_update per chunk (chunk callback in C:Claude's bash), openai-completions cache_write_tokens, MCP numbers as JS prints them, ctrl+o expand of summaries, extension message renderers.
4. Phases I, J, L, M, O.

## Other Notes
Flow that worked: checklist in the ledger first, one Sonnet agent per batch in a feature/ worktree, Opus check before a no-ff merge, merged heads gated once, worktree removed in the same step. Owner allowed 3 agents for this run only.
