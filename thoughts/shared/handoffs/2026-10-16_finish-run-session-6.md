---
date: 2026-10-16
topic: finish-open-plans run, session 6
ledger: thoughts/shared/plans/2026-10-15-finish-open-plans.md
previous: thoughts/shared/handoffs/2026-10-16_finish-run-session-5.md
---

# Finish-open-plans run, session 6

Ledger is the source of truth; grep `^- \[ \]` and read the "session 6" notes under FP0.2, FP3.4, FP3.6.

## Done this session
- PROF sub-counter diff dropped (owner approved); kept as thoughts/shared/patches/2026-10-16_prof-rest-subcounters.patch.
- FP3.4 instrument fixed (4a909c3e): PROF=1 prints phases via PROF_LOG to serial without the DBG trace;
  SERIAL=1 no longer needed. Test: tools/test_handler_prof.py.
- tmux_rig stale-binary guard (63c62c06): refuses a tmux-bin that fails tests/test_stack_cookie.py.
  Test: tools/test_rig_tmux_guard.py.
- Clean re-measure (001a6801, research/2026-10-16_fp34-rest-clean.md): rest ~0.2 ms a write net of ReadEClock.
  ASM1 target #1 = painter on colour shapes (paint ~1 s), #2 = row composition.

## Open, in order
1. Owner decisions: drop FP3.6 row 12 (no gain left in rest)? Cursor pass 2.7 ms a frame on sync-line:
   investigate as algorithm (what the cursor pass redraws) before ASM1?
2. ASM1 needs a renderer profile: prof68k covers only engbench. Next: paintbench on the colour shapes, or
   extend prof68k to the painter. Row 7 (one-plane pen-1-on-0) belongs here.
3. FP4.1 batch A (bc, ed, column/iconv, ctags, jq), then FP4.2 B, FP4.4 D: upterm-ports, one sonnet rig agent
   at a time, rig 2 or 3, never rig 1.
4. FP1.10 waits on owner: iTerm2 Python API on + `pip3 install iterm2` (plan research/2026-10-15_fp45-fp110.md:84).
   FP1.9 needs a quiet Mac.
5. Phase 5: cost checkpoint and owner-approved plan before any code.

Blocked on owner (unchanged): FP3.9, FP3.8, FP4.7, FP0.6.
