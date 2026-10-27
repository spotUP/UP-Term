---
date: 2026-10-27
topic: Warm path for the drawn menu (amarchy-gui serve), reusing the amarchy-menu warm server machinery
status: done (G1-G12, 2026-10-27)
ledger: thoughts/spot/todos.md row AMARCHY-GUI-WARM
---
# Amarchy drawn menu warm path

## Problem
On the A1200 the menu style is drawn. Every hotkey press runs amarchy-gui cold (LoadSeg of 282 KB, library,
screen, font, icon and index setup) and it has no timing marks, so the warm-open number cannot be measured.
The text menu already has a warm server (`amarchy-menu serve`, port AMARCHY.MENUSRV, plan
2026-10-27-amarchy-warm-menu-server.md). The drawn menu needs the same, through the same code.

## Owner decisions (2026-10-27)
- Both servers start at boot (amarchy-menu serve and amarchy-gui serve). The style is picked per press
  (cx_menu_style: a terminal in front can pick the text menu), so both get used.
- The drawn server reloads keymap, index, pens and screen per open. Font (mt_open) and icons (ig_open) are
  cached only if the rig marks show they cost more than 50 ms; then they are invalidated on a screen or
  AM_FONT change.
- This row comes before AMARCHY-LAUNCH-FIELD7.

## Design
- One mechanism, two instances. `mo_srv` (menu_open.h) gets `port` and `task` fields; srv_find, srv_take_over,
  srv_send and mo_srv_quit use them instead of MS_PORT / MS_TASK_NAME. The pure core ms_open is unchanged.
- New names in menu_open.h: GS_PORT "AMARCHY.GUISRV", GS_TASK_NAME "amarchy-gui". The message is the same
  struct ms_msg; the drawn server ignores spec (it opens its own Intuition window).
- Pure choice in cxdesc.c: `cx_menu_srv(text, &port, &task)` names the server for a style (host-tested).
- act.c: two mo_srv instances. act_menu_server_start starts both (srv_start_proc takes the program name from
  the instance), act_menu_server_quit quits both, act_menu_open(text, ...) picks the instance. The drawn cold
  fallback is today's act_run(line).
- amarchy.c act_menu_kind: the drawn branch calls act_menu_open (text 0) instead of act_run.
- gui.c: main split into gui_open(viewer, node) (one open: everything main did after argument parsing, state
  reset first) and `serve` (port, Wait on port | Ctrl-C, reply at once, idle Ctrl-C ignored, MS_QUIT ends).
  Libraries open once per process. `menu` / `viewer` command lines keep working (the cold fallback).
- Marks in gui.c: start (open begins), window (window open), ready (first full draw done). Cold and warm
  paths mark the same names, so the rig script reads both.
- Stack: amarchy-gui keeps its $STACK cookie; the server start reads it (seg_stack), as for amarchy-menu.

## Checklist
- [x] G1 mo_srv port/task fields; menu_open.c uses them; act.c text instance sets MS_PORT / MS_TASK_NAME.
- [x] G2 GS_PORT / GS_TASK_NAME; names chosen by `ms_server_names(text, &port, &task)` in the pure
  menusrv_core.c (all four names moved there from menu_open.h) instead of cx_menu_srv in cxdesc.c, so the
  names and the choice live in one header; host test test_server_names in suite_menusrv.
- [x] G3 act.c two instances, start both, quit both, act_menu_open picks by text flag.
- [x] G4 amarchy.c drawn branch through act_menu_open.
- [x] G5 gui.c gui_open split + per-open reset (G struct, km, ixs, menu_cond, watcher, Ctrl-C, mo_register).
- [x] G6 gui.c serve loop.
- [x] G7 gui.c marks start / window / ready.
- [x] G8 build Amarchy + amarchy-gui clean (vbcc, warnings as errors); host suites menusrv 33 checks and
  keymap 11241 checks, 0 failed. amarchy-menu needs UI_HOTKEY_RIGHT from the unmerged upterm-ports branch
  feat/ui-hotkey-right (pre-existing, c5d7bb19); builds with UPKGUI_SYS pointing at that sysroot.
- [x] G9 rig 7 (commit 11d4947d, evidence research/2026-10-27-amarchy-gui-warm-timing.md): warm presses 120/140/180 ms,
  median 140 ms, each with a send mark and one server owner; true cold (stale port, act_run fallback) median 140 ms.
  Font (start to font) 20-40 ms, icons 0 ms in 5 of 6 runs. Original text: timing script on the drawn style: three presses, each with a send mark, one amarchy-gui task,
  ready - hotkey printed; record font / icon cost from marks (decides the cache step).
- [x] G10 exemption: no cache. G9 measured font 20-40 ms and icons 0-20 ms per open, both under the 50 ms bar
  the owner set, so font and icons keep loading per open (no invalidation logic needed).
- [x] G11 PASS: stale port falls back cold, restarted server serves warm (new owner), commodity quit removes
  AMARCHY.GUISRV and ends the task. Original text: kill the gui server, press: cold fallback opens the menu; next press warm again.
- [x] G12 A1200 deploy (Amarchy 31138cd1, amarchy-gui 42549511 to UP-Term:bin only, old ones kept as *.bak-g12;
  UHCBIN untouched) and timing with the drawn style. Commodity restarted with `arexx AMARCHY QUIT` then
  `Run >NIL: UP-Term:bin/Amarchy`.

## Results
- Rig 7 (060): warm median 140 ms hotkey to ready; true cold fallback also median 140 ms at the 20 ms tick, so
  the warm server's gain on a 060 is below the clock resolution. The A1200 (G12) decides whether it pays there.
- A1200 (G12, drawn style): warm median 880 ms (44 ticks over 7 runs: 44 44 44 45 45 44 44), cold fallback median
  1440 ms (72 ticks over 4 runs: 72 73 72 72). The warm server saves about 560 ms per open. Warm split: font 180 ms,
  icons ~20 ms, window 240 ms, draw to ready 420 ms. Cold adds ~380 ms of process start before the font mark.
  Cold runs: server task killed (stale port), press fell back cold, the commodity restarted the server each time.
- Found on the A1200: its prefs had no `menu-in-terminal` key, so the default text style opened first; the agent set
  graphical for the run and restored the original prefs after.
- The drawn menu closes on a second hotkey press, not on Ctrl-C typed by hand (gui.c, commodity signal).
