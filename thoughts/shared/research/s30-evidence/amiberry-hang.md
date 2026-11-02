---
date: 2026-11-02
topic: Amiberry rig hang, s30 opus hunt (rig 47, OS 3.2)
status: in progress
---

# Amiberry rig hang: s30 hunt on rig 47

Scope (coordinator, 2026-11-02): (1) does OS 3.2 hang like 3.1, (2) another Amiberry build, (3) boot bisect.
vtcon-ee's hunt 9 on rig 37 holds: cpu_speed=max/cpu_idle=0 probe, and build/amiga from up-term.lha. Not repeated here.

## Static findings

- wasabid main loop (wasabi-eval/wasabi/wasabid.c:7018) sleeps in `WaitSelect(nfds+1, &rd, NULL, NULL, &tv, &sigs)`
  and calls `accept()` (7106) only after it wakes. "Host kernel accepts the TCP connect, wasabid never replies"
  therefore means the guest's WaitSelect never wakes: neither the bsdsocket readiness event nor the timeout
  (timer.device) reaches the task. Same family as hunt 8 (JIT off): `C:Wait 15` never returns = timer.device
  never fires.
- Installed Amiberry is 8.2.0. Newer: 8.4.0 (bsdsocket "event monitor and blocking-call fixes", PR #2334,
  SO_RCVTIMEO PR #2384), 8.4.1, 8.4.2 ("a slow IPC client no longer stalls emulation", #2448), 8.5.0 (27 Oct).
  8.5.0 extracted to the session scratchpad (Amiberry85.app) for an A/B boot via UPTERM_AMIBERRY.

## Tests

- T1 rig 47 OS 3.2 start (08:41): FAILED before the emulator ran: ENOSPC while staging build/rig47/vtc
  ("[Errno 28] No space left on device: .../build/rig47/vtc/fonts/IBM/16"). Disk was 1.1 GB free, then 0.8 GB.
  Own partial build/rig47 removed (mv aside, rm); 8.5.0 app copy deleted to free space.
- Sound lead: amiberry.py writes sound_output=exact only when volume>0; rigs are silent by default (rig.py --sound),
  so sound pacing cannot be the cause of default-rig hangs. Ruled out statically.
- vtcon-ee hunt 9 (relayed): throttle/idle off (cpu_idle=0, cpu_throttle=0, all *_pause=false, cpu_speed=max) still hangs.

## T2: OS 3.2 on rig 47 under Amiberry 8.2.0 does NOT hang (2026-11-02 ~09:10)

- Start: `UPTERM_RIG=47 rig.py start --max --emu amiberry --os 32`. Staging took ~20 min (I/O bound, Defender ~60% CPU).
- Up 09:10:29, rc=0, boot/boot.log reached "wasabid started".
- Host load heavy: four fs-uae rigs (2, 3, 46, 48) at 83-97% CPU each.
- Amiberry pid 6716 kept running (TIME 1:39 -> 1:54, 45-83% CPU). It did not freeze at 0.1% like rig 37.
- `ami.py ping` -> "wasabid 0.11b2" 3/3. `exec "Version"`, `exec "Assign LIST"` (42 lines), `exec "List RAM:"` all rc 0.
- Config: kickstart_rom_file=/Users/spot/Desktop/KICK_323.rom, 68060 + FPU, cpu_speed=max, cachesize=8192, z3mem 1024, ZorroIII RTG 16, bsdsocket_emu=true, DH0 dir os-32.
- Reading: the hang is specific to the OS 3.1 path (Kickstart 3.1 ROM / 3.1 sys tree) under Amiberry, or not deterministic. One boot only; not repeated.
- Next: boot 3.1 on a fresh rig with the same flags to confirm the 3.1/3.2 split; then A/B the 3.1 config with Amiberry 8.5.0 (UPTERM_AMIBERRY=<scratch app>), whose 8.4.0 bsdsocket event-monitor fixes (#2334) are the candidate.
