# TE10-MATCH-BACKTRACK: gt_screen_match made linear

Invariant: matching cost is bounded by pattern length times name length. The old recursive matcher was exponential
(`#?a#?a#?a#?a#?a#?ab` against 24 x `a`).

Design:
- Non-recursive state-set (NFA) simulation, no malloc; two byte arrays of GT_SCR_PAT + 2 on the stack.
- Pattern tokenised on the fly: `#c` (repeat c, `#?` any) when `#` has a following char, else one literal / `?`. Lone trailing `#` is a literal.
- reach[i] = token starting at offset i can be read next; repeat tokens are skipped by closure to i + 2.
- Per name char one pass over the pattern: O(plen * nlen). Pattern longer than GT_SCR_PAT - 1 returns 0 (the parser cannot produce one).
- Counter `gt_screen_match_steps` (gthemes_core.h) counts one unit per inner step; the old code had it too for the red proof.

Tests (tests/test_gthemes.c, screen-rules block): differential test against `ref_screen_match` (old matcher copy), all patterns 0..5 over
{a,b,?,#} x all names 0..4 over {a,b,A} (1365 x 121 pairs), 0 differences; bound test steps <= 4*(plen+1)*(nlen+1) for the crafted
pattern (result 0) and, guarded by the crafted bound (old code would not finish), for 138/139-character `#?a` patterns.

Red (old matcher + counter, final test code): `FAIL tests/test_gthemes.c:731: w <= 4UL * (strlen(crafted) + 1) * (strlen(name) + 1) = 0, want 1`
-> `[FAIL] gthemes: 6379 checks, 1 failed` (differential check passed: no behaviour difference).
Green: `RUNTEST gthemes rc=0 [OK] gthemes: 6383 checks, 0 failed` (baseline 6375).
Second revert-and-rerun not done (budget); the red run above used the identical final test file.
m68k: `make build/amiga/{Amarchy,amarchy-gui,amarchy-menu,amarchy-index,amarchy-icons,amarchy-flat,upprefs}` rc=0.
