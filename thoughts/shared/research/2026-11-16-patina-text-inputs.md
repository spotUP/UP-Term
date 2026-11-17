---
date: 2026-11-16
topic: Patina/Prefs settings shown as free text although the value set is fixed or discoverable
status: audit
author: agent
---

# Text inputs that should not be text inputs

Source of truth: every user-visible setting is one row of `prefs/settings_def.c` (`sd_settings[]`); Prefs
(`prefs/prefs_ui.c` table, `prefs/upprefs.c` gadgets) and the Patina menu (`patina/leaf_core.c`, `patina/menu.c`
`ask_value` = K_INPUT) are both built from it. The theme editor rows are built in `patina/gthemes_core.c`
(`er_add`). Fixes for Prefs rows belong in the shared row kind, so one change serves Prefs and menu.

Note: there is no `patina-gui` / `patina-menu` directory; the sources are `patina/`, `prefs/`.
Already fixed, skipped: Icon Font (font requester).

## Findings (ranked by user impact)

| # | Setting (as shown) | Row defined | Current control | Value set | Proposed control | Effort |
|---|---|---|---|---|---|---|
| 1 | Hidden drawers | prefs/settings_def.c:199; Prefs gadget prefs/prefs_ui.c:210 | Prefs: string gadget, comma list. Menu: already a drawer requester (`lf_is_drawer_row`, leaf_core.c:582) | existing directories (discoverable) | Prefs: add a "Choose..." drawer requester button toggling a drawer in/out, reuse `lf_setting_pick` logic (core is in leaf_core, move the list toggle to a shared helper); keep the text gadget as read-only/optional | M |
| 2 | Music drawers | settings_def.c:193; prefs_ui.c:204 | same as above | directories | same as 1 | M (shared with 1) |
| 3 | Demo drawers | settings_def.c:196; prefs_ui.c:207 | same as above | directories | same as 1 | S after 1 |
| 4 | Scrollback | settings_def.c:64; Prefs prefs_ui.c:57 (INTEGER_KIND, upprefs.c:1019) + "No scrollback" checkbox; menu: typed number (leaf_core.c:694, SD_INT) | number 1..99999 | slider/stepper in Prefs (GTSL/ +- buttons, or cycle of presets 100/500/1000/5000/20000 + Custom); menu: cycle presets + Custom | S-M |
| 5 | Theme editor: Corner Radius, Border Width, Title Bar Height, Gadget Size, Gadget Padding, Scroll Bar Width, Knob Minimum Length, Gadget Glyph Set, Inactive Window Dimming | patina/gthemes_core.c:2260-2272 (`look_defs`, SD_INT, ranges 0..8/16/64/100 in the `LD` max column) | menu: typed number via `ask_value` (K_INPUT) | integer with a known max (0..max) | stepper (left/right = -1/+1) in the menu row, GA slider in a gadget UI; Glyph Set (0..8) as a cycle of named sets | M (one shared SD_INT stepper path fixes all 9 rows and Scrollback) |
| 6 | Group colour rows (theme editor "<group> <state>", page THEME_GROUPS) | gthemes_core.c:2350 (SD_SWATCHREF) | picker with palette entries, but the "Custom" entry opens a typed field: "colour or reference" (leaf_core.c:440) | swatch names of the open theme (known set) + RRGGBB | list of the theme's swatches (pick_value list) as first-level choices; typed reference only behind an "Expression..." entry (Darker(...), Mix(...)) | M |
| 7 | Element colour rows (theme editor "<element> <state>", Icon Label Colour) | gthemes_core.c:2358, 2395 (SD_ELEMENT) | same as 6, empty text = follows group | swatch names + empty | same as 6; "Follow group" already exists as SD_PICK_DEFAULT | M (shared with 6) |
| 8 | Fixed colour hex fields: Text colour, Background, Selected text, Selection, Cursor colour | settings_def.c:74-98; Prefs prefs_ui.c:69-108 (PGK_SWATCH + PGK_STRING hex field, upprefs.c:1354 for the palette editor) | swatch (opens picker) plus a 7-char hex string gadget | any colour | keep swatch and picker; the hex gadget is acceptable as an advanced field next to a colour picker. Make the hex field read-only unless the picker is open? Low priority | S (optional) |
| 9 | Palette entries 0-15 (hex) | prefs_ui.c:262-292 (PG_PAL_HEX+n, PGK_STRING) | 16 hex string gadgets | colours | each entry already has swatch/picker via SD_HAS_PICKER; same note as 8 | S (optional) |
| 10 | Terminal type | settings_def.c:113; prefs_ui.c:123 (PGK_STRING, 20 chars) | string gadget, "empty: vtcon" | practical set: vtcon, xterm, xterm-256color, ansi, vt100, linux, + terminfo names in `terminfo/` (discoverable) | cycle/list of known TERM names plus "Other..." (typed) | S-M |
| 11 | Link command | settings_def.c:183; prefs_ui.c:192 | string gadget with Default button | a command template with %s (free) | KEEP (command), optionally a file requester for the program part | - |
| 12 | Hidden file-only keys (no Prefs row, edited by hand in ENVARC:Patina/prefs): tile-gap, tile-layout, tile-ratio, tile-nmaster, desktop-*, welcome-shown, screen-themes, shot-dir, ai-agent, patina-index-skip, patina-index-volumes | settings_def.c:252-283 (`sd_hidden`, SD_STRING, page -1) | not shown anywhere; stored as free strings | tile-layout: enum (dwindle...), tile-gap/ratio/nmaster: numbers with range, desktop-*, welcome-shown, screen-themes: on/off, shot-dir: drawer, ai-agent: command name from a short list | not a UI problem today. When these get Prefs rows (tiling page), use cycle, stepper, checkbox, drawer requester, NOT SD_STRING. Retype now: `desktop-*`, `welcome-shown`, `screen-themes` are SD_STRING but hold on/off (SD_CHECK) | S per key (retype), M for a tiling page |

Count: 10 UI findings (rows 1-10 incl. 3 optional/low), plus the hidden-key retype group (12). Rows 1-3 are one fix, 5 is one fix for 10 rows.

## Top 5 by user impact

1. Theme editor numeric rows (5): 9 rows of typed numbers with a 0..N range, one stepper fixes all.
2. Theme editor colour reference rows (6, 7): about 200 rows (groups x states, elements x states) whose Custom entry is a typed expression; swatch list first.
3. Music/Demo/Hidden drawers in Prefs (1-3): typed comma lists although the menu already has a drawer requester; Prefs has none.
4. Scrollback (4): typed number, presets would do.
5. Terminal type (10): free text for a short known set.

## Legitimately free text (keep)

- Profile name, theme name, swatch name (New/Rename/Save As prompts: names).
- Link command (command template with %s).
- Add Swatch / reference expressions behind an explicit "Expression..." entry (Darker(...), Mix(...)) once row 6/7 is done.
- Menu filter line (menu.c:406), job/command input lines, action argument prompts (`ask_value` for actions that need a value, menu.c:85).
- Hex colour field next to a swatch picker (rows 8, 9) is the advanced entry, not a lazy input.

## Already controlled correctly (no text field)

Font, Fallback font (requester), Screen mode (screen mode requester), Menu theme (SD_DYNLIST list), Colour mode, Cursor, Icons, Mode, all SD_CHECK/SD_CYCLE rows, theme Font slots, theme Look cycles (Title Alignment, Style, Knob Shape, Gadget Order, Ghost Style, Hover).

## Not covered

- Did not read `patina/themewin.c`, `gui.c` (menu window UI), `index_core.c` package/index rows, `menu_theme.c`.
- Did not check the hex field count claim for the theme editor group/element row totals from the builder (about 200 is the product GTG_COUNT*GT_STATES + GTE_COUNT*GT_STATES, unmeasured).
- No rig/UI run: the menu `SD_INT` typed-entry path was read from `leaf_core.c:694` and `menu.c ask_value`, not observed.
- Other prefs-like dialogs outside `prefs/` and `patina/` (e.g. `handler/`, `tools/`) not searched.
