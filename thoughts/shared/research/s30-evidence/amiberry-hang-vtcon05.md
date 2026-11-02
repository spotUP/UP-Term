---
date: 2026-11-02
topic: Amiberry 8.2.0 (installed /Applications/Amiberry.app) OS 3.1 rig hang, macOS sample of the frozen emulator
author: vtcon-05 session 49 subagent
---

# Step 10: rig 37, Amiberry 8.2.0, OS 3.1 (default --max), sampled at "wasabid started"

## Run
- `UPTERM_RIG=37 rig.py start --max --emu amiberry --purpose "vtcon-05 session 49: Amiberry 8.2.0 OS 3.1 hang sample"`, 09:20:55.
- boot.log reached every line through "wasabid started" at 09:26:00 (go started, kit shadows, early shadows,
  wait done, env ok, XCON mounted, assigns done, wasabid started).
- Amiberry pid 45490: `/Applications/Amiberry.app/Contents/MacOS/Amiberry -G --log -f .../build/rig37/win/vtcon rig 37 ... .uae`.
- sample a 09:26:00-09:26:09, sample b 09:26:44-09:26:53 (5 s each). %CPU: 5.9 (09:26:00), 27.9, 5.0, 1.6 (09:28:37), 31.8 / 56.2 while ami.py retried.

## HANG: YES
- rig.py start's own check: `ERR: the Amiga stopped answering while Assign was running (nothing for 25 s, and a ping got no reply)`;
  wasabi `_fill: self.sock.recv(65536) TimeoutError` (TCP connected, request sent, no reply).
- Later `ami.py ping` (09:28+): `socket.create_connection ... TimeoutError: [Errno 60] Operation timed out` -- now even the TCP
  connect fails (listen backlog full: the guest no longer accepts). `exec "Assign LIST"`: no output, timed out.

## Sample a (right after "wasabid started"), per thread (Amiberry frames, 3309 samples)
- Main thread: `m68k_go -> m68k_run_jit 2690 -> exec_nostats 1303 / op_4e72_40_ff (STOP) 632 -> do_cycles_slow -> do_cycles_normal
  -> sync_equalline_handler / custom_trigger_start / draw_line_fast / hsync_handler; uae_time -> clock_gettime`. Also cia_bget 165,
  doint, intlev, MakeFromSR_x, op_4e73 (RTE) present. CPU is emulating; guest mostly in STOP (idle), interrupts serviced.
- filesys #1 (busy): `filesys_thread 2530 -> action_examine_next 1865 -> lookup_child_aino_for_exnext 1849 ->
  custom_fsdb_lookup_aino_nname 1665 -> fsdb_read_uaem 1432 -> fopen -> __open_nocancel`; fs_opendir 217; get_fileinfo 134.
  A guest process was ExNext-scanning a host directory (UAE fsdb .uaem lookup per entry) at that moment.
- filesys #2: idle, SDL_WaitSemaphoreTimeoutNS -> __psynch_cvwait 3308.
- bsdsocket #1 (Thread_110270622): idle 3309/3309 in `bsdlib_threadfunc -> SDL_WaitSemaphoreTimeoutNS -> __psynch_cvwait`.
- bsdsocket #2 (Thread_110272643): idle cond-wait 3005; `bsdthr_WaitSelect -> __select` 303; `bsdthr_blockingstuff ->
  bsdthr_Accept_2 -> write_log` 1. So the guest's WaitSelect DID reach the host, host select() returned, and the guest
  issued Accept: wasabid accepted rig.py's connection.
- denise, CVDisplayLink, ide, hardfile, extended-trap, NSEventThread, caulk, CoreAnalytics: idle waits (not detailed).

## Sample b (44 s later, 3902 samples)
- Main thread: `m68k_run_jit 3259 -> exec_nostats 1835 / 1172, op_4e72_40_ff (STOP) 1019 (1161 total), do_cycles_normal 1396,
  sync_equalline_handler 2410 total, hsync_handler 770 total`. Still advancing beam lines, still idling in STOP, doint/intlev/RTE present.
- filesys #1: idle now (action_examine_next 0). bsdsocket #1 and #2: both 3902/3902 in __psynch_cvwait (no WaitSelect, no
  Accept, no Recv/Send in flight).
- Same thread set as a, plus two short-lived worker threads.

## Reading
- The emulator is NOT frozen: the CPU thread advances in both samples (hsync/line handlers, interrupts and RTEs serviced).
  The guest is idle in STOP: every Amiga task is Wait()ing.
- Amiberry's bsdsocket emulation is NOT blocked: both host threads sit in their request cond-wait with no guest call pending.
  In a, one WaitSelect -> select() returned and one Accept ran; after that the guest made no further bsdsocket call
  (no recv of the request, no send of a reply), and in b nothing.
- So emulated time on the beam runs, but the guest tasks that should wake (wasabid after accept: recv / run Assign;
  C:Wait 15 in hunt 8) do not get their wake-up. Where emulated time stops is guest-side: a signal or timer.device
  wake-up that never arrives (consistent with hunt 8's C:Wait 15), not a host thread block. Next probe candidates
  (not run here): timer.device/CIA timer events on 3.1 under Amiberry 8.2.0 (e.g. guest `Date` twice, or a wasabid-free
  `Wait 1` echo marker in boot.log), and whether the filesys ExNext scan seen in a (what guest process?) ends before the stall.
- Unverified: which guest task did the ExNext scan; whether it was Workbench/Amarchy or the rig's shadow copy.

## Files
- Samples (scratchpad, small, kept): sample-822-a.txt 0.5 MB, sample-822-b.txt 0.4 MB in
  /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/70c37d5e-2f18-48ee-94ac-8832cdfd1d3d/scratchpad/
- build/rig37 left in place, 2.4 GB; rig stopped (rig.py stop), Amiberry pid 45490 gone.

## Step 10b (8.5.0)

Run: UPTERM_AMIBERRY=<scratchpad>/amiberry-850/app/Amiberry.app UPTERM_RIG=48 rig.py start --max --emu amiberry. ps args confirmed the 8.5.0 binary ran (rig 48 only).

- Boot 1: HANG. Emulator up 2.5+ min at 0.1% CPU, wasabid TCP port refused (ami.py ping: ConnectionRefusedError), rig.py start never returned. ami.py steps (Assign/Version/List) not runnable. Same symptom as 8.2.0.
- Boot 2: not run (boot 1 hung).
- Config keys rejected by 8.5.0: none found in the Amiberry log (only "Failed to set size of shared memory: Invalid argument", no key warnings).
- sample (5 s, 4288 samples, file: scratchpad/sample-850.txt): main thread 100% in
  m68k_go > do_cycles_slow > do_cck > custom_trigger_start > hsync_handler > trigger_vsync_line >
  inputdevice_read_msg > handle_msgpump > process_event > amiberry_inactive (window focus lost) >
  SDL_SYS_SetThreadPriority > pthread_setschedparam > thread_policy > mach_msg2_trap.
  Other threads (denise, ide, filesys x2) idle in SDL_WaitSemaphore / cond_wait. So the emulation loop is blocked on the main thread inside a mach thread_policy call made from the focus-lost handler, not in the guest.
- Rig 48 stopped via rig.py stop; no leftover Amiberry for rig 48. build/rig48 kept.

## Step 11 (session 49, hunt 11): boot `Assign LIST` silence, static analysis only

Status: cause NOT confirmed. No repro run completed (budget spent; rig 51 setup copy of sys.hdf + vtc was still running after ~4 min and disk sat at 2.7 GB free, so it was stopped and cleared before boot).

Findings:
- The rig's `C:Assign` is v37.4 (`$VER: assign 37.4 (25.4.91)`, 3220 bytes), taken from `/Users/spot/Code/UP-Term/images/nyhd2.hdf` (paths.SYSTEM_HDF). Disassembled with m68k-amigaos-objdump.
- LIST path: `LockDosList(LDF_READ|...)` (LVO -654) at 0x4ac; the loop formats into an AllocMem buffer and calls `NameFromLock` (LVO -402, at 0x9ba via sub 0x93c) for every assign target WHILE the DosList lock is held; `NextDosEntry` (-690) at 0x670; `UnLockDosList` (-660) at 0x682; only after that does sub 0xa86 write the buffer with `WriteChars` (-942).
- So the earlier hypothesis "Assign blocks writing to wasabid's capture file while holding the DosList lock" is wrong: Assign writes nothing while the lock is held. It does send packets (NameFromLock: ACTION_EXAMINE_OBJECT / ACTION_PARENT) to each target's handler (DH0 FFS, uaehf VTCX/BOOTX, RAM) under the shared lock.
- wasabid exec path (wasabid.c rev b9d1812d, l.816-1030): runner opens `T:wasabi-run-N` MODE_READWRITE; the daemon task (the only bsdsocket user) keeps doing Read/ExamineFH/re-Open on T: while the command runs. Any wait in DOS device lookup (GetDeviceProc on "T:") stalls the daemon and with it every socket reply, which matches "the guest goes silent".

Working hypothesis (unverified): Assign holds DosList READ and waits on a NameFromLock packet to handler H (likely a uaehf volume, the Amiberry trace had the filesys thread in ExNext of a host directory). A concurrent Startup-Sequence task (Workbench/Amarchy launch: Assign/AddDosEntry/Mount) queues for DosList WRITE. If H, or the daemon's GetDeviceProc("T:"), needs DosList READ it queues behind the pending writer: three-way deadlock, timing-dependent, explains rig-to-rig variance.

Next step to confirm (one run): `UPTERM_FSUAE=/Users/spot/Code/Emulators/fs-uae-debug/fs-uae UPTERM_RIG=<n> rig.py start --max --emu fsuae --ram 256 --debugger`, `rig.py dbg g`, at the hang `rig.py dbgbreak` then `rig.py dbg T` (debugger.log): read each waiting task's PC and tc_SigWait; dump the DosInfo di_DevLock semaphore (owner, nest count, wait queue). Also discriminate Assign vs any exec: issue `ami.req(0x02, ..., b"Echo x")` at the same boot point.

Fix direction once confirmed: rig.py `boot_assigns_missing` stops using `Assign LIST` during live boot and checks each needed name with `Assign <NAME>: EXISTS` (no NameFromLock under the list lock), with a regression test in tools/rig tests that asserts the boot check never sends `Assign LIST`. No code changed and no branch made in hunt 11.

## Step 12 (session 49, hunt 12, rig 2 on loan from musicplayer-16) - cause NOT confirmed

Setup: `UPTERM_RIG=2 rig.py start --max --ram 256 --emu fsuae --debugger --idle-minutes 30` under the
debug fs-uae (`UPTERM_FSUAE=/Users/spot/Code/Emulators/fs-uae-debug/fs-uae`), OS 3.1, rig silent by default.

- Boot 1 (10:44): GOOD. "up", Assign check passed, watchdog started, `[WARN] the Workbench stayed PAL (W43)`.
- Boot b2 (10:48): FAILED, but not with the Assign-check message. Sequence in the start log:
  - `up` (status answered)
  - `Exception in thread Thread-1 (_watch): ... TimeoutError: timed out` - this is ami.py's
    `_RequesterWatch` thread (second wasabid connection, polls the UI walk while `Assign LIST` runs).
  - `[WARN] S:Shell-Startup not read: auth failed: the Amiga took the connection but its daemon did not
    answer for 150 s - frozen, or stuck on something`
  - `auth failed: ... did not answer for 150 s` again from the IPrefs/WB retry step; rc=1.
  So wasabid stopped answering during the boot `Assign LIST` exec, with the requester-watch UI walk
  running on its second connection at the same moment. The Assign check itself did not report; the
  daemon was already frozen when the next step connected.
- The hunt script matched only "stopped answering", so it classified b2 as GOOD and started b3,
  which stopped rig 2: the frozen state was lost before `dbgbreak` / `dbg T`. No task dump exists.
  DosList semaphore owner / wait queue: NOT read.
- Echo x probe (step 2): not run.

Lead for the next hunt (unverified): the freeze needs wasabid's UI walk (`mui3_prescan` ->
`mui3_open` OpenLibrary, LockIBase walk, Forbid RAM scan, `LockDosList(LDF_VOLUMES|LDF_READ)`)
overlapping a running exec job. Cheap discriminator: boot with `UPTERM_REQUESTER_WATCH=0`; if the
freeze never shows in ~6 boots while it shows ~1 in 2 with the watch on, the deadlock is inside
wasabid's UI walk, not DOS/Assign. Fix the hunt script to break on `auth failed|did not answer|ERR`
before anything else runs (script: session scratchpad hunt.sh).

## Step 12b (session 49, hunt 12b, rig 2) - requester watch OFF still freezes: lead WRONG

- UPTERM_REQUESTER_WATCH=0: 1 boot run, 1 FROZEN, 0 clean (stopped at the first freeze, per brief; 5 more not run). Watch ON: not run (not needed once the watch-off boot froze).
- Frozen boot start log ends in a wasabi socket TimeoutError (build/wasabi/wasabi line 495 recv), rc=1. The patterns "did not answer"/"auth failed" did not appear in the log tail.
- Debugger break (rig.py dbgbreak) landed at PC 40030D0E inside 68060.library (BT.B loop), SR STP=1 IMASK=0: the CPU is idle in STOP, not spinning. So the Amiga side is idle/all tasks waiting, not a CPU livelock.
- NOT obtained: a blocked-task list. The debugger's T/Tl commands here trace and list libraries; no exec task list was printed (a task dump needs another debugger command or a wasabi-side listing). Full dump: /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/70c37d5e-2f18-48ee-94ac-8832cdfd1d3d/scratchpad/dbg-w0-1.log (4.9 KB). Verdicts: /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/70c37d5e-2f18-48ee-94ac-8832cdfd1d3d/scratchpad/verdicts.txt.
- Hunt script: /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/70c37d5e-2f18-48ee-94ac-8832cdfd1d3d/scratchpad/hunt2.sh (fixed match pattern) + loop.sh.

## Step 13 (session 49, hunt 13, rig 2 fs-uae debug, OS 3.1, 060, 256 MB) - the holder is IPrefs, stuck in WaitTOF

Repro: `hunt2.sh w0-2 0` (requester watch OFF) FROZE on boot 1 of 1 (rc=1; retry_wb_reset's `ami.req(0x0A)` = wasabi SCREEN
list timed out). Debugger stopped the frozen machine; `T` printed nothing again, `Tt` works (fs-uae debug.cpp:4935
`show_exec_tasks`). Raw output in the session scratchpad: tasks-w0-2.txt, stacks-w0-2.txt, sems-w0-2.txt,
custom-w0-2.txt, vertb-chain-w0-2.txt, bp-vbsrv-w0-2.txt, vbsrv-dis-w0-2.txt (dir /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/70c37d5e-2f18-48ee-94ac-8832cdfd1d3d/scratchpad).

Ready list EMPTY; every task waits. Waiting on signal $10 (SIGF_SINGLE/semaphore): wasabid ($40124280, CLI 9
BOOTX:wasabid), input.device ($4000985A), Amarchy ($402CF980, CLI 5), IPrefs ($400F6408), dopus ($407CB198), ramlib.

Decoded from the stacks (exec ObtainSemaphore return $F82E7A, Wait return $F828DA) and the semaphores' fields:

| Object | Owner (+$28) | Queue (in order) |
|---|---|---|
| SignalSemaphore $4000B86E = IntuitionBase($4000B04C)+$822, nest 1, QueueCount 2 | IPrefs $400F6408 | input.device, wasabid |
| SignalSemaphore $4000B926 = IntuitionBase+$8DA (next-but-3 ISemaphore, stride $2E) | IPrefs $400F6408 | Amarchy |
| List $40004C0C = GfxBase($40004B4C)+$C0 = TOF_WaitQ (nodes ln_Name=task, pri 4) | - | dopus, IPrefs |

IPrefs' Wait returns to $F87344, i.e. graphics WaitTOF (`move.l #$10,d0; jsr Wait(a6)` at $F8733E, then unlinks
the TOF node). So: **IPrefs holds two Intuition-private locks (one is the LockIBase lock wasabid and input.device
want) and sleeps in WaitTOF(), which never returns.** wasabid is a victim: it blocks in LockIBase(0) in
`cmd_screenctl` (build/wasabi/src/wasabid.c:5783). input.device blocked on the same lock = mouse and keyboard
dead for any user, with or without wasabid.

Why WaitTOF never returns (NOT resolved): INTENA=$682C (VERTB on, master on), INTREQ=$0041 (no stuck VERTB), CPU
in STOP with IMASK=0. The VERTB server chain (ExecBase IntVects[5] list $40000030): $40004B96 (graphics vbsrv,
pri 10, first) -> $4088AB5A -> $40009488 -> $400095CA -> $400040E0 (pri -10). graphics' vbsrv is_Code is a RAM
patch at $4009E054 (not the ROM server) and a breakpoint there HITS right after `g`, so VBlank runs; yet
nothing signals TOF_WaitQ. The first 45 instructions (to $4009E0FA) handle GfxBase+$17C (monitor/RTG-ish state),
$1C8/$1DC beam registers; the TOF_WaitQ walk was not reached. Lead: the patched server skips the TOF signal in the
state IPrefs' RTG mode switch leaves (GfxBase+$9F bit 2 / $A5 / $EC tests at $4009E0E8..). Owner of the patch
($4009E054: SetPatch, rtg.library/Picasso96 or cybergraphics) not identified.

Secondary wasabid defect (not the cause here): `cmd_screenctl` (wasabid.c:5783-5802) calls send_frame() (socket I/O)
while holding LockIBase, unlike WINDOWS (wasabid.c:2591, walks into a buffer). Fix upstream in wasabi: buffer, unlock,
then send.

Next: disassemble $4009E0FA.. to the GfxBase+$C0 walk and name the skip condition; find which module allocated
$4009E054 (memory-walk the segment lists or compare with a boot without IPrefs' RTG mode / without P96).

## Step 14 (vtcon-05 session 49, rig 2, boot h14a): who owns the VBlank patch, and is TOF really not woken

**Patch owner.** The replacement VERTB server comes from Picasso96 `Libs:Picasso96/rtg.library` 43.787 (6.6.2026, 231356 bytes) on rig 2's sys.hdf. The server's head bytes `41F900DFF00048E728000C6900280014` sit at file offset 0xBEF8 of rtg.library. They are absent from SetPatch 40.16, LoadModule, 68060/680x0.library, Picasso96API.library, the uaegfx monitor and emulation.library. rtg.library loads at a different address each boot: $4009E054 earlier, $400A05B4 in h14a (graphics VERTB node $40004B96 is_Code). Our startup files (User-Startup: DOpus5, MUI, KingCON, AmiSSL, upgetty, UPConsole, shadows-up, Amarchy) install no VBlank or graphics patch. The patch arrives through DEVS:Monitors/uaegfx, which opens rtg.library.

**Step 13 premise was wrong: TOF_WaitQ IS woken.** Static disassembly shows the rtg.library server walks GfxBase+$C0 (TOF_WaitQ) unconditionally and Signal()s each waiter with 1<<ln_Pri. In frozen boot h14a, a breakpoint at the walk ($400A07AA) hits. The Signal call ($400A07BE) hits every frame with A1=$407CC798 (dopus) and A1=$400F56E0 (IPrefs), D0=$14. Step 13's "no hit at $4009E24A" was a stale address from a different boot. The custom chip state is normal (INTENA $682C, VERTB enabled).

**What IPrefs really does: a WaitTOF busy loop in intuition ROM.** IPrefs (task $400F56E0) stack: WaitTOF, then rtg.library WaitTOF patch (if the front screen is a P96 screen it waits on board VSync, else it calls the original WaitTOF), then $00FE822E, then return $00FD13BE in intuition 40.85 ROM. The loop at $00FD13A6:

```
00FD13A6 JSR $00fe7a40        ; lock
00FD13AC TST.W ($0b3a,A3)     ; A3 = IntuitionBase
00FD13B0 BEQ $00fd13c0        ; zero: take it (ADDQ.W #1,$b3a), go on
00FD13B2 JSR $00fe7ad4        ; unlock
00FD13B8 JSR $00fe8224        ; WaitTOF
00FD13BE BRA $00fd13a6        ; try again next frame
```

IPrefs waits one frame at a time for the word IntuitionBase+$B3A to drop to zero. The word stays nonzero, so IPrefs loops forever while it holds the IntuitionBase+$822 and +$8DA semaphores (Step 13). Every task that later needs those semaphores (wasabid screenctl, Workbench) blocks. The word is a busy/in-use count. Some other path incremented it during the RTG screen-mode switch and never decremented it. The holder is NOT identified (budget ran out). Next probe: read IntuitionBase+$B3A on a frozen boot, then set a write watchpoint on that word from cold boot to catch the increment without a matching decrement.

**Layer.** Third party: intuition 40.85 (OS 3.1 ROM) plus P96 rtg.library 43.787 during IPrefs' boot-time switch of Workbench to an RTG mode. No code of ours is in the path. No fix branch is made. wasabid cmd_screenctl holding LockIBase across send_frame (Step 13) stays a separate, secondary wasabid defect: it makes wasabid hang with the system instead of reporting.

**Mitigation (proposal only, not done).** On the rigs, keep the boot screen mode PAL for OS 3.1 (rig ENV:Sys/screenmode.prefs, or no uaegfx monitor at boot) and switch to RTG after boot when a test needs RTG. This removes the IPrefs mode switch, which is the only trigger seen (vtcon-16: 3.2 boots stayed PAL and never hung; their 3.1 boot switched to RTG). Report to P96 with the $FD13A6 loop and the $B3A word once the holder is known.

**Who can hit it.** Any OS 3.1 (intuition 40.85) system with P96 rtg.library 43.787 whose saved Workbench mode is an RTG mode, so IPrefs switches at boot. That includes Amiberry and FS-UAE users with uaegfx and 3.1. The A1200 runs OS 3.2 (different intuition); it is not seen there and is unverified. A real 060 with an RTG card on 3.1 could hit it in theory (unverified). The hang is intermittent (musicplayer-16 and vtcon-05 hit it on rig 2; vtcon-16 hit 0 of 5).

## Step 15 (vtcon-05 session 49, hunt 15): who raises IntuitionBase+$B3A - answered from the Step 13 frozen dump, rig 2 not started

**Method.** Static, no new boot. Disassembled the rig's Kickstart (`roms/kick40068-a1200.rom`, `m68k-elf-objdump -b binary -m m68k:68020 --adjust-vma=0xf80000`; output in the session scratchpad as kick31.dis) and re-read the frozen boot w0-2 stacks (scratchpad stacks-w0-2.txt, Step 13). The live write watchpoint was not run: the answer was already in the dump, and FS-UAE memwatch is unreliable with the JIT on (rig.py has no JIT-off switch except --mmu, which changes the machine).

**Every ROM access to IntuitionBase+$B3A (whole-ROM grep for offset 2874 / $0B3A on any register):**

```
fd13ac  4a6b 0b3a  tstw   %a3@(2874)      ; the spin test (Step 14 loop)
fd13c0  526b 0b3a  addqw  #1,%a3@(2874)   ; INCREMENT, after the lock at $FE7A40
fd1434  536b 0b3a  subqw  #1,%a3@(2874)   ; DECREMENT, function exit
```

All three sit in one intuition 40.85 function at $FD139C(d3 = screen type), called as $FD139C(1) from $FD2884 when a public-screen lookup by name ($FD2828) matches the ROM string "Workbench" ($FE8548) and that screen is not open. The path is LockPubScreen(name) (LVO stub $FCF9FA -> $FD25F8 -> $FD2828 -> $FD2888). Between the increment and the decrement it releases the screen lock ($FCFDAA at $FD1404) and, for d3 = 1, calls $FD23C6 (the internal Workbench open) at $FD142C. Nothing else in the ROM writes the word; rtg.library is not a writer.

**Who holds it (frozen boot w0-2, stack of Amarchy task $402CF980, dump at $402D5B94):**

```
402D5B94 4000 0810 00F8 28DA 00F8 2E7A 4000 B93A   ; Wait <- ObtainSemaphore
402D5BA4 4000 B936 402C F980 ...                     ; saved: task = Amarchy
402D5BB4 4000 B926 ...                               ; semaphore IntuitionBase+$8DA (IPrefs owns it, Step 13)
402D5BD4 00FC FD9E 0000 0004 00FD 1F9C ...           ; Workbench-open internals
402D5C20 .... 00FC FD68 00FD 23FE ...
402D5C54 00FD 1432 ...                               ; return from jsr $FD23C6 at $FD142C: INSIDE the $B3A section
402D5C64 4004 F3B8 00FD 288E 0000 0001               ; $FD139C(1) from the name lookup
402D5C74 .... 00FD 260A 4033 C5BC                    ; LockPubScreen(name = $4033C5BC)
402D5C84 4000 B04C 00FC FA02                         ; LVO stub
```

IPrefs ($400F6408, stack $400F6E38) and dopus ($407CB198, stack $407CD12C) both show `00FE 822E 4000 B04C 00FD 13BE ... 00FD 288E` (IPrefs also `00FD 260A`): both are in the $FD13A6 spin through the same LockPubScreen("Workbench") path.

| Writer PC | Instruction | Module | Task in w0-2 | Matched? |
|---|---|---|---|---|
| $FD13C0 | ADDQ.W #1,$B3A(A3) | intuition 40.85 ROM, LockPubScreen("Workbench") auto-open | Amarchy (CLI 5) | NO: Amarchy blocked at $FD142C -> $FD23C6 on IntuitionBase+$8DA |
| $FD1434 | SUBQ.W #1,$B3A(A3) | same function | never reached | - |
| $FD13AC | TST.W (read) | same function | IPrefs, dopus spin | - |

**Verdict: ABBA deadlock inside intuition 40.85, triggered by our Amarchy.** Amarchy calls LockPubScreen("Workbench") while IPrefs has the Workbench closed for its boot-time RTG mode switch. Intuition then reopens the Workbench for Amarchy: it raises $B3A, drops its own lock, and waits for IntuitionBase+$8DA. IPrefs holds $8DA (and $822) and, through its own LockPubScreen("Workbench"), waits one frame at a time for $B3A to fall. Neither can move. The ROM bug is the lock order; the trigger is a third task calling LockPubScreen("Workbench") inside IPrefs' close/reopen window.

**Our call site (inferred: Amarchy's return address into its own code is past the end of the dump).** amarchy/wbmode.c:38, `wb_mode_id()`: `LockPubScreen((STRPTR)"Workbench")`, called by `wbm_check()` (wbmode.c:51) at start (amarchy.c:577) and on every 1 s tick while the windows are held (amarchy.c:267, `wb_gate`, W43 commit d64bffe0). It is the only Amarchy Workbench lock that runs during boot by design: it polls exactly while IPrefs switches the mode. Same hazard, user-triggered only: gthemes_dos.c:49 and :157, strut_patch.c:115. amarchy/iconpen_dos.c:82-91 already avoids it (`wb_screen()` walks LockPubScreenList(), comment: "LockPubScreen("Workbench") is never used: it opens a closed Workbench").

**Fix (proposal, not made).** In wbmode.c `wb_mode_id()`, read the mode under LockPubScreenList() through the existing `wb_screen()` walk (move it from iconpen_dos.c to a shared helper, reuse from both), return WBM_INVALID_ID when the Workbench is closed so the gate keeps waiting. Then apply the same helper to gthemes_dos.c:49/157 and strut_patch.c:115. The list lock does not touch $B3A, so a closed Workbench means "wait", never "reopen". Regression proof: OS 3.1 rig boot with the RTG screenmode.prefs, repeated boots (the hang is intermittent, Step 14), plus a sentinel that wb_mode_id never calls LockPubScreen. PAL-first boot also avoids it (no IPrefs close/reopen) but is a mitigation only; any other program polling LockPubScreen("Workbench") at boot can still hit the ROM race, so keep our code off that call; the ROM 40.85 lock order itself cannot be fixed.

**Next.** Implement the wbmode.c change, then a live confirmation on rig 2: break at $FD13C0 and $FD1434 (instruction breakpoints work under the JIT, Step 14), log ThisTask per hit, expect no Amarchy hit at $FD13C0 after the fix.
