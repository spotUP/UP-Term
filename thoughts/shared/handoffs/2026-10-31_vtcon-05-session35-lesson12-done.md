---
date: 2026-10-31
session: vtcon-05 s35
status: complete
---

# vtcon-05 session 35: kill Ctrl-C fallback, installer reruns, lesson 12 passes

## Done (all on origin/main)
- 1ff03915: trapsig_rig checks E and F prove vsh kill's Ctrl-C fallback for a non-vsh target.
- ca0d371c: ledger rows s30 RERUN-AFTER-IPREFS, s25 ADDON-OPTIONAL-UPTERM-WRITES, s30 ADDON-UPTREAL-FOLLOWUP ticked (rig 4 --fresh: addon-pi 26/26, anx 41/41; dist-base built from upterm-ports worktree .claude/worktrees/kit-main at ff7916d).
- 1afb50dc, fec811d9, 9af97d6a, 423bfd46: tutorial lesson 12 passes on rig 4 twice (progress 1..12).
  Root cause: 51f68616 moved Clear screen to Right Amiga Shift K; the lesson still said Right Amiga K and the
  Shift K branch (handler/vtcon_handler.c ~5643) counted no menu pick. Owner chose: lesson says Shift K, the
  branch calls stat_note(STATE_MENU, MENU_CLEAR_SCREEN). Regression test: tests/test_tutorial_wiring.py
  Lesson12ClearShortcut. Research: thoughts/shared/research/s34-lesson12-ramiga-t.md (s35d, s35e).

## Gotchas
- tutorial_rig.py does not install lessons. The rig's UP-Term:share/tutorial copy goes stale; put the current
  lesson with ami.py put (or kit-install) before a run.
- typewin.quiet_screen_text returns tab 1 text while another tab is active.

## Open (ledger)
- upterm-ports mk/common.mk:123: `seq 1 0` on macOS prints "1 0", so every package without LHA URLs fails
  ("first argument to `word' function must be greater than 0"). Guard with $(if $($(1)_LHA_URLS),...) plus a
  test, in a ports session (the main checkout is on another session's port/htop branch).

## Next
Pick the next open vtcon-05 row in thoughts/spot/todos.md (grep "vtcon-05" and "- [ ]").
