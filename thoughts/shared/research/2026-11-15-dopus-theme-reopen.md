---
date: 2026-11-15
topic: DOpus does not close/reopen on Patina theme pick (WB reset)
status: findings, fix not chosen
---

# DOpus close/reopen on theme pick

Symptom (S82 themepick agent, rig 7): with DOpus5 running, a theme pick raises the
IPrefs System Request "Intuition is attempting to reset the Workbench screen. Please
close all windows, except drawers" (Retry|Cancel). It goes through only after DOpus quits.
Owner: DOpus never redraws right after a theme change because it is not closed and reopened.

Why a reset at all: the pick rewrites palette.prefs (md5 5603c3438f -> df7b1ce4cb); font,
WBPattern and screenmode prefs are unchanged. DrawInfo pens are fixed at screen open, so
IPrefs must reset the Workbench screen to apply new pens. The reset is inherent.

DOpus already has close+reopen for this (BlitterStudio dopus5 source):
- Library/wb.c:1059 patches CloseWorkBench and sends DN_CLOSE_WORKBENCH (no wait),
  then calls the old CloseWorkBench at once. wb.c:1080 does the same for OpenWorkBench.
- Program/event_loop.c:614 on DN_CLOSE_WORKBENCH starts "dopus_closewb".
- Program/misc_proc.c:1114 runs check_closescreen(); Program/misc.c:441 walks the WB
  screen windows (4 tries). It refuses if any window is not a WB tool window, not a DOpus
  window, and not owned by the task named "<< IPrefs >>".
- event_loop.c:481 MAIN_CLOSEWB: close_display, set GUIF_CLOSED, CloseWorkBench again,
  start a timer. event_loop.c:629 on DN_OPEN_WORKBENCH reopens with display_open(0).

So screennotify.library is not needed: DOpus does not use it and it only notifies.

Hypothesis (unverified on rig): a Patina-owned window on the WB screen (theme picker,
bar, desk windows, console) makes check_closescreen refuse, so DOpus stays open, and the
same window also blocks the Intuition reset itself. Proof needed: list the WB screen
windows with owner task at the moment the requester shows.

Fix directions:
1. Patina side: before a pen-changing apply, Patina closes its own WB windows, lets
   DOpus run its built-in close, waits for the reset, reopens after OpenWorkBench.
   No DOpus change.
2. DOpus PR (owner allows): check_closescreen also accepts windows of named tasks
   (Patina), and/or the notify waits. Not enough alone: Patina windows still block
   Intuition's reset.
3. ARexx `dopus hide` / reveal around the apply: fallback if 1 does not trigger DOpus.

## Decision (owner 2026-11-15: "pick the best most correct solution")

Chosen: direction 1, Patina side. Reason: the reset needs every foreign WB window closed
anyway (Intuition rule), and DOpus already closes and reopens on CloseWorkBench/OpenWorkBench.
A DOpus PR cannot remove the Patina-window block. Order:
1. Rig probe: at the requester, list WB screen windows + owner task. Confirms the blockers.
2. Patina closes its own WB windows before a pen-changing apply, reopens after OpenWorkBench.
3. Only if DOpus still refuses with Patina windows closed: DOpus PR (check_closescreen) with owner OK.
Start: next session, after /clear.

## Rig probe (S82)

Rig 8 (68060, DOpus 5.104 as Workbench, Patina commodity running; build/amiga binaries of 2026-11-14, `--no-build`). Pick: `patina_theme_save_rig.py --selects 2-2`. Window list from ami `read_ui(every_screen=True)` (title, owner task from the window's UserPort task name), polled every 5 s while the requester was up (12 polls, identical set). Flags / WFLG_WBENCHWINDOW is NOT exposed by read_ui, so it is unknown (no raw memory peek, by order).

Screen "Directory Opus ... graphics ... other" (800x600), IPrefs "System Request" Retry|Cancel up:

| Window title | Owner task | WBENCHWINDOW |
|---|---|---|
| Patina bar | `Patina` | unknown |
| System Request | `<< IPrefs >>` | unknown (the requester itself) |
| Ram Disk, 1,018.5M free ... (lister) | `dopus_lister` | unknown |
| (no title, 84x332 button bank) | `dopus_buttons` | unknown |
| (no title, 800x573 backdrop) | `DOpus5:DirectoryOpus` | unknown |

No Patina theme picker, console or desk window is open; no stock Workbench window (DOpus replaces it). The only non-DOpus, non-IPrefs window is `Patina bar` (owner task `Patina`).

Also seen: the same requester appeared at boot 1 before any pick (palette apply on first boot), and a Cancel click re-raised it.

Verdict: DOpus did NOT close its lister, buttons or backdrop window. The only candidate blocker for check_closescreen (task name not `<< IPrefs >>`, not DOpus) is the Patina bar, which supports the hypothesis for exactly one window. Unproven: whether check_closescreen rejects it, because it also depends on the WBENCHWINDOW flag (the bar being flagged as a WB window would let it pass). Next step is still direction 1 (Patina closes the bar before the pen-changing apply); to test, close the bar and re-pick with DOpus running: if DOpus then closes itself, the bar was the blocker.

## Root cause (S82)

Code reading only (no rig run, budget). Proven / unproven marked per line.

Proven by code:
1. "Bar stayed open the whole time" does NOT mean Ctrl-F never fired. The gate reopens every hidden client after each
   FAILED CloseWorkBench: wbreset_patch.c:72-77 (wbr_close_end, then signal_all) -> wbreset_core.c:89-97 (failed:
   phase OPEN, hidden_list) -> wbreset_core.c:117-126 (DO_SHOW) -> patina.c:216 desk_wb_open. With DOpus windows still
   up every try fails, so the bar is down only for the gate's few-tick wait and back long before the next 5 s poll.
2. The bar is NOT what makes DOpus refuse. tw_open opens it with WA_WBenchWindow (themewin.c:104-106, since 7d6bdb09
   2026-11-13, so in the 11-14 rig build; s28 census Flags 0a010840 = flagged), and check_closescreen skips every
   WFLG_WBENCHWINDOW window (dopus Program/misc.c:457). Direction 1 (close the bar first) therefore cannot change DOpus.
3. Patch chain order is not the cause: DOpus's patch always calls the old vector (Library/wb.c:1059-1074), Patina's
   gate always calls the saved one (wbreset_patch.c:70). Either order reaches both hooks and the original.
4. The requester means CloseWorkBench returned FALSE; IPrefs' auto-retry only fires when no unflagged window is left
   (2026-10-31_wbreset-external-use.md, routine 0x58e / nonwb_free 0x48a). DOpus windows are unflagged, so the reset
   can only pass after DOpus closed itself. Cancel ends that attempt; a re-raise is a NEW reset trigger.
5. The theme pick with a DOpus lister/button bank open goes down IPREFS-FOREIGN-WINDOWS deferral: those windows are
   non-backdrop, non-WBENCHWINDOW -> desk_window_foreign (win_core.c:197-199) -> desk_dos.c:534-547 reopens the bar at
   once and writes the palette for the pen map IN FORCE, i.e. by design there should be no reset at all, and no
   dk_wait_reset before dopus_theme_send (desk_dos.c:569-570) fires synchronous ARexx at DOpus.

Unproven (needs the rig):
- Why a reset happens despite the deferral: either deferral did not trigger (check sentinels PATINA_DEFERRED,
  PATINA_FOREIGN), or the deferred palette still changes pens IPrefs compares (desk_pens_reset checks DK_SLOTS only).
- Why DOpus does not close: in the steady state (DOpus + IPrefs requester + flagged bar) check_closescreen should pass,
  so either DN_CLOSE_WORKBENCH is not acted on (GUIF_CLOSE_PENDING set / DOpus main busy with the dopus_theme_send
  ARexx at that moment, event_loop.c:614-626) or a transient unflagged non-DOpus window exists during its 1 s check.
- Boot requester and Cancel re-raise: likely the start apply (desk_dos.c:1505-1522, no deferral check) and a second
  trigger (Patina re-apply or a second IPrefs notify); not traced.

Rig check that splits it (one run, rig != 8/81/83/183): DOpus + lister open, pick, then while the requester is up
read the gate record with tests/amiga/wincensus (closes/failed/diag_wins/diag_vis/timeouts) and the PATINA_DEFERRED /
PATINA_FOREIGN sentinels; then press Retry once. Retry passes -> the first try had a transient blocker (DOpus check ran
too early / was busy). Retry also fails with failed++ and DOpus windows still there -> DOpus never acts on the notify.

## Deciding run (S82)

Rig 9 (060, DOpus 5.104 installed fresh by `rig.py dopus`, lister + button bank open, binaries of 2026-11-15 02:46-02:48). Script: scratchpad run.py (boot snapshot, Cancel, pick tokyo-night -> 2 down via the Patina menu, poll, Retry, 2 snapshots).
Gate counters (tests/amiga/wincensus): NOT COLLECTED. build/amiga/wincensus did not exist; its buildq job was position 6 behind other sessions' builds for the whole run. Still open.

Raw values:
- Boot: IPrefs "System Request" (Retry|Cancel) already up at boot; PATINA_DEFERRED, PATINA_FOREIGN, PATINA_WBCLOSES all empty (no apply yet). Cancel on it: the requester was still listed 5 s later (re-raised or never closed).
- After pick (requester up, 'Patina desktop theme' Keep window also up): PATINA_DEFERRED=1, PATINA_WBCLOSES=1,
  PATINA_FOREIGN='System Request/8031046;Ram Disk, 1,020.8M free, 591K in use, 0% full/2020203f;-/200a70;'
  (flags: IPrefs requester 0x8031046, DOpus lister 0x2020203f, untitled 0x200a70 = the DOpus button bank; none has WFLG_WBENCHWINDOW 0x02000000.)
- Retry clicked once. +20 s and +40 s: requester still up, DOpus lister, buttons, backdrop windows all still there (same titles/tasks), PATINA_* unchanged.

Verdict: deferral path WAS taken (PATINA_DEFERRED=1) yet the IPrefs reset requester is up anyway; Retry fails, DOpus windows unchanged -> DOpus ignores the close notice (not a transient first-try blocker). Caveat: failed++ in the gate record unverified (wincensus missing); and FOREIGN counts the System Request itself as a foreign window, so the deferral may have been triggered by the requester, not by DOpus alone.

## Cause found (S82, lead)

DOpus `check_closescreen` (Program/misc.c:474 in BlitterStudio dopus5) excuses the IPrefs requester window by comparing the window's task name with `"« IPrefs »"`. In the source the literal is the bytes `EF BF BD 20 "IPrefs" 20 EF BF BD`: UTF-8 replacement characters left by a lossy re-encode of the Latin-1 file. The real task name is Latin-1 `AB 20 "IPrefs" 20 BB` (both build/rig/os-31 and os-32 C/IPrefs contain it). The compare never matches.

Effect: IPrefs calls CloseWorkBench, which fails while DOpus windows are open, so IPrefs raises its "reset the Workbench screen" requester. DOpus gets the close notice, runs check_closescreen, sees the requester as a foreign window and refuses. Retry repeats the same loop, so the reset deadlocks. This explains the boot requester, the theme-pick requester and why Retry never helps. The Patina bar and patch order are not involved (see Root cause (S82)).

Evidence: the shipped binary build/dist/up-dopus/Files/os3/DOpus5/DirectoryOpus (from vendor/dopus5-5.104/DOpus5_104_os3.lha, md5-pinned) contains `EF BF BD " IPrefs"` once and `AB " IPrefs " BB` zero times; the os3-68000 binary is the same. The local reference clone is shallow (one commit, 96ede09), so the upstream commit that broke the bytes is not known here. The same corruption hits `FindTask("« sysihack »")` and several comment and doc strings.

Fix:
1. Upstream: write the literal as `"\xab IPrefs \xbb"` (and `"\xab sysihack \xbb"`), so the source no longer depends on file encoding. Send a PR to BlitterStudio/dopus5 (owner OK given).
2. Kit, until a release carries it: patch the 14-byte literal in place to `AB " IPrefs " BB 00` (10 bytes plus NUL, fits) at kit build, after the md5 check. This is unproven on the rig.

## Kit patch rig check (S82)

Patch: tools/dopus_iprefs_patch.py, called by Makefile dist-dopus after the md5-checked unpack, on Files/os3 and Files/os3-68000 DOpus5/DirectoryOpus. Test: tests/test_dopus_iprefs_patch.py (ONLY=dopus_iprefs_patch): on both real binaries one damaged literal before, `AB " IPrefs " BB` after, same size; idempotent; other counts give [ERROR]. The "sysihack" FindTask literal is damaged the same way and left alone.
Rig 10 (060, `rig.py dopus` install from the unpatched os3 archive, then the same tool run on the installed VTC:DOpus5/DirectoryOpus; md5 e91d6f13... -> 21e935e9322e4128f2e8fa9c1f4a2cc8, equal to the lha-unpacked + patched kit binary; the full kit build itself was not run).
- Boot: no IPrefs requester; PATINA_DEFERRED/FOREIGN/WBCLOSES empty; Cancel not needed.
- Pick tokyo-night -> white (PATINA_THEME=white): the daemon was unresponsive for >30 s during the reset, then back. No requester afterwards; screen title went "Directory Opus" -> "Workbench Screen" (screen closed and reopened), lister (memory text changed 1,019.4M -> 1,018.5M free) and button bank reopened.
- PATINA_DEFERRED=1, PATINA_WBCLOSES=1, PATINA_FOREIGN='Ram Disk,...(lister 0x2020203f);-/200a70 (button bank)' : the requester is no longer listed as foreign.
Verdict: PASS for requester and reopen. Theme pens on the reopened DOpus windows not measured separately (no screenshot taken).

## S83 half-drawn theme window

Probe of rig 21 (owner's held rig, "S83 session: eye check A theme pick"), read-only, 2026-11-15. No code changed, no clicks or keys sent.

### Symptom
DOpus 5 Workbench (IPrefs-patched DOpus md5 21e935e9, one lister, flat on). Style > Patina theme, pick "starship", confirm Keep. Theme applies, DOpus lister closes and reopens (lister text went 593K -> 652K in use), but the picker window "Patina" (220,182 360x236) stays half drawn: orange top and left edge, rows miasma/nord/retro-82 and a sliced 4th row, no right/bottom edge, no bottom bar, nothing below about y=418. Owner shot: rig21-a.png.

### Evidence
- Window list (newest first): "Patina bar", "Patina" (active), DOpus lister, DOpus buttons, backdrop. The picker window opened after the lister reopened.
- patina-gui task is alive and idle in event_loop Wait (gui.c:1433), wait mask 0x20009000 (window port bit 29, CTRL_C, CTRL_F). It is not stuck mid-draw; it is not drawing.
- ENV: PATINA_WBCLOSES=1, PATINA_DEFERRED=1 (foreign DOpus windows took the deferral branch), PATINA_FOREIGN = DOpus lister + button bank, PATINA_DRAWS=10, PATINA_THEME=starship, CONFIRM_SHOWN=1, CONFIRM_KEEPS=1, TEARS=0, PATINA_ROWS="Patina theme|Terminal" (possibly stale from before the pick).
- The window uses WA_BackFill LAYERS_NOBACKFILL (gui.c:1579-1592), so any area not painted by Patina shows the stale screen pixels. That matches the picture: old picker remnants over lister background.

### Root cause (leading hypothesis, unverified)
The gui's draw guard `g->await` stays 1, so every draw_full/draw_list returns at once (gui.c:692, 715; also 522, 1303, 1310, 1374, 1420).
- relayout (gui.c:764-773) sets `g->await = 1` and calls ChangeWindowBox; only IDCMP_CHANGEWINDOW on that window clears it (gui.c:1463-1464).
- The theme apply closes and reopens Patina's windows (desk_dos.c dk_windows_close / dk_windows_open via desk_dos_own_close = gui_hide, own_open = gui_show, gui.c:1976-1977). If the window is closed while await=1, the owed IDCMP_CHANGEWINDOW dies with the old window.
- gui_show (gui.c:1742-1761) opens the new window at the laid-out box, then calls draw_full, but never clears `g->await`. draw_full returns at once; no CHANGEWINDOW is ever coming for the new window. The window stays undrawn until some later event, and the NOBACKFILL layer shows the old pixels.
- Contributing: the deferral branch in desk_dos_apply (desk_dos.c ~525-540) reopens Patina windows before the palette write, on the old assumption that DOpus blocks the IPrefs reset. With the S82 DOpus IPrefs fix the reset now happens after Patina's windows are back, so DOpus closes/reopens around an open picker.
Not proven: the exact moment await went to 1 (preview relayout before apply, or theme_live -> list_rebuild_keep -> relayout after it). wincensus (gate counters, window owner) was not run: build queue too long; build cancelled.

### Smallest fix
In gui_show (gui.c:1742-1761) set `g->await = 0;` before `draw_full(g)`. The new window already has the laid-out box, so no CHANGEWINDOW is owed. One line, root layer (gui owns its draw guard).
Follow-up to decide separately: in desk_dos_apply's deferral branch, now that DOpus cooperates with the reset, wait for the reset (dk_wait_reset) before dk_windows_open, or stop counting a cooperating DOpus as a blocking foreign window.

### Rig verify
Fresh rig (not 8/81/83/183, not 21 unless owner frees it), DOpus 5 Workbench with one lister, flat on. Style > Patina theme, pick a theme, Keep. PASS: picker fully drawn (all four edges, bottom bar, buttons), PATINA_DRAWS rises after the pick, gui still in event_loop. Also pick a theme from a different-height level to force a ChangeWindowBox. Append a test-register row.

### Light grey WB title bar
Not investigated (budget). Unverified whether flat mode should leave the screen title bar light grey.
