# Amarchy roadmap: desktop theme, status bar, tiling, capture/notify/power (2026-10-26)

Status: PLAN ONLY, nothing built. Line refs are origin/main (fetched 2026-10-26). "Omarchy" citations are pages of
learn.omacom.io fetched today (manual base /2/the-omarchy-manual/); the omacom/omarchy repo was NOT fetched, so any
repo-path claim is "unverified". Amiga API facts marked (U) are unverified: each names a probe.

## 0. Order and dependencies

Owner order: P1 theme, P2 status bar, P3 tiling, P4 capture/notify/power.
Added 2026-10-27: Phase J (shared job view) runs after prefs-inline R2 and before AMARCHY-UHC; AMARCHY-UHC runs before P1.
Phase L (logo, font tiers, About) runs after AMARCHY-UHC and before P1 (decision 17).

- P1 builds on thoughts/shared/plans/2026-10-26-amarchy-gui-themes.md (GUI themes, sections 1-2: the 16-colour roles and
  theme files in UP-Term:Amarchy/Themes/; default tokyo-night). P1 cannot start before that plan's parser (section 2) lands.
- P2 depends on P1 only for colours (reads the same roles). Needs a new timer-driven task in the commodity (no timer exists today).
- P3 is independent of P1/P2 but must know the P2 bar height (usable area shrinks): P3 reads the usable box from one
  function that P2 also writes to. Build that function in P2 (R2.9), consume in P3.
- P4 notifications reuse P1 colours and the P2 timer/popup-window code; capture is independent; power is independent.
- Shared new infrastructure, build once in P2: (a) a timer.device tick source in amarchy.c's Wait loop (Wait at amarchy.c:329),
  (b) a small themed borderless-window helper (used by bar P2, popup P4).
- Every phase adds ARexx verbs to the AMARCHY port (amarchy.c:206-213) and a `STATUS` sentinel counter (amarchy.c:126), the
  existing pattern for reachability proofs.

## Phase 1. Desktop-wide theme

Omarchy: a theme restyles the whole desktop (terminal, waybar, mako, walker, hyprlock) from Style > Theme, Super+Alt+Space
menu, or Super+Ctrl+Shift+Space directly (/52/themes). Super+Ctrl+Space cycles the theme's backgrounds, kept in
~/.config/omarchy/backgrounds/[theme] (/89/backgrounds; user adds images there). Own-theme format: /92/making-your-own-theme (page listed, not read).

Amiga mechanism:
- Palette: ENV:Sys/palette.prefs + ENVARC:Sys/palette.prefs (IFF PREF/PRHD + PALT, struct PalettePrefs, prefs/palette.h) and live
  LoadRGB32 on the Workbench ViewPort (v2 research table row 9a, thoughts/shared/research/2026-10-22_amarchy-v2-native-workbench.md:40;
  decision D4 at :140/:154: live + file + 15 s auto-revert).
- Backdrop: ENV:Sys/wbpattern.prefs (IFF PREF/PRHD + WBPT, prefs/wbpattern.h) (U); apply by writing the file and nudging IPrefs
  (U, probe: does a Prefs-file write notify IPrefs, or must `C:IPrefs` / `SYS:Prefs/WBPattern USE` run). Picture backdrops need
  datatypes picture; solid colour needs none.
- Fonts: ENV:Sys/font.prefs (IFF PREF/FONT, struct FontPrefs: FP_SYSFONT, FP_SCREENFONT, FP_WBFONT) (U for exact file layout;
  probe: write on rig, read back with Prefs/Font). Fonts must exist in FONTS: ; ship none new, theme names an existing one.
- Auto-revert: before writing, snapshot current ENV:Sys/{palette,wbpattern,font}.prefs to T:amarchy-revert/, apply, open a
  confirm requester (Intuition EasyRequest or gui.c window) with a 15 s timer; no click = copy snapshots back and re-apply.
  Persist (copy ENV: to ENVARC:) only after confirm. Crash safety: write the snapshot dir name to ENVARC:Amarchy/pending-revert so
  the next Amarchy start restores it (a hung screen must not outlive the process).
- Palette safety: Workbench 4-colour screens have pens 0-3 used by every window; theme mapping must keep contrast between pen 0 and 1
  (host test) and never change more than the pens the theme defines (Intuition pens 0-7 first; 8-15 only on deep screens).

Reuse (searched amarchy/ for palette, LoadRGB, wbpattern, prefs: no hit; only the theme code below):
- Role mapping from 16-colour palette: amarchy/menu_theme.c:22 (menu_theme_roles_fb), :37 (menu_theme_roles); theme parser from the
  GUI-themes plan section 2.
- ENVARC write/copy and "AM_GTHEME" saved choice pattern: GUI-themes plan section 3 (same key; add AM_DESKTOP_THEME flag).
- ARexx verb plumbing: amarchy.c:206-213; menu row dispatch via act.c (act_rexx at act.c:122).
- Window helper for the confirm popup: amarchy/gui.c (LockPubScreen at gui.c:1160).

New pieces: amarchy/desk_core.c/.h (portable: theme -> PalettePrefs/WBPattern/FontPrefs bytes, backdrop list, cycle index, contrast
check), amarchy/desk.c (OS side: write prefs, LoadRGB32, snapshot/revert, IPrefs nudge), theme file keys `backdrop=`, `font=`,
`wbpalette=` in the GUI theme format, Super+Ctrl+Space = `desk:backdrop next` in dist/keymap (+ config/keymap.h kind KA_DESK, like KA_WIN
at keymap.h:81; keymap.c win_arg_ok pattern for the arg check), `Style > Desktop theme` rows, man/amarchy.1 section.
Backdrops: UP-Term:Amarchy/Backdrops/<theme>/*.{iff,png-if-datatype}; user dir ENVARC:Amarchy/backdrops/<theme>/ wins (mirrors D1).
Hosts: pure solid-colour backdrops generated at apply time (no image shipped) for every theme as the guaranteed first entry.

Risks: unreadable Workbench (mitigated by auto-revert and boot-time restore); IPrefs not re-reading (U); chip-RAM cost of a picture
backdrop on a 2 MB chip machine (limit backdrop size, depth <= screen depth); font change shifts every window (confirm covers it);
OS 3.2 Prefs files differ in chunk layout (U).

Tests: host: desk_core byte-exact PalettePrefs/WBPattern/FontPrefs vs golden files written by real Prefs on the rig; contrast check
over all themes; cycle wraps; revert snapshot chooses the right files. Rig reachability: send Super+Ctrl+Space through the real
commodity hotkey path (not the ARexx verb), assert STATUS `desks=` counter rises and ENV:Sys/wbpattern.prefs changed; run
`desk:theme tokyo-night`, confirm GetRGB4 of pen 0 equals the theme background via amiga_screenshot probe or a tiny pen-readback
tool; then let the requester time out and assert ENV: files equal the snapshot (auto-revert reachability).

Checklist:
- R1.1 probe wbpattern.prefs and font.prefs layout and IPrefs notification on the rig (text output only)
- R1.2 desk_core: theme -> palette.prefs bytes, golden-tested
- R1.3 desk_core: WBPattern bytes (solid colour and picture forms)
- R1.4 desk_core: FontPrefs bytes
- R1.5 theme file keys backdrop/font/wbpalette parsed (extends GUI-themes parser)
- R1.6 contrast guard for pens 0 and 1, all shipped themes
- R1.7 desk.c: snapshot, apply (file + LoadRGB32), IPrefs nudge
- R1.8 confirm requester with 15 s timer, revert on timeout
- R1.9 boot-time pending-revert restore
- R1.10 persist to ENVARC: only after confirm
- R1.11 per-theme backdrop set plus solid fallback; user dir overrides
- R1.12 keymap kind desk: and Super+Ctrl+Space binding; keymap.c arg check; dist/keymap line
- R1.13 menu rows Style > Desktop theme and Backdrop
- R1.14 STATUS desks= sentinel
- R1.15 man/amarchy.1 and installer copies Backdrops
- R1.16 host tests (R1.2-R1.6, revert logic)
- R1.17 rig reachability test (hotkey, apply, auto-revert)
- R1.18 (owner 2026-10-29) backdrop solid/tiled flat pattern in a slightly DARKER tone of the theme colour, so Amarchy windows, bar and icons stay visible; an on/off option; contrast test against the bar/menu background role
- R1.19 (owner 2026-10-29) window colours: DrawInfo pen assignments in palette.prefs (title fill, text, shine/shadow, active window)
- R1.20 (owner 2026-10-29) drawer window backgrounds: wbpattern.prefs drawer entry, darker theme tone or panel tone
- R1.21 (owner 2026-10-29) pointer colours (sprite pens 17-19) plus optional normal/busy shapes: pointer.prefs
- R1.22 (owner 2026-10-29) icon label colour/outline and Workbench screen title format: 3.2 workbench.prefs (layout unverified, add to R1.1 probe)
- R1.23 (owner 2026-10-29) DOpus 5 Magellan II desktop: same theme applies when DOpus replaces Workbench ("we need to support both"; DOpus is more configurable). Research first: environment file format, ARexx DOPUS.1 verbs, detection, live apply
- R1.24 (owner 2026-10-29) MUI apps through the Selecta flat MUI theme (format pending from the Selecta session)
- R1.25 (owner 2026-10-29) flat Amarchy look for the whole GUI through VisualPrefs (with VP-QuickFix 1.5n for 3.2) + FullPalette pen locking, and Birdie border fills; Amarchy designs its own flat theme (borderless frames, flat symbols, shine/shadow pens = fill, no 3D title bar) in the theme colours. Research: thoughts/shared/research/2026-10-29-amarchy-visualprefs-birdie.md. OWNER: every GUI skinning patch is optional: VisualPrefs, FullPalette and Birdie each have their own Amarchy option, default off; with all off Amarchy never writes their files or touches their Startup-sequence lines, and turning one off restores the stock look. Open before planning: map the undocumented gui.prefs GUI chunk by diffing saves, FullPalette lock flags, headless live apply, Birdie on 3.2 and clean restart, bundle vs link (licences unclear)
Out: screen depth change not picked.

## Phase 2. Status bar on the Workbench screen

Omarchy: Waybar bar; config in ~/.config/waybar/config.jsonc, clock format there, tray group/tray-expander (/102/common-tweaks,
/67/faq); themed with the desktop theme (/52/themes). Workspaces module: not documented in the manual pages fetched, unverified.

Amiga mechanism:
- Window: a borderless backdrop-ish window on the Workbench public screen (LockPubScreen "Workbench", pattern at win.c:148-150,
  gui.c:1160), height = font height + 2, SIMPLE_REFRESH, WFLG_BORDERLESS, never active. Reserve space: Workbench has no
  "struts"; windows may overlap. Choice: place bar in the title-bar area is impossible, so either (a) bar at the screen bottom and
  Amarchy's own win: verbs clamp to the usable area (P3), or (b) overlay only. Plan assumes (a); non-Amarchy windows may cover it
  (acceptable, stated in man page).
- Clock: DateStamp/timer.device UNIT_VBLANK, redraw only on minute change (cheap). RAM: AvailMem(MEMF_CHIP|MEMF_FAST) each 2 s.
- CPU load: no OS counter. Approach (U): sample idle time via a lowest-priority (-128) counting task: counts loops per second vs
  calibration = load%. Cost: it burns idle cycles but yields to everything. Probe: calibrate on 68060 rig and check it changes
  measured benchmarks < 1%. Alternative: read exec's task switch counter (not public, rejected).
- Network: Roadshow bsdsocket.library `SocketBase`, query interface via SocketBaseTags/ `QueryInterfaceTags` (bsdsocket V4 interface
  API, U) for up/down and RX/TX bytes; probe: OpenLibrary("bsdsocket.library",4) on the rig and call
  `GetInterfaceList`/`QueryInterfaceTags(IFQ_State)` (U). Without a stack the item shows "no network" and costs nothing.
- Workspace = front screen name: list Screens via LockIBase + Screen chain (as in do_screen win.c:142-175); highlight front one;
  click = ScreenToFront. Window-key actions already exist (`win:screen next`, dist/keymap:81-82).
- Tray actions: clickable text items firing existing menu rows via `RUN <row>` (amarchy.c ARexx RUN) e.g. Power, Clipboard history.
- Light on 68060: no timer faster than 1 Hz except none; redraw only changed segments (cached strings, compare then draw); fixed-width
  text via one font; no ReAction; no allocations in the tick; all sampling in the commodity's existing Wait loop (amarchy.c:329)
  with one timer.device request, no extra task except the idle counter.

Reuse (searched amarchy/ for AvailMem, timer.device, TR_ADDREQUEST, Roadshow, bsdsocket: none): Wait loop and signal masks amarchy.c:322-329;
ARexx port amarchy.c:303-306; screen walking win.c:142-175; theme roles menu_theme.c:37 and GUI themes plan; draw helpers in gui.c
(checked name list only; read ranges when building); `STATUS` string amarchy.c:126.

New pieces: bar_core.c/.h (portable: segment layout, string formatting of bytes/clock/percent, hit-test, load-percent from counts), bar.c
(window, timer, sampling, redraw, click), bar load task (idle counter), net.c (bsdsocket optional, opened lazily and closed), usable-area
function `bar_usable(screen)` used by P3, config file ENVARC:Amarchy/bar.conf (segments order/on/off, clock format subset of strftime
%H %M %a %d %b), ARexx `BAR ON|OFF|TOGGLE`, keymap `super+alt+b` currently win:title toggle (dist/keymap:83): bar toggle uses another key.

Risks: bar overlapped by other windows; chip RAM for a bar window ~ small; idle counter distorts CPU numbers on slow Zorro setups (U);
bsdsocket open fails or stalls without a stack (use FindPort/OpenLibrary check, never block); overlapping Workbench window
backdrop redraw on menu themes; screen mode other than WB (hires interlace cost: bar redraw on interlace flicker, keep 1 line).

Tests: host: bar_core layout, formatting, hit-test, load calc, RAM string at 4 GB-safe unsigned long math. Rig (060): start Amarchy, send
`STATUS` and assert `bar=1 ticks>0`; wait 3 s and assert ticks rose by ~3 and redraws only for changed segments (counter `bar_draws`
below a threshold, proving no per-tick full redraw); measure idle-counter overhead with a fixed benchmark before/after; click on a
workspace item via amiga_mouse and assert front screen changed (real input path).

Checklist:
- R2.1 probe bsdsocket interface query on rig (up/down, bytes)
- R2.2 probe idle-counter calibration and overhead on 060
- R2.3 timer.device tick in the commodity Wait loop
- R2.4 bar_core: segments, formatting, hit-test (host tests)
- R2.5 bar window on Workbench screen, themed from P1 roles
- R2.6 clock segment
- R2.7 chip/fast RAM segment
- R2.8 CPU load segment (idle counter task, stops when bar off)
- R2.9 `bar_usable()` exported for P3
- R2.10 network segment with lazy bsdsocket and "no network" state
- R2.11 workspace segment (screens, click to front)
- R2.12 tray action segment (RUN <row>)
- R2.13 bar.conf parse and ARexx BAR verbs; keymap binding
- R2.14 STATUS bar= ticks= draws= sentinels
- R2.15 man page and installer
- R2.16 rig reachability + overhead check

## Phase 3. Tiling and snapping

Omarchy: Hyprland tiling: Super+J split orientation, Super+T tiling/floating, Super+L dwindle/scrolling layout, Super+F fullscreen,
Super+Alt+F full width (/53/hotkeys); focus/move bindings by direction: the manual page text fetched lists no arrow bindings,
unverified (omarchy repo default bindings not fetched).

Amiga mechanism: no window manager; Intuition ChangeWindowBox / MoveWindow / SizeWindow on windows of the front screen (already in
win.c). Enumerate with LockIBase (as win.c:56 `usable`, :43 `member`).

Existing today: `win:move/size <dir> [px]` keys at dist/keymap:72-79 (super+shift+arrows move, super+alt+arrows size), handler
win.c:195 `win_exec`, cases at win.c:228, geometry in win_core.c via win_core.h:42-45 (win_move, win_size, win_fullwidth,
win_fullscreen), `win_pick(boxes,n,target,dir)` win_core.h:65 already picks the neighbour by direction for focus (WV_FOCUS case win.c:245).
So focus-by-direction exists: the plan only binds/extends it (verify with `grep -n WV_FOCUS`; arg forms from win_parse).

New verbs (all in the pure core, host-testable): `win:snap left|right|up|down` (halves), `win:snap tl|tr|bl|br` (quarters), `win:snap center`,
`win:tile` (master-stack: master = active window takes left 55%, the rest stack on the right; applied to the screen's usable windows
in the order of the intuition window list), `win:tile off`, `win:master promote` (swap active with master), `win:focus <dir>`
(bind if absent). Tiling is one-shot (apply layout now), plus optional re-tile on window open/close via the commodity's IDCMP
NewWindow notice (P3b, optional; Intuition has no open-window events without a screen notify hook (U); probe: `SetWindowPointer` no; use
a timer scan comparing window counts, reusing P2's tick). Per screen: keep state keyed by screen (win_box.screen exists).
Window exclusion: the terminal and ones with WFLG_BACKDROP/small min=max (not resizable) are skipped; bar window is excluded and
usable area comes from `bar_usable` (R2.9). Honour MinWidth/MaxWidth (win_box minw/maxw) so a refusing window leaves a gap, not overlap.

Reuse (searched amarchy/, config/keymap.h for snap, tile, layout: none): win_core geometry and ring, win_parse closed verb table
(win_core.h:1-27 extend enum win_verb, win_arg at lines ~10-20), keymap check `win_arg_ok` in config/keymap.c, KA_WIN keymap.h:81.

Risks: windows with fixed size; different screens modes; tiling fights Workbench drawer windows (user opt-out list in ENVARC:Amarchy/tile.conf);
re-tile loops if a window grows itself (guard by counter); GUI refresh cost on 060 negligible.

Tests: host: layout arithmetic for 1..9 windows, min/max clamp, usable area with bar, snap quarters on odd sizes; win_parse accepts
new forms and rejects bad (extends tests for win_core). Rig: through the real hotkey (send super+... via amiga_key_press) open three windows,
assert STATUS `wins=` rose and window boxes (amiga_windows) equal the expected master-stack, and that a fixed-size window was skipped.

### P3b. Automatic re-tiling mechanism (design, vtcon-a1 2026-10-26, owner decision 6)

Mechanism: a window-list scan on P2's timer tick, not a patch. Rejected: SetFunction on
OpenWindowTagList/CloseWindow/ChangeWindowBox. A patch runs inside every program's task (stack and
register rules of the caller), cannot be removed safely while another patcher sits on top of it, and
breaks the "quit the commodity, system is as before" property. Intuition has no system-wide
window-open event (IDCMP_CHANGEWINDOW and friends only reach the window's owner); screennotify.library is
third-party. So the commodity looks instead of listening.

- Tick: P2's timer.device source (R2.3), 4 Hz while auto-tiling is on for any screen, no tick at all when
  every screen is off (zero cost when unused).
- Fingerprint per screen, taken under LockIBase (pattern win.c:56 `usable`): the set of tileable
  Window pointers in window-list order. Boxes are NOT in the fingerprint: a window that resizes itself
  does not trigger a re-tile, which is what prevents fights.
- Trigger: the set changed (a window opened, closed, or moved screen: it leaves one screen's set and
  enters another's). Debounce: re-tile when the new set has been stable for 2 ticks (0.5 s), so a
  program opening four windows at start re-tiles once.
- Apply: win_core master-stack (R3.4) on bar_usable() (R2.9), ChangeWindowBox under the lock (existing
  win.c:116 path). New windows join as the last stack entry; master stays unless closed.
- Refusal guard: after applying, the next stable tick compares each window's box with the one asked for.
  A window that does not take its box on 2 consecutive applies becomes floating for its lifetime (keyed by
  Window pointer, dropped when it closes). Counter in STATUS (`tilefloat=`).
- Tileable = all of: has WFLG_SIZEGADGET, not WFLG_BACKDROP, MinWidth != MaxWidth or MinHeight != MaxHeight,
  not an Amarchy-owned window (bar, menu, notify popup), title not matched by ENVARC:Amarchy/tile.conf
  (one AmigaDOS pattern per line, ParsePatternNoCase). Requesters (EasyRequest, ASL) have no size gadget
  and drop out by the first rule; probe confirms.
- Default: auto re-tiling is off on every screen until toggled (owner decision 11).
- Per-screen state keyed by screen title (Screen pointers are reused); `win:tile auto on|off|toggle` verb,
  saved as `tile-auto=<title>,<title>` in ENV:/ENVARC:Amarchy/prefs (amconf, both files as D4 of the
  prefs-inline plan).
- Sentinels: STATUS `retiles=` (applies done) and `tilefloat=`; rig test opens three shells on Workbench
  with auto on, asserts retiles rose by exactly 1 per open after debounce, and amiga_windows boxes equal
  the master-stack.
- Probe P3b.0 (rig, text output only, before code): (1) EasyRequest and ASL requester windows lack
  WFLG_SIZEGADGET; (2) LockIBase walk at 4 Hz costs under 0.5% CPU on the 060 rig (idle counter from R2.2);
  (3) a Workbench drawer window takes ChangeWindowBox and keeps it after the drawer refreshes.

Checklist:
- R3.1 grep and confirm existing focus-direction forms and keys (win_parse, win.c:245)
- R3.2 win_parse: snap and tile and master forms (closed list)
- R3.3 win_core: halves and quarters, usable-area aware
- R3.4 win_core: master-stack layout, min/max aware
- R3.5 win.c: enumerate tileable windows per screen, apply
- R3.6 skip list (fixed size, backdrop, bar, user opt-out file)
- R3.7 keymap bindings and keymap.c arg check, dist/keymap lines
- R3.7b P3b.0 probe, then auto re-tile scan, debounce, refusal guard, tile.conf, per-screen toggle, STATUS retiles=/tilefloat=
- R3.8 menu rows under Window
- R3.9 optional auto re-tile on window count change with loop guard
- R3.10 man page
- R3.11 host tests
- R3.12 rig reachability test (hotkey -> layout boxes)

## Phase 4. Capture, notifications, power

Omarchy: `omarchy screenshot` CLI (/115/omarchy-cli, only mentioned); save location set in Setup > Defaults (/67/faq); region OCR to clipboard
Super+Ctrl+PrtScr (/116/text-extraction-dictation). Notifications: mako, styled by the theme (/52/themes; /119/notices exists, content not read).
Power: Super+Escape system menu (suspend, restart...), Super+Ctrl+L lock (/53/hotkeys).

Amiga mechanism:
- Screenshot: read the screen's BitMap (ReadPixel/ReadPixelArray8, or direct plane copy for the chosen screen/window/region rectangle),
  with the ColorMap for the palette; write IFF ILBM via iffparse.library (ILBM BMHD+CMAP+BODY, ByteRun1 compression, small code) to
  ENVARC:Amarchy/Shots/ (default RAM:Shots, configurable). Region: reuse a rubber-band selector (IDCMP mouse on a transparent
  window or GUI; probe: window over the whole screen with no content works on Workbench).
- PNG: only if a png.datatype with write support exists (U; datatypes can read PNG; write needs `png.datatype` V45+ plus the
  picture.datatype DTM_WRITE; probe: DoMethod DTM_WRITE with `PNG` on the rig). Otherwise IFF only (decision below).
- Clipboard ILBM: write the same FORM ILBM to clipboard.device unit 0 (the clipboard IFF format; reuse the clipboard device code
  from clipwatch.c:42-52 which opens `clipboard.device` PRIMARY_CLIP with CBD ops; write path is new).
- Notifications: no Ringhio in OS 3.2 (the owner's statement; not re-verified). Build an ARexx-port daemon part inside the commodity:
  `NOTIFY <title>|<body>|<seconds>` on the existing AMARCHY port (amarchy.c:206), or a separate port AMARCHY.NOTIFY (U: separate port
  allows other programs to post without the commodity; choose the separate port). Popup: borderless themed window at screen top-right
  (P1 colours, P2 window helper), auto-closes via the P2 timer tick, stacks up to 3, click to dismiss.
  CLI `amnotify` (tiny C tool, no new framework) so scripts say `amnotify "Done" "build ok"`.
- Power: menu "Safe to switch off": run the kit's own `Files/Reboot`-style flush (Install.installer:475-491 states "writes every
  volume's buffers out before ColdReboot"; reuse its binary, do not rewrite), without the ColdReboot step: list volumes
  (LockDosList), `Inhibit`-free approach: send ACTION_FLUSH to each file system (DoPkt ACTION_FLUSH) and wait until the drive is quiet
  (probe: `Info` shows no disk activity; wait for the IO Request queue empty (U); poll 500 ms for 5 s of no change in fh/lock counts
  and a fixed minimum wait), then show a notification "Safe to switch off". Cold reboot: System > Reboot already exists: reuse the same
  row and command (searched amarchy/ for Reboot: no hit; installer wires Files/Reboot; the System > Reboot menu row lives in the
  generated index, find with `grep -rn Reboot config dist` before editing).
- Lock: out of scope here (OS 3.2 has no lock screen).

Reuse (searched amarchy/ for png, ILBM, iffparse, screenshot, notify: none): clipboard device open pattern clipwatch.c:34-52; ARexx
port amarchy.c:303-306; popup/timer/theme from P1/P2; Reboot kit binary per installer lines 475-491; screen walk win.c:142.

New pieces: shot_core.c/.h (portable ILBM encoder: planar conversion, ByteRun1, chunk writer, host-tested), shot.c (OS read and region
UI, file and clip write), notify_core.c (queue, stacking positions, expiry) + notify.c (window, port), amnotify tool, power.c (flush +
quiet wait), keymap kind `cap:` (`cap:screen`, `cap:window`, `cap:region`) and menu rows; man page.

Risks: reading a screen's planes on RTG/Picasso96 screens differs from chipset screens (use ReadPixelArray8 / graphics API, not planes;
probe on the rig, 060 only chipset? U); memory for a large screen ILBM (stream rows, no whole-image buffer); clipboard ILBM
size on small RAM; notify popup stealing focus (WFLG_NOCAREREFRESH and no activate); flush wait never ends on a CD drive (cap at 15 s);
"Safe to switch off" is advice only, and only for filesystems that honour ACTION_FLUSH (FFS/SFS yes, U for others).

Tests: host: ILBM encoder round-trip against a golden file and an independent decoder (python script reading IFF), ByteRun1 edge runs,
notify stacking and expiry arithmetic. Rig: press the real capture hotkey; assert a new file in the shots dir, header FORM/ILBM and
dimensions equal the screen; read clipboard via clipboard.device and assert the same FORM; post `amnotify` and assert STATUS `notes=`
rose and a window titled by the popup exists via amiga_windows; run power menu row and assert the `flush` counter and the final
notification, never an actual reboot on the rig except in the one reboot-row test (wait command + 5 s quiet rule first).

Checklist:
- R4.1 probe png.datatype write support and screen read path on rig (text output)
- R4.2 shot_core: ILBM encoder + host golden/decoder test
- R4.3 shot.c: capture screen rectangle to file
- R4.4 capture window and region (rubber-band)
- R4.5 clipboard ILBM write
- R4.6 PNG path if R4.1 says yes, else documented IFF only
- R4.7 keymap cap: kind, arg check, bindings, menu rows
- R4.8 notify_core: queue, stacking, expiry (host tests)
- R4.9 notify.c: ARexx port AMARCHY.NOTIFY and popup window, themed
- R4.10 amnotify CLI
- R4.11 notify on capture done (feature uses notify)
- R4.12 power.c: flush all volumes, quiet wait with cap
- R4.13 "Safe to switch off" row and final notice
- R4.14 cold reboot row wired to the existing System > Reboot command
- R4.15 STATUS shots= notes= flush= sentinels
- R4.16 man page and installer
- R4.17 rig reachability tests (capture, clip, notify, power)

Totals: P1 17, P2 16, P3 12, P4 17 = 62 items.

## Phase J. Shared job view (before AMARCHY-UHC)

Source: Omarchy gap research thoughts/shared/research/2026-10-27-omarchy-tui-gap.md (section 4). Omarchy wraps every job in
omarchy-show-logo, the live output, then omarchy-show-done. Owner (2026-10-27): build this once, as its own item, before
AMARCHY-UHC, so packages, Update and every later action that runs something reuse it.

Amarchy gap today: act_run does not read the child's output; there is no log view; the gui loop waits only on IDCMP and
CTRL_C, so streaming needs a pipe or timer signal in that Wait.
- RJ.1 Job runner: start a command with its output on a PIPE: (or a temp file plus a poll tick), signal bit added to the gui Wait
- RJ.2 Job view in the drawn menu: logo header band, scrolling live output, Done/Failed footer with the exit code
- RJ.3 Input rows inside the job view (string, choose-one, confirm), first user: upkg interactive questions
- RJ.4 Theme colours for header, output text and footer from the GUI-themes roles
- RJ.5 Cancel (Esc sends CTRL_C to the child) and close after Done
- RJ.6 Host tests (output line splitter, footer state machine)
- RJ.7 Rig reachability test: a menu row runs a job, the view shows its output lines and the Done footer (sentinel counter)

## Phase L. Omarchy look: logo, font tiers, About (owner answers 2026-10-27)

- RL.1 Startup logo when the Amarchy commodity starts (themed window, about 2 s). Not in Startup-Sequence: nothing touches the boot.
- RL.2 Logo file in UP-Term:Amarchy/ that the user can replace (branding); RJ.2 and RL.1 read the same file
- RL.3 Three font tiers in the drawn menu: header, row title, smaller description line, all scaled from one base size the theme sets
  (Omarchy: base 12, title/heading 16, description 11). menu_font.c opens up to three fonts per screen; render/fontpair.c pairs exist.
- RL.4 Row description line from the keymap `|` field; row height grows, so the visible row count on 256-line screens drops
- RL.5 Theme file tokens font-base plus ratios (extends R1.5)
- RL.6 About row: logo, Kickstart/Workbench version, CPU, chip/fast RAM, UP-Term and Amarchy versions, uptime
- RL.7 Host tests and one rig reachability test (menu opens with three fonts; About row draws the version text)

## Phase X. Omarchy extras (owner picked all four, 2026-10-27; after P2 because RX.3 needs the R2.3 tick)

- RX.1 Toggle rows that show state: keymap row `checked:` condition (ENV: variable or file exists), tick glyph drawn
- RX.2 Add-launcher wizard: input rows (RJ.3) for name, command, window style, icon; writes an Amarchy app entry
- RX.3 Reminders: list with times, fired by the R2.3 tick, shown through the P4 notification (R4.9)
- RX.4 Disk speed test row: write and read a file on the chosen volume, timed with timer.device, result in the job view
- RX.5 Host tests and one rig reachability test per row kind

Later phase, after R2.3 (timer tick): screensaver with the logo and a text effect. Owner: "Later phase". No work now.

## Open decisions for the owner

1. Bar placement. Recommend: bar at screen bottom, Amarchy verbs respect it, other windows may cover it. Trade-off: cannot reserve space like a Wayland strut; the alternative (top overlay on the title bar) hides the Workbench menu bar clock/menus.
2. Auto-revert timeout and persistence. Recommend: 15 s, persist only after confirm, boot-time restore after a crash. Trade-off: a requester every theme change versus a real risk of an unreadable screen.
3. Backdrop images. Recommend: ship solid-colour backdrops per theme plus a user directory for pictures; no image assets in the repo. Trade-off: less pretty out of the box, no chip-RAM or licensing cost.
4. Fonts per theme. Recommend: theme may name a font that already exists in FONTS:; ship none. Trade-off: fewer visual changes on a stock install.
5. CPU load source. Recommend: lowest-priority idle counter, off when the bar is off. Trade-off: tiny overhead and an inexact number versus no CPU item at all.
6. Tiling model. Recommend: one-shot snap and tile commands (no auto re-tile), master-stack only. Trade-off: not Hyprland-like automatic, but no fights with windows that resize themselves; auto re-tile can follow as R3.9.
7. PNG. Recommend: IFF ILBM only unless the probe finds a PNG writer. Trade-off: IFF is native and opens everywhere on Amiga but not on a Mac without a converter.
8. Notification transport. Recommend: separate port AMARCHY.NOTIFY plus the amnotify tool. Trade-off: one more port versus other programs posting while the commodity is busy in a menu.
9. "Safe to switch off" semantics. Recommend: flush plus fixed quiet wait, message says "best effort". Trade-off: no true guarantee on third-party file systems.
10. Build order inside phases. Recommend: P1 R1.1 probes first (one rig session answering the (U) list for P1 and P4 in one go). Trade-off: delays code by one session, saves reworking prefs writers.


## Decisions (2026-10-26)

Owner answers:
- 1 Status bar: at the bottom of the Workbench screen; Amarchy verbs (snap, zip, tile) keep clear of it through bar_usable().
- 2 Theme writes: live preview with 15 s auto-revert; saved to ENVARC: only after confirm; boot-time restore of the old prefs after a crash.
- 3 Backdrops: a solid theme-coloured backdrop per theme plus a per-theme picture drawer the user fills; Super+Ctrl+Space cycles it. No Omarchy wallpapers shipped.
- 6 Tiling: AUTOMATIC re-tiling (owner chose this over the one-shot recommendation). Windows re-tile when one opens, closes or changes screen. The plan must now name the mechanism (resident watcher on IDCMP/Intuition events vs a patched OpenWindow/CloseWindow), an opt-out list for windows that refuse resizing (requesters, fixed-size windows, backdrop windows), a per-screen on/off toggle, and a probe for the chosen mechanism before any code.
- 11 Auto-tile default (2026-10-26): every screen starts with auto re-tiling OFF; `win:tile auto toggle` turns it on per screen and tile-auto= remembers it. Nothing moves on first start.
- 12 Start: the roadmap starts after the prefs-inline plan finishes R2. The P1 probe session does not share rig 7 with prefs R1.

Taken at the plan's recommendation (implementation defaults, owner may veto):
- 4 Theme fonts: only fonts already in FONTS:.
- 5 CPU load: lowest-priority idle counter, stopped when the bar is off.
- 7 Capture format: IFF ILBM only unless the probe finds a PNG writer.
- 8 Notifications: separate AMARCHY.NOTIFY port plus an amnotify tool.
- 9 Safe to switch off: flush plus quiet wait, best effort.
- 10 One rig probe session first for every unverified item.
- 13 (2026-10-27) Logo: shown at Amarchy start and as the job view header, plus an About row. No pre-Workbench splash.
- 14 (2026-10-27) Font sizes: three tiers (header, row, description) from one theme base size.
- 15 (2026-10-27) Shared job view: its own Phase J, before AMARCHY-UHC.
- 16 (2026-10-27) Screensaver: a later phase after the R2.3 timer; no work now.
- 17 (2026-10-27) Phase L runs after AMARCHY-UHC and before P1.
- 18 (2026-10-27) Input rows: general (text, choose one, confirm), built once in RJ.3 and reused by every form.
- 19 (2026-10-27) Extras: all four (toggle rows, add-launcher wizard, reminders, disk speed test) as Phase X.
