---
date: 2026-11-01
session: vtcon-05 s42
topic: wasabi PR #3 rebase, A1200 drift covers LIBS:, window-close fixed on the A1200, WB-PARK-ZORDER 3.2 done
---

# vtcon-05 session 42 handoff

## Done
- Wasabi PR #3 (pr/selecta-lists) rebased onto 4bb21c3 (f3c19fe, force-with-lease approved by the owner).
  PRs #2, #3 and #4 are mergeable on creep-ltx main e8fc865. The owner posted the merge ask:
  https://github.com/creep-ltx/wasabi/pull/2#issuecomment-6556608974 (text in thoughts/spot/wasabi-merge-ask.md).
- A1200-DRIFT-LIBS (main 5e1eea66): tools/rig/a1200_drift.py now checks LIBS:ixemul.library and
  LIBS:ixnet.library against paths.py (--nofpu picks IXEMUL_LIB_SOFT); install_map skips only the unbuilt
  sources of a Copy line (one unbuilt program hid all 8 Amarchy programs); the script inserts its own drawer
  on sys.path so the documented `python3 -I` run works. tests/test_a1200_drift.py 11 pass, each new row
  revert-checked.
- WINDOW-CLOSE-KILLS-APP done on the A1200: ixemul a8a7bf4c and handler dc2a9265 installed (backups
  LIBS:ixemul.library.s41bak and L:vtcon-handler.s41bak left on the A1200); nano and vsh close PASS.
- WB-PARK-ZORDER done: 3.2 rig 6 (a) 2/2 PASS, (c) late drawer reopen keeps the drawer in front (expected).
  Evidence thoughts/spot/term-findings/wbpark-zorder-32.md.

## Open, in order
1. A1200 is a full kit behind: live drift (scratchpad drift.log, 11-01) = 6 same, 27 DIFF, amarchy-flat
   missing. Ask the owner before refreshing it with the kit. Every A1200 brief runs a1200_drift first.
2. MG-CLOSE-WINDOW-STAYS (new row): closing a window running mg ends mg and vsh, but the window and its
   XCON task stay until a second close click. Hunt cause in the handler close path (opus if unknown).
3. WASABI-UPSTREAM-PRS: wait for upstream merge, then WASABI-PIN-UPSTREAM (tools/nas/wasabi.rev f59c839,
   wasabi.repo spotUP fork -> creep-ltx main, make wasabi, one rig check). Delete the scratchpad
   wasabi-fork clone after the merge.
4. RIG-BATCH-DAILY (1) upkg 0053 / AMITCP-GONE / IXNET-UG-NOREQ need an upterm-ports kit build.
5. NVIM-HEAP-CORRUPT (opus hunt), RIG-RECHECK-STALE, SELECTA-SHORTHELP-DANGLING (MusicPlayer session),
   NAS-DEPLOY-AMIGA leftovers, ledger triage (owner direction per section).

## Gotchas
- `make` can fail in host tests before the Amiga build and leave build/amiga stale (s42 rig agent saw two
  NAS Dockerfile asserts; test_nas_deploy.py passes 23/23 on main 5e1eea66). Check build/amiga times.
- Window list order on a rig is not depth; pixel samples are the proof for z-order.
- build/rig6 (1.0 GB) kept for the next rig 6 start.
