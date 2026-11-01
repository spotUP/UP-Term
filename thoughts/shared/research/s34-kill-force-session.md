# s34 kill_force_session run

Verdict: PASS (all 6 checks, exit 0) against main 91e0e355.

How run: `make build/amiga/vsh` (rebuilt 00:48), `UPTERM_RIG=5 rig.py start --max --purpose "vtcon-05 s34: kill_force_session run"`
(start refreshed the kit; md5 of build/amiga/vsh = UP-Term:bin/vsh = VTC:vsh on the rig = 9284814e...), then
`UPTERM_RIG=5 python3 tools/rig/sessions/kill_force_session.py` (it opens its own titled XCON window via typewin.open_xcon + UP-Term > New tab; no manual driving).

Output (verbatim, cut):
    ps vsh job lines: ['0x10447130 proc    5 wait     16000   15670    2  vsh job 2 (Wait)']
    ok   one "vsh job" runner is running
    kill: ''
    tab text (tail): '   T H E   A M I G A   S C E N E\nNew to a Unix shell? Type  tutorial  for a short hands-on course.\nHide this hint: echo > $HOME/.tutorial-hint-off\nSYS: % Wait 60\nSYS: % Echo prompt-is-back\nprompt-is-back\nSYS: % UPTerm screen FILE RAM:kf.txt'
    ok   the prompt came back after the kill and ran Echo
    ps new child lines: ['0x103f27b8 proc    5 wait     16000   15670   13  Background CLI (Wait)']  runner lines: ['0x10449160 proc    5 wait      8000    7470   11  vsh job 5 (UP-Term:bin/vsh)']
    ok   a script runs as one new "Background CLI" child under one "vsh job" runner
    kill child: ''
    tab text (tail): 't: echo > $HOME/.tutorial-hint-off\nSYS: % Wait 60\nSYS: % Echo prompt-is-back\nprompt-is-back\nSYS: % UPTerm screen FILE RAM:kf.txt\nSYS: % RAM:ks.script\nSYS: % Echo child-prompt-is-back\nchild-prompt-is-back\nSYS: % UPTerm screen FILE RAM:kf.txt'
    ok   the prompt came back after only the child was removed
    ps after: []
    ok   the runner "vsh job" and the child are gone (no leaked task)
    ps new child lines (redirected): ['0x10449ad0 proc    5 wait     16000   15670   15  Background CLI (Wait)']
    ok   the redirected script runs as one new "Background CLI" child
    kill child: ''
    ok   the redirected file is closed after the child was removed (Delete works; a locked file says "No file to delete")
    kill_force: PASS

No code change, no commits. Case 1 tab text showed 'SYS: % Echo prompt-is-back' / 'prompt-is-back'; case 2 'child-prompt-is-back'; ps after: [].
