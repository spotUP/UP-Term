---
date: 2026-10-16
topic: finish-open-plans run, session 4 handoff
ledger: thoughts/shared/plans/2026-10-15-finish-open-plans.md
---

# State

Done this session (main, 3 commits ahead of origin until pushed: e5657808, 2eadd490, 66a88542):
- gcc addx/Z fix installed, test `make test ONLY=gcc_mul_overflow_cc` fails on old cc1 (3 ERROR),
  passes on new. PR https://codeberg.org/bebbo/gcc/pulls/15 (FP4.7).
- rig `--a500` boots (870da016); W7 T1 pre-check 2 and FP3.3 measured
  (research/2026-10-16_w7-68000-engbench.md). engbench skips the renderer check on a 68000.
- screen_text stale-clipboard fix (7df72254), FP4.5 winsize check stays a separate script.

# Next, in order

1. FP4.7: needs owner go (upterm-ports is another repo): rebuild the 33 binaries listed in
   research/2026-10-16_gcc-addx-cc.md, drop tools/m68k-cc workaround, cp/du/ls on a rig.
2. Rig work, one agent each (sonnet, rig 2 or 3, never rig 1): FP0.2, FP3.4, FP3.6 (rows 5, 7,
   10, 12 of research/2026-10-04_ccon-b9-techniques-vs-ours.md), FP3.7, FP3.9.
3. FP4.1/4.2/4.4 port batches; FP1.9 needs a quiet Mac; FP1.10 writing only.
4. Phase 5: cost checkpoint + owner-approved plan before code.

Owner questions still open: FP0.6 who commits upterm-ports recipe.mk.
