---
date: 2026-10-15
topic: Finish run, session 2 (stack fixes, screen, prompt assigns, rig checks, unix ports)
tags: [handoff, finish-plan, rig, screen, ports, amiga-pi]
status: draft
---

# Handoff: finish run 2026-10-15, session 2

## Task(s)

Run `thoughts/shared/plans/2026-10-15-finish-open-plans.md` (finish plan) to 100%. Ledger: `thoughts/shared/plans/2026-10-07-amiga-pi.progress.md`, heading `## Finish run 2026-10-15`. Partial execution is a failed plan; the ledger is the truth, not this file.

Count at write time (grep of the plan file, `- [x] FP` / `- [ ] FP` / `- [~] FP`): 11 ticked, 24 open, 1 partial (FP1.6). Re-measure from the plan before quoting.

## Agents running at write time (check before spawning; limit 2, owner allowed a third once)

- Agent ade6c65dafeef9be5 (sonnet, rig 2): screen_rig cube half on RTG, then FP2.1/FP2.2 RTG true colour; worktree `build/wt-fp2`, branch `fix/finish-fp2`.
- Agent a9efd0a8abf896bc3 (sonnet, rig 3): FP4.2 batch B (joe, mg, nc, tldr, wget) on upterm-ports branch `fix/finish-fp4`.
- Their reports arrive only in the session that spawned them. If this session is cleared first, read their branches and the ledger instead. Stop duplicate reports with TaskStop.

## Critical references

- Finish plan: `thoughts/shared/plans/2026-10-15-finish-open-plans.md` (rules block lines 28-44; Phase 1 line 61, Phase 2 line 83, Phase 3 line 91, Phase 4 line 106, Phase 5 line 118).
- Ledger entries of this session: grep `2026-10-15 stack check`, `ROOT CAUSE`, `Rig crashes SOLVED`, `OPEN (owner report` in the progress file.
- Patch for screen: `thoughts/shared/research/2026-10-15_screen-amiga-master-stack.patch` (already applied in `../screen-amiga/src`, which is not a git repo).
- Parked column port: `thoughts/shared/research/2026-10-15_column-port-blocked/`.

## Recent changes (vtcon main, all pushed up to a8a29945; 14 commits ahead of origin at write time)

- `96c6b372` system prompt names the standard assigns (S:, C:, LIBS:, DEVS:, ENVARC:, ENV:, T:) in `claude/amiga_text.h`; goldens regenerated; test `test_system_prompt_names_standard_assigns`. amiga-pi `2b29ba1` (needed a code split in `amiga-pi/prompt.c`: 665-byte string over gcc 2.95's 509-byte limit), upterm pin `7d2f779`. Pushed.
- `cd68476c` handler: termios owner is set before tty_enter feeds typed-ahead keys (`handler/ttyown.h`); test `make test ONLY=ttyown` 18 checks. Merged `a8a29945`, pushed. Fixes the offset prompt (LF without CR) after a failed screen.
- screen-amiga patch (`__stack = 262144` plus `$STACK: 262144` cookie): applied and rig-verified (owner's scenario passes). Not committed anywhere; ships with `make dist`.
- `3bf5ea4d`, `21e0d18b` `__main__` guards for claude_rig, pf1_rig, claude_rig3, mouse_rig; test `rig_claude_import`.
- `84b339f8` W46 (a): Settings menu items Terminal type and Colour mode run `/term` and `/colors` through `slash_run`; test `settings_menu_items_run_the_typed_commands` (slash suite 283 checks). Merged.
- Rig scripts on main: `hlmd_rig.py` (3f8f0c1d), `demo_rig.py` (d5f2c5fc), `uptelnet_rig.py` (5946f257), RULES rows (cc63349a). FP1.7 and FP1.8 ticked; FP1.6 partial.
- Ledger/plan commits: `92d01490`, `2ac2d470`, `d7083b45`, `a9751cbc`, `9d17744f`, `3aae921f`, `0af8b00a`.
- upterm-ports branch `fix/finish-fp4` (HEAD 07e796b, not merged, not pushed): bc/dc 7766199, ed d5dc28c + 65ab204, iconv 78d6c68, ctags 3a1c672 + 53fb78e. Rig 3 verdicts: bc 6/6, ed 11/11, iconv 9/9, ctags 9/9. jq recipe fce93a0 is parked (reverted by 07e796b).

## Learnings

- Stack is the usual cause of Amiga crashes: vsh starts programs without a `$STACK` cookie on 16000 bytes; ixemul's vfork child ignores the shell's `Stack` value (library/vfork.c get_stack_size), only IXSTACK or the CLI default count. That is why `Stack 262144` did not help screen's backend while `IXSTACK=262144` did.
- Tool path resolution is right (`path_join`: `s:` resolves). The "show me the startup" failure was the prompt not naming assigns. C:Claude's start directory is the launching process's current dir, as in real claude; `RAM:` came from the agents' rig harness, not a bug.
- Rig 2 booted PAL because `ENVARC:Sys/screenmode.prefs` held the AGA16 file from an earlier `rig.py aga`; `rig.py rtg` gives 800x600 and unblocks the screen cube check.
- After a wasabi pin merge run `make wasabi` in main before any rig start.
- A fresh worktree has no `vendor/`; symlink main's or set `VTCON_NDK=<main>/vendor/ndk-3.2r4-Include_H`. At load 60-85 a `make test ONLY=` takes 10-30 minutes.
- Agents that stop "waiting on their own background work" re-send the same report; TaskStop them once their work is merged.

## Artifacts

Branches: vtcon `fix/finish-fp2` (worktree `build/wt-fp2`, unmerged), upterm-ports `fix/finish-fp4` (worktree `../upterm-ports-fp4`). Untracked or modified at write time: progress ledger has an uncommitted modification (an agent appending); check `git status` first.

## Next steps (ordered)

1. Collect the two agent reports (or read their branches and ledger lines). Review, then merge `fix/finish-fp2` and `fix/finish-fp4` (non-ff merges create ledger conflicts: keep both sides).
2. `make dist` and `make dist-check` once (kit content changed: bc, ed, iconv, ctags, screen patch, stack cookies, prompt). Then on a rig: "show me the startup" against the fixture should read S:Startup-Sequence.
3. FP1.6 rest: stock A1200 timing (`rig.py start --a1200`), hlp/mdp needs `less` in the kit. FP1.9 needs the owner's word "quiet". FP1.10 P7 table.
4. FP3.2, 3.3, 3.4, 3.6, 3.7 (rig-bound). FP3.5 (b) Prefs gadgets and (d) rig check. FP4.3 to FP4.6.
5. ixemul-vtcon changes for column and jq (`reallocarray`, `strtonum`, `strptime`, `isnormal`, pthread subset), if the owner allows; then jq's `execute.c:986` assertion (suspect m68k struct alignment, unverified).
6. Phase 5: recommended first, the gcc 16 hunt (Opus, the stack fixes may have changed it).
7. Push vtcon main (14 commits ahead) as `spotUP` after review.

## Other notes

- Owner decisions open: W46 (c) 16-colour mode (recommended: do not build); plan D1 text vs code for tour keys (code ends the tour on Esc, Q, Ctrl-C only); FP0.3 (AmigaOS 3.2 tree path); FP0.6 (who commits coreutils `recipe.mk`); FP3.8/FP3.9 branches `feature/r1-rom`, `feature/cc1-cc2` belong to another session; ixemul-vtcon edit permission; which Phase 5 product first; real-key half of T13.
- Owner eyeball list: colours of `hl` and `mdv` output, the demo tour's look. Input box showed stale text after the "time limit" prompt (unconfirmed, unplanned).
- Not ours: three old `claude_fixture.py` processes (pids 27697, 72299, 78876); other sessions' worktrees `feature/dctelnet-root`, `feature/demo-perf`; large scratch dirs under /private/tmp/claude-501 from other sessions.
- Rig 1 is the owner's; never use it.
