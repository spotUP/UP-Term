---
date: 2026-10-12
topic: amiga-pi Phase H, follow-ups, UP-Term rig rendering, PH-S, TUI parity merged; next pi 1.1.0
tags: [handoff, amiga-pi, vtcon, upterm]
status: final
---

# Start here (2026-10-12, night)

Supersedes 2026-10-12_px-n-merged.md for state. Nothing is pushed; every merge is on local main.
Ledger: `thoughts/shared/plans/2026-10-07-amiga-pi.progress.md`, sections "## Phase H", "## Queue after Phase H",
"## PH-S". Trust the ledger over this file where they differ.

## Merged to local main today
- Phase H 16 of 16 (print/json/rpc modes, 33 RPC commands, extension UI over RPC, piapi.h), Opus audit 5 blockers.
- HF1-HF6: Amiga diag stream, retry cap and overflow exclusion, reachability through the host stdin reader,
  LS_PENDING, rig step 14, context-overflow recovery as agent-session.ts (Opus check fixed the allowance reset
  and the pre-prompt check).
- UT1-UT7: rig `UP-Term:` assign at boot (VTC:upterm), glyph_rig.py pixel check, screen_text clears its
  selection and asserts no grey, rig 3 RTG depth 24, selection dropped on write (xterm), Unifont before
  stand-ins (W33 reversed), pi colour depth like pi-tui.
- TM2: window title `π - <session> - <dir>` (pi_dir_name cuts at an Amiga volume's ':').
- PH-S: pi 1.0.4 message fields; source pinned at ~/Code/amiga-pi/vendor/pi-src (v1.0.4, 7c10bd43).
- TM1/TM3/TM4/TM6 (PI a8e073e, VT bdacf0d; merged heads gated: make test and make amiga rc 0 in both): tuiMode fullscreen (default) + regular, resume
  line, editor placement, no notice in pi's look, panel backgrounds via theme tokens, pi-tui's rgbToAnsi256.
  Goldens from pi 1.0.4 at ~/Code/amiga-pi/vendor/pi-1.0.4 (tui_gen.py, PI_BIN overrides).

## Owner decisions today
- Follow pi's newest message format (done as PH-S for 1.0.4).
- UP-Term drops a selection like xterm; Unifont glyph first (em dash).
- **Move the parity target to pi 1.1.0 now** (owner, 2026-10-12 evening; /opt/homebrew pi is 1.1.0).

## Next, in order
1. Phase P11 -- pi 1.1.0: re-pin vendor/pi-src to tag v1.1.0, install the oracle in vendor/pi-1.1.0 and point
   tui_gen.py and rpc_gen.py at it, diff 1.0.4..1.1.0 (durationMs on messages and tool_execution_end, the new
   classifier api, docs cli/json/models/providers/settings), implement, regenerate every golden, re-check masks.
   Checklist into the ledger first. Sonnet implements, Opus checks.
2. Open items in the ledger queue: TM5 (π in Amiga titles: Latin-1 stand-in, look on the rig), HF7 (Amiga diag
   rig check), HF6-R2, PHS7 (usage.reasoning, allowedFallbackModels pricing, MCP structuredContent), custom/
   compaction/branch-summary message components (customMessageBg), live Elapsed rows, tool-box header colour
   when tools end out of order, --session-dir in the resume line.
3. Rig checks for the owner (below).
4. Then phases I, J, L, M, O.

## Owner checks open
- Rig 3 TM1: `UPTERM_RIG=3 python3 -I tools/rig/rig.py start --max`, then step 13 or a manual
  `VTC:amiga-pi` in an UP-Term window: alternate screen entered and cleared at start; PageUp/PageDown/Ctrl+End
  scroll; on exit the transcript and `To resume this session: amiga-pi --session <id>` land on the main screen.
  Also `fullscreenExitOutput: "resume-hint"` and `--tui-mode regular`.
- The window title's π on the rig (TM5).
- Three claude.ai connectors to disable by hand: `/mcp disable claude.ai Gmail`, `... Google Calendar`,
  `... Google Drive` (from /doctor).
- Commit `~/Code/Up_Rough_Demo_System/.claude/skills/port-amiga-effect/` in that repo (moved by /doctor).

## Learnings
- A golden generator must use a pinned pi, never /opt/homebrew's: the owner's `pi update` silently moved it
  to 1.1.0 mid-phase.
- Rig scripts must activate the target window by title before typing (typewin require_active); a stray window
  swallowed step 13's keys once.
- An Amiga device root (VTC:) is not a directory for dd_dir; the UP-Term: assign points at VTC:upterm.
- Two full `make test` runs at once on one machine time out bashdiff posix/special_errors; run gates one at a time.
- vtcon worktrees need UPTERM_ROOT=/Users/spot/Code and a `vendor` symlink.
- pi_llm prints 32 "[ERROR] cannot open .../*.headers" lines and still passes: noise to check (fixture or test).
- Disk ~8 GB free; worktree pairs ~1 GB.
