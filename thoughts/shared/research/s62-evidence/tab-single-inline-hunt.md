---
date: 2026-11-06
topic: TAB-SINGLE-INLINE root cause (single-match Tab in vsh inserts nothing)
ledger: thoughts/spot/todos.md TAB-SINGLE-INLINE
branch: fix/tab-single-inline
---

# Single-match Tab in vsh inside XCON: inserts nothing

## Symptom (rig 165, origin/main 319fd963, os32)
`Type tabprobe_z` + Tab in RAM:, the only match RAM:tabprobe_zzz: line stays
`Type tabprobe_z`, ghost `zz` shown; Return accepts the ghost.

## Root cause (from code)
- vsh arms V93 completion (dist/vshrc has `complete -F ...` specs, so ncomps > 0).
- Tab: comp_marker answers vsh's Read with the marker line. vsh's sh_complete
  finds no spec for `Type`, answers ACTION_VTCON_COMPLETE with VTCON_COMP_DEFAULT
  (DoPkt: vsh is blocked inside that packet, no Read of it is held).
- handler/vtcon_handler.c comp_words (was ~4904): `default_completion(c)` ran
  right there. comp_start's s61 cd gate (cc_cd_blocked over c->reads) found no
  held synchronous Read from the opener, so q->cd_from = 0.
- handler/complete.c worker then has no current directory: the relative
  lookup of `tabprobe_z` matches 0 files, finish_completion beeps, inserts nothing.
- The ghost works because its lookup starts while vsh's Read is held (cd gate passes).
- Regression from the s61 cd_from gate (correct gate, wrong timing in the DEFAULT path).

## Fix
- New pure step `cc_default_step` (handler/complete_core.c/.h): the DEFAULT answer
  sets a pending flag; vsh's next Read (sender == comp_armed, line unchanged,
  window open, not raw) starts default_completion after the Read is queued, so
  comp_start sees a held synchronous Read and DupLocks vsh's directory.
  A key typed meanwhile drops the pending answer.
- start_completion: Tab while pending does nothing; pending is dropped if the
  line changed or the shell task is gone.

## Test
tests/test_complete.c `single_match_tab_in_vsh_gets_the_shells_directory`
(fake handler chains cc_default_step + cc_cd_blocked). Revert check: make the
step start at the answer (old behaviour) and the test fails.

## Still open
Rig proof (rig 166 or the owner's eye-check rig): `Type tabprobe_z` + Tab in RAM:
gives `Type tabprobe_zzz `. Not run in this hunt (host reproduced the cause).

## Proof (host)
- With fix: `make test ONLY=complete` -> `[OK] complete: 621 checks, 0 failed`.
- Reverted (step returns 1 at the answer): `[FAIL] complete: 621 checks, 5 failed`,
  decisive line `tests/test_complete.c:206: lookup_had_dir = 0, want 1`.
- `make build/amiga/vtcon-handler` (vbcc, warnings as errors): rc 0.
