---
title: What ends FS-UAE silently when nvim runs on a vtcon rig
date: 2026-10-31
---
Question: FS-UAE vanishes 0-2 s after nvim starts; no .ips, log cut mid-line, no quit reason.

## Verdict
It is a HOST CRASH (fatal signal), not a quit path, and macOS suppressed the crash report.
Evidence: `/usr/bin/log show --last 5d` (kernel) lists 12 lines `Process[pid] crashed: fs-uae. Too many corpses being created.`
each preceded by `fs-uae[pid] Corpse failure, too many 6` and `AMFI: Denying core dump`, between 19:47 and 20:31 on 2026-10-31
(pids 28856 29521 38481 41400 41964 93505 23568 25594 29123 30253 32633, matching the nvim runs of s38). The kernel prints
"crashed" only for core-dump signals (SEGV/BUS/ABRT/ILL), not for SIGKILL/SIGTERM/SIGPIPE. ReportCrash writes no .ips when
6 corpses are already in flight (test_palette, tp_red, test_iconpens, test_term_engine also hit it). No .ips for fs-uae
exists for 2026-10-31 (last one 26 Oct). Scratch log of the kernel query: only 50 lines, deleted below.
Log cut mid-line is just stdio block buffering: libfsemu/src/log.c:84-112 flushes only with `log_flush`/`flush_log`; rig
config sets neither; build/rig/logs/fs-uae.log.txt is 929792 B = 227 x 4096 (block boundary). An exit() would flush
stdio, so a cut log also rules out every exit() path below.

## Ranked candidates
1. Host SIGSEGV/SIGBUS from guest-controlled data (most likely, unconfirmed which site). The emulated 68060 with
   `bsdsocket_library = 1` (tools/rig/rig.py:1062,1065) runs nvim, which does socket/select/pipe work via ixnet; with a
   corrupted heap the guest passes garbage pointers/lengths/fd sets to the host-side bsdsocket traps:
   src/od-fs/bsdsocket_posix.cpp (recv :755, recvfrom :762, send :775, sendto :780, WaitSelect :1166, host_socket :1228).
   Log of surviving rig 1 ends in `uae_start_tread "bsdsocket"` lines (threads per socket), so bsdsocket is live.
2. abort() in FS-UAE bookkeeping: src/cpummu.cpp:1496 (`try stack underflow`), :1506 (`try stack overflow`),
   src/vm.cpp:59/69/82 (alloc table full / address not found). The message goes to stderr, which rig.py:1163 sends to null
   unless `--debugger`, so nothing is visible. Guest-reachable only through MMU exception nesting / mmap tracking.
3. Not candidates (checked):
   - rig tooling: rig.py stop (:1818) does `pkill -TERM -f CFG`, `pkill -9` only after 90 s (:1840-1846); watchdog
     (:1927) calls stop() only after 60 idle min or a 10+ CPU-reset/min loop (recover_reset_loop :1905); start retry (:1713-1722)
     only when wasabid never answered. TERM would be an orderly SDL quit that flushes the log. wasabi.py:199 kills only its own stream pid.
     No other repo script pkills fs-uae (grep of UP-Term repos and ~/.claude/hooks).
   - exit() paths: libfsemu emu.c:221 (fs_emu_fatal), :257 (gui mutex), render.c:1806, sdl.c:549/680, filesys.c:127,
     od-fs/video.cpp:1035 (picasso mode, non-RTG build). All flush stdio, so a cut log excludes them; none is
     reachable from guest action except video.cpp:1035 (gfx_set_picasso_modeinfo, #else build only).
   - uae_quit()/quit_program: src/pci.cpp:1098, src/debug.cpp:4982, src/main.cpp:770, newcpu.cpp:5339-5354 are orderly
     quits (log closed, reset path). Guest cannot trigger them via ordinary memory writes.
   - SIGPIPE: bsdsocket send paths use MSG_NOSIGNAL (:775,:780); this SDK (26.2) defines it. recv has none but cannot SIGPIPE.
     Binary imports no signal()/sigaction (nm -u fs-uae), so any SIGPIPE from another write would kill silently
     (no "crashed" line though, so it does not match the kernel evidence).
   - jetsam/memory pressure: no jetsam line for fs-uae in the 5 d kernel query.
   - CPU halt option: no halt/quit key in rig config (rig.py:1062-1140).

## Cheap confirmation (do not run on rigs 1, 2, 8)
- `sysctl` is no help; corpses are not tunable here. Instead run ONE fresh rig's FS-UAE under lldb:
  `lldb -- build/rig<N>/../../Emulators... fs-uae <cfg>` (path: ~/Code/Emulators/fs-uae/fs-uae <rig cfg>), `process handle
  SIGSEGV -s true`, run nvim --headless +q on the guest; lldb stops at the faulting host line and `bt` names the
  site (bsdsocket trap vs abort). lldb catches the signal before the kernel corpse step, so the corpse limit is irrelevant.
- Alternative: clear the corpse backlog (let ReportCrash drain, close crashed test_* processes) and rerun once; an .ips then appears.
- Add `log_flush = 1` to a test rig config to get the log up to the crash line (cheap, one config key).

## lldb (s39)
Rig 4 (060 max, fresh start), nvim a3aa3137 + uhome staged into build/rig4/vtc, `SetEnv HOME /VTCX/uhome`, `VTC:nvimd/bin/nvim --headless +q`. lldb attached to FS-UAE pid before the run (python loop: stop on SIGSEGV/SIGBUS/SIGILL/SIGABRT, pass, print bt 15, continue; tested by attach/detach first). FS-UAE has symbols but no debug info (no dSYM; line numbers only for some libfsemu frames), so `frame variable` is unavailable.
- Result of run 1: NO crash. ami.py reported "Amiga stopped answering" (25 s); under lldb for ~100 s no signal at all reached lldb, FS-UAE stayed alive (50-57% CPU, later 9%), wasabi dead. This is the known HANG outcome, not the exit outcome.
- Log: fs-uae log cut mid-line at "[AUDIO] fs_e" after `uae_start_tread "bsdsocket"` (stdout buffered, so last lines are unreliable). Three bsdsocket threads exist (nvim opened sockets).
- `sample` of the hung process: bsdsocket threads idle in bsdlib_threadfunc -> SDL_SemWait; emulation thread in m68k_run_2 -> do_specialties -> haltloop (newcpu.cpp:3598) ~100% of samples, i.e. the guest CPU is halted/stopped (usleep + do_cycles_slow), not spinning in guest code. Not yet determined if it is regs.halted (cpu_halt, newcpu.cpp:4630, writes "CPU halted: reason" to the unflushed log) or a plain STOP; reading `regs` via lldb failed (duplicate symbol, no type info).
- Not obtained: a fatal host backtrace. Exit outcome (process vanishes) did not occur this run; 1 of 3 allowed runs used (rig start took ~15 min wall: start restarted FS-UAE once).
- Next: run again with lldb attached on a fresh rig until the vanish outcome occurs, plus `breakpoint set -n cpu_halt` and `-n exit -n _exit -n abort` to catch a host exit path; flush the log (fs-uae log is block-buffered) or read `gui_data.cpu_halted`.
