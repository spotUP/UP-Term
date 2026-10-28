# Apps > Tmux "Program failed (error #87000004)" - static analysis (2026-10-28)

## Verdict
Not an illegal-instruction trap. 0x87000004 = dead-end alert (bit 31) + subsystem 0x07 (dos.library) + 0x0004 =
AN_AsyncPkt, "Unexpected packet received" (ndk-include/exec/alerts.h:200). dos raises it when a process waiting for a
packet reply on its pr_MsgPort receives some other message/packet there.

## Evidence
- build/amiga/amarchy-menu (28 Oct 00:28:57) is newer than every amarchy/*.c (newest 00:28:44) and handler (00:28:45): current.
- Disassembly (142243 lines): 0 `illegal` instructions; `trap #3..#7` hits are in _mo_* code (not the Tmux path), 1 fmove hit is
  misdecoded data at odd address 0x72dd. No 060-missing FPU/CPU code on the path.
- Tmux entry: dist/keymap:131 `apps.tmux = Tmux | run:tmux` -> amarchy/leaf.c run_in_uterm (leaf.c:57-84): shell_line, termenv_ensure,
  Open(UP-Term: spec, MODE_NEWFILE) in the menu process, then SystemTags(line, SYS_Input=w, SYS_Output=0, SYS_Asynch, NP_StackSize).
  Not the wblaunch.c WBStartup path (that is for index rows, not keymap run: rows).
- The alert task is amarchy-menu, so the unexpected packet lands on the menu process's pr_MsgPort, i.e. in a DOS call the menu itself makes
  in run_in_uterm: Open() on the vtcon handler window (packet reply via pr_MsgPort) or the SystemTags/SYS_Asynch setup.

## Violated invariant (hypothesis, unverified at runtime)
Every packet the vtcon handler (handler/vtcon_handler.c, reply() at :485 -> ReplyPkt) sends to dp_Port must be the single reply to a
request the sender is waiting for. A second/late reply to the opener's port (stale or answered-twice packet, e.g. a WAIT_CHAR/READ held
in c->waitchar / c->reads and answered after the request was already answered or after the owner task changed; ws_add/ws_drop_task keyed
on dp_Port->mp_SigTask, vtcon_handler.c:6950-6971, 3526) arrives while amarchy-menu waits in Open() and trips AN_AsyncPkt. Candidate
sites to check next: finish_waitchar (handler ~7500-7510 area), the read-queue completion near :3526, and the SYS_Output=0 default
(SystemTags with output 0 inherits the menu's own Output(), which is the menu's XCON/console, not w - confirm intended).

## Next step (needs rig/debug, out of scope here)
wasabi debug/snoop on amarchy-menu: break on Alert, print the offending message's mn_Node.ln_Name / dp_Type and its reply port origin
(handler task); also log every ReplyPkt dp_Port equal to the menu task's pr_MsgPort.

## Packet snoop (rig 7, 2026-10-28 01:1x, sub-agent 3)
- Discriminator: Apps > Editor (dist/keymap:132 `run:nvim`, Down x2) crashes with the SAME requester as Tmux ("Software Failure", task
  Rushhours:Tools/UP-Term/bin/amarchy-menu, buttons Suspend|Reboot, text per earlier OCR "Program failed (error #87000004)." / "Wait for disk
  activity to finish."). Tmux reproduced 2 more times (3 total on fresh rigs). So it is every run: row through run_in_uterm, not tmux-specific
  (other run: rows not individually tried: only nvim and tmux). Only `open:uterm` (UP-Term row) was not tried.
- Repro needs a wait: after Left Amiga+Space the menu window shows "Filter:" + "Scanning..." while amarchy-index scans; Return/Down
  pressed 2 s after opening hit the wrong state on 2 of 4 tries (no crash, no requester). Wait ~5 s after opening, then Return, Down, Return.
- wasabi snoop cannot show the packet. Task names are "Background CLI (Rushhours:Tools/UP-Term/bin/amarchy-menu)"; filters '*amarchy*',
  '*amarchy-menu*', and '~(*amarchy-index*|*dopus*...)' all produced ZERO lines for the menu task during the repro (only amarchy-index
  Lock/FindTask lines appear with '*amarchy*'); the stream reports only "[wasabi: N snoop event(s) lost]" at menu key/launch times
  (snoop_tmux.log, snoop_nvim.log). snoop logs DOS library calls by name (Lock/Open/FindTask); it never shows dp_Type, dp_Port, sender or
  ReplyPkt. So sender/handler/dp_Type of the offending message is NOT captured. Needs a SetFunction/own trace in the handler's reply() (log
  dp_Port + dp_Type + dp_Link when dp_Port == the menu's pr_MsgPort) or the FS-UAE debugger on the held task.
- Held task: `wasabi ps`: amarchy-menu STACK 262144 FREE ~258000 (not a stack issue).
- Logs: scratchpad/snoop_nvim.log, snoop_tmux.log, snoop_tmux2.log, snoop_tmux3.log, snoop_tmux4.log.

## Handler stray message (static, 2026-10-28; code reading only, no rig)

- bc6ecabf (close_gadget -> send_break CTRL_D, handler/vtcon_handler.c:5392) is NOT a message: brk_send (handler/brk.c:57) does Signal(task, sig) on the opener's task. No PutMsg to a client port. Cleared as direct cause (it only makes ixemul raise SIGHUP, ignored by 7c88fa0f).
- All PutMsg calls in the handler (:627, :1067, :5994 etc.) go to the handler's own worker processes, never a client. The only way the handler touches a client's pr_MsgPort is ReplyPkt (reply(), :489).
- Suspect (unverified at runtime): LATE/UNSOLICITED REPLY of a held WAIT_CHAR. An ixemul select sends WAIT_CHAR asynchronously and may return (timeout/EINTR from the CTRL_D signal) while the packet is still held. The handler later replies it: (a) finish_waitchar(c, DOSFALSE) at console teardown, :7516 (window close of the OSD console), (b) finish_waitchar on input/eof (:3091, :3303, :3342), (c) the "newer WAIT_CHAR from same task ends the older" reply, :6944-6950, and the full-set eviction reply :6970. If the menu task is inside DoPkt (Open UP-Term:) when any of these lands, dos sees a packet it did not wait for: AN_AsyncPkt 0x07000004 (+0x80000000 deadend).
- Violated invariant: a packet is replied only while its client still waits for it; ixemul's select abandons packets, the handler replies them anyway. Introduced by f680e1bb / 842467dd (WAIT_CHAR held per task, replied on close with DOSFALSE, :7516); bc6ecabf makes it likelier (adds EINTR wake-up exactly at close).
- Open 4: Open("UP-Term:...") goes to the vtcon handler, same segment but a different process per window; the closing OSD window's process is not the one serving the new Open, so (a) happens on the OSD window's process teardown, replying to the menu task. Not verified.
- Confidence: mechanism = plausible (code reading), exact packet = guess until a rig log shows the dp_Port/task of the stray reply.
- Fix direction: at close (:7516) and wherever a held WAIT_CHAR is replied, reply only if the waiting task is still in that select, i.e. ixemul must cancel its WAIT_CHAR (ACTION_FLUSH/cancel) or wait for the reply before returning from select; handler side: do not reply held WAIT_CHARs of a task that has since sent another DOS packet. Cheapest proof: DBG-log dp_Port->mp_SigTask in reply() for WAIT_CHAR and compare with the crashing task.

## Reply log (rig 7, DEBUG=1 handler with reply() logging dp_Port + sigtask name; 1 run; log: scratchpad/vtcon_ram3.log)
- Crash reproduced (window list: "Software Failure"; menu task 0x10856050 still in wait). Requester text not read (screen_text blank).
- Menu task has TWO reply ports: ixemul's own async port 107a61f0 and pr_MsgPort 108560ac. WAIT_CHAR (type 0x14, 10 s timeout 0x989680) packets: 1087cb58 (port 107a61f0, held, replied ffffffff on key 0d just before the launch), and 10a3887c (port 108560ac = pr_MsgPort, 10 s timeout) which was HELD with no reply when the launch started and is not replied anywhere in the log.
- Launch: after the key reply: type 0x7655 (reply ffffffff to port 1079fe60), then Open 0x3ee (packet 10a38738) replied ffffffff to pr_MsgPort 108560ac, then "request 2". Log ends there; no stray reply to the menu task logged after the Open reply (log not capped: 381 lines, flushed from main loop; later replies could be unflushed).
- Verdict: UNCLEAR, not confirmed. Late reply of a held WAIT_CHAR not observed in the log. Observed instead: a WAIT_CHAR held on pr_MsgPort itself (10a3887c) is outstanding while the menu does DoPkt(Open); dos sees any non-Open packet arriving on pr_MsgPort as AN_AsyncPkt, so a held-packet reply (timeout 10 s, or teardown) is still the leading suspect; needs a log that survives (SERIAL file was never created by fs-uae; DEBUG buffer 2048 flushes only from the main loop) or a reply() trace written with RawPutChar to a working serial path.

## ixemul WAIT_CHAR source (static, ixemul-vtcon feature/ixemul-80 + pg8-text2 amarchy)
- __srwport = u.u_sync_mp, __selport = u.u_select_mp (library/ixemul.h:415-416). Both are created in library/ix_open.c:158-159 with ix_create_port(0,0) -> createport.c CreatePort(): fresh ports, own signal bits. Neither is or aliases pr_MsgPort. No pr_MsgPort use in ixemul except vfork.c and ix_startup.c (WBStartup / vfork_msg waits).
- Every ixemul WAIT_CHAR sender: __fselect.c:303-306 (dp_Port=__selport, 10 s, the select packet), __close.c:166 (dp_Port=__srwport, timeout 0, sync, waited), WaitForChar(fh,0) at __read.c:301 and __fioctl.c:112 and __fselect.c:348 (dos.library, timeout 0, synchronous). None targets pr_MsgPort with 10 s. The 107a61f0 packet 1087cb58 is the __fselect.c:303 one (replied normally).
- Menu (amarchy/*.c, termenv.c, menu.c:570-620, leaf.c:57-84): no WaitForChar, select, poll, tcsetattr; termenv_ensure is GetVar/SetVar only. menu.c:583-586 dup2/close(fd) run in the ui path, not before leaf.c's Open.
- Result: the sender of 10a3887c is NOT in current ixemul source or in amarchy. Unproven candidates: (a) the rig's ixemul.library is not this tree (memory: build/rig/ixemul.library stale, 29 Sep; ixemul 48 selected on a different port) - check the rig's ixemul version and md5 against the build; (b) another linked library calling dos WaitForChar(fh, 10000000) (vsh.c:1380 read -t, claude/io_amiga.c:137 do, with other timeouts); dos WaitForChar replies via pr_MsgPort by design.
- Violated invariant (if confirmed): a packet whose reply port is pr_MsgPort must be synchronous (answered before the process issues the next DoPkt); async packets need a private port.
- Fix location proposal: ixemul (select must use u_select_mp, as __fselect.c:303 does) if the rig lib is older or a path leaks the port; handler-side defence: ws_drop_task keys on dp_Port->mp_SigTask (vtcon_handler.c:6945), so a later same-task WAIT_CHAR/close already answers the old one; the handler could also answer any held WAIT_CHAR whose port is the task's pr_MsgPort when a non-WAIT_CHAR packet from that task arrives. Do not pick before the rig lib version is known.
- Confidence: ixemul/menu negative = code-proven (grep of all dp_Port/WAIT_CHAR sites); identity of the real sender = guess.

## Sender trace (rig 7, 2026-10-28, DEBUG=1 handler with synchronous log writes; log: scratchpad/trace7.log, 485 lines, complete)
- Deps: rig LIBS: ixemul.library = md5 605b42c6122781ad66ac3881a193ce7a, 201140 bytes, Version FULL "ixemul.library 80.1". Identical to /Users/spot/Code/UP-Term/ixemul-vtcon build295/library/68020/68881/amigaos/ixemul.library (feature/ixemul-80, d1fa642). build/rig/ixemul.library does not exist; build/rig/os32/Libs/ixemul.library (f54fe081..., 193752, 7 Oct) is NOT what loads. So the library is current and (a) of the earlier candidates is out.
- Linked libs: libupkgui.a, libixcompat.a, libncursesw.a contain no WaitForChar symbol (strings count 0); no WaitForChar/WAIT_CHAR in the upkgui sources.
- Trace (log written per line with a packet on a private reply port, so it survives the crash): the menu task is "Background CLI" 10857058, pr_MsgPort 108570b4 (isPrMsgPort=1 confirmed). Every WAIT_CHAR on pr_MsgPort in the run (22 of them, pkts 10a3a224 / 10a3a330 alternating) has timeout 0 and is replied DOSFALSE at once. All 10 WAIT_CHAR with timeout 10 s (0x989680) have port 107a69f0 (ixemul select port), none on pr_MsgPort. The last 10 s one is replied ffffffff on key 0d, then READ, then Open 0x3ee (new window process), reply 0x3ee ffffffff, "request 2"; log ends. No WAIT_CHAR is held on pr_MsgPort when the Open runs, and the handler sends nothing to the menu task after the Open reply (reply() logs every non-READ/WRITE reply).
- Crash reproduced (1 run): requester "Software Failure", task Rushhours:Tools/UP-Term/bin/amarchy-menu, buttons Suspend|Reboot (text is custom drawn; uitree shows only the title).
- Verdict: REFUTED for the held-10-s-on-pr_MsgPort hypothesis. The earlier "10a3887c" pairing was probably a misread of the "packet" line (prints Arg1 of the packet before its recv line) against the wrong recv. The stray packet is not sent by the vtcon handler.

## Post-Open trace (static, 2026-10-28, worktree pg8-text2; rig 7 repro attempt aborted, no result)
- Verdict: UNCLEAR. No sender in the vtcon handler or amarchy sources can put a message on the menu's pr_MsgPort after the Open.
- Every handler PutMsg/ReplyMsg/ReplyPkt site was listed (grep over handler/): PutMsg(&p->pr_MsgPort) at vtcon_handler.c:627 (png worker), :1090/1110/1121/2334 (watch worker), :5994 (start_worker), complete.c:671 (warm_worker) all target Processes the handler itself created with CreateNewProcTags, never the opener. ReplyMsg/PutMsg on tab_port/host_pub/req_port go to other UP-Term windows or the handler's own ports. brk.c only Signal()s the opener (brk_open stores dp_Port of the first Open, vtcon_handler.c:6669; brk_send does Signal only).
- The only ReplyPkt outside reply() (vtcon_handler.c:489) is the startup packet at handler_main (~:7320, ReplyPkt(p, DOSTRUE, 0)): it is sent to the dp_Port of the startup packet, i.e. the opener's pr_MsgPort, as dos expects (dos waits for it inside Open). It is replied once and p is not reused. It is not logged by reply(), which explains why the earlier trace shows only the Open reply; it is not stray.
- "request 2" (vtcon_handler.c:5498, req_service) is a quick-menu upreq request taken from the window's req_port; upreq_send (upreq.c:46-69) answers on a private CreateMsgPort reply port, not pr_MsgPort. The menu code has no PutMsg/WaitPort/GetMsg on pr_MsgPort (amarchy/*.c grep).
- By elimination the stray message is produced inside dos.library's SystemTags(SYS_Input=w, SYS_Output=0, SYS_Asynch) path or by the handler-start sequence in dos (Open on an unstarted device node: startup packet, then the Open). Not provable from source: SYS_Output=0 makes dos duplicate/open an output itself (packet to w's handler or to the menu's console), and the new async CLI process is created by dos in the menu's context.
- Next probe (not done): wasabi snoop on task 'Background CLI*' with --entry during the Tmux pick, to see the last DOS call before the alert (Open("*") / DupLockFromFH / ACTION_* on w); or a one-line test passing SYS_Output=<Open("NIL:")> or the same w dup (Open of a second handle on the same window) to see whether the alert disappears. Candidate root-cause fix location if SYS_Output=0 is the trigger: amarchy/leaf.c:75 (give SYS_Output an explicit handle, as handler/vtcon_handler.c:6010 does with NIL: for SYS_Input and leaf.c's own sibling run paths).
- Rig 7 attempt: menu opened (Left Amiga+Space), typing "tmux" + Return did not open a window and wasabi snoop wrote no log; stopped without a repro. No source edits made.

## SYS_Output A/B (rig 7, 2026-10-28, worktree pg8-text2 @ e6ccdb9b; NOT REPRODUCED, no B run)
- Run A1 (drawn GUI menu via amarchy_gui_rig setup + case_run_tmux, Apps > Tmux by walk_to rows): no crash, `tmux ls` rc 0, UP-Term window present, no Guru (T:lastguru absent).
- Run A2 (text menu: Left Amiga+Space in a host terminal, 5 s wait, Return on Apps, Down, Return on Tmux): menu closed, `vsh job (tmux)` and `vfork()'d process (tmux)` running (stack 262144), no alert, no Guru. Snoop on 'Background CLI*' (build/rig7/wasabi/snoop.log) recorded only GetVar("echo") fail / FindTask("(null)") repeats with "snoop events lost" notes; it shows no Open/SystemTags/packet call near the pick, so it does not name the DOS call in which the alert would hit.
- Verdict: crash with SYS_Output=0: NOT seen in 2 runs on this build (amarchy restarted by setup, not "started at boot" as in the original repro: that difference is untested). Crash with explicit handle: not tested, since an A/B needs a crashing A. amarchy/leaf.c unchanged, no commit.
- Next: reproduce on a cold boot with Amarchy from the boot sequence (rig.py start, no setup, no restart of Amarchy), then A; only then B (second Open of the same spec as SYS_Output, closed on rc == -1).

## Cold-boot rerun (rig 7, 2026-10-28, worktree pg8-text2)
- Setup: `UPTERM_RIG=7 rig.py start --max` only; Amarchy started by the boot sequence (ps: Background CLI UP-Term:bin/Amarchy, amarchy-index), no amarchy_gui_rig setup, no restart. First boot attempt logged "[ERROR] no wasabid answer ... retrying the boot", then up. No [WARN] drift line.
- Run (ab_text.py: Left Amiga+Space, 5 s, Return on Apps, Down, Return on Tmux): CRASH. Requester "Software Failure" on Workbench Screen, task "Rushhours:Tools/UP-Term/bin/amarchy-menu" (menu task name "Background CLI (Rushhours:Tools/UP-Term/bin/amarchy-menu)"), buttons Suspend | Reboot. No tmux/vsh job started; T:lastguru absent. Log: scratchpad cold_run1.log.
- So the earlier no-crash runs differ by Amarchy being restarted by setup; boot-started Amarchy still crashes with current VTC: programs.
- B run (leaf.c run_in_uterm: SYS_Output = second Open of the same spec, both closed on rc == -1; amarchy-menu rebuilt, 479732 bytes confirmed on the rig by List; rig stopped and cold-booted again): CRASH again, identical requester (Software Failure, amarchy-menu, Suspend|Reboot). Explicit SYS_Output handle does not fix it. leaf.c edit reverted, nothing committed.

## Kit vs build

Rig 7, cold boot, purpose "vtcon-a1 Amarchy s11: kit vs build amarchy-menu crash". Start output had no [WARN] drift line.

md5 on the rig versus the worktree's build/amiga (pg8-text2) and VTCX:Apps/bin:

| file | kit (Rushhours:Tools/UP-Term/bin) | VTCX:Apps/bin | worktree build/amiga |
|---|---|---|---|
| Amarchy | a54637fe3348c180d94c274bf1f2f0f3 | same | same |
| amarchy-menu | 6ce50cfd40e466a51814ec8d491165e5 | same | same |
| amarchy-gui | 733b4cc21aae58f4e0b3785e5c51514a | same | same |
| amarchy-index | 279de05508056887ebf6b2d463c53c6d | same | same |
| LIBS:ixemul.library | 605b42c6122781ad66ac3881a193ce7a | - | same as build/dist/up-term/Files/libs |

Note: the MAIN checkout's build/amiga (/Users/spot/Code/UP-Term/vtcon/build/amiga) is older (Amarchy 7f7873ef..., amarchy-menu 9969ac4c...) and differs; the worktree's build is what the rig carries. Anyone comparing against the main checkout sees a false "stale kit".

Running processes after boot: UP-Term:bin/Amarchy and UP-Term:bin/amarchy-index (both from the startup-sequence).

Run A1 (as-is, boot-started Amarchy, ab_text.py): CRASH. Requester: title "Software Failure", task "Rushhours:Tools/UP-Term/bin/amarchy-menu", buttons Suspend, Reboot. T:lastguru absent.

Run B (kill boot Amarchy, Run >NIL: UP-Term:bin/Amarchy from wasabi): NOT DONE. The second cold boot of rig 7 reached Workbench but wasabid never answered (rig.py status: down, connection refused) after more than 30 minutes; run aborted.

Verdict: binaries are identical, so STALE-KIT is ruled out. START-PATH vs NEITHER is undecided (case B not run).

## Case B (wasabi-started)

Rig 7, cold boot (start came up after ~6 min), purpose "vtcon-a1 Amarchy s11: menu crash case B (wasabi-started Amarchy)".

Process notes (Status FULL):
- Boot-started Amarchy (S:User-Startup `Run >NIL: UP-Term:bin/Amarchy`): stk 4096, pri 0, "Loaded as command: UP-Term:bin/Amarchy". amarchy-index likewise stk 4096 pri 0.
- Wasabi-started (Break boot Amarchy, confirmed gone, then `Run >NIL: UP-Term:bin/Amarchy` via condev_rig.run): stk 16384, pri 1 (inherits wasabid's stack and priority). Its children amarchy-gui and amarchy-menu show as "Loaded as command: Rushhours:Tools/UP-Term/bin/amarchy-gui|amarchy-menu", both stk 262144, pri 1 (boot case pri 0 expected, not captured for children). The process list shows full command names, not "Background CLI" labels.

Run B1: ab_text.py first attempt: menu did not open within 15 s (hotkey timing, "menu open False"); keys went to the shell, no crash. Discarded as inconclusive. Re-driven by hand: open menu OK, Enter on Apps, Down, Enter on Tmux: new UP-Term windows opened, window list shows no Software Failure requester, T:lastguru absent. Verdict: NO CRASH.

Run B2: not done.

Verdict: START-PATH. Boot Amarchy runs with stk 4096/pri 0 and crashes every time; wasabi-started (stk 16384/pri 1) did not crash (now 3 runs total incl. 2 earlier). Next: start Amarchy at boot with Stack 16384 / pri 1 (or Run with the same stack) and re-test.

## Case C (stack vs priority)

Run inline from the main session (the sonnet agent died on the weekly limit). Rig 7, cold boot, worktree pg8-text2 @ 9de2df81.

Boot Status FULL before any hotkey: only `UP-Term:bin/Amarchy` (stk 4096, pri 0) is listed; amarchy-menu and amarchy-gui do not exist yet, so the children start on first use, not at boot.

C1, priority 0 with stack 16384: Break the boot Amarchy, then `Run >NIL: Execute RAM:c1` where c1 is `ChangeTaskPri 0` + `Run >NIL: UP-Term:bin/Amarchy`. Status FULL: Amarchy stk 16384 pri 0; amarchy-gui and amarchy-menu start at once, both stk 262144 pri 0. ab_text.py: menu opened, Apps > Tmux: NO CRASH; tmux, VTC:vsh and -vsh processes run, amarchy-menu still alive, T:lastguru absent. Verdict: priority is ruled out.

C2, stack 4096 with priority 1 (`Stack 4096` + Run in an Execute script): Status FULL showed Amarchy stk 4096 pri 1, children stk 262144 pri 1. The first drive failed on a leftover 'menuhost' window from C1 whose shell was dead (the window could not be activated), so the run was not done. Rig 7 restarted for a clean C2 from boot.

### Case C3: cold boot, first press (rig 7, 2026-10-28)

- After a cold boot, `wasabi ps` again shows only `Background CLI (UP-Term:bin/Amarchy)` (stack 16384, pri 0). amarchy-menu and amarchy-gui are not running. This was seen on 2 of 2 boots.
- First press, Apps > Tmux: the menu opens on the cold path (act_in_window). After Return on Tmux, a "Software Failure" requester is up. `T:lastguru` stays absent until the requester is answered. Suspend removed the crashed task from the list: CLIs 6 and 8 are gone, and which one crashed is not known.
- ms_open's throttled restart then starts amarchy-menu serve (CLI 2, stack 262144). amarchy-gui is NOT restarted, because only the pressed style restarts.
- Second and third presses through that warm server: the driver's `x9_open_menu` returns False. No menu window opens, and the keys go to the menuhost vsh. Not yet explained. Possible causes: the warm server is wedged by the suspended crashed task, or a server started from the boot process is broken.
- Comparison: on a wasabi-started Amarchy (both servers spawned at start), the same run: row works.
- Why the warm path is blocked after boot is still not known. Confirm the crash is cold-path only with a non-boot Amarchy whose menu server is removed. Break cannot do that: serve clears Ctrl-C at menu.c:639. Use an MS_QUIT message, or a fresh Amarchy start with the server binary missing.

Open questions for the next session:
1. Why are the servers missing after boot? act_menu_server_start runs at amarchy.c:321 and pl_start returns a value that is ignored. Test this: does SystemTags fail from the boot process, or does serve exit at once? An exit with code 5 means it found a live port. Log pl_start's rc and the serve exit code to a T: file.
2. Which process gets AN_AsyncPkt: the cold amarchy-menu or the run: child? Read the requester body (screenshot via a haiku subagent) before answering it.
3. Why does the restarted warm server open no menu?

Driver gotcha: the boot "UP-Term" window (0,20 640x300) fully covers 'menuhost' (0,20 600x200). close_vsh in vshwin.py clicks the centre, which hits UP-Term, and then refuses. Click UP-Term's sys:depth gadget (629,27) first. Tool fix pending: close_vsh should send covering windows back.

### Answers and fix (session 12, rig 7, 2026-10-28)

1. Missing warm servers after boot: a rig-only race, not an Amarchy bug. The rig GO script's `shadows_up` (tools/rig/rig.py, after `C:Wait 15` and the ENV wait) copies the build's Amarchy, amarchy-menu, amarchy-gui and amarchy-index over UP-Term:bin while the boot User-Startup's `Run >NIL: UP-Term:bin/Amarchy` starts the servers. Fresh `rig.py start`: no AMARCHY.MENUSRV / AMARCHY.GUISRV on 3 of 3 starts this session (the port list checked with `SYS:Rexxc/rx "say show('P')"` after 150 s). `rig.py reboot` (same disk, files already current): both servers up. With the User-Startup line delayed (`C:Wait 45` first) both servers came up too. That the LoadSeg reads a file mid-Copy is inferred, not proven. Starting `amarchy-menu serve` by hand registers the port at once. Ledger row RIG-WARM-SRV-RACE.
2. Which task crashed: the cold menu, `amarchy-menu menu` (the path ms_open takes when no server answers, and when the binary stamp changed). Forced deterministically with `SetDate UP-Term:bin/amarchy-menu`. Cause: the cold menu draws in a CLOSEINACTIVE XCON console; the leaf's UP-Term window (leaf.c run_in_uterm) closes that console, ixemul raises SIGHUP on the console's CTRL_D (ixemul-vtcon library/__vtcon.c _psignalgrp), and main() kept the default action, so the process died inside the launch's DOS call. The packet reply then reached a dead process: AN_AsyncPkt (0x87000004). The warm server had ignored SIGHUP since 7c88fa0f; the cold path never did.
3. No menu on the next presses: the suspended crashed cold menu still owned AMARCHY.MENU, so the hotkey went to mo_close_open (amarchy.c), which only Signals CTRL_C to it and returns. Gone with the fix: the rig row's second hotkey opens the menu.

Fix: amarchy/menu.c main() calls `menu_ignore_hangup()` (serve_sig.h, renamed from serve_ignore_hangup; comment now covers both processes) before run_ui. Regression test: rig row `cold_launch` in tools/rig/amarchy_menu_rig.py (SetDate forces the cold path, Apps > Tmux, asserts no system requester, a new UP-Term window, and the next hotkey opens the menu). With the call removed it fails with the "Software Failure" requester naming Rushhours:Tools/UP-Term/bin/amarchy-menu; with it, it passes. Host test test_menu_ignores_hangup covers the helper.

Side observation (unverified cause): after `rig.py reboot` of rig 7 the wasabid connection died within a minute of the servers coming up and never came back; a stop + start fixed it. Noted under RIG-WARM-SRV-RACE.
