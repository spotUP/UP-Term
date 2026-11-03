# NS3.3 pane_close SIGWINCH leak loop (vtcon-ee, 2026-11-02)
Main at 461f1d4f (contains 639c4cdd). wasabi rebuilt to pin 0c43ce3, make amiga + inputlist OK.
Rig 65 (060 max), `splits_rig.py 65 --repeat 15 --fresh-boot --only pane_close`.
Log: build/rig-reports/rig65/splits/repeat-pane_close.log
Result: rounds 1-5 PASS (fresh boot each). Run then ended at round 5 of 15: build/rig65 was removed
(`rm -rf build/rig65.del` seen running, not by this session), killing rig and runner. No FAIL seen.
Not run: rounds 6-15, host_exit_cycles. Verdict: INCONCLUSIVE (5/15 PASS, 0 FAIL).

## Retry on rig 68 (vtcon-ee, 2026-11-02 16:00-16:20): BLOCKED, 0 new rounds
Rounds 6-15 and host_exit_cycles did NOT run. Cause: RIG-STALE-BASE-GUARD. HEAD d94a609c lacked 3 origin/main commits
(c2367aca, 9373fcf8, a4928c9d, AmiSSL install). `rig.py start --allow-stale-base "<reason>"` boots, but
`splits_rig.py --fresh-boot` reboot() (tools/rig/splits_rig.py:209-212) calls `rig stop --keep` then plain `rig start`
with output captured, so the guard refuses silently, wasabid is gone, round 1 reports "ConnectionRefusedError ... HANG".
This HANG is a harness/guard artefact, not a SIGWINCH result. Unblock: merge origin/main into the checkout, then rerun
`splits_rig.py <rig> --repeat 10 --fresh-boot --only pane_close` and `--only host_exit_cycles`.
Totals still: pane_close 5/15 PASS, 0 FAIL; host_exit_cycles not run.

## R4 rerun, 2026-11-02, HEAD d8c605f9 (rig 69, splits_rig --repeat 10 --fresh-boot)
- pane_close: 2/10 PASS, 0 FAIL, 0 HANG. Run stopped before round 3.
- host_exit_cycles: not run (0/10).
- Round 2 boot stopped by the guard, not by a test failure. First attempt (round 2 boot) died with wasabi
  ConnectionResetError (Errno 54) during rig start, host load ~119; retry resumed at round 2, passed it, then:
  `rig: RIG-STALE-BASE: HEAD d8c605f9 does not contain origin/main 25413f2a (2 origin/main commits missing); rebase or merge origin/main, or pass --allow-stale-base "<reason>"`
  origin/main moved during the run; the guard was not overridden. Rerun needs main merged with origin/main first.
- Note: `--repeat` resumes recorded rounds from build/rig-reports/rig69/splits/repeat-<case>.log (rig 69 cleared afterwards).

## R4 repeat, fresh-boot, rig 69 (2026-11-02), HEAD eb18e57a
Host load at start: busy (fs-uae rig70 95%, node 89%, other session driver 89%, fs-uae rig67 53%); not waited on.
- pane_close: 10/10 PASS, 0 FAIL, 0 HANG
- host_exit_cycles: 10/10 PASS, 0 FAIL, 0 HANG
Each case stopped once on a reboot with "[ERROR] splits_rig: rig.py start failed (exit 1)": wasabi TimeoutError (Amiga daemon silent 25 s / 150 s), not a stale-base guard refusal (pane_close after round 3, host_exit_cycles after round 6). One retry each resumed the repeat log and finished 10/10. No fail-/hang- reports written. Rig 69 stopped and cleared.
