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

## Implementation findings (2026-10-29, bar and agents panel)

Built: the Amarchy bar (roadmap phase 2, R2.4) with one segment, "Agents", at the right end, and the agents panel it opens.

- The panel rows live in `dist/keymap` as the `agents.*` group (`agents.claude`, `agents.pi`, `agents.launch`, `agents.review`, `agents.tests`, `agents.explain`), with icons in `dist/menu-icons`. The root menu now shows an "Agents" group, so the root has 13 groups (tests/test_keymap.c updated from 12).
- The geometry is not in a separate `agents_panel_core.c` as first proposed. The anchored layout is in `amarchy/menu_geom.c` (`mg_screen.anchored`, `ax`, `ay`; the menu hangs upward from the bar and is clamped on screen) and the segment list, hit-test and click map are in `amarchy/bar_core.c`. The anchor travels through `act_menu_open_at`, `ms_open`, the `menu_open.c` message and `gui_open`.
- Left click on the segment opens the panel (`BAR_ACT_PANEL`). Right click fires the `MENU_AGENT` request through `upreq_send`, the same action as super+l (`BAR_ACT_LAUNCH_AGENT`).
- Part E (limit meters and token use) is a named slot only: `BAR_AGENT_LIMITS_ROW` = `agents.limits`. No row and no numbers exist.
- STATUS reply gained `bar`, `draws`, `panels`, `launches`.

Tests: `make test ONLY=bar` (44 checks), `ONLY=keymap` (14995), `ONLY=menusrv` (55). Revert proof: `bar_click` panel branch returning NONE and the anchored layout branch disabled make `bar` fail (6 checks); removing the `agents.*` rows makes `keymap` fail (5 checks); sending a copy of the request with `anchored = 0` in `ms_open` makes `menusrv` fail (`g_sent.anchored = 0, want 1`).

Open:
- `tools/rig/amarchy_bar_rig.py` is written and NOT RUN (rig 7 was held by another agent). The Amiga-side `amarchy/bar.c` has only been cross-compiled, never executed. The right-button number (1) in the rig script is unverified.
- The cold-spawn menu path (`srv_spawn_cold` to `act_run`) ignores the anchor and opens the panel centred.
- Segments for clock, RAM, CPU, network and workspace, `bar.conf`, and an on/off switch (`bar_usable`) are not built. The bar is on by default.
- Theme pens are taken once at open; the bar does not follow a live theme switch.
- The right-click launch uses `upreq_send`, which blocks the commodity loop until the handler answers, and gives NOPORT (no tab) when no UP-Term window exists.
- No plan items are ticked.
