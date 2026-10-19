---
date: 2026-10-19
topic: IDE spike vid_puts memo, kitty graphics K9-K13, CTerm painter port
ledger: thoughts/spot/todos.md (sections "2026-10-19 painter / AGA" and "2026-10-19 memo verdict on the 020")
plans: thoughts/shared/plans/2026-10-19-kitty-graphics.md, thoughts/shared/plans/2026-10-19-native-picture-tools.md (later)
---

# Handoff: IDE memo, kitty, painter

Standing goal (owner): "keep goin until the ide is finished". IDE spike = ~/Code/UP-Term/ide-spike
(scidemo-lite on 020 + 68882, AGA). Targets: open <= 2 s, key <= 100 ms, scroll < 300 ms, RAM drop <= 4 MB.

## State at handoff (14:24)

- vtcon is clean. Pushed/committed today: 2f9b8b03 (rig.py screenmode keeps start args, test
  ONLY=rig_screenmode), e040fa08 (kitty engine, by agent), a43a5f78 (painter measurement, by agent).
- Two agents of the OLD session were still running at handoff: kitty engine (afcdac37a8249ea8a) and
  painter port (a64d804fd565b71db, rig 2). Their final reports go to the old session only. Check git log
  and rig 2 (`UPTERM_RIG=2 python3 tools/rig/rig.py status`) for what they left; stop rig 2 if idle
  (never rig 1).
- Rig 3 stopped at handoff. Start it with `make wasabi` then `UPTERM_RIG=3 python3 tools/rig/rig.py start --a1200`; stop it after the bench.

## ide-spike: uncommitted WIP (do not lose)

Working tree holds the memo work, rowcmp already removed:
- scidemo.cxx: vidmemo counters + BENCH vidmemo line; argv[4] off/verify picks memo mode.
- bench_rig.py: machine_ok (avail_start_kb < 100000 for a1200), vidmemo_ok sentinel now >= 30% hits
  and bad == 0 (measured 2090 of 6062 = 34%), rowcmp check dropped, final exit now lists each
  "variant: [FAIL] reason" instead of the misleading "open-path sentinel".
- build/ncurses/src = src.orig + tparm patch + memo (rowcmp removed from curses.priv.h and
  tty_update.c). build/ncurses/obj and out/scidemo-lite rebuilt from it at 14:2x (both make exit 0).
- results/scidemo-lite-a1200.txt holds an old memo run (still has a rowcmp line): re-run overwrites it.
- Backups: ide-spike/build/memo-backup/ (copy of the session scratchpad: ide-spike-wip-with-machine-ok.patch,
  ncurses-src-memo-wip.patch (-p1 from build/ncurses, still WITH rowcmp), result files, run scripts).

Measured on rig 3 020 (valid, avail_start 68445 kB), clean HEAD vs memo, same session:
- HEAD: open 2770/2853, scroll 1366/1371, vid_puts_colour 668 us.
- memo: open 2827/2662, scroll 1259/1239, vid_puts_colour 131 us. Scroll -9%. Verdict: KEEP memo.
- rowcmp skipped 0 of 600 rows: dropped.

## Next, in order

1. Bench the rowcmp-free build on rig 3:
   `cd ~/Code/UP-Term/ide-spike && UPTERM_RIG=3 python3 bench_rig.py --machine a1200 --variants scidemo-lite --runs 2 --force`
   (log to a file; takes several minutes). Expect no [FAIL] lines; vidmemo hits ~34%, scroll ~1250.
2. Write patches/ncurses-vidputs-memo.patch: diff (src.orig + patches/ncurses-tparm-pointer-cache.patch)
   against build/ncurses/src, a/ b/ paths like the tparm patch. Verify: fresh copy of src.orig + both
   patches == build/ncurses/src.
3. Commit in ide-spike by file name (bench_rig.py, scidemo.cxx, patches/ncurses-vidputs-memo.patch,
   results/scidemo-lite-a1200.txt) with the HEAD-vs-memo numbers above; push (spotUP, no attribution).
4. Stop rig 3 (start it before step 1).
5. Kitty: K9 handler datatypes PNG decoder behind vt_set_image_decoder, K11 render host test,
   K12 rig reachability (sentinels: kitty placed counter + datatypes decode count), K13 docs +
   THIRD-PARTY notice (CTerm MIT, Tobias Karlsson). Read the plan checklist first.
6. Painter: read a43a5f78 + the agent's ledger notes; then re-run the ide-spike bench on AGA
   (`rig.py aga` now keeps --a1200).
7. IDE targets still far: open ~2.7 s vs 2 s, scroll ~1.25 s vs 300 ms. Biggest costs: vidputs
   probe ~7.2 s total, mvcur 716 calls / 2.3 s. Next speed hunt goes there (subagent, sonnet).

## Constraints

No AI attribution. Add files by name. No git stash. Never stop rig 1; never reset a rig mid-write.
`make wasabi` before rig start. Images via haiku subagent only. Max 2 agents. Message up-term-45
before touching upterm-ports / kit-stage / upstreaming ncurses patches. CCON: ideas only, no code.
