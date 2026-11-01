---
date: 2026-10-21
topic: Amarchy -- Omarchy's Super bindings and menu, mapped onto UP-Term and the Amiga Amiga keys
tags: [research, amarchy, omarchy, keybindings, vtcon-handler, amiga-keys, keymap, menu, upkg-ui]
status: final
---

# Amarchy: Omarchy's Super system on the Amiga

Source read: github.com/basecamp/omarchy at commit `82662dd2627db3b22c63616d3cae025aeb0f1e47`
(HEAD of master, 2026-10-21T16:33Z), raw files via the contents API. Files: `default/hypr/bindings/*.lua`
(applications, clipboard, dictation, input-method, jis, media, tiling, utilities), `default/omarchy/omarchy-menu.jsonc`,
`config/omarchy/extensions/omarchy-menu.jsonc`, `bin/omarchy-menu`, `docs/menu.md`. Not read: `shell/plugins/menu/Menu.qml`
(rendering), `omarchy-menu-keybindings` (the viewer; it lists live Hyprland binds with their description strings, which is
why every `o.bind` carries a description). Tree note: Omarchy has moved from Hyprland `.conf` files to Lua
(`o.bind(keys, description, action, opts)`); older blog posts and videos show the `.conf` form.

Counts (grep of `o.bind|o.bind_toggle|o.bind_hold` call sites, per file): applications 30, clipboard 5, dictation 3,
input-method 3, jis 1, media 28, tiling 78, utilities 61 = 209 call sites. Three loops expand (workspaces 1-10 x3 =
+27, bar panels 1-9 = +8, group windows 1-5 = +4), so about 248 actual bindings, of which about 35 are XF86/Print/lid/
mouse hardware keys and about 165 carry SUPER. Hand-written summary below groups the repeats.

## 1. Omarchy Super bindings

Keys: `S` = SUPER, `Sh` = SHIFT, `C` = CTRL, `A` = ALT. `code:10..19` are the digit row 1..0, `code:20/21` are minus/equal.

### Launcher and menu
| Key | Action | Category |
|---|---|---|
| S+Space | Omarchy menu (root) | menu |
| S+A+Space | Apps launcher (dock apps) | launcher |
| S+Esc | System menu | menu |
| S+C+E | Emoji panel | launcher |
| S+C+C | Capture menu | menu |
| S+C+O | Toggle menu | menu |
| S+C+H | Hardware menu | menu |
| S+K | Keybinding viewer | menu |
| S+A+K / S+C+K | Tmux / Herdr keybinding viewer | menu |
| S+C+Space, S+C+W | Background switcher | style |
| S+Sh+C+Space, S+C+T | Theme menu | style |
| S+Sh+Space | Toggle top bar | style |
| S+Backspace / S+Sh+Backspace / S+C+Backspace | Window transparency / gaps / single-window aspect toggle | style |
| S+C+A+F | Toggle full screen desktop | style |
| S+C+V | Clipboard manager | launcher |

### Apps
| Key | Action | Category |
|---|---|---|
| S+Return | Terminal | apps |
| S+Sh+Return, S+Sh+B | Browser (S+Sh+A+B private) | apps |
| S+Sh+F (+A) | File manager (cwd) | apps |
| S+Sh+N | Editor | apps |
| S+A+Return | Tmux terminal | apps |
| S+C+Return | Herdr terminal | apps |
| S+Sh+Q | Calculator | apps |
| S+Sh+M (+A) | Music (TUI) | apps |
| S+Sh+D | Docker TUI | apps |
| S+Sh+G, S+Sh+O, S+Sh+W, S+Sh+Slash | Signal, Obsidian, Omawrite, Passwords | apps |
| S+Sh+A (+A), C, E (+A), Y, P, S, X (+A), A+G, C+G | Web apps: ChatGPT, Grok, Calendar, Email (new), YouTube, Photos, Maps, X (post), WhatsApp, Google Messages | apps |
| S+Sh+C+A | Agent picker | apps |
| S+Sh+T | Activity (btop) | apps |
| S+C+A / B / D / N / P | Audio / Bluetooth / Display / Network / Power panel | system |
| S+C+A+D / S+C+A+E | Calendar / world clock | system |
| S+C+1..9 | Bar panel 1..9 | system |

### Window
| Key | Action | Category |
|---|---|---|
| S+W, S+Q | Close window | window |
| C+A+Delete | Close all windows | window |
| S+J | Toggle split | window |
| S+P | Pseudo window | window |
| S+T | Floating/tiling toggle | window |
| S+F, S+C+F, S+A+F | Full screen, tiled full screen, full width | window |
| S+O | Pop out (float and pin) | window |
| S+Home, S+A+Home | Restore / save window width | window |
| S+L, S+A+L | Cycle workspace layout (back) | window |
| S+Arrows | Focus left/right/up/down | window |
| S+Sh+Arrows | Swap window | window |
| S+minus / S+equal (+Sh, +A, +C) | Resize left/right/up/down; A = small step 25, C = large step 300, none = 100 | window |
| A+Tab, A+Sh+Tab | Next / previous window | window |
| S+G, S+A+G | Group toggle, ungroup | window |
| S+A+Arrows | Move window into group | window |
| S+A+Tab, S+A+Sh+Tab, S+C+Left/Right, S+A+wheel | Next/previous window in group | window |
| S+A+1..5 | Group window 1..5 | window |
| S+Left mouse drag, S+Right mouse drag | Move / resize window | window |
| S+M | Minimize to dock | window |
| S+Slash, S+A+Slash | Monitor scaling up / down | style |
| S+Sh+C+Arrows, S+Sh+A+Arrows | Move window / workspace to monitor | window |
| C+A+Tab, C+A+Sh+Tab | Focus next / previous monitor | window |

### Workspace
| Key | Action | Category |
|---|---|---|
| S+1..0 | Switch to workspace 1..10 | workspace |
| S+Sh+1..0 | Move window to workspace | workspace |
| S+Sh+A+1..0 | Move window silently | workspace |
| S+Tab, S+Sh+Tab, S+C+Tab | Next, previous, former workspace | workspace |
| S+S, S+grave | Toggle scratchpad | workspace |
| S+A+S, S+Sh+grave | Move window to scratchpad | workspace |
| S+wheel up/down | Scroll workspaces | workspace |

### System, clipboard, notifications
| Key | Action | Category |
|---|---|---|
| S+C+L | Lock | system |
| S+C+I | Toggle idle lock | system |
| S+C+Delete, S+C+A+Delete | Laptop display toggle / mirror | system |
| S+, (comma) family | Dismiss / dismiss all / silence / invoke / history of notifications | system |
| S+C+R, S+C+A+R, S+Sh+C+R | Set / show / clear reminders | system |
| S+C+A+T / B / W | Show time / battery / weather | system |
| S+C+S | Share menu | system |
| S+C+. (period) | Transcode | system |
| S+C+Z, S+C+A+Z | Zoom in / reset | style |
| S+C+X | Toggle dictation (only if a backend is installed) | system |
| S+I, S+Sh+I | Switch input language (forward/back) | system |
| S+A, S+C, S+V, S+X | Select all, copy, paste, cut (copy/paste send Ctrl+Shift+C/V in a terminal) | system |
| S+C+A+C, Print, S+Print, S+C+Print, A+Print, S+A+code:34/35 | Screenshot, colour picker, OCR, screenrecording, webcam resize | system |
| XF86 volume/brightness/media/touchpad/lid | Hardware keys, 25 or so | system |

## 2. Omarchy menu: structure and definition

Format: one JSONC object, keys are item ids, the dotted id is the tree (`trigger.share.file` is under `trigger.share`; no
dot = on the root menu). No parent field. Kind is inferred: has `action` = action, has `target` = link to another
submenu, neither = submenu. Fields: `icon`, `iconFont`, `label`, `title`, `action` (shell command, run detached), `target`,
`provider` (a shell-defined runtime row source such as fonts, apps, language), `aliases` (extra route names, searchable),
`description` (subtitle + search text), `when` (shell condition, row hidden if it fails), `checked` (shell condition,
appends a tick), `disabled` (stays listed, dimmed). Whole-line `//` comments only; an inline comment breaks the parse and
drops the whole file silently.

Files: shipped `default/omarchy/omarchy-menu.jsonc` (452 lines, read from `$OMARCHY_PATH`) overlaid per key by the user's
`~/.config/omarchy/extensions/omarchy-menu.jsonc`. A reused id replaces only the fields given. Reload on file change,
`omarchy menu refresh`. `omarchy menu [toggle|summon|close|refresh|ping] [route]` opens at a route (id or alias, e.g. `power`).

Root, in file order: Apps (provider; search all apps), Learn, Appearance, Trigger, Setup, Install, Remove, Update, About,
System. Second level:
- Learn: Keybindings, Omarchy, Hyprland, Arch, Neovim, Bash, Tmux, Herdr, Community (web manuals and the viewers).
- Appearance: Theme, Background, Font, Menu Bar, Windows (gaps, corners, opacity, 1-window ratio), Branding, Dock, Workspace Layout, Animations, Unlock.
- Trigger: Toggle (stay awake, notifications, nightlight, screensaver, crash capture), Emoji, Clipboard History, Reminder, Capture (screenshot, screenrecord, text, QR, colour), Transcode, Share, Hardware, Speed Test.
- Setup: Monitors, Keybindings, Input, Typing, Language, Network (DNS), Default (agent, browser, dictation, terminal, editor), Plugin, Security, Config, Direct Boot, Reset Computer.
- Install: Package, AI, Service, Development, Editor, Style, Gaming, Browser, Webapp, Terminal, TUI, Windows, AUR, Preinstalls. Remove: the same plus Dictation, Theme, Security.
- Update: Omarchy, Channel, Kernel, Config, Themes, Process, Hardware, Firmware, Password, Timezone, Time.
- System: Screensaver, Lock, Suspend, Hibernate, Logout, Reboot, Shutdown.

Bindings and menu are two files in Omarchy: bindings in Lua (the viewer reads live Hyprland binds), menu in JSONC. Amarchy's
owner decision is one file for both; Omarchy gives no precedent for that, only for the description-string-per-bind idea.

## 3. Equivalents in UP-Term / Amiga

Rewritten 2026-10-25 against OS 3.2 (owner disputed the earlier "~40 of ~165" claim; it was a surface read). Full row-by-row
evidence: `thoughts/shared/research/2026-10-25_amarchy-os32-gap-table.md` (rows A1-A87 bindings, B1-B55 menu rows, Table C).
Verified in `vendor/ndk-3.2r4-Include_H/clib/intuition_protos.h`: MoveWindow, SizeWindow, ChangeWindowBox, ZipWindow, WindowToFront,
WindowToBack, ActivateWindow, MoveWindowInFrontOf, ScreenToFront, ScreenToBack, ShowTitle, LockPubScreen, WBenchToFront; and in
wb_protos/workbench.h: OpenWorkbenchObjectA, CloseWorkbenchObjectA, WorkbenchControlA, AddAppMenuItemA. The WORKBENCH ARexx command
list (ACTIVATEWINDOW, CHANGEWINDOW, MOVEWINDOW, SIZEWINDOW, ZOOMWINDOW, UNZOOMWINDOW, WINDOWTOFRONT, WINDOWTOBACK, WINDOW, MENU, NEWDRAWER,
INFO, RENAME, DELETE, ICON, VIEW, KEYBOARD, LOCKGUI, UNLOCKGUI, RX, GETATTR, FAULT, HELP) is from the Hyperion wiki page only: no ARexx
documentation for the Workbench port is in vendor/ or thoughts/, and the NDK headers carry no command names. Marked unverified:
that every command exists in 3.2r4, the exact MENU argument syntax, and whether the WINDOW commands reach non-Workbench windows
(the wiki says drawer windows, ROOT and ACTIVE). Nothing here was run on a rig.

Dispatch today: `KA_REXX` -> `LF_REXX` (amarchy/leaf_core.c:15, leaf.h) runs RX "script" from the commodity; other kinds are
`KA_MENU/VIEWER/TAB/MENUID/SLASH/RUN/OPEN/DETACH/SCAN` (config/keymap.h:68-80). A `rexx:` row costs no C: `rexx:address WORKBENCH 'WINDOW ACTIVE CYCLE'`
(needs RexxMast running; the exact RX string form is unverified). Intuition calls on windows of OTHER programs cannot be
reached from ARexx and need one NEW C action kind, `KA_WIN:<verb>` (verbs: next, prev, front, back, zip, fullwidth, save, restore,
focus-left/right/up/down, swap-*, nudge/grow-*), running under LockIBase in the commodity task. CloseWindow on a foreign window is
unsafe (the owner holds the IDCMP port): closing stays with the owner (break signal) or the Workbench ARexx WINDOW ... CLOSE for drawers.

| Omarchy binding | Omarchy effect | OS 3.2 equivalent | Amarchy row kind | Group |
|---|---|---|---|---|
| S+Space, S+Esc, S+K, S+A+K | root / system menu, key viewer | Amarchy itself | existing menu: / viewer | none |
| S+A+Space, S+Sh+Return, S+Sh+B, S+Sh+A.. web apps | apps launcher, browser | OpenWorkbenchObjectA; app index | existing run: / index | Apps |
| S+Return, S+A+Return, S+Sh+N, S+Sh+M, S+Sh+Q.. | terminal, tmux, editor, music | UPTerm, tmux, nvim | existing open: / run: | Apps |
| S+Sh+F (+A) | file manager | WORKBENCH WINDOW ROOT OPEN (unverified syntax) | existing rexx: | Workbench |
| S+Sh+T | activity | WorkbenchControlA GetProgramList; Status | existing run: Status | System |
| S+C+A/B/D/N/P, S+C+H, S+C+A+D/E | audio..power panels | Prefs: Sound, ScreenMode, Input, Internet, Time | existing run: / rexx: OPEN | Workbench |
| S+C+E, S+C+V, S+X | emoji, clipboard, cut | dist/emoji, clipboard.device | existing run: / slash: ; cut = NEW | System |
| S+A, S+C, S+V; S+C+Z; S+C+Space/T | select/copy/paste, zoom, theme | MENU_*, /font-size, /theme | existing menuid: / slash: | Style |
| S+C+O | toggle menu (backdrop) | WORKBENCH MENU .. INVOKE (name form unverified) | existing rexx: | Workbench |
| S+Sh+Space | toggle top bar | ShowTitle(screen, FALSE) | NEW C (KA_WIN verb) | Workspace/Screens |
| S+W, S+Q | close window | own tab: tab:close; drawer: WORKBENCH WINDOW ACTIVE CLOSE | existing tab: / rexx: | Window |
| C+A+Delete | close all | WBCTRLA_GetOpenDrawerList, CloseWorkbenchObjectA each | NEW C | Window |
| S+F, S+C+A+F | full screen | WORKBENCH WINDOW ACTIVE MAX; /screen full | existing rexx: / slash: | Window |
| S+A+F | full width | ChangeWindowBox(left 0, screen width) | NEW C | Window |
| S+Home, S+A+Home | restore / save width | UNZOOMWINDOW (rexx); saved box = ChangeWindowBox | existing rexx: / NEW C (save) | Window |
| S+M | minimize | WORKBENCH WINDOW ACTIVE MIN, WINDOWTOBACK | existing rexx: (foreign: NEW back) | Window |
| A+Tab, A+Sh+Tab | next / prev window | WORKBENCH WINDOW ACTIVE CYCLE (drawers); foreign: Screen->FirstWindow walk + ActivateWindow + WindowToFront | existing rexx: / NEW C | Window |
| S+Arrows | focus direction | geometry search + ActivateWindow | NEW C | Window |
| S+Sh+Arrows | swap windows | 2x ChangeWindowBox | NEW C | Window |
| S+minus/equal (+Sh,A,C) | resize 25/100/300 | SizeWindow / MoveWindow (relative); ARexx SIZEWINDOW absolute | existing rexx: (drawers) / NEW C | Window |
| S+Left/Right mouse drag | move / resize | commodity input handler + MoveWindow / SizeWindow | NEW C (handler, larger) | Window |
| S+S, S+grave | scratchpad toggle | WindowToBack / ScreenToBack of one chosen window | NEW C | Window |
| S+1..0, S+Tab, S+Sh+Tab, S+C+Tab, C+A+Tab | workspaces / monitors | tabs 1-9; ScreenToFront / ScreenToBack on public screens | existing tab: / NEW C (screens) | Workspace/Screens |
| S+C+L | lock | LOCKGUI / UNLOCKGUI (drawers only, no real lock) | existing rexx: | System |
| S+C+I, B19/B49 screensaver | idle lock / blanker | Blanker commodity enable / disable | NEW C (broker command) | System |
| S+C+Delete.. , S+I, S+Sh+I | input language | Prefs Input / Locale | existing run: | System |
| Workbench menu: Backdrop, Execute Command, Redraw All, Update All, Last Message, About, Open volume (unverified), Quit | not in Omarchy | WORKBENCH MENU WINDOW ROOT INVOKE ... | existing rexx: (8 rows) | Workbench |
| Window menu: New Drawer, Open Parent, Close, Update, Select Contents, Clean Up, Snapshot, Show, View By, Sort Order (unverified) | not in Omarchy | NEWDRAWER; MENU INVOKE; VIEW | existing rexx: (10 rows) | Window |
| Icons menu: Open, Copy, Rename, Information, Snapshot, UnSnapshot, Leave Out, Put Away, Delete, Format Disk, Empty Trash | not in Omarchy | MENU INVOKE on selection; RENAME, DELETE, INFO | existing rexx: (11 rows) | Workbench |
| Tools menu: ResetWB, user items | not in Omarchy | MENU INVOKE; AddAppMenuItemA for an Amarchy entry | existing rexx: / NEW C (AppMenuItem) | Workbench |
| S+J, S+P, S+T, S+C+F, S+O, S+L, S+A+L, S+G, S+A+G, S+A+Arrows, S+A+1..5, S+Backspace family | tiling: split, pseudo, float, layouts, groups, gaps, opacity | none: Intuition has no tiling layout engine or compositor | none (out) | none |
| S+Sh+1..0, S+Sh+A+1..0, S+A+S, S+Sh+C+Arrows | move window to workspace / scratchpad / monitor | none: a window cannot change screen | out | none |
| Print family, S+, family, reminders, weather, dictation, share, transcode, docker, signal, XF86 keys, S+wheel, laptop display | Linux desktop | none stock on 3.2 | out | none |

Counts (Table A of the research file, 87 binding rows, one row may stand for several keys): 50 have an equivalent, 37 stay out.
Of the 50: 27 reuse an existing kind (menu/run/slash/tab/open), 10 reuse `rexx:`, 13 need new C (one `KA_WIN` kind plus ShowTitle, screens, Blanker, clipboard cut).
Table C adds about 30 Workbench menu rows (Workbench, Window, Icons, Tools) that Omarchy lacks: all `rexx:`, no new C, once the MENU syntax is verified on a rig.
Proposed groups: Window (about 22 rows), Workbench (about 30), Workspace/Screens (about 6); System and Apps exist already.
Cut first: (1) all tiling rows (split, group, layout, gaps, pseudo), (2) move-to-workspace/monitor, (3) the mouse-drag input handler (largest new C for the least use).

## 4. The modifier on the Amiga

What the handler does with Amiga keys today (grep of handler/ and render/):
- Left Amiga = Meta (xterm ESC prefix) for programs inside the terminal: render/vtwin.c:1360 comment, `vti_mods`, and the key
  is stripped before the keymap (render/vtwin.c:1556). With `meta = alt` (up-term.conf) Alt takes that role instead and
  Left Amiga is free. So Left Amiga + letter reaches vsh, vim, nvim, tmux and Claude as ESC+letter (vsh line editor M-b, M-f).
- Right Amiga: Intuition menu shortcuts. The menu defines T (new tab), . and , (next/previous tab), L (Claude), C, V, A, F, G, K
  (clear screen), + and - (font). Intuition turns those into IDCMP_MENUPICK before the handler sees a RAWKEY; only keys with
  no command key reach the handler as RAWKEY.
- Handler-owned Right Amiga keys: F (find prompt, vtcon_handler.c:5264, shadowed by the menu item), 1-9 (switch tab, only with
  more than one tab, vtcon_handler.c:5972), C/V copy/paste (render/vtwin.c:1387), Up/Down scrollback, Shift+Up/Down previous/next prompt.
- Shift+Left/Right = Home/End (amiga-keys). Nothing else uses Amiga keys in vsh (handler/lineedit.c has no COMMAND qualifier).
- tmux: prefix is the default Ctrl-B (dist/tmux.conf sets no prefix; lesson 13 teaches Ctrl-B). No Amiga-key use.

Intuition and Workbench clashes (from OS knowledge, mark unverified until a rig test): Left Amiga+M sends the front screen
to back, Left Amiga+N brings the Workbench screen to front; Ctrl+Left Amiga+Right Amiga = hardware reset (so no binding
may need Ctrl while both Amigas are held); Left Amiga + mouse drag moves a window and Left Amiga + Right mouse
button is a window-menu/depth shortcut on 3.x with some Prefs settings; Workbench has Right Amiga menu shortcuts (O open,
I info, N new drawer, R rename...) when a Workbench window has focus, which does not apply inside a UPTerm window; many
applications use Right Amiga Q/W/N etc.

Candidates:
- Left Amiga as Super: collides with Meta (breaks vsh/vim/tmux M- keys in the one place Amarchy works) and with Left Amiga M/N.
  Only viable if `meta = alt` is made the default, an incompatible change.
- Right Amiga as Super: no Meta clash; Intuition already hands unused Right Amiga keys to the handler. Clashes: the ten menu
  command keys above (including K, which the owner wants for the viewer) and every Right Amiga key a program running in
  the tab might want (none do in the terminal, since Right Amiga does not reach terminal programs today).
- Both Amigas held: unambiguous and clash-free with menu shortcuts, but cannot combine with Ctrl (reset), awkward to chord.

Safest choice for the first version: Right Amiga is Super.
- Menu (S+A+Space): Right Amiga + Space has no menu item and no known Intuition use; the handler gets the RAWKEY (code 0x40).
  Use plain Right Amiga+Space for the menu and Right Amiga+Shift+Space for the "Apps" launcher (S+Sh+Space slot).
- Viewer (S+K): Right Amiga+K is taken by Clear screen. Either move Clear screen to Right Amiga+Shift+K (Ctrl-L already
  clears for shells) and give Right Amiga+K the viewer, or give the viewer Right Amiga+Help (a real key, no clash, and it
  is the Amiga's help key). Owner decision.
- The first release can only intercept in a focused UPTerm window; keys must be matched in the handler before vtwin_key,
  next to the existing find_open hook (vtcon_handler.c:5264). A command key given to a menu item disappears from the
  handler: the keymap file must either own the menu command keys or declare them reserved.

## 5. Keymap file

Existing config style: UP-Term uses plain INI-like text files, parsed by C with no JSON, one place each:
`dist/up-term.conf` -> `ENVARC:up-term/up-term` (config/upconf.c: `[profile name]` sections, `key = value`, `;` comments,
fixed-size struct, no allocation, portable C89, host-tested in tests/test_upconf.c, comments kept on write-back);
`dist/upkg.conf` and `ENVARC:upkg`; `ENVARC:up-term/Dir` (one line). Helpers read ENV: first and save to ENVARC:.
JSON would add a parser and a heap; do not use it.

Proposal: reuse upconf's tokenising and put the keymap at `ENVARC:up-term/keymap` (copy to `ENV:up-term/keymap`, same
mechanism as up-term), shipped default `dist/keymap` installed beside `dist/up-term.conf`. Format: sections are the menu tree
and the bindings together, one line per item, so the viewer and menu are generated from the same rows.

    ; Amarchy keymap. Super = Right Amiga.
    [bind]
    ; key             = action            ; label (the viewer shows it)  | category
    super+space       = menu:root         ; Menu                         | menu
    super+help        = viewer            ; Keybindings                  | menu
    super+t           = tab:new           ; New tab                      | window
    super+1..9        = tab:goto %n       ; Switch to tab %n             | workspace
    super+shift+s     = slash:/screen own ; Own screen                   | style
    super+return      = run:tmux          ; Tmux                         | apps

    [menu]
    ; id (dotted tree, as Omarchy)          = label | action
    apps.upkg         = Install add-ons | run:upkg-browse
    style.theme       = Theme           | slash:/theme

Why this shape: one line per item with a fixed `key = action ; label | category` layout parses with a line scanner and a
split on `=` and `|` (no recursion, no tokeniser beyond upconf's); the dotted menu id gives the tree without a parent field
(Omarchy's own choice); `1..9` and `%n` are the only expansions (the digits loop). Actions are a small closed set of
prefixes: `menu:` / `viewer`, `tab:` and the existing `MENU_*` names, `slash:` (reuse handler/slash.c's parser, so every
"/x" command is bindable for free), `run:` (spawn a command in a new tab or the current shell), `rexx:` (later, for the
commodity). Key names are lower-case words (`space`, `help`, `return`, `tab`, `f1`..`f10`, `up`..`right`, digits and letters by
position, i.e. raw codes as the tab code does at vtcon_handler.c:5972, so a Swedish keymap does not move them).
The same file read by the later commodity gives the system-wide variant without a second definition.

The menu: upkg-ui `--menu` (dist/upkg-browse already drives it, lines 211-214) is a dialog-style list; a filter field is a
new `upkg-ui` feature (inputbox + menu loop, or a `--filter` flag on `--menu`), not found in dist/ today.

## Decisions the owner must make

1. Super = Right Amiga (recommended), Left Amiga (needs Meta moved to Alt by default), or both held.
2. Viewer key: move Clear screen off K and use Right Amiga+K, or use Right Amiga+Help.
3. Who owns the menu command keys: generated from the keymap into the NewMenu array (single source, recommended) or declared reserved.
4. Which actions are in scope: only the ~40 with an equivalent (tabs, fonts, themes, find, upkg, Claude, tmux), or also placeholders for tiling (tmux panes) and Workbench launches (Prefs programs).
5. Action language and file location: closed prefix set (`menu:`, `tab:`, `slash:`, `run:`) in `ENVARC:up-term/keymap` as above, with `slash:` reusing the existing slash table.

## Clash risks

- Right Amiga+K (Clear screen), plus T . , L C V A F G + - already belong to the menu; a binding on them never reaches the handler.
- Ctrl while both Amigas are held is the hardware reset; never bind Super+Ctrl if Super could mean both Amigas.
- Left Amiga is Meta for vim/vsh/tmux; Left Amiga+M/N are screen cycling (unverified from OS knowledge, test on rig).
- Alt is a keymap modifier (national characters), so Super+Alt combos need the key tested on the Swedish keymap.
- Keys reach only the focused UPTerm window; nothing works with another window active until the commodity exists.
