---
date: 2026-10-25
topic: Amarchy - AmigaOS 3.2 equivalents for every Omarchy Super binding and menu row, and Workbench features Omarchy lacks
status: research
---

# AmigaOS 3.2 gap table

Checks the claim in `2026-10-21_amarchy-super-keys.md` section 3 ("about 40 of roughly 165 bindings have an equivalent, the rest are tiling or Linux-desktop only"). Result: the claim is too low. See counts at the end.

Sources and verification:
- V-NDK = verified in `vendor/ndk-3.2r4-Include_H` (clib/intuition_protos.h, clib/wb_protos.h, workbench/workbench.h, intuition/screens.h, clib/commodities_protos.h): MoveWindow, SizeWindow, ChangeWindowBox, WindowToFront, WindowToBack, ActivateWindow, ZipWindow, ScreenToFront, ScreenToBack, ScreenDepth (SDEPTH_TOFRONT/TOBACK/INFAMILY), CloseWindow, ShowTitle, ScreenPosition, LockPubScreen, PubScreenStatus, OpenWorkbenchObjectA, CloseWorkbenchObjectA, WorkbenchControlA (WBCTRLA_IsOpen, GetSelectedIconList, GetOpenDrawerList, GetProgramList, GetHiddenDeviceList, Add/RemoveHiddenDeviceName ...), AddAppIconA, AddAppMenuItemA, AddAppWindowDropZoneA, ChangeWorkbenchSelectionA, MakeWorkbenchObjectVisibleA, UpdateWorkbench, WBInfo, CxBroker, ActivateCxObj.
- V-WEB(arexx) = https://wiki.amigaos.net/wiki/Workbench_ARexx_Port (fetched 2026-10-25). The page lists 23 commands: ACTIVATEWINDOW, CHANGEWINDOW, DELETE, FAULT, GETATTR, HELP, ICON, INFO, KEYBOARD, LOCKGUI, MENU, MOVEWINDOW, NEWDRAWER, RENAME, RX, SIZEWINDOW, UNLOCKGUI, UNZOOMWINDOW, VIEW, WINDOW, WINDOWTOBACK, WINDOWTOFRONT, ZOOMWINDOW. It does NOT say which OS version added each (unverified whether all 23 exist on 3.2; the page is the Hyperion wiki for the current Workbench). Port name WORKBENCH (from the page). Needs rexxsyslib.library and RexxMast running.
- V-WEB(menus) = https://wiki.amigaos.net/wiki/AmigaOS_Manual:_Workbench_Using (older manual, no shortcuts listed; not guaranteed to match 3.2). Plus a forum/feature-list search result naming 3.2 additions "Open volume" (Workbench menu) and "Sort Order" (Window menu) - unverified, search snippet only.
- The ARexx WINDOW/ACTIVATEWINDOW/CHANGEWINDOW/WINDOWTOFRONT commands act on Workbench drawer windows (and ROOT/ACTIVE) per the wiki; they are NOT verified to reach windows of other programs. Foreign windows need the Intuition calls below (unverified at runtime; not run on a rig).

Window-call safety (applies to every window row):
- The Intuition calls on a foreign Window pointer need LockIBase to walk Screen->FirstWindow and to be sure the window is not closing; the pointer can die after UnlockIBase. Move/Size/ChangeWindowBox/ZipWindow/WindowToFront/Back/ActivateWindow only queue a request to Intuition, so they are the normal way commodities (ClickToFront, IHelp) work. Safe enough with the lock held.
- CloseWindow on another task's window is UNSAFE: the owner has the IDCMP port, gadgets, menus and requesters; closing it from outside leaves the owner with a dangling pointer. The Amiga way is to ask the owner: for a Workbench drawer window use WORKBENCH WINDOW <name> CLOSE or CloseWorkbenchObjectA; for any other program send it a break (Signal CTRL_C) or none.
- Windows are not workspaces: Amiga has screens (ScreenToFront/Back, ScreenDepth) and the Workbench screen is one desktop; "workspace N" maps to screens only if each is a real public screen.

## Table A: Omarchy Super bindings (section 1 of the 10-21 file)

"Already" = in dist/keymap [menu] (rows: apps.upterm, apps.tmux, apps.editor, apps.rescan, apps.claude, style.theme/font/background, window.tab, window.preferences, learn.keys/tutorial/help/demo, system.reboot/workbench/own, install.browse/upgrade/update) or a slash command in handler/slash.c.

| # | Omarchy key | Omarchy action | AmigaOS 3.2 mechanism | Already in Amarchy | Verified |
|---|---|---|---|---|---|
| A1 | S+Space | Omarchy menu | none: this is Amarchy itself (the launcher) | yes: the menu | n/a |
| A2 | S+A+Space | Apps launcher | OpenWorkbenchObjectA(path) / WORKBENCH ICON ... OPEN; amarchy-index scan | yes: apps.rescan + app index | V-NDK |
| A3 | S+Esc | System menu | Amarchy group System | yes: system.* rows | n/a |
| A4 | S+C+E | Emoji panel | none Workbench-native; dist/emoji + clipboard.device | no | unverified |
| A5 | S+C+C | Capture menu | none: no screen-capture API (screenshot needs a tool such as a Wasabi-style grab) | no | unverified |
| A6 | S+C+O | Toggle menu | Workbench menu Backdrop; Prefs toggles via run: | no | V-WEB(menus) |
| A7 | S+C+H | Hardware menu | Prefs programs (Sound, Screenmode, Input, Serial, Printer) | no | V-WEB |
| A8 | S+K | Keybinding viewer | Amarchy viewer | yes: learn.keys | n/a |
| A9 | S+A+K / S+C+K | Tmux / Herdr key viewer | none Amiga-specific; tmux list-keys in a tab | no | n/a |
| A10 | S+C+Space, S+C+W | Background switcher | Prefs WBPattern; /bg | yes: style.background (colour only) | V-WEB |
| A11 | S+Sh+C+Space, S+C+T | Theme menu | /theme | yes: style.theme | V-NDK n/a |
| A12 | S+Sh+Space | Toggle top bar | ShowTitle(screen, FALSE/TRUE) on the Workbench screen; WORKBENCH MENU is unaffected | no | V-NDK |
| A13 | S+Backspace | Window transparency | none: no compositor | no | n/a |
| A14 | S+Sh+Backspace | Gaps toggle | none: tiling layout engine | no | n/a |
| A15 | S+C+Backspace | Single-window aspect toggle | none: tiling layout engine | no | n/a |
| A16 | S+C+A+F | Toggle full screen desktop | ScreenToFront/ScreenToBack; WORKBENCH WINDOW ... MAX; Workbench menu Backdrop | partial: system.workbench / system.own | V-NDK |
| A17 | S+C+V | Clipboard manager | clipboard.device (unit 0..255); /paste | partial: /paste, /copy | unverified |
| A18 | S+Return | Terminal | OpenWorkbenchObjectA("SYS:.../UPTerm") or run: | yes: apps.upterm | V-NDK |
| A19 | S+Sh+Return, S+Sh+B | Browser | OpenWorkbenchObjectA on the installed browser (IBrowse/Odyssey/AWeb); app index | yes if scanned: apps index | unverified |
| A20 | S+Sh+F (+A) | File manager | OpenWorkbenchObjectA("SYS:") / WORKBENCH WINDOW ROOT OPEN / Directory Opus | no | V-NDK |
| A21 | S+Sh+N | Editor | run: nvim / OpenWorkbenchObjectA("SYS:Tools/MEmacs") | yes: apps.editor | n/a |
| A22 | S+A+Return | Tmux terminal | run:tmux | yes: apps.tmux | n/a |
| A23 | S+C+Return | Herdr terminal | none: no herdr port | no | n/a |
| A24 | S+Sh+Q | Calculator | none shipped in 3.2 (Tools has none); app index finds a 3rd-party one | no | unverified |
| A25 | S+Sh+M (+A) | Music TUI | app index (music disk entries) | yes: music groups (d675aaf1) | n/a |
| A26 | S+Sh+D | Docker TUI | none: Linux-only | none | n/a |
| A27 | S+Sh+G, O, W, Slash | Signal, Obsidian, Omawrite, Passwords | none: Linux-only apps | none | n/a |
| A28 | S+Sh+A,C,E,Y,P,S,X,A+G,C+G | Web apps (ChatGPT, Grok, Calendar, ...) | browser launch with URL via OpenWorkbenchObjectA + WBOPENA arguments or /link-open | no | unverified |
| A29 | S+Sh+C+A | Agent picker | MENU_CLAUDE; dist/addons/pi | yes: apps.claude | n/a |
| A30 | S+Sh+T | Activity (btop) | Exchange/ps-like: WBCTRLA_GetProgramList; SYS:Tools/Monitor; run: Avail / Status | no | V-NDK |
| A31 | S+C+A | Audio panel | Prefs Sound; AHI Prefs | no | V-WEB |
| A32 | S+C+B | Bluetooth panel | none: no stock stack | none | n/a |
| A33 | S+C+D | Display panel | Prefs ScreenMode / Overscan / Palette / Pointer | no | V-WEB |
| A34 | S+C+N | Network panel | Prefs Internet / Roadshow/Miami config | no | unverified |
| A35 | S+C+P | Power panel | none: no power management; Reboot only | none | n/a |
| A36 | S+C+A+D / S+C+A+E | Calendar / world clock | Prefs Time (clock set); Tools Clock (Tools/Clock is a stock program: unverified for 3.2) | no | unverified |
| A37 | S+C+1..9 | Bar panels | none: no bar | none | n/a |
| A38 | S+W, S+Q | Close window | Window menu Close (WORKBENCH WINDOW x CLOSE for drawers); for own tab MENU_TAB_CLOSE; foreign window: NOT CloseWindow, send break | yes: tab:close (own tab only) | V-WEB(arexx), V-NDK |
| A39 | C+A+Delete | Close all windows | WORKBENCH WINDOW ... CLOSE with every name from WBCTRLA_GetOpenDrawerList; foreign: none safe | no | V-NDK |
| A40 | S+J | Toggle split | none: tiling layout engine | none | n/a |
| A41 | S+P | Pseudo window | none: tiling layout engine | none | n/a |
| A42 | S+T | Floating/tiling toggle | none: tiling layout engine | none | n/a |
| A43 | S+F | Full screen | ChangeWindowBox to screen size; ZoomWindow; WORKBENCH WINDOW x MAX | partial: /screen fullscreen (own window) | V-NDK, V-WEB |
| A44 | S+C+F | Tiled full screen | none: tiling layout engine (ChangeWindowBox gives the same size) | no | n/a |
| A45 | S+A+F | Full width | ChangeWindowBox(left=0,width=screen width) | no | V-NDK |
| A46 | S+O | Pop out (float and pin) | none: no pin; WindowToFront is closest | no | n/a |
| A47 | S+Home | Restore window width | ChangeWindowBox with saved box; ZipWindow / UNZOOMWINDOW (Zoom box restores the alternate size) | no | V-NDK |
| A48 | S+A+Home | Save window width | read Window->LeftEdge/Width under LockIBase; Window zoom data | no | V-NDK |
| A49 | S+L | Cycle layout | none: tiling layout engine | none | n/a |
| A50 | S+A+L | Cycle layout back | none: tiling layout engine | none | n/a |
| A51 | S+Left/Right/Up/Down | Focus direction | ActivateWindow + geometry search over Screen->FirstWindow; WORKBENCH ICON ACTIVATE/CYCLE for icons | no | V-NDK |
| A52 | S+Sh+Arrows | Swap window | MoveWindow x2 using two boxes read under LockIBase | no | V-NDK |
| A53 | S+minus/equal (+Sh,A,C) | Resize L/R/U/D (25/100/300) | SizeWindow(dx,dy) relative; MoveWindow(dx,dy) relative (both relative in the NDK); ARexx SIZEWINDOW/MOVEWINDOW absolute | no | V-NDK, V-WEB |
| A54 | A+Tab, A+Sh+Tab | Next/prev window | ActivateWindow + WindowToFront walking the screen list; WORKBENCH WINDOW x CYCLE (CYCLE/K) | no | V-NDK, V-WEB |
| A55 | S+G, S+A+G | Group toggle, ungroup | none: tiling layout engine | none | n/a |
| A56 | S+A+Arrows | Move window into group | none: tiling layout engine | none | n/a |
| A57 | S+A+Tab, S+C+Left/Right, S+A+wheel | Next/prev in group | none: tiling layout engine (tmux window next in a tab) | none | n/a |
| A58 | S+A+1..5 | Group window 1..5 | none: tiling layout engine | none | n/a |
| A59 | S+Left drag, S+Right drag | Move / resize by mouse | none in 3.2 Intuition for modifier-drag of any window (no Alt-drag); needs an input handler plus MoveWindow/SizeWindow (a commodity with CxFilter + custom, as MagicWB-era tools do) | no | V-NDK partial, unverified |
| A60 | S+M | Minimize to dock | WORKBENCH WINDOW x MIN; WindowToBack; ZipWindow; AddAppIconA for a dock-style icon | no | V-NDK, V-WEB |
| A61 | S+Slash, S+A+Slash | Monitor scaling | none: no fractional scaling (font size via /font-size is the Amiga analogue) | partial: /font-size | n/a |
| A62 | S+Sh+C+Arrows, S+Sh+A+Arrows | Move to monitor | none: one display; screens are the nearest (ScreenToFront) | none | n/a |
| A63 | C+A+Tab | Focus next/prev monitor | ScreenToFront/ScreenToBack/ScreenDepth cycle over screens (Intuition screen list under LockIBase) | no | V-NDK |
| A64 | S+1..0 | Switch workspace 1..10 | ScreenToFront on public screens (LockPubScreen name); own tabs RAmiga 1-9 | partial: tab 1-9 | V-NDK |
| A65 | S+Sh+1..0 | Move window to workspace | none: windows cannot change screens on the fly (needs close/reopen) | none | n/a |
| A66 | S+Sh+A+1..0 | Move window silently | none: same | none | n/a |
| A67 | S+Tab, S+Sh+Tab, S+C+Tab | Next/prev/former workspace | ScreenToFront / ScreenDepth cycle; Amiga-M / Amiga-N screen cycling is built into Intuition | partial: tab:next/prev | V-NDK |
| A68 | S+S, S+grave | Toggle scratchpad | WindowToBack / ScreenToBack toggle of a dedicated window | no | V-NDK |
| A69 | S+A+S, S+Sh+grave | Move to scratchpad | none: no scratchpad concept | none | n/a |
| A70 | S+wheel | Scroll workspaces | none: no wheel on stock Intuition (mousewheel via IECLASS_NEWMOUSE only) | none | n/a |
| A71 | S+C+L | Lock | WORKBENCH LOCKGUI / UNLOCKGUI (blocks drawer windows only, not a real lock) | no | V-WEB(arexx), partial |
| A72 | S+C+I | Toggle idle lock | Blanker commodity (screen blank, no lock) | no | unverified |
| A73 | S+C+Delete, S+C+A+Delete | Laptop display / mirror | none: Linux-only | none | n/a |
| A74 | S+, family | Notifications | none: no notification daemon (Last Message is a title-bar string, ShowTitle area) | none | n/a |
| A75 | S+C+R, S+C+A+R, S+Sh+C+R | Reminders | none stock; timer.device based commodity | none | n/a |
| A76 | S+C+A+T / B / W | Time / battery / weather | Date command (time); battery and weather: none | none | n/a |
| A77 | S+C+S | Share menu | none: Linux-only | none | n/a |
| A78 | S+C+. | Transcode | none: Linux-only | none | n/a |
| A79 | S+C+Z, S+C+A+Z | Zoom in / reset | /font-size bigger/smaller; ZoomWindow | yes: /font-size | n/a |
| A80 | S+C+X | Dictation | none | none | n/a |
| A81 | S+I, S+Sh+I | Input language | Prefs Input / Locale; setkeyboard command (keymap) | no | V-WEB partial |
| A82 | S+A | Select all | /select-all | yes: MENU_SELECT_ALL | n/a |
| A83 | S+C | Copy | /copy | yes | n/a |
| A84 | S+V | Paste | /paste | yes | n/a |
| A85 | S+X | Cut | none in a terminal; clipboard.device write | none | n/a |
| A86 | Print, S+Print, S+C+Print, A+Print, S+A+34/35 | Screenshot, picker, OCR, record, webcam | none: no capture API on 3.2 | none | n/a |
| A87 | XF86 volume/brightness/media/touchpad/lid | Hardware keys | none: no such keys; volume via AHI Prefs / Sound prefs | none | n/a |

## Table B: Omarchy menu rows (section 2 of the 10-21 file)

| # | Omarchy row | Omarchy action | AmigaOS 3.2 mechanism | Already in Amarchy | Verified |
|---|---|---|---|---|---|
| B1 | Root: Apps | Search all apps | app index; OpenWorkbenchObjectA | yes: apps.* + index | V-NDK |
| B2 | Root: Learn | Manuals and viewers | Workbench Help (AmigaGuide, MultiView); HELP key | yes: learn.* | n/a |
| B3 | Learn: Keybindings | Viewer | Amarchy viewer | yes: learn.keys | n/a |
| B4 | Learn: Omarchy / Hyprland / Arch | Web manuals | none: Linux-only; AmigaOS manual guide via MultiView | none | n/a |
| B5 | Learn: Neovim / Bash / Tmux / Herdr | Web manuals | run: man pages in a tab (upkg packages) | no | n/a |
| B6 | Learn: Community | Web | browser launch | no | n/a |
| B7 | Root: Appearance | Style group | Prefs; /theme /font /bg | yes: style.* | n/a |
| B8 | Appearance: Theme | Theme list | /theme | yes: style.theme | n/a |
| B9 | Appearance: Background | Wallpaper | Prefs WBPattern; backdrop (Workbench menu Backdrop) | partial: style.background (term colour) | V-WEB |
| B10 | Appearance: Font | Font | /font; Prefs Font | yes: style.font | n/a |
| B11 | Appearance: Menu Bar | Top bar | ShowTitle | no | V-NDK |
| B12 | Appearance: Windows (gaps, corners, opacity, ratio) | Window look | none: no compositor/tiling | none | n/a |
| B13 | Appearance: Branding | Logo | none: Linux-only | none | n/a |
| B14 | Appearance: Dock | Dock | AddAppIconA + Workbench dock tools (AmiDock); none stock | none | unverified |
| B15 | Appearance: Workspace Layout | Layout | none: tiling layout engine | none | n/a |
| B16 | Appearance: Animations | Animations | none | none | n/a |
| B17 | Appearance: Unlock | Unlock theme | none | none | n/a |
| B18 | Root: Trigger | Action group | Amarchy groups | n/a | n/a |
| B19 | Trigger: Toggle (stay awake, notifications, nightlight, screensaver, crash capture) | Toggles | Blanker commodity toggle (ActivateCxObj via Exchange/CX_APPEAR); others none | no | unverified |
| B20 | Trigger: Emoji | Emoji | dist/emoji | no | unverified |
| B21 | Trigger: Clipboard History | Clipboard | clipboard.device; none stock history | no | unverified |
| B22 | Trigger: Reminder | Reminders | none | none | n/a |
| B23 | Trigger: Capture | Screenshot etc. | none | none | n/a |
| B24 | Trigger: Transcode | Transcode | none | none | n/a |
| B25 | Trigger: Share | Share | none | none | n/a |
| B26 | Trigger: Hardware | Hardware panel | Prefs programs | no | V-WEB |
| B27 | Trigger: Speed Test | Network speed | none stock | none | n/a |
| B28 | Root: Setup | Settings group | Prefs drawer (SYS:Prefs) via OpenWorkbenchObjectA | partial: window.preferences (UP-Term Prefs only) | V-NDK |
| B29 | Setup: Monitors | Monitor layout | Prefs ScreenMode, Overscan | no | V-WEB |
| B30 | Setup: Keybindings | Edit bindings | edit dist/keymap | no | n/a |
| B31 | Setup: Input | Mouse/keyboard | Prefs Input | no | V-WEB |
| B32 | Setup: Typing | Repeat rate | Prefs Input | no | V-WEB |
| B33 | Setup: Language | Locale | Prefs Locale | no | V-WEB |
| B34 | Setup: Network (DNS) | Network | Roadshow/Miami Prefs (Internet) | no | unverified |
| B35 | Setup: Default (agent, browser, dictation, terminal, editor) | Defaults | Prefs Workbench default tool; Default Tool in icon Information | no | partial |
| B36 | Setup: Plugin | Plugins | upkg | partial: install.* | n/a |
| B37 | Setup: Security | Security | none | none | n/a |
| B38 | Setup: Config | Config files | run: editor on S:/ENVARC: | no | n/a |
| B39 | Setup: Direct Boot | Boot | Prefs Workbench/startup-sequence; Early Startup Menu | no | n/a |
| B40 | Setup: Reset Computer | Reset | Reboot | yes: system.reboot | n/a |
| B41 | Root: Install (Package, AI, Service, Development, Editor, Style, Gaming, Browser, Webapp, Terminal, TUI, Windows, AUR, Preinstalls) | Install | upkg install, upkg-browse | yes: install.browse | n/a |
| B42 | Root: Remove (same + Dictation, Theme, Security) | Remove | upkg remove | partial: no remove row | n/a |
| B43 | Root: Update | Update group | upkg update/upgrade | yes: install.upgrade/update | n/a |
| B44 | Update: Omarchy / Channel / Kernel / Config / Themes / Process / Hardware / Firmware | Updates | upkg upgrade; Kickstart/firmware: none | partial | n/a |
| B45 | Update: Password | Passwd | none | none | n/a |
| B46 | Update: Timezone / Time | Time | Prefs Time, Locale; SetDST | no | V-WEB partial |
| B47 | Root: About | About | Workbench menu About; /about | partial: /about | V-WEB |
| B48 | Root: System | System group | Amarchy group | yes: system.* | n/a |
| B49 | System: Screensaver | Screensaver | Blanker commodity | no | unverified |
| B50 | System: Lock | Lock | LOCKGUI (partial) | no | V-WEB |
| B51 | System: Suspend | Suspend | none | none | n/a |
| B52 | System: Hibernate | Hibernate | none | none | n/a |
| B53 | System: Logout | Logout | Workbench menu Quit (closes Workbench, only when no program is running) | no | V-WEB(menus) |
| B54 | System: Reboot | Reboot | Reboot / ColdReboot | yes: system.reboot | n/a |
| B55 | System: Shutdown | Shutdown | none: Amiga has no power-off | none | n/a |

## Table C: AmigaOS 3.2 features with no Omarchy counterpart

Menu items are from the older manual (V-WEB menus) and may differ in 3.2; the ARexx MENU command (V-WEB arexx) can invoke any of them by name, so each is a possible Amarchy row without keystroke injection.

| Feature | Mechanism | Suggested Amarchy group |
|---|---|---|
| Workbench > Backdrop | WORKBENCH MENU WINDOW ROOT INVOKE WB/BACKDROP (name form unverified) | Workbench |
| Workbench > Execute Command | MENU INVOKE; or Amarchy's own run: | Workbench |
| Workbench > Open volume (3.2, unverified) | MENU INVOKE | Workbench |
| Workbench > Redraw All | MENU INVOKE | Workbench |
| Workbench > Update All | MENU INVOKE; UpdateWorkbench() | Workbench |
| Workbench > Last Message | MENU INVOKE | Workbench |
| Workbench > About | MENU INVOKE | Workbench |
| Workbench > Quit | MENU INVOKE (also System: Logout) | System |
| Window > New Drawer | NEWDRAWER NAME | Window / Files |
| Window > Open Parent | MENU INVOKE (WINDOW ACTIVE) | Window |
| Window > Close | WINDOW ... CLOSE; CloseWorkbenchObjectA | Window |
| Window > Update | MENU INVOKE | Window |
| Window > Select Contents | MENU INVOKE; ChangeWorkbenchSelectionA | Window |
| Window > Clean Up | MENU INVOKE | Window |
| Window > Snapshot (Window / All) | WINDOW ... SNAPSHOT; MENU INVOKE | Window |
| Window > Show (Only Icons / All Files) | MENU INVOKE | Window |
| Window > View By (Icons / Name / Date / Size) | MENU INVOKE; VIEW | Window |
| Window > Sort Order (3.2, unverified) | MENU INVOKE | Window |
| Icons > Open, Copy, Rename, Information, Snapshot, UnSnapshot, Leave Out, Put Away, Delete, Format Disk, Empty Trash | MENU INVOKE on the selection (ICON ... SELECT/OPEN, WBCTRLA_GetSelectedIconList); RENAME, DELETE, INFO commands | Files / Icons |
| Tools > ResetWB | MENU INVOKE; same as WB restart | Workbench |
| Tools > user items | MENU ADD lines in SYS:WBStartup/MenuTools (3.2 mechanism per search; unverified) | Workbench |
| Window ops on any window | WINDOW (OPEN CLOSE SNAPSHOT ACTIVATE MIN MAX FRONT BACK CYCLE), MOVEWINDOW, SIZEWINDOW, CHANGEWINDOW, ZOOMWINDOW, UNZOOMWINDOW, WINDOWTOFRONT, WINDOWTOBACK, ACTIVATEWINDOW | Window |
| Icon keyboard navigation | ICON (UP DOWN LEFT RIGHT ACTIVATE CYCLE MOVE) | Window |
| Scroll a window | VIEW PAGE/PIXEL UP DOWN LEFT RIGHT | Window |
| Run ARexx | RX CMD (CONSOLE, ASYNC) | Workbench |
| Info and attributes | INFO NAME, GETATTR OBJECT (STEM/VAR), FAULT CODE | Workbench |
| Lock out Workbench | LOCKGUI / UNLOCKGUI | System |
| Hot keys | KEYBOARD ADD/REMOVE (Workbench-level key to command) | Keys |
| Screen cycling | ScreenToFront, ScreenToBack, ScreenDepth, built-in Amiga-M/Amiga-N | Screens |
| Public screens | LockPubScreen, PubScreenStatus, SetPubScreenModes; Prefs Overscan | Screens |
| Drive/volume actions | Format Disk, WBCTRLA_Add/RemoveHiddenDeviceName, WBInfo | Disks |
| Exchange commodity | SYS:Tools/Commodities/Exchange; CxBroker enumerates; ActivateCxObj on/off | Commodities |
| ClickToFront, AutoPoint (focus follows mouse), Blanker, FKey, IHelp, NoCapsLock | SYS:Tools/Commodities programs (list unverified for 3.2; check the disk) | Commodities |
| Prefs programs | SYS:Prefs drawer: Font, IControl, Input, Locale, Overscan, Palette, Pointer, Printer, ScreenMode, Serial, Sound, Time, WBPattern, Workbench (list from the 3.x convention, unverified for 3.2r4 disk) | Prefs |
| AppIcon / AppMenuItem | AddAppIconA, AddAppMenuItemA: Amarchy can add itself to the Tools menu and an icon | Self |
| Trashcan | Icons > Empty Trash | Files |

## Counts (from the table rows above)

Computed from Table A column 4 and Table B column 4: rows whose mechanism starts with "none" count as without; the rest have a mechanism.

- Table A (bindings, rows A1-A87; one row can stand for several keys): with mechanism 46, none 41.
- Table B (menu rows): with mechanism 39, none 16.
- Of the A rows, those that are "none: tiling layout engine" or "none: Linux-only" are the true gaps; the 10-21 claim (about 40 of 165 have equivalents) counted individual keys, these rows group keys, so compare ratios: 46/87 = 52 percent with a mechanism versus the old 40/165 = 24 percent.
- Caveat: a mechanism is not the same as implemented; most 'with mechanism' rows still need Amarchy code, and the window rows need runtime tests on a rig (nothing here was run).

## Decisions for the owner

- Add a Window group: next/previous window, to back, zip/zoom, full width, save/restore box, close-for-drawers (Intuition + WORKBENCH WINDOW).
- Add a Workbench group: Backdrop, Execute Command, Redraw All, Update All, Last Message, Open volume, ResetWB, About via ARexx MENU INVOKE.
- Add a Screens group: cycle screens, Workbench to front, public screens (the real Amiga workspaces).
- Add a Files group: New Drawer, Open Parent, Clean Up, View By, Empty Trash, Format Disk.
- Add a Commodities group: Exchange, Blanker, ClickToFront, AutoPoint, FKey toggles.
- Extend Prefs from UP-Term Prefs to the SYS:Prefs drawer (Sound, ScreenMode, Input, Locale, Time, WBPattern...).
- Decide whether foreign windows are in scope (needs LockIBase code; CloseWindow stays out) or only Workbench drawer windows via ARexx.
- Decide whether Amarchy registers an AppMenuItem/AppIcon so it also appears in the Workbench Tools menu.
- Leave tiling rows (split, group, layouts, gaps, pseudo window) out; state it in the viewer.

