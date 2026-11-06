# Restart crash: intuition patch remover leak measurement (rig 176)

Branch restart-measure (from 7b565387). Debug sentinels (global ENV AMDBG_*) added to strut_patch.c, wbreset_patch.c, borderblank_patch.c, amarchy.c. Nothing fixed.

## Commands
- make build/amiga/Amarchy build/amiga/amarchy-gui build/amiga/amarchy-flat; make build/amiga/amarchy-menu; make chkabort-check ([OK] x3)
- UPTERM_RIG=176 python3 tools/rig/rig.py start --max --purpose "vtcon-c1 Amarchy: restart crash leak measurement"
- amarchy_gui_rig.py --setup --no-build (prints known "FAIL flat prefs off")
- loop3.py <N> <startcycle> (scratch script): per cycle run Amarchy, step a = read markers after port up, step b = Break, wait for exit, read markers; on crash copy fs-uae log, reboot.
- Pass 1: loop3.py 12 1 (cycles 1-2), loop3.py 12 3 (cycles 3-12). Pass 2: loop3.py 12 1.

## Marker meaning
INST = install return (0 = ok). VEC0/VEC1 = jump table target of the two patched LVOs before arm. GATE0/GATE1 = new gate address. POST0/POST1 = vector after unarm inside Forbid. RC = rc1*100+rc2 of unarm. REM: 1 start, 3 busy leak return, 4 clean end, 9 not published. WAIT = busy wait loops. MAIN: 1 start, 2 rexx open, 3 Ctrl-C seen, 4 return.

## Pass 2 (cycles 1-12, every marker value)
19:12:16 cycle 1 a START port True procs ['3']
  BB_GATE0=0x100fa6d0 BB_GATE1=0x100fbbd0 BB_INST=0 BB_VEC0=0xff4c2c BB_VEC1=0x1008ffd6 MAIN=2 STRUT_GATE0=0x1005b418 STRUT_GATE1=0x10094298 STRUT_INST=0 STRUT_VEC0=0xff4c3c STRUT_VEC1=0xff4cbe WBR_GATE0=0x100fa3d0 WBR_GATE1=0x100fab90 WBR_INST=0 WBR_VEC0=0x1009004a WBR_VEC1=0xff1b66
19:12:20 cycle 1 b BREAK gone True
  BB_GATE0=0x100fa6d0 BB_GATE1=0x100fbbd0 BB_INST=0 BB_POST0=0xff4c2c BB_POST1=0x1008ffd6 BB_RC=0 BB_REM=4 BB_VEC0=0xff4c2c BB_VEC1=0x1008ffd6 BB_WAIT=0 MAIN=4 STRUT_GATE0=0x1005b418 STRUT_GATE1=0x10094298 STRUT_INST=0 STRUT_POST0=0xff4c3c STRUT_POST1=0xff4cbe STRUT_RC=0 STRUT_REM=4 STRUT_VEC0=0xff4c3c STRUT_VEC1=0xff4cbe STRUT_WAIT=0 WBR_GATE0=0x100fa3d0 WBR_GATE1=0x100fab90 WBR_INST=0 WBR_POST0=0x1009004a WBR_POST1=0xff1b66 WBR_RC=0 WBR_REM=4 WBR_VEC0=0x1009004a WBR_VEC1=0xff1b66 WBR_WAIT=0
19:12:31 cycle 2 EXCEPTION ConnectionRefusedError(61, 'Connection refused')
19:12:33 CRASH cycle 2 step a-or-b (connection lost; last logged step above) PC 1025f04a
19:13:05 rebooted
19:13:25 cycle 3 a START port True procs ['7']
  BB_GATE0=0x100fb368 BB_GATE1=0x1010fdf8 BB_INST=0 BB_VEC0=0xff4c2c BB_VEC1=0x100a8696 MAIN=2 STRUT_GATE0=0x1005f8c8 STRUT_GATE1=0x1008e7a8 STRUT_INST=0 STRUT_VEC0=0xff4c3c STRUT_VEC1=0xff4cbe WBR_GATE0=0x100fb338 WBR_GATE1=0x100fb350 WBR_INST=0 WBR_VEC0=0x100a870a WBR_VEC1=0xff1b66
19:13:29 cycle 3 b BREAK gone True
  BB_GATE0=0x100fb368 BB_GATE1=0x1010fdf8 BB_INST=0 BB_POST0=0xff4c2c BB_POST1=0x100a8696 BB_RC=0 BB_REM=4 BB_VEC0=0xff4c2c BB_VEC1=0x100a8696 BB_WAIT=0 MAIN=4 STRUT_GATE0=0x1005f8c8 STRUT_GATE1=0x1008e7a8 STRUT_INST=0 STRUT_POST0=0xff4c3c STRUT_POST1=0xff4cbe STRUT_RC=0 STRUT_REM=4 STRUT_VEC0=0xff4c3c STRUT_VEC1=0xff4cbe STRUT_WAIT=0 WBR_GATE0=0x100fb338 WBR_GATE1=0x100fb350 WBR_INST=0 WBR_POST0=0x100a870a WBR_POST1=0xff1b66 WBR_RC=0 WBR_REM=4 WBR_VEC0=0x100a870a WBR_VEC1=0xff1b66 WBR_WAIT=0
19:14:01 cycle 4 a START port True procs ['3']
  BB_GATE0=0x1008f840 BB_GATE1=0x1008f858 BB_INST=0 BB_POST0=0xff4c2c BB_POST1=0x100a8696 BB_RC=0 BB_REM=4 BB_VEC0=0xff4c2c BB_VEC1=0x100a8696 BB_WAIT=0 MAIN=2 STRUT_GATE0=0x100557d0 STRUT_GATE1=0x1005a958 STRUT_INST=0 STRUT_POST0=0xff4c3c STRUT_POST1=0xff4cbe STRUT_RC=0 STRUT_REM=4 STRUT_VEC0=0xff4c3c STRUT_VEC1=0xff4cbe STRUT_WAIT=0 WBR_GATE0=0x1008ed18 WBR_GATE1=0x1008ed30 WBR_INST=0 WBR_POST0=0x100a870a WBR_POST1=0xff1b66 WBR_RC=0 WBR_REM=4 WBR_VEC0=0x100a870a WBR_VEC1=0xff1b66 WBR_WAIT=0
19:14:26 cycle 4 b BREAK gone True
  BB_GATE0=0x1008f840 BB_GATE1=0x1008f858 BB_INST=0 BB_POST0=0xff4c2c BB_POST1=0x100a8696 BB_RC=0 BB_REM=4 BB_VEC0=0xff4c2c BB_VEC1=0x100a8696 BB_WAIT=0 MAIN=4 STRUT_GATE0=0x100557d0 STRUT_GATE1=0x1005a958 STRUT_INST=0 STRUT_POST0=0xff4c3c STRUT_POST1=0xff4cbe STRUT_RC=0 STRUT_REM=4 STRUT_VEC0=0xff4c3c STRUT_VEC1=0xff4cbe STRUT_WAIT=0 WBR_GATE0=0x1008ed18 WBR_GATE1=0x1008ed30 WBR_INST=0 WBR_POST0=0x100a870a WBR_POST1=0xff1b66 WBR_RC=0 WBR_REM=4 WBR_VEC0=0x100a870a WBR_VEC1=0xff1b66 WBR_WAIT=0
19:14:52 cycle 5 a START port True procs ['3']
  BB_GATE0=0x1008ecd8 BB_GATE1=0x1008ecf0 BB_INST=0 BB_POST0=0xff4c2c BB_POST1=0x100a8696 BB_RC=0 BB_REM=4 BB_VEC0=0xff4c2c BB_VEC1=0x100a8696 BB_WAIT=0 MAIN=2 STRUT_GATE0=0x10012aa0 STRUT_GATE1=0x10059aa0 STRUT_INST=0 STRUT_POST0=0xff4c3c STRUT_POST1=0xff4cbe STRUT_RC=0 STRUT_REM=4 STRUT_VEC0=0xff4c3c STRUT_VEC1=0xff4cbe STRUT_WAIT=0 WBR_GATE0=0x10059ad0 WBR_GATE1=0x1008eb38 WBR_INST=0 WBR_POST0=0x100a870a WBR_POST1=0xff1b66 WBR_RC=0 WBR_REM=4 WBR_VEC0=0x100a870a WBR_VEC1=0xff1b66 WBR_WAIT=0
19:14:57 cycle 5 b BREAK gone True
  BB_GATE0=0x1008ecd8 BB_GATE1=0x1008ecf0 BB_INST=0 BB_POST0=0xff4c2c BB_POST1=0x100a8696 BB_RC=0 BB_REM=4 BB_VEC0=0xff4c2c BB_VEC1=0x100a8696 BB_WAIT=0 MAIN=4 STRUT_GATE0=0x10012aa0 STRUT_GATE1=0x10059aa0 STRUT_INST=0 STRUT_POST0=0xff4c3c STRUT_POST1=0xff4cbe STRUT_RC=0 STRUT_REM=4 STRUT_VEC0=0xff4c3c STRUT_VEC1=0xff4cbe STRUT_WAIT=0 WBR_GATE0=0x10059ad0 WBR_GATE1=0x1008eb38 WBR_INST=0 WBR_POST0=0x100a870a WBR_POST1=0xff1b66 WBR_RC=0 WBR_REM=4 WBR_VEC0=0x100a870a WBR_VEC1=0xff1b66 WBR_WAIT=0
19:15:22 cycle 6 a START port True procs ['3']
  BB_GATE0=0x1008e398 BB_GATE1=0x1008e3b0 BB_INST=0 BB_POST0=0xff4c2c BB_POST1=0x100a8696 BB_RC=0 BB_REM=4 BB_VEC0=0xff4c2c BB_VEC1=0x100a8696 BB_WAIT=0 MAIN=2 STRUT_GATE0=0x10061488 STRUT_GATE1=0x10065ae8 STRUT_INST=0 STRUT_POST0=0xff4c3c STRUT_POST1=0xff4cbe STRUT_RC=0 STRUT_REM=4 STRUT_VEC0=0xff4c3c STRUT_VEC1=0xff4cbe STRUT_WAIT=0 WBR_GATE0=0x1008e368 WBR_GATE1=0x1008e380 WBR_INST=0 WBR_POST0=0x100a870a WBR_POST1=0xff1b66 WBR_RC=0 WBR_REM=4 WBR_VEC0=0x100a870a WBR_VEC1=0xff1b66 WBR_WAIT=0
19:15:25 cycle 6 EXCEPTION ConnectionRefusedError(61, 'Connection refused')
19:15:33 connection lost, no Exception 3 line: treating as reset
19:16:00 rebooted
19:16:20 cycle 7 a START port True procs ['7']
  BB_GATE0=0x1014b6d8 BB_GATE1=0x1014b6f0 BB_INST=0 BB_VEC0=0xff4c2c BB_VEC1=0x1008ffd6 MAIN=2 STRUT_GATE0=0x1005a848 STRUT_GATE1=0x100611a0 STRUT_INST=0 STRUT_VEC0=0xff4c3c STRUT_VEC1=0xff4cbe WBR_GATE0=0x1013f0c0 WBR_GATE1=0x1014b6c0 WBR_INST=0 WBR_VEC0=0x1009004a WBR_VEC1=0xff1b66
19:16:24 cycle 7 b BREAK gone True
  BB_GATE0=0x1014b6d8 BB_GATE1=0x1014b6f0 BB_INST=0 BB_POST0=0xff4c2c BB_POST1=0x1008ffd6 BB_RC=0 BB_REM=4 BB_VEC0=0xff4c2c BB_VEC1=0x1008ffd6 BB_WAIT=0 MAIN=4 STRUT_GATE0=0x1005a848 STRUT_GATE1=0x100611a0 STRUT_INST=0 STRUT_POST0=0xff4c3c STRUT_POST1=0xff4cbe STRUT_RC=0 STRUT_REM=4 STRUT_VEC0=0xff4c3c STRUT_VEC1=0xff4cbe STRUT_WAIT=0 WBR_GATE0=0x1013f0c0 WBR_GATE1=0x1014b6c0 WBR_INST=0 WBR_POST0=0x1009004a WBR_POST1=0xff1b66 WBR_RC=0 WBR_REM=4 WBR_VEC0=0x1009004a WBR_VEC1=0xff1b66 WBR_WAIT=0
19:16:35 cycle 8 EXCEPTION ConnectionRefusedError(61, 'Connection refused')
19:16:37 CRASH cycle 8 step a-or-b (connection lost; last logged step above) PC 102674da
19:17:09 rebooted
19:17:29 cycle 9 a START port True procs ['7']
  BB_GATE0=0x10114008 BB_GATE1=0x10114050 BB_INST=0 BB_VEC0=0xff4c2c BB_VEC1=0x1008ffd6 MAIN=2 STRUT_GATE0=0x1005b128 STRUT_GATE1=0x1005c3c0 STRUT_INST=0 STRUT_VEC0=0xff4c3c STRUT_VEC1=0xff4cbe WBR_GATE0=0x10113fd8 WBR_GATE1=0x10113ff0 WBR_INST=0 WBR_VEC0=0x1009004a WBR_VEC1=0xff1b66
19:17:34 cycle 9 b BREAK gone True
  BB_GATE0=0x10114008 BB_GATE1=0x10114050 BB_INST=0 BB_POST0=0xff4c2c BB_POST1=0x1008ffd6 BB_RC=0 BB_REM=4 BB_VEC0=0xff4c2c BB_VEC1=0x1008ffd6 BB_WAIT=0 MAIN=4 STRUT_GATE0=0x1005b128 STRUT_GATE1=0x1005c3c0 STRUT_INST=0 STRUT_POST0=0xff4c3c STRUT_POST1=0xff4cbe STRUT_RC=0 STRUT_REM=4 STRUT_VEC0=0xff4c3c STRUT_VEC1=0xff4cbe STRUT_WAIT=0 WBR_GATE0=0x10113fd8 WBR_GATE1=0x10113ff0 WBR_INST=0 WBR_POST0=0x1009004a WBR_POST1=0xff1b66 WBR_RC=0 WBR_REM=4 WBR_VEC0=0x1009004a WBR_VEC1=0xff1b66 WBR_WAIT=0
19:17:45 cycle 10 EXCEPTION ConnectionRefusedError(61, 'Connection refused')
19:17:46 CRASH cycle 10 step a-or-b (connection lost; last logged step above) PC 10264fda
19:18:18 rebooted
19:18:18 DONE crashes 4

## Pass 1 (cycles 1-2)
19:05:47 cycle 2 EXCEPTION ConnectionRefusedError(61, 'Connection refused')
    say('CRASH cycle', cyc, 'step', step, 'PC', pc[1], flush=True)

## Pass 1 (cycles 3-12; cycles 9 and 10 INVALID: no reboot wait after bare reset in cycle 8)
19:07:07 cycle 4 EXCEPTION ConnectionRefusedError(61, 'Connection refused')
19:07:09 CRASH cycle 4 step a-or-b (connection lost; last logged step above) PC 10262f22
19:07:41 rebooted
19:08:16 cycle 6 EXCEPTION ConnectionRefusedError(61, 'Connection refused')
19:08:18 CRASH cycle 6 step a-or-b (connection lost; last logged step above) PC 102674ea
19:08:50 rebooted
19:09:45 cycle 8 EXCEPTION ConnectionRefusedError(61, 'Connection refused')
19:09:54 cycle 9 EXCEPTION ConnectionRefusedError(61, 'Connection refused')
19:10:03 cycle 10 EXCEPTION ConnectionRefusedError(61, 'Connection refused')
19:10:48 cycle 12 EXCEPTION ConnectionRefusedError(61, 'Connection refused')
19:10:50 CRASH cycle 12 step a-or-b (connection lost; last logged step above) PC 10266ca2
19:11:22 rebooted
19:11:23 DONE crashes 3

## Readings
- No remover took the busy leak return: REM=3 never seen, REM=9 never seen, in any complete step-b reading, both passes. Every complete reading: REM=4, RC=0, WAIT=0, POST equal to VEC (vectors restored to pre-arm values).
- Every install returned 0 on every start (STRUT_INST, WBR_INST, BB_INST).
- Vectors after a clean Break pointed at the pre-arm targets, not at an old gate. UNVERIFIED: VEC at next start compared to earlier GATE: not done; VEC1 of WBR/BB were RAM addresses that differ between boots, none equal to a logged GATE by eye.
- Crashes pass 2: Exception 3 in cycles 2 (PC 1025f04a), 8 (PC 102674da), 10 (PC 10264fda); cycle 6 connection lost with no Exception line, treated as reset. Pass 1: Exception 3 cycles 2 (102597ba), 4 (10262f22), 6 (102674ea), 12 (10266ca2); cycle 8 bare CPU reset.
- Crash step: always in the cycle after a clean Break, 5 to 8 s after Run, after the port came up during the step-a marker read (or just before it completed). Step-a markers of crash cycles are partly or fully missing, so the new run's own install values for those cycles are UNVERIFIED. Last good reading before each crash is a clean step b.
- Marker read takes about 15 s and may itself affect timing; UNVERIFIED whether crashes occur without the marker polling.
- Stale-marker caveat: ENV markers persist across runs in a boot; a cycle whose step a shows previous REM=4 values means the new run had not yet reached its remover.
- Not measured: cause of the crash; which code is at the PCs (0x1025f04a etc. are RAM addresses, not resolved).
