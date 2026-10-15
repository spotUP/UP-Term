---
date: 2026-10-15
topic: finish every open plan item except the hardware tests
tags: [plan, open-items, vtcon, ports, rig]
status: draft
---

# Finish all open plan items except the hardware tests

<!-- statusline: no-rollup (this plan re-lists the open items of other plans; the footer counts them there) -->
Evidence base: `thoughts/shared/research/2026-10-15_plan-audit.md` (item by item, with commits). Counts
used here come from the plan files' structure, not a single grep: 80 open boxes in 11 plans on 2026-10-15
(vtcon 28, unix-tool-ports 20, replay-hardware-tests 12, ledger 9, a2-progress 4, amiga-pi 2, five singles).

Done = every item in "In scope" below is ticked in its own plan with a commit and a rig or host test that
drives the product entry point and a sentinel (REACHABILITY_PROTOCOL). A partial run is reported as
"N of M, K open", never "done". Scope cuts are the owner's call.

## Out of scope (the owner's hardware, or a third party): not planned here

Real boards and machines: replay-hardware-tests H2-H14 (12), O6 (real AmiSSL login per provider),
V4 and W3 (A1200 manual checks), W50 (Replay freeze), T1 pre-check 2 and W7 on real hardware,
NL22 (real Amiga), a1 A1.4 hardware part (A1200), a2 1.8 and 5.5 (A1200/030/060 timings on real boards).
Third parties: X2 (ixemul upstream, waits for the maintainer's reply, branch `feature/x2-vtcon-1`),
W54 (tmux over eriQue's auxcon, not released). These stay open in their plans; this plan only keeps the
branches current. Rig 1 is the owner's machine: never used.

## Rules for every phase (decisions already made, not re-litigated)

- At most 2 agents at once; one rig per agent (rigs 2 and 3, `rig.py start --max`, 060); stop the rig at the
  end; no CPU burners; no `git stash`; WIP commits; worktrees `build/wt-<name>` removed in the same task.
- Models: sonnet writes and runs; haiku runs tests and reads screenshots (text answer only, the main session
  never sees an image); opus only for the root-cause hunt (FP5.7) and plan reviews.
- A box is ticked only with the commit hash and the test name in the note. Owner "look" items (G2, G3, D1, P7)
  get an objective criterion (pixel or text read by a haiku subagent). The agent ticks the box only when that
  criterion passes; the owner's eyes are collected in one list at the end of the phase ("owner visual
  sign-off"), not a blocker for the next phase.
- Run only the affected subset (`make test ONLY=<suite>`, `install_rig.py --only <case>`); a full gate only
  to land (kit content changes: `make dist` + `make dist-check`).
- Each fix ships a regression test that fails before and passes after (revert-check it).
- Ledger first: every phase appends its checklist and results to `thoughts/shared/plans/2026-10-07-amiga-pi.progress.md`
  under a new `## Finish run 2026-10-15` heading before the first edit; tick there as items complete.
- After each phase: handoff file `thoughts/shared/handoffs/YYYY-MM-DD_finish-phase-N.md`, then `/clear`.

## Phase 0: evidence and housekeeping (1 agent, sonnet; small)

- [x] FP0.1 X1: find the stray `PI_DEBUG_LOG`/`debug_log` hunk in `~/Code/UP-Term/amiga-pi` worktree `build/wt-u2`
  (`git -C ... status`, `git diff`); if it is not committed work: report it, remove only after the owner says
  (worktree of another session). Ledger line 1137.
- [ ] FP0.2 W47: re-run `tools/rig/resize_rig.py` on rig 2 and quote the PASS line
  (tmux 47x10 to 72x19, nvim 47x11 to 72x20). The PASS exists only in commit 54de76b3's message today.
- [ ] FP0.3 W6: read the S1 race number for main (`tools/rig` race script; CCON 1.2.8b9 was 30.68 s at 5f2dc765),
  run it on rig 2 once, record main's number in the vtcon plan row.
- [x] FP0.4 P12 close step: "cut_cases oracle reproducible" (rides with U3 part 2): run the oracle twice, compare.
- [x] FP0.5 Ledger headings: "Queue run (after P11) 12 of 14" has 19 ticked: recount by box range and fix.
- [ ] FP0.6 Coreutils 3.7: `../upterm-ports/pkgs/coreutils/recipe.mk` (9.12, GPG-verified) is untracked work of
  another session: do not touch; ask the owner once who commits it (collected under Blocked on me).

Verification: quoted rig lines and commit hashes. Gate: none (docs plus rig runs).

## Phase 1: rig checks the agents can run (emulator, no real board) (1 agent, sonnet + haiku screenshot reader)

- [x] FP1.1 (done: 5e24ecc1 amigapi_rig step 13e PASS, last footer cell pixel-equal to the first 'm' of medium, flush right at 63 columns, 2026-10-15) G2: footer right edge `claude-opus-5-5 . medium` not clipped: rig 3, pixel read of the last 8 columns
  of the footer row; criterion: the last glyph column inside the window width (golden `start_w64` family).
- [x] FP1.2 (done: 5e24ecc1 amigapi_rig step 13f PASS, header whole in a 63-column window with outputPad 0, 2026-10-15) G3: `outputPad 0` header fully inside the window: rig 3 run, text read by the haiku reader.
- [x] FP1.3 P2 (done 2026-10-15: host test custom_headers_reach_the_request 24/24, revert-check FAILED without the fixture header line; rig: amigapi_rig step 15 13 of 13 with amiga-pi 7d46d16, run by the Opus agent on rig 2; the P2 crash was the 32 KB stack, not the header path): `SetEnv SAVE ANTHROPIC_CUSTOM_HEADERS` path: test through the top-level entry point against
  `tools/claude_fixture.py` extended to record request headers; criterion: header present, no 400, workspace id
  header absent unless set. Regression test name: `custom_headers_reach_the_request`.
- [ ] FP1.4 a3 T13: Claude TUI on the rig against `tools/claude_fixture.py` (conversation, Ctrl+C). The "then with
  a real key" half is the owner's (real key): note it, do not tick the real-key half.
- [ ] FP1.5 a2 3.8 and 4.8: conversation on the rig at 77x20 + Ctrl+C; tool checks on the rig (fixture).
- [ ] FP1.6 highlight-markdown R1: the manual rig checks listed in its plan, by criterion.
- [ ] FP1.7 D1: demo menu to tab to tour to tab closes (rig 3), text/pixel criterion.
- [ ] FP1.8 5.2 telnet: `C:uptelnet` + vshrc `telnet()` rig run against `tools/uptelnetd.py` locally (rig half of A1.4).
- [ ] FP1.9 W-T8: re-measure the W-T4 numbers on a quiet Mac (`uptime` load < 5, no other sessions' builds;
  schedule when the machine is idle; ask the owner to say "quiet" when ready: this one only needs that word).
- [ ] FP1.10 P7: write the objective part of the iTerm2 cell-level comparison (what differs cell by cell, one
  table); the verdict "good enough" stays the owner's.

Owner visual sign-off list (output of the phase): G2, G3, D1, P7, R1 screenshots saved under
`thoughts/shared/research/` as text descriptions; no image reaches the main session.

## Phase 2: RTG true colour without owning pens (1 agent, sonnet; medium)

- [ ] FP2.1 W-T13: draw true colour on RTG with `FillPixelArray` for fills (cause found: 24-bit WB has a 32-entry
  map and 0 free pens, ledger 1293). Files: `render/amiga_render.c` and its host test `tests/test_render.c`
  (pen allocation path vs RTG path, sentinel proves the RTG path ran).
- [ ] FP2.2 W-T11: the split-screen scene whose text scroll wrong colours: rig 2 full demo run after FP2.1;
  criterion: pixel reads of the text rows equal the palette colours (haiku reader, text answer).

## Phase 3: vtcon.md not-started and in-progress items (1 agent at a time; each its own branch, 1 PR-sized commit set)

Order (cheapest evidence first):
- [x] FP3.1 Q2: Neovim `tui_spec` replay (host test under `tests/`, `git log --grep tui_spec` is empty). (done: 50887859, make test ONLY=xterm neovim_tui_streams_land_where_libvterm_puts_them, make test-ref ONLY=nvim 6 streams 0 failed, 2026-10-15; Q2 rig session part stays open)
- [ ] FP3.2 RT1/W8: phase profile of retro32-term against ibmcon (the plan's own measurement; rig, one table).
- [ ] FP3.3 W7: A500 (68000) rig config and the speed measurement the plan lists (emulator config, not a board).
- [ ] FP3.4 S1: the asm hot-path part (S2 perf is merged at 0912d65); measure before and after on rig 2.
- [ ] FP3.5 W46: remaining commands of "setup as / commands" (468f1b03 lists what stays open).
- [ ] FP3.6 W27: CCON techniques rows 5, 7, 10, 12 (research doc exists).
- [ ] FP3.7 P3: planar fast path cycle-exact comparison (`7ecdb88`); emulator cycle counter as oracle.
- [ ] FP3.8 R1/W34: UP-Term in ROM: fix `InternalLoadSeg failed (ABI)` (b3e8aea9); boot proof in fs-uae with the
  ROM image; branch `feature/r1-rom` belongs to another session: coordinate first (list under Blocked on me).
- [ ] FP3.9 W10: CC1 blitter overlap and CC2 sprite cursor proof with `tools/rig/cc_rig.py` (exists only on branch `feature/cc1-cc2`, not on main; emulator); branch
  `feature/cc1-cc2` belongs to another session: same coordination.

## Phase 4: unix-tool-ports.md (1 agent, sonnet; per tool: recipe, package, rig test, kit entry)

Per tool the acceptance is the plan's own row plus: package in `../upterm-ports/pkgs/<tool>/`, `make dist` lists
it, `install_rig.py --only <case>` installs and runs it. Batches by size (decision: smallest first):
- [ ] FP4.1 batch A: 4.6 bc, 4.10 ed, 4.9 column/iconv, 4.11 ctags, 4.7 jq.
- [ ] FP4.2 batch B: 4.5 joe, 4.2 mg, 5.1 nc, 5.9 tldr, 5.3 wget as a vshrc function (curl is in `dist/net`).
- [ ] FP4.3 batch C: 4.8 gawk, 5.5 make, 4.4 Lua (pi has its own: reuse before building a second one).
- [ ] FP4.4 batch D: 4.1 vim, 5.7 rsync, 5.6 git, 5.8 perl.
- [ ] FP4.5 5.4 BebboSSH patches: K6 (BebboSSH 1.46.1, VT 4ae821c) is related; decide from the plan row whether 5.4 is
  already covered, tick with that evidence or do the remaining patches.
- [ ] FP4.6 3.7 coreutils 9.12: only after FP0.6 (owner says who commits `recipe.mk`).

## Phase 5: large projects (each starts with its own research + plan, per the workflow; code only after approval)

W39 gcc 16 string-object hang is a root-cause hunt (opus subagent, bounded: stop and report after 10 tool
calls without a foothold, then the owner decides). The rest are products; "finish" is a scope decision, so
each gets Phase 1-2 documents first (`thoughts/shared/research/` and `plans/`), and the owner approves each
plan before any code. This is the one place this plan stops for the owner.
- [ ] FP5.1 W16 MicroPython port. [ ] FP5.2 W19 TUI prefs app (prefs/ has GUI files only). [ ] FP5.3 W38 tiny LLM (marked LAST).
- [ ] FP5.4 W40 CPython asm (marked future). [ ] FP5.5 W55 native multiplexer. [ ] FP5.6 W56 browser thin client (marked LATER).
- [ ] FP5.7 W39 gcc 16 hang hunt (ixstackext 35bee4a2 is the state).

## Order, size, cost checkpoint

Phase 0 and 1: one sonnet agent each, a few hours of wall clock, light (the rig is the load). Phase 2: medium.
Phase 3: nine independent items, about one agent-session each. Phase 4: 18 ports, large (batches A and B are
the cheap ones; vim, git and perl are the big ones). Phase 5: research documents only until approved.
Order: 0, 1, 2, 3, 4, 5. Phases 3 and 4 do not share files: they can run as the two agents at the same time.
Checkpoint (the cost rule): ask before starting Phase 3, Phase 4 and each Phase 5 product; Phases 0 to 2 start
on the owner's word.

## Verification (separate from manual)

Automated: per item the named test; landing gate `make dist` + `make dist-check` when kit content changes;
`make test ONLY=<suite>` for the touched suites. Manual (owner): the visual sign-off list of Phase 1, the
real-key halves (FP1.4), approvals of Phase 5 plans, answers on FP0.6, FP3.8, FP3.9 (other sessions' branches).
