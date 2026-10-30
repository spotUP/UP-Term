# s33 tutorial_rig leftovers (rig 4, 2026-10-31)

Branch fix/tutorial-leftovers, worktree /Users/spot/Code/UP-Term/vtcon-tutfix, commit bccf1cfb (not pushed, main untouched).

## 3. check-b
Re-run on rig 4 with the be8228c1 fix: `[PASS] check-b`. Before (rig 3 run): FAIL (hard-coded 13). Closed.

## 1. selftest-14 (rx: Unknown command)
Two causes, both real:
a) Path. The rig image HAS rx (build/rig4/os32/Rexxc/RX) and boot starts RexxMast (rig.py:398), but the rig shell path is C: only; `rx "say 2"` -> "rx: Unknown command" rc 10, `SYS:Rexxc/rx "say 2"` -> 2. A real Workbench has SYS:Rexxc in the path. amarchy_menu_rig.py r_start already does `Path SYS:Rexxc ADD`. The Amarchy commodity is also not running on a fresh rig 4.
b) Lesson bug (not rig): plain `rx "address AMARCHY STATUS"` prints nothing, even with Path and Amarchy running (rc 0, empty output); the check `grep -q rows=` can never pass on a real Amiga. Only the host test passed because its fake rx echoes. `rx "options results; address AMARCHY STATUS; say result"` prints hits=0 rows=69 enabled=1 ...
Fix (bccf1cfb): lesson 14 uses the options-results form in all 4 lines (question, case pattern, hint, answer); tutorial_rig case_selftest(14) prefixes `Path SYS:Rexxc ADD` in the same exec and starts Amarchy when the AMARCHY port is absent (amarchy_port()).
Before: `[FAIL] selftest-14 rc 0: ok 1 | FAIL 2` (3 runs; debug copy of dist/tutorial printed out=[] for the typed line). After: `[PASS] selftest-14`. tests/test_tutorial_lessons.py: 7 passed.
Gotcha for the next rig test: the uae dir fs does not see host-side edits of files made after boot; push a changed lesson with `ami.py put <file> UP-Term:share/tutorial/<name>`, or restart the rig. Also never run `Assign UP-Term:` (no target) - it removes the assign (I did, restored with `Assign UP-Term: VTC:UP-Term`).
The rig kit does not ship this lesson change until main has the commit and rig.py start refreshes it.

## 2. lesson12 paste ("TYPE can't open T:tutorial.out")
NOT FIXED, cause narrowed but not final. It is not flaky: 4 of 4 runs on rig 4 failed the same way (also before I started Amarchy).
Evidence (screenshots taken after RAmiga V / after Return, tutorial tab): the tab shows `tutorial> echo xte` and the answer `Not quite. Hint: Hold Right Amiga and press T; the new tab opens...` which is the STEP 1 hint. So the lesson never advanced past step 1: counters tabs-opened (1,1) did not grow (all others did: tab-switches 1->5, menu-picks 1->7, slash 0->1, copies 0->4), progress file stays 1..11 (no 12). T:tutorial.out is written only when step 6's check reads $LEARN_OUT, so it never existed. The pasted word was "xte" (end of a wrapped /help line, "xterm-256color"), because step 5's task line "Drag over this word" was never printed: steps 1..5 were not passed either. The case's `log` True values for steps come from the pixel test ret()/reprompted(), which reports "advanced" falsely.
Open: why Right Amiga T (key 0x14 + RCMD, tutorial_rig.py ~line 392) does not raise tabs-opened on rig 4 now (the screen has only 2 tabs: tabwin, UP-Term). Candidates: the new-tab key changed or is consumed (Amarchy / keymap), or the case's tab 2 is not the window the key goes to. Next probe: after open_tab("menu"), press RAmiga T and look at the tab bar and `UPTerm state tabs-opened`.
Also: case verdict should include dict(log)["tabs-opened"] from the counters, not only the paste, and the detail is cut to the last 300 chars by the runner (hid the cause).
