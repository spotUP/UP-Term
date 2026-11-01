# s33 tutorial rerun after rc161 (rig 3, main dc21b06f)

Date 2026-10-30. Rig 3 only, no source edits.

## New vsh on the rig
make wasabi amiga rc=0. md5 build/amiga/vsh = build/rig3/vtc/vsh = build/rig3/vtc/Apps/bin/vsh = cddbc6cab906c9957d8735b8e46a09b4 (rig start had no drift [WARN]; only dopus-not-installed and PAL warnings).

## rc161_rig
`rc161_rig: OK` (resident_true st=0, script_kill ended).

## tutorial_rig: 24 of 27 pass (was 4/27). All rc161 failures are gone.
- selftest-14 (14-amarchy-keys.lesson), "ok 1 / FAIL 2": step 2 needs `rx "address AMARCHY STATUS"`; on rig 3 `rx: Unknown command` rc 10 (no ARexx/rexxmast in this rig). Class (c) environment. (Not verified that rx exists on other rigs.)
- check-b: "14 lessons listed; missing []": test hard-codes `== 13` (tutorial_rig.py:507) but dist/tutorial-lessons has 14 lessons (14-amarchy-keys added 2026-10-26). Class (b) test bug. Docstring line 7 also says 13.
- lesson12: window-lesson steps all True except ('paste', False); echo said `TYPE can't open T:tutorial.out / object not found`. Counters slash-count 0 of 1, copies 0 of 4 are normal for the shown state. Not rc161. Class (c)/(b) undecided: paste step reads T:tutorial.out that was never created (clipboard paste into the tab did not happen); earlier commit 0932094d says this case was flaky on rig 4. Not investigated further (budget).

## VSH-KILL-TERM-NATIVE-HANG: still reproduces
Native vsh script in background (Run UP-Term:bin/vsh RAM:kt.sh, loop until RAM:kt.stop), pid = $$ = 271763488, then `UP-Term:bin/vsh -c "kill -s TERM 271763488"` via ami run: sender did not return in 90 s, ignored Ctrl-C ("vsh is still running after 90 s and ignored Ctrl-C - it keeps running in slot 1"). 1 run (the first attempt had no live script, returned in 0.3 s with rc 0, not valid). So the hang is NOT fixed by the rc161 vsh. The pfind-fix ixemul claim in the brief: whether the rig loaded the new ixemul with pfind was not checked by me (no md5 of ixemul.library done).
Logs: scratchpad tut.log, kt.log, rc161.log, start.log.
