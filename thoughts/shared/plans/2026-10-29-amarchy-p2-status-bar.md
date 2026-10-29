---
date: 2026-10-29
author: spot (Amarchy session s24)
topic: AMARCHY-P2
status: decided
ledger: thoughts/spot/todos.md (add row AMARCHY-P2; rows tagged s24)
roadmap: thoughts/shared/plans/2026-10-26-amarchy-roadmap.md, Phase 2 (R2.1-R2.16)
depends: P1 theme roles (menu_theme.c, pens_amiga.c, in AMARCHY_SRC), OMARCHY-AI F2 bar (4086d352), P3-B1 strut (e521437e)
---

# Amarchy Phase 2: status bar on the Workbench screen (AMARCHY-P2)

## Goal

The Amarchy bar at the bottom of the Workbench screen grows from one segment (Agents) into the Omarchy-style status bar:
screens on the left, clock in the centre, CPU, RAM, network, Agents and a system icon on the right. It costs nothing when
nothing changes: one timer tick per second in the commodity's Wait loop, redraws only for segments whose text changed,
the CPU idle counter stops when the bar is off.

P2 also delivers three shared pieces that P4 is waiting for (P4 plan section "Interfaces assumed from Phase 2"):

- T: the commodity tick (`tick_now()`, per-tick hooks, STATUS `ticks=`), running whether the bar is on or off.
- B: one themed borderless window helper (open on a pubscreen at x/y/w/h, never activated, theme pens, draw a text line,
  click input on a port the commodity Waits on, close). The bar is its first user, P4 notices the second.
- U: `bar_usable(screen, box)`, the usable area of a screen once the bar strut is taken off.

## Facts found (origin/main 75562887) and what to reuse

| Need | Reuse | Where |
|---|---|---|
| Bar window | "Amarchy bar", borderless, bottom of Workbench pubscreen, height font+2, WA_Activate FALSE, SimpleRefresh, RMBTrap | amarchy/bar.c (4086d352) |
| Segment layout, hit-test, dirty flag | bar_core: BAR_MAX_SEGS 12, BAR_TEXT 32, BAR_LEFT/BAR_RIGHT, bar_set_text dirty only on change, bar_hit, bar_click, bar_take_dirty | amarchy/bar_core.c/.h, tests/test_bar.c |
| Theme pens | pens_get() via gt_dos_load_current + menu_theme_roles_gui + vr_pen_obtain; pens_release() | amarchy/bar.c, render/pens_amiga.h, amarchy/menu_theme.c |
| Timer | poll_timer: pt_open/close/start/stop/sigmask/fired, timer.device UNIT_VBLANK, one request re-armed | amarchy/poll_timer.c/.h (shared with AmTile tile.c) |
| Re-raise over foreign windows | bar_covered + BAR_RAISE_POLL_MICROS 500000 | amarchy/bar_core.c, bar.c g_poll |
| Strut | window titled "Amarchy bar" on the window's own screen gives s->bottom = win_strut() | amarchy/win_apply.c:20-35 (e521437e) |
| Panel anchored to a segment | bar_panel_anchor, menu node "agents" | amarchy/bar.c |
| Right click launch | bar_takes_right_press in the input handler + bar_right_click | amarchy/bar.c, amarchy.c |
| Notice tick | note_tick(now) in note_core (fa35244b), port AMARCHY.NOTIFY (7b806183) | amarchy/note_core.c, note.c |
| Wait loop | Wait(cxs|rxs|nos|nts|lps|wrs|cws|bs|bts|shot|CTRL_C); bar_open at ~377, bar_close ~418 | amarchy/amarchy.c |
| STATUS | 15 key=value pairs, result capped at REXX_HOST_OUT 160 | amarchy/amarchy.c:147-175, amarchy/rexx_host.h:7 |
| System menu | super+esc opens menu:system | dist/keymap |
| Keymap | super+b is free; super+ctrl+b = win:title toggle; super+shift+b = /bg; super+alt chords refused by the parser | dist/keymap, config/keymap.c |
| Network on rigs | rig config sets `bsdsocket_library = 1` (UAE emulated bsdsocket, not Roadshow) | tools/rig/rig.py:984 |
| Build | AMARCHY_SRC already links menu_theme.c, pens_amiga.c, poll_timer.c, bar.c, bar_core.c | Makefile:737 |

Searched for an existing tick module, window helper and usable-area function: none. The bar's `g_poll` runs only while
the bar is open, so it cannot be the commodity tick. win_apply.c holds the only copy of the strut rule.

## Checklist

IDs from the roadmap; splits get a letter. DONE rows name the commit.

Probes (before code that depends on them)
- [x] R2.1 (rig half 816bb673; A1200 half done 2026-10-29, see research doc) Probe network query on rig 4 and on the A1200: QueryInterfaceTags (Roadshow) for interface name and up/down;
      what UAE's emulated bsdsocket answers; behaviour with no stack (OpenLibrary fails, never blocks). Result to
      thoughts/shared/research/. Decides the fallback in R2.10.
- [x] R2.2 (rig half 816bb673; A1200 half done 2026-10-29, see research doc) Probe the idle counter on rig 4 (060) and A1200: counts per second idle, calibration over 1 s, overhead of a
      -128 task on a fixed benchmark (before/after under 2 percent).

Shared pieces (P4 waits on these)
- [x] R2.3a (05e11fa6; rig proof open under R2.16) tick.c/.h: owns one poll_timer at 1 s, `tick_open/close/sigmask/serve`, `tick_now()` (seconds since start),
      hook list `tick_hook(fn)` (fixed array, no allocation in the tick). Opened by amarchy.c before bar_open, runs with
      the bar off. STATUS `ticks=`.
- [x] R2.3b (05e11fa6; rig proof open under R2.16) Register note_tick as a hook (notice expiry) and the bar sampler as a hook.
- [ ] R2.3c Bar raise check: keep the bar's own 0.5 s g_poll only while open (unchanged), or ride the tick if 1 s raise
      latency is fine on the rig. Default: keep g_poll; measure raises in the rig test.
- [x] R2.5a (05e11fa6; rig proof open under R2.16) Extract the themed window helper from bar.c to themewin.c/.h: `tw_open(screen, x, y, w, h, title, idcmp)`,
      `tw_pens()` (bg, text, accent, border, MT_OK, MT_FAIL), `tw_text(x, y, s, pen)`, `tw_fill`, `tw_frame`, `tw_close`,
      user port shared by the commodity (one signal bit for all helper windows). bar.c becomes its first user, no
      behaviour change (bar host tests and the rig Agents check stay green).
- [x] R2.5b (0622dde7; host status_core 35 checks + revert proof; rig 6 step recolour green, 15 of 15 steps, and FAIL with tick_hook(tw_tick) removed) Recolour on theme switch: helper re-reads pens on the theme-change signal and redraws every helper window.
      Closes OMARCHY-AI F3 (thoughts/shared/plans/2026-10-28-omarchy-ai.md:95).
- [x] R2.5 Bar window on the Workbench screen, themed from P1 roles. DONE 4086d352.
- [x] R2.5c (c5ee09b1) BAR ON/OFF/TOGGLE during the W43 hold is remembered (status_core bar_plan, g_bar_want) and applied when the hold ends; host test in test_status_core.c. Rig proof (rig 6, 2026-10-29, step bar_hold in amarchy_p2_rig.py): during the W43 hold (prefs rtg vs AGA mode) BAR OFF and BAR ON leave no bar window (bar=0, draws=0); the window opens 31 s after start, after the hold. Revert proof: with bar_plan neutered, test_status_core fails 2 of 30 (lines 55, 60); restored 0 of 30.
- [x] R2.9 (05e11fa6; rig proof open under R2.16) `bar_usable(screen, box)` exported from bar.c; win_apply.c calls it instead of its own title lookup (one
      source for the strut rule). Returns the full screen minus title bar when the bar is off or on another screen.

Bar core
- [x] R2.4a (8bccda94) bar_core: add BAR_CENTRE group; layout = left group from the left edge, right group from the right edge,
      centre group centred on the screen and pushed aside when it would overlap. Host tests at widths 640, 800, 1280.
- [x] R2.4b (8bccda94) bar_core: cut rule. When the groups do not fit, the screens list loses entries from its right end and shows
- [x] R2.4d (c5ee09b1) Narrow bar drops right segments by rank: Network, CPU, RAM, Agents; Clock, System, first screen never (bar_core place_right). Host test overlap case fails before (11 checks), passes after. Drop test revert proof: drop logic reverted, test_bar fails 10 checks (lines 69-88). Rig 6 at 640 px (AGA PAL hires), all 14 rig steps OK: BAR TEXT 2@265+54|5@321+49|6@372+91|3@465+88|1@555+41|4@598+42|200@0+60*, no overlap, no negative x, ends at 640. Very narrow bar (R2.4e): bar_core clips the first screen to the room left of the centre group; test_very_narrow_bar_clips_first_screen_without_overlap fails 11 checks without the fix, passes with it (234 checks). Open: when centre plus undroppable right segments alone fill the bar, the left group still overlaps.
      a "+N" marker; the centre and right groups never cut. Host test at 640 with 6 screens.
- [x] R2.4 Segments, hit-test, Agents segment. PARTIAL DONE 4086d352; the rows above finish it.
- [x] R2.4c (8bccda94) bar_core formatters (pure, host-tested): bytes to "1.2M"/"61M"/"512K" (unsigned long, safe to 4 GB),
      percent from idle counts, clock strftime subset %H %M %a %d %b %Y, screen short name (first word, whole; the "+N" rule cuts for width).

Segments
- [x] R2.6 Clock (centre): "Mon 14:05" from `%a %H:%M`; left click flips to "Mon 29 Oct 2026" (`%a %d %b %Y`) until the (done 7f8d531d, rig 6)
      next click. Text set every tick, redraw only when the minute or the mode changes.
- [x] R2.7 RAM (right): "C 1.2M F 61M", AvailMem(MEMF_CHIP) and AvailMem(MEMF_FAST), sampled every 2 s. (done 7f8d531d, rig 6)
- [x] R2.8 (0432be4a, rig 6) CPU (right): "CPU 12%" from an idle counter task at priority -128, started by bar_open, stopped by bar_close
      (Signal + wait for exit, no Forbid loop). Sampled every tick.
- [~] R2.10 (0432be4a, 1caaa456, rig 6 half DONE; A1200: up half DONE 2026-10-29, down half DONE 2026-10-29 via ConfigureNetInterface replayeth DOWN/UP: net=3 "replayeth down" 5 s after DOWN, net=4 "replayeth up" 5 s after UP; see research 2026-10-29-amarchy-p2-probes.md) Network (right): the first interface that is up (else the first) from ObtainInterfaceList + QueryInterfaceTagList(IFQ_State, ti_Data = LONG *), "eth0 up" / "eth0 down", "No network" when no stack, no interface or the query fails; "Network emulated" and NO query when the id string starts "UAE". Tags and SM_* come from third_party/roadshow-netinclude (bar.c asserts bar_core.h's SM numbers equal the header's at compile time). BAR STATUS now ends net=N (1 emulated, 2 stack without answering interface, 3 down, 4 up; -1 before the first sample). Host test: bar suite (bar_net_state_kind, bar_fmt_net), revert proof (UP/ONLINE mapping broken: 5 FAIL lines), status_core net=; rig 6: amarchy_p2_rig.py 0 failed, network step net=1. Library opened at sample time and closed after; sampled every 5 s; redraw on kind or name change. No traffic numbers.
      A1200 2026-10-29: new Amarchy installed (md5 e4f31a48... = build of 354eda1f; old kept as UP-Term:bin/Amarchy.bak-20261029); step (1) PASS: BAR STATUS net=4, BAR TEXT segment `6@404+78=replayeth up`, still net=4 6 s later, no Guru, no requester. Steps (2) and (3) later RUN with DOWN/UP (wasabid stayed up); earlier note: the only interface `replayeth` carries wasabi's own link, taking it down cuts the A1200 off; needs another path (serial/console, or a second interface). Original steps: copy the new Amarchy, start it, then (1) `rx "address AMARCHY 'BAR STATUS'"` (or the owner's usual BAR STATUS call) and `BAR TEXT` with the network cable in and Roadshow up: expect net=4 and segment text "<iface> up"; (2) `Roadshow` interface taken down (`NetShutdown`/`ifconfig <iface> down` equivalent, or unplug and wait one 5 s sample): expect net=3 and "<iface> down" (a stack with no interface answering: net=2 "No network"); (3) bring it up again: net=4 within 5 s. PASS = the three values in that order, no Guru.
      no stack or the query fails. Library opened at sample time and closed after; sampled every 5 s; redraw only on
      change. No traffic numbers.
- [x] R2.11 Screens (left): every public screen as its short name, the front screen in the accent pen, left click = (done 7f8d531d, rig 6)
      ScreenToFront of that screen. List rebuilt each tick from the pubscreen list (LockPubScreenList, copy names,
      UnlockPubScreenList at once).
- [x] R2.12a System icon (right end): left click opens menu:system (same node as super+esc), panel anchored to the (done 7f8d531d, rig 6)
      segment with bar_panel_anchor.
- [x] R2.12 Tray DONE (edcbf029): RUN <row> segments from bar.conf (label + command line, click runs it through proclaunch). barconf_plan orders/hides segments; host tests test_plan + test_tray_click revert-proven; rig step tray (real click makes a sentinel file) on rig 6, full amarchy_p2_rig 16 of 16.

Config, control, docs
- [x] R2.13a (done 8a99c34f, rig 6; start/clock/clockalt wired, segment order + on/off + run rows parsed and host-tested, consumed by the later layout/tray rows) bar.conf at ENVARC:Amarchy/bar.conf (ENV: copy read first): `start on|off`, `clock <fmt>`, `clockalt <fmt>`,
      segment order per group, segment on/off, `run <label> <command>` rows. Parsed by a pure core (host tests), missing
      file = defaults.
- [x] R2.13b (part, 04125970) ARexx `BAR ON|OFF|TOGGLE` and `BAR STATUS` (bar= draws= panels= launches= raises= ticks= done, rig 6 green; segs=, cpu=, net= open until those segments exist), so the STATUS string
      stays inside REXX_HOST_OUT 160.
- [x] R2.13c (done 8a99c34f, rig 6 bar_conf step; revert proof: start-off read and bar_port dispatch each made it FAIL) Key super+b = bar toggle (new commodity action, dist/keymap and keymap_default), bar on at start unless
      bar.conf says `start off`.
- [x] R2.14 (04125970; STATUS 99 bytes at zeros on rig 6, 147 at 5-digit counters, tests/test_status_core.c) STATUS adds `ticks=` (bar=, draws= exist). Check the full STATUS string length on the rig stays under 160.
- [ ] R2.15 Man page (bar segments, bar.conf keys, BAR command, super+b) and installer copies a default bar.conf.

Proof
- [ ] R2.16 (partial: tools/rig/amarchy_p2_rig.py steps start, tick_rises, bar_status, bar_off, bar_on, status_keys pass on rig 6; later rows add steps) Rig reachability test tools/rig/amarchy_p2_rig.py on rig 4 (060), see below, plus idle overhead measurement.
- [ ] R2.17 A1200 last check: bar visible, clock, RAM, network row from the real stack (Roadshow), super+b.

### Knobs and the case that proves each moves output

| Knob | Case |
|---|---|
| bar.conf `start off` | rig: commodity starts, STATUS bar=0, no window titled "Amarchy bar" |
| bar.conf `clock %H:%M` | host: formatter gives "14:05"; rig: segment text read through BAR STATUS segs= |
| bar.conf `clockalt` | host: second format after a click |
| bar.conf segment order/off | host: layout without the RAM segment moves Agents right edge; rig: BAR STATUS segs= lists the order |
| bar.conf `run` row | rig: click on the tray segment starts the command (sentinel file written) |
| BAR ON/OFF/TOGGLE | rig: STATUS bar= flips; idle task gone (amiga_ps) when off |
| super+b | rig: key press flips bar= (real input path) |
| tick hooks | host: hook list calls each hook once per tick; rig: ticks= rises by about 3 in 3 s with bar off |

## Host tests (all `make test ONLY=<suite>`, nice 19, max 2 workers)

- test_bar (extend): centre group, cut rule with "+N", widths 640/800/1280, formatters, screen short names, hit-test of
  every segment kind.
- test_tick (new): hook list, tick_now, no hook runs twice per tick.
- test_barconf (new): bar.conf parse, defaults on missing keys, bad lines ignored with a count.
- test_win (existing): strut via bar_usable gives the same boxes as before (regression for R2.9).

## Rig reachability test (one script, rows share state)

tools/rig/amarchy_p2_rig.py, rig 4, `UPTERM_RIG=4 rig.py start --max --purpose "Amarchy <session>: P2 bar rig proof"`.
Drives the real commodity binary through its ARexx port and real input.

1. Start Amarchy; STATUS has `bar=1 ticks=`; window "Amarchy bar" exists at the screen bottom (amiga_windows).
2. Wait 3 s: ticks rose by 2 to 4; draws rose by fewer than ticks + 2 (proves no full redraw per tick).
3. BAR STATUS segs= lists screens, clock, CPU, RAM, network, Agents, system in the bar.conf order.
4. Open a second public screen (UP-Term): screens segment gains its name; mouse click on it brings it to front
   (amiga_screens front changes).
5. Click the clock: segs= clock text changes to the long form; click again: back.
6. Click the system icon: menu:system panel opens (amiga_windows title), Escape closes it.
7. super+b: bar=0, no bar window, idle task gone (amiga_ps); a window maximised with the Amarchy verb now reaches the
   screen bottom; super+b again: bar=1, maximised window stops above the bar (bar_usable wired).
8. Theme switch: bar redraws (draws rose) with the new pens (pen numbers read through BAR STATUS).
9. Overhead: fixed benchmark with the bar off and on; difference under 2 percent.
10. Stop rig 4.

Revert proof: revert R2.9 wiring in win_apply.c and step 7 fails; revert the tick hook registration and step 2 fails.

## Dependencies

- P4 rows R4.9b, R4.9c, R4.11, R4.12 (window), R4.13 (notice) wait for R2.3a, R2.5a and R2.9.
- P3 G*/R* rows that use the usable area take R2.9 when it lands; until then win_apply.c keeps its title lookup.
- OMARCHY-AI F3 closed by R2.5b (0622dde7): bar recolours live; agent-tab text colour itself not run.

## Risks

- UAE's emulated bsdsocket may not answer QueryInterfaceTags; R2.1 decides whether the rig proves "no network" only and
  the A1200 proves the up/down row.
- An idle task at -128 on 68020 machines without fast RAM may count too few loops for a stable percent; R2.2 measures.
- LockPubScreenList held too long blocks screen opens; copy names and unlock at once, never draw while locked.
- STATUS is near its 160-character cap; new counters go to BAR STATUS.
- Merge points with P4 and P3: status_text arrays, the Wait mask, Makefile AMARCHY_SRC (line 737). Land R2.3a and R2.5a
  first and alone so P4 can rebase on them.
- Theme switch while a panel is open: helper must redraw both windows, not only the bar.

## Owner answers (2026-10-29)

- Layout: Omarchy split. Left: screens. Centre: clock. Right: CPU, RAM, network, Agents, system icon. On 640-wide screens
  the screens list is cut first.
- Toggle: super+b, bar on at start; bar.conf can turn it off at start.
- Network: interface name and up/down from bsdsocket QueryInterfaceTags, "no network" without a stack, redraw only on
  change; no traffic numbers.
- Screens: each public screen as a short name (first word, max 8 characters), front screen in the accent pen, click =
  ScreenToFront; cut from the right with a "+N" marker.
- Clock: "Mon 14:05" (%a %H:%M); click flips to "Mon 29 Oct 2026" until the next click; bar.conf takes the strftime
  subset %H %M %a %d %b %Y.
- RAM: "C 1.2M F 61M", free chip and fast memory, every 2 s.

## Owner decisions (none taken silently)

Taken earlier (roadmap, 2026-10-26): bar at the bottom of the Workbench screen; Amarchy verbs keep clear of it via
bar_usable(); CPU load from a -128 idle counter task that stops when the bar is off.

Implementation defaults chosen in this plan (not product behaviour; change by editing this file):
- The tick lives in its own module and runs with the bar off, because notices need expiry.
- One window helper for bar and notices (single source), with click input.
- The system icon opens the same menu node as super+esc.
- Bar counters beyond `ticks=` go to a new `BAR STATUS` command, because STATUS is near its 160-character cap.

## Order

1. R2.3a, R2.3b (tick) and R2.5a (helper extract, no behaviour change), R2.9 (bar_usable). Land alone; P4 unblocks.
2. R2.1, R2.2 probes on rig 4 (one agent, can run beside step 1).
3. R2.4a-c bar_core with host tests.
4. R2.6, R2.7, R2.11, R2.12a, then R2.8 and R2.10 (probe results).
5. R2.13a-c, R2.14, R2.5b, R2.12.
6. R2.16 rig test with revert proofs, R2.15 docs, R2.17 A1200.
