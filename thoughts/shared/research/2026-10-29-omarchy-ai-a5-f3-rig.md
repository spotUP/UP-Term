# OMARCHY-AI A5/F3 rig check (session 21 agent, rig 7, build from origin/main 18e12299)

Setup: detached worktree build of origin/main; rig 7 `--max`; C:Claude (Oct 11, stale) and C:amiga-pi (missing) replaced with the
fresh builds; amiga-pi pointed at tools/claude_fixture.py on port 8117 (ENVARC:amiga-pi/models.json + settings.json defaultProvider fixo).
No API key on the rig, so C:Claude cannot answer a task (see below).

1. `Agent "list the files"` in the UP-Term tab: PASS (command line). A tab "2 Claude" opened; T:UP-Term-claude holds
   `C:Claude "list the files"`; Status listed C:Claude. UNVERIFIED: that Claude then runs the task. Rig has no key, so Claude's
   first-run wizard (/setup, "Later") takes the screen and the prompt is never shown. main_amiga.c:403 sends `cli.prompt` once the
   session is up; the end-to-end Claude case needs a configured key or `URL=` fixture (standalone_rig launches C:Claude with URL=).
2. `/ask list the files`: PASS (same). Script deleted first, rewritten with `C:Claude "list the files"`, new tab "Claude".
3. `/agent pi` then `/ask list the files`: PASS. ENVARC:Amarchy/prefs got `ai-agent = pi`; T:UP-Term-pi = `C:amiga-pi "list the files"`;
   fixture request dump 001.json: the user message is exactly `list the files` (one text, not "list"), screen shows pi answering.
   Needs the agent to have a provider configured; with default anthropic pi prints "No API key" and does not send the prompt.
4. super+l (LAMIGA + l, raw 0x28): PASS. With pref = pi, a "pi - SYS" tab and T:UP-Term-pi; after `/agent claude` (ai-agent line
   gone from prefs, default) a "Claude" tab and C:Claude. Pref restored to claude.
5. F1 colour follow: NOT VERIFIED (budget). F1 code is on main (claude/theme.c:45-46 palette-only names, orange = bright red). On the rig Claude
   was stuck at the key prompt (no key), so /color orange could not be applied and no theme switch was tested. To finish: put a
   dummy `ENVARC:Claude/key` (or launch with URL= fixture), `/color orange`, `/theme <other>` in tab 1, screenshot before/after.

Found
- Rig kit start leaves C:Claude (11 Oct) stale and no C:amiga-pi: rig.py install did not refresh them, I put them by hand.
- Typing into a wizard/login prompt eats the typed line: `/exit` is needed first; `3` at the setup step 1 landed on a later prompt.
- Screenshots: ~/Desktop/upterm-shots/s21-a4-{1,2,3,pi}.png
