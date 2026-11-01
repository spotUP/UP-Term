---
date: 2026-11-01
session: vtcon-05 s41
topic: wasabi upstream PRs rig check, WB-PARK-ZORDER revert check, ledger archive
---

# vtcon-05 session 41 handoff

## Done
- WB-PARK-ZORDER (b): ea8dca18 (pushed to main). The "user picked another window" rule moved from
  vtcon_handler.c into wbpark.h (wbpark_user_other) with host tests; WBPARK_RAISE_TRIES pinned to 10.
  Mutation run: bar exclusion, early stop, user stop, drawer-not-user, 10-tick span, old inline handler
  each fail the suite; restored wbpark 75 checks, 0 failed.
- Wasabi PRs rig-checked on rig 6 (muimaster 19.35): #3 and #4 PASS. #2 failed because MUIA_ShortHelp was
  defined as 0x80420b17 (real 0x80428fe3); fixed in 4bb21c3 on spotUP pr/mui-buttons (GetAttr only, no
  offset). 18 Selecta buttons named.
- Remaining junk names on 5 Selecta buttons are a Selecta bug (dangling MUIA_ShortHelp pointer), ledger
  row SELECTA-SHORTHELP-DANGLING. Not a wasabi bug.
- LEDGER-ARCHIVE: 405 rows moved to thoughts/spot/todos-archive.md (383 old done, 22 open superseded).
  Review list: thoughts/spot/term-findings/ledger-archive-moved.md. 370 open rows remain.

Evidence: thoughts/spot/term-findings/wasabi-upstream-prs-rig.md (sections "#2 fix (s41)", "#2 junk names (s41)").

## Next, in order
1. Rebase #3 (pr/selecta-lists) onto 4bb21c3, push (no force unless owner approves; it is a stacked PR
   branch, so a rebase needs force: ask first). Then ask upstream to merge #2-#4.
2. After the merge: WASABI-PIN-UPSTREAM (re-pin tools/nas/wasabi.rev + wasabi.repo to creep-ltx main,
   make wasabi, one rig check).
3. WB-PARK-ZORDER (a) 3.2 rig check (`rig.py start --max --os 32`), (c) drawer reopened late.
4. Open-row triage of the 370 open ledger rows: needs owner direction per section.
5. NAS-DEPLOY-AMIGA leftovers, RIG-BATCH-DAILY, NVIM-PERF, NVIM-HEAP-CORRUPT, RIG-RECHECK-STALE.

## Gotchas
- Mutation checks: removing a parameter's use trips -Werror unused-parameter; mutate with `|| 1` / `&& 0`.
- Pinned build/wasabi f59c839 carries the broken #2 constant too; the re-pin fixes it.
