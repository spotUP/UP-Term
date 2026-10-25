---
date: 2026-10-25
author: vtcon-a1
status: decisions recorded, design not written
topic: Amarchy gets its own preferences window; UP-Term Prefs drops the Amarchy page
---

# Plan stub: Amarchy preferences live inside Amarchy

## Owner report (2026-10-25, about 23:55)

"The Amarchy menu font also opens UP-Term [Prefs], and the UP-Term Prefs, Amarchy and UP-Term need to be
separate apps." Today both Settings rows in `dist/keymap` (lines 104-105, `settings.amarchy.preferences` and
`settings.amarchy.font`) run `run:'C:UP-Term Prefs' AMARCHY`, so the two rows are duplicates and both open the
terminal's preferences program.

## Owner decisions (AskUserQuestion, 2026-10-25)

1. Settings file: Amarchy's settings move to their own file, `ENVARC:Amarchy/prefs` (ENV: copy as usual). The
   UP-Term file `ENVARC:up-term/up-term` no longer holds `amarchy-*` keys. A one-time migration reads the old keys
   when the new file is absent.
2. No separate program: the preferences window lives INSIDE the Amarchy commodity ("it could live inside amarchy,
   no need for a separate app"). Natural entry points: Exchange "Show Interface" (CXCMD_APPEAR, mapped to
   CXE_APPEAR in `amarchy/cx.c:164` but not acted on today) and the Settings menu rows.
3. All Amarchy preferences move into Amarchy, the menu font included ("move all amarchy prefs into amarchy").
   The Menu font row opens Amarchy's own window (design decides whether it opens straight on the font field or
   the font requester).
4. UP-Term Prefs loses the Amarchy page and the `AMARCHY` argument (`prefs/upprefs.c:1868-1871`).

## Facts found (2026-10-25, grep, not yet a full survey)

- UP-Term Prefs reads and writes the keys in `prefs/prefs_core.c:73-77` and `:762-768`: amarchy-music-drawers,
  amarchy-demo-drawers, amarchy-hide-drawers, amarchy-menu-font, amarchy-app-icons (plus any others the design
  survey finds; count from the prefs item table, not from this list).
- Readers of `amarchy-*` keys: amarchy/gui.c (572 app icons, 1067 menu font), amarchy/menu.c, amarchy/index.c,
  amarchy/index_core.c/.h, amarchy/cxdesc.c. Tests and rigs naming them: tests/test_prefs.c, tests/test_keymap.c,
  tests/test_stage_rig.py, tests/test_rig_ixp6.py, tools/test_rig_upterm.py, tools/rig/amarchy_menu_rig.py,
  tools/rig/rig.py (peer up-term-00's file), tests/amiga/scrprobe.c.
- Amarchy's commodity is created with COF_SHOW_HIDE (`amarchy/cx.c:70`).
- The menu-style key (graphical | text, owner extends with minimal per the MENU-MINIMAL plan) is read by
  cx_menu_cmd from the UP-Term config (`config/upconf_rows.c:139-157`); the design must decide whether menu-style
  moves to the Amarchy file too (it is an Amarchy setting) and coordinate with vtcon-05's MENU-MINIMAL work.

## Open for the design (next session)

- Reuse: the window should reuse the GadTools layout code in prefs/prefs_ui.c (font-sensitive layout) rather than a
  second layout engine; check whether it can be linked into Amarchy or needs a shared module.
- One reader for the new file shared by the commodity, amarchy-gui and amarchy-menu (single source of truth).
- Migration test, reachability test (Exchange Show Interface opens the window on the rig, Save writes
  ENVARC:Amarchy/prefs, amarchy-gui picks up the new font), dist/keymap rows 104-105 rewritten, docs (vtcon-05).
