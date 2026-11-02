---
date: 2026-11-02
topic: Amiberry 8.2.0 rig hang at boot "Assign" step
status: root cause narrowed, not fixed
branch: s48-amiberry-default
---

# Amiberry rig hang: not a DOS handler, it is wasabid's UI-tree read

## Verdict

The premise "a DOS handler fails to answer `Assign LIST`" is wrong. Every DOS
handler and every assign answers under Amiberry 8.2.0. The hang comes from the
`ami.py` requester watcher, which opens a second wasabi connection at the start
of every `exec` and calls `read_ui(every_screen=True)`. Under Amiberry that UI
read never returns and wasabid stops answering every connection afterwards.
`rig.py boot_assigns_missing` is simply the first `exec` of the boot that runs
with the watcher on, so the hang shows up as "Assign".

## Evidence (rig 37, Amiberry 8.2.0, --max, 1 GB Z3, JIT on)

With `UPTERM_REQUESTER_WATCH=0` every probe returned rc 0:
- `Info` on each volume: Ram Disk, VTCX (DH1_0), System (DH0), BOOTX (DH2), Rushhours (DH1).
- `Assign X: EXISTS` for SYS C S L LIBS DEVS FONTS KEYMAPS LOCALE PRINTERS REXX
  CLIPS T ENV ENVARC VTC ETC UP-TERM GG TMP HELP (WINS and REPOS are not assigned).
- `Assign LIST`, `Assign LIST DIRS` directly through `ami.py exec`, and
  `Assign LIST` via Run right at "up" (48 lines written to RAM:early.log).
- `wasabi ps` before the hang: 31 tasks, handlers DH0 DH1 DH1_0 DH2 RAM PTY idle.

With the watcher on (default):
- `ami.py exec "Assign LIST" 30`: `Exception in thread Thread-1 (_watch) ... TimeoutError: timed out`,
  then `ERR: the Amiga stopped answering while Assign was running (nothing for 25 s, and a ping got no reply)`.

Without any exec, after a fresh reboot:
- `ami.py requesters` alone: timeout in `read_ui` (wasabi client `drain_to_end` recv TimeoutError).
- `ami.py uitree`: timeout, 0 bytes.
- `ami.py ping` afterwards: timeout. wasabid is dead until reboot.
- Host side after the hang: the Amiberry process sits at 0.1 % CPU. The emulated
  machine is not busy-looping; either the Amiga side waits forever (a lock or
  a Wait() that never gets a signal) or Amiberry stopped emulating.

## Volume declarations compared (not the cause)

Amiberry: `uaehf0=hdf,rw,DH0:sys.hdf,...`, `uaehf1=dir,rw,DH1:VTCX:...vtc,0`,
`uaehf2=dir,rw,DH2:BOOTX:...boot,10`. FS-UAE: hard_drive_0 sys.hdf, hard_drive_1
vtc label VTCX, hard_drive_2 boot label BOOTX priority 10. Equivalent.
Side note: sys.hdf's RDB holds a second partition DH1 "Rushhours", so Amiberry
renames the dir drive to DH1_0. Harmless in every probe, but worth a rename of
our dir drive to avoid the clash.

## Suspects inside wasabid (wasabi rev b9d1812d, build/wasabi/src/wasabid.c)

1. `mui3_prescan()` (about lines 3350-3440): walks every MemList region word by
   word to find MUI 3.x windows. Runs before UI walks whenever MUI 3 is loaded
   (amarchy-gui, DOpus windows are open at boot). Low host CPU argues against a
   long scan, unless a read of an unmapped or Amiberry-specific region (for
   example an RTG or UAE board area listed in MemList) stalls the emulator.
2. `LockIBase` walks (about lines 3383, 3434, 3948): a deadlock with an
   input.device or Intuition holder would match the 0.1 % CPU picture.
3. MUI GetAttr/DoMethod on foreign objects (about 3544-3870).
4. RTG / P96 screen walk (`every_screen=True`): Amiberry's uaegfx differs from
   FS-UAE's; a screen list walk touching the RTG board is the other Amiberry-only difference.

## Next probe (one boot each, rig 37)

- `wasabi` UI read limited to the front screen (no every_screen) after boot:
  answers? Then suspect 4.
- Same with MUI apps not started (boot with amarchy-gui and DOpus off): answers?
  Then suspect 1 or 3.
- Amiberry with JIT off (`cachesize=0`): answers? Then an Amiberry JIT issue,
  upstream report to Amiberry with this repro.

## Repro

1. Boot rig 37 under Amiberry 8.2.0 with `rig.py start --max`.
2. `UPTERM_RIG=37 python3 tools/rig/ami.py uitree` -> timeout, then `ami.py ping` -> timeout.

## Step 3 (2026-11-02, rig 37): the stall is on the host, not in wasabid

The UI-tree premise does not hold either. With `UPTERM_REQUESTER_WATCH=0` the
rig 37 boot (two `rig.py start --max` runs) never got as far as a UI read:
- boot 1 of run 2: boot.log stops at "go started" for 180 s, "no wasabid answer";
- boot 2 of run 2: "up", then the very first wasabi command after it,
  `retry_wb_reset` -> `ami.req(0x0A)` -> SCREEN (cmd_screenctl, no mui3_prescan,
  no UI walk), times out in `drain_to_end`. `ami.py ping` then times out too.
So any wasabid command, and the boot itself, stall. Probes a/b/c were not run:
there is no wasabid fault to bisect.

Host evidence (macOS `sample`, `ps`), same minute:
- Amiberry rig 37 (pid 94182) and Amiberry rig 2 of another session (pid 29720)
  both got 0.00-0.01 s CPU in 8 s; FS-UAE rig (pid 96558, also nice 19, same
  host load ~108 on 8 cores) got 12.3 s in the same 8 s.
- Two `sample` runs of 94182: every sample identical, once in
  `IPCHandle()` -> `poll(...,0)` (a non-blocking poll, amiberry_ipc_socket.cpp:2825
  in v8.2.0), once in `op_4e72` (STOP) -> `do_cck` (pure CPU code, no syscall).
  A thread sitting 1-2 s in non-blocking code at 0 CPU is a thread the kernel
  does not schedule: the process is throttled, not deadlocked.
- `lsappinfo`: both Amiberry processes are bundle apps (com.blitterstudio.Amiberry,
  type Foreground), App Nap eligible; Info.plist has no NSAppSleepDisabled.
  FS-UAE runs as a bare binary (no bundle id) and is not napped.

Conclusion (cause strongly indicated, last link unverified): macOS App Nap /
background QoS throttles the rig's Amiberry.app (window not frontmost, started
from a background process, `nice 19`), and under host load the throttled
emulator gets near-zero CPU, so wasabid stops answering. It looked tied to the
UI read only because that read is the first longer command after boot.
Earlier "fine with UPTERM_REQUESTER_WATCH=0" runs were timing luck.

Next probe (one boot): launch Amiberry with App Nap off for that process only,
`Amiberry -NSAppSleepDisabled YES ...` (NSUserDefaults argument domain; check
Amiberry's argv parser tolerates it) or `defaults write com.blitterstudio.Amiberry
NSAppSleepDisabled -bool YES` (global user default: owner approval). Pass =
`ps -o time=` of the rig's Amiberry advances like FS-UAE's and `ami.py requesters`
answers. If it passes, the fix lives in tools/rig/amiberry.py command() plus a
host test in tests/test_rig_emu_default.py asserting the flag is on the command line.
No Amiberry upstream report: Amiberry's IPC poll is non-blocking and not at fault.

## Step 4 (2026-11-02, rig 37): App Nap disabled via argv does NOT fix the stall

- `-NSAppSleepDisabled YES` is tolerated by Amiberry 8.2.0 (--help prints normally, no unknown-option error). Added to command(); rig 37 booted with it in the argv (confirmed in ps).
- Boot: rig.py start ended with `ERR: the Amiga stopped answering while Assign was running (nothing for 25 s)`, same as before.
- Amiberry pid 40599 CPU time: 0:40.23 -> 0:42.50 over 8 s (2.3 s), later 1:45.11 -> 1:49.69 over 8 s (4.6 s). So CPU is no longer ~0 (earlier runs: 0.00-0.01 s), but still far below the 12.3 s of the fs-uae rig.
- `ami.py ping` and `ami.py exec Version`: both TimeoutError connecting to wasabid port (rc=1). FAIL.
- `pmset -g assertions`: Amiberry holds PreventUserIdleDisplaySleep ("Amiberry using SDL_DisableScreenSaver"); no PreventUserIdleSystemSleep/NoIdle assertion.
- Verdict: FAIL; the edit was reverted. App Nap is at most a partial factor; the wasabid stall remains.

## Step 5 (2026-11-02, rig 37): not throttling, not the generated config

Three boots, rig 37, `--max --emu amiberry --allow-stale-base`, Amiberry 8.2.0, host load ~20-35 (1-min), wasabi pin b9d1812d (same as 4254f87d).

- Probe 1 (boot 1, branch config as is): Amiberry CPU 6.9 s per 8 s in the background (0:36.88 -> 0:43.77), 3.9 s per 8 s after `osascript` made it frontmost (frontmost confirmed: "Amiberry"). `ami.py ping` timed out in both states; a raw TCP connect to 127.0.0.1:1270 succeeds at once (host-side bsdsocket listen), but wasabid never replies. boot.log ends "wasabid started"; the Amiberry log shows ~27 BSDSOCK sendto/recvfrom lines (the first exchanges), then none. Verdict: the emulator runs and gets CPU; background/App Nap is NOT the cause (Step 3's 0 s CPU was host load ~108, not the stall's cause).
- Probe 2 (diff vs 4254f87d's generator): amiberry_machine() is identical; the only generated differences are `serial_port=TCP://127.0.0.1:12037`, `amiberry.screenshot=LCtrl+LShift+F10` and argv `-o screenshot_dir=<rig>/shots` (plus the serialwatch helper). Machine (060, FPU 060, JIT cachesize=8192, Z3 1024, RTG 16, DH0 sys.hdf, DH1 VTCX, DH2 BOOTX pri 10, OS 3.1) is the same as rig 20 had.
- Boot 2: `--serial <scratch file>` (no serial_port line, no serialwatch): same failure (requester-watch read_ui timeout, ping no reply).
- Boot 3: additionally screenshot hotkey and `-o screenshot_dir` removed (temporary edit, reverted): config equivalent to 4254f87d's. Same failure: "up", then `ERR: the Amiga stopped answering while Assign was running (nothing for 25 s, and a ping got no reply)`.

Conclusion: no difference in the generated .uae or argv flips it; the bisect of probe 3 (JIT, Z3, CPU, OS, drives) was not run because the machine block is unchanged since 4254f87d. What differs from the session-29 rig 20 boot lies outside the config: the boot drawer (`go` script, 33 KIT_SHADOWS programs incl. Amarchy, upgetty, vtcon-handler, pty-handler, up-console.device copied over the kit at boot) and the DH0 clone (`--allow-stale-base`). Next probe (one boot): start rig 37 from 4254f87d's tools/rig (a scratch worktree) so its boot drawer and shadows are rebuilt by that code, same Amiberry and host; if it answers, bisect the shadow list (start with upgetty, which owns serial.device, and vtcon-handler/up-console.device).

## Step 6 (2026-11-02, rig 37): not tools/rig, not the DH0 source; main's built programs remain

- Coordinator fact: on current main the hang is not Amiberry-only. Rig 35 under fs-uae (`rig.py start --max`) booted to the shell and the Amarchy desktop, then "auth failed: daemon did not answer for 150 s"; `ami.py ping` to port 1268 timed out (Errno 60), host load ~22 (normal). So the cause sits in what main puts on the rig (boot drawer, kit shadows, built programs), not in the emulator.
- tools/rig diff 4254f87d..main is small and touches no boot-drawer content (rig.py: serialwatch + serial_port; paths.py: Amiberry log-line parsing, serial_tcp_port). The go script, KIT_SHADOWS and launches (RexxMast, wasabid) are generated the same way.
- Boot A (scratch worktree at 4254f87d, its tools/rig, build/amiga = symlink to main's build/amiga, Amiberry 8.2.0, `--max --emu amiberry --allow-stale-base`): boot.log ends "wasabid started", `ami.py ping` TimeoutError. FAIL. So 4254f87d's rig code alone does not give a good boot: the s29 rig 20 boot differed in the programs it copied (s29 build), not the code that copies them.
- DH0 source: `images/nyhd2.hdf` (UPTERM_SYSTEM_HDF, cloned on every start) was modified 1 Nov 23:43, after the s29 proof. Boot B = boot A with UPTERM_SYSTEM_HDF = copy of build/rig/sys.hdf (31 Oct 21:34, md5 differs from nyhd2): "up", then `[WARN] S:Shell-Startup not read: auth failed: ... did not answer for 150 s`, rc=1. FAIL. The DH0 source change is not the cause.
- Remaining variable: the main build/amiga programs the go script copies over the kit (KIT_SHADOWS, 33 entries incl. upgetty, vtcon-handler, pty-handler, up-console.device, UPConsole, Claude, amiga-pi, ixpipe-handler) and the UP-Term: shadows.
- Boot C = boot A with KIT_SHADOWS emptied in the scratch worktree's rig.py (go script left with 2 non-restore VTC copies; boot.log still "kit shadows done" ... "wasabid started"): "up", then `ERR: the Amiga stopped answering while Assign was running (nothing for 25 s, and a ping got no reply)`, rc=1. FAIL. The 33 kit-shadow programs are not the cause either.
- Gotcha: `rig.py stop` after boot B returned "stopped" but rig 37's Amiberry survived (pid 46787, killed by hand); the next start said "already running". Amiberry stop() leaves the process alive at least sometimes.
- Not flipped yet (budget hit, ~35 tool calls). Still common to all three bad boots: the VTC: drawer content staged from main's build/amiga (vtc/ host dir, incl. the UP-Term: shadows under VTC:Apps, Amarchy, wasabid's environment), the RexxMast + wasabid launch, XCON/PTY/IXPIPE mounts, LIBS: assigns to VTC:ixp6/amissl. Next probe (one boot each, fs-uae is fine per rig 35): (1) boot A with build/amiga pointed at a build of 4254f87d (s29 binaries) - if good, bisect the staged programs; (2) go script cut after "wasabid started" with the UP-Term: shadows and the XCON/PTY/IXPIPE mounts removed.

## Step 7 (2026-11-02, rig 37, fs-uae, current main incl. the 07:01-07:25 build/amiga)

- Deps: build/rig37/boot/wasabid md5 217b3d92... = build/wasabi/wasabid (pin b9d1812d); VTC:ixp6/screennotify.library md5 = build/screennotify. Staged deps current.
- Probe 0: `start --emu fsuae --ram 256` (no --max; MAX is the default anyway, only zorro_iii_memory 262144 differs). boot.log to "wasabid started", `rig.py status` = "wasabid 0.11b2". GOOD.
- Probe 1: `start --max` (1 GB Z3), same tree, same binaries. "wasabid 0.11b2", `run "Version LIBS:ixemul.library"` answered. GOOD. So neither the 07:01 binaries, the mounts, the LIBS: assigns nor RexxMast break wasabid on fs-uae.
- Difference to failing rig 35 (fs-uae, --max, 07:39): config identical except window size and `zoom = 640x512`; rig 35 has `screenmode.name` = `aga` (persisted `rig.py aga`, sys_hdf_screenmode writes the AGA ScreenMode prefs into every fresh sys.hdf). Rig 37 had none (master image's mode).
- Probe 2: `rig.py aga` on rig 37 (ScreenMode prefs + reboot with --max): the generated .fs-uae is now byte-identical to rig 35's failing one except the title; go script identical (rig/port names aside); staged amarchy-gui, pty-handler, up-console.device, vtcon-handler md5-identical to rig 35's. "wasabid 0.11b2". GOOD.
- Probe 3: stop + fresh `start --max` again on AGA: "wasabid 0.11b2". GOOD.
- VTC: diff rig 35 vs 37 outside kit-orig: only grid-fit/aadither test inputs (gf.conf, aad*.txt, GridfitJB/Sym.ttf, SymbolsNerdFontMono.ttf, pal16) written by rig scripts after boot, plus kit-hash. None is read by the go script. rig 35 serial.log: no error/guru lines; its retry.log shows wasabid answering at 07:06.
- Verdict: 4 of 4 fs-uae boots of current main on rig 37 answer, under every condition rig 35 had at 07:39. The fs-uae hang does NOT reproduce from the boot staging; the halving probes (mounts, LIBS:, RexxMast) had nothing to halve. Rig 35's 07:39 failure is intermittent or came from host-side state at that moment (rig 35's start log: "the Amiga took the connection but its daemon did not answer for 150 s"). The Amiberry hang (pre-07:01) is a separate cause still open.
- Next probe: repeat-boot rig 35's exact start (`UPTERM_RIG=35 ... start --max`, with its owner's OK) to see whether it is rig-35-specific; for Amiberry, rerun Step 6 boot A now that the fs-uae baseline is green.

## Step 8 (2026-11-02, rig 37, Amiberry 8.2.0, `--max --emu amiberry`, hunt 8)

- Probe 1, JIT off (cachesize=0 through a temporary env knob in tools/rig/amiberry.py, reverted, not committed): WORSE, not a flip. Two boots in a row (Amiberry pid 1568 at 08:19, then pid 10739 at 08:25 after rig.py's 180 s retry) both stop at boot.log "kit shadows done" / "go started": the go script's next line `C:Wait 15` never returns ("wait done" never written). The emulator sits at 0.1 % host CPU (TIME 2.86 s -> 3.01 s over ~1 min), state RNs, its log silent after "68060 FPU state: disabled"; no modal dialog (only window: the rig window, AXStandardWindow 800x628); serial TCP 12037 established with empty queues, so no blocked serial write. wasabid's port 1270 never listens. So with JIT off the Amiga stops getting time (timer/vblank) far earlier than the JIT-on wasabid stall; JIT is not the deciding item, but "emulated time stops while the host idles" is a new symptom worth one more probe.
- Probe 3, FPU line: red herring. Amiberry's "FPU UNIMPLEMENTED INSTRUCTION/FPU DISABLED EXCEPTION PC=00f80c96" is the Kickstart's FPU probe at boot; fs-uae rig 8 (good) logs the same probe as "no FPU: F327-0000 PC=00F80D6C", and an older Amiberry rig 36 log (00:41) has it at PC=00f80d6e.
- Probe 2 (old binaries from build/up-term.lha) not run: budget hit. Staging needs a private build/amiga (a scratch worktree with build/amiga pointing at the extracted lha, as in Step 6 boot A); swapping main's build/amiga would break other sessions' rigs.
- Next probes: (a) Step 6 boot A with build/amiga = extracted build/up-term.lha (1 Nov 12:46); (b) JIT on, `cpu_speed=max` instead of `real` plus `cpu_idle=0`, to test whether Amiberry's "real speed" idle throttling stops emulated time (it fits both the JIT-off freeze and the JIT-on stall after bsdsocket traffic).

## Step 9 (2026-11-02, rig 37, Amiberry 8.2.0, `--max --emu amiberry`, hunt 9)

- Premise correction: `--max` already writes `cpu_speed=max` (rig.py amiberry_machine: `speed="max" if FAST or M060`). Every earlier Amiberry --max boot ran at max speed, so "real-speed throttling" was never in play.
- Probe (b): JIT on (cachesize=8192), cpu_speed=max, plus a temporary env knob in tools/rig/amiberry.py (reverted, not committed) adding `cpu_idle=0`, `cpu_throttle=0`, `amiberry.inactive_pause=false`, `amiberry.minimized_pause=false`, `amiberry.active_not_captured_pause=false` (all confirmed in build/rig37/vtcon-rig.uae lines 14-18). FAIL, same as Step 6 boot A: boot.log ends "wasabid started", start reports `ERR: the Amiga stopped answering while Assign was running (nothing for 25 s, and a ping got no reply)`. Amiberry pid 73409 then at 0.1 % CPU, state RNs, CPU time 19.86 s -> 19.87 s; its log only repeats the status line (`091%%`). So neither Amiberry's idle sleep, throttle nor any pause-on-inactive/minimized/not-captured setting stops emulated time; the freeze is elsewhere (host-side wait inside Amiberry after bsdsocket traffic, or an emulated-side wait that never gets its event).
- Probe (a) (Step 6 boot A with build/amiga from build/up-term.lha) NOT run: disk fell to 2.0 GB free during probe (b) (three rig starts staging at once, two from other sessions), 1.1 GB after stop; under the 3 GB floor for another 1.5 GB sys.hdf clone.
- Next: probe (a) once disk allows; and a `sample` of the frozen Amiberry process right after "wasabid started" to see which thread waits on what (Step 3's sample showed the main thread in IPCHandle poll / do_cck under host load, not this case).

## Step 10 (2026-11-02, rig 58, hunt 10)

Worktree vtcon-s48-amiberry-default HEAD 719e95c6, `UPTERM_RIG=58 rig.py start --max --emu amiberry --allow-stale-base`, Amiberry 8.2.0, OS 3.1. Host load: top CPU users fs-uae rig 55 (105 %), a DEViLBOX driver (94 %), another session's Amiberry rig 3 (52 %); boot 2 also storybook + tsc (85/78 %). Evidence files: scratchpad `amib-hunt/` (sample1/2.txt, ps*, lsof*, start.log; boot 2 in `amib-hunt/b2/`).

- Boot 1 (13:34): GOOD. boot.log "wasabid started" 13:35:41, `start` printed "up", `ami.py ping` -> "wasabid 0.11b2". The hang did NOT reproduce. Amiberry CPU 3.2 s per ~25 s (0.1-11 %), not the 75-80 % of run 3.
  - `sample` 5 s x2 of the healthy idle state (reference for a hung one): main thread in m68k_run_jit -> op_4e72 (STOP) / do_cycles / display present (emulated CPU idle, all tasks waiting); extended-trap thread in bsdsocklib_WaitSelect -> host_WaitSelect -> trap_call_lib -> trap_Call68k -> SDL_WaitSemaphore (the 68k Wait() half of WaitSelect, normal); bsdsocket thread of wasabid in bsdthr_WaitSelect -> select() 87 %, cycling through its semaphore (wasabid's tv timeout); a second bsdsocket thread (THREAD_START 13:34:55, 45 s before wasabid's at 13:35:39) idles in its semaphore: some earlier boot program opens bsdsocket.library.
  - Amiberry log: 852 WaitSelect (action 5), 334 recvfrom, 282 sendto, CloseSocket on peer close; no error. lsof: wasabid port 1291 LISTEN in Amiberry, serial 12058 ESTABLISHED to serialwatch. serial.log: no Guru/alert.
- Boot 2 (13:43, stop + same start): FAIL, but at a different place: 3 of 3 attempts (rig.py's own retries) stop with boot.log at "early LIBS: assigns done (before Amarchy)", i.e. BEFORE "wasabid started"; "no wasabid answer", "the boot failed 3 times". The one `sample` taken (pid 12770, 13:48:49, 570 samples) is unsymbolicated (process in teardown by rig.py's retry) and not usable.
- So under Amiberry the boot stalls at varying points (Step 3: "go started"; Step 8: `C:Wait 15`; Step 9/run 3: after "wasabid started"; today: after the early LIBS: assigns, or not at all). The stall is not tied to wasabid or bsdsocket: wasabid answered normally whenever the boot reached it in time.

Verdict: OPEN (side not identified). Strongly indicated: an intermittent stop of emulated progress during the boot script (DOS/handler path), independent of wasabid; the fixed "after wasabid started" location was coincidence of timing.
Next probe (one): boot until a stall, then, while frozen, `sample` the Amiberry pid (wait until rig.py is not tearing it down: run start with retries off or sample within the first 240 s attempt) and look at the `hardfile`, `filesys` and `extended-trap` threads: a filesys/hardfile thread blocked on a host call or the main thread spinning in STOP with no interrupt would name the side. Second best: compare the step-at-stall across 5 boots against the go script line that follows (next line after "early LIBS: assigns done" is the Amarchy start).

## Step 11 (quiet host) (2026-11-02, rig 58, not run: host never quiet)

Quiet gate (scratchpad `amib-quiet/gate.sh`: no non-emulator process > 40 % CPU, WindowServer <= 60 % allowed, other rigs' fs-uae/Amiberry summed < 150 %; `waitquiet.sh` needs two QUIET verdicts 15 s apart) polled 14:04-14:49 (45 min): BUSY every time. No boot was made; build/rig58 was never created.
- Last gate lines (14:49): DEViLBOX `tsc -b --force` 60 %, Xcode clang -cc1 47 %, DEViLBOX scratchpad `drv` 48 %, FSEvents 47 %; fs-uae rig55 62 % + rig56 50 % (sum 112 %). Earlier: tsc 77-119 %, storybook 43-49 %, WindowServer 46 %.
- Finding while preparing: rig.py launches Amiberry under `nice -n 19` (tools/rig/rig.py `_start`, `subprocess.Popen(["nice", "-n", "19"] + amiberry.command(...))`). On a host with 4+ busy normal-priority processes the emulator gets the leftovers; this fits Step 10 (boot 1 good on a lighter host, boot 2 failed 3/3 with tsc + storybook up) and the moving stall point. Not yet proven: needs a sampled stall.
- Ready for the next try: `amib-quiet/boot.sh N` runs rig.py start with retries off (`start0.py` sets `rig.start.__defaults__ = (0,)`; rig.py has no retry flag, START_RETRIES = 2 is hardcoded) and `sample`s Amiberry for 10 s once boot.log has not moved for 60 s (inside rig.py's 180 s BOOT_STALL window), saving ps, boot.log/serial.log tails and `ami.py ping`.

Verdict: OPEN. Next probe: same boot.sh when the gate passes (other sessions' tsc/storybook/clang idle), or, to test the starvation theory directly on a busy host, one boot with Amiberry at normal priority (drop the nice 19) and compare the stall rate.
