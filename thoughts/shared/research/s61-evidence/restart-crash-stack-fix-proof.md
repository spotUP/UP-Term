# Amarchy restart crash: stack 65536 fix proof (rig 177)

Recipe used: `UPTERM_RIG=177 rig.py start --max --mmu --os 32 --purpose "up-term-49: Amarchy restart fix proof"`; `amarchy_gui_rig.py --setup --no-build` (FAIL line 'flat prefs off' as known); `rig.py muforce on`. Binary: build/amiga/Amarchy (HEAD 2ef1c4d1), no rebuild, no AMDBG. Script scratchpad/loopfix.py: Break the setup instance, then per cycle Run VTCX:Apps/bin/Amarchy, wait for port AMARCHY, +20 s, record, Break, wait until gone. Crash test: Exception/RESET line in rig fs-uae log, port not answering, MuForce hits.log growth.

Binary check: `List VTCX:Apps/bin/Amarchy` = 367256 bytes (matches build/amiga/Amarchy).

| cycle | crash | Exception text | MuForce hits | ps stack line (STACK / FREE) |
|---|---|---|---|---|
| 1-12 (each) | no | none | 0 (hits.log 0 bytes) | proc 1 wait 65536 65306 CLI 3 Background CLI (VTCX:Apps/bin/Amarchy) |

All 12 cycles gave the identical ps line (FREE 65306 at +20 s). Port answered in every cycle, every Break completed. fs-uae log: 0 Exception lines.

Verdict: PASS 12/12

Not measured: more than 12 restarts; the 16384-byte stack build on this run (no control run in this session); stack low-water over a longer uptime or under UI activity (ps FREE read once, at +20 s idle); first-start-after-boot behaviour; the 'flat prefs off' setup FAIL; Break timing other than after 20 s.
