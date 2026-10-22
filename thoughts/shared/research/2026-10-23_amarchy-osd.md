---
date: 2026-10-23
topic: Amarchy quick menu as an on-screen display (AM-OSD) - facts
status: research
---
# Amarchy OSD: facts

Paths are relative to the vtcon worktree unless absolute.

## 1. Window spec parser (handler/vtcon_handler.c parse_spec, 1330-1487)
- Fields 0-3 are numeric x/y/w/h (1364-1381), field 4 the title (1382-1384). Defaults 0/0/640/200, title "vtcon" (1334-1338). Initial wflags = DRAGBAR|DEPTHGADGET|SIZEGADGET|SIZEBRIGHT|ACTIVATE|SMART_REFRESH (1352).
- Keywords after field 4: CLOSE (adds WFLG_CLOSEGADGET, 1385), WAIT (wait_close, 1387), BACKDROP (BACKDROP|BORDERLESS, removes dragbar/depth/size, 1389-1391), NODRAG, NOBORDER, NOSIZE, NODEPTH, INACTIVE (no ACTIVATE), AUTO, WINDOW 0xaddr, SIMPLE/SMART (no-ops), XTERM, AMIGA, PCANSI, LATIN1, DARK, LIGHT, FGrrggbb, BGrrggbb, CP437, OWNSCREEN, FULLSCREEN, PUBSCREEN<name>, SCREENMODE<hex>, DEPTH<n>, SCREEN<name>, FONT1..9/FRAKTUR, FONT, TAB<port>, PROFILE<name> (1385-1481).
- No centring option, no "size from content" option, no close-on-deactivate option. The only size logic is winspec_size (handler/winspec.h): w/h 0 = screen size from the origin on.
- NOBORDER (1394-1396) sets only WFLG_BORDERLESS. It does NOT clear DRAGBAR, DEPTHGADGET, SIZEGADGET (NODRAG/NODEPTH/NOSIZE do that separately). create_window always passes WA_Title (handler 2800ish: `WA_Title = c->w.title`), so a title would still be requested. sbar_gad.c:17 skips the scroll bar when the window is BORDERLESS (so NOBORDER drops the scroll bar); verified only by that one line, not read in full.
- IDCMP: window opens with ACTIVEWINDOW|INACTIVEWINDOW|CLOSEWINDOW|NEWSIZE... (2807-2809). INACTIVEWINDOW is handled at 5472-5476 only as ua_set(c,0) (UPTERM.active count) and vtwin_focus (focus report CSI O when ?1004 is on). It never closes or breaks. So the program can learn of focus loss only by enabling DECSET 1004 and reading CSI O on its input; no handler change needed for that, but not tried.
- IDCMP_CLOSEWINDOW -> close_gadget (5458-5460, 5310-5320): sets c->closing=1; for a non-RAW window sets c->eof=1 (the program's next read returns EOF); for RAW it reports to the program if asked, else send_break(SIGBREAKF_CTRL_C). XCON is non-raw, so the program gets EOF on read, no break.
- tests/test_winspec.c (33 lines): only winspec_size (width/height 0 = screen, origin offset, edge off screen) and winspec_ground (default colours unless DARK/LIGHT/FG/BG). It does not cover parse_spec or any keyword.

## 2. libupkgui and the "art"
- Makefile:660-668: libupkgui.a comes from $(UPKGUI_SYS)/lib = /Users/spot/Code/UP-Term/upterm-ports/build/sysroot/UP-Term (built from upterm-ports). Source: /Users/spot/Code/UP-Term/upterm-ports/pkgs/upkg-ui/src/ (ui.c 923 lines, ui.h, chrome.h, upkg-ui.c).
- menu.c calls ui_start("amarchy-menu") (amarchy/menu.c:231), sets ui_opt (kind K_MENU, title "Menu", backtitle node or "Amarchy", hotkeys, entries, prompt "Filter: ...", 143-151), ui_prepare(), ui_run() (152-154).
- The "art": /Users/spot/Code/UP-Term/upterm-ports/pkgs/upkg-ui/src/ui.c:25 `#define RAIL "/////"` drawn by draw_masthead (ui.c:143-156, ch_mast_layout in chrome.h:22) on row 1, and animated by paint_rail/ch_rail_frame (ui.c:158-163, tick at 31/646-652). Plus a frame_box (ui.c:125-134), hrules (draw_all, ui.c:~457-463), a prompt row, and a footer of hints H_MENU "Up/Down Move, Enter Accept, Escape Cancel" (ui.c ~450). All of it is in libupkgui; amarchy/menu.c has no art itself. Leader glyph is UTF-8 or ASCII (ui.c:139).
- Layout in the library is fixed to the terminal: width COLS-4, menu rows = LINES-4-y (ui.c:378-382), frame/footer rows at LINES-1/-3/-4. The library has chrome of about 8 rows (frame 2, masthead 1, rule 1, prompt 1+, rule, footer 1) around the list.

## 3. Menu size (default keymap)
- dist/keymap has [keymap] 30, [bind] 34, [menu] 94 (grep -n '^\['); compiled copy build/amiga/keymap_default.inc:96-107. Counted from the [menu] block: 9 rows (grep -c of dotted ids = 9, file ends at line 104).
- Levels (mn_children, amarchy/menu_core.c:34-71, one item per row or per distinct first segment; capped by MN_MAX_ITEMS=64, menu_core.h:10; label cap KM_LABEL 32, config/keymap.h:54):
  root: 3 rows (Apps, Style, Window); apps: 4 (Install add-ons, Tmux, Editor, Claude); style: 3 (Theme, Font, Background); window: 2 (New tab, Preferences).
- Max rows = 4 (apps). Widest label = "Install add-ons" 15 chars (labels; group rows get ">" in the item column, menu.c:141). User keymap (ENVARC:up-term/keymap, keymap.user) can add rows, so size must be computed at run time from mn_children/mn_filter (menu.c:137-138), not hard-coded. The typed filter shrinks the count further.
- Viewer mode (K_TEXTBOX, menu.c:193-214) is a different size: mn_viewer_text over all binds (about 50 rows), not part of the OSD unless wanted.

## 4. Resize / move from the program
- engine/vtengine.c window_op (3333-3380, dispatched from csi_xterm 3788) takes CSI t ops 14, 16, 18, 19 (reports only), 22/23 (title stack). Anything else, including 3 (move), 4 (resize pixels), 8 (resize chars), goes to note_unhandled (default case). So CSI 8;rows;cols t and CSI 3;x;y t are NOT implemented.
- Handler has no MoveWindow/SizeWindow/ChangeWindowBox/ZipWindow call (grep over handler and engine: only WindowToFront at vtcon_handler.c:5200 for the find window). A slash command "/size 132x43" exists (keymap `slash:/size`), implemented in the handler (via the upreq port, handler/upreq.h), but it resizes by cols/rows and has no position/centre; not read in detail. Verified absence of move/size calls only by grep.
- The only way today to set size/position is the open spec (field 0-3), fixed before the program runs.

## 5. Esc today
- menu.c:157-165: ui_run() returns 255 on Escape; with a filter it clears it, with a parent node it goes up one level, at the top it returns 0. main (menu.c:243-244) calls ui_stop and returns rc (255 -> 0). So Esc at the top level ends amarchy-menu.
- Window then closes: handler main loop exits when opens<=0 and (!wait_close || closing) (vtcon_handler.c:7316). Spec has no WAIT, so the handle close from process exit ends the handler and closes the window. act_in_window (amarchy/act.c:64-83) says "the window closes when the command ends"; it opens the file handle itself and passes it as SYS_Input; ownership/close by SystemTags with SYS_Asynch.

## 6. Screen
- lock_screen (vtcon_handler.c:2767-2778): own screen if OWNSCREEN/FULLSCREEN/PUBSCREEN given; else LockPubScreen(c->screen) if SCREEN<name> was in the spec; else LockPubScreen(0) = the default public screen (normally Workbench), NOT the front screen. The window opens with WA_PubScreen = that screen (vtcon_handler.c ~2811) with x/y from the spec relative to that screen. Profile key `screen = own|fullscreen` (1219-1227).
- The commodity (amarchy/*.c): grep for Screen/LockIBase/FirstScreen/IntuitionBase found nothing, so it does not know the front screen or its size today. A new spec keyword or the commodity would have to query it (LockIBase + FirstScreen, or GetScreenData) and pass x/y/w/h or SCREEN<name> in the XCON: string, or the handler would need a centre keyword.

## Gaps to decide in the plan
- Closing on focus loss: handler already receives INACTIVEWINDOW (5473); the least invasive hook is a new spec keyword (e.g. close on inactive: set closing + eof or send_break there) or the program turning on ?1004 and reading CSI O.
- Centring and content sizing need either handler-side options (parse_spec has none) or the commodity computing pixel x/y/w/h before Open; pixel size needs cell size (font) which the commodity does not have except via CSI 16 t after open, which is too late (window already open).
- libupkgui draws fixed chrome (frame, RAIL masthead, footer); a "no ASCII art, modern look" menu means changing upkg-ui (shared with upkg-browse) or a separate draw path in amarchy-menu; the RAIL is one define (ui.c:25) but the masthead/frame are structural.
