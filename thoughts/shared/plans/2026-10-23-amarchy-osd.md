---
date: 2026-10-23
topic: Amarchy quick menu as an on-screen display (AM-OSD)
status: plan
research: thoughts/shared/research/2026-10-23_amarchy-osd.md, thoughts/shared/research/2026-10-22_omarchy-menu-icons.md
ledger: thoughts/spot/todos.md, entries AM-OSD (2026-10-22, 2026-10-23)
branch: amarchy-a (after the R6 merge to main, work continues on main)
---

# Amarchy quick menu as an on-screen display

## Owner requirements (ledger AM-OSD)

The menu window feels like Omarchy's launcher: borderless, centred on the
front screen, sized to the menu, closes on Esc and when it loses focus, no
ASCII art, modern look. No fade (owner dropped it; do not re-propose).
The art goes through a plain OSD style option in libupkgui that
amarchy-menu turns on; upkg-browse keeps its look (owner 2026-10-23).

## Where things are today (research file has file:line)

- amarchy/act.c `act_in_window` opens `XCON:0/20/640/300/Amarchy/CLOSE`.
- handler parse_spec (handler/vtcon_handler.c ~1330-1487): no centring,
  no size in cells, no close on deactivate. NOBORDER only adds
  WFLG_BORDERLESS; dragbar, depth and size gadgets and the scroll bar stay.
- IDCMP_INACTIVEWINDOW (~5473) only counts focus and sends ?1004 reports.
  CLOSEWINDOW on a non-raw window sets eof (~5310): the program reads EOF.
- The handler opens on LockPubScreen(0) (~2767), normally Workbench.
- CSI t has no move or resize; there is no MoveWindow/SizeWindow call.
- The masthead, `/////` rail, frame, rules and footer are drawn by
  upterm-ports/pkgs/upkg-ui/src/ui.c (RAIL at :25, masthead :143-163,
  draw_all), shared with upkg-browse.
- The commodity holds the keymap (amarchy/amarchy.c g_km) and can count
  the menu's rows and widest label with mn_children (amarchy/menu_core.c).

## Design

The window is placed and sized once, at open, by the handler: no window
first appears at a default size and then moves (no flicker). The handler
knows the font's cell size; the commodity knows the menu's rows and
columns. So the spec carries the size in cells and asks for centring.

New XCON:/CON: spec fields (handler/vtcon_handler.c parse_spec, the single
parser; documented where the other fields are):

- `CELLS cols x rows` (e.g. `CELLS 34x9`): the window's text area is that
  many cells; it overrides the width and height fields. The outer size is
  the text area plus the window's borders (none with NOBORDER) and the
  inner margin the handler already uses.
- `CENTRE` (alias `CENTER`): centred on the screen it opens on; overrides
  the x and y fields. Clamped to the screen.
- `FRONT`: opens on the front screen when that screen is public (its
  public name found by walking the public screen list for
  IntuitionBase->FirstScreen under LockPubScreenList); else the default
  public screen as today.
- `CLOSEINACTIVE`: losing focus (IDCMP_INACTIVEWINDOW) acts as the close
  gadget: the program reads EOF, as with CLOSE today. One path, the
  existing CLOSEWINDOW handling, called from both events.
- NOBORDER: borderless with no dragbar, depth gadget, size gadget or
  scroll bar. Check every existing user of NOBORDER first (grep the spec
  strings in the repo, the docs and the dist files); if any relies on the
  gadgets, report it to the owner before changing NOBORDER's meaning.

Commodity (amarchy/act.c, amarchy/amarchy.c): `act_in_window` takes the
size in cells. For `menu:<node>`, the size is the largest level reachable
from node: rows = most children of any level + the OSD style's fixed
lines (filter line, gap), cols = widest label + the style's padding,
counted with mn_children over the keymap the commodity holds (one source:
the same walk amarchy-menu uses). Kept within the screen by the handler's
clamp. The key viewer (Super+K) keeps a fixed size in cells.
Spec: `XCON:0/0/0/0/Amarchy/NOBORDER/CENTRE/FRONT/CLOSEINACTIVE/CELLS cxr`.
amarchy-menu exits on EOF (check that the menu loop treats a read EOF as
leave; ui.c's key reader).

libupkgui (upterm-ports/pkgs/upkg-ui/src/ui.c): an OSD style option set
by the caller before the menu is shown (one setter in upkgui/ui.h). With
it: no masthead, no rail, no frame, no footer art; a filter line at the
top, then the rows with icon and label, the selected row as a full-width
highlight bar in the theme's accent colour. Colours from the theme
(no hard-coded colours). Without it: the current look, byte for byte.
amarchy-menu turns it on. The OSD style's fixed lines and padding are
constants in ui.h so the commodity sizes from the same numbers.

## Checklist

- [ ] O1 handler: `CELLS`, `CENTRE`, `FRONT`, `CLOSEINACTIVE` in
  parse_spec; NOBORDER drops the gadgets and scroll bar (after the user
  check). Host tests: parse_spec is not host-testable today; move the
  field parsing that is pure (the spec string to a struct) into the
  winspec module tests/test_winspec.c already covers, and test every new
  field there, including clamping and CENTRE overriding x/y.
- [ ] O2 handler: CLOSEINACTIVE routes IDCMP_INACTIVEWINDOW through the
  CLOSEWINDOW path. Rig test: a window opened with it reads EOF after
  another window is activated; a window without it does not.
- [ ] O3 libupkgui OSD style option + host test of its line layout if the
  library has tests, else a rig screenshot check; upkg-browse unchanged
  (rig screenshot before/after compared by pixels).
- [ ] O4 amarchy-menu turns on the OSD style; exits on EOF.
- [ ] O5 commodity: menu size in cells from mn_children, new spec string.
  Host test: the size for the default keymap's root and apps nodes.
- [ ] O6 reachability rig test (one, through the real entry point): the
  commodity's Super+Space opens the menu window; Wasabi window list shows
  a borderless window centred on the front screen (centre within 1 cell),
  its size the computed cells; no masthead text on screen; clicking
  another window closes it and amarchy-menu exits; Esc at the top level
  closes it. Sentinel: the handler's `UPTerm state` counter of windows
  opened with CENTRE (or the existing state reader if one fits).
- [ ] O7 A1200 manual check by the owner: Super+Space on Workbench and on
  a non-Workbench public screen.

Done = O1-O7 ticked with commits; dist and dist-check pass.
