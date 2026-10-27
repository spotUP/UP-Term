# Amarchy drawn menu warm server: timing evidence (rig 7, 060, 2026-10-27)

Plan: thoughts/shared/plans/2026-10-27-amarchy-gui-warm.md items G9, G11. Script: `UPTERM_RIG=7 python3 tools/rig/amarchy_timing_rig.py --drawn --no-build`
(menu-in-terminal = graphical written to ENV:Amarchy/prefs so the drawn menu opens over the rig's host terminal).
Marks are cumulative ticks since the hotkey mark, DateStamp 20 ms resolution; font = pick_font returned, icons = ig_open returned
(at_mark "font" / "icons" added to gui_open in amarchy/gui.c). Cost of font = font - start, icons = icons - font.

Rows labelled "cold" in the script run are flushed memory (Avail FLUSH) with the warm server still running. True cold = server
killed + stale port + flush (the fallback via act_run), measured separately below.

## Summary
- warm ready - hotkey (final run): 120, 140, 180 ms, median 140 ms (earlier runs: 100/120/140, 120/120/180; medians 120)
- flushed-memory but server-warm ("cold" rows): median 160 ms (140/160/180)
- true cold fallback (server killed, flush): 140, 180, 140 ms (median 140 ms), marks hotkey,start,window,ready, no send
- font cost (start -> font): 20-40 ms (warm median 40 ms at 20 ms resolution); icons cost (font -> icons): 0 ms in 5 of 6 runs, 20 ms once
- marks font/icons kept in gui.c: cheap (two file appends, only when AMARCHY_TIMING is set) and they show where time goes
- G9 verdicts: all PASS (send mark, same port owner, marks in order, one window then gone)
- G11 verdicts: all PASS (below)

## Raw output (final run, rc=0)
```
cold 1: {'hotkey': 0, 'send': 0, 'start': 0, 'font': 20, 'icons': 20, 'window': 60, 'ready': 140}
cold 2: {'hotkey': 0, 'send': 0, 'start': 0, 'font': 20, 'icons': 20, 'window': 60, 'ready': 160}
cold 3: {'hotkey': 0, 'send': 20, 'start': 20, 'font': 40, 'icons': 60, 'window': 100, 'ready': 180}
warm 1: {'hotkey': 0, 'send': 0, 'start': 0, 'font': 40, 'icons': 40, 'window': 60, 'ready': 120}
warm 2: {'hotkey': 0, 'send': 0, 'start': 0, 'font': 40, 'icons': 40, 'window': 80, 'ready': 140}
warm 3: {'hotkey': 0, 'send': 20, 'start': 20, 'font': 40, 'icons': 40, 'window': 80, 'ready': 180}
PASS the port owner is a live task before the opens 0x108985a0
PASS open 1 marks in order hotkey/send/start/window/ready ['hotkey', 'send', 'start', 'window', 'ready']
PASS open 1 hotkey to ready <= 500 ms 120 ms
PASS open 1 served by the same port owner 0x108985a0
PASS open 1 window up, then gone after Escape 
PASS open 2 marks in order hotkey/send/start/window/ready ['hotkey', 'send', 'start', 'window', 'ready']
PASS open 2 hotkey to ready <= 500 ms 160 ms
PASS open 2 served by the same port owner 0x108985a0
PASS open 2 window up, then gone after Escape 
PASS open 3 marks in order hotkey/send/start/window/ready ['hotkey', 'send', 'start', 'window', 'ready']
PASS open 3 hotkey to ready <= 500 ms 180 ms
PASS open 3 served by the same port owner 0x108985a0
PASS open 3 window up, then gone after Escape 
PASS viewer open (second size) works 80 ms marks ['hotkey', 'send', 'start', 'window', 'ready']
PASS Ctrl-C (drawn: the hotkey again) closes the menu, the port owner stays alive 0x108985a0
PASS open after Ctrl-C is warm again ['hotkey', 'send', 'start', 'window', 'ready']
PASS owner removed, port left behind (stale) owner 0x108985a0
PASS stale port: the open falls back cold (menu opens, no send mark) ['hotkey', 'start', 'window', 'ready'] owner 0x108985a0
PASS next open is warm on a restarted server (new port owner) ['hotkey', 'send', 'start', 'window', 'ready'] owner 0x107eea10
PASS commodity quit: AMARCHY.GUISRV gone, server task ended owner 0x107eea10 port listed False
PASS after the quit a restarted commodity serves warm again ['hotkey', 'send', 'start', 'window', 'ready'] owner 0x103506f0
cold start median 0 ms (runs [0, 0, 20])
cold window median 60 ms (runs [60, 60, 100])
cold font median 20 ms (runs [20, 20, 40])
cold icons median 20 ms (runs [20, 20, 60])
cold ready median 160 ms (runs [140, 160, 180])
warm start median 0 ms (runs [0, 0, 20])
warm window median 80 ms (runs [60, 80, 80])
warm font median 40 ms (runs [40, 40, 40])
warm icons median 40 ms (runs [40, 40, 40])
warm ready median 140 ms (runs [120, 140, 180])
```

## True cold (server killed, stale port, Avail FLUSH)
```
truecold 1 {'hotkey': 0, 'start': 20, 'font': 20, 'icons': 20, 'window': 60, 'ready': 140}
truecold 2 {'hotkey': 0, 'start': 20, 'font': 40, 'icons': 40, 'window': 100, 'ready': 180}
truecold 3 {'hotkey': 0, 'start': 0, 'font': 20, 'icons': 40, 'window': 80, 'ready': 140}
warm after {'hotkey': 0, 'send': 0, 'start': 20, 'font': 40, 'icons': 40, 'window': 80, 'ready': 140}
```

## Earlier runs
Run 1 failed only on rig-script faults (window check looked for an XCON window; the drawn window is titled "Amarchy", task amarchy-gui). Run 2 failed Ctrl-C steps: the drawn menu ignores the Ctrl-C key, it closes on the hotkey pressed again (gui.c:968), so the script uses that for --drawn. Run 3 is clean.
