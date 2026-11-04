# FS-UAE upstream report draft: 3.1 palette.prefs save crashes guest (RTG switch)

Status: DRAFT, not filed. Filing is outward-facing; owner approves first.

## Summary
On FS-UAE with AmigaOS 3.1, a 68060 CPU and graphics_card = uaegfx (Picasso96), copying a palette.prefs
file to ENV:Sys/palette.prefs makes IPrefs reset the Workbench. The display switches RTG -> native -> Picasso96.
After that switch the CPU often runs from address $A6 / $7A and the guest dies.

## Steps
1. Boot AmigaOS 3.1 with uaegfx RTG Workbench, 68060.library.
2. `Copy RAM:palette.prefs ENV:Sys/palette.prefs`.
3. Watch the log during the IPrefs Workbench reset.

## Observed (log lines)
- `BEAMCON0 0000 -> 0020 PC=10031f1e` (68060.library Dispatch idle STOP), `SetSwitch() - Picasso96`
- `Illegal instruction: 0b7a at 000000A6 -> 00000000`
- then thousands of `filesys_lput/wput called PC=0000007a`, or `IDE1 DATA write without request!? 00 PC=0000007A`,
  or `CPU reset PC=f80dc8` followed by a reset loop. FS-UAE exits.

## Frequency
Bare OS, no Intuition patchers: 2 of 6 boots crash. With patchers loaded the odds rise to about 3 of 4.
The same save through the Workbench Palette prefs program on 3.2 applies cleanly.

## Evidence
thoughts/shared/research/s54-evidence/31-copy-crash-bisect.md (rounds 1-3, commits 117c7add, 766108d7).
FS-UAE version: 3.2.35, built from source at v3.2.35-3-gbf6a7ce72 (github.com/FrodeSolheim/fs-uae). The 3 local commits on top touch FPU Inf handling on native host-double builds, filesys open handles following a rename, and bsdsocket reset on exit; none touches the CPU core, chipset or RTG switching, but the report should say so and offer a retest on plain v3.2.35.

Config (tools/rig/rig.py, `--max` 060 rig, stock 3.1 ROM):
    amiga_model = A1200
    cpu = 68060-NOMMU, fpu = 68060, uae_cpu_speed = max, uae_cpu_cycle_exact = false, uae_cpu_compatible = false
    jit_compiler = 1
    fast_memory = 8192, zorro_iii_memory = 1048576
    graphics_card = uaegfx, uae_gfxcard_hardware_sprite = true
    bsdsocket_library = 1, hard_drive_0..2 = host directories, serial_port = file
Host: macOS 15 (Darwin 24.6.0), Apple Silicon.

## Stock v3.2.35 retest (s56)

Binary: plain tag v3.2.35 (`git describe` = v3.2.35, commit 4ae7ddaec, no local commits), built in a separate worktree
with `./bootstrap && ./configure && make -j2` (the main checkout's config.status has no configure arguments either).
Rig 68, `rig.py start --max --os 31` (bare stock 3.1 Workbench, 060 JIT + uaegfx), `UPTERM_FSUAE` = that binary,
confirmed in `ps`. SolidWin (started by the rig's boot script) was switched off by a `; C:SolidWin` line in S:User-Startup;
`ps` on the guest at Copy time listed only the stock Workbench process. Recipe: fresh boot (first run after start,
five after `rig.py reboot`), 20 s settle, `Copy RAM:palette.prefs ENV:Sys/palette.prefs` with the owner's GUI-save bytes, 30 s wait.

Result: 6 of 6 boots crash on plain v3.2.35 (patched build: 2 of 6). A control boot with no Copy stayed alive 100 s.
The failure differs in form from the patched-build report: the fs-uae process disappears right after the RTG -> native switch,
before any `SetSwitch() - Picasso96`, with no `Illegal instruction at 000000A6`, no `CPU reset` line, no IDE line and no
macOS crash report. The log is buffered, so its last lines are what had been flushed:
- R1 to R6: wasabi `cannot reach 127.0.0.1:1301 - Connection refused` after the Copy; `cpu_resets 0->0`, `ide 0->0`, no reset-loop file.
- Last guest-related log lines per run: `SetSwitch() - amiga` x1 to x3 (R1 3, R2 2, R3 1, R4 2, R5 3, R6 2), then nothing.
- One extra boot with SolidWin running (not counted) died the same way (`SetSwitch() - amiga` x2).
Reading: the Copy-triggered Workbench reset kills plain v3.2.35 every time, so the bug is not in the three local commits.
It is unverified whether the 6/6 host-process death and the patched build's 2/6 guest crash are the same fault or two
(the stock process exits without the guest-side jump to $A6); a debugger run (lldb on the stock binary) would tell.

## lldb on stock v3.2.35 (s56)

Setup: rig 80, `rig.py start --max --os 31`, `UPTERM_FSUAE` = a wrapper that starts the stock debug binary (v3.2.35, -g -O2) normally
and attaches lldb to its pid 4 s later (launching it under lldb hangs in `g_dir_open` of the plugins dir, apparently a macOS file-access
prompt for the debugserver chain). Recipe as above: boot, put tests/data/desk/palette-gui-save.prefs to RAM:palette.prefs, 15 s settle,
`Copy RAM:palette.prefs ENV:Sys/palette.prefs`. Stop-hook (`bt 25`, `thread backtrace all`) set so output appears at the stop.

Stop reason: `EXC_BAD_ACCESS (code=2, address=0x351004000)` on thread `emulation`, a host SIGSEGV (protection failure, unmapped host page).
It is not exit(), _exit() or abort(); the breakpoints on those never fired. Continuing re-faults at the same pc (0x100be5334), so no
signal handler absorbs it and the process dies. (In an earlier attempt the same fault gave the same address class, 0x35x004000.)

Faulting thread, top frames (stock tag v3.2.35):
```
#0  do_get_mem_word(a) at maccess.h:20           (*b << 8) | *(b+1), b = 0x351004000
#1  get_diword(o=0) at newcpu.h:366
#2  m68k_run_2() at newcpu.cpp:5241              r->opcode = x_get_iword(0)   (instruction fetch)
#3  m68k_go(may_quit=1) at newcpu.cpp:5497
#4  do_start_program() at main.cpp:1039
#5  start_program() at main.cpp:1055
#6  real_main2 at main.cpp:1196
#7  real_main at main.cpp:1212
#8  amiga_main() at libamiga.cpp:315
#9  main_function at main.c:750
#10 emulation_thread at emu.c:413
#11 g_thread_proxy (libglib)   #12 _pthread_start
```
Main thread is idle in `fs_ml_render_iteration` (render.c:656, condition wait), so the render thread is not involved.
Reading: the 68k instruction fetch in the interpreter loop (`m68k_run_2`, not JIT code) reads through a host pointer that
`regs.pcp` computed for the new PC, and that page is unmapped. So the guest PC was set to an address whose bank maps to a host
pointer past a mapped region (the 4 KB page boundary 0x...004000 suggests the end of an allocated block), not a bad store or a libc fault.

Attempts (1 launched under lldb, 4 attached):
1. launched under lldb: stuck 5 min in g_dir_open, no boot (wrapper changed to attach).
2. attach, default signals: lldb stopped on the EXC_BAD_ACCESS above, then lldb and fs-uae were gone before `bt` printed.
3. same, SIGSEGV/SIGBUS pass-through set: same stop, same loss (bt lost; lldb killed with the dying rig).
4. attach with lldb in its own session: no host death this time; guest went dead (wasabi refused), a reset-loop log was written, fs-uae kept running at 106 % CPU. This is the patched build's guest-crash form.
5. attach with stop-hook: stop at the same EXC_BAD_ACCESS, full backtrace captured (above).
Result: host SIGSEGV in 3 of 4 Copy runs (attempts 2, 3, 5), guest reset loop in 1 (attempt 4).

Same fault as the patched build? Probably the same root: the guest executes from a bad PC after the RTG -> native switch. In the patched
build that PC ($A6/$7A) is a valid low address and the guest crashes; in stock the fetch lands on an unmapped host page and takes the
host down. Attempt 4 on the stock binary shows the guest-crash form too, so both outcomes occur on stock. Unverified: the guest PC and
register values at the fault (not read).

## Amiberry retest (s56)

Rig 84, `rig.py start --max --os 31 --emu amiberry` (/Applications/Amiberry.app, 060 JIT, 8 MB fast, 1 GB Z3, ZorroIII gfx 16 MB), same recipe:
fresh boot (first run after start, then `rig.py reboot`), 20 s settle, owner's GUI-save bytes (tests/data/desk/palette-gui-save.prefs) put to RAM:palette.prefs,
`Copy RAM:palette.prefs ENV:Sys/palette.prefs`, 30 s wait. SolidWin was NOT switched off (guest `Status` at Copy time: wasabid x2, SolidWin, Workbench), unlike the FS-UAE stock retest.
Before the Copy every boot answered wasabi (`Version` rc 0), so no boot-config failure.

Result: Amiberry: guest reset into green screen. After the Copy the Amiberry host process stayed alive in all 7 runs (no host death, unlike stock FS-UAE),
but in 5 of 6 Copy boots (R1-R5) the guest stopped answering wasabi (`Echo alive` returned nothing / connection error). R6 survived (answered rc 0). Control boot R7 (no Copy): alive and answering.
Owner saw the green screen live on rig 84 in R1 (Kickstart chip-RAM colour, i.e. the guest was resetting/crashed); R1 log: about 3 s after the Copy (13:24:12.9) three
`--- New exception --- Error in compiled code` JIT faults (AARCH64 store to unmapped m68k address bc9e1002 / cdfe1006, pc=00f8198a in ROM, D0=cf3a1002, "dummy_bank store ... ignored"),
then the log stops at 13:24:17 and the process idles at 0.1 % CPU. The JIT-fault lines were seen in R1 only by log inspection; R2-R5 were judged by wasabi silence (their exception counts in the shared log were not tracked per run: 0,3,3,3,0,0 for R2-R7 as counted, unreliable).
Reading: the bug is not FS-UAE specific; Amiberry with JIT reproduces a guest crash (5 of 6) after the same Copy. Host process survives there, so the stock FS-UAE host SIGSEGV is a different symptom of the same guest-side event (wild store/jump after the RTG -> native Workbench reset). Unverified: whether it reproduces with JIT off.

## Amiberry JIT-off retest (s56)

NOT ANSWERED: no Copy boot ran. With the JIT off the Amiberry rig never reached a booted guest (0 of 6 Copy boots, no JIT-on control boot).

Command: `UPTERM_RIG=84 python3 tools/rig/rig.py start --max --os 31 --emu amiberry --no-jit --idle-minutes 0 --purpose "vtcon-16 Amarchy session: palette Copy crash, JIT off"`
(new flag `--no-jit`, commit 5d3f99d0; written build/rig84/vtcon-rig.uae has `cpu_model=68060`, `cachesize=0`, no `mmu_model`; log shows `JIT: cache=0`).

Three starts, each: `boot 60/120/180/... s: (go not started)`, wasabid never answered, Amiberry host at 0.1 % CPU with 1.5-2.4 s total CPU time.
- Start 1 and 2: log ends right after the P96 mode list (no "Error in compiled code", no exception lines). `sample` of the process: main thread inside
  `m68k_run_2_020 -> ... -> handle_msgpump -> process_event -> amiberry_inactive -> SDL pthread wait`: Amiberry paused the guest because its window was inactive.
- Fix tried: amiberry.py now writes `inactive_pause=false`, `minimized_pause=false` (second commit). Start 3: guest got further (12 s CPU, log to 14:36:23, "098%%" lines), then
  stalled again at 0.1 % CPU; `sample`: main thread in `process_event -> isfullscreen() ->` SDL main-thread wait. So a host window/SDL event stall, not a guest crash.
  Still no wasabid answer after 180 s.
Host load averaged 47-52 during the attempts (other sessions' rigs and vitest); a second Amiberry (another session) was running.

Reading: inconclusive. Nothing here shows a JIT effect either way; the JIT-off stall is a window-focus / SDL event pause (the rig window not frontmost), which the JIT-on runs did not hit,
so a retest needs the rig 84 window frontmost (owner click on it) or a headless/focused Amiberry run. Where to file upstream stays open.
