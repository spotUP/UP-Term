# Amarchy demo: research (B4a)

Read-only survey of worktree vtcon-s61-rig96 (branch post-release-fixes). Paths relative to the worktree root. Another agent was
editing amarchy/ at the same time; line numbers are as read. "unverified" = not read to the end.

## Q1 Feature set (from the dispatch structures)

### Dispatch structures

ARexx port AMARCHY, `rexx_cmd` (amarchy/amarchy.c:476-515). `starts()` (amarchy.c:54) is a case-insensitive PREFIX test, so a verb
`DEMO` must come before any verb that is a prefix of it and must not be shadowed (no existing verb starts with D except `DESK CONFIRM`,
amarchy.c:483, which needs the full word `DESK CONFIRM`; `DEMO` does not match it).

| Verb | Line | Does |
|---|---|---|
| STATUS | amarchy.c:479 | status_text: counters (win_count, win_snap_count, bar_is_open, wb pens, shots, notes, ticks; amarchy.c:340-351) |
| BAR <word> | 481 -> bar_port 356-382 | STATUS, ON, OFF, TOGGLE, TEXT, PENS, NET, NETUP, NETDOWN (bar_verb; BARV_*) |
| DESK CONFIRM <mask> | 483 | dc_begin: Keep/Revert prompt for an applied theme (desk_confirm_dos.c:92) |
| RELOAD | 485 | reload() amarchy.c:441: style_load, keymap reload |
| RUN <row> | 487 | do_row (amarchy.c:401): one keymap row's action |
| WIN <verb> | 489 -> win_port win.c:358 | window verbs (below) |
| CAP screen/window/region | 491 -> cap_run 391 | screenshot (shot.h:18-23); reply is the file |
| SHOTS | 499 | capture counter |
| NOTES | 504 -> note_run | notice counters; the same verb on port AMARCHY.NOTIFY (note.h:6-23) |
| QUIT | 507 | g_quit = 1 |
| SHOW | 509 | act_menu_kind(KA_VIEWER) |
| other | 511 | rc 10 |

Keymap action kinds, config/keymap.h:68-86 (the KA_* enum lives there, not in handler/keymap_act.h; act.h only declares
act_menu_kind): KA_MENU (quick menu node), KA_VIEWER, KA_TAB, KA_MENUID, KA_SLASH, KA_RUN, KA_REXX, KA_OPEN, KA_DETACH, KA_SCAN,
KA_WIN, KA_GTHEME, KA_JOB, KA_CAP, KA_SETTINGS, KA_GUI, KA_AMARCHY (quit), KA_BAR. The commodity executes them in `do_row`
amarchy.c:401-436 (switch: KA_RUN/JOB/DETACH/SCAN -> act_run; KA_REXX -> act_rexx; KA_WIN -> act_win; KA_GTHEME ->
gt_dos_select(arg,0,0); KA_CAP -> cap_run; KA_OPEN -> act_open_uterm; KA_BAR -> bar_port; KA_GUI -> amarchy-gui helper;
KA_AMARCHY -> g_quit; KA_MENU/KA_VIEWER -> act_menu_kind).

Window verbs (win_parse, amarchy/win_core.c:30-39): bare front, back, zip, fullwidth, fullscreen, save, restore, closedrawers;
with argument cycle (next|prev), move, size, focus (left|right|up|down), screen, title, snap (left right up down tl tr bl br
center). Executed by `win_exec` win.c:233 on the active window (Intuition ActiveWindow under LockIBase).

Tiling is NOT in the Amarchy process: AmTile is its own commodity (amarchy/tile.c; header comment tile.c:1-8, usage tile.c:622):
ON OFF TOGGLE RETILE GAP n STATUS QUIT LAYOUT name|NEXT|PREV RATIO n NMASTER n CYCLE NEXT|PREV PROMOTE FLOAT ON|OFF|TOGGLE
SWAP LEFT|RIGHT|UP|DOWN. Layouts (tile_core.h tile_layout_kind): dwindle, master-stack, mirrored, columns, rows, grid, monocle.
The menu rows drive it with `run:UP-Term:bin/AmTile ...` (dist/keymap:262-283). Amarchy-side state of AmTile: static `g_on`,
`g_layout`, `g_nmaster` (tile.c:61-64), persisted as prefs keys tile-layout/ratio/gap/nmaster (amconf_core.h:22-25).

Menu model: the `[menu]` table of dist/keymap (line 176 on), one row per `dotted.id = label | action`; the first dotted segment
is the group (apps, dopus, settings, agents, network, style, capture, window, screens, workbench, learn, system, install,
packages, trigger, help, quit). Parsed into `km_menu` rows, rendered by menu_core.c / menu_theme.c / menu_tree.c (mn_children,
mn_row_visible, menu_tree.c:21-45). Callback shape of a leaf: `lf_plan` -> `lf_kind` (amarchy/leaf.h; LF_QUIT leaf.h:51;
dispatch leaf.c:~700-760: LF_SCAN, LF_JOB, LF_WINDOW, LF_VIEWER/LF_GUI, LF_QUIT, LF_REXX, LF_OPEN, LF_WIN, LF_CAP ...), executed
in the menu program (amarchy-menu / amarchy-gui process), which reaches the commodity by `rexx_send("AMARCHY", "<verb>", 0, 0)`
(rexx_host.h:43; LF_QUIT at leaf.c:738-744, LF_WIN via send_win).

### Groups (10 scenes). Drive = from inside the Amarchy process, the same function the port verb calls.

| # | Group | Features | Driver (file:line) | Visible change / rig observable |
|---|---|---|---|---|
| 1 | Windows: move, size, zip | win:move/size/zip/fullwidth/fullscreen/save/restore on a demo window | win_exec win.c:233 (win_port win.c:358) | window box changes: wasabi `windows`; STATUS win=N snaps |
| 2 | Snap | win:snap left/right/tl/tr/bl/br/center | win_exec WV_SNAP win.c:~246 (snap uses tile-gap from amconf) | box = half/quarter; `windows`; STATUS win_snap_count (amarchy.c:344) |
| 3 | Focus and cycle | win:focus dir, win:cycle next/prev, win:front/back | win_exec | active window title changes (`windows` / `ui list`) |
| 4 | Tiling layouts | dwindle, master-stack, mirrored, columns, rows, grid, monocle, ratio, gap, float, swap | AmTile is another process: send `LAYOUT x` to its port (tile.c:403 starts "LAYOUT"); the pure box math is tile_layout tile_core.h:~50 | boxes of the tiled windows; AmTile `STATUS` (tile.c:516). Not callable in-process without linking tile_core + Intuition glue |
| 5 | Themes | switch Omarchy theme live (gtheme rows) | gt_dos_select gthemes_dos.h:34 (KA_GTHEME, amarchy.c:422); name via gt_dos_current gthemes_dos.h:12 | screen palette + Workbench pens; counters gt_dos_wb_pens (amarchy.c:347), desk_dos_confirm_asks (desk_dos.h:56). Heavy: writes prefs and may reset Workbench (desk_dos.h:30), then waits Keep/Revert (dc_begin) |
| 6 | Desktop look | flat window frame/gadgets, icon style filled/outline, icon label mode, border size, backdrop, pointer | prefs keys AM_FLAT, AM_FLATGAD, AM_FLATSIZE, AM_ICONSTYLE, AM_ICONTEXT, AM_DESK* (amconf_core.h:29-39); amarchy-flat is a separate helper (flat_shared.h) | amarchy-flat sentinels (flat_shared.h:34-47: dcalls, dflat), desk apply counters (desk_dos.h:35). Persists to ENVARC unless reverted: the demo must not use these without restore |
| 7 | Status bar | segments screens, clock, cpu, ram, network, agents, system, tray; on/off/toggle | bar_port amarchy.c:356 (bar_plan, bar_open/bar_close) | BAR STATUS counters (amarchy_bar_rig.py:68,158-160), `bar` bit in STATUS (amarchy.c:346) |
| 8 | Notices and clips | notify queue, clipboard history | note_run note.h:23 (NOTES verb; post path note_core); cw_start/cw_changed clipwatch.h:8-12, clips_core.c | notice counters (STATUS notes=, note_posted note.h:21); notice window not built yet ("no window yet (P2)", note.h:2) |
| 9 | Capture | screen / window / region screenshot | cap_run amarchy.c:391 (shot_screen/shot_window/shot_region shot.h:21-23) | file in shot-dir; SHOTS counter (amarchy.c:499) |
| 10 | Launcher and packages | quick menu (SHOW), app index, Rescan, package browse (upkg), WBStartup launch | act_menu_kind(KA_VIEWER) amarchy.c:509; index_core.c, pkgs_core.c, pkgs_dos.c; wbl_start/wbl_open wblaunch.h:17-27; pl_start proclaunch.h:19 | menu window opens (`windows`, `ui list`); launch counter via STATUS launches |
| (11) | Help and welcome | amarchy-gui help / welcome | KA_GUI -> cx_helper_cmd + act_run amarchy.c:426-428; help_core.c | GUI window title |

Candidate to fold: 11 into 10, giving 10 groups at 12 s = 2 minutes.
Danger groups for a showcase: 5 and 6 (persisting writes, Workbench reset, 15 s Keep/Revert prompt; desk_core.h:317-324). The
demo needs an "apply without persist" path or must restore through desk_dos_revert (desk_dos.h:50) at the end.

## Q2 Entry wiring

### Command line today
- `main` amarchy.c:559. `qmark_line(GetArgStr())` (566) handles `Amarchy ?`; then `ReadArgs("CX_PRIORITY/K/N")` (570-575). A word
  `demo` fails the template (ReadArgs returns 0, the code ignores the failure and continues with pri = 0) so today `Amarchy demo`
  would START (or try to start) a second commodity, not talk to the running one.
- Single instance: `cx_open(pri)` (amarchy.c ~580; amarchy/cx.c:93 CxBroker) returns 0 when a broker of that name exists ("already
  running (it was told)", amarchy.c ~581, returns 5). So a second invocation never reaches the main loop; it is told through the
  broker (CXCMD_UNIQUE), not through the ARexx port.
- The port: `rexx_host_open(&rexx, "AMARCHY")` (amarchy.c ~585), commands into `rexx_cmd` (amarchy.c:476). A second process sends with
  `rexx_send("AMARCHY", line, result, cap)` (amarchy/rexx_host.h:43; used at leaf.c:738 for QUIT).

### Proposed shape (one code path)
- Port verb `DEMO` with optional argument: `DEMO` (start, loop), `DEMO STOP` (end, restore), `DEMO STATUS` (state and scene index),
  `DEMO <n>` (one scene alone, for a rig row). Parsed in `rexx_cmd` next to `WIN`/`BAR`: `else if (starts(cmd,"DEMO")) rc = demo_port(cmd+4, out);` (new `demo_port` in a new amarchy/demo.c, like `bar_port` / `win_port`).
- CLI form: in `main` before ReadArgs (amarchy.c:566-570): if the first word of `GetArgStr()` is `demo` (case-insensitive, via `starts`), do not run the commodity at all: `rexx_send("AMARCHY","DEMO ...",out,cap)`, print out, return its rc (5 when the port is absent: "Amarchy is not running"). This is what the menu item also does through the leaf shape below, so one handler (`demo_port`) serves both.
- Menu item: `learn`/`help` style row in the `[menu]` table with a new action kind, or reuse `rexx:address AMARCHY DEMO` (existing KA_REXX kind, as network.up at dist/keymap:~248 does with `rexx:address AMARCHY BAR NETUP`). Using KA_REXX needs no new KA_* kind, no keymap.h/leaf.h change, and the menu row hits the very same `DEMO` verb. Fallback if a new kind is wanted: `demo` prefix in the keymap parser (config/keymap.h:68-86, plus a leaf kind in leaf.h and the leaf.c switch like LF_QUIT).
- Demo must not block the main Wait loop: `rexx_cmd` runs inside the loop (amarchy.c), so `DEMO` only arms a state machine; the scene steps and the "key or click ends it" check run from the commodity tick (tick.c / tick.h hook, as `note_expire`), or the loop gets a signal bit for an input handler. A blocking `demo_run()` inside `rexx_cmd` would freeze hotkeys, the bar and the reply to the sender.

### About item
- Amarchy's menu has NO "About Amarchy" row (grep -i about over dist/keymap, amarchy/*.c, prefs/*.c: only the rows below).
- "About Workbench": dist/keymap:295, `workbench.about = About Workbench | rexx:address WORKBENCH 'MENU WINDOW ROOT INVOKE WORKBENCH.ABOUT'` (condition MN_COND_WB from menu_tree.c:32 `workbench.` prefix; test row tests/test_keymap.c:1083).
- "About UP-Term...": handler/menu_def.h:23 (MD_ROW ... MENU_ABOUT), handled at handler/vtcon_handler.c:3041 -> `about(c)` vtcon_handler.c:2981; slash `/about` handler/slash.c:81. That is the terminal's own menu bar, not the Amarchy quick menu.
- The nearest existing neighbour for a Demo row in the Amarchy menu is `learn.demo = Demo tour | slash:/demo` (dist/keymap:332-ish; tests/test_keymap.c:1170 checks it), which starts UPDemo in a terminal. The new row therefore goes into a group beside `learn.*`/`help`, e.g. `learn.amarchydemo = Amarchy demo | rexx:address AMARCHY DEMO` (id and group to be decided in design; the `[menu]` order defines the position).

### UPDemo reuse (demo/updemo.h, demo/updemo.c)
- `updemo_io` (updemo.h ~20-30): key(wait_ms), ticks() in 1/50 s, command(); `updemo_run(io, cols, rows, first, scene_ticks)` (updemo.h:~48). `scene_ticks` = every scene's length in 1/50 s, 0 = each scene's own (updemo.h comment, updemo.c `dur = scene_ticks ? scene_ticks : sc->ticks`, ~2700).
- Scene table: `typedef struct scene_def { name; void (*fn)(long t,int first); pixels; ticks; caption; main_screen; bench; caption_ansi; }` updemo.c:2584-2593, `static const scene_def scenes[]` updemo.c:2595-2603 on (each ticks e.g. 450 = 9 s).
- Tick loop: `play()` updemo.c ~2700-2790: per scene `while (!moved && (t = io->ticks() - t0) < dur)` calls `sc->fn(t, fresh)` once per frame, `status(s)` draws the caption (updemo.c ~2660: `Demo n/N  caption ... Space next B back Q quit`), keys via `read_event` (updemo.c:1075) mapped by `act_key`: Space/Return/Right next, B/Left back, Q/Esc/Ctrl-C quit (comment above `play`); mouse is no key.
- Reusable as-is: the SHAPE (table of {name, fn(t, first), ticks, caption}, per-scene clock t from a 1/50 s ticker, caption on a status line, "key ends" mapping, the `scene_ticks` override for rig speed, the `first` argument for one scene). Not reusable: every `fn` and `ps()/pn()/at()` writes escape sequences to a terminal, `follow_size`/`ask_size` ask the console size with CSI 18 t, alternate screen 1049; none of that applies to Intuition windows. Amarchy's scene fn would call the group drivers above and wait on the clock; ending is "any key or mouse click" from an input handler/IDCMP on a window or the commodity's input handler (cx.c filters), not read_event.

## Q3 State to save and restore

- Window geometry: no whole-desktop snapshot exists. Only `win_ring g_ring` (win.c:39-40, win_core.h:112-120): the last WIN_RING_MAX windows' boxes, keyed by window pointer, filled by `win:save` (win.c:287-289) and read by `win:restore` (win.c:293). The ring is per window and shared with the user's own save/restore: the demo must not use it (it would overwrite the user's saved positions); it needs its own `win_ring`/array filled by the same `win_save`/`win_box` types for every window on the screen, and a restore loop with ChangeWindowBox/MoveSizeWindow (win_apply.c; win_box from win_core.h).
- Tiling: AmTile state `g_on`, `g_layout`, `g_nmaster` (tile.c:61-64) and the prefs keys tile-gap/layout/ratio/nmaster (amconf_core.h:22-25). The demo changes them through AmTile's port: read via AmTile `STATUS` (tile.c:516), put back with LAYOUT/RATIO/NMASTER/ON/OFF. Stored by AmTile in ENVARC prefs (tile.c:266 "could not save the layout settings"), so a demo layout change persists unless put back.
- Theme: `gt_dos_current(out,cap)` (gthemes_dos.h:12, gthemes_dos.c:81) is the current theme name; restore = `gt_dos_select(saved,0,0)`. Apply is the heavy path (desk_dos_apply desk_dos.h:12, snapshot dir T:amarchy-revert/ desk_dos.h:49-50, marker ENVARC:Amarchy/pending-revert, desk_core.h:243-244) and arms the 15 s Keep/Revert prompt (dc_begin, desk_confirm_dos.c:92); `desk_dos_revert()` (desk_dos.h:50) puts the whole snapshot back. A crashed demo mid-theme is recovered at next start by `desk_dos_marker_recover` (desk_dos.h:52).
- Bar visibility: `bar_is_open()` and `g_bar_want` (amarchy.c:~346, 371-378); restore with `bar_port("ON"/"OFF")`. Persisted start key: barconf `start` (barconf_core).
- Workspace: no workspaces exist in the Amarchy source (grep workspace in amarchy/: none among the files read; unverified beyond the verb tables above). Screen order: `win:screen` verbs (win_core.c:38) change front screen; state = which screen is in front (IntuitionBase->FirstScreen), save/restore with ScreenToFront.
- Focus: `IntuitionBase->ActiveWindow` (win.c:~262 `aw = IntuitionBase->ActiveWindow` under LockIBase); restore with ActivateWindow.
- Existing snapshot/restore functions found: desk_snapshot / desk_restore (desk_core.h:201,229) and desk_dos_revert (desk_dos.h:50) cover THEME/DESKTOP FILES only; gt_dos_desk_restore (gthemes_dos.h:24) only the picker preview. None covers window boxes, tiling, bar or focus. Answer: a general state snapshot does NOT exist; one `demo_state` struct with {win_box + id per window, active id, theme name, AmTile layout/on/ratio/gap/nmaster, bar_is_open, front screen} has to be written (new), reusing win_box/win_save, gt_dos_current, bar_is_open.

## Also

- Rig start/observe: tools/rig/amarchy_rig.py:59 `c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)`, waits for the ARexx port (61); sends verbs with `arexx(cmd)` amarchy_rig.py:30-37 = wasabi `m.arexx(conn,'AMARCHY',cmd)` (empty string when the port is absent), `status()` follows (line 40). Other rigs: tools/rig/amarchy_bar_rig.py:68,158-160 (BAR STATUS counters), amarchy_note_rig.py, amarchy_window_rig.py, amarchy_tile_rig.py, amarchy_cap_rig.py. tools/test_rig_amarchy_conf.py only guards where Amarchy prefs keys are written (ENV:Amarchy/prefs vs ENV:up-term/up-term); it does not start or send.
- Sentinels: `am_sentinel(name, n)` / `am_sentinel_str(name, value)` amarchy/sentinel_dos.h:5-8 publish an AmigaDOS global variable a rig case reads after the run (use in borderblank_patch.c:190-240, e.g. AMARCHY_BORDERBLANK_INSTALLED). A demo scene counter would be published the same way (`AMARCHY_DEMO_SCENE`, `AMARCHY_DEMO_DONE`) plus extra fields in STATUS/`DEMO STATUS`. Rig case reads: wasabi `windows` / `ui list` text and `arexx('DEMO STATUS')`.
- Stack: `long __stack = 65536;` and cookie `"$STACK: 65536"` at amarchy/amarchy.c:72-73 (comment 62-71: 396 bytes more than the former 16384 overran into the BSS hunk holding saved system-patch vectors; pinned by tests/test_amarchy_stack.py; amarchy-gui runs the same code on the same size). Makefile test row `dist_stack` (Makefile:118) checks every kit program sets a `$STACK:` cookie. The demo's frames add to the main task's stack; a scene state machine in the tick hook keeps depth small; do not put big buffers (win array of WIN_MAXW boxes, a `gtheme` ~KB) on the stack, as iconpen_dos.c:225 already notes for a 6 KB struct; make `demo_state` static.

## Open items for the design (not decided here)
1. Pick the menu group/position for the Amarchy-demo row (there is no Amarchy About row; neighbours are workbench.about dist/keymap:295 and learn.demo).
2. Whether AmTile is driven from the demo through its port (separate process, async) or the tile math is linked in-process; the port route works but is asynchronous: a scene must wait for AmTile's STATUS to show the layout.
3. Themes/desktop scenes (groups 5, 6) persist to ENVARC unless reverted; need a no-persist apply or a guaranteed `desk_dos_revert` on every exit path (key, click, QUIT during demo, crash recovery marker).
4. Ending on "key or mouse click" inside the commodity: input handler in cx.c or an IDCMP window per scene is unread; needs a reading of cx.c:93-140 before design.
