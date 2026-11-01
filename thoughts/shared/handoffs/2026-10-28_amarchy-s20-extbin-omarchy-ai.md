---
date: 2026-10-28
session: Amarchy s20
status: handoff
---

# Amarchy s20 handoff

## Done (all on origin main)
- RIG-NO-THEMES: 98e70d23 + d7d64cf8; rig 7 confirmed 23 themes + JetBrainsMono.ttf.
- UHC-EXTBIN-LIVE-PATH: config/cmdpath.h shared path copy, WBPath tool, Amarchy copies the Workbench path
  (07f71354, merge 3d4e3822); WBPath %ld fix 04dcdc21. Rig 7 PASS, evidence
  thoughts/shared/research/2026-10-28-uhc-extbin-live-path-rig7.md.
- OMARCHY-AI plan: thoughts/shared/plans/2026-10-28-omarchy-ai.md (04dcdc21).

## Next
1. OMARCHY-AI implementation, plan order A, F, B, then E, D, C. A is small: TAB_PI + MENU_PI beside
   TAB_CLAUDE (handler/vtcon_handler.c:6039, 6223), `ai-agent` pref picks what super+l opens, Ask AI row.
   F: theme sync into agent colours (check tui.c colour config first) + Agents panel menu.
   One sonnet agent per letter in a worktree; owner rule max 3 agents.
2. Open checks before B and D: C:Claude skills search path; whether "B5" (C:Claude on shared llm/) landed.
3. C1 crash research doc needs owner review before any watcher code.

## Open, unrelated
- Monocle raise on activation: owner manual test.
- Pre-existing test failures: 3 EveryAddOn (test_dist_addons.py), WasabiDaemonOption (wasabi rev).
- make dist-base needs `make kit-stage` in upterm-ports first (SOURCES.txt).
- Rows not taken: AMARCHY-PLUGINS, TMUX-RESET-R5, INSTALLER-CLICK-SPEED (rig 9 is vtcon-05's).
