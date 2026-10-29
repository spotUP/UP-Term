---
date: 2026-10-29
author: spot (Amarchy planning agent)
topic: AMARCHY-P4
status: decided
ledger: thoughts/spot/todos.md (no AMARCHY-P4 / notify / screenshot / power rows exist yet; add row AMARCHY-P4 when approved)
roadmap: thoughts/shared/plans/2026-10-26-amarchy-roadmap.md, Phase 4 (R4.1-R4.17), decisions 7, 8, 9
depends: Phase 2 (R2.3 timer tick, themed borderless-window helper, theme pens in the commodity), being built on a branch now
---

# Amarchy Phase 4: capture, notifications, power (AMARCHY-P4)

## Goal

Omarchy's capture, notification and system-menu features on Workbench, in the Amarchy commodity:

1. A hotkey captures the front screen, the active window or a dragged region to an IFF ILBM file and to the clipboard.
2. Any program or script posts a themed notification (`amnotify "Done" "build ok"` or ARexx to `AMARCHY.NOTIFY`); it shows at
   a screen corner, stacks, expires through the P2 timer tick, click dismisses.
3. A system menu (Omarchy Super+Escape) with "Safe to switch off" (flush every volume, then a notice) and Reboot.

Done (REACHABILITY_PROTOCOL): one rig run drives the real hotkeys and the real `amnotify` binary on rig (060, `rig.py start --max`),
and the commodity's STATUS counters plus the written file and clipboard prove each path ran, with the numbers below asserted.

## Facts found (origin/main 98998de6) and what to reuse

| Need | Reuse | Where |
|---|---|---|
| ARexx verbs, one dispatch | `rexx_cmd` STATUS/RELOAD/RUN/WIN/QUIT/SHOW | amarchy/amarchy.c:220-238 |
| STATUS sentinel string | `status_text` (5 keys; extend the key/val arrays) | amarchy/amarchy.c:143-161 |
| Hotkey -> action, one counter | `do_row` switch on `b->kind` (add `case KA_CAP`) | amarchy/amarchy.c:163-187 |
| Wait loop and signal masks | `Wait(cxs\|rxs\|nts\|lps\|wrs\|cws\|...)` | amarchy/amarchy.c:322-345 |
| Second public ARexx port | `rexx_host_open/sigmask/serve/close` (a second `struct rexx_host` for AMARCHY.NOTIFY) | amarchy/rexx_host.h:15-36 |
| Client side for amnotify | `rexx_send(portname, line, result, cap)` | amarchy/rexx_host.h:41, rexx_host.c:77-107 |
| Keymap kinds, arg checks | prefix table, `win_arg_ok` pattern, KS_COMMODITY scope | config/keymap.h:68-82, config/keymap.c:120-135, 217, 246, 320-332 |
| Key names | `help` 0x5F, `esc` 0x45 exist | config/keymap.c:43, 47 |
| Bindings and menu rows | `[bind]` (super+escape, super+help, super+ctrl+l all unbound today) and `[menu]` `system.*` rows | dist/keymap:39-141, 264-266 |
| Reboot row | `system.reboot = Reboot \| detach:Reboot` (already wired; R4.14 is a check, not new code) | dist/keymap:264 |
| Flush every volume | `Reboot DRYRUN`: ACTION_FLUSH to every volume with SendPkt, 3 s cap per handler, names non-answering volumes, rc 0/5, no reboot | install/reboot.c:1-27, 67-142 |
| Clipboard write | `clip_open` / `put` (CMD_WRITE) / CMD_UPDATE sequence, already linked into the commodity | handler/clip.c:14-75 (FTXT only today), Makefile:724 |
| IFF chunk helpers | FORM head writer pattern, `get32` reader | handler/clipfmt.c:83-123 |
| Clipboard change hook (counts our own write) | `cw_start`/`cw_changed` | amarchy/clipwatch.c:34-90 |
| Pixel read, one-row temp RastPort | `ReadPixelArray8` with `px.trp` temp rastport | amarchy/icon_gfx.c:167, 313-317 |
| Active window at hotkey time | `win_ticket_take` (LockIBase, ActiveWindow, before a menu steals focus) | amarchy/win.c:78-97 |
| Screen walk / front screen | LockIBase + FirstScreen | amarchy/win.c:143-176 |
| Borderless window tags | `window_open` tag list (WA_Borderless, IDCMP set) | amarchy/gui.c:1295-1309 |
| Theme roles | `MT_*` roles incl. MT_OK/MT_FAIL, MT_BORDER_ROLE, MT_HEAD_ROLE | amarchy/menu_theme.h:19-40 |
| Rig sentinel published as env var | `am_sentinel`, `am_sentinel_str` | amarchy/sentinel_dos.h |
| Rig harness | `amarchy_rig.arexx()`, `status()`; table-driven, resumable verdict files, `--only/--failed/--fresh`, RAW key table + LAMIGA qualifier | tools/rig/amarchy_rig.py:30-58, tools/rig/amarchy_menu_rig.py:1-72 |
| Man page | Amarchy man page | man/amarchy.1 |

Searched and absent (git grep origin/main, *.c *.h *.py): ILBM, BMHD, ByteRun, iffparse (only tests/test_clip.c, FTXT), png write,
timer.device, notification popup. The OSD plan (2026-10-23-amarchy-osd.md) is the XCON text menu window, not a native popup:
nothing to reuse for the notice window beyond its "no activate, closes on its own" behaviour notes.

Name clash to avoid: amarchy.c already has `notify_start/notify_stop/nr/nts` for DOS file notification (amarchy.c:190-212).
The new modules use the prefix `note_` (note_core.c, note.c) and the mask `nos`, never `notify_`.

Commodity link fact: AMARCHY_SRC (Makefile:724) has no menu_theme.c / render/pens_amiga.c; the drawn GUI has them
(Makefile:803). P2's bar window needs them in the commodity too; P4 takes them from P2, it does not add them itself.

## Interfaces assumed from Phase 2 (match names when P2 lands)

- T (R2.3): one timer.device request in the commodity Wait loop, a signal bit in the mask, a 1 Hz callback list or a
  `tick_now()` seconds counter readable by other modules; STATUS `ticks=`. P4 needs: a per-tick hook (`note_tick(now)`), and
  a way to ask for a faster tick while a notice is up (fade is not wanted; 1 Hz is enough for expiry, so no faster tick).
- B (R2.5 helper): open a borderless, never-activated, themed window at x/y/w/h on a given public screen, get theme pens
  (bg, text, accent/border, MT_OK/MT_FAIL), draw a text line, close. P4 needs one extra: IDCMP_MOUSEBUTTONS on the window
  (click to dismiss) delivered on a port the commodity Waits on. If the helper has no input hook, P4 adds it to the helper
  (single source), not a second window helper.
- U (R2.9): `bar_usable(screen)` so notices sit inside the usable area and never on top of the bar.

## Checklist (IDs from the roadmap; splits get a letter)

Each item: what, where, the test that proves it. Red first (Gate 2). [P2] = waits for Phase 2 landing.

- [ ] R4.1 Probe on rig 060 (one session, text output only): (a) ReadPixelArray8 of a Workbench chipset screen row matches
  WritePixelArray8 input; (b) on an RTG screen deeper than 8 bit, ReadPixelArray8 result (expected wrong/unsupported) and
  cybergraphics.library `ReadPixelArray` RECTFMT_RGB presence; (c) png.datatype DTM_WRITE with a PNG subclass: rc and file;
  (d) clipboard.device accepts a 300 KB CMD_WRITE FORM ILBM; (e) `Reboot DRYRUN` rc and lines on the rig's volumes.
  Where: tests/amiga/shotprobe.c (probe tool, not shipped). Proof: probe log in thoughts/shared/research/2026-10-xx-p4-probe.md.
- [ ] R4.2 shot_core.c/.h (portable C89): chunky row -> planes, ByteRun1 packer, BMHD/CMAP/CAMG/BODY writer through a sink
  with `write(buf,n)` and `patch32(offset,value)` (FORM and BODY sizes patched at the end; rows streamed, no whole-image
  buffer); also deep (24-bit) ILBM if R4.1b says RTG truecolour is in scope. Host tests tests/test_shot_core.c: ByteRun1 edge
  runs (1, 2, 127, 128, 129 equal bytes; 128 literals; alternating), odd widths (row padding to 16 bits), depth 1..8, golden file
  tests/data/shot_4x3_d2.ilbm byte-equal, and round-trip through an independent decoder tools/ilbm_decode.py (python, no
  shared code) equal to the input pixels.
- [ ] R4.3 shot.c (Amiga): capture a screen rectangle: LockPubScreen/front screen via LockIBase (win.c:143 pattern),
  ReadPixelArray8 one row at a time into a temp RastPort (icon_gfx.c:167 pattern), ColorMap -> CMAP (GetRGB32), CAMG from
  the screen's ModeID, file sink with Seek patch. File name `<dir>/shot-YYYYMMDD-HHMMSS.iff` (dir: owner decision D1).
  Proof: rig row r4_screen: file exists, FORM/ILBM, BMHD w/h equal the screen, decodes with tools/ilbm_decode.py.
- [ ] R4.4a capture the active window: rectangle from `win_ticket_take` (win.c:78) so the window active before the hotkey
  is the one captured; reads the screen rectangle under the window (overlapping windows included, Omarchy-like).
  Proof: rig row r4_window: BMHD w/h equal the window's Width/Height from amiga_windows.
- [ ] R4.4b region: full-screen borderless, transparent (no backfill, no drawing but the frame) window on the front screen,
  rubber-band frame in COMPLEMENT draw mode (no pens needed, so no P2 dependency), Esc cancels, release captures (window
  closed first so the frame is not in the shot). Proof: rig row r4_region via amiga_drag from (40,40) to (240,140):
  BMHD 201x101 (inclusive); Esc row leaves no new file and `shots=` unchanged.
- [ ] R4.5 clipboard ILBM: export a FORM writer from handler/clip.c next to clip_write (same clip_open/put/CMD_UPDATE,
  `clip_write_form(const void *head, ...)` streaming from the finished file, length known from the file), no second
  clipboard opener. Proof: host test of the length bookkeeping; rig row r4_clip reads unit 0 (`clip_read` path or a
  CMD_READ probe) and asserts FORM/ILBM and the same BMHD as the file; clipwatch does not add an ILBM clip as a text entry
  (cw_changed ignores non-FTXT: assert clips count unchanged).
- [ ] R4.6 PNG: only if R4.1c finds a writer and the owner picks it (D1); else man page line "IFF ILBM only" and a written
  knob exemption. Proof: R4.1c log line quoted in the commit.
- [ ] R4.7 keymap kind `cap:` with closed arguments `screen | window | region` (+ ` clip` suffix if D3 picks per-key
  clipboard); `cap_arg_ok` beside `win_arg_ok` (config/keymap.c:246), KS_COMMODITY scope (keymap.c:120-135), prefix entry
  (keymap.c:217), `case KA_CAP` in do_row (amarchy.c:163). Bindings per D2, menu group `capture.*` rows, key viewer rows.
  Proof: `make test ONLY=keymap` cases: valid forms parse, `cap:foo` and `cap:` refused, viewer lists the rows.
- [ ] R4.8 note_core.c/.h (portable): bounded queue (max shown per D5), slot positions for a corner and a usable box, expiry
  by seconds, oldest dropped when full, dismiss by hit-test, `title|body|seconds` parse with escaping of `|`, length limits.
  Host tests tests/test_note_core.c: stacking y positions for 1..N, expiry at exactly t0+timeout, 0 = sticky until click,
  overflow drops oldest, hit-test, malformed lines refused with rc.
- [ ] R4.9a note.c port side (no P2): second `struct rexx_host` named AMARCHY.NOTIFY (rexx_host.h:26), mask in the Wait loop,
  verbs `NOTIFY <title>|<body>[|<seconds>]`, `CLEAR`, `COUNT`; queue in note_core; also `NOTES` verb on AMARCHY returning
  `notes=N shown=N last=<title>` (REXX_HOST_OUT is 96 bytes: STATUS gets only the counters). Proof: rig `rx` to the port,
  COUNT rises; port gone after QUIT.
- [ ] R4.9b [P2-B] popup window: P2 helper window at the D4 corner of the front public screen inside `bar_usable()`, theme
  pens (title in MT_HEAD_ROLE, body in text role, border MT_BORDER_ROLE), never activated, click dismisses; text width cut
  with ellipsis. Proof: rig row r4_note_window: window titled `Amarchy notice` in amiga_windows; ActiveWindow unchanged
  (focus not stolen: assert the UP-Term window still active).
- [ ] R4.9c [P2-T] expiry on the tick: `note_tick(now)` closes expired notices, re-stacks the rest. Proof: rig row
  r4_note_expire: post with 3 s, window present at +1 s, gone by +5 s, `ticks=` rose (the tick ran it).
- [ ] R4.10 amnotify CLI (amarchy/amnotify.c, Makefile target, kit install): ReadArgs `TITLE/A,BODY,SECONDS/N/K,BODYFILE/K`,
  sends via `rexx_send` (rexx_host.c:77); rc 0 shown, 5 commodity not running (message, not a hang), 10 bad args; `?` help
  (tty/qmark.h as in reboot.c). Unix and Amiga forms both (`amnotify -t 3 Done "build ok"`, memory: unix-and-amiga-forms).
  Proof: host test of its argument parse (portable part); rig row r4_amnotify: notes= +1.
- [ ] R4.11 [P2-B] capture posts a notice "Screenshot saved" + file name (or the error), clipboard noted. Proof: part of r4_screen:
  notes= +1 after the hotkey.
- [ ] R4.12 power: "Safe to switch off" reuses `Reboot DRYRUN` (install/reboot.c) instead of a new power.c (roadmap had power.c;
  D6 confirms). A script UP-Term:S/safe-off: `Reboot DRYRUN >T:safeoff.txt`, then `amnotify` with the result (rc 0:
  "Safe to switch off"; rc 5: "Not all disks answered" with BODYFILE T:safeoff.txt). Proof: rig row r4_safeoff: NOTES
  last= the expected title; host: dist-check that the script names only installed commands.
- [ ] R4.13 [P2-B] menu row `system.safeoff = Safe to switch off | detach:Execute UP-Term:S/safe-off` and the system menu key
  per D7 (`super+escape = menu:system`). Proof: rig row r4_power_menu: press the real key, RUN of the row via the menu,
  NOTES last= "Safe to switch off".
- [ ] R4.14 Reboot row: already `detach:Reboot` (dist/keymap:264). Check only: row present in the system menu, confirm per D8.
  Never pressed on the rig except one deliberate row (memory: never reset a rig mid-write: command done + 5 s quiet first).
- [ ] R4.15 STATUS adds ` shots=N notes=N` (amarchy.c:143-161), and the bar/tick keys P2 adds stay; also `am_sentinel("AMARCHY_SHOTS")`.
  Proof: rig r4_status: both keys present and 0 at start (known-dead baseline, protocol section 3), non-zero after r4_screen.
- [ ] R4.16 man page (man/amarchy.1: cap: kind, AMARCHY.NOTIFY verbs, amnotify, safe-off), installer copies amnotify +
  safe-off script, README key list (generated from dist/keymap), `make dist-check` rows.
- [ ] R4.17 rig reachability test tools/rig/amarchy_p4_rig.py (below) PASS on rig 060, verdict files committed in the ledger
  row with the commit hash.
- [ ] R4.18 (new, protocol section 2) knob table: every knob below moves output or carries a written exemption; host or rig case
  per knob.
- [ ] R4.19 (new, owner D9) cosmetic lock: super+ctrl+l opens a full-screen theme-coloured window on the front public screen
  with the clock and "Locked"; any key or click closes it. No password, no input handler, no claim of security (man page says so).
  Proof: rig row r4_lock: press the real key -> STATUS locks=1 and a window covering the screen in amiga_windows; one key -> gone.

Total 23 checklist rows (17 roadmap IDs; R4.4, R4.9 split; R4.18 added; R4.19 from owner D9).

### Knobs and the case that proves each moves output

| Knob | Case |
|---|---|
| shot dir (amconf key, D1) | r4_screen with the key set to RAM:ShotsB: file lands there |
| capture mode screen/window/region | r4_screen / r4_window / r4_region dimensions differ as asserted |
| clipboard on/off (D3) | r4_clip with off: clipboard FORM unchanged |
| notice timeout | note_core host expiry; r4_note_expire 3 s vs default |
| notice corner (D4) | note_core host positions; r4_note_window x/y from amiga_windows |
| notice max shown (D5) | note_core host overflow |

## Host tests (all `make test ONLY=<suite>`, nice 19, max 2 workers)

- shot_core: ByteRun1 runs, planar conversion, row padding, golden file, independent python decoder round-trip, deep ILBM if in scope.
- note_core: parse, stacking, expiry, overflow, hit-test.
- keymap: `cap:` parse and refusals, viewer rows, new bindings clash-free (existing keymap suite).
- amnotify argument parse (portable part split into amnotify_core if needed).
- clip length bookkeeping for clip_write_form.

## The rig reachability test (one script, rows share state)

tools/rig/amarchy_p4_rig.py, pattern of tools/rig/amarchy_menu_rig.py (table driven, verdict per row in <RIG>/amarchy/p4_<row>.verdict,
`--only/--failed/--fresh`), on `UPTERM_RIG=<n> rig.py start --max --purpose "Amarchy P4: capture/notify/power check"`.
Top-level entry points only: the real hotkey through the input stream (ami key press with LAMIGA qualifier) and the real
`amnotify` binary in the rig shell; ARexx only to read sentinels.

1. r4_status: STATUS has `shots=0 notes=0` (baseline; proves the instrument reads a dead value as 0).
2. r4_screen: press the D2 screen key -> STATUS shots=1 (do_row ran KA_CAP), new file in the shot dir, FORM/ILBM, BMHD equal the
   Workbench size, decodes; notes=1 (R4.11).
3. r4_clip: clipboard unit 0 holds FORM ILBM with the same BMHD.
4. r4_window, r4_region (amiga_drag), r4_region_esc (no file, shots unchanged).
5. r4_amnotify: `amnotify -t 3 P4 hello` rc 0 -> notes +1, window `Amarchy notice` exists, UP-Term still active; at +5 s gone, ticks rose.
6. r4_power_menu: real system-menu key, pick Safe to switch off -> NOTES last=Safe to switch off (Reboot DRYRUN rc 0 on rig volumes).
7. r4_quit: QUIT -> AMARCHY.NOTIFY port gone; `amnotify` now rc 5 with a message, no hang.
Never presses Reboot. Stops its rig at the end.

## Dependencies on Phase 2

- Can start now (no P2): R4.1, R4.2, R4.3, R4.4a, R4.4b, R4.5, R4.6, R4.7, R4.8, R4.9a, R4.10, R4.12 (script part), R4.14, R4.15, R4.16 (partial).
- Wait for P2 helper (B) and theme pens in the commodity: R4.9b, R4.11, R4.13 (final notice visible), R4.17 rows 2 (notes part), 5, 6.
- Wait for P2 tick (T, R2.3): R4.9c, R4.17 row 5 expiry.
- Wait for `bar_usable()` (U, R2.9): notice placement in R4.9b (until then the full screen box; switch is one call).
- Without a window, R4.9a still counts notices, so amnotify and safe-off are testable through NOTES before P2 lands.
- Merge risk: P2 edits the same lines (status_text arrays amarchy.c:143-161, Wait mask amarchy.c:332, Makefile:724). P4 rebases
  on P2 before R4.9b; until then keep P4 edits to those lines minimal and separate commits.

## Risks

- RTG truecolour screens: ReadPixelArray8 is chunky-8 only; a 16/24-bit Picasso96 Workbench needs cybergraphics ReadPixelArray and
  deep ILBM (R4.1b decides; else "8-bit screens only" message and notice, not a garbage file).
- Memory: a 1024x768x8 BODY is ~800 KB uncompressed; rows are streamed, file written first, clipboard fed from the file; T: or
  RAM: space for clipboard data on small machines (clipboard.device keeps its own copy in CLIPS:, usually RAM:).
- Region overlay on a non-public or custom screen: only public screens can host a pubscreen window; capture of a custom screen
  (game, demo) is screen-mode only via the screen's RastPort (unverified, R4.1).
- Notice stealing focus: WA_Activate FALSE, no IDCMP that activates; checked in r4_note_window.
- Notices while the menu is open: the menu is another process (amarchy-gui), the commodity loop stays free; z-order: the notice opens
  behind? WindowToFront on post (unverified which wins on Workbench).
- ACTION_FLUSH: honoured by FFS/SFS/PFS (U for others); Reboot DRYRUN's 3 s cap per handler bounds the wait; message says best effort.
- Hotkey clashes: Help is used by applications (AmigaGuide, MultiView); bare Help would steal it (D2).
- Port name AMARCHY.NOTIFY taken by an old instance: rexx_host_open returns 0, commodity runs without notices and says so once.
- P2 interface drift: helper and tick names unknown until P2 lands; plan names behaviour, not symbols.

## Owner answers (2026-10-29)

- D1: (b) RAM:Shots only. No disk writes; shots are lost on reboot. The shot-dir amconf key stays as the knob (default RAM:Shots),
  R4.6 PNG is dropped (IFF ILBM only, man page line).
- D2: (a) super+help region, super+shift+help screen, super+alt+help window.
- D3: (a) every capture goes to file and clipboard unit 0.
- D4: (a) top-right inside the usable area, accent border.
- D5: (a) 5 s default, per-notice SECONDS, 0 = until clicked, max 3 shown, oldest dropped.
- D6: (a) reuse `Reboot DRYRUN` + `Wait 2` + notice; no power.c. The roadmap's power.c line is superseded.
- D7: (a) super+escape opens menu:system (Safe to switch off, Reboot, Workbench screen, Own screen); no suspend or shutdown.
- D8: (a) a confirm row before Reboot (RJ.3 if landed; if not, R4.14 waits for it).
- D9: (b) cosmetic lock now, any key unlocks, no password (the owner chose it knowing it protects nothing). New row R4.19.
  A password variant was not asked; do not add one without the owner.
- D10: (a) AMARCHY.NOTIFY port + amnotify CLI command; no NOTIFY ARexx verb.

## Owner decisions (none taken silently; recommended first)

- D1 Screenshot format and save location.
  (a) IFF ILBM to `SYS:Pictures/Screenshots` if present else `RAM:Shots`, amconf key `shot-dir` (Recommended): survives reboot on a
  normal install, IFF opens in every Amiga viewer; trade-off: not viewable on a Mac without a converter.
  (b) IFF ILBM to RAM:Shots only (roadmap default): no disk writes, lost on reboot.
  (c) PNG when R4.1 finds a png.datatype writer, else IFF: Mac-friendly; trade-off: two code paths and a datatype dependency that most
  OS 3.x installs lack.
  (d) The roadmap's ENVARC:Amarchy/Shots: rejected suggestion, ENVARC: is for prefs and is copied to ENV: (RAM) at every boot.
- D2 Capture keys (Amiga keyboards have no PrtScr; Omarchy uses PrtScr variants, exact mapping unverified).
  (a) super+help = region, super+shift+help = screen, super+alt+help = window (Recommended): Help is where PrtScr sits on an Amiga
  keyboard and Left Amiga keeps it from applications; trade-off: three-key chords for two of them.
  (b) bare help / shift+help / alt+help: one key; trade-off: steals Help from AmigaGuide and every app that uses it.
  (c) super+shift+3/4/5 (macOS style): familiar to Mac users; trade-off: not Omarchy, and super+shift+digits may be wanted for
  "move window to tab N" later.
- D3 Clipboard behaviour. (a) every capture goes to file and clipboard (Recommended): one key does both; trade-off: a large picture
  replaces the clipboard text each time. (b) separate `... clip` keys (Omarchy's Shift variant): clipboard text survives captures;
  trade-off: twice the bindings.
- D4 Notification look and position. (a) top-right of the front public screen, inside the usable area, theme bg + border in the
  accent, title line in the header role, body one or two lines, 30-character min width, 40% screen max (Recommended): matches mako
  defaults; trade-off: covers the Workbench screen title bar's right end (depth gadget) on a small screen. (b) bottom-right above the
  P2 bar: never over the title bar; trade-off: sits next to the bar's tray. (c) top-centre.
- D5 Timeout and stacking. (a) 5 s default, per-notice SECONDS, 0 = until clicked, max 3 shown, oldest dropped (Recommended):
  mako-like; trade-off: a burst of 4+ loses the oldest. (b) 10 s, max 5. (c) queue, show one at a time.
- D6 "Safe to switch off" mechanism. (a) reuse `Reboot DRYRUN` (flush every volume, 3 s cap per handler) + the notice (Recommended):
  the same flush the kit trusts before ColdReboot, no new C; trade-off: no extra "drive quiet" wait beyond Reboot's own 2 s,
  and Reboot DRYRUN does not do the settle wait (only the real reboot does), so the script adds `Wait 2`.
  (b) new power.c with the roadmap's 5 s quiet wait polling lock counts: stronger claim; trade-off: duplicate flush code and an
  invented quiet heuristic with no OS counter behind it (U).
- D7 System menu key and contents. (a) super+escape opens menu:system with Safe to switch off, Reboot, Workbench screen, Own screen
  (Recommended): Omarchy's key; trade-off: Esc closes menus, so super+escape inside an open menu must still close it (check).
  (b) no key, rows only in the root menu.
  Suspend / shut down: the Amiga has neither; (a) omit them (Recommended), (b) "Switch off" = safe-off + full-screen black window
  telling the user to switch off now; trade-off: (b) looks like a shut-down but cannot cut power.
- D8 Reboot confirmation. (a) a confirm row (RJ.3 input rows, if landed; unverified) before Reboot (Recommended): one wrong key
  in a menu no longer reboots; trade-off: depends on RJ.3. (b) as today, immediate (`detach:Reboot`).
- D9 Lock screen (Omarchy Super+Ctrl+L). (a) out of scope, documented (Recommended, roadmap's stance): no real security is
  possible (reset, Early Startup, or a CLI on another screen bypass it). (b) a cosmetic lock: full-screen window on every screen + input
  handler eating input until a password from ENVARC: is typed; trade-off: false sense of security, an input handler that can lock the
  owner out if it misbehaves. (c) later, together with the screensaver phase (decision 16), as "blank + lock".
- D10 Who may post notices. (a) any program via AMARCHY.NOTIFY and amnotify (Recommended, decision 8). (b) also a NOTIFY verb on
  AMARCHY: one port less; trade-off: two ways to do one thing.

## Order

R4.1 probe (one rig session) -> R4.2, R4.8 host cores (parallel-safe, host only) -> R4.3, R4.4a/b, R4.5, R4.7 capture end to end
-> R4.9a, R4.10, R4.12 port + CLI + safe-off -> [P2 lands] -> R4.9b, R4.9c, R4.11, R4.13 -> R4.14-R4.16 -> R4.17 rig run -> R4.18 knob check.
RX.3 (reminders) consumes R4.9a/b: it posts through note_core directly from the tick, not through the port.
