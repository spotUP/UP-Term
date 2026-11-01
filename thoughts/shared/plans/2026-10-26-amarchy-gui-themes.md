# Amarchy GUI menu themes (Omarchy-style), 2026-10-26

Owner decision 2026-10-26: the drawn GUI menu (amarchy/gui.c) stops using the UP-Term theme (upconf palette + terminal fg/bg via
menu_theme_roles_fb). It gets its own Omarchy theme set chosen from Style > Theme. GUI menu only; the text menu (amarchy-menu in a
terminal) and the terminal keep the UP-Term theme. Border and selected row share one colour (draw_border to MT_SELBOX is another
agent's change; not planned here). Plan only. Read from origin/main after git fetch on 2026-10-26.

Finish line: every item in section 7 ticked; a rig run shows the GUI menu colours change with the theme while the text menu and
terminal do not.

## 1. Colour roles and the shipped themes

Source of values. Omarchy keeps ONE colour file per theme: themes/<name>/colors.toml (flat keys, `key = "#rrggbb"`, plus
`mode = "dark"|"light"`). Every app config (ghostty, waybar, walker, ...) is generated from it by templates in default/themed/*.tpl.
Fetched via gh api / raw.githubusercontent.com, repo basecamp/omarchy (API redirects to omacom/omarchy), branch `quattro`
(the default branch). The walker template was NOT found: default/themed/walker.css.tpl is 404 and no path in the repo tree contains
"walker". So Omarchy's exact walker-role to colour-key mapping is UNVERIFIED; the mapping below is a proposal (open decision D2).
The manual page "making your own theme" (learn.omacom.io/2/the-omarchy-manual/92) says: custom themes go in
~/.config/omarchy/themes (each a folder), colors.toml is the main file, `light.mode` is an empty marker file for light themes,
`icons.theme`, `unlock.png` optional; share as a git repo named omarchy-<name>-theme. It does not list the colors.toml keys.

colors.toml keys (all 23 shipped files have them; tokyo-night shown): mode, accent, selection, muted, background, dark_background,
darker_background, lighter_background, foreground, dark_foreground, light_foreground, bright_foreground, red, yellow, orange, green,
cyan, blue, magenta, brown, bright_red, bright_yellow, bright_green, bright_cyan, bright_blue, bright_magenta.

Role map (proposal): MT_BG = background; MT_TEXT = foreground; MT_DIM = dark_foreground; MT_SELBOX = accent (box and border, one
colour as the owner asked); MT_ACCENT = background (selected row text: inverse on the box, as the minimal menu does). Alternative
for MT_SELBOX is `selection` (Omarchy's own highlight key), but it is only a few shades off background in every dark theme
(tokyo-night #292e42 on #1a1b26), too faint for a border. The Amiga screen has few pens: gui.c obtains 5 pens per open (pens_get),
so five roles is the whole interface; the other 20 keys are parsed and ignored (kept in the file so a copied Omarchy colors.toml works).

Shipped themes (23; 8 columns read from each colors.toml, source URL
https://raw.githubusercontent.com/omacom/omarchy/quattro/themes/<name>/colors.toml; hex as in the file, case preserved):

| theme | mode | background | foreground | dark_foreground | accent | selection | muted |
|---|---|---|---|---|---|---|---|
| catppuccin-latte | light | #eff1f5 | #4c4f69 | #9ca0b0 | #1e66f5 | #ccd0da | #acb0be |
| catppuccin | dark | #1e1e2e | #cdd6f4 | #6c7086 | #89b4fa | #45475a | #585b70 |
| ethereal | dark | #060B1E | #ffcead | #6d7db6 | #7d82d9 | #252e56 | #6d7db6 |
| everforest | dark | #2d353b | #d3c6aa | #4f585e | #7fbbb3 | #3d484d | #475258 |
| flexoki-light | light | #FFFCF0 | #100F0F | #878580 | #205EA6 | #CECDC3 | #B7B5AC |
| gruvbox | dark | #282828 | #d4be98 | #7c6f64 | #7daea3 | #504945 | #665c54 |
| hackerman | dark | #0B0C16 | #ddf7ff | #6a6e95 | #82FB9C | #1f253a | #2d3450 |
| jade-horse | dark | #142620 | #dfede5 | #91ad9f | #8bcbb1 | #2b5147 | #64887b |
| kanagawa | dark | #1f1f28 | #dcd7ba | #727169 | #dcd7ba | #363646 | #54546D |
| last-horizon | dark | #0c0b0c | #FAFCFB | #584e51 | #b59790 | #584e51 | #584e51 |
| lumon | dark | #16242d | #d6e2ee | #4d86b0 | #8bc9eb | #243d56 | #304860 |
| lupine | light | #fafafa | #212121 | #757575 | #3264eb | #d0d0d0 | #9e9e9e |
| matte-black | dark | #121212 | #bebebe | #555555 | #e68e0d | #2a2a2a | #333333 |
| miasma | dark | #222222 | #c2c2b0 | #555555 | #78824b | #383838 | #666666 |
| nord | dark | #2e3440 | #d8dee9 | #667080 | #81a1c1 | #434c5e | #4c566a |
| retro-82 | dark | #05182e | #f6dcac | #3f8f8a | #faa968 | #134e5a | #2a6b78 |
| ristretto | dark | #2c2525 | #e6d9db | #72696a | #f38d70 | #403e41 | #72696a |
| rose-pine | light | #faf4ed | #575279 | #9893a5 | #56949f | #dfdad9 | #cecacd |
| solitude | dark | #101315 | #cacccc | #4b4e55 | #798186 | #343d41 | #4b4e55 |
| starship | dark | #1c1c1e | #f5f5f7 | #98989d | #ff9f0a | #3a3a3c | #8e8e93 |
| tokyo-night | dark | #1a1b26 | #a9b1d6 | #565f89 | #7aa2f7 | #292e42 | #414868 |
| vantablack | dark | #000000 | #ffffff | #505050 | #8d8d8d | #1a1a1a | #7a7a7a |
| white | light | #ffffff | #000000 | #c0c0c0 | #6e6e6e | #c0c0c0 | #808080 |

Note: the Omarchy manual's "extra themes" page (learn.omacom.io/2/the-omarchy-manual/90) lists 117 community themes by name and
repo URL only, no colour values; they are out of scope (users can add them as files, section 2). dark_foreground is very dark in
a few themes (white: #c0c0c0 on #ffffff, last-horizon #584e51 on #0c0b0c): MT_DIM may be low contrast there; the parser keeps
the file's value, a contrast clamp is open decision D5.

## 2. Where themes live on the Amiga, and the parser

Reuse, do not write a TOML parser. The Omarchy colors.toml subset (`key = "#rrggbb"`, one per line, `mode = "dark"`) is
read by the existing upconf reader (config/upconf.h: upconf_str / upconf_rgb, "0xRRGGBB" from hex text; amconf_core already feeds
Amarchy's prefs through it, amarchy/amconf_core.h). To check in implementation (T1): whether upconf_rgb accepts `#rrggbb` and a
quoted value, and a file with no [profile] header. If not, add a `#` and quote tolerance to ONE helper in config/upconf.c
(shared, tested there), not a second parser in amarchy.
Files: one file per theme, NAME.theme, content = Omarchy colors.toml verbatim (so a theme copied from Omarchy or any
omarchy-<name>-theme repo works by renaming colors.toml). Shipped set: dist/amarchy-themes/*.theme (23 files, generated from the
table above by a one-off tool tools/gen_amarchy_themes.py that fetches colors.toml; output committed because the install is offline),
installed per decision D1 (path). Recommended: shipped in UP-Term:Amarchy/Themes/, user themes in
ENVARC:Amarchy/themes/ (same ENV/ENVARC pair as the prefs file, amconf_io callbacks), user file with the same name overrides.
Directory listing for the menu uses the same DOS scan seam as the index loader (amarchy/index_load.c); the pure part
(`name.theme` -> label, sort, override) goes in a new amarchy/gthemes_core.c with callbacks, like amconf_core.

## 3. Saved choice, GUI read at open, menu entry

Saved: new key `amarchy-gui-theme` (name, default per section 5) in amconf_core's table (AM_GTHEME in enum, am_keys[] in
amarchy/amconf_core.c) so it is saved by the existing amconf_save_with to ENVARC: and ENV:, read by the preferences window and the
migration walk with no extra code. NOT stored in the UP-Term upconf (that file stays the terminal's).
gui.c: pens_get (gui.c ~line 928) replaces the upconf palette/fg/bg read for the graphical style with:
amconf_load_with -> AM_GTHEME -> gthemes_load(name) -> five roles via new menu_theme_roles_gui(const gtheme *, rgb[MT_ROLES]).
It is read at every open (pens_get already runs per open), so a switch applies on the next open with no reload. Unknown or
unreadable theme name: fall back to the default theme, never to the UP-Term roles.
Menu entry: Style > Theme exists today and is the TERMINAL theme (dist/keymap line 148: `style.theme = Theme | slash:/theme`, plus
the super+shift+t binding line 95). Conflict, see D3. Recommended: rename that row to "Terminal theme" and make `style.theme`
a submenu `menu:style.theme` of dynamic children. The menu is built from [menu] rows in dist/keymap (rows read by
amarchy/menu_core.c / menu_tree.c; the index adds dynamic rows after them, index_core.h). Theme children are dynamic (user files),
so: new action kind KA_GTHEME `gtheme:<name>` in config/keymap.h (after KA_WIN) handled in amarchy/act.c: writes AM_GTHEME via
amconf_save_with (ENV: and ENVARC:), and a menu_core hook appends one row per theme under node style.theme, same mechanism as the
index rows. Selected theme shown with a marker in its label.

## 4. Other consumers of the GUI roles

- menu_theme_roles_fb stays, unchanged, for the text menu and anything terminal-side (its callers in tests/test_keymap.c stay).
  gui.c stops calling it for the graphical path. Confirm by grep that amarchy-menu (text) never used it (only gui.c:941 and tests
  call it on origin/main).
- menu_theme_minimal (menu-style = minimal, gui.c ~946): operates on rgb[] from whichever roles. DECISION D4: minimal is a GUI
  style, so it follows the GUI theme (bg and fg = the theme's MT_BG and MT_TEXT); the bg_set/fg_set fallback on upconf keys goes
  (it only exists to match the terminal). The "equal colours -> screen pens" fallback stays.
- menu_theme_dopus_commands: DOpus listers match the menu's roles today (plan 2026-10-25-amarchy-dopus-integration, D14, P4 asserts
  dopus pens equal menu_theme_roles_fb). No caller found in amarchy/handler/prefs on origin/main besides tests (the commodity
  entry is planned/in a branch: verify). Recommended: DOpus follows the GUI theme too (it is the Amarchy desktop layer), so that
  entry takes the same menu_theme_roles_gui output; the P4 test is re-pointed. D6.
- Prefs (UP-Term Prefs AMARCHY page, "Menu font" row exists): add a read-only or selectable "Menu theme" row on the same key;
  recommended selectable, same list as the menu (D7 only if owner wants it out).
- menu_icon.c / icons: icons are bitmaps with their own colours; no change. Check: any icon colour pulled from MT_ACCENT (grep).

## 5. Default theme

Recommended: tokyo-night (dark, a mid-saturation accent that reads on a few-pen screen; that it is Omarchy's own default is unverified,
not read from the repo). If the GUI theme key is absent and the theme file is missing,
fall back to a theme compiled into gthemes_core.c (the tokyo-night five values), so a bare install without the data drawer still
draws. A fresh install no longer shows the UP-Term colours in the GUI menu: this is a visible change, noted in the Amarchy docs.

## 6. Tests

Host (tests/test_keymap.c style, in the Makefile list for vttest_host, new gthemes_core.c added in both vttest_host rules):
H1 parse a colors.toml text (quoted, `#`, CRLF, comments, unknown keys, missing key -> that role's built-in value, `mode`);
H2 role mapping per section 1 for all 23 shipped files (read from dist/amarchy-themes, count 23 asserted from the directory,
and equal to the table's row count); H3 the shipped file for each theme holds the exact table values (guards typos);
H4 user override by name; sort order; label from file name; H5 amconf round trip of AM_GTHEME; H6 menu_theme_roles_fb unchanged
(existing tests stay green); H7 minimal over GUI roles; H8 contrast check printed, not failed (D5).
Reachability (ONE rig check, tools/rig/amarchy_menu_rig.py, on a --max rig with purpose "<session>: GUI theme reachability"):
open the GUI menu with theme A (Left Amiga + Space) and read its pens/pixels with the existing MENU-MINIMAL-style pixel counter;
assert background pixel = A.background and the selected box = A.accent; pick Style > Theme > B through the real menu (drives
KA_GTHEME), reopen, assert B's values and that the call-count sentinel for pens_get_gui incremented; open the text menu in a
terminal (amarchy-menu) and assert its colours and the terminal's fg/bg still equal the upconf ones before and after. Bad theme
name in the prefs: default theme drawn. Human check after: owner opens Style > Theme on the rig/A1200 and looks (PASS: menu
recolours on next open, terminal and text menu unchanged).

## 7. Checklist (0 of 22 done)

- [x] T0 Confirm upconf reads colors.toml syntax (`#`, quotes, no header); else extend config/upconf.c once (in main b4530646; test_gthemes passes)
- [x] T1 tools/gen_amarchy_themes.py + dist/amarchy-themes/*.theme (23 files), install script copies them (in main b4530646; test_gthemes passes)
- [x] T2 amarchy/gthemes_core.[ch]: parse, roles map, list/override, built-in default; no OS calls (in main b4530646; test_gthemes passes)
- [x] T3 menu_theme_roles_gui in menu_theme.[ch]; menu_theme_roles_fb untouched (in main b4530646; test_gthemes passes)
- [x] T4 AM_GTHEME in amconf_core (key, default, serialize, tests) (in main b4530646; test_gthemes passes)
- [x] T5 DOS side: scan shipped and user theme drawers (reuse index_load seam)
- [x] T6 gui.c pens_get uses the GUI theme; no upconf palette/fg/bg read in the graphical path (rig 5 c4: up-term fg/bg 0 px)
- [x] T7 minimal style follows the GUI theme (D4) (rig 5 c4: gruvbox mode 2, 2 pens)
- [x] T8 KA_GTHEME in config/keymap.[ch] parser; act.c handler saves and notifies reload
- [x] T9 dynamic rows under style.theme; dist/keymap: terminal row renamed (D3), submenu added
- [x] T10 current theme marker in the list
- [x] T11 DOpus entry follows the GUI roles; P4 test re-pointed (D6)
- [x] T12 Prefs AMARCHY page row
- [x] T13 default theme and compiled-in fallback
- [x] T14 host tests H1..H8 (in main b4530646; test_gthemes passes)
- [x] T15 reachability rig check (section 6) incl. sentinel and text menu / terminal unchanged (rig 5 c4: kbg via menu, AMARCHY_THEME=kbg, 60862 px, terminal 17 colours; text menu unchanged by structure: amarchy-menu links no theme code, nm 0 theme symbols)
- [x] T16 docs: how to add a theme (copy colors.toml to ENVARC:Amarchy/themes/NAME.theme)
- [x] T17 grep-verify every caller of menu_theme_roles_fb (count from the call graph, twice)
- [x] T18 type/host build: make host test, amiga build
- [ ] T19 owner check: screen test on rig and A1200 (asks the owner, URL-less: Left Amiga + Space)
- [x] T20 knob audit: every key used moves output (background, foreground, dark_foreground, accent each asserted); unused keys carry the written exemption in section 1 (rig 5 c4: bg 60862, fg 1894, dark_fg 469, accent 6615 px)
- [x] T21 cleanup: no stray branches or rigs left (gthemes, gthemes-b deleted local+remote; worktrees removed; rig 5 stopped; A1200 deploy b4530646 drift 3 of 21, all UP-Term side)

## Open decisions for the owner

- D1 Theme paths. Recommended: shipped UP-Term:Amarchy/Themes/, user ENVARC:Amarchy/themes/ (override by name). Trade-off: two
  drawers to scan vs one place to edit shipped themes; alternative is a single drawer only (users edit shipped files, upgrades overwrite).
- D2 Role map. Recommended: SELBOX = accent (border and row box), selected text = background, DIM = dark_foreground. Trade-off:
  Omarchy's own `selection` key is a faint highlight and unusable as a border; walker mapping could not be verified (template not in repo).
- D3 Style > Theme collides with the existing terminal theme command. Recommended: rename the terminal row "Terminal theme"
  (super+shift+t stays on it) and give Style > Theme to the GUI themes. Trade-off: users lose the muscle-memory row; the
  alternative is the GUI list under a new "Menu theme" row, which contradicts your wording.
- D4 Minimal style follows the GUI theme. Recommended yes. Trade-off: minimal no longer matches the terminal it opens from.
- D5 Low-contrast roles (white, last-horizon dim text). Recommended: keep the file's values, print a warning in the host test only.
  Trade-off: faithful to Omarchy vs a clamp that changes theme values.
- D6 DOpus lister colours follow the GUI theme. Recommended yes. Trade-off: the DOpus pens then differ from the terminal.
- D7 Prefs "Menu theme" row. Recommended: add it. Trade-off: one more place to keep in step with the menu list (shared key, small).
- D8 Default theme. Recommended tokyo-night. Trade-off: first run looks different from today's UP-Term-coloured menu.

## Decisions (2026-10-26)

Owner answers:
- D3: rename the terminal row to "Terminal theme" (super+shift+t stays on it); Style > Theme lists the GUI themes.
- D4: the minimal style follows the GUI theme.
- D6: Directory Opus lister colours follow the GUI theme.
- D8: default GUI theme is tokyo-night.

Taken at the plan's recommendation (implementation defaults, owner may veto):
- D1: shipped themes in UP-Term:Amarchy/Themes/, user themes in ENVARC:Amarchy/themes/, a user file of the same name wins.
- D2: SELBOX = accent (row box and border, matching MT_BORDER_ROLE from 5269352e), selected row text = background, DIM = dark_foreground, BG = background, TEXT = foreground.
- D5: keep Omarchy's values as shipped; the host test only warns on low contrast.
- D7: add a "Menu theme" row to Prefs, reading and writing the same AM_GTHEME key.

## Progress (2026-10-26, main session ticks)

8 of 22 done, on origin branch `gthemes` (b4e738e2, on top of 34754afb), NOT on main:
- [x] T0 upconf accepts quoted hex (34754afb)
- [x] T1 23 shipped .theme files + install copy (34754afb)
- [x] T2 gthemes_core (34754afb)
- [x] T3 menu_theme_roles_gui (34754afb)
- [x] T4 AM_GTHEME key, default tokyo-night (34754afb)
- [x] T14 host suite gthemes (34754afb; parser reverted 211 failed, role map broken 24 failed)
- [x] T5 gt_scan DOS side + test_gthemes_scan (b4e738e2; fail-before demo NOT done)
- [x] T9 Style > Theme rows + "Terminal theme" rename + style_theme_lists_the_gui_theme_rows (b4e738e2; fail-before demo NOT done)
Code written, not verified: T6/T7 (gui.c and minimal draw from the saved theme). gthemes 319/0, keymap 10257/0.
Open: T6, T7 (rig sentinel AMARCHY_THEME, with T15), T8, T10, T11, T12, T13, T15, T16, T17, T20, T21; T19 owner. T5, T9, T18 proven 2026-10-26 (amiga build links at b4e738e2).

### Progress 3 (2026-10-26, main session)
- origin/gthemes-b at d012c079, rebased on origin/main adc80c2b. Suites at that head: prefs 8225/0, amconf 170/0, gthemes 329/0.
- T8: KA_GTHEME parsed (config/keymap.c:129, :215, name guard :325), dispatched in amarchy/amarchy.c:166 (gt_dos_select), leaf plan tested at tests/test_keymap.c:1041.
- T10: gt_label marks the current theme (gthemes_core.c:124, used gthemes_dos.c:89, tests test_gthemes.c:259-261).
- T11: DOpus palette test follows the GUI roles (commit 0233ab78).
- T12: Prefs Amarchy row "Me_nu theme" (98e92ba4 + d012c079). Fail-before: reverting the am_gtheme line in amconf_core.c gives FAIL tests/test_amconf.c:131.
- T13: GT_DEFAULT fallback (gthemes_core.c:26, :150; test_gthemes_fallback). gt_name_ok fail-before: body forced to return 1 gives FAIL tests/test_gthemes.c:333-336.
- T16: man/amarchy.1 section on adding a theme (0233ab78).
- T17 open (half): source count of menu_theme_roles_fb callers = 1 product caller (menu_theme.c:39, inside menu_theme_roles) plus 7 test calls. The second count, from the m68k objects (nm, U references), is due in agent (c) after the Amiga build.

## Progress 4 (2026-10-26)

- T17 done: count 1 from source (1 product caller, menu_theme.c:39, plus 7 test calls); count 2 from m68k objects of the amarchy-gui and Amarchy links: only menu_theme.o has _menu_theme_roles_fb (definition :22, call :39); amarchy-menu does not link menu_theme.c. Objects not scanned: some Amarchy objects (claude/util.c) that failed to compile standalone; none of them is a theme caller in source.
- Build fix 6a6d499c (Makefile link lists) on origin/gthemes-b.
- T15/T6/T7/T20 still open: rig case r_gtheme in progress.
