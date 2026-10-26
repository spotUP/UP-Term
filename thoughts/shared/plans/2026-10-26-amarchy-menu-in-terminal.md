---
date: 2026-10-26
topic: Amarchy text menu inside the terminal (prompt takeover)
status: design, not started
owner_decision: prompt takeover (AskUserQuestion 2026-10-26, recommended option)
---

# Amarchy text menu inside the terminal

## What the owner asked

"An option for Amarchy's text mode UI to open inside the terminal as a terminal app."
Chosen form: prompt takeover. A menu hotkey pressed over an UP-Term window whose vsh sits idle
at its prompt draws the text menu in that window, like an fzf widget, and the chosen leaf runs
there. Over any other window, or over a terminal that is running a program, the hotkey opens the
XCON popup exactly as today.

## Facts the design rests on (origin/main 2026-10-26)

- Hotkeys over an UP-Term window reach the handler first. handler/keymap_act.c maps KA_MENU,
  KA_OTHERMENU, KA_VIEWER and KA_OTHERVIEWER to KACT_DEFER: the handler hands them to the
  commodity, which runs `amarchy-menu <node>` in an XCON window (amarchy/amarchy.c
  act_menu_kind, sized by mn_osd_size_t).
- `amarchy-menu` is a upkg-ui (curses-style) program. It already runs in any console; nothing in
  it needs XCON (amarchy/menu.c main: `amarchy-menu [viewer|<node>]`).
- The handler knows when vsh is at its prompt: vsh arms the console before each prompt
  (handler/lineedit.h, le_ctrl_d shell_armed), and the line editor holds the typed line.
- The line editor already queues keys typed ahead of the prompt (lineedit.h, "Keys typed ahead",
  owner 2026-10-12): the path to put a command line into the prompt exists.

## Design

1. New upconf setting `menu-place` = `popup` (default, today's behaviour) or `terminal`.
   Only the text style uses it; the graphical style always opens its own window.
2. Handler, in the KACT_DEFER branch for menu and viewer kinds: when `menu-place` is `terminal`,
   the menu style is text, the window's console is armed by vsh, and the prompt line is empty,
   the handler does not defer. It types `amarchy-menu <node>` plus Return into that window
   through the typed-ahead path, hidden from history (no history entry, no echo of the line).
   Otherwise it defers as today.
3. `amarchy-menu` needs a mode where a leaf runs in the same console: a `run:` leaf becomes the
   program's exit action (it Execute()s the line in its own console and returns), so the prompt
   comes back after the leaf ends. Other leaf kinds (detach:, rexx:, win:, open:, menuid:) keep
   going through the commodity port, as today.
4. Escape at the top level ends the program; vsh redraws its prompt.

## Open questions for the owner (ask through AskUserQuestion before phase 2)

- A non-empty prompt line (half-typed command): fall back to the popup (recommended) or keep
  the line and restore it after the menu?
- Should the viewer hotkey (Super+K) follow the same setting?
- Window height: the menu uses the whole terminal, or only MN_OSD_MAX_ENTRIES + head rows at
  the bottom like fzf --height (recommended: bottom rows, the scrollback stays visible)?

## Checklist

- [ ] T1 upconf key `menu-place` (config/upconf_rows.c), Prefs row, docs line.
- [ ] T2 handler: prompt-idle test (armed, empty line) as a pure function with a host test.
- [ ] T3 handler: KACT_DEFER branch types the command when T2 holds and the setting says
      terminal; host test with a sentinel that the deferral did not run.
- [ ] T4 amarchy-menu: in-console run: leaves; host test in tests/test_keymap.c (leaf_core).
- [ ] T5 rig row in tools/rig/amarchy_menu_rig.py: hotkey at an idle vsh prompt, the menu text
      appears in the UP-Term window (screen_text), a run: leaf output lands there, prompt back.
- [ ] T6 rig row: same hotkey while a program runs opens the XCON popup.
