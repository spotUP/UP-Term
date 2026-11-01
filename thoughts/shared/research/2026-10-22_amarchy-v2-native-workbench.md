---
date: 2026-10-22
topic: Amarchy v2, a commodity that uses the native AmigaOS UI and knows Workbench
status: research
---

# Amarchy v2: native Workbench

Method: Omarchy facts from memory (UNVERIFIED, marked U). AmigaOS API names checked by grep in the NDK 3.2R4 headers
at `vendor/ndk-3.2r4-Include_H` (clib/*_protos.h, workbench/workbench.h, prefs/*.h, libraries/commodities.h,
reaction/, gadgets/, classes/); marked V when found there. Behaviour (what a call does, which OS version) is from
memory: U. No rig run, nothing built.

Finding about Phase 1: `amarchy/` does not exist in this tree and no `*.c` mentions Amarchy
(`git ls-files | grep -i amarchy` = 4 thoughts files). The plan's commodity code is not in the repo, only the plan,
research and handoff. The handoff mentions `keymap.c` (heap fix 859aeea4) but I did not find it by `find ..
-maxdepth 4 -name keymap.c`. Check that Phase 1 code was committed before planning v2 on top.

Important environment fact (RULES.md:80): Directory Opus replaces Workbench on the rigs and the A1200, so
`OpenWorkbenchObjectA` and `C:WBRun` fail there. Every "workbench.library" row below is therefore
Workbench-only; on the target machines it needs a Directory Opus (dopus5.library / ARexx `DOPUS.1`) path or the
repo's own `wbrun` tool (RULES.md:80). Rigs boot 3.2 (`rig.py start --os 32`, RULES.md:43); the A1200 OS version is not
recorded in RULES.md (grep for 3.1/3.9 found nothing): ask owner.

## 1. Omarchy feature inventory and AmigaOS mapping

Legend: Feas = works (W), needs patch/SetFunction/extra tool (P), impossible or not worth it (X). OS = minimum.

| # | Omarchy feature (U) | AmigaOS mechanism | Feas | OS |
|---|---|---|---|---|
| 1 | Super+Space launcher menu (Walker/ Elephant) | Commodity hotkey (`CxBroker`, `CxFilter`, `CxSender` V libraries/commodities.h:28-29, `CxBroker` V commodities_protos.h:37) opens own window | W | 2.0 |
| 2 | Launch apps / Super+Return terminal / Super+B browser | `SystemTagList` with `NP_` / `SYS_Asynch` (dos, U); icon-style launch via `wbrun` tool (in repo) or `OpenWorkbenchObjectA` V wb_protos.h:57 (Workbench only) | W (WB: P under DOpus) | 2.0 / 3.2 for OpenWorkbenchObject |
| 3 | Keybinding viewer (Super+K) | Own window listing the keymap file; phase 1 already does text | W | 3.0 |
| 4 | Window tiling / move / resize / float (Hyprland dwindle) | `ChangeWindowBox`, `MoveWindow`, `SizeWindow`, `ZipWindow` V intuition_protos.h; enumerate via `LockIBase` + `Screen->FirstWindow` (U). Foreign windows with no WFLG_SIZEGADGET still obey ChangeWindowBox but redraw is the app's job; refresh via IDCMP_NEWSIZE (U). Tiler = layout algorithm over a screen's window list | P (works for well-behaved windows; no per-app rules, flicker, apps ignoring size) | 2.0 (ChangeWindowBox 3.0 V) |
| 5 | Workspaces 1-10, Super+n, move window to workspace | No virtual desktops. Closest: one public screen per workspace (`OpenScreenTags` + `PubScreenName`, `SetPubScreenModes` V, `ScreenToFront` V, `PubScreenStatus` U), but windows cannot move between screens (needs close/reopen with `WA_PubScreen`, app cooperation). Or iconify via commodity-free tools (Exchange "hide windows" not generic) | X for generic move; W for "switch between existing screens" | 2.0 |
| 6 | Focus follows mouse / focus direction (Super+arrows) | `ActivateWindow` (U) on the neighbour found geometrically; focus-follows-mouse is a classic commodity (ClickToFront, Sloppy-Mouse style, `InputHandler` on input.device, U) | W | 2.0 |
| 7 | Close window (Super+W) | Send `IDCMP_CLOSEWINDOW` is not possible from outside; options: simulate click on close gadget via `input.device` IND_ADDEVENT (U) or `WindowToBack`. No generic close | P (fake click on close gadget at its known position; fails without close gadget) | 2.0 |
| 8 | Fullscreen / maximise (Super+F) | `ZipWindow` or `ChangeWindowBox` to screen size, restore stored box | W | 2.0 |
| 9 | Themes applied system-wide (Super+Ctrl+Shift+Space; Tokyo Night, Catppuccin, ...) | See 9a-9e | mixed | |
| 9a | ...colour palette of Workbench screen | `ENV:Sys/palette.prefs` (IFF PREF/PRHD + `PALT`, `struct PalettePrefs` V prefs/palette.h:31); live apply: `LoadRGB32` / `ObtainBestPenA` V graphics, `SetRGB32` (U) on the WB `ViewPort`. IPrefs re-reads on the notification of the prefs file (U); easiest: write file then run `C:IPrefs` / start `SYS:Prefs/Palette` USE (U) | W (live LoadRGB32 works at once; file for persistence) | 2.0 |
| 9b | ...backdrop pattern / wallpaper | `ENV:Sys/wbpattern.prefs` (`ID_PTRN` V prefs/wbpattern.h:24, `struct WBPatternPrefs` V), picture via datatypes backdrop (3.2 Wbpattern prefs, U); IPrefs applies | W (3.0 patterns; picture backdrops 3.2/3.9) | 3.0 |
| 9c | ...screen mode / depth | `ENV:Sys/screenmode.prefs` (`ID_SCRM` V prefs/screenmode.h:27); a mode change takes effect only after WB reopen: `CloseWorkBench`/`OpenWorkBench` V intuition_protos.h (fails with windows open) | P (restart-ish) | 2.0 |
| 9d | ...fonts | `ENV:Sys/font.prefs` (`ID_FONT` V prefs/font.h:31); applies to new windows only | W (new windows) | 2.0 |
| 9e | ...pen/colour of UI (Hyprland border colours, waybar) | Palette pens are fixed slots (DetailPen, BlockPen, ...). Intuition 3.x pens come from the 4-colour WB palette (or 8 with MWB); 3.2 adds more pens in `ENV:Sys/Workbench.prefs`? U. `ReactionPrefs` V prefs/reaction.h exists (3.2). No per-window border colours | P (only the screen palette moves; theme looks coarse in 4 to 8 colours) | 3.2 for new pens |
| 9f | ...terminal colours | UP-Term profile files: `themes/*.conf` (112 themes, themes/README.md) | W, already done | |
| 10 | Audio menu (volume, device) | `AHI` prefs (`ENV:AHI` / `Prefs/AHI`, U) and Paula via `audio.device`; volume of Paula channels no system master. AHI needs 3rd party | P (AHI only) | 2.0 + AHI |
| 11 | Network menu (Wi-Fi, bluetooth) | Roadshow/Miami: `ENVARC:Sys/Roadshow`? unverified; `C:AddNetInterface`, `C:ConfigureNetInterface`, `Prefs/Internet` (U); Wi-Fi through the card's driver | P (Roadshow only; no common UI) | 3.1 + Roadshow |
| 12 | Display menu (monitors, scale) | `Prefs/ScreenMode`, `Prefs/Overscan` (`prefs/overscan.h` V), `Prefs/Palette`; `SetPubScreenModes` V | P | 2.0 |
| 13 | Power menu (lock, suspend, restart, shutdown) | Reboot: `ColdReboot()` (exec 36, U); shutdown on real Amigas not available (no soft power) | W reboot; X shutdown, suspend, lock | 2.0 |
| 14 | Screenshots (Super+Print, region, to clipboard) | No OS tool. Grab `Screen->RastPort` bitmap with `ReadPixelArray8` / `GetBitMapAttr` (graphics, U) and write ILBM with iffparse (`libraries/iffparse.h` present V) or the picture.datatype `DTM_WRITE` (datatypes dir present V, class usage U). Clipboard: `clipboard.device` with an FTXT/ILBM IFF (U) | W (own ILBM writer or datatype writer) | 3.0 |
| 15 | Screen recording | Not feasible at useful rates | X | |
| 16 | Notifications (mako) | No system notification service. Own small screen-top window / Intuition `EasyRequest` is modal; `AppIcon` flash V `AddAppIconA` wb_protos.h (Workbench only) | P | 2.0 |
| 17 | Status bar (waybar: clock, battery, cpu) | Own borderless window on the public screen's title bar area is not possible; a backdrop window at screen bottom with `WA_Borderless` and `WA_BackFill` (U). Memory `AvailMem`, clock via `timer.device`; no battery | P (works as an app window; steals space) | 2.0 |
| 18 | Clipboard manager (Super+Ctrl+V) | `clipboard.device` unit 0 history watcher (U), CLIPS: | W | 2.0 |
| 19 | Emoji / symbol picker | No emoji font in Amiga text; Omit | X (planned to drop anyway, D5 decided to ship all, so list only) | |
| 20 | Dotfile edits, "Setup" menu (Super+Alt+Space) | Edit `S:User-Startup`, `S:Startup-Sequence`, `ENVARC:Sys/*.prefs`, WBStartup drawer (a directory scan, `Examine`/`ExNext`, plain dos) | W (file edits with backup; marker block in User-Startup so we never touch user lines) | 2.0 |
| 21 | Install apps (pacman/yay Install menu, Super+Alt+Space > Install) | `upkg` (this repo's package tool, `upkg-ui`), the existing back end; Aminet browse | W | needs upkg |
| 22 | Update / system update | `upkg update` | W | |
| 23 | Remove / manage packages | `upkg` | W | |
| 24 | Trigger a Workbench object as the user would | `OpenWorkbenchObjectA` V; `WorkbenchControlA` V (V45 tags: `WBCTRLA_IsOpen` V workbench.h:288, `WBCTRLA_GetProgramList` V:306, `WBCTRLA_GetSelectedIconList` V:312, `WBCTRLA_GetOpenDrawerList` V:318, `WBCTRLA_AddHiddenDeviceName` V:332); `AddAppMenuItemA`/`AddAppWindowA`/`AddAppIconA` (V wb_protos.h) for adding our entries to the WB Tools menu or an icon | W on stock Workbench 3.2 (P under DOpus; Dopus has its own ARexx and listers) | 3.2 for WorkbenchControl and OpenWorkbenchObject (V45) |
| 25 | List what is running / app switcher (Super+Tab) | `LockIBase` + screens and windows list (U); `Forbid` + `SysBase->TaskReady/TaskWait` task list for "processes" (U); `CliProcessList` / `rexxsyslib` | W | 2.0 |
| 26 | Assigns as "mounts" | `AssignPath`/`AssignLate`, `Assign` list via `LockDosList(LDF_ASSIGNS|LDF_READ)` (U, dos) | W (read), P (edit) | 2.0 |
| 27 | Hyprland config reload without restart | Already designed: `StartNotify` on keymap file | W | 2.0 |
| 28 | Idle lock / screensaver (hypridle) | Blanker commodity pattern (input.device handler counting events, then `BlankScreen`: no OS call; custom screen or `LoadRGB4` all black, U) | P | 2.0 |
| 29 | Brightness / night light | No backlight. Palette tint via `LoadRGB32` | P (CRT; palette only) | |
| 30 | ARexx scripting of everything (hyprctl) | Amarchy ARexx port `AMARCHY` (planned). Third party ports: Workbench port in 3.2 (`WORKBENCH`, commands unverified U, `rexx/` headers do not list it V), DOpus port `DOPUS.1` | W (own port); WB port P | 2.0 / 3.2 |

Verified in NDK (names exist, V): LockPubScreenList, LockPubScreen, SetPubScreenModes, ScreenToFront, ScreenToBack, ChangeWindowBox,
ZipWindow, CloseWorkBench, OpenWorkBench, WorkbenchControlA (wb_protos.h), OpenWorkbenchObjectA, CloseWorkbenchObject, UpdateWorkbench,
AddAppIconA, LoadRGB4, ObtainBestPenA, WBCTRLA tags above, WBAPPICONA/WBAPPMENUA tags, WBOPENA_ArgLock/ArgName/Show/ViewBy,
prefs ids ID_PRHD/ID_PALT/ID_PTRN/ID_SCRM/ID_FONT/ID_WBNC/ID_WBHD/ID_WBTF, struct WBPatternPrefs, commodities.h macros.
NOT found (do not exist or other spelling): `ChangeWorkBench`, `SetWindowPens`. Not verified (no header): ARexx WORKBENCH port
command set, IPrefs notification behaviour, `ColdReboot`, picture.datatype write, ClipboardUnit formats.

## 2. Native UI options

Available on target: rigs are 3.2 on a 68060; the owner's A1200 version is unconfirmed (ask). Headers present in
NDK 3.2R4 `reaction/`, `gadgets/`, `classes/`, `libraries/gadtools.h` (V). ReAction is part of 3.2 (ClassAct became
AmigaOS 3.2's own); on a pure 3.1 it is absent unless ClassAct is installed (U). MUI is not in the OS (third party,
needs MUI install and its own prefs: U).

| Toolkit | On 3.1 | On 3.2 | Size cost | vbcc build | Look |
|---|---|---|---|---|---|
| GadTools | yes (ROM 2.0+, V libraries/gadtools.h) | yes | tiny (no extra libs) | already used: prefs/upprefs.c:52,60,73 builds with this repo's vbcc | classic 3D bevel, follows system pens and font; no layout engine (manual boxes) |
| ReAction (BOOPSI classes: window.class, layout.gadget, listbrowser, chooser, V gadgets/*.h) | no (ClassAct needed) | yes, native | classes are ROM/disk libs, small client code via `reaction_macros.h` (V) | macros need `__inline`/`proto` stubs; vbcc ok in principle (U), not done here yet | themed per Workbench prefs (ReactionPrefs V), layout engine, listbrowser for menu, speedbar, getscreenmode, getfont, getcolor (palette prefs UI for free) |
| Pure BOOPSI / Intuition | yes | yes | small, but you write the classes | fine | whatever you draw |
| MUI | needs install | needs install | MUI is ~1 MB | needs MUI includes (not in NDK, not in tree V) | best, but external dependency |

Recommendation: GadTools for v2.0 (menu list, viewer, small panels), ReAction as an optional later layer only if the
owner commits to 3.2 as the floor. Decisive trade-off: GadTools runs on every 3.x and is already proven in this repo
(upprefs.c) but has no layout engine and a plain look; ReAction gives layout, list browser and pickers (screenmode,
colour, font) for less code but locks to 3.2. If the A1200 is on 3.2, ReAction wins on code size for v2.2 panels;
if on 3.1, GadTools is the only answer. Do not choose MUI.

Note on phase 1 reuse: the quick menu and viewer were decided as TEXT via libupkgui (curses in an UP-Term window,
`upterm-ports/pkgs/upkg-ui/src/ui.c`, `chrome.c`). Native means a second renderer; keep `keymap` parsing and the
action list shared so both can coexist (text menu inside UP-Term, native menu elsewhere).

## 3. Existing code to reuse

- GadTools window + gadget table driven by data: `prefs/upprefs.c:29` (header comment), `:52` and `:60` includes, `:73` `GadToolsBase`,
  `:98` `VisualInfo`, `:862` one-gadget helper, `:1851` `LockPubScreen(0)`; the gadget table `prefs/prefs_ui.c:189-190`
  (swatch + hex string kinds PGK_SWATCH / PGK_STRING). A colour swatch/hex editor exists and can become the theme colour editor.
- Preference core (key-value store, parse and save): `prefs/prefs_core.c`, `prefs/prefs_core.h`, `prefs/prefs_dos.c`, `prefs/prefs_asl.c` (ASL requester glue).
- Screens and public screens: `handler/vtcon_handler.c:2574` `LockPubScreen("Workbench")`, `:2645` `OpenScreenTagList` (own screen),
  `:2703-2707` locking the named/own/default public screen; config key for the target screen is in `dist/up-term.conf`. This is the
  one place that already chooses a screen; reuse the same function for "which screen do windows go to".
- Themes: `themes/*.conf` and `.yaml` (112, themes/README.md:1-25) store `fg`, `bg`, `cursor-color`, `palette = idx,RRGGBB,...` in
  vtcon profile format. A theme-to-Workbench step needs a mapping: choose 4 (or 8) of the 16 ANSI colours for `PalettePrefs`. Add
  a small shared function `theme_to_wbpalette()`; parse theme with the existing upconf parser (handler/ or prefs/; the plan's D3
  `upconf_rows`).
- Launching icons: `tools/` wbrun (RULES.md:80, `make build/amiga/wbrun`): reuse for "launch app" on DOpus machines instead of `OpenWorkbenchObjectA`.
- Package back end: upkg and `upterm-ports/pkgs/upkg-ui/src/upkg-ui.c:166` (`main`), `ui.c`, `chrome.c`, `test_chrome.c` for install/update/remove actions of rows 21-23.
- Keymap file and parser from the plan: `thoughts/shared/plans/2026-10-22-amarchy-commodity.md` section 1 and 2 (broker, filter, notify);
  the code itself I could not locate (see top).
- Rigs and drive tools: `tools/rig/ami.py`, `tools/rig/wbrun_rig.py`; the `mcp__amiga__*` tools for ui list and screenshots on live tests.
- None found for screenshots (grep `ILBM`, `datatypes`, `screenshot` in handler/view found only a hit in vtcon_handler.c line unrelated or absent: unverified) and none for ARexx ports or input handlers.

## 4. Scope tiers

v2.0 native menu, viewer, theme to Workbench (smallest slice that feels like Omarchy):
- GadTools window on the current public screen: menu list (ListView) fed from the keymap file, filter string, Enter runs the action; viewer as a read-only ListView of the same rows.
- Theme picker: list the 112 themes, apply to (a) UP-Term profile (exists), (b) Workbench palette live with `LoadRGB32` plus `ENV:Sys/palette.prefs` (+ `ENVARC:`) written as IFF, (c) optional WBPattern solid colour.
- Launch actions: `SystemTagList` and `wbrun`.
- Risks: palette mapping looks bad on 4 colours (needs a per-theme choice and a revert, a bad palette makes the UI unreadable: always keep a "restore" hotkey and a 15 s auto-revert); IPrefs notification behaviour unverified; DOpus replaces Workbench (no WB APIs).

v2.1 window management:
- Tiling on one screen (`LockIBase`, `ChangeWindowBox`), float/fullscreen toggle, focus by direction (`ActivateWindow`), workspace switch between screens (`ScreenToFront`), screenshot (ILBM writer, `S:`/`SYS:Screenshots`).
- Risks: foreign windows redraw slowly and some ignore resize (no refresh messages); `LockIBase` held too long freezes the system (hold microseconds only); closing a window has no safe generic call; workspaces cannot move windows; 68000/020 speed. Needs a per-app float list.

v2.2 system panels:
- Setup menu for User-Startup/WBStartup edits with marker blocks and backups, Assign list/edit, audio (AHI), network (Roadshow), display (ScreenMode via `CloseWorkBench` cycle), power (`ColdReboot`), clipboard history, screensaver blanker, notifications.
- Risks: each panel depends on third-party software; editing Startup-Sequence can brick boot (never edit it, only User-Startup marker block); `CloseWorkBench` refuses with open windows.

## 5. Owner decisions needed

1. OS floor. Recommended: AmigaOS 3.2 (rigs run it; unlocks ReAction and WorkbenchControl). Alternative: 3.1 compatible. Trade-off: 3.1 drops ReAction/WB API and keeps A1200 on 3.1 working.
2. Toolkit. Recommended: GadTools for v2.0 and v2.1, ReAction only in v2.2 panels if 3.2 is the floor. Alternative: ReAction from the start. Trade-off: GadTools is proven here but plain.
3. Directory Opus is the Workbench on the rigs and A1200 (RULES.md:80). Recommended: treat Workbench APIs as optional, use `wbrun` and own mechanisms, add a `DOPUS.1` ARexx adapter later. Alternative: require Workbench. Trade-off: the second cuts DOpus users.
4. Theme to Workbench palette. Recommended: apply live (`LoadRGB32`) plus write `palette.prefs`, with 15 s auto-revert. Alternative: UP-Term windows only. Trade-off: system-wide look vs risk of unreadable screen on 4-colour WB.
5. Keep the text (libupkgui) menu and viewer. Recommended: keep it as the in-UP-Term renderer, native as default elsewhere (shared action table). Alternative: replace text with native everywhere. Trade-off: two renderers to test vs losing the terminal-native menu.
6. Window management scope. Recommended: start with float, fullscreen, focus direction, screen switch (v2.1 core); defer tiling. Alternative: full tiling immediately. Trade-off: tiling on foreign windows is fragile and slow on 68020.
7. Workspaces. Recommended: one public screen per workspace only for UP-Term-owned windows. Alternative: drop workspaces. Trade-off: generic move of foreign windows between screens is impossible.
8. Screenshot format. Recommended: ILBM via iffparse own writer (no datatype dependency). Alternative: picture.datatype writer (PNG). Trade-off: ILBM opens everywhere on Amiga; PNG is host friendly but needs datatypes installed.
9. Setup menu edits (User-Startup). Recommended: marker block only, with automatic backup, never Startup-Sequence. Alternative: no file edits, only run tools. Trade-off: convenience vs boot risk.
10. (Resolved) Phase 1 code lives on branch amarchy-a (worktree .claude/worktrees/agent-a7edb2573dbabf68d, directory amarchy/), not yet merged to main; the research agent searched the main checkout only.
11. A1200 OS version (not recorded). Recommended: record in RULES.md. Needed for decisions 1 and 2.

## 6. Owner answers (2026-10-22, not re-litigated)

- D1 OS floor: AmigaOS 3.2.
- D2 Toolkit: ReAction from the start (owner overrode the GadTools-first recommendation).
- D3 Desktop: support BOTH Workbench and Directory Opus. Detect which desktop runs; Workbench through workbench.library (3.2 WorkbenchControl etc.), Directory Opus through its ARexx port (DOPUS.1). Neither is optional; both are first-class targets.
- D4 Theme: apply live (LoadRGB32) plus write palette.prefs, with a 15 s auto-revert.
- D10 resolved: Phase 1 code is on branch amarchy-a, directory amarchy/.
- Still open: D5 (keep text menu as in-terminal renderer), D6 window-management scope, D7 workspaces, D8 screenshot format, D9 User-Startup edits, D11 record the A1200 OS version in RULES.md.
