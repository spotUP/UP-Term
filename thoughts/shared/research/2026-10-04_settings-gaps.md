---
date: 2026-10-04
topic: UP-Term user settings vs modern terminals -- what a daily user can configure, and what is missing
tags: [research, settings, prefs, profiles, keyboard, mouse, ux, gaps]
status: draft
---

# UP-Term settings gaps

**Scope.** This document covers settings and user-configurable behaviour: what a user can
change and where. Escape-sequence and protocol conformance is in
`2026-10-04_modern-terminal-gaps.md`. Parts of that document are now out of date: the
source has since gained double-click and triple-click selection, middle-click paste,
focus reports, ?1007, the kitty keyboard protocol, OSC 7, 8, 9, 52, 133 and 777, and a
UTF-8 clipboard. The code below is the reference.

**Reference terminals:** iTerm2 (Profiles and Preferences), WezTerm (config reference),
kitty (`kitty.conf`), Alacritty, GNOME Terminal and VTE profiles, Windows Terminal,
Terminal.app, foot (`foot.ini`) and Ghostty. The "modern terminals" column is general
knowledge of those products. It was not measured here.

**Where UP-Term settings live.** The table below lists every place I read.

| Surface | Source |
|---|---|
| Profile keys, `[profile <name>]` in `ENV:up-term/up-term` (falls back to ENVARC:) | `handler/vtcon_handler.c:836-951` `apply_profile` (+ `apply_colours_rest :802`, `link-open` read at `:4341`); documented in `dist/up-term.conf` |
| Window spec options (`XCON:x/y/w/h/title/OPT...`) | `handler/vtcon_handler.c:954-1122` `parse_spec` |
| Prefs editor (UP-Term Prefs): pages General and Colors | `prefs/upprefs.c:115, 440-500`; model `prefs/prefs_core.c` (`prefs_from_conf :56`, `prefs_stage :174`) |
| Window menu (UP-Term, Edit, View, Settings, Complete) | `handler/vtcon_handler.c:1145-1224`; ids `handler/menu_ids.h` |
| Slash commands (`/name` at the prompt, `C:UPTerm name` from scripts) | `handler/slash.c:52-89`; `handler/upterm.c` |
| Live reload: a watcher per window re-applies its profile when the file changes | `handler/vtcon_handler.c:622-631` |

**Status words.** *yes* means a user can set it. *partial* means it exists but has a real
limit: one surface only, fixed behaviour, or a narrower form. *missing* means there is no
code for it. *N/A* means it does not apply on AmigaOS 3.x. For each "missing" status, the
table cell says where I looked. A grep result only shows that a spelling is absent. Every
absence below was also checked against the structure that would hold the setting: the
`apply_profile` key list, `prefs_fields`, `menu_def`/`menu_set` and the slash `table[]`.

**Effort.** S is hours to a day: a profile key plus plumbing. M is several days and
involves renderer or UI work. L is a week or more.

---

## 1. Keyboard

| Setting | Modern terminals | UP-Term | Where / evidence | Effort |
|---|---|---|---|---|
| Key repeat rate / delay | The OS sets it in every one of them; no profile overrides it | **yes (system Input prefs)** | Repeats come from input.device and carry `IEQUALIFIER_REPEAT`. UP-Term passes them through and never sets its own rate. DECARM `?8` off drops them (`render/vtwin.c:1269-1270`), and the kitty protocol reports them as repeat events (`:1228`). No `WA_RptQueue` is set (grep on handler/render), so Intuition's default queue limit applies: a busy window drops repeats and does not run on afterwards. This matches the modern terminals. A per-profile rate would be unusual. | -- |
| Option/Alt as Meta (ESC prefix) | all | yes | Profile `meta = amiga \| alt` (`vtcon_handler.c:925-927`), Prefs "Meta key" (`upprefs.c:459`), menu Settings > Meta key, `/meta` | -- |
| Meta per side (left/right Option separately) | iTerm2, kitty (`macos_option_as_alt left`), Ghostty, WezTerm | partial | `vti_mods` treats Left Amiga as Meta, or with `meta_alt` both Alt keys (`render/vtinput.c:13`). There is no "left Alt only" choice that would keep Right Alt for national characters. | S |
| Backspace sends DEL or BS | VTE, iTerm2, Terminal.app, xterm | partial | Only the profile key `backspace = del \| bs` (`vtcon_handler.c:941-943`) and DECBKM `?67` set it. It has no Prefs gadget, menu item or slash command. **Saving from Prefs or from the menu deletes it from the profile** (see Found below). | S |
| Delete key sends `CSI 3~` / DEL / BS | VTE, Terminal.app | missing | Hard-coded in the engine encoder (`vt_encode_key`). No profile key exists (checked the `apply_profile` key list). | S |
| Key bindings / remapping (actions to keys) | kitty `map`, WezTerm `keys`, iTerm2 Key Mappings, Windows Terminal `actions`, Ghostty `keybind`, foot `[key-bindings]` | missing | The console's own keys are hard-coded in `console_key` (`vtwin.c:1154-1193`): RAmiga C/V, RAmiga Up/Down, Shift+PgUp/PgDn, RAmiga+Shift+Up/Down. RAmiga F is hard-coded in `dispatch` (`vtcon_handler.c`, IDCMP_RAWKEY case). Menu shortcuts are fixed in `menu_def` (`:1150-1176`). No profile key exists. | M |
| Send-text bindings (a key types a string) | iTerm2, kitty `send_text`, WezTerm `SendString`, Windows Terminal | missing | Same places as the row above. | M (with bindings) |
| Keymap / keyboard layout per window | Terminal.app (input), mostly OS | partial | XCON uses the system default keymap (`w->keymap` = 0, `vtwin.h`). console.device units follow `CD_SETKEYMAP` (`device/upcon_unit.c:283`). There is no profile key. | S |
| Compose / dead keys | OS input method | yes (system) | The keymap does it: `RawKeyConvert` with the previous keys (`vtwin.c:1334-1342`) | -- |
| Keyboard protocol switches (kitty protocol on/off, modifyOtherKeys default) | WezTerm `enable_kitty_keyboard`, xterm resources | partial | The engine supports both and programs turn them on (`vtwin.c:1216`, `:1325`). The user cannot force them off or set a default. | S |

## 2. Mouse

| Setting | Modern terminals | UP-Term | Where / evidence | Effort |
|---|---|---|---|---|
| Copy on select | all | yes | Profile `copy-on-select` (`vtcon_handler.c:932-934`), Prefs "Copy on select", menu, `/copy-on-select`; applied at `vtwin.c` `VTI_SELECT_END` | -- |
| Middle-click paste | all on X11/Wayland, opt-in on macOS | yes (always on) | `VTI_PASTE` -> `paste()` (`vtwin.c`, `vtwin_mouse`). There is no switch to turn it off. | -- |
| Right-click action (paste / context menu) | Windows Terminal, iTerm2, kitty | N/A | On the Amiga the right button is the menu bar. That is the platform's context menu. | -- |
| Wheel scrolls the scrollback (on/off) | all | yes | Profile `wheel = scroll \| ignore`, Prefs, menu "Wheel scrolls", `/wheel` | -- |
| Wheel speed (lines per notch) | iTerm2, kitty `wheel_scroll_multiplier`, Alacritty `scrolling.multiplier`, WezTerm | missing | `VTI_WHEEL_LINES 3` is a constant (`render/vtinput.h:42`) | S |
| Wheel in pagers on the alternate screen (alternate scroll) | kitty, foot, VTE and Ghostty turn it on by default; xterm has a resource | partial | Only programs can enable it, with `?1007` (`render/vtinput.c:211-216`). It is off after reset (`engine/vtengine.c:5158`). `less`, `man` and `git log` never send ?1007, so the wheel does nothing useful in them. No profile key exists. | S |
| Mouse reporting on/off (let programs take the mouse) | iTerm2 "Enable mouse reporting", kitty, WezTerm | partial | Holding Shift takes the mouse back for one action (`vti_button`, as in xterm). There is no setting. | S |
| Focus follows mouse | kitty, WezTerm, Ghostty, X11 WM | missing | Nothing in `vtwin_mouse`. On the Amiga this is a system-wide commodity's job (SunMouse-style tools), so it is low value. | S |
| Hide pointer while typing | kitty, Alacritty, WezTerm, Ghostty, iTerm2, foot | missing | No `SetWindowPointer` or `SetPointer` anywhere in handler/ or render/ (grep). The key path (`vtwin_key`) does not touch the pointer. | S |
| URL detection in plain text + click to open | all except xterm | partial | Only OSC 8 links open, with **Ctrl**+click (`vtwin.c:1392-1408`). The modifier is fixed. The open command is profile `link-open` (`vtcon_handler.c:4341`; `OpenURL %s` by default), which has no Prefs gadget, menu item or slash command. Plain-text `http://...` is not detected: no matcher exists in vtwin, vtinput or termurl (grep "http"). | M |
| Double-click word characters (separators) | iTerm2, VTE, kitty `select_by_word_characters`, WezTerm, foot | partial | Fixed class: letters, digits, everything >= 0x80, and `-#%&+,./=?@\_~:` (`render/vtinput.c:133-145`). It cannot be configured. | S |
| Triple-click line (wrapped line whole) | all | yes | `vti_line` (`render/vtinput.c:175-183`) | -- |
| Rectangular (block) selection with a modifier | iTerm2, kitty, WezTerm, foot, VTE, Windows Terminal | missing | `vr_select` is linear only (`vtwin.c` `VTI_SELECT` case) | M |
| Double-click time | the OS | yes (system) | `DoubleClick()` with the user's Input prefs (`vtwin.c`, `vtwin_mouse`) | -- |

## 3. Text, fonts and cursor

| Setting | Modern terminals | UP-Term | Where / evidence | Effort |
|---|---|---|---|---|
| Font face and size | all | yes | Profile `font = NAME:SIZE` (`vtcon_handler.c:846-855`), Prefs "Font" (a text field with no requester, `upprefs.c:449`), menu Settings > Font... (ASL), `/font` | -- |
| Font size up/down shortcut | all | yes | View > Bigger/Smaller font (RAmiga + / -), `/font-size`. It steps to the next designed size (`vtwin.c:1577`). | -- |
| Fallback fonts | all (font chains) | partial | One outline font: profile `font-fallback`, Prefs "Fallback font", `/font-fallback` (`vtcon_handler.c:885`). There is no chain. | M |
| Line spacing (line height) | iTerm2, kitty `modify_font cell_height`, Alacritty `font.offset`, WezTerm `line_height`, Ghostty `adjust-cell-height`, foot | missing | The cell height is the font's `tf_YSize` (renderer `r.ch`). No key exists in `apply_profile` or `prefs_fields`. | M |
| Cell width / letter spacing | iTerm2, kitty, Alacritty, WezTerm `cell_width`, Ghostty | missing | Same places as the row above (`r.cw` comes from the font) | M |
| Bold as bright | all | yes | Profile `bold-bright`, Prefs, menu "Bold is bright", `/bold-bright` (`vtcon_handler.c:922-924`) | -- |
| Bold weight on/off, separate bold font | iTerm2 "Use bold fonts", Alacritty/kitty/WezTerm bold font, Terminal.app | missing | Bold is always algorithmic (`render/amiga_render.c:740`, `:1123`). No key exists. | S |
| Italic / alternate fonts | all (italic font) | partial | Italic is algorithmic (`FSF_ITALIC`). The SGR 11-20 fonts can be set only with the spec options `FONT1..FONT9` and `FRAKTUR` (`vtcon_handler.c:1093-1096`). They are not profile keys. | S |
| Ligatures | kitty, WezTerm, Ghostty, iTerm2 | N/A | Bitmap fonts. Not realistic on a 68k. | -- |
| Font aspect (square vs tall pixels, Amiga) | -- (Amiga-specific) | partial | Only the profile key `font-aspect = off` (`vtcon_handler.c:881-883`). It has no Prefs gadget, and Save drops it (Found). | S |
| Blinking text allowed | iTerm2, VTE "Allow blinking text", Terminal.app | missing | SGR 5 and 6 always blink (`amiga_render.c:1278`, `:1841`). No key exists. | S |
| Cursor shape | all | yes | Profile `cursor`, Prefs, menu Cursor >, `/cursor` | -- |
| Cursor blink | all | yes | Profile `cursor-blink`, Prefs, menu, `/cursor-blink` | -- |
| Cursor blink rate / stop after N s | kitty `cursor_blink_interval`/`stop_blinking_after`, Alacritty, Windows Terminal | missing | The rate is fixed in frames (`amiga_render.c:1841`) | S |
| Cursor colour | all | yes | Profile `cursor-color`, Prefs "Cursor colour", `/cursor-color` (`vtcon_handler.c:804-808`) | -- |
| Cursor text colour | kitty `cursor_text_color`, Alacritty `cursor.text`, iTerm2, Ghostty | missing | Only `cursor_rgb` exists (`vtwin.h`). The cell under the cursor is inverted. | S |
| Hollow cursor when the window is not active | kitty, Alacritty, WezTerm, iTerm2, foot, Ghostty | missing | `vtwin_focus` only sends the `?1004` report (`vtwin.c:1528-1537`). The renderer is not told about focus (grep "focus\|inactive" in render/). | S |
| Minimum contrast | iTerm2, Ghostty `minimum-contrast`, kitty (`text_fg_override_threshold`), WezTerm | missing | Nothing in `vt_resolve_colors` or the renderer | M |
| Character encoding / emulation (UTF-8, Latin-1, CP437; xterm/amiga/pcansi) | iTerm2, Terminal.app, GNOME (encoding); "declare terminal as" | partial | Only spec options `XTERM` `AMIGA` `PCANSI` `LATIN1` `CP437` set it (`vtcon_handler.c:1047-1064`). It has no profile key, Prefs gadget, menu item or slash command. | S |
| Ambiguous-width characters as wide | VTE, iTerm2, Windows Terminal | missing | `engine/vtwidth.h` has a single table | S |

## 4. Colours

| Setting | Modern terminals | UP-Term | Where / evidence | Effort |
|---|---|---|---|---|
| Themes / colour schemes | all | yes | 113 theme files in `themes/` (one `.conf` per theme; the `.yaml` files sit alongside them). Menu Settings > Theme..., `/theme NAME`, Prefs "Theme..." (`prefs_apply_theme`, `prefs_core.c:96`) | -- |
| Foreground / background | all | yes | Profile `fg`/`bg`, Prefs Colors, `/fg` `/bg`, spec `FG`/`BG`/`DARK`/`LIGHT` | -- |
| 16-colour palette | all | yes | Profile `palette = i,RRGGBB,...` (`vtcon_handler.c:813-817`), Prefs Colors (16 fields) | -- |
| Palette entries 16-255 | kitty `color16..255`, WezTerm, foot | missing | `pal16[16]` only (`vtwin.h`). Programs can still change any entry with OSC 4. | S |
| Bold text colour | iTerm2, Terminal.app, GNOME (bold colour), Windows Terminal (`intenseTextStyle`) | missing | No key in `apply_colours_rest` | S |
| Selection colours | all | yes | `selection-fg`/`selection-bg`: profile, Prefs, `/selection-fg` `/selection-bg` | -- |
| Faint / dim text | all (automatic); some have a dim-opacity setting | yes (automatic) | Faint text is drawn halfway to the background in any colour (`engine/vtengine.c:6248-6260`). There is no setting, which is normal. | -- |
| Transparency / blur | all except Terminal.app classic | N/A | AmigaOS 3.x has no compositor | -- |
| Automatic light/dark theme | Ghostty, WezTerm, kitty, Windows Terminal | N/A | OS 3.x has no system appearance setting. The engine answers `?996n`/`?2031` from the colours it uses. | -- |
| Tab bar colours / per-profile tab colour | iTerm2, WezTerm, kitty, Windows Terminal | missing | The tab bar is drawn with screen pens. No key exists. | S |

## 5. Window and tabs

| Setting | Modern terminals | UP-Term | Where / evidence | Effort |
|---|---|---|---|---|
| Initial size in columns × rows | all | partial | At open, the spec's pixel `w/h` sets the size (`vtcon_handler.c:1000-1013`). After open, `/size COLSxROWS` and View > 80 x 24 / 132 x 43 can change it. No profile key exists (checked the `apply_profile` list). | S |
| Initial position | most | partial | Spec `x/y` only | S |
| Remember last size and position | Terminal.app, iTerm2, Windows Terminal (`launchMode`) | missing | Nothing saves the window geometry: `prefs_fields` has no geometry field | S |
| Padding around the text | all | missing | The only inset is `inset_top` for the tab bar (`vtwin.h`). No key exists. | S |
| Window title / title format | iTerm2, Windows Terminal, kitty `tab_title_template`, WezTerm, VTE | partial | The spec title and OSC 0/2 set it (`vtwin.c:121`). OSC 9/777 notices replace it for a while (`vtwin.c:139-180`). There is no format setting (profile name, cwd, size). | S |
| Allow programs to change the title | iTerm2, Windows Terminal (`suppressApplicationTitle`), VTE | missing | Nothing gates `cb_title` | S |
| Tabs | most | yes | Menu UP-Term > New/Next/Previous/Close tab (RAmiga T . ,), `/tab` (`vtcon_handler.c:4365-4413`) | -- |
| New tab with a chosen profile | iTerm2, Windows Terminal, WezTerm | partial | A new tab always takes the current window's profile (`host_command`, `vtcon_handler.c:4397`). `/profile` can switch afterwards. | S |
| Rename tab, hide bar with one tab, tab activity mark | iTerm2, kitty, WezTerm, Windows Terminal | missing | The tab label is the program's title (`titled` callback). There is no rename command or bar option. | S |
| Splits / panes | iTerm2, kitty, WezTerm, Ghostty, Windows Terminal | missing | None. tmux and screen cover it (both are in `dist/`). | L |
| Full screen | all | yes | Profile `screen = fullscreen`, menu Settings > Screen, `/screen`, spec `FULLSCREEN` | -- |
| Own screen, screen mode, depth (Amiga) | -- | partial | Profile `screen`/`screen-mode`/`screen-depth` (`vtcon_handler.c:859-879`), menu, `/screen`, spec options. Prefs has no gadgets for them: the fields survive a save but are not shown (`prefs_core.h`; no gadget in `upprefs.c`). | S |
| Always on top | iTerm2 (hotkey), Windows Terminal, kitty (WM) | N/A | Intuition has no stay-on-top layer. Spec `BACKDROP` is the opposite case. | -- |
| Window decorations (borderless, no drag/size gadgets) | most | yes (spec only) | Spec `NOBORDER` `NODRAG` `NOSIZE` `NODEPTH` `BACKDROP` `CLOSE` (`vtcon_handler.c:1017-1030`) | S (profile) |
| Scrollbar | all except Alacritty and foot | missing | No PROPGADGET or scroller in handler/render (grep) | M |
| Hotkey / drop-down (quake) window | iTerm2, Windows Terminal, Guake, kitty `quick-access` | missing | No commodity hotkey (grep for the commodities library returns nothing in handler/) | M |

## 6. Scrollback and search

| Setting | Modern terminals | UP-Term | Where / evidence | Effort |
|---|---|---|---|---|
| Scrollback lines | all | yes | Profile `scrollback` (`vtcon_handler.c:893-899`), Prefs, menu presets None/500/1000/2000/5000, `/scrollback N` (up to 100000, `:2943`) | -- |
| Unlimited scrollback | iTerm2, VTE, Terminal.app, WezTerm | missing | Capped at 100000 (`vtcon_handler.c:2943`), at 16 bytes per cell | S |
| Search | all | partial | Find... / Find next (RAmiga F / G), `/find` (`vtwin.c:1628`, engine `vt_find :6152`). It is a forward substring search, ASCII case-folded. It has no highlight-all, match count, regex, case switch or backward search. | M |
| Clear screen / clear scrollback | all | yes | Edit > Clear screen (RAmiga K), Clear scrollback, Reset terminal; `/clear`, `/reset` | -- |
| Scroll to the bottom on output | iTerm2, VTE and Konsole have a setting (VTE default off); Terminal.app off | partial (always on) | Any output while the user reads the scrollback snaps the view back (`vtwin.c:929-935`). There is no setting. | S |
| Scroll to the bottom on a keypress | VTE, Konsole (setting) | yes (always on) | `vtwin.c:1316` | -- |
| Jump to the previous / next prompt | iTerm2, kitty, WezTerm, Ghostty, foot | yes | RAmiga+Shift+Up/Down over OSC 133 marks (`vtwin.c:1157-1168`) | -- |
| Copy the last command's output | iTerm2, kitty, WezTerm, Ghostty | missing | The OSC 133 marks exist (`vt_find_mark`). No action uses them. | S |
| Save scrollback to a file | iTerm2, Terminal.app, VTE, kitty (pipe) | missing | No menu item or slash command (checked `menu_def`, slash `table[]`) | S |
| Reflow on resize | most | yes | Profile `reflow`, Prefs, menu, `/reflow` | -- |

## 7. Bell and notifications

| Setting | Modern terminals | UP-Term | Where / evidence | Effort |
|---|---|---|---|---|
| Audible bell | all | yes | `bell = beep` -> `DisplayBeep` (`vtwin.c:81-91`). What a beep sounds like is the system Sound preferences' choice (Amiga convention; not checked on the rig). | -- |
| Visual bell | all | yes | `bell = visual` (a one-frame flash); profile, Prefs, menu, `/bell` | -- |
| Bell brings the window or screen to front, or marks the tab | iTerm2 (bounce), VTE/kitty (urgent hint), Windows Terminal (`bellStyle: taskbar`) | missing | `cb_bell` has only the three cases (`vtwin.c:81-91`) | S |
| Custom bell sound | iTerm2, kitty `bell_path`, Windows Terminal | missing | -- (could play an 8SVX through datatypes) | S |
| Program notifications (OSC 9 / 777 / 99) | iTerm2, kitty, foot, WezTerm, Ghostty | partial | The text appears in the title for a while (`vtwin.c:139-163`). There is no requester or Ringhio-style popup, no OSC 99, and no on/off setting. | S |
| Notify when a long command finishes; activity/silence monitoring | iTerm2, kitty `notify_on_cmd_finish`, Konsole, WezTerm | missing | OSC 133 D (command end) is parsed but nothing acts on it | M |

## 8. Behaviour

| Setting | Modern terminals | UP-Term | Where / evidence | Effort |
|---|---|---|---|---|
| Confirm closing a window or tab with a running program | iTerm2, Terminal.app, kitty `confirm_os_window_close`, WezTerm, Windows Terminal, GNOME | missing | `close_gadget` sends EOF or CTRL-C at once (`vtcon_handler.c:3946-3957`). There is no requester. | S-M |
| Confirm a multi-line or large paste | iTerm2, Windows Terminal, kitty `paste_actions`, Ghostty | missing | `paste()` types everything (`vtwin.c:1097-1128`) | S |
| Restore windows and tabs at start | iTerm2, WezTerm, Windows Terminal, Terminal.app | missing | No session file (checked: config key list, watch/tab code) | L |
| Working directory of a new tab | all | yes (automatic) | The new tab follows the shell's OSC 7 (`vtcon_handler.c:4304-4310`). There is no "home" or "fixed dir" choice. | S (choice) |
| Shell / startup command per profile | all | partial | The window's command is whatever opened XCON (NewShell, an icon's WINDOW tooltype). A new tab hard-codes vsh, else the Shell (`vtcon_handler.c:4296-4318`). There is no profile `command` key. | S |
| TERM and environment per profile | iTerm2, kitty `env`, WezTerm `set_environment_variables`, Alacritty `env`, Windows Terminal | partial | vsh exports `TERM=vtcon` unless the parent set one (`shell/vsh.c:1295-1303`). There is no profile key. This matters for ssh: no remote host has a vtcon entry. | S |
| Login shell | GNOME, Terminal.app, iTerm2 | N/A | AmigaDOS has no login shell. Shell-Startup or `vshrc` play that part. | -- |
| Keep the window open after the program exits | Windows Terminal `closeOnExit`, GNOME, iTerm2 | partial (spec only) | Spec `WAIT` (`vtcon_handler.c:1015`) | S |
| Program permissions: clipboard (OSC 52) | kitty `clipboard_control`, WezTerm, iTerm2, foot | yes (file only) | Profile `program-clipboard = write \| read-write \| off` (`vtcon_handler.c:928-931`). It survives a Prefs Save (`prefs_core.c:227`) but has no gadget, menu item or slash command. | S |
| Program permissions: resize window, set title, change colours | iTerm2, kitty, Windows Terminal | partial | Only DECCOLM is gated (by `?40`). Title and colour changes are always allowed. | S |
| Tab completion and line editing in the console (Amiga) | -- (beyond modern terminals) | yes | `completion`, `kingcon-*`: profile, Prefs, menus, slash | -- |

## 9. Profiles and configuration

| Setting | Modern terminals | UP-Term | Where / evidence | Effort |
|---|---|---|---|---|
| Several profiles; pick one per window | all | yes | `[profile name]`, spec `PROFILE name`, menu Settings > Profile, `/profile`, Prefs Load/New/Delete | -- |
| A profile inherits from another (default + overrides) | WezTerm (Lua), Windows Terminal (`defaults`), kitty (`include`), Ghostty (`config-file`) | missing | A profile falls back to the built-in values, not to `default` (`dist/up-term.conf:9`; `apply_profile` reads one section) | S |
| Switch profile automatically per host or directory | iTerm2 (Automatic Profile Switching) | missing | No OSC 1337 SetProfile (engine OSC list: 1, 4, 7, 8, 9, 52, 104, 110-112, 133, 777). No host rules. | M |
| Live reload of the config | kitty (signal), Alacritty, WezTerm, Ghostty, foot | yes | A watcher per window re-applies the profile (`vtcon_handler.c:622-631`) | -- |
| Save the window's current settings into the profile | iTerm2 (sort of) | yes | Menu "Save settings to profile", `/save` (`save_ask`, `vtcon_handler.c:1505`). It drops two keys (Found). | -- |
| Import / export settings, foreign theme formats | iTerm2 (.itermcolors, JSON), Windows Terminal (JSON), most (plain file) | partial | The config is a plain text file in `ENV:`/`ENVARC:`, and themes are files. There is no import of `.itermcolors`, Xresources, kitty or Alacritty themes on the Amiga. | S |
| Scriptable control (CLI / command palette) | WezTerm CLI, kitty `@` remote control, iTerm2 Python, Windows Terminal palette | yes | Slash commands plus `C:UPTerm` (`handler/upterm.c`) | -- |
| One GUI for every setting | iTerm2, Terminal.app, GNOME, Windows Terminal | partial | Prefs has gadgets for 21 of the 28 profile keys. `backspace`, `font-aspect`, `program-clipboard`, `link-open`, `screen`, `screen-mode` and `screen-depth` have none. Counted from the `apply_profile` and `apply_colours_rest` reads plus `link-open`, against the `prefs_fields` members that have a gadget in `upprefs.c:440-500`. | S |

---

## Counts per group

| Group | Rows | yes | partial | missing | N/A |
|---|---|---|---|---|---|
| 1. Keyboard | 10 | 3 | 4 | 3 | 0 |
| 2. Mouse | 14 | 5 | 4 | 4 | 1 |
| 3. Text, fonts, cursor | 20 | 6 | 4 | 9 | 1 |
| 4. Colours | 10 | 5 | 0 | 3 | 2 |
| 5. Window and tabs | 16 | 3 | 5 | 7 | 1 |
| 6. Scrollback and search | 10 | 5 | 2 | 3 | 0 |
| 7. Bell and notifications | 6 | 2 | 1 | 3 | 0 |
| 8. Behaviour | 11 | 3 | 4 | 3 | 1 |
| 9. Profiles and configuration | 8 | 4 | 2 | 2 | 0 |
| **Total** | **105** | **36** | **26** | **37** | **6** |

Several "yes" rows have one surface only (the spec or the file). The table cell says
which.

---

## Ranked gaps: what makes UP-Term feel basic to a daily user

The ranking is by how often a daily user runs into the gap, and how badly, in a typical
session (shell, editor, ssh, pager, builds). It is not ranked by effort. Each item says
how it would fit the existing pattern: a profile key in `apply_profile`, a
`prefs_fields` member with a gadget, and a menu item and/or slash command through
`menu_ids.h`, so the menu and the slash command share the action.

1. **Settings disappear and are hard to find.** Saving from Prefs or with "Save settings
   to profile" deletes `backspace` and `font-aspect` from the profile (Found), and Prefs
   has no gadget for 7 keys. Fit: add `backspace`/`font-aspect` to `prefs_fields`,
   `prefs_from_conf` and `prefs_stage`, with a test in `tests/test_prefs.c`. Add a third
   Prefs page, "Keyboard & Mouse", for backspace, meta, wheel, copy-on-select,
   program-clipboard and link-open. Add a "Screen" row for screen, mode and depth. Add
   `/backspace` and `/program-clipboard`.
2. **Reading the scrollback is interrupted by output.** Any write snaps the view to the
   bottom (`vtwin.c:929`). Fit: profile `scroll-on-output = on|off` (VTE's default is
   off), Prefs checkbox, menu Settings > "Scroll to bottom on output", `/scroll-on-output`.
   While it is off, show a "more below" mark.
3. **Closing a window kills the running program without asking.** Fit: profile
   `confirm-close = on|off|busy`. The "busy" test checks whether the shell process's
   `cli_Module` is non-zero (a command is running), then asks with an EasyRequest in
   `close_gadget`. Add a Prefs checkbox. The menu item is not needed.
4. **The wheel does nothing in less, man and git log** because ?1007 is off unless a
   program sets it. Fit: profile `alternate-scroll = on` (default on, as kitty, foot and
   VTE do), set into the engine at attach. Add to the Prefs checkbox group and `/alternate-scroll`.
5. **Plain URLs cannot be clicked.** Only OSC 8 links work. Fit: a URL matcher over the
   row under the pointer in `link_click` (`vtwin.c:1392`), using the existing
   `link-open` command. Profile `link-modifier = ctrl|none` and `detect-urls = on|off`,
   in the Prefs "Keyboard & Mouse" page.
6. **Windows open at a pixel size, and size is not remembered.** Fit: profile
   `size = 80x24` (as `/size`), applied in `parse_spec` when the spec gives no w/h. Add
   `remember-geometry = on`, which writes `geometry = x,y,w,h` on close through the
   existing save path. Add Prefs fields.
7. **Search is minimal.** Fit: highlight every match (the renderer already draws
   selection colours), show "n of m" in the find window, and add Case / Backwards
   switches in the find prompt (`find_open`). Regex comes last (the engine stays C89).
8. **No scrollbar.** Fit: a GadTools/BOOPSI `propgclass` scroller in the right border,
   bound to `vr_set_view`. Profile `scrollbar = on|off`, Prefs checkbox, menu View >
   Scrollbar.
9. **No key bindings.** Fit: profile lines `bind = <qualifiers>+<key> <action|text>`
   (several allowed), where an action is any `MENU_*` or slash command name. This reuses
   the slash table as the action list. Lookup goes in `console_key` before the
   hard-coded keys. Add `/bind` to try a binding live. The Prefs list gadget can come
   later.
10. **Text touches the window border.** Fit: profile `padding = N` (pixels), added to the
    renderer origin (`r.ox`/`r.oy`) like `inset_top`. Add a Prefs field.
11. **No line spacing or cell width.** Fit: profile `line-spacing = N`, `cell-width = N`
    (pixels added to `r.ch`/`r.cw`, with glyphs centred). Add Prefs fields and View menu
    steps. The renderer blits need checking against cells taller than the glyphs.
12. **Shell, startup command and TERM cannot be set per profile.** Fit: profile
    `command = ...` (used by `tab_spawner` instead of the hard-coded vsh/Shell) and
    `term = xterm-256color`, exported to the child as a local variable before vsh starts.
    vsh already keeps a TERM the parent set (`shell/vsh.c:1300`). Add Prefs fields.
13. **Tabs lack the usual controls.** Fit: "New tab with profile >" submenu (the same
    list as Settings > Profile), `/tab rename NAME`, profile `tab-bar = always|auto`, and
    an activity dot on hidden tabs that received output.
14. **Bells and notifications are easy to miss.** Fit: profile `bell-action = front|tab`
    (WindowToFront/ScreenToFront, or mark the tab), and `notify = title|requester`
    for OSC 9/777. `notify-on-finish = N` seconds uses OSC 133 D. Add to Prefs, on the
    Bell row.
15. **Profiles cannot inherit.** Fit: profile key `inherit = default`, resolved in
    `upconf_str` lookup order (profile, then parent, then built-in). This needs care in
    `prefs_stage` so a save does not flatten the profile.
16. **Pasting several lines runs them at once.** Fit: profile
    `confirm-paste = multiline|off`, an EasyRequest in `paste()` when the text has a line
    break and ?2004 is off. Add a Prefs checkbox.
17. **The pointer covers the text while typing.** Fit: profile `hide-pointer = on`;
    `SetWindowPointer(WA_Pointer, blank)` in `vtwin_key`, restored on MOUSEMOVE. That needs
    ReportMouse briefly, or the next button event. Add a Prefs checkbox.
18. **Wheel speed is fixed at 3 lines.** Fit: profile `wheel-lines = N` replaces
    `VTI_WHEEL_LINES`, passed through `vti_wheel`. Add a Prefs field.
19. **The cursor gives no modern focus cues.** Fit: draw a hollow cursor while the window
    is inactive (`vtwin_focus` already gets the event). Add profile `cursor-text = RRGGBB`
    and `cursor-blink-rate = ms`. Add these to the Prefs cursor row and the theme files
    (`cursor-text` is a theme colour).
20. **There is no control over titles.** Fit: profile `title = %t` with `%t` program
    title, `%p` profile, `%d` OSC 7 directory and `%s` size, and `allow-title = on|off`.
    Add a Prefs field.

Below the top 20, in order:

21. Bold colour, bold weight on/off (`bold-color`, `bold-font = on|off`), blinking text
    on/off (`text-blink`), minimum contrast.
22. Rectangular selection (Alt + drag); configurable double-click word characters
    (`word-chars`).
23. Save the scrollback to a file; copy the last command's output (OSC 133 marks).
24. Encoding and emulation as profile keys (`personality`, `charset`); Delete key code;
    Meta on Left Alt only (`meta = left-alt`).
25. Scrollback beyond 100000 lines; a mouse-reporting off switch; palette entries
    16-255; tab bar colours.
26. Automatic profile per host (OSC 1337 SetProfile or host rules), a hotkey drop-down
    window (commodity), splits (tmux covers them), session restore.

Some features are not gaps, because UP-Term follows the platform as modern terminals
follow theirs:

- Key repeat rate and delay come from Input prefs, and DECARM is honoured.
- Double-click time comes from Input prefs.
- Dead keys come from the keymap.
- The beep sound comes from Sound prefs.
- Transparency, ligatures, login shells and automatic light/dark themes do not apply on
  OS 3.x.

## Found while reading (not asked)

- **Bug: saving drops profile keys.** `prefs_stage` (`prefs/prefs_core.c:174`) removes
  the whole profile with `upconf_rmprof` (`config/upconf.c:128`). It then writes back
  only the `prefs_fields` members. `backspace` (`vtcon_handler.c:941`) and
  `font-aspect` (`:881`) are not members (`prefs/prefs_core.h`), so both are lost on
  every Prefs Save or Use and every menu "Save settings to profile" (`save_ask` ->
  `window_fields` -> `prefs_from_conf` -> `prefs_stage`, `vtcon_handler.c:1505-1517`).
  Any key typed by hand that the editor does not know is lost the same way. I found this
  by reading the source. It was not run, and `tests/test_prefs.c` has no case for either
  key (grep). A root-cause fix would carry every key the editor does not model through
  the save, rather than adding the two members.
- `dist/up-term.conf` documents `link-open` and `program-clipboard`, which have no Prefs
  gadgets. Users who never open the file never see them.
- `2026-10-04_modern-terminal-gaps.md` is stale on at least the following. Double-click
  word and triple-click line selection exist (`vtinput.c:133-183`). Middle-click paste
  exists. ?1004 focus reports are sent (`vtwin.c:1528`). ?1007 exists. The kitty
  keyboard protocol exists (`vtwin.c:1216`). OSC 7, 8, 9, 52, 133 and 777 are handled.
  The clipboard writes UTF8 alongside CHRS (`handler/clip.c:1-3`). Its gap list should
  be re-checked before anyone plans from it.
