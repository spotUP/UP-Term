---
date: 2026-11-06
session: up-term-49 (sonnet measurement agent, hunt 5)
build: branch restart-measure f33e987b + uncommitted AMDBG gate-check code (amarchy/strut_patch.c, .h, bar.c, amarchy.c)
verdict: NO ANSWER to (a)-(d); the instrument did not deliver. Two instrument bugs below.
---

# Amarchy restart crash, hunt 5 (gate check)

## 1. Rig and recipe
Rig 177, `rig.py start --max --mmu --os 32 --purpose "up-term-49: restart crash hunt 5 gate check"`, `amarchy_gui_rig.py --setup --no-build` (known FAIL "flat prefs off"), `rig.py muforce on`. Loop: scratchpad loop5.py (copy of hunt 4 loopm.py plus a log read). Code added: strut_patch_check() under Forbid for g_pw and g_pt (rebuild expected 22 bytes, compare, compare *slot with gate; latch, print one block), called in plain_hook/tags_hook before the call through ipatch_saved (falls back to sw.saved on a difference), and at loop-start, loop-before-tick, loop-after-tick, loop-end, bar_tick-start/after-sentinels/end, install-end.

## 2. Run 1: serial output via exec RawPutChar (LVO -516)
serial.log stayed 0 bytes through 4 clean cycles, so the install line and any block were not captured (UNVERIFIED whether RawPutChar printed anything). Cycle 4 Break was clean; the Run of cycle 5 never answered on the port ("port up False") and died about 6 s after Run with Exception 3. MuForce (verbatim, hits.log copy at the scratchpad h5hits.log):
```
BYTE READ from 000000B5   PC: 40AC70A0  (9 distinct hits, B5..BC and more, same PC, hunk 0 offset 00053FD8)
Exception !!   00000003   TCB: 40A03420
  PC: 40B6A6EE   (in the data/stack area, odd)
Data: 40B6A6DF 0000002E 0000006F 40B6BD8C 40AC0EDC 00000000 00000004 00000001
Addr: 00000000 40B6BE08 40B6A6D8 001A1B7C ...
Name: "Background CLI"  CLI: "VTCX:Apps/bin/Amarchy"
```
This differs from hunt 4 (there PC was jsr (a2) in tags_hook with A2 = "kyo-"). Here the PC 40AC70A0 (hunk offset 0x53FD8 in this build) reads bytes at 0xB5..0xBC in a loop, a string walk starting at a near-zero pointer; D0 = 0x2E ('.') and 0x2F suggest a text walk over "..", then a jump into data at 40B6A6EE. The function at offset 0x53FD8 was not resolved (no map of this build made). UNVERIFIED whether this is the same corruption as hunt 4.

## 3. Run 2: log via RAM:amarchy-gate.log (flush from the Amarchy process)
Cycle 1 printed the install line (verbatim):
```
AMDBG install: gate0=40057A58 gate1=40058640 vec0=40057A58 vec1=40058640 saved0=00FF4C3C saved1=00FF4CBE
```
So in a clean instance, vec = gate for both gates at install end (this is the state at "install-end"; no check block printed in cycle 1). After cycle 1 the flush function wrote a file of 1033609216 bytes (strut_patch_flush: Seek(f,0,OFFSET_END) then Write on a MODE_READWRITE handle; cause not analysed), the rig ran out of memory and `Type` failed. No further cycles.

## 4. Answers
- (a) g_pt.gate == *g_pt.slot: PROVEN only for the install moment of a clean instance (gate1 = vec1 = 40058640). At the bad moment: UNVERIFIED.
- (b) differing bytes / text: UNVERIFIED (no block captured).
- (c) first point in the main loop: UNVERIFIED.
- (d) task that runs the hook: UNVERIFIED (hunt 4: "Background CLI", Amarchy's own process, still the only evidence).

## 5. Not measured / next
- Capture channel: serial.log gets nothing from RawPutChar on this rig. A working channel is the in-memory buffer already in strut_patch.c (g_dbg_buf, 3072 bytes) flushed from the Amarchy process; its flush must be fixed (write the whole buffer once with Open(MODE_NEWFILE) per flush, no Seek) or the buffer read after a crash through a MuForce-visible symbol.
- The cycle-5 crash (byte reads from 0xB5) needs a link map of this exact build to name the function at hunk offset 0x53FD8.

## 6. Cleanup
Rig 177 stopped (rig.py stop removed build/rig177), no fs-uae of mine running. Left: scratchpad loop5.py, loop5.log, loop5b.log (small), h5hits.log (6 KB), h5build.log. AMDBG code uncommitted in the worktree.
