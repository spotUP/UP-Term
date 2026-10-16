---
date: 2026-10-16
topic: finish-open-plans run, session 5
ledger: thoughts/shared/plans/2026-10-15-finish-open-plans.md
previous: thoughts/shared/handoffs/2026-10-16_finish-run-session-4.md
---

# Finish-open-plans run, session 5

Ledger is the source of truth; grep `^- \[ \]` and read the "2026-10-16 session 5" notes under each item.

## Done this session
- FP3.7 closed: DIRECT=1 gives no gain on the cycle-exact 020 (within 5%, single runs); stays opt-in.
  research/2026-10-16_fp37-exact-direct.md.
- FP3.4 measured three ways (060 split, --exact split, rest split). research/2026-10-16_fp34-phase-split.md,
  2026-10-16_fp37-exact-direct.md, 2026-10-16_fp34-rest-split.md.
- FP0.2 cause found: stale tmux-amiga/build/tmux-bin without the $STACK cookie (tmux-amiga 0d30fa7). Rebuilt
  (owner approved). A rig 2 re-run of resize_rig.py was running at session end: check the ledger FP0.2 note
  for its result; if absent, re-run it.

## Open, in order
1. Uncommitted in the working tree: handler/vtcon_handler.c, 187+/16- PROF-only sub-counters (under
   VTCON_PROF) from the rest-split agent. Review, then either commit as instrumentation or drop it. Do not
   build on its sub-0.1 ms numbers: ReadEClock costs 41 us a call on the 020.
2. Fix the instrument (FP3.4): PROF=1 SERIAL=1 turns on the per-packet DBG trace ("packet" :6065, "reply"
   :486, ~5.4 ms each). Phase output to serial must not enable that trace. Then re-measure rest with one timer
   pair around 100 packets (or the emulator cycle counter).
3. Then decide ASM1 target and FP3.6 rows 7 and 12 from the clean numbers. Known candidate: service_reads runs
   twice per WRITE packet (after the reply at :6233 and in the loop tail).
4. FP0.2: nvim "truth None" (ixwinch printed nothing in 4 s) if the re-run still shows it.
5. Handoff-4 step 3: FP4.1/4.2/4.4 port batches, FP1.10 (writing). FP1.9 needs a quiet Mac.
6. Phase 5: cost checkpoint and owner-approved plan before any code.

## Blocked on owner
- FP3.9: tools/rig/cc_rig.py only on branch feature/cc1-cc2 (another session's).
- FP3.8: branch feature/r1-rom belongs to another session.
- FP4.7: changes upterm-ports (other repo), needs go.
- FP0.6: who commits upterm-ports recipe.mk.

## Gotchas
- --exact boots sometimes hang ("daemon did not answer for 150 s"): stop the rig and reboot; no data lost.
