# OMARCHY-AI F2: blocked, no Amarchy bar exists (2026-10-29)

F2 needs an agents icon at the right end of the Amarchy bar. The bar is roadmap Phase 2
(thoughts/shared/plans/2026-10-26-amarchy-roadmap.md, R2.4-R2.9: bar_core.c, bar.c, bar window,
click hit-test). Checked at origin/main 18e12299: no bar*.c/.h in amarchy/, no bar_core, no bar window.
amarchy/menu*.c has no bar icon or right-corner drop-down: menus are opened by hotkey/ARexx via
menusrv_core and drawn by gui.c in their own window; menu_geom.c lays out a list rect only.

So there is no pattern to copy for "icon at right end of the bar" or "drop-down under its right corner",
and the click dispatch (left = panel, right = launch default) has no top-level entry to run through.
Building the bar (R2.1-R2.9) is a separate plan phase, not part of F2.

Proposed split once the bar lands (or if the owner wants the bar first):
1. agents_panel_core.c (portable, host-tested): row list (default agent toggle on the ai-agent amconf key,
   Launch <agent>, starter prompts, a named slot for part E meters), geometry of a drop-down anchored at the
   bar's right corner, click dispatch (left = panel, right = launch default, prompt row = task string).
2. bar segment "agents" calling it; reachability test through the bar's click entry with a sentinel.
No code written; nothing to revert.
