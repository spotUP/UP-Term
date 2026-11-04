# HANDLER-DOS-IN-PACKET-LOOP: fix design (final, s56 vtcon-ee)

Invariant: the handler process (and any process that serves DOS packets on its pr_MsgPort) makes no
synchronous DOS call. DoPkt waits on pr_MsgPort and takes a client packet: AN_AsyncPkt 87000004.

Level: handler process structure, not the engine. The engine stays OS-free (vt_set_file_reader hooks).

Reuse, do not invent:
- start_worker (vtcon_handler.c ~6767): fire-and-forget worker, takes a Message, NP_Input/Output 0.
- png_decode/png_worker (~568-700): synchronous worker, parent Waits on a private signal; worker ends
  with Forbid(); Signal(parent, mask) so the job (on the parent's stack) is not touched after.

Plan:
1. Extract one synchronous helper from png_decode: worker_call(entry, name, struct worker_job *j)
   where struct worker_job { struct Message m; struct Task *parent; ULONG sigmask; } is the header of
   every job; it AllocSignal, start_worker-style CreateNewProcTags (plus NP_CurrentDir 0, NP_HomeDir 0
   so no DupLock runs on the handler), PutMsg, Wait, FreeSignal. worker_done(j) = Forbid + Signal.
   png_decode becomes a user of it (no behaviour change).
2. kfile_read / kfile_unlink: bodies move to workers (kfile_read_job / kfile_unlink_job); the hooks the
   engine calls become thin worker_call wrappers. pr_WindowPtr = -1 is set on the WORKER (requesters
   follow the calling process). Sentinels kitty_files_read/deleted unchanged.
3. SLASH_SCREEN_TEXT: buffer filled on the handler (vt_screen_text), Open/Write/Close on a worker.
4. clip_write_form: NOT a violation. Only caller is amarchy/shot.c (separate program); verified git grep.
5. dbg_flush (VTCON_DEBUG build only, :487): Open/Seek/Write on RAM:vtcon.log from the handler; the
   IsListEmpty check narrows but does not close the race. Move the Open/Write to worker_call too.
   Every window, tab and pane is its own handler_main process serving packets on pr_MsgPort (:8716),
   so the rule covers all of them (inventory).
6. Guard: extend tests/test_handler_no_dos_on_handler.py: every DOS call in handler/*.c lives in a
   function on an allow-list of worker entry/job functions (count from the source, not a spelling grep).
7. Rig: kitty t=f image file + t=t delete + /screen-text on a rig, sentinels in PROF line, no 87000004.
