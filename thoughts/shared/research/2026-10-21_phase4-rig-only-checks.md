# Phase 4 rig-only checks (rig 2), partial: rig 2 hung, restart refused

## Q1 userland_rig --only upkg-ui: FAIL on rig 2 (7 of 17 pass)
- upterm-ports/pkgs/upkg-ui/check/tty HAS the new cases: msgbox, textbox, texthot (--buttons Close,Next), programbox, colors.
- The 7 non-tty cases pass (usage, nowidget, badmenu, badhotkeys, badbuttons, notextfile, noprogramargs).
- All 10 tty cases (input, yesno, menu, hotkey, hotenter, msgbox, textbox, texthot, programbox, colors) fail identically:
  `program status: expected 'exit N', rig 'exit 127'` (out/upkg-ui/<case>.txt = "exit 127"). The old ones (input, yesno, menu) fail too, so not new-widget related.
- Cause read: every tty command is `sh -c "upkg-ui ..."`; the case scripts set PATH=/VTC/userland/bin:$PATH, and userland/bin has no sh
  (sh is only VTC:bin/sh). Running ptyrun with /VTC/userland/bin/upkg-ui directly (no sh) works: out/upkg-ui/t.stream = 1406 bytes,
  marks "1 1188 / 2 1274 / 3 1360 / end 1406". Whether rig 4 got 17/17 via a different PATH/sh is unexplained (rig4 vtc/bin/sh is identical).
  A hand run with `/VTC/bin/sh -c` as ptyrun command also gave exit 127.
- Next step (not done): make `sh` resolvable for the tty cases (stage sh into userland/bin, or ask the rig-4 owner what differs).

## Q2-Q5: NOT RUN
After the second ptyrun of my hand script (upkg-ui by bare name via PATH) rig 2 stopped answering (ami.py ping times out, 25 s+ and again 100 s later; serial.log shows no crash text).
`rig.py stop` + `start --max` was refused by the permission classifier, so I did not restart. Rig 2 is left running but hung. No Monitor/background tasks running.
Unverified, still open: --buttons label on stderr under ixemul (the texthot/textbox cases would prove it via ans.txt once sh works), lists-dir format + list-new,
up-claude plain-window path, pipe timing under patch 0024.
Static pointer: the Mac-side `make host-upkg-ui` expectations exist in upterm-ports/build/expected.

## Update (second agent): root cause of the tty exit 127
- Measured on rig 2: from `VTC:vsh script` with `export PATH=/VTC/userland/bin:$PATH`, `$PATH` is just "/VTC/userland/bin:" (the rig's vsh starts with an empty PATH);
  `env sh -c "echo hi"` -> "env: 'sh': No such file or directory" rc 127; `env /gg/bin/sh -c "echo hi2"` -> hi2 rc 0. ptyrun does execvp(argv[3]) which searches only PATH.
- Invariant: a rig script that execs `sh` by name must have the directory holding sh on PATH. sh lives only in GG:bin (VTC:gg/bin/sh), never in userland/bin.
- Why rig 4 passed: not found host-side. Rig 2 and 4 have identical boot/startup-sequence, go, vshrc, assigns, gg/bin/sh (rig 2's was a stale vsh, 237660 vs 237648 bytes, but replacing it did not matter).
  Likely rig 4's persistent disk or run state gave vsh a PATH containing gg/bin (unverified, not run on rig 4).
- Fix: tools/rig/userland_rig.py tty_stage scripts use PATH=/VTC/userland/bin:/gg/bin:$PATH; host test UserlandTtyStage in tests/test_rig_fixtures.py (fails without the fix).
- Rig 2 then stopped answering wasabid (connection refused right after a successful ping; status "down" while fs-uae pid lives), so the rerun and Q2-Q5 were NOT run.

## Q1-Q5 on fresh rig 4 (2026-10-22)
- Rig 4 started with `rig.py start --max`. Q2-Q5 are not defined in the plan or this file; only the "unverified, still open" list above exists (--buttons on stderr, lists-dir/list-new, up-claude plain window, pipe timing under 0024). Q2-Q5 NOT RUN.
- Q1 `UPTERM_RIG=4 userland_rig.py --only upkg-ui`: FAIL, 7 of 17 pass, then the run aborted. Same 7 non-tty cases OK (usage, nowidget, badmenu, badhotkeys, badbuttons, notextfile, noprogramargs).
  At the first tty case (input) the wasabid connection was refused: ConnectionRefusedError in ami.py requesters() (ami.py:125 -> wasabi _open). `rig.py status` = "down" while fs-uae pid 52257 still lives; `ami.py ping` also refused afterwards.
  Same symptom as rig 2 (wasabid gone at the first tty/ptyrun case), so it is not rig-2-specific. serial.log and fs-uae.log.txt show no crash text; build/rig4/wasabi/debug.err is stale (20 Oct).
  Evidence: stdout log /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/87a17433-32bd-494e-bac5-37c13673dc52/scratchpad/q1.log, build/rig4/serial.log, build/rig4/logs/fs-uae.log.txt.
- Stopped per instructions on the failing check; no code changed. Open: why wasabid dies on the first ptyrun case (suspect ptyrun/ixemul pty path or a wasabid crash on that command); needs a wasabid log, not found under build/rig4.

## Rig 4 rerun 2026-10-22 (session 2)
Setup: UPTERM_RIG=4, make wasabi OK (be4c0559), rig.py start --max. Scratchpad: /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/8c0a20f9-7ad8-44fc-ac09-4cb6c018546f/scratchpad (a.log, b.log, browse.log, ask.log, serial4.log, fsuae4.log).
- (A1) userland_rig --only upkg-ui --force, plain: FAIL, 7 of 17, aborted at tty:input with ConnectionRefusedError (wasabid gone). rig.py status "down" with fs-uae alive; fs-uae log has 84 "CPU reset" lines (PC=f80dc8 / f800d0 Kickstart), no exception text printed. Saved: /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/8c0a20f9-7ad8-44fc-ac09-4cb6c018546f/scratchpad/a.log, /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/8c0a20f9-7ad8-44fc-ac09-4cb6c018546f/scratchpad/serial4.log, /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/8c0a20f9-7ad8-44fc-ac09-4cb6c018546f/scratchpad/fsuae4.log. Reproduces the reset-loop Guru; it is intermittent (earlier note: once in 200+ runs; rig 4 hit it in 2 of 2 sessions today before this).
- (A2) probe per Amiga-crash rule, same run under Stack 262144 (crashloop/stackrun.py B): PASS 17 of 17, no crash (/private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/8c0a20f9-7ad8-44fc-ac09-4cb6c018546f/scratchpad/b.log; build/rig4/userland/upkg-ui.verdict = PASS). One passing run does not clear the intermittent Guru; plain mode was not re-run after the crash, so A in plain mode stays FAIL/unreproduced-pass.
- (B1) --buttons label on stderr under ixemul: PASS. texthot (--buttons Close,Next --textbox, 2>ans.txt) compared byte for byte: expected ../upterm-ports/build/expected/upkg-ui/texthot.file = "Next", tty_compare reads the rig's ans.txt, verdict tty:texthot PASS and tty:textbox PASS (build/rig4/userland/upkg-ui.verdict).
- (B2) lists-dir format + list-new: PARTIAL. upkg_browse_rig.py PASS 12 of 12 (/private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/8c0a20f9-7ad8-44fc-ac09-4cb6c018546f/scratchpad/browse.log): browse reads its row lists (7 rows, up-vim row 6) and install/remove sentinels, green colour rises 0 -> 1938 and falls back. Direct `upkg list-new` on the rig: NOT RUN. A by-hand try via ami.py exec failed because the rig had no T:upkg-rig.conf and UP-Term:bin/upkg is not a runnable command ("Unknown command"); the scripts upkg_rig/upkg_browse_rig set up conf and vsh themselves. Host checks pkgs/opkg/check/list_new.sh, browse_lists.sh exist but are not in the rig case list.
- (B3) up-claude plain-window path: PARTIAL. upkg_ask_rig.py: "Verdict: A PASS, B PASS, C PASS" (/private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/8c0a20f9-7ad8-44fc-ac09-4cb6c018546f/scratchpad/ask.log): install up-claude with upkg-ui (inputbox) and without (plain prompt "[rigA.lan 2323]: rigB.lan 2424") both work. The browse-dialog route (control field sends an interactive package to the plain window, from `upkg browse`) was NOT RUN: no script drives it.
- (B4) pipe timing under patch 0024 with a slow package: NOT RUN. Only host check pkgs/opkg/check/pipe_lines.sh exists (file:// feed + a held http server); no rig script, no slow package. browse install of up-vim PASSed with programbox output, but that does not measure timing.
- Rig 4 stopped: yes (rig.py stop, status down). No code changed, nothing committed.

## Kit with ixemul 889299a + B2-B4 (2026-10-22)
- Kit ixemul: PASS. ixemul-vtcon HEAD 889299a built with docker/build.sh (the uncommitted change builds BOTH 68020/68881 and 68020/soft-float; the kit copies only the 68881 one, Makefile IXEMUL_LIB). cmp of build295/library/68020/68881/amigaos/ixemul.library (sha 77b91713..., 199972 bytes, built 08:02) equals build/dist/up-term/Files/libs/ixemul.library and build/dist/up-python/Files/libs/ixemul.library. build/dist/Python3/Files/libs/ixemul.library differs: stale leftover dir of the old archive name, not in packages.txt.
- Kit build: upterm-ports make kit-stage rc 0; vtcon make dist dist-ipk rc 0; make dist-check rc 0 (8 tests OK). Kit VERSIONS: UP-Term 1.7 d9dee815+dirty.
- B2 list-new + lists dir: PASS. New case listnew in tools/rig/upkg_rig.py, rig 4, 9 of 9: first update leaves no upterm.prev, list-new empty; after a feed gaining up-pi, upterm.prev kept and list-new prints exactly the row of up-pi; list-browse T:lb writes T:lb.new and T:lb.all equal to list-new / list output. Log: /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/8c0a20f9-7ad8-44fc-ac09-4cb6c018546f/scratchpad/listnew.log
- B3 up-claude in the plain window: PASS. New tools/rig/upkg_browse_extra_rig.py (setup/teardown now shared from upkg_browse_rig.py), 5 checks: question "Claude Code on your server" on the window text, no programbox title, typed "rigC.lan 2525" reaches ENVARC:Claude/remote, "Press Return to go back to the list." follows, up-claude in list-installed. Log: /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/8c0a20f9-7ad8-44fc-ac09-4cb6c018546f/scratchpad/extra2.log (first run had 1 FAIL from a wrong sentinel path in my script, fixed, rerun clean).
- B4 pipe timing under patch 0024: PASS. up-slow (temp feed drawer, postinst: echo SLOW-FIRST; sleep 40; echo SLOW-LAST) installed from the browser: SLOW-FIRST on the window 8.2 s after Install while SLOW-LAST absent, SLOW-LAST at 60.0 s (52 s later; first run 6.9 s -> 42.1 s). Lines arrive live.
- Rig 4 stopped: yes. Rig default Stack throughout, no Guru seen.
