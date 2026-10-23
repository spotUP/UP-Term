---
date: 2026-10-23
topic: Amarchy quick menu as a real graphical window (AM-GUI), next to the text menu
status: plan
amends: 2026-10-23-amarchy-osd.md and section 3a of 2026-10-22-amarchy-commodity.md (the text menu stays; a graphical one is added and is the default)
research: thoughts/shared/research/2026-10-23_amarchy-vs-omarchy-geometry.md, thoughts/shared/research/2026-10-22_omarchy-menu-icons.md
ledger: thoughts/spot/todos.md, entries AM-GUI
---

# Amarchy quick menu as a real graphical window

## Owner decisions (2026-10-23)

- The commodity's UI is graphical, never a terminal: "the commodity should
  not use the terminal only, it needs a real graphical ui". The earlier
  plans chose upkg-ui (curses) in an XCON: window.
- Both stay (owner 2026-10-23: "keep the text amarchy and make the graphical
  one as well"). One upconf setting picks which one Super+Space and Super+K
  open: graphical | text, graphical by default. An extra modifier opens
  the other one (owner: "setting and extra modifier key is best").
- Drawing: an own borderless Intuition window, drawn with graphics.library
  (border, rows, highlight box, chevrons, icons). The menu logic stays:
  amarchy/menu_core.c, amarchy/menu_tree.c, mn_filter.
- Size anchor: the font. Every measure is a ratio of the font's cap height
  or character width, taken from Omarchy (research file); levels longer
  than the visible rows scroll.
- Target look: Omarchy's native menu (shots 1-2 of the second set), not
  the launcher: square corners, 2-3 px accent border, dim header text,
  selected row as a filled box with accent text, chevron on groups, no
  search box, no footer, no separators. Colours follow the UP-Term theme.

## Design

### Where the window lives

The text menu, amarchy-menu (gcc/ixemul, libupkgui, in an XCON: window),
stays as it is. The graphical menu is a new native vbcc program,
amarchy-gui (like Reboot and the commodity): no ixemul, no ncurses, no
libupkgui, no XCON: window. It opens its own window, so start time is one
LoadSeg plus OpenWindow. The commodity runs it with SystemTags
(asynchronous, input and output NIL:). Kept apart from the commodity so its
32 KB resident limit (tests/test_amarchy_size.py) holds.

Shared, not copied: both programs walk the menu with menu_core.c and
menu_tree.c and run leaves the same way. run_leaf and the request-port
code move out of amarchy/menu.c into one module (amarchy/leaf.c) that both
link; only the screen side differs.

The choice: one upconf key (name to match upconf's existing keys, e.g.
`amarchy-menu = graphical | text`), read by the commodity at start and on
reload. act_menu and the viewer action start amarchy-gui or, for text,
amarchy-menu in its XCON: window exactly as today.

The extra modifier: Alt. Super+Shift+Space is already menu:apps in
dist/keymap:37, so Super+Alt+Space and Super+Alt+K open the style the
setting does not name. They are ordinary keymap lines with two new
actions, `othermenu:<node>` and `otherviewer`, so a user can move them like
any other binding:
    super+alt+space = othermenu:root ; Menu (other style)  | menu
    super+alt+k     = otherviewer    ; Key bindings (other style) | menu

Every libupkgui use in the text menu has a drawn counterpart in amarchy-gui:
- K_MENU (menu_loop): the drawn list.
- K_INPUT (ask_value, a value for /bg and the like): one input row drawn
  in place of the list, same window, Enter takes it, Esc cancels.
- ui_message (say_refused): the message drawn as the window's only row,
  any key or click closes.
- K_TEXTBOX (viewer, Super+K): the same drawn list, no selection box,
  scrolls with cursor keys and the wheel.

### Geometry (pure, host-tested)

A new pure module amarchy/menu_geom.c (no OS calls) computes every rectangle
from the font's metrics (cap height, character width, line height), the
screen size and the row count. Ratios from the research file:
- border: max(2, cap/11) px, square corners;
- row pitch about 4.2 x cap height; selected box 0.93 x pitch;
- box inset about 2 characters from the border, text 2 characters inside
  the box, icon column then a 1.5-character gap;
- header row (the node name, dim) above the rows;
- visible rows: at most 8, or what fits in 70 % of the screen height; when
  the level has more, the next row is drawn as a peek fading to the
  background (the scroll cue), and the list scrolls with the selection;
- width: widest label plus icon, chevron and insets; at least the header;
  at most the screen width minus a margin;
- placed centred on the front screen.
mn_osd_size and the MN_OSD_* constants stay: the text menu still uses them.

### Colours

From the UP-Term theme: the palette in the user's upconf (config/upconf.h,
upconf_palette_parse), pens obtained with ObtainBestPen on the window's
screen (reuse the pen code in render/amiga_render.c, do not write a second
one). Roles: background, text, dim text (header), accent (border, selected
text), selected box. Which palette index is which role is fixed in one
table in menu_geom.h (or a theme module if one exists by then).

### Icons

Omarchy's rows carry Nerd Font glyphs (research file, licences listed). The
Amiga has no such font: icons are converted at build time to small images
and drawn with the existing image path the emoji pages use (render/emoji.h,
palette images blended to pens). Size: the icon column is about 1.3 x cap
height. Rows without an icon in the keymap leave the column empty. The
licence of each glyph set used goes into the kit's licence file.

### Input

IDCMP_VANILLAKEY and RAWKEY: typing filters (as now), Backspace shortens,
cursor up/down and the wheel move, Enter opens or runs, Esc clears the
filter, else goes up, else leaves. IDCMP_INACTIVEWINDOW closes the menu
(the CLOSEINACTIVE behaviour, now in the program). Mouse: hover selects,
click runs.

### G5 program design (main session, 2026-10-23)

- amarchy/gui.c, vbcc, no ixemul. Same command line as amarchy-menu
  (`amarchy-gui menu [node] | viewer`, `?`/--help/-h/help print the usage),
  so the commodity switches only the program name (G6).
- Links the pure modules: menu_core.c, menu_tree.c, menu_geom.c, the G2
  theme module, leaf.c (G4) and the keymap loaders amarchy-menu links.
- Screen: the one amarchy-menu's OSD opens on (front public screen, else
  the default public screen), locked for the window's life.
- Font: the screen's font (scr->Font, OpenFont); mg_font = {tf_YSize,
  tf_XSize}; mg_text_fn = TextLength on the window's RastPort.
- Window: WA_Borderless, WA_Activate, WA_RMBTrap, WA_SmartRefresh,
  WA_ReportMouse, WA_BackFill LAYERS_NOBACKFILL (no white flash before the
  first draw: the owner saw that on UP-Term tab switches, TABB2), size and
  place from mg_width + mg_layout. IDCMP VANILLAKEY RAWKEY MOUSEBUTTONS
  MOUSEMOVE INACTIVEWINDOW.
- Level change: ChangeWindowBox to the level's mg_layout box, draw after
  IDCMP_CHANGEWINDOW. The width stays (mg_width covers every level).
- Draw: background RectFill with the theme background pen (never pen 0),
  border in the accent pen, header dim, rows; the selected row's box in the
  selected-box pen, its text accent. A selection move redraws the two rows
  it touches, not the window. The peek row when mg_more.
- Sentinel for G7: every full draw bumps a counter written to the global
  env var AMARCHY_DRAWS (SetVar GVF_GLOBAL_ONLY).
- Modes in the same window: list; input row (leaf.c ask callback: the
  header shows the prompt, the first row is the edit line); message (leaf.c
  refusal callback: one row of text, any key closes); viewer (the [bind]
  lines as rows without boxes, cursor/wheel scroll, Esc closes).
- A leaf runs through leaf.c, then the program ends; a group descends.
- Stack: __stack cookie like amarchy-menu; tests/test_amarchy_stack.py
  extended to the vbcc binary (check how vbcc's startup reads it).
- Built by `make amarchy-gui`, added to dist-base, the kit copy line,
  dist/parts.txt, the base MANIFEST and tests/test_amarchy_docs.py.

## Checklist

- [x] G1 amarchy/menu_geom.c + host test: rectangles for the default
  keymap's root and apps levels at topaz 8 and a 16 px font; scroll cue
  shown only when the level exceeds the visible rows; clamped to a
  640x256 screen.
- [x] G2 (7637eb45) theme roles from upconf's palette, pens through the existing
  render code; host test of the role table.
- [x] G3 (f95c1616; drawing verified by G7's rig pixel grab) icon images: build-time conversion, a test that every [menu] row
  naming an icon has an image, licence file entry.
- [x] G4 (c6ae01a8) amarchy/leaf.c: run_leaf and the request-port code out of
  amarchy/menu.c, linked by both programs; the text menu unchanged in
  behaviour (its host and rig tests still pass).
- [x] G5 (fc419f5d; draw proven by G7: sentinel and geometry on the rig) amarchy-gui, a vbcc Intuition program: window, draw, input,
  filter, scroll; input, message and viewer modes. Stack cookie test for
  it next to amarchy-menu's. Built, in parts.txt and the kit, installed
  next to amarchy-menu.
- [x] G6 (d5263804) the setting: upconf key graphical | text (default graphical),
  parsed in config/, host test of the parse; the commodity starts the one
  it names for menu: and viewer, and the other one for othermenu: and
  otherviewer (Super+Alt+Space, Super+Alt+K in dist/keymap); host test of
  the command it builds for all four actions under each value.
- [x] G6b docs: RULES.md:59, dist/README.txt, the setting in the user docs,
  the two earlier plans pointed at this one.
- [x] G7 (rig 4: PASS, 37 checks incl. AMB3 public and non-public screen) reachability rig test (one, a new tools/rig/amarchy_gui_rig.py;
  amarchy_osd_rig.py keeps covering the text menu with the setting = text): Super+Space through the commodity opens a window that is
  not a console (Wasabi window list: title, size, borderless, centred
  within 2 px), its size equals menu_geom's numbers for the rig's font;
  sentinel: amarchy-menu's draw counter read through its ARexx-less
  probe (a `$AMARCHY_DRAWS` env var it sets) is above 0; Esc closes it.
- [ ] G8 A1200 manual check by the owner against the Omarchy shots, and
  the text menu opening with Super+Alt+Space and with the setting = text.

Done = G1-G8 (with G6b) ticked with commits; make dist and make dist-check pass.
