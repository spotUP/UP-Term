# Handler DOS call inventory (read-only analysis, 2026-11-04)

Method: grep of DOS entry points over handler/*.c (code lines, not comments), then caller chains by grep. Files with zero code hits: be32, brk, clipfmt, complete_core, frontscr, histfile, kcmode, keymap_act, lineedit, sbar_gad, slash, upactive, upmru, upreq, waitset, winspec, tabslot. pty_handler.c: no DOS file/lock calls (only OpenLibrary/OpenDevice). Note: grep proves spelling only; a call through a function pointer (vt_set_file_reader callbacks, cc_*_os tables) was followed by hand.

## Tab/pane processes (decisive)
Every window, tab and pane is its OWN process running handler_main (vtcon_handler.c:8705; c->port = &me->pr_MsgPort at 8716; main loop waits on c->port at ~8782, also tab_port/tab_reply/comp_port/watch_port). Comments say "one code, many processes" (~5192) and "a hand-over to a tab process" (~6343). is_tab only marks that the window belongs to the host (tab_activate / TM_REGISTER via tab_host port, 7825). So tab processes DO receive DOS packets on their own pr_MsgPort: their DOS calls are violations exactly like the host's. All HANDLER rows below apply to every window/tab process.

## Table
| file:line | call | enclosing function | process | chain / evidence |
|---|---|---|---|---|
| vtcon_handler.c:497-502 | Open, Seek, Write (RAM:vtcon.log) | dbg_flush | HANDLER (mitigated) | dbg() ~460-478 (any debug print on any handler process) -> dbg_flush only if dbg_port->mp_MsgList empty (478); DBG_FLUSH() at 8768 in handler_main startup. Guard checks the list, not a packet arriving during Write: still a violation |
| vtcon_handler.c:761,766-768 | IsFileSystem, Lock, Examine, UnLock | kfile_lock | HANDLER | callers kfile_read:789, kfile_unlink:829; registered via vt_set_file_reader(c->w.t, kfile_read, kfile_unlink) at 3311 (con setup on handler process); invoked by the vt engine while it processes output on that process (trigger vt_* call not traced: unverified) |
| vtcon_handler.c:796-809 | OpenFromLock, Seek, Read, Close, UnLock | kfile_read | HANDLER | same as above |
| vtcon_handler.c:834-844 | ParentDir, Lock("T:"), SameLock, UnLock, DeleteFile | kfile_unlink | HANDLER | same as above |
| vtcon_handler.c:4938-4943 | Open, Write, Close | slash_run (SLASH_SCREEN_TEXT) | HANDLER | slash_run callers: 8488 (packet action with dp_Arg2/3 = slash command, reply(p,...) on packet loop), 2912/2926 (slash_menu_line path), 6067 slash_menu, 5194 (Return in line editor); all on the window's process |
| clip.c:88-118 | Open, ExamineFH, Read, Seek, Close | clip_write_form | UNKNOWN (no caller) | grep over handler/ tests/ tools/: only definition, clip.h:10 and a test comment (tests/test_clip.c:191); unreferenced in the handler build. Violation if ever called from a handler process |
| vtcon_handler.c:602-605,645 | Open, Write, Close, DeleteFile | png_worker | WORKER "vtcon picture" | CreateNewProcTags 678, PutMsg job 684 |
| vtcon_handler.c:979-981 | Lock, UnLock | fonts_ttf_exists | WORKER (config_worker / watch_worker) | only callers 1210 and 1322, both inside config_worker (1201) / watch_worker (1268); fixed precedent |
| vtcon_handler.c:1149-1170 | Open, Read, Close (ENV:/ENVARC: up-term) | read_conf | WORKER config_worker, watch_worker | callers 1209, 1321 |
| vtcon_handler.c:1178-1194 | Open, Read, Close, GetVar | read_splits | WORKER config_worker | caller 1212 |
| vtcon_handler.c:1249-1257 | Open, Read, Close | read_active_theme | WORKER config_worker, watch_worker | callers 1213, 1314 |
| vtcon_handler.c:1308 | Delay | watch_worker | WORKER | not a packet call |
| vtcon_handler.c:2174-2188 | Lock, UnLock, Open(NIL:), SystemTags, Close | launcher | WORKER "UP-Term Prefs launcher" | CreateNewProcTags 2193 |
| vtcon_handler.c:6791-6792 | SystemTags, Open(NIL:) x2 | run_async | WORKER (tab_spawner) | callers 6870, 6911, 6927, all inside tab_spawner / link_opener helper started by start_worker 7011 |
| vtcon_handler.c:6845-6855 | Lock, UnLock, Open, FPuts, Close | tab_prog_script | WORKER tab_spawner "UP-Term new tab" | caller 6869 |
| vtcon_handler.c:6880,6893-6907 | GetVar, Open, FPuts, Close | tab_spawner | WORKER | start_worker 6771 (CreateNewProcTags), 7011 |
| complete.c:242-243 | Examine, ExNext | scan_dir | WORKER | complete.c worker (990) |
| complete.c:318 | DupLock, Lock | cw_c_next | WORKER | cmd_dirs_os callback, from worker/warm_worker |
| complete.c:352,366-369 | UnLock, Lock | cw_drop, cw_visit | WORKER | same |
| complete.c:456-470 | NameFromLock, Examine, ExNext | co_stat, co_scan | WORKER | cache_os callbacks, from warm_worker |
| complete.c:484-489 | Open, Read, Close | co_load | WORKER warm_worker | |
| complete.c:495-508 | Lock, UnLock, Open, Write, Close, DeleteFile | co_save | WORKER warm_worker | |
| complete.c:729-741 | Lock, UnLock | command_exists | WORKER | worker 1010 |
| complete.c:777-794 | Open, Read, Close | history_load | WORKER | worker 1012 |
| complete.c:806-830 | Open, Write, Seek, Close | history_load/history_append | WORKER | worker 1014 |
| complete.c:859 | NameFromLock | asl_pick | WORKER | worker 996 |
| complete.c:884-889 | Open, Read, Close | theme_read | WORKER | worker (asl theme / theme_list) |
| complete.c:946-956 | Lock, Examine, ExNext, UnLock | theme_list | WORKER | worker 1002 |
| complete.c:990-1070 | DupLock, Lock, UnLock | worker | WORKER | |
| complete.c:667,1093 | CreateNewProcTags only | complete_warm, complete_start | (spawners, no DOS I/O) | complete_start (1087) only creates "vtcon completion" proc and PutMsg; callers on the handler process (2219, 2370, 2436, 4174, 4242, 4301, 4382, 4409, 5393, 7212; complete_warm at 8121) do no DOS. complete.c is entirely worker-side. |
| vtcon_handler.c:3520, 7751 | Delay | wb_client_remove, sync_pause | HANDLER, not a packet call | Delay uses timer.device, never DoPkt on pr_MsgPort; listed for completeness, not a violation |
| upterm.c:115 | ReadArgs | main of the UPTerm client command | separate CLI program | not handler code |
| pty_handler.c | none | - | - | own handler process (PTY:), no DOS file/lock calls found |
| histfile.c | none | - | - | pure formatting; file I/O lives in complete.c history_* (worker) |

## Counts (by function-level site)
HANDLER 5 (dbg_flush, kfile_lock, kfile_read, kfile_unlink, slash_run SCREEN_TEXT); UNKNOWN 1 (clip_write_form, no caller); WORKER 24 (10 in vtcon_handler.c, 14 in complete.c). Non-packet (Delay x3) and client program (ReadArgs) excluded.
Caveat: the vt-engine trigger path for kfile_* and any DOS reached through ASL (asl_pick_file runs in the completion worker) were not traced further.
