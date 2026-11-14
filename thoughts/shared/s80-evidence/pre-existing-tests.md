# Pre-existing test problems (s80-pretests)

## 1. test_tutorial_wiring WindowStateWiring (stale test)
The "p1 bump-point" failure is the same test (test_p1_each_action_has_its_bump_point): it was the only failing one.
handler/vtcon_handler.c still has the wiring (h_clipped notes STATE_COPY/STATE_PASTE and sits in the vtwin_host table);
the table gained a trailing h_rip_block hook, so the pinned `h_knob, h_clipped };` no longer matched. Fixed the test.

## 3. make test ONLY=packages: "recipe.mk has no git_SUMMARY" (checkout, not vtcon)
vtcon's check (tools/pkgs.py summary(): hand-built SUMMARY dict, else the recipe's <id>_SUMMARY) is right: git moved to
its recipe in aabb311c. upterm-ports commit 687392b "git recipe: add git_SUMMARY" is on main (and fetch-mirrors), but the
checkout at /Users/spot/Code/UP-Term/upterm-ports is on branch port/htop, which lacks it. No vtcon change. Proposed change
in upterm-ports if that branch must carry it: add to pkgs/git/recipe.mk
  git_SUMMARY   := git 2.56 version control, local commands only, in UP-Term:bin and UP-Term:share/git-core
Workaround: UPTERM_PORTS=/Users/spot/Code/UP-Term/upterm-ports/.claude/worktrees/kit-main (main).

Verified: with UPTERM_PORTS pointing at the main checkout, `make test ONLY=packages` no longer raises the git_SUMMARY
error. One other failure remains, not among the three: tests/test_dist_ipk.py:117 expects 'Version: 2.1' but the feed
Version is '2.1-r1' (revision suffix added in b8409589; the test is stale or the nvim revision differs by ports checkout).

## 2. claude_repl with a long TMPDIR (stale test buffer)
test_reach (not test_gaps_more) had `char s[200]` for the ListS tool result, which holds the start directory path;
with TMPDIR = the 96-character scratchpad the path is ~200 characters, json_str() cut it and the strstr check failed
(tests/test_claude_repl.c:633). Widened to 1024. A TMPDIR of ~180 characters makes the start directory (283) exceed
the product's own cl_tools.root[256]; that is a real product limit, not tested here, and the suite then fails wholesale.
The second failure seen once (line 4707, Bash timeout/output cut) did not recur after the fix: a 1 s timeout under
machine load, flaky, not path related. Result with TMPDIR=scratchpad: 1819 checks, 0 failed.
