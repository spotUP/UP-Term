---
date: 2026-10-27
topic: Amarchy warm menu server (amarchy-menu stays loaded; the hotkey sends a message instead of spawning)
status: design, not built. Read on worktree prefs-a1 (branch prefs-inline). Two probes (P1, P2) to run first on the rig.
---
# Amarchy warm menu server

Problem and owner decision: see 2026-10-27-amarchy-text-speed.md. 1.6 s of the 1.8-2.1 s open is LoadSeg plus
ixemul startup of the 438 KB amarchy-menu, once per open (amarchy.c act_menu_kind -> act.c:97 act_in_window ->
SystemTags async). Our code from "start" to "ready" is 0.24 s. Decision (final): WARM SERVER. Target after the
change: ready - hotkey about 0.3 s on the A1200 (commodity 15 ms + window open + our 0.24 s), no LoadSeg per open.

## 1. A fresh console per open

### What the code assumes today
- amarchy-menu main (amarchy/menu.c:363) calls ui_start once, runs the menu, ui_stop, exits. The XCON: window is
  opened by the commodity (act.c window_spec + Open) and given as SYS_Input of the new process; ixemul binds fd
  0/1/2 to it at process start.
- libupkgui ui_start (upterm-ports/pkgs/upkg-ui/src/ui.c:906): setlocale, set_escdelay, keyfp = open_keys(), then
  newterm(NULL, stdout, keyfp), cbreak/noecho/nonl/keypad/curs_set, init_theme. open_keys (ui.c:796) tries
  fopen("/dev/tty") first (ixemul maps it to the process console "*"), then dup of the first tty among fd 0..2.
  Output always goes to the stdout FILE (fd 1). The window size comes from newterm's own size query on that fd, so
  a new newterm per open picks up the new window's cols x rows. No signal handlers are installed by the library
  (menu.c installs SIGINT itself).
- ui_stop (ui.c:937) is only endwin(): the SCREEN is not deleted and keyfp is not closed. A second ui_start in the
  same process would leak a SCREEN and, worse, keep a FILE open on the previous (closed) window.

### Options weighed
A. The server redirects its own stdio per open. The commodity keeps opening the window exactly as today (window_spec
   stays the one copy, act.c:82) and passes the window's BPTR in the message. The server, per open:
   `old = SetConsoleTask(((struct FileHandle *)BADDR(w))->fh_Type)`; `fd = open("/dev/tty", O_RDWR)` (ixemul ->
   "*" = the console just set, a second handle on the same XCON window); fflush(stdout/stderr);
   dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); close(fd); clearerr(stdin/stdout/stderr). Then ui_start as today; its
   open_keys finds /dev/tty on the same window. At close: ui_stop (fixed, below), fflush, `n = open("NIL:", O_RDWR)`,
   dup2(n, 0/1/2), close(n), SetConsoleTask(old), Close(w). The window closes when the last handle goes.
B. libupkgui draws through an explicit FILE pair (a new ui_start_on(FILE *out, FILE *keys)). newterm already takes
   both, so the library side is small, but every other stdout/stderr/getchar/system() use in amarchy-menu and leaf.c
   (lf_run's system() at leaf.c:495/544 runs a command in the menu's own window via fd 0/1; ui_message; the "no
   terminal" fputs+getchar in main) would still hit the server's NIL: stdio. It needs a second change per caller.
C. Plain SelectInput/SelectOutput: changes only the DOS handles; ixemul's fd 0/1/2 and FILEs keep their startup
   binding, so curses would still draw to NIL:. Not enough alone.

Pick A. It rebinds every layer at once (DOS console task, ixemul fds, stdio FILEs, /dev/tty), so all of today's
code, system() in leaf.c included, keeps working unchanged, and the window stays owned and sized by the commodity's
one window_spec. B is a library-wide API change for the same result and still needs A's dup2 for system().
Also SelectInput(w)/SelectOutput(w) per open (restore after), so a native child started with SYS_Input 0 / the
process's Input() sees the window too; cheap, and matches the vsh.c:550 pattern.

Probes to run first (one rig session, before building):
- P1: in a tiny ixemul test program started with NIL: stdio, Open an XCON: window, SetConsoleTask to it, open
  ("/dev/tty"), dup2 to 0/1/2, printf + read a key, restore, Close; repeat twice. Pass: text in each window, the key
  read, the window gone after Close, no handle left (wasabi lists XCON windows: count back to 1). If /dev/tty does not
  follow SetConsoleTask in ixemul 47, fall back to ixemul open() of the window spec string itself (the commodity
  sends the spec instead of a BPTR); only that one function changes.
- P2: ncurses newterm/delscreen twice in one ixemul process on two windows of different sizes: the second draws at
  the second size.

### libupkgui change (upterm-ports, UP-Term session's repo: ledger it there, do not edit from the Amarchy session)
ui_stop becomes the full inverse of ui_start: endwin(); delscreen(scr); fclose(keyfp); keyfp = NULL; scr kept in a
static. Also reset the per-run widget statics a second ui_start must not inherit (msel, mtop, hotkey, nbtn/bsel/
bchosen, clear_lines(), ui_on_tick(0,0), osd stays caller-set). `upkg` calls ui_stop once at exit, so making it
complete changes nothing there. Add a check in pkgs/upkg-ui/check/ (restart.c: ui_start, ui_stop, ui_start on a
second pty of another size, assert LINES/COLS and no fd growth). amarchy-menu pins the new libupkgui
(UPKGUI_SYS in the Makefile).

## 2. Lifecycle

- Port names: the server's long-lived request port is `AMARCHY.MENUSRV` (new, in menu_open.h as MS_PORT). The
  existing `AMARCHY.MENU` (MO_PORT) keeps its meaning "a menu is showing now" for MENU-TOGGLE; the server
  mo_register()s it at the start of each open and mo_unregister()s it at the end (both functions exist; the atexit
  hook is installed only once). amarchy-gui keeps using MO_PORT unchanged.
- Message (menu_open.h):
  `struct ms_msg { struct Message m; LONG kind; /* MS_MENU, MS_VIEWER, MS_QUIT */ BPTR win; LONG cols, rows;
  char node[KM_ID * 2]; }`. The commodity owns one static ms_msg and one reply port (ms_reply, created in main next
  to wb_reply).
- Server reply rule: the server ReplyMsg()s at once on GetMsg, after copying the fields, before opening anything.
  So the commodity never waits and can tell a wedged server: a message that is still not replied at the next send
  means the server stopped serving.
- Start: the commodity starts the server in main after cx_install: `amarchy-menu serve`, SystemTags async,
  SYS_Input/SYS_Output NIL:, NP_Path dup_path(), NP_StackSize from the binary's $STACK cookie (config/segstack.h,
  RULES.md rule 6), NP_Name "amarchy-menu". Not waited for: the first hotkey before the server has its port takes
  the fallback path once.
- Open (new act_menu_open in act.c, called by act_menu_kind instead of act_in_window for KA_MENU/KA_VIEWER):
  1. drain ms_reply (GetMsg). If a send is outstanding and still unreplied: the server is wedged -> mark it lost
     (leak that one message: allocate a fresh one), fallback.
  2. Forbid; p = FindPort(MS_PORT); alive = p && task_alive(p->mp_SigTask, "amarchy-menu"); if p && !alive:
     RemPort(p) (stale port of a killed server: take it over, its memory is lost with the task); Permit.
  3. alive: open the window (window_spec + Open, as today), fill ms_msg, PutMsg(p) inside the same Forbid as the
     FindPort (so the port cannot vanish between them), at_mark("send"). Done.
  4. not alive: fallback = today's act_in_window(line) spawn path once, then restart the server (start above),
     at most once per 10 s (a server that dies at start must not respawn on every key).
- Shutdown: on commodity quit (after cx_close, before the launch_port drain): if the server port is alive, PutMsg
  MS_QUIT and wait for its reply with a bounded wait (the commodity's Wait on ms_reply plus a 2 s timer, or poll
  10 x Delay(10)); the server replies, mo_unregister, RemPort its port, exits. A menu showing at quit gets
  mo_close_open first (Ctrl-C), then MS_QUIT.
- Server main loop (`amarchy-menu serve`): register MS_PORT (Forbid/FindPort/AddPort, refuse if a live one exists:
  exit 5), then `for (;;) Wait(port | SIGBREAKF_CTRL_C)`; idle Ctrl-C is cleared and ignored (a stray toggle after
  close must not kill the server); per message run serve_one(). The `menu [node]` and `viewer` command lines keep
  working unchanged (fallback path and the shell), sharing serve_one with the window taken from stdio as today.

### Liveness check (fixes AMARCHY-MENU-STALE-PORT too)
The stale-port class is "a public port found by name whose owner task is gone". Fix once, used by both ports:
- Move ixwatch.c's static task_exists (ixwatch.c:18, walks ThisTask/TaskReady/TaskWait under Forbid, checks the
  name against address reuse) to a shared amarchy/taskalive.c + taskalive.h (`int task_alive(struct Task *, const
  char *name)`); ixwatch.c calls it; the commodity (vbcc) and amarchy-menu/amarchy-gui (gcc) link it.
- mo_close_open (menu_open.c:37): signal only when task_alive(owner, name) holds, name being "amarchy-menu" or
  "amarchy-gui" (both get a fixed NP_Name at start, so the name check catches a reused task address); a dead owner ->
  RemPort the stale port and return 0, so the hotkey opens a menu.
- The pure verdict lives in a host-tested core (below): `mo_verdict(found, alive) -> MO_NONE | MO_SIGNAL |
  MO_TAKE_OVER` and `ms_route(found, alive, outstanding_unreplied) -> MS_SEND | MS_SPAWN_RESTART`.
- A task that exists but is suspended (Guru'd) still counts as alive; the unreplied-message rule in step 1 catches
  it for the server on the next press. For MO_PORT a suspended owner stays a residual case: note it in the ledger row.

## 3. State reset between opens
serve_one() runs exactly what main does today after the "start" mark, every open, and frees it at the end:
- at_mark("start") on receipt (keeps the rig script's mark names), SetSignal(0, SIGBREAKF_CTRL_C), closing = 0.
- keymap_free(&km); keymap_load(&km, km_default_text): prefs or keymap edits since the last open are picked up.
  Cheap (it is inside today's 0.24 s); a later optimisation may skip it on an unchanged generation, not now.
- menu_loop already loads and frees the index (ix_load_file/ix_free), lister rows, gthemes rows and menu_cond per
  call; ix_watcher_begin re-snapshots the scan. alloc_level/free_level pair per call. Keep.
- leaf_core.c statics (lf_conf, lf_work, lf_active at :257-260): add lf_reset() that drops the loaded settings so
  the next read reloads ENV:/ENVARC: (Sonnet: confirm whether they are loaded per call or cached; reset only if cached).
- Theme: colours are terminal palette indices (ui.c init_theme runs inside ui_start, so per open); the file theme is
  read through lf_* per open after lf_reset. Nothing to cache.
- TERM: mn_term_default/setenv once at server start (the commodity's termenv_ensure sets the global var; ixemul's
  environ is a startup copy, so read TERM via GetVar per open if it can change; today it cannot).
- cwd and path: the server inherits the commodity's (NP_Path copy, CurrentDir of the commodity), the same as each
  spawned menu got before. Restore CurrentDir after any leaf that changed it (none today: lf_run uses SystemTags/
  system with paths).
- ui_osd(kind != viewer) per open; hot[] is constant (hotkeys_init once).
- Stdio rebind and restore as in section 1 (option A).
- An open ends on Escape, a launched leaf, Ctrl-C (toggle) or EXIT_NO_TERMINAL (CLOSEINACTIVE closed the window):
  all return to the Wait loop, never exit. Only MS_QUIT or a failed port registration exits.

## 4. Stack
amarchy-menu keeps `long __stack = MENU_STACK` (262144) and its "$STACK: 262144" cookie (menu.c:35-39) unchanged;
the server is the same binary, so tests/test_amarchy_stack.py (dist-check) still covers it. The commodity starts it
with NP_StackSize read from the cookie via config/segstack.h, not a fixed number (act_in_window's 65536 is the
pattern to drop for the server start). Server state lives in statics, not on the stack, as today.

## 5. Tests

Host regression test (fails today, passes with the server):
- New pure core amarchy/menusrv_core.c + menusrv_core.h: mo_verdict, ms_route, and `int ms_open(const ms_ops *ops,
  const ms_req *r)` where ms_ops = { find (found, alive, outstanding), send, spawn, start_server, now } function
  pointers. act.c's act_menu_open is a thin shell that fills ms_ops with the Amiga calls and calls ms_open: the only
  open path for the text menu and viewer.
- New tests/test_menusrv.c, suite "menusrv" in tests/test_main.c, added to TESTS and the vttest_host prerequisites in
  the Makefile. Cases: live server -> send 1, spawn 0, start 0 (the sentinel counters); no port -> spawn 1, start 1;
  stale port (found, dead) -> take over, spawn 1, start 1 (AMARCHY-MENU-STALE-PORT); unreplied outstanding -> spawn
  1; restart throttle (two dead opens inside 10 s -> start 1); mo_verdict(found dead) -> MO_TAKE_OVER.
- Before the change the suite does not exist and act_menu_kind calls act_in_window directly (every open spawns);
  revert check: make ms_open always spawn -> the live-server case fails.
- Run: `make test ONLY=menusrv` (nice/2-worker caps per RULES).

Rig reachability (proves the warm path ran, through the real hotkey):
- tools/rig/amarchy_timing_rig.py: press Left Amiga+Space three times (Escape between). Marks: hotkey, send (new,
  commodity, step 3), start, terminal, ready. Pass: every press has a "send" mark (sentinel: the warm path ran, not
  the fallback), one amarchy-menu task with the same address across all three presses (wasabi ps; the server did not
  restart), and ready - hotkey under a --max-ms threshold (default 500 on the rig). Add --max-ms and the send/
  task checks; print the per-press table as today.
- Stale-port step (AMARCHY-MENU-STALE-PORT rig step): `wasabi kill --force` the server, press: the fallback opens
  the menu (no "send" mark, a "start"), and the next press has a "send" mark again (restarted).
- There is no make target for this script today (grep: test-rig runs tools/rig/reach.py only). Run as
  `UPTERM_RIG=7 python3 tools/rig/amarchy_timing_rig.py --max-ms 500` after `rig.py start --max --purpose
  "<session>: warm menu timing"`; the A1200 number (spec target about 0.3 s) is taken with the same script via wasabi.

## 6. Files to change, in order
1. upterm-ports pkgs/upkg-ui/src/ui.c (+ check/restart.c): ui_stop = endwin + delscreen + fclose(keyfp) + reset widget
   statics; ledger for the UP-Term session, then bump the pin vtcon builds against.
2. amarchy/taskalive.c, amarchy/taskalive.h (new): task_alive moved out of ixwatch.c.
3. amarchy/ixwatch.c: use task_alive, drop the static copy.
4. amarchy/menusrv_core.c, amarchy/menusrv_core.h (new): ms_req/ms_ops, mo_verdict, ms_route, ms_open (pure).
5. amarchy/menu_open.h, amarchy/menu_open.c: MS_PORT, struct ms_msg, MS_* kinds; mo_close_open uses task_alive and
   RemPorts a dead owner's port; mo_register/mo_unregister callable once per open.
6. amarchy/act.c (+ act.h): act_menu_open (fills ms_ops: FindPort+task_alive, window_spec+Open+PutMsg, the old
   act_in_window as spawn, server start with segstack cookie), act_menu_server_start, act_menu_server_quit.
7. amarchy/amarchy.c: act_menu_kind calls act_menu_open for text menus; main creates ms_reply, starts the server after
   cx_install, sends MS_QUIT on quit before the launch drain.
8. amarchy/menu.c: split main into serve_one(kind, node, win) + `serve` loop (MS_PORT, Wait, reply at once,
   stdio rebind/restore per open, keymap reload, closing/Ctrl-C reset, at_mark("start")); `menu`/`viewer` lines unchanged.
9. amarchy/leaf_core.c (+ leaf.h): lf_reset() if lf_conf is cached across calls.
10. Makefile: taskalive.c into AMARCHY_SRC, AMARCHY_MENU_SRC and amarchy-gui's list; menusrv_core.c into AMARCHY_SRC
    and the vttest_host prerequisites; tests/test_menusrv.c into TESTS.
11. tests/test_menusrv.c (new), tests/test_main.c: suite "menusrv".
12. tools/rig/amarchy_timing_rig.py: send mark, same-task check, --max-ms, stale-port step.
13. amarchy/atiming.h: document the "send" mark in the header comment (no code change).
14. thoughts/spot/todos.md: AMARCHY-TEXT-SPEED and AMARCHY-MENU-STALE-PORT rows point here; libupkgui row for UP-Term.
