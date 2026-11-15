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
