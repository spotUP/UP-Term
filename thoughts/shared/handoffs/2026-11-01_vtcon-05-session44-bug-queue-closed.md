---
date: 2026-11-01
session: vtcon-05 s44
branch: main
head: 6e6264f6
status: s43 bug queue closed except Q3 (parked)
---

# vtcon-05 session 44: s43 bug queue closed

Ledger: `thoughts/spot/todos.md`, section "## vtcon-05 s43 bug queue" (rows Q1-Q9). Previous handoff: `2026-11-01_vtcon-05-session43b-bug-queue.md`.

## Outcome per row

- Q1 closed: fixed by FS-UAE s43d (froze 3 of 6 runs before the fix, 0 of 20 after). Owner chose "close as fixed", no revert run.
- Q2 done: fdb0d707.
- Q3 PARKED (owner decision): pane close lives only on feature/native-splits (worktree ../vtcon-splits, 68ea2a87), about 1500 commits behind main and missing 639c4cdd. Verify with pane_close x40 after main is merged into the splits branch.
- Q4 done in s43 (fe76c6a7, 010faab9).
- Q5 closed, not a vtcon bug: the untitled centred window is the Amarchy text quick menu (amarchy/act.c:58, XCON NOBORDER/CENTRE). The DOpus blame in tools/rig/amarchy_menu_rig.py (~line 1019) is wrong. Row AM-RIG-TEXTMENU-LEFTOVER added for the Amarchy session. Findings: thoughts/spot/term-findings/rig4-untitled-windows-s44.md.
- Q6 closed: NVIM-PTYRUN-HANG fixed by FS-UAE s43d, 3 of 3 full paints on rig 4. Not tested: nvim md5 ab666756, interactive window. Findings: thoughts/spot/term-findings/nvim-ptyrun-hang-s44.md.
- Q7 closed, not a bug: the duplicate C:upgetty / C:wasabid processes are NP_Cli children that inherit the parent's command name (device/upgetty.c:195, wasabid.c:916).
- Q8 closed: NEWSHELL grey window fix f2cd54d6 PASS on the A1200 (black ground, readable prompt). Checked with an explicit CON: spec, not the Workbench Execute Command menu path. Ledger row NEWSHELL (line 1059) ticked too.
- Q9 closed: NVIM-HEAP-CORRUPT not reproduced 0 of 3 on rig 6 (start screen and `:q` only, no file edit). Reopen if a file-edit run crashes. Findings: thoughts/spot/term-findings/nvim-heap-corrupt-s44.md.

## Leftovers

- Rig-owned staging left in place: build/rig4/vtc/q6 (about 7 MB), build/rig6/vtc/nvimd (32 MB). Rigs 4 and 6 are stopped.
- fs-uae PRs #393/#394 wait for upstream review.
- Untracked thoughts/shared/research/s28-evidence/side-worktrees.md and the modified amarchy session28 handoff belong to the Amarchy session; not touched.

## Next

No queue left for vtcon-05. Next session: pick the next open vtcon rows from todos.md (grep `^- \[ \]` outside the s43 queue) or take a new owner task.
