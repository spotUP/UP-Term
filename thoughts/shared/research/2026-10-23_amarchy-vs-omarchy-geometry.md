---
date: 2026-10-23
topic: Omarchy main menu geometry vs Amarchy menu geometry, scaling proposal
status: research, no code edited
---
# Amarchy vs Omarchy main menu geometry

Sources fetched to /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/0b08680f-cab3-4bb7-b24b-030c4ca95b4d/scratchpad/omarchy/
(style.css, layout.xml, config.toml from tag v3.8.4 default/walker/themes/omarchy-default + config/walker; Menu.qml and Style.qml from v4.0.6 shell/plugins/menu and shell/Commons).
Line numbers below are in those fetched files.

## 1a. Omarchy v3.8.4 (Walker, GTK4)

| Property | Value | Source |
|---|---|---|
| Window width | 644 px (`width-request` on box-wrapper), centred (halign/valign center) | layout.xml:15, 17-19 |
| Window height | not fixed; sum of parts. Max about 2*2 + 2*20 + search + 10 + 300 = ~400 px | derived |
| List area | max-content-width 600, max-content-height 300, min 0, propagate natural height/width | layout.xml:88-92 |
| Visible rows | 300 / 50 = 6 rows, then scroll (scrollbar opacity 0). max_results 256 | style.css:13-15, config.toml:17; row height derived below |
| Border | 2 px solid @border (theme colour) | style.css:28 |
| Radius | none (`* { all: unset }`, no radius set) | style.css:3-5 |
| Outer padding | 20 px (box-wrapper) | style.css:27 |
| Gap search to list | 10 px (box spacing) | layout.xml:28 |
| Search container | padding 10 px, background @base; entry itself unstyled | style.css:37-40 |
| Search field height | about 10+10 + one GTK entry (about 1 line + entry padding), derived about 44-46 px at 18 px font (unverified, not measured) | derived |
| Font | monospace, 18 px, colour @text | style.css:7-11 |
| Row height | `.item-text-box` padding 14px 0 => 28 px + one text line (about 22 px at 18 px) = about 50 px (derived, not measured) | style.css:89-92 |
| Row left padding | 14 px (item-box), icon margin-right 14 px | style.css:85-87, 104-107 |
| Icon size | `.large-icons` 32 px, scaled 0.9 => about 29 px; `.normal-icons` 16 px | style.css:17-23, 106 |
| Subtext | hidden (font-size 0) | style.css:97-102 |
| Selected row | background alpha(@text, 0.07), text @selected-text | style.css:77-83 |
| Window background | alpha(@base, 0.95) | style.css:26 |
| Colours | @base, @border, @text, @selected-text, @background come from the theme file walker.css (imported, style.css:1); not in this repo of files, per theme |

## 1b. Omarchy v4.0.6 (Quickshell QML, shell/plugins/menu/Menu.qml)

All px values are multiplied by `Style.space()` = px * spacingScale * fontScale (rounded, min 1): Style.qml:204-224. fontScale tracks `[font] base-size`; default body font 12 px (Style.qml:330 comment "12"), so scale 1 at base 12.

| Property | Value | Source |
|---|---|---|
| Card width | 300 * scale (520 for screenrecord and style.font submenus), capped to panel width minus 2*gapsOut | Menu.qml:112 |
| Card height | 2*contentMargin + headerHeight + contentSpacing + visibleRowsHeight, capped to panel height minus gapsOut | Menu.qml:114-116 |
| Max list height | min(panel height - top - gapsOut - paddings - header, 70% of panel height) | Menu.qml:155-162 |
| Rows | no fixed count: as many whole rows as fit, plus a peek of the next row (0.55 row) as scroll hint | Menu.qml:107, 165-179 |
| Border | max(1, space(2)) = 2 px | Menu.qml:91 |
| Radius | Style.cornerRadius, default 0, mirrors Hyprland decoration:rounding | Menu.qml:99, Style.qml:12, 32 |
| Padding (contentMargin) | spacing.panelPadding = 18 | Menu.qml:100, Style.qml:260 |
| Header (search) height | max(space(34), title font + 2*controlPaddingY) = max(34, 14+12) = 34 | Menu.qml:101, Style.qml:248, 332 |
| Content spacing | spacing.md = 6 | Menu.qml:102, Style.qml:239 |
| Row height | max(space(50), body + 2*rowPaddingX) = max(50, 12+24) = 50; detail row 58 | Menu.qml:103-104, Style.qml:257 |
| Row spacing | spacing.xs = 3 | Menu.qml:108, Style.qml:237 |
| Divider | 17 px tall row, 1 px hairline at alpha 0.2 | Menu.qml:109, 1184-1197 |
| Font | Style.font.menuFamily (theme menu font); header Style.font.heading = 16, body 12 | Menu.qml:47, 1163; Style.qml:317-333 |
| Colours | Commons.Color.menu.* (border, selectedBorder, foreground, background), scrim behind the card | Menu.qml:90-96, 1048 |

v4 card is much narrower relative to its font than v3: 300/12 = 25 em, row 50/12 = 4.2 em.

## 2. Amarchy today

The menu is a terminal window (amarchy-menu, libupkgui OSD) sized in character cells, not pixels.

- Width: `cols = max(MN_OSD_MIN_COLS, widest "label  item" + 2*MN_OSD_PAD)`; MIN_COLS 24, PAD 2 (menu_core.h:40-42, menu_tree.c:80-101; widest = strlen(label)+2+(group ? 1 : 0), menu_tree.c:70-73).
- Height: `rows = MN_OSD_HEAD_LINES (2) + max entries of any level reachable from the node` (menu_tree.c:100; menu_core.h:40, 43-46).
- Root today: 24x6 cells (RULES.md:59, tools/rig/amarchy_osd_rig.py:25), i.e. 192 x 48 px in Topaz 8.
- The size is passed as cells to `act_in_window(line, cols, rows)` via `run_helper` (amarchy/amarchy.c:64-70, called from act_menu at :73-87). Window centred on the front screen (RULES.md:59).
- Font: none chosen by Amarchy; the terminal window's own font (vtcon default, Topaz 8 on 640x256). The only setting is `ui_osd(1)` (menu.c:242: no frame, no footer). Head and pad constants must equal the library's UI_OSD_HEAD_LINES/UI_OSD_PAD (compile-time check menu.c:31-33).
- Row height = 1 text line; padding = 2 columns left/right; no border, no radius, no icons, no search-field box (filter is the first head line), no 6-row cap (all entries shown; rows grows with the largest level, so a tall level makes a tall window; scrolling not seen in the lines read: unverified).

## 3. Scaling proposal

Omarchy v3.8.4 proportions at its design point (1920x1080, 18 px):
- Width 644 / 1920 = 33.5% of screen width = 35.8 em of font size (content 600 px = 33 em; monospace 18 px is about 10.8 px wide, so 600 px is about 55 text columns).
- Height about 400 / 1080 = 37% of screen height; list 6 rows, row = 50 px = 2.8 x font size (28 px padding + 1 line).
- Frame: border 2 px (0.11 em), outer padding 20 px (1.1 em), search padding 10 px, gaps 10 px.

Cell-grid constraint: a terminal row is exactly one font line, so the 2.8 em row height cannot be reproduced with one text line per item. Options inside the cell model: one item = one line (row 1.0 em; current), or item plus a blank spacer line (2.0 em, closest to 2.8 em, halves the visible items). Padding and border in px are only possible if amarchy draws them (a window border/inner margin via the library); that is outside the cell terminal. State this as the decision for the owner.

Numbers that reproduce the proportions in cells (cell width w, screen width W, screen height H):
- cols = round(0.335 * W / w), never below the current minimum (widest label + 2*PAD). 640x256 hires, w=8: 27 cols (216 px; today 24). 800x600, w=8: 33 cols. 1024x768, w=8: 43 cols. With a larger font (e.g. 16 px cell height, w=8..10) the same 33.5% rule applies; use the font's real cell width.
- visible rows = 6 list rows (same as Walker 300/50), so rows = HEAD_LINES (2) + min(6, maxn) when 1 line per item. With the spacer-line variant rows = 2 + 2*6 = 14, which does not fit 256-line screens in Topaz 8 (14*8 = 112 px, fine; but the 640x256 screen only gains legibility, not fit problems) and is fine on RTG.
- Cap on height: 37% of H: 256 -> 94 px = 11 Topaz-8 rows; 600 -> 222 px = 27 rows; both allow 6 rows + head.
- Font: Omarchy window is 35.8 em wide. On a 640 px screen at Topaz 8 the screen is 80 em wide, so 33.5% of the screen would be 27 em, not 35.8 em: proportion to the SCREEN and proportion to the FONT cannot both hold. Pick one: match the screen share (cols formula above, recommended since it keeps the look from afar) or match 36 em (cols = 36 + 2*PAD = 40, 320 px = 50% of a 640 screen). Needs the owner's choice.

Where each change lands:
1. Column count: `mn_osd_size` (amarchy/menu_tree.c:99) currently `c = maxw + 2*MN_OSD_PAD`; add the screen-share target there (needs screen width and cell width as parameters, they are not known to menu_tree.c today; caller amarchy/amarchy.c:84 knows the front screen) and keep MN_OSD_MIN_COLS (menu_core.h:42) as the floor.
2. Row cap of 6: `*rows = MN_OSD_HEAD_LINES + maxn` (menu_tree.c:100) becomes `HEAD + min(maxn, 6)`; the list in menu.c/menu_core.c must then scroll (not verified that it does; grep menu.c before relying on it).
3. Height cap by screen share: same function, same parameters as 1.
4. Font: set by the terminal window opened in act_in_window (amarchy.c:69), not by menu.c; a larger font on RTG screens changes cell size and therefore pixel width; use the real cell size when computing 1.
5. Border, 20 px padding, 0.07 selected-row tint, 0.95 alpha: not expressible in the cell OSD (menu.c:242 turns the frame off); needs library support or a drawn pixel layer.
6. Icons (32 px) and v4 dividers/peek row: out of scope for the text OSD (see 2026-10-22_omarchy-menu-icons.md).

Unverified: Walker row height and search field height (derived from CSS paddings plus an assumed 22 px line; not measured in a running Walker); v4 default base-size 12 taken from the Style.qml token comments; whether menu.c scrolls lists longer than the window.
