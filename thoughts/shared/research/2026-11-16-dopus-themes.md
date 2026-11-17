---
date: 2026-11-16
topic: Can Patina drive DOpus 5 via a DOpus theme (.theme) instead of / on top of its own `dopus set` lines?
status: findings, decision pending (owner)
---

# DOpus 5 themes vs Patina's ARexx sequence

Source: ~/Code/Reference/dopus5 @ 96ede09. Not measured on a rig (unverified at runtime).

## 1. What a theme is, and how it is applied
- A theme is NOT a data format: `<name>.theme` is an ARexx script (header "/* D5THEME ... */", `parse arg dopus_port apply_flags`,
  default port DOPUS.1, flags default "PFBS") written by themes.c write_theme_intro (source/Modules/themes/themes.c:599-630).
- Body, each block guarded by `index(apply_flags,"X")`: B backgrounds (desktop/lister/req via `dopus set|clear background`, themes.c:~495-505),
  S sounds (`dopus set sound`), F fonts (`dopus set font screen|listers|iconsd|iconsw`, themes.c:~553-561), P pens+palette
  (`dopus set pens <icons files dirs selfiles seldirs devices assigns source dest gauge user>`, themes_data.c:42-43, then
  `dopus set palette ...`, themes.c:565-575). Last line: `dopus refresh all` (outro, themes.c:636).
- There is NO ARexx command "apply theme"/"save theme". Apply = DOpus function `LoadTheme FILE AP AF AB AS` (themes_data.c:38,27): the module
  builds `"<file> <port> <flags>"` and runs it as a script with COMMANDF_RUN_SCRIPT (themes.c:~285-292). So (a) = send
  `rx` of the .theme (or the DOpus command LoadTheme) from Patina; the same script can be run with plain `rx file.theme DOPUS.1 P`.
- Theme folder: assign D5THEMES: (rexx_dopus.c:1040-1053 theme_expand_path). No sample .theme files ship in the source checkout.

## 2. What it sets that Patina does not
Patina today (patina/menu_theme.c:83-130): `set palette` (4 colours, own-screen only) + 10 `set pens` lines + refresh; desktop background
(desk_dos.c:1396,1651). Theme-only fields: fonts (screen, listers, iconsd, iconsw; rexx_dopus.c:688-700, env font_name/font_size),
lister + requester backgrounds (`set background lister|req`), sounds, and the `user` pen count. Everything else in a theme is the same
`dopus set pens/palette` Patina already writes. Fonts/lister bg/req bg are the only real additions and are not theme colours in Patina's model.

## 3. Reopen cost
A theme ends with `dopus refresh all` = rexx_send_reset(CONFIG_CHANGE_DISPLAY) (rexx_dopus.c:946-948) = close_display(CLOSE_ALL)+display_open:
the full screen/lister reopen. Patina already sends exactly this on an own screen (palette=1, ledger B8b) and `refresh lister`
(rexx_dopus.c:960-982, redraw only) on Workbench. A theme script has no lister-only variant, so applying one on the Workbench screen would
add a flicker/reopen Patina currently avoids (the F flag also changes screen font => reopen anyway; fonts/palette pens are written to env only).

## 4. Recommendation
- (a) generate a .theme per Patina theme + LoadTheme/rx: adds nothing for colours (same commands), forces `refresh all` everywhere, adds a file to
  install/generate per theme; only worth it to let users pick a Patina theme from DOpus's own Themes menu.
- (b) extend `dopus set` lines with missing fields: cheap, keeps lister-only refresh on WB; only candidates are `set font listers|screen`
  (needs a font policy in Patina, owner call) and lister/req backgrounds (needs a Patina background asset per theme).
- (c) nothing: colours are already complete; recommended unless fonts/lister backgrounds become a Patina feature.
Recommendation: (c) now; if wanted, next row "B8c DOPUS-THEME-FONT-BG: `dopus set font listers` + `dopus set background lister` from the
theme table, refresh all only when font changed". Do not do (a).
