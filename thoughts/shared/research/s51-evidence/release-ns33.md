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
