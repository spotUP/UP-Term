---
date: 2026-10-28
topic: OMARCHY-AI F1 - C:Claude follows the console theme
status: decided (owner): follow the console palette, no per-theme JSON files
---

# F1: agents follow the UP-Term console theme

## Decision

C:Claude draws with palette indices 0-15 only. A console theme switch (Amarchy or `themes/*.conf`) recolours it
with no theme file and no new mode. The JSON theme-file loader (commit c16dbb89) stays in the branch as a way to
override, but nothing needs it for this goal.

## How the console palette changes

- Profile themes: `themes/*.conf` and `*.yaml` carry `palette = 0,RRGGBB,...,15,...`.
  `handler/vtcon_handler.c:1143-1162` (`apply_colours_rest`) parses it into `c->w.pal16[]`.
- Menu preview and apply: `handler/vtcon_handler.c:2150-2162` copies the theme palette to `c->w.pal16` and calls
  `vtwin_apply_settings`.
- `render/vtwin.c:889-893` calls `vt_set_palette` / `vt_clear_palette` for the 16 entries.
- `engine/vtengine.c:6068` `vt_set_palette` sets `pal_set[i]` and calls `colors_changed`, which marks every row
  dirty (`mark_rows(t, 0, t->rows)`) and re-derives the pens. Text already on screen in colours 0-15 is
  recoloured. This is read from code. NOT confirmed on a running window.
- Own screen: `own_screen_open` (`handler/vtcon_handler.c:2660-2710`) makes the first 16 pens the ANSI colours.
- `amarchy/gthemes_core.c` (colors.toml, six keys) drives the drawn Amarchy menu only. It does not write the
  console palette itself.

## What C:Claude drew outside the palette

Audit (grep of `38;5`, `38;2`, `48;` over `claude/*.c` and `view/hl_style.c`, then counted from the tables):

- `cl_themes[7]` (`claude/theme.c:12`): all SGR are 3x/9x/4x/10x plus attributes. Already palette-only. `dark` and
  `dark-ansi` hold the same colours, so the default theme already follows the console.
- Highlighter "ansi" (`view/hl_style.c:199`, `ansi()`): indices 0-15 only. "rich" is RGB but no `cl_theme` uses it.
- `named_sgr` (`claude/theme.c`, used by `/color` and the prompt bar): orange and pink were `38;5;208` and
  `38;5;205`. They did not follow the palette. Now `91` (bright red) and `95` (bright magenta).
- pi look (`TUI_LOOK_PI`, amiga-pi only): `pitheme_color` (`theme.c`) emits `38;2` for RGB colours at true colour
  and a cube index at 256. `pitheme_system` (`theme.c:111`) uses ANSI slots 1-6, 13 and depth 16, so it is
  palette-only. C:Claude's own screen never sets `pith`.

## Mode choice

No mode switch. Default theme `dark` is palette-only, so UP-Term needs nothing. The `theme` setting still wins:
`ui_attach` reads it, `/theme NAME` changes it. Detection of UP-Term exists (`io->upterm`, `claude/io_amiga.c:124`)
but is not needed, so it is not used.

## Tests

`tests/test_claude_tui.c`, `palette_follow()` (called after `themes()` in the suite `claude_tui`):

- every field of all 7 `cl_themes` passes `sgr_palette_only` (no 38, no 48, only 30-37/39/40-49/90-97/100-107
  and attributes);
- every `hl_theme_builtin(th->hl)` class has no RGB colour and an index below 16;
- every `/color` name through `theme_named`;
- `pitheme_system` through `theme_from_pi` and `pitheme_sgr` for all `PR_COUNT` roles.

Revert proof: with `named_sgr` back at `38;5;208` and `38;5;205` the suite reports 2 failed (orange, pink). Restored:
1246 checks, 0 failed.

## amiga-pi theme JSON and indices

`pi_color` (`claude/theme.c`, the parser both programs share) accepts a number 0-255 as an ANSI index. So a JSON
theme can use indices 0-15 only. amiga-pi (a separate repo) builds this parser from vtcon's `claude/`. It already
defaults to the built-in `system` theme (`amiga-pi/app.c:382-383`, `pitheme_system`), which is palette-only. So no
extra "console" file is written. If one is wanted later, it needs every role listed below set to a number 0-15.

## Role keys the theme loader reads (`pi_role_names`, `claude/theme.c`)

accent, border, borderAccent, borderMuted, success, error, warning, muted, dim, text, thinkingText, selectedBg,
scrollbarTrack, scrollbarThumb, searchMatchBg, searchMatchText, userMessageBg, userMessageText, customMessageBg,
customMessageText, customMessageLabel, toolPendingBg, toolSuccessBg, toolErrorBg, toolTitle, toolOutput,
mdHeading, mdCodeBlock, mdCodeBlockBorder, mdQuote, mdQuoteBorder, mdHr, mdListBullet, diffAdded, diffRemoved
(and the syntax, thinking-level and bashMode roles that follow in the same table; `grep -n pi_role_names
claude/theme.c` for the full ordered list of `PR_COUNT`).

C:Claude's file-theme mapping (`theme_from_pi`, commit c16dbb89): box=border, spin/sel/accent=accent,
accept/ok=success, plan=borderAccent, bash=bashMode, memory=mdHeading, err=error, add=diffAdded,
del=diffRemoved; dim fixed `\033[2m`.

## Open

- Live check that a theme switch recolours text already on screen (UP-Term console, any 16-colour text).
- Keep or strip the file loader (c16dbb89). It is now an override, not needed for the goal.
- Amiga cross-build `make build/amiga/Claude`: built, no warnings; `$STACK: 32768` cookie in
  `claude/main_amiga.c:60` untouched. Not run on a rig.
