---
date: 2026-10-29
author: spot (Amarchy planning agent)
topic: AMARCHY-P3
status: draft
ledger: thoughts/spot/todos.md (add row AMARCHY-P3 when approved; grep it first for snap/float/swap rows)
roadmap: thoughts/shared/plans/2026-10-26-amarchy-roadmap.md, Phase 3 (R3.1-R3.12), decisions 1, 6, 11
builds on: thoughts/shared/plans/2026-10-28-amarchy-autotile.md (done 21 of 21), thoughts/shared/plans/2026-10-28-amtile-layout.md (done 43 of 43, handoff 2734c421)
depends: Phase 2 bar window, unmerged branch worktree-agent-a2768f656aa0a7b7f (b3474a77); P1b P1B-D1 desk_dopus_mode (2026-10-29-amarchy-p1b-third-party-theme.md, OD-B2)
read at: origin/main 3ec9b09e
---

# Amarchy Phase 3: manual tiling and snapping (AMARCHY-P3)

## Goal

The owner snaps the active window to a half, a quarter or the centre of the screen with one hotkey, moves focus by
direction with a hotkey, floats or un-floats a window out of AmTile's tiling, swaps a tiled window with its neighbour, and
every Amarchy geometry verb and every AmTile layout keeps clear of the Phase 2 bar (decision 1). Automatic re-tiling (P3b)
is NOT re-planned: AmTile already does it; this plan extends AmTile and win_core, it does not add a second layout core.

Finish line: every checklist row ticked, host suites green, one rig run where the real hotkey super+alt+shift+left (D1)
moves a UP-Term window to the left half of the usable area (bar excluded) and STATUS `snaps=` rises by exactly 1.

## What is already done elsewhere (roadmap Phase 3 rows)

| Roadmap row | State | Where |
|---|---|---|
| R3.1 focus-direction forms | done: `win:focus left/right/up/down/next/prev` parse (win_core.c:43 d6), run (win.c:246, win_pick win_core.c:218), menu rows only (dist/keymap:197-200), NO hotkey | this plan binds it (P3-D1) |
| R3.2 tile and master forms | superseded: AmTile `TOGGLE`, `RETILE`, `PROMOTE`, `LAYOUT` (tile.c:382 command); only the snap forms remain | P3-A3 |
| R3.3 halves and quarters | not built | P3-A3 |
| R3.4 master-stack, min/max aware | done, plus six more layouts: `tile_layout` tile_core.h:53, every cell through win_fit | reuse |
| R3.5 enumerate and apply | done in AmTile (walk tile.c:247, front screen per autotile D4) | reuse |
| R3.6 skip list | partly: tile_classify (tile_core.c:11) floats no-size-gadget, backdrop, borderless, title "Amarchy"; user opt-out file NOT built | P3-C4 (owner D7) |
| R3.7 keymap bindings | done for tiling (dist/keymap:77-92); snap/focus/float/swap not bound | P3-D1 |
| R3.7b / R3.9 auto re-tile | done by AmTile (250 ms poll, set fingerprint); NOT built from the roadmap text: per-screen toggle (owner replaced it with front-screen, autotile D4) and the refusal guard `tilefloat=` | owner D9 |
| R3.8 menu rows | done for tile + 8 layout rows (dist/keymap:203-211) | P3-D2 adds snap/float/swap rows |
| R3.10 man page | AmTile text unverified (not grepped in man/amarchy.1) | P3-D4 checks |
| R3.11 / R3.12 host and rig | done for AmTile (test_tile, amarchy_tile_rig.py 29/29) | P3-E, P3-F |

## Facts found (origin/main 3ec9b09e) and what to reuse

- Verb and arg enums: amarchy/win_core.h:10-13 (`win_verb`), :15-18 (`win_arg`); parser `win_parse` win_core.c:30 with
  closed word tables :33-49. Snap is one new verb plus five new args; no new action kind (KA_WIN covers it).
- A SECOND closed list of win: forms lives in config/keymap.c:246 `win_arg_ok` (bare, dir4, dir6, cyc, scr, ttl). Today the
  two lists must be edited in step by hand; no test asserts they agree (grep tests/ for win_arg_ok: none). P3-A4 removes the
  duplicate.
- Geometry: `win_fit` win_core.c:126, `win_move` :106 (clamped inside the whole screen), `win_fullscreen` :166 (area below
  `s->title`, bar ignored), `win_fullwidth` :156 (left and width only). `win_screen` (win_core.h) has width, height, title;
  no bottom reserve.
- `win_box_of` amarchy/win_apply.c:8 fills `win_screen` from the window's screen (title = BarHeight + 1). It is the one
  place both win.c and AmTile (tile.c:247 walk) read a screen, so the bar reserve goes there once.
- Work-area arithmetic is copied five times in amarchy/tile_core.c (:66-67, :91-92, :100-101, :109-110, :128:
  `x0 = gap, y0 = s->title + gap, w = s->width - 2*gap, h = s->height - s->title - 2*gap`). P3-A1 extracts it.
- Amarchy-owned window test copied twice: win.c:39 `is_ours` (Title == MENU_TITLE) and tile_core.c:11 (title "Amarchy").
- win_exec win.c:196: target = active window or the K5a menu ticket; geometry cases win.c:226-232 call win_core then
  `win_apply_box` (win_apply.c:24); `g_wins++` on success; STATUS keys amarchy.c:143-160 (5 keys, `wins=` last).
- AmTile: commands in tile.c:382 `command()`, `active_id` tile.c:299, `window_alive` tile.c:229, PROMOTE tile.c:462,
  STATUS tile.c:472 = `<on|off> <norder> <retiles> <layout> <nmaster> <ratio>`, pure order helpers `tile_promote`
  tile_core.h:56, `tile_cycle` :59, `tile_order_update` :33. Neighbour by direction already exists: `win_pick`.
- ARexx client for Amarchy -> AmTile: `rexx_send` in amarchy/rexx_host.c (extracted by autotile P2-1).
- Bar (branch worktree-agent-a2768f656aa0a7b7f, amarchy/bar.c:121-135): opens on the Workbench public screen only,
  borderless, untitled, `WA_Top = scr->Height - h`, `h = font YSize + 2`, full width. `bar_usable` is NOT built (F2 research
  doc line 35). It floats in AmTile already (borderless rule) but nothing subtracts its height.
- Free key chords (dist/keymap:43-133 read, spec_parse config/keymap.c:575-615: super required, shift/alt allowed, ctrl
  refused, no keypad names in knames :31-47): every `super+alt+shift+<key>` except tab; super+up, super+down;
  super+alt+8/9/0. super+left/right are `tab:prev/next` (dist/keymap:61-62), duplicates of super+comma/period (:59-60).
- Keymap ceilings near the limit: amtile-layout D-6 measured km.nbind 86 (ceiling 100) and heap 3365 (AMARCHY_KEYMAP_HEAP_MAX
  3400); KM_FILE_MAX 32768. This plan adds about 19 hotkeys and 15 menu rows: the ceilings move (P3-D3).
- Rig: tools/rig/amarchy_window_rig.py (win verbs, table of steps), tools/rig/amarchy_tile_rig.py (AmTile, hotkey()
  :275 with SUPER_ALT :245 through `ami.key`), host box helper tools/rig/amarchy_tile_boxes.c (compiles tile_core on the host).

## Implementation choices (mine, stated; owner may veto)

- Snap is a `win:` verb in Amarchy (one-shot, like move/size), not an AmTile command: it must work with AmTile off or not
  installed. Float and swap are AmTile commands: they only mean something to tiling.
- Bar reserve: the bar window gets the title `Amarchy bar` (borderless, no drag bar: the title is never drawn). A pure
  `win_strut(const win_box *bar, const win_screen *s)` returns the bottom reserve (screen height - bar top when the box is
  full width and touches the bottom edge, else 0). `win_box_of` walks the window's screen for that title (callers already
  hold LockIBase) and sets the new `win_screen.bottom`. Screens without the bar get 0, so every existing assertion stays
  byte-identical. The bar plan's R2.9 `bar_usable` is this function; if R2.9 lands first under that name, P3 reuses it.
- One Amarchy-window test `win_is_amarchy(title)` in win_core ("Amarchy" and "Amarchy bar"), used by is_ours and tile_classify.
- Window ids over ARexx are the window address in hex (`FLOAT ON $07A3B2C0`); AmTile checks membership with window_alive
  before use, so a stale id is rc 5, never a crash.
- Sentinels: Amarchy STATUS appends ` snaps=N` (6th key, after wins=); AmTile STATUS appends ` <floats> <swaps>` (fields 7, 8).

## Directory Opus 5 (Magellan II) desktop: both desktops supported

Owner rule (P1b plan, "Fixed already"): Workbench and the DOpus desktop are both supported. P3 must work in three setups:
(W) Workbench only, (DW) DOpus replaces Workbench (DOpus draws the desktop on the Workbench screen, `dopus screen` first
word `Workbench`), (DS) DOpus on its own public screen `DOPUS.n` (beside Workbench or replacing it).

Facts (with source) and gaps:
- Detection exists: `upactive_dopus_present` (FindPort "DOPUS.1") handler/upactive.c:132, `upactive_workbench_present`
  (FindPort "WORKBENCH", 3.2 only) :122; `rexx_send("DOPUS.1", ...)` pattern amarchy/leaf.c:443-464. The DW vs DS signal is
  P1b OD-B2 (recommended there: DOPUS.1 present and `dopus screen` first word `Workbench`), built as P1B-D1
  `desk_dopus_mode()` NONE / OWN_SCREEN / WB_DESKTOP. P3 REUSES that function; it does not add a second detector.
- Rig 7 `dopus screen` reply `Workbench 640 256 8 10 444 574` (thoughts/spot/term-findings/dopus-probe-2026-10-25.md:17).
  Rig 4 runs DOpus 5.82 as the Workbench replacement; a new lister there appeared as screen "Directory Opus", window
  title "Device List" (dopus integration plan D6 row, line 184). The owner's A1200 runs DOpus 5.91 as WB replacement.
- AmTile today (tile_classify tile_core.c:11) floats backdrop, borderless and no-size-gadget windows. So, UNVERIFIED:
  the DOpus desktop window is probably backdrop (floats already); listers have size gadgets (tiled today, no rule names them);
  DOpus button banks, the DOpus clock/memory bar and DOpus requesters vary. No probe has listed their flags.
- In DS, AmTile tiles the front screen (autotile D4), so it tiles the DOpus screen when that is in front. The bar opens on
  the Workbench public screen only (bar.c:121), so DS has no bar reserve (bottom 0) unless the bar follows DOpus (a Phase 2
  question, not P3).
- In DW the Workbench screen still exists and the bar sits on it; the DOpus desktop icons may sit under the bar (P1b/P2
  concern; P3 only keeps its own windows clear).
- Window-title matching for listers is unreliable: a lister title is its path or "Device List". Identification needs the
  window's task or a DOpus ARexx query (`lister query all` gives handles, leaf.c:445; whether a handle maps to a Window
  address is UNVERIFIED).

Probe rows (rig, DOpus kit 5.104 and rig 4's 5.82; text only, amiga_windows flags, no screenshots):
- [ ] P3-R1 For DW and DS: list every DOpus window (title, flags BACKDROP / BORDERLESS / SIZEGADGET, min/max, screen,
  owning task name). Desktop window, one lister, a button bank, a requester. Result decides which tile_classify rule already
  covers each, written into this plan.
- [ ] P3-R2 Is the owning task name of a lister stable (`dopus_lister` or similar, per version 5.82 / 5.91 / 5.104)? Can
  `lister query <h> window` or similar give a Window address? Result picks the lister identification for P3-G2.
- [ ] P3-R3 Does a lister accept ChangeWindowBox from outside and keep it (or does DOpus re-lay it out / snap it back)?
  Snap a lister with `WIN snap left` and read its box after 1 s and after a `lister refresh`.
- [ ] P3-R4 In DS: does `win_box_of` see the DOpus screen's BarHeight correctly (screen title bar shown or hidden)?

Build rows (after R1-R4):
- [ ] P3-G1 `win_screen` / tile walk know the desktop mode via `desk_dopus_mode()` (P1B-D1). If P1b is not merged when P3
  starts, P3 builds P1B-D1 itself in the P1b plan's shape (same name, same file, same host test) and P1b reuses it.
- [ ] P3-G2 DOpus window rule in tile_classify per owner D12 (desktop always floats; listers per decision), identified by
  the R2 signal. Host test with box fixtures from R1.
- [ ] P3-G3 Snap of a lister per owner D13 (works like any window if R3 says it keeps the box, else refused with a beep and
  documented). Host test only if a rule is added; rig step `snap_lister`.
- [ ] P3-G4 Rig steps `dopus_dw` and `dopus_ds` in amarchy_tile_rig.py: AmTile on, two UP-Term windows plus one lister;
  STATUS norder counts exactly what D12 says (2 or 3); desktop window box unchanged; snap of a UP-Term window in DW
  leaves the bar clear, in DS uses bottom 0.

## Checklist (IDs P3-xx; red first, Gate 2; [BAR] waits for the bar branch merge)

A. Pure core (host-testable)
- [ ] P3-A1 `win_screen.bottom` (default 0) and `win_work_area(s, gap, &x0, &y0, &w, &h)` in win_core; the five tile_core copies
  and win_fullscreen use it. Test: every existing test_win and test_tile assertion unchanged with bottom 0; new cases bottom 16
  (800x600 title 14 -> fullscreen height 570, dwindle work area h 562 at gap 4).
- [ ] P3-A2 `win_strut` and `win_is_amarchy`; is_ours (win.c:39) and tile_classify (tile_core.c:11) call it. Test: strut 0 for a
  non-full-width box, a box not at the bottom; 16 for the bar box; "Amarchy bar" floats in tile_classify.
- [ ] P3-A3 `WV_SNAP` with args left, right, up, down (halves), tl, tr, bl, br (quarters), center; `win_snap(b, arg, gap, mode, s)`
  on the work area. Halves split at (w - gap) / 2, the remainder pixel to the right/bottom cell (same rule as tile_layout).
  Center per owner D4. Every box through win_fit; a fixed-size window per owner D6. win_parse accepts the nine forms,
  refuses `snap`, `snap middle`, `snap left 5`. Test: worked numbers below for all nine at gap 0 and 4, bottom 0 and 16;
  odd screen 801x601; min/max clamp; no-op returns 0.
- [ ] P3-A4 config/keymap.c `win_arg_ok` delegates to `win_parse` (one closed list); handler/commodity link win_core.o (check
  Makefile targets that link config/keymap.c; the cross build size of the handler before and after goes in the ledger).
  Test: test_keymap accepts every existing win: form and the nine snap forms, refuses the same bad forms as before.
- [ ] P3-A5 tile_core pure helpers: `tile_swap(order, n, a, b)` (swap two ids, 0 when either is absent) and a float set
  (`tile_float_set/clear/has`, pruned with the order on close, TILE_MAX entries). Test: swap both ways, absent id, float
  survives a retile, pruned id gone.

B. Amarchy commodity
- [ ] P3-B1 [BAR] bar window title `Amarchy bar` (branch bar.c:133 WA_Title); `win_box_of` fills `bottom` via win_strut under
  the caller's lock. Test: rig step `bar_strut` (P3-F1) reads fullscreen height = screen - title - bar height.
- [ ] P3-B2 win.c `WV_SNAP` case beside win.c:231: win_snap then win_apply_box; `g_snaps++`; STATUS ` snaps=` (amarchy.c:147
  key table to 6). Test: host status_text test if one exists, else rig `snap_status`.
- [ ] P3-B3 snap gap source per owner D3 (recommended: `tile-gap` read with `amconf_tile_gap`, the existing reader; read at each
  snap, the file is small). Test: rig step with tile-gap 0 vs 8 gives different boxes.
- [ ] P3-B4 snap of a window AmTile tiles, per owner D5 (recommended: before applying, Amarchy sends `FLOAT ON <id>` over
  rexx_send when port AMTILE exists; no port, no message). Test: rig `snap_floats` (AmTile on, 3 windows, snap one: AmTile
  floats +1, the other two re-tile, the snapped box stays after 1 s).

C. AmTile
- [ ] P3-C1 work area honours `bottom` (comes free through A1 + B1). Test: rig `tile_bar` boxes equal helper boxes with bottom.
- [ ] P3-C2 `FLOAT ON|OFF|TOGGLE [<id>]` (no id = active window via active_id): floated windows are skipped by the walk's
  classify; tick(1). rc 5 when the window is not on the tiled screen. STATUS field 7 floats. Test: rig `float_toggle`.
- [ ] P3-C3 `SWAP LEFT|RIGHT|UP|DOWN`: neighbour of the active tiled window by `win_pick` over the tiled boxes, `tile_swap`,
  tick(1); rc 5 when none. STATUS field 8 swaps. Test: rig `swap_right` in master-stack (master and stack top exchange boxes).
- [ ] P3-C4 opt-out list per owner D7 (recommended: ENV:Amarchy/tile.conf, one AmigaDOS pattern per line, matched
  case-insensitively on the window title with ParsePatternNoCase/MatchPatternNoCase; read at start and RETILE; pure line
  splitter in tile_core). Test: host splitter (comments `;`, blank lines, 64 lines max, long line cut); rig `optout`.
- [ ] P3-C5 cross build AmTile and Amarchy rc 0; stack cookie test still green (memory: every Amiga program keeps __stack).

D. dist and docs
- [ ] P3-D1 hotkeys per owner D1/D2 (recommended set below), format `key = action ; label | window`.
- [ ] P3-D2 menu rows (flat `window.<id>`, as window.tile): snapleft, snapright, snaptop, snapbottom, snaptopleft,
  snaptopright, snapbottomleft, snapbottomright, snapcenter, float, swapleft, swapright, swapup, swapdown; dist/menu-icons
  line each (glyph checked in the icon table, fallback U+F009). Labels in full words ("Snap to left half", "Float or tile window").
- [ ] P3-D3 test_keymap rows, kinds, clash-free; measured nbind, heap and file size written in the ledger, ceilings raised
  only to the measured value plus the margin the earlier rows used.
- [ ] P3-D4 man/amarchy.1, dist/README.txt AMARCHY section, AmTile usage line (tile.c:6), tests/test_amarchy_docs.py.

E. Host tests (`make test ONLY=win|tile|keymap|amconf`, nice 19, max 2 workers, log to scratchpad)
- [ ] P3-E1 test_win: A1, A2, A3 cases and worked numbers. P3-E2 test_tile: A1 bottom, A5, C4 splitter.
- [ ] P3-E3 test_keymap: A4 agreement, D1-D3. P3-E4 knob test: each knob below moves the host output.

F. Rig (one script per existing area; reuse, no new rig framework)
- [ ] P3-F1 extend tools/rig/amarchy_window_rig.py with snap steps; extend tools/rig/amarchy_tile_rig.py with float, swap,
  optout, tile_bar; box helper tools/rig/amarchy_tile_boxes.c gains a `snap <zone>` mode and a bottom argument (same C code).
- [ ] P3-F2 rig run (060, `UPTERM_RIG=<n> rig.py start --max --purpose "Amarchy P3: snap and float check"`), every step OK,
  evidence doc thoughts/shared/research/2026-10-xx-amarchy-p3-rig.md, rig stopped (no write running).

Count: 24 items here plus 8 DOpus rows above (P3-R1..R4, P3-G1..G4) = 32.

### Worked numbers (host and rig assert these; 800x600, title 14, bottom 0)

gap 0: work area 0,14,800,586. left 0,14,400,586 | right 400,14,400,586 | up 0,14,800,293 | down 0,307,800,293 |
tl 0,14,400,293 | br 400,307,400,293. gap 4: work area 4,18,792,578; left 4,18,394,578 | right 402,18,394,578 |
up 4,18,792,287 | down 4,309,792,291 (remainder to the bottom cell: (578-4)/2 = 287, 578-4-287 = 287; check by the
implementer, numbers in the test are computed from the rule, then copied here). With the bar (bottom 16): work area height
drops by 16 in every case. Center (D4 a): a 300x200 window -> left 250, top 14 + (586-200)/2 = 207.

## Knobs and the case that proves each moves output

| Knob | Case |
|---|---|
| snap zone (9 args) | host: nine distinct boxes; rig `snap_left` and `snap_br` |
| gap (tile-gap, D3) | host gap 0 vs 4; rig `snap_gap` 0 vs 8 boxes differ |
| bar reserve (bottom) | host bottom 0 vs 16; rig `bar_strut` fullscreen height, `tile_bar` |
| center mode (D4) | host keep-size box vs fixed-fraction box (only the chosen mode ships; the other is not a knob) |
| float on/off | rig `float_toggle`: floated window keeps its box through a RETILE, un-floated rejoins |
| swap direction | rig `swap_right` vs `swap_left` from different start windows |
| opt-out pattern (D7) | rig `optout`: matching title floats, a non-matching one tiles |
| per-screen (D8) | rig `snap_ownscreen`: on a UP-Term own screen (no bar) bottom is 0 |
| desktop mode W / DW / DS | rig `dopus_dw` vs `dopus_ds`: bottom = bar height vs 0; tiled set differs per D12 |
| DOpus lister rule (D12) | rig `dopus_dw`: norder 3 (listers tiled) vs 2 (listers float) |

## The rig reachability test

ONE wiring step through the real entry point: `snap_hotkey` in amarchy_window_rig.py.
1. Baseline: AMARCHY STATUS read, `snaps=0` (proves the instrument reads 0 when nothing ran); bar open (branch merged).
2. A UP-Term window active on Workbench; press super+alt+shift+left through `ami.key` (rawkey 0x4F, qualifier
   LCOMMAND|LALT|LSHIFT 0x51), the same keyboard path amarchy_tile_rig.py:275 uses.
3. PASS = STATUS `snaps=1` AND `wins=` +1 AND the window box from amiga_windows equals the host helper's `snap left` box for
   the rig screen with bottom = bar height. FAIL looks like snaps unchanged (key not bound or verb not run), or the box of
   the full screen height (bar ignored).
Every other behaviour is tested where it lives: snap zones via `WIN snap <zone>` on port AMARCHY, float/swap/optout via
`AMTILE <cmd>` (rig.py send), each with STATUS + box compare. Stops its rig at the end.

## Dependencies

- Phase 2 bar branch merged (P3-B1, C1, F-steps marked [BAR]); A, C2-C4, D can be built before it with bottom 0.
- AmTile layouts (done) and rexx_host (done). tile-gap amconf key (done, autotile P3-3).
- Ordering inside P3: A1 -> A2 -> A3 -> A4 -> A5 -> B -> C -> D -> E alongside each row -> F. One sonnet agent per letter group.

## Risks

- Keymap ceilings (nbind 100, heap 3400) are already at 86 and 3365: about 19 more hotkeys break them; D3 measures and raises.
- A4 links win_core.o into whatever links config/keymap.c (the handler too): a few hundred bytes; measured before/after.
- Snap and AmTile racing on one window: without B4 AmTile re-takes a snapped window at its next retile event; with B4 the
  float arrives before ChangeWindowBox is queued, so the next poll already skips it.
- The bar title is a new contract between bar.c and win_box_of; the bar rig script and tile_classify must not depend on
  "untitled". win_box_of walks one window list per call: under the existing lock, at most WIN_MAXW windows, negligible.
- `ZipWindow` (super+z) uses the application's zoom box; it cannot be held clear of the bar (Intuition owns it). Documented.
- win_move still clamps to the whole screen, so super+shift+down can push a window under the bar; changing it is part of D8.

## Owner decisions (none taken silently; recommended first)

Owner answers 2026-10-29 (AskUserQuestion, s21): every decision D1-D14 took the recommended option (a). D1 arrow family snap keys; D2 focus on super+arrows (super+left/right stop switching UP-Term tabs, super+comma/period stay), swap super+alt+shift+h/j/k/l, float super+alt+shift+t; D3 gap = tile-gap; D4 centre keeps the window size; D5 a snapped tiled window floats; D6 fit within Min/Max; D7 build tile.conf; D8 win_move also stops at the bar; D9 refusal guard on, front-screen kept; D10 no cycling; D11 DOpus detection by `dopus screen` reply (same as P1b OD-B2); D12 listers tile, other DOpus windows float; D13 lister snap allowed when probe R3 shows it keeps its box, else beep; D14 bar reserve 0 on the DOpus screen until Phase 2 decides. Status: decided.

- D1 Snap keys.
  (a) halves super+alt+shift+arrows, quarters super+alt+shift+1/2/3/4 (tl, tr, bl, br), center super+alt+shift+c
  (Recommended): one arrow family (shift move, alt size, alt+shift snap), all free today; trade-off: four-key chords.
  (b) quarters on super+alt+shift+u/i/j/k (a 2x2 key square): spatial; trade-off: clashes with swap in (D2 a).
  (c) menu rows only for quarters and center: fewer keys and keymap ceiling room; trade-off: slower.
- D2 Focus by direction and swap keys.
  (a) focus on super+left/right/up/down (Omarchy's super+arrows), tab prev/next keeps super+comma/period; swap on
  super+alt+shift+h/j/k/l; float toggle super+alt+shift+t (Recommended): Omarchy parity for focus; trade-off: super+left/right
  stop switching tabs inside UP-Term (the comma/period pair remains).
  (b) focus on super+alt+shift+h/j/k/l, swap menu-only, super+left/right unchanged: no habit broken; trade-off: no Omarchy keys.
  (c) leave focus menu-only (today): nothing changes; trade-off: the roadmap's focus-by-direction stays unreachable by key.
- D3 Gap around snapped windows.
  (a) the AmTile `tile-gap` (default 4) (Recommended): snapped and tiled windows line up; trade-off: 4 px less space.
  (b) 0, edge to edge: maximum space; trade-off: a snapped window and a tiled one do not line up.
- D4 Snap center size.
  (a) keep the window's size, centred in the usable area (Recommended): no surprise resize; trade-off: a huge window stays huge.
  (b) 60 % x 70 % of the usable area: one predictable "focus" box; trade-off: resizes small dialogs bigger.
- D5 Snapping a window that AmTile tiles.
  (a) the window floats in AmTile, the rest re-tile (Recommended, Hyprland behaviour); trade-off: the owner un-floats with the
  float key to get it back into the layout.
  (b) refuse with a beep while the window is tiled: no hidden state; trade-off: snap is useless with tiling on.
  (c) apply, let AmTile take it back at its next retile: simplest; trade-off: the snap vanishes at the next window open.
- D6 Fixed-size or size-limited windows on snap.
  (a) sized as far as Min/Max allow, placed at the zone's top-left (centre for center) (Recommended): never overlaps outside the
  zone more than the window forces; trade-off: may leave a gap in the zone.
  (b) refuse with a beep when the window cannot take the zone's size: clear signal; trade-off: most requesters and some
  tools cannot be snapped at all.
- D7 User opt-out list (roadmap R3.6 rest).
  (a) build it: ENV:/ENVARC:Amarchy/tile.conf, AmigaDOS title patterns, read at start and RETILE (Recommended): drawer or tool
  windows the owner never wants tiled stay free; trade-off: one more file to document.
  (b) not now; FLOAT (P3-C2) per window is enough: less code; trade-off: floats are lost when the window closes.
- D8 Per-screen behaviour.
  (a) the bar reserve applies only on the screen the bar sits on (Workbench); snap, fullscreen and AmTile use the window's own
  screen; win_move also stops at the bar (Recommended): matches decision 1 everywhere; trade-off: super+shift+down no
  longer reaches the bottom pixel row of Workbench.
  (b) as (a) but win_move unchanged: smallest change; trade-off: one verb ignores the bar.
- D9 Roadmap P3b leftovers AmTile does not have: refusal guard (`tilefloat=`, auto-float a window that refuses its box twice)
  and per-screen auto toggle (replaced by autotile D4 front screen).
  (a) build the refusal guard into the C2 float set, keep front-screen (Recommended): reuses the float set, stops a fight with a
  self-resizing window; trade-off: a window can float without the owner asking (STATUS shows it).
  (b) neither: autotile D4 stands as the answer; trade-off: a self-resizing window re-tiles on every set change.
- D10 Repeated snap cycles sizes (left 1/2 -> 1/3 -> 2/3, as Windows/Rectangle do).
  (a) no, one size per key (Recommended): Omarchy has no cycling, simpler tests; trade-off: no third-width snaps.
  (b) yes: more sizes without keys; trade-off: state per window and a timing rule.

- D11 Detection signal for DW vs DS: take P1b OD-B2's answer (one decision for both plans). (a) Recommended there:
  DOPUS.1 present and `dopus screen` first word `Workbench`; trade-off: one ARexx round-trip per mode check, so P3 caches it
  at AmTile start and on RETILE, not per tick. (b) DOPUS.1 present and WORKBENCH absent; trade-off: wrong on 3.1.
- D12 DOpus windows in AmTile.
  (a) desktop window, button banks and requesters float; listers tile like any sizeable window (Recommended): listers are the
  DOpus file windows, the thing a tiler should arrange; trade-off: DOpus remembers lister positions and may fight (R3 measures).
  (b) every DOpus window floats: DOpus keeps its own layout; trade-off: a DOpus-heavy desktop gets no tiling at all.
  (c) listers tile only in DS (DOpus own screen), float in DW: Workbench desktop stays a mixed desk; trade-off: two behaviours.
- D13 Snap of a DOpus lister: (a) allowed like any window when R3 shows it keeps the box, else refused with a beep
  (Recommended); trade-off: depends on a probe. (b) always refused: no fight with DOpus; trade-off: no lister snaps.
- D14 Bar reserve in DS: (a) bottom 0 on the DOpus screen until Phase 2 decides whether the bar follows DOpus
  (Recommended); trade-off: none for P3, the gap is Phase 2's. (b) P3 reserves the bar height on the DOpus screen too;
  trade-off: empty strip with no bar in it.

## Order

R1-R4 probe (one rig session, DOpus kit) first, so G1-G4 and D12/D13 rest on measured flags. Then
A (pure, one agent) -> B and C (two agents, different files: win.c/amarchy.c vs tile.c) -> D -> F after the bar merge.
