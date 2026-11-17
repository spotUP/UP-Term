# README screenshots (s74) -- record

Rig 85, `rig.py start --max --os 32 --purpose "up-term-ba s74: README screenshots"` (068060 max, AmigaOS 3.2, os-322 tree),
then `rig.py rtg` (Workbench on the graphics card: 800x600, depth 24; the AGA default was 640x256 depth 8, non-square pixels),
then `amarchy_gui_rig.py --setup --no-build` (build/amiga Amarchy, amarchy-menu, amarchy-gui of 12 Nov 00:08, amarchy-flat of 11 Nov 23:11;
STATUS flat=on). The OS 3.2 rig booted plain Workbench 3.2 (no DOPUS.1 port), UP-Term: = VTCX:Apps (only Amarchy*, AmTile, icat, vsh).

| file | size | how |
|---|---|---|
| docs/screenshots/amarchy-desktop.png | 800x600, 8977 B | Esc closed "Welcome to Amarchy"; `rx "ADDRESS WORKBENCH; 'WINDOW SYS: OPEN'; 'CHANGEWINDOW SYS: 300 110 470 330'"`; `ami.py shot`. Haiku check: bar, desktop icons, drawer Empty-12MB-FFS with icons, no requesters, no pointer visible. |
| docs/screenshots/prefs-flat.png | 800x600, 7455 B | drawer closed (`WINDOW SYS: CLOSE`), `Run >NIL: SYS:Prefs/Time`; uitree: "Time Preferences" 332x199 at 0,11, active, Amarchy bar. Text-checked only (no image check). Window sits top-left, not centred. |

Missing: terminal-unix.png, terminal-kitty-picture.png, upkg-browser.png, dopus-lister.png.
Cause: the OS 3.2 rig has no installed UP-Term kit (no vim, tmux, upkg in UP-Term:bin) and no Directory Opus running.
`upkg_rig.ensure_base` (the kit base install upkg_browse_rig uses) fails on 3.2: its DEST is Rushhours:, which the 3.2 rig lacks
("[ERROR] free-space precheck before the base install: no row for Rushhours in the Info output").
Way forward: the default 3.1 kit rig (`rig.py start --max`, kit at Rushhours:Tools/UP-Term, DOpus 5.104 as Workbench) for shots 2,3,4,6;
vim needs the up-vim ipk from build/ipk installed through upkg (upkg_browse_rig.py does this with a served feed).

Note: docs/screenshots/ already held upkg-browse-install.png, upkg-browse-list.png, upkg-browse-remove.png (not made by this run, untouched).

Cleanup: rig 85 stopped, build/rig85 removed by rig.py; build/rig-saved/85 (12 MB, files from the failed base install) deleted;
three scratch probe PNGs (7-9 KB each) deleted.

## Part 2 (rig 86, OS 3.1 kit rig, 12 Nov)

`UPTERM_RIG=86 rig.py start --max --purpose "up-term-ba s74: README screenshots part 2"`, `rig.py rtg` (800x600 depth 24, kept across later starts),
`rig.py stop` + `rig.py dopus` (5.104 installed into the rig's VTC: drawer) + start again: DOpus5:DirectoryOpus as Workbench, Amarchy + amarchy-flat
(`STATUS`: flat=on, dopus flat=1) running from the kit. Shots with `ami.py shot`; scripts in the session scratchpad (shots12.py, shot3.py).
Each PNG checked by one haiku agent (text answer only).

| file | size | how | check |
|---|---|---|---|
| docs/screenshots/terminal-unix.png | 800x600, 23687 B | `vshwin.open_vsh('UP-Term vsh', box 40/40/720/500)`; typed `cd Rushhours:Tools/UP-Term`, `ls -l --color=always`, `ls --color=always bin` | OK: multi-column listing, prompt, no requester, no pointer. Colour is mostly one light text colour; host pixel count shows only ~1300 px of cyan/blue accents (a vim or tmux shot would show more colour; not installed on the kit) |
| docs/screenshots/terminal-kitty-picture.png | 800x600, 230854 B | demo/media/fjord.jpg resized on the host to 360x225 PNG (`sips -Z 360`), put to RAM:fjord.png, `icat RAM:fjord.png` in a vsh window | OK: clean fjord photo under the command line, no stripes, no requester, no pointer |
| docs/screenshots/upkg-browser.png | 800x600, 29710 B | upkg_browse_rig.setup() with `upkg_rig.ensure_base` replaced by a no-op (served build/ipk feed, its conf as default), `upkg update` rc 0, `upkg browse` in a vsh window, then `q`, conf restored | OK: framed list up-bitchx ... up-picocom, footer "Up/Down: Move  Enter: Accept  Escape: Cancel", no requester, no pointer. Descriptions clipped at the right edge |
| docs/screenshots/dopus-lister.png | 800x600, 14712 B | `rx "OPTIONS RESULTS; ADDRESS DOPUS.1; 'lister new 120/70/560/440 Rushhours:Tools/UP-Term'"` (the geometry must be quoted, else ARexx error 41) | OK: lister with Amarchy, bin, emoji ... flat (no bevels), no requester, no pointer |

Gotcha: the first upkg attempt ran the unpatched `upkg_rig.ensure_base`; on the kit rig it judged the base stale and ran Delete ALL on
Rushhours:Tools/UP-Term while Amarchy/DOpus held files ("object is in use", rc 20): the clone's kit was mostly deleted. Only rig 86's throwaway
clone was hit; a stop + fresh start restored it. On a running kit rig, never call ensure_base.

Cleanup: rig 86 stopped (build/rig86 removed by rig.py); build/rig-saved/86 (32 MB, two save sweeps) deleted; probe PNGs and the resized
picture deleted from the scratchpad. build/rig-reports/rig86 is empty (0 B), left. Scripts/logs (a few KB) left in the scratchpad.
Note: docs/screenshots/prefs-flat.png is now 2265 B, not the 7455 B recorded above (not touched by this run).

## Part 3 (rig 87, OS 3.1 kit rig, 12 Nov): themes and menus

`UPTERM_RIG=87 rig.py start --max --purpose "up-term-ba s74: README theme and menu screenshots"`, `rig.py rtg` (800x600 depth 24; the rig came back up
after rtg, a second start said "already running"). DOpus 5 as Workbench (rig 1's VTC: drawer), Amarchy running, no theme line in ENV:Amarchy/prefs
(default tokyo-night). Script: scratchpad s74p3.py (reuses amarchy_theme_save_rig: open_menu, wait_stable, step, style_downs, keep_requester,
wait_apply_done) and reopen.sh (Cancel on Time Preferences via wasabi ui_click, sys:close on the lister, reopen both, shoot, host pixel count).

Theme switch: Left Amiga + Space > Style > Amarchy theme > walk to the name > Return > Keep in "Amarchy desktop theme" (dist/keymap:229
`style.theme = Amarchy theme | menu:style.theme`; hotkey dist/README.txt:748). Writes `amarchy-gui-theme = <name>` to ENV/ENVARC:Amarchy/prefs.

| file | size | check |
|---|---|---|
| docs/screenshots/amarchy-menu.png | 17072 B | haiku: menu open, Apps..Window readable, highlight on Apps; last row "Screens" cut by the panel bottom (the list scrolls, 18 rows) |
| docs/screenshots/amarchy-menu-themes.png | 16847 B | haiku: theme list miasma..starship, tokyo-night (current) highlighted; last row vantablack cut by the bottom edge (same scrolling) |
| docs/screenshots/theme-tokyo-night.png | 19527 B | haiku CLEAN: bar, Time Preferences, lister, flat gadgets |
| docs/screenshots/theme-gruvbox.png | 20260 B | haiku CLEAN; host count bar 282828/d4be98, accent 7daea3 |
| docs/screenshots/theme-catppuccin-latte.png | 19580 B | haiku: windows and bar light and clean; it objected only that the desktop backdrop stays near-black (0e0e14 in every theme, the DOpus backdrop) |

Gotcha: in 24-bit RTG, windows open before a theme apply keep the old theme's pixels (host count: 23248 px of tokyo-night 1a1b26 inside Time
Preferences and the lister after the switch). Close and reopen the windows after each apply, then shoot. The small "Start!" window at 12,456
(39x15) keeps tokyo-night colours in every theme (482 px of 1a1b26); left in the gruvbox and latte shots.
Possible product finding (not filed): the drawn menu's last visible row is cut by the panel bottom edge on a 600-line screen.

Cleanup: rig 87 stopped (build/rig87 removed by rig.py); build/rig-saved/87 (5.6 MB) deleted; probe and copied PNGs deleted from the scratchpad.
build/rig-reports/rig87 (0 B) left. Scripts/logs (a few KB) left in the scratchpad. Committed 47990447 (README + 5 PNGs); this file not committed.

## Part 4 (rig 96, OS 3.2, 12 Nov): prefs-flat.png retaken

The Part 1 prefs-flat.png in the tree was 344x224 (a cut-off window corner with Cancel), not the 800x600 recorded above.
Retaken on rig 96: `UPTERM_RIG=96 python3 tools/rig/rig.py start --max --os 32 --purpose "up-term-ba s74: README prefs screenshot"`,
`rig.py rtg` (screens: Workbench Screen 800x600 depth 24), `amarchy_gui_rig.py --setup --no-build` (STATUS: flat=on calls=163),
`ami.py exec 'Run >NIL: SYS:Prefs/Input'`; uitree: "Input Preferences" 636x197 at 0,11, active; requesters: none.
`ami.py shot <scratch>/prefs-flat.png` with no region (full screen) -> 800x600, 9375 B (checked with struct), copied to
docs/screenshots/prefs-flat.png. Window left top-left (no window-move command in the rig tools).
Haiku image check: literal answer "NO" only because the window does not fill the screen; it reported an 800x600 screen,
a complete Input Preferences window (title bar, close/depth gadgets, Mouse Speed/Acceleration/Key Repeat gadgets, country list,
Save/Use/Cancel), flat rounded gadgets, no requester. Read as PASS for the README shot.

Register row (for thoughts/shared/research/test-register.md; not added there because that file has a foreign uncommitted edit):
| 12 Nov | rig 96 OS 3.2 060 max, RTG 800x600x24, build/amiga as is, amarchy-flat flat=on | README shot: SYS:Prefs/Input full-screen `ami.py shot` | PASS (800x600, 9375 B; haiku: complete window, flat gadgets, no requester) |
