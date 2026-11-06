# S7c-7 vsh Execute recheck (rig 98, OS 3.2, 060 --max, origin/main 82c61d8e, worktree ../vtcon-s60-case7, make amiga rc 0)
Shell = C:Execute in the 3.2 Shell via ami.py exec; vsh = VTC:vsh -c "Execute ..." (quotes passed with AmigaDOS *" escapes). Scripts RAM:q5 (Quit 5), c1/c2/c3, r7c, r6/k1 (.KEY w/A + echo "got <w>").
Commits present: cc796bc6 (c), 8ce8ad85 (a, b).

(c) rc after Execute q5 then Lab / If..EndIf / If false..Else..EndIf, Echo "rc=$RC": PASS x3. shell rc=0 'rc=0'; vsh rc=0 'rc=0' (each).
(c) chain r7c (a..e): PASS. shell and vsh both rc 0, a0 b0 c0 d0 e0.
(a) Execute RAM:r6 ? <NIL:: rc PASS (shell rc 0, no text; vsh rc 0, was 20 'Bad arguments'). Text DIFFERS: vsh prints 'w/A: got ' (empty arg, echo runs); shell prints nothing.
(b) Execute RAM:k1 "?": PASS. shell rc 0 'got ?'; vsh rc 0 'got ?'. (First attempt with \" quoting printed nothing = harness quoting artefact, redone with *".)
Reply bind: Execute RAM:r6 ? <RAM:reply (reply=foo): vsh rc 0 'w/A: got foo' (still binds). Shell side rc 10 'foo: Unknown command' (the shell runs the reply as a command, harness artefact as in case7-run.md).
Raw output: scratchpad c7/out.txt (small, left).
