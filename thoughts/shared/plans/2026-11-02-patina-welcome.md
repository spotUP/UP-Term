---
date: 2026-11-02
topic: PATINA-WELCOME (RL16) first-start welcome window and Help view
status: approved, owner decisions written in
ledger: thoughts/spot/todos.md rows RL16 (~3796) and s23 PATINA-WELCOME (~2980)
---

# Plan: PATINA-WELCOME (RL16)

## Findings
- Startup: patina/patina.c main() (~l.502) runs dir_resolve, amconf_migrate, style_load, keymap_load_compact(km_default_text), cx_open, note_open, ..., start_windows or wb_gate, desk_dos_start_apply (~l.560), then the Wait loop. The first-start hook goes after desk_dos_start_apply, before the loop. When g_wb_hold is set it fires from the wb_gate tick hook instead (W43: no foreign windows before the WB reset).
- amconf: keys are the enum in patina/amconf_core.h (last AM_BORDERBLANK, then AM_NKEYS); each has a row in prefs/settings_def.c (SD_PATINA, SDF_AMSTR, AO(AM_x), default). amconf_keys.c derives names and defaults. amconf_save_with writes ENV: and ENVARC:; a key at its default is not written. Hidden-key pattern: AM_SHOTDIR, AM_TILEGAP.
- No first-run hint pattern exists. note.c is a timed toast with no checkbox: wrong tool.
- Window to reuse: patina/gui.c (patina-gui process). Modes M_LIST, M_INPUT, M_MSG, M_VIEW, M_JOB; `patina-gui viewer` already shows text through mn_viewer_text and lines_split (gui_open ~l.1620). Themed, works on RTG and AGA.
- Bindings live in dist/keymap [bind] (super+space menu, super+q close, super+ctrl+shift+arrows snap, super+ctrl+shift+t tile, super+help / shift / ctrl capture). tools/gen_keymap_default.py builds km_default_text. mn_viewer_line (menu_core.c ~l.178) formats bindings. No quit-Patina key found.
- Menu: [menu] rows in dist/keymap, walked by mn_children / mn_levels (menu_tree.c). New action kinds parse in config/keymap.c and handler/keymap_act.c.
- man/patina.1: mdoc, sections NAME, SYNOPSIS, DESCRIPTION, FILES, STATUS BAR, ARexx, NOTICES, SEE ALSO.

## Single source
dist/patina.help with sections `[welcome]` and `[help]`. tools/gen_patina_help.py (style of gen_keymap_default.py) emits build/amiga/amhelp.inc for gui.c and splices a ".Sh GETTING STARTED" section into man/patina.1 between marker comments. Key names are placeholders (`{key:menu:root}`, `{key:win:snap left}`, `{key:cap:region}`, `{menu:style.theme}`) resolved at run time from the live keymap, so rebinding shows correctly.

## Checklist
Phase 1, pure core (host-testable):
- W1 patina/help_core.c/.h (C89): hc_expand(keymap, tmpl, out, cap) resolves placeholders; "(unbound)" fallback.
- W2 hc_section / hc_lines: section lookup and line split.
- W3 hc_should_welcome(prefs) / hc_mark_shown(prefs) on key AM_WELCOME.
- W4 AM_WELCOME in amconf_core.h, row in prefs/settings_def.c, any enum-count checks.
- W5 tests/test_help_core.c (expansion, unbound, rebinding, truncation, sections, default, amconf round trip); Makefile TESTS, vttest_host deps and link line, ONLY comment, PATINA_SRC; tests/test_amconf.c key default.
Phase 2, text and generator:
- W6 dist/patina.help text (welcome: super key, menu, tiling/snap, Style > Theme, capture, quit; help: deeper content).
- W7 tools/gen_patina_help.py + Makefile rule; host test help_man_sync fails when man/patina.1 drifts from the source.
Phase 3, UI:
- W8 gui.c modes `welcome` and `help`: M_VIEW text, "Don't show this again" checkbox row, "Close" and "Open Help" buttons; Space toggles, Return closes; ticked close saves AM_WELCOME through amconf_save_with (ENV + ENVARC). Help scrolls.
- W9 hc_welcome_key(ch, &ticked, &done) pure checkbox state + host tests; gui.c only draws.
- W10 Patina launches `patina-gui welcome` after desk_dos_start_apply (or from the wb_gate hook when held), via the existing launch path; __stack + $STACK cookie confirmed (make test ONLY=dist_stack).
- W11 Help row in [menu], bind line, action kind `gui:help` in config/keymap.c / keymap_act.c; test_keymap / test_menu_text rows.
- W12 man/patina.1: generated section, new key in FILES, `patina-gui welcome|help` in SYNOPSIS.
Phase 4, reachability:
- W13 tools/rig/patina_welcome_rig.py (pattern: patina_gui_rig.py), on shared rig 91: Patina prefs deleted, start Patina, window "Welcome to Patina" present, tick box, close, restart Patina, window absent, ENVARC prefs hold the key. Sentinel: window title seen via amiga_windows.
- W14 Ledger rows ticked with commit.

## Owner decisions
Owner answers 2026-11-02:
1. Window: patina-gui M_VIEW mode with a drawn checkbox row (themed), not a GadTools requester.
2. Text source: one file dist/patina.help generating both the gui text and the man page section; key names filled from the live keymap at run time.
3. Help key: super+shift+slash ("?"). super+shift+h is Tutorial, super+f1 is tab 1, super+alt is reserved for mouse emulation.
4. Quit: add a real "Quit Patina" menu row and key binding (new item W11b: action, row, binding, test); the welcome text names that key.
Defaults chosen without asking: hidden amconf key `welcome-shown` default off; Help is a top-level menu row; when a WB reset is pending, the welcome shows after wb_gate releases.
