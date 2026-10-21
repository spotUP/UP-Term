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
