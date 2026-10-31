# s34: lesson 12, why Right Amiga T opens no tab

## Cause
Right Amiga T is no longer bound. The keymap makes the Right Amiga menu command key of an item only when the
keymap binds `super+<letter>` and does NOT also bind `super+shift+<letter>` (handler/keymap_act.c kact_comm_key,
because Intuition ignores Shift in a command key). dist/keymap line 135 binds `super+shift+t = slash:/theme`
(and line 139 `super+shift+w` for Close tab), so "New tab" and "Close tab" get no command key.
tests/test_keymap.c:703 asserts exactly that: `comm_is("t", MENU_TAB_NEW, 0)`. The window key is `super+t`,
and keymap_key (handler/vtcon_handler.c ~5561) accepts only IEQUALIFIER_LCOMMAND (Left Amiga) and rejects RCOMMAND.
Evidence on rig 4: `menus tabwin` lists "New tab" and "Close tab" with an empty shortcut, "Next tab" ".".
RAmiga T (0x14, 0x80): no tab, tabs=1. LAmiga T (0x14, 0x40): active title "UP-Term", tabs-opened 0->1, tabs 1->2, km-hits 0->1.
dist/README.txt (line 355) and lesson 12 still said Right Amiga T: stale text, the lesson was unpassable for users too.

## Fix (branch fix/tutorial-lesson12)
- tutorial_rig.py case_lesson12 and tabs_rig.py send Left Amiga T.
- dist/tutorial-lessons/12-...lesson task, hint, and `!` line say Left Amiga T; dist/README.txt line fixed.
- case verdict detail now starts with the counter deltas (tabs-opened ...), the runner prints detail[:600]
  (was the last 300 chars, which hid the counters), clips (word, clipboard, rows) are listed.
- word_span(skip) now uses the retry index (the loop called word_span(0) every time).

## Verdicts, rig 4
- Before (main): FAIL, counters tabs-opened (1,1), tab-switches (1,5), menu-picks (1,7), copies (0,4); paste "TYPE can't open T:tutorial.out".
- After: still FAIL, but tabs-opened (1,2) and steps 1-4 pass (progress file reaches 11 and 12 is missing only because step 6 fails).
  Remaining failure is a different, older cause: the copy step selects the wrong word. Clipboard after the drag is
  "Return:" / "xte" / "close", never `up<pid>`; task_rows picks a row that is not the task line's tail (task wraps at
  77 columns; rows seen [(221,77),(205,36),(189,77)...]). Needs a look at the real screen (rig pixel, not host logic).
  A later run (after a killed run) read counters None: state output empty, rig tab left in a bad state; re-run on a clean rig.

## Open
- case lesson12 does not PASS yet: the drag/copy word finder (Screen.task_rows/word_span) must find the `up<pid>` word.
- host tests: pytest tests/test_tutorial*.py 33 passed.

## s34 step 5 (second agent; NOT resolved, rig 4 went down under host load 70-100)
- Screenshot of the tab right after settled() at step 5 (rig 4, run before the crash): the lesson DOES print the task as
  "Task: Drag the mouse over this word, press Right Amiga C, then press Return:" and wraps the word `up273825984` onto its own
  short line (11 columns) directly above TWO "tutorial>" prompt lines (the empty Return of step 4 re-prompted once).
  So the lesson prints something a user can drag over; the word is on the row two above the bottom prompt.
- Same run, clips: task_rows() = [(214,76),(198,44),(182,76),(166,77)] and later [(221,76),(205,76),(189,29),(173,77)]:
  every candidate is a 76/77-column row ("Task:... Return:" , "Amiga C puts the selection ...", "Copy takes two moves ...").
  The short `up<pid>` row never appears in task_rows, so word_span picks "Return:" (ends the 76-col Task row), "close" etc.
  Counters in that run: tabs-opened (1,2) tab-switches (1,6) menu-picks (1,7) slash-count (0,1) copies (0,4);
  progress 1-11, paste False. Counters were NOT None this time (the earlier None came from the rig tab left bad after a killed run).
- Suspect (unproven): Screen.task_rows measures `bottom` from the last ink row of the whole window; the up-row is skipped or
  mis-aligned because line_sig(bottom,k) assumes LINE=16 rows from that baseline and the bottom prompt's cursor block shifts
  the baseline, or the second prompt row is not pixel-equal to the bottom prompt (cursor in the bottom one only
  compares cols 1-9, so that is fine). A debug print of (sc.rows()[-1], per-k prompt-equal flag, last col) at step 5 would decide;
  the run that was to print it (DBG line) died: rig 4 emulator exited, wasabi connection refused.
- Rig lesson: a worktree needs `ln -s <main>/build build` before any rig tool runs (kitset reads build/amiga); not committed.
- Not done: no code fix committed, host tests not rerun, `case lesson12` still FAIL.
