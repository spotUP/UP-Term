# Omarchy main menu: entries, icon code points, font, theme, licences

Date: 2026-10-22. Sources read from GitHub (basecamp/omarchy tags v3.8.4 and v4.0.6, ryanoasis/nerd-fonts master).

## Key finding: two generations of the menu

- v3.x (last v3.8.4): `bin/omarchy-menu` is a bash script, shown through walker (dmenu mode, prompt "Go"). This is what the question describes.
  https://github.com/basecamp/omarchy/blob/v3.8.4/bin/omarchy-menu (function `show_main_menu`)
- v4.0.x (v4.0.6 released 2026-10-19; v4.5.0-rc1 tag exists): walker is gone from the menu. `bin/omarchy-menu` is a thin wrapper that calls `omarchy-shell shell toggle omarchy.menu`; the menu is a Quickshell QML plugin (`shell/plugins/menu/Menu.qml`) driven by `default/omarchy/omarchy-menu.jsonc`.
  https://github.com/basecamp/omarchy/blob/v4.0.6/default/omarchy/omarchy-menu.jsonc
- The repo's default branch is `quattro`; `master` serves the same v4 wrapper.
- The root entries and icons are identical in v3 and v4.

## 1. Top-level entries (same order in v3 and v4)

Each row is rendered as `<glyph><two spaces><label>`. Names are from Nerd Fonts `glyphnames.json` (master, v3.5.1).

| Label | Code point | Nerd Fonts name | Set |
|---|---|---|---|
| Apps | U+F003B | md-apps | Material Design Icons |
| Learn | U+F09D1 | md-brain | Material Design Icons |
| Trigger | U+F14DE | md-rocket_launch | Material Design Icons |
| Style | U+EBCF | cod-wand | Codicons |
| Setup | U+E615 | seti-config | Seti-UI (custom) |
| Install | U+F0249 | md-floppy | Material Design Icons |
| Remove | U+F0B4C | md-tab_remove | Material Design Icons |
| Update | U+F021 | fa-arrows_rotate | Font Awesome |
| About | U+EA74 | cod-info | Codicons |
| System | U+F011 | fa-power_off | Font Awesome |

Search line placeholder (walker config): U+F002 (fa-magnifying_glass, Font Awesome) followed by " Search..."; empty list text "No Results". Source: v3.8.4 `config/walker/config.toml`.

System submenu (v4 jsonc): Screensaver U+F1104, Lock U+F023, Suspend U+F04B2, Hibernate U+F0901, Logout U+F0343, Reboot U+F0709, Shutdown U+F0425. (Names for these not looked up: unverified.)
Learn submenu glyphs are in the same file if needed.

Five of ten root glyphs are in plane 15 (Material Design Icons): Apps, Learn, Trigger, Install, Remove.

## 2. Font

- Terminal/UI font: "JetBrainsMono Nerd Font" (ghostty config, v3.8.4 `config/ghostty/config`). Package: `ttf-jetbrains-mono-nerd` (Arch, `install/omarchy-base.packages`). The package's Nerd Fonts version is not stated in the Omarchy repo: unverified. Latest Nerd Fonts release at time of reading: v3.5.1 (note: the Arch package version was not checked).
- Walker CSS sets `font-family: monospace; font-size: 18px;`, so the glyph comes from fontconfig fallback (the monospace default resolves to the Nerd Font via Omarchy's fonts.conf; resolution chain not fully traced: unverified).
- Omarchy also ships its own icon font `omarchy.ttf` (private-use U+E900 to U+E90D: Omarchy logo and AI-agent marks). It is not used by the root menu glyphs. v4 jsonc supports a per-row `iconFont` field for these. https://github.com/basecamp/omarchy/blob/v4.0.6/default/fonts/omarchy/README.md
- `woff2-font-awesome` and `noto-fonts-emoji` are also installed (base packages).

## 3. Walker theme (v3.8.4 `default/walker/themes/omarchy-default/style.css` and `layout.xml`)

- Panel: width-request 644 px (layout.xml); list max content 600 x 300 px.
- Border: 2px solid @border (theme foreground colour). Corner radius: none set (square corners).
- Padding: box-wrapper 20px; search container 10px; item box padding-left 14px; item text box padding 14px 0; icon margin-right 14px.
- Background: alpha(@base, 0.95) panel; search container solid @base (theme background).
- Selected row: background alpha(@text, 0.07) (a faint tint of the foreground); text colour @selected-text, which the theme template sets to the accent colour (`default/themed/walker.css.tpl`).
- Normal text: @text = theme foreground. Font size 18px. Scrollbar hidden. Placeholder opacity 0.5. Subtext hidden. Action hints hidden. Selection wraps.
- Colours are per-theme (imported from `~/.config/omarchy/current/theme/walker.css`), so no fixed hex values.
- v4 Quickshell menu: card width Style.space(300), colours from `[menu]` in shell.toml, corner radius Style.cornerRadius; numeric values not resolved: unverified.

## 4. Licences and ranges (Nerd Fonts v3, from font-patcher on master)

Nerd Fonts project: source fonts, patched fonts and OFL folders under SIL OFL 1.1; scripts and font-patcher under MIT; per-glyph-set licences in license-audit.md.
https://github.com/ryanoasis/nerd-fonts/blob/master/LICENSE and https://github.com/ryanoasis/nerd-fonts/blob/master/license-audit.md

| Set | Range in Nerd Fonts v3 | Licence (license-audit.md) |
|---|---|---|
| Pomicons | U+E000-E00A | SIL OFL 1.1 |
| Powerline | U+E0A0-E0A2, E0B0-E0B3 (extra E0A3, E0B4-E0D7) | not listed in rows I read |
| Seti-UI + custom | U+E4FA-E5FF (menu uses U+E615, in the Devicons-adjacent area per glyphnames) | MIT (modified Seti-UI) |
| Devicons | U+E600-E858 as patched range | MIT |
| Codicons | U+EA60-EC84 | CC BY 4.0 (audit table) |
| Font Awesome | U+ED00-F2FF (FA legacy U+F000-F2E0 also used; arrows_rotate U+F021 is the legacy slot) | CC BY 4.0 |
| Font Awesome Extension | U+E000-E0A9 | MIT |
| Weather Icons | U+F000-F0EB | SIL OFL 1.1 |
| Octicons | U+F000-F105, F27C-F306, 2665, 26A1 | MIT |
| Font Logos | U+F300-F385 | "Unlicensed" |
| Power Symbols | U+23FB-23FE, 2B58 | not read |
| Material Design Icons | U+F0001-F1AF0 (plane 15, Supplementary Private Use Area-A) | Apache 2.0 per audit table; the icon-pack file says Pictogrammers Free License, "free and open source, GPL friendly" |

Plane 15 (U+F0000+): only Material Design Icons. Everything else is in the BMP private use area U+E000-F8FF. U+E615 and the U+F000 block overlap between sets in the raw ranges (Octicons/Weather/Font Awesome legacy share U+F000-F2FF); Nerd Fonts v3 resolves this by order of application, so use glyphnames.json for the authoritative mapping (that is how names in section 1 were obtained).

Caveat: the Seti-UI range row was copied from the patcher's SymStart/SymEnd; the claim that U+E615 belongs to Seti comes from glyphnames.json name `seti-config` (read). Licence rows for Powerline and Power Symbols were not read from the audit: unverified. Licence summaries are not legal advice; CC BY 4.0 requires attribution.
