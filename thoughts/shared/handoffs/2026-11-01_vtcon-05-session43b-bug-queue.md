---
date: 2026-11-01
session: vtcon-05 session 43 (second half)
branch: main
head: 6e6264f6 (pushed)
status: unfinished, nothing in flight
---

# vtcon-05 session 43b: bug queue Q1-Q9

Ledger: thoughts/spot/todos.md, section "vtcon-05 s43 bug queue" (rows Q1-Q9). Triage: thoughts/spot/vtcon05-bug-triage-s43.md.

## Done
- Q2 DIST-CHECK-UHC: fdb0d707, pushed.
- Q4 rig test commit f4ab23cc pushed (merge 5a5d36b8).
- Disk cleanup: rigs 4, 5, 6 folders, old vtcon scratch, Google/playwright/Adobe caches removed (owner approved). 10 GB free after.

## In flight at restart (check before anything else)
- Q4 VSH-JOB-SIGNALS: DONE and pushed (fe76c6a7 own handle per job, 010faab9 reap seeks shell handle to job end; merge 6e6264f6). Rig 6 proof: kill test old vsh fails round 2, fixed 4 of 4; order test "one two three" 3 of 3. Pipes fall back to NIL: for background output.
- Q1 rig 4 teardown freeze: NOT REPRODUCED, 20 of 20 rounds pass on fixed FS-UAE with the splits build (findings rig4-teardown-freeze-s43.md). Row left unticked: decide close vs rerun on fs-uae.pre-s43 to tie it to fix B. Rig 4 stopped.

## Next
- Q3 (SIGWINCH leak at pane close) after Q1 result; then Q5-Q9 in order.
- fs-uae PRs #393/#394 upstream review pending.
- wasabi pin moved by another session to creep-ltx/wasabi b9d1812 (PRs #2-#4 merged).

## Gotchas
- Never touch rigs 1, 2, 3, 8, 10, 14 (other sessions).
- MusicPlayer scratch partly in use by a live session; left.
