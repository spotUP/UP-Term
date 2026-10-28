---
date: 2026-10-29
author: spot (Amarchy s20, second half)
status: OMARCHY-AI A1-A4 + F1 on main, rig checks open
plan: thoughts/shared/plans/2026-10-28-omarchy-ai.md
ledger: thoughts/spot/todos.md rows OMARCHY-AI, AGENT-TASK-DASH, AMTILE-RIG-MONOCLE-PROBE, AMARCHY-PLUGINS
---

# Amarchy s20 handoff: OMARCHY-AI A4 merged

## Done (origin/main)
- A1-A3: `ai-agent` amconf key, TAB_PI/MENU_PI, super+l = MENU_AGENT (369c39a2, e257c842).
- F1: C:Claude colours follow the console palette (5e790e4c). The superseded pi theme
  loader is kept as thoughts/shared/research/2026-10-29-claude-pi-theme-loader.patch.
- A4: `Agent "<task>"` (install/agent.c) sends `/ask TASK` through the shared window route
  handler/upcmd.h; SLASH_ASK -> tab_command_arg(MENU_AGENT) -> tab_spawn(..., task);
  tabprog_task_cmd quotes the task with vsh_put_word (shell/vsh_quote.h, shared with
  vsh_command_line). Commits 11eb55f5, 01b01b82, f45d957c (Makefile header deps), plan tick.
  Revert proof: /ask row removed fails 6 test_slash checks; * escape removed fails
  test_tabprog.c:39. Notes: thoughts/shared/research/2026-10-29-omarchy-ai-a4.md.

## Open, in order
1. A5 + F3 rig batch (one sonnet agent, rig 7, purpose "Amarchy s21: A4/F1 agent check";
   never rigs 1, 2, 9): `Agent "list the files"` and `/ask list the files` each open a new
   tab running the agent on that task (ps/taskpath shows the quoted command line; check
   ixemul argv for amiga-pi gets the task as one word); super+l opens Claude; `/agent pi`
   then super+l opens pi; F1: C:Claude `/color orange`, switch UP-Term theme, colour follows.
2. F2 agents panel: icon at the right end of the Amarchy bar, drop-down under the right
   corner (left click panel, right click launches default agent).
3. B2/B3 Amarchy SKILL.md from source tables, two install copies (B1 research c40ed131).
4. E, D2/D3, then C (C1 research needs owner review before code).
5. AGENT-TASK-DASH (ledger): a task starting with `-` is read as an option by amiga-pi
   (args.c:187) and maybe C:Claude (cli.c:668).

## Gotchas
- Local main checkout lags origin: another session's uncommitted Makefile crashme hunk
  sits in the main tree. Commit through `git worktree add --detach <scratch>/x origin/main`.
  Never stash or discard that hunk.
- My stale edit to the plan file in the main tree is superseded by origin; overwrite with
  `git show origin/main:<path> > <path>` when fast-forwarding.
- Pre-existing failures, not ours: test_amarchy_docs.py (2), test_dist_addons.py (3),
  WasabiDaemonOption in test_dist_installer.py.
