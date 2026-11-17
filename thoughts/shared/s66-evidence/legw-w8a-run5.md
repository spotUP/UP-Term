# W8a run 5 (s66): legw after keeping the real Startup-Sequence in place
Verdict: legw passed 22 of 22 (the case has 22 checks). Fix commit f7d1250c. Steps 12 and 14 pass once the Flat block exists (they followed from the missing block).

ok 1 base Install runs
ok 2 the base wrote UP-Term:UP-Term.version (the add-ons' base check reads it)
ok 3 legw (a): one ";BEGIN UP-Term Amarchy-Flat" block in S:Startup-Sequence
ok 4 legw (a): the block ends before IPrefs
ok 5 legw (a): the block sits after AddDataTypes
ok 6 legw (a): the block names the absolute drawer path, no stray character (ENVARC:up-term/Dir: 'SYS:UP-Term')
ok 7 legw (b): SYS:WBStartup/Amarchy.info exists
ok 8 legw (b): SYS:WBStartup/Amarchy.info carries DONOTWAIT STARTPRI=10 and the tool UP-Term:bin/Amarchy
ok 9 legw (b): SYS:WBStartup/amarchy-index.info exists
ok 10 legw (b): SYS:WBStartup/amarchy-index.info carries DONOTWAIT STARTPRI=-10 TOOLPRI=-5 and the tool UP-Term:bin/amarchy-index
ok 11 legw (c): after the reboot Status lists amarchy-flat (S:Startup-Sequence block ran)
ok 12 legw (c): Amarchy (WBStartup icon) runs: port AMARCHY answers STATUS, so Workbench did not hang on it (D6)
ok 13 legw (c): amarchy-index (WBStartup icon) ran: ENVARC:Amarchy/apps written (D6)
ok 14 legw (d): amarchy-flat is process 3, below [6] started in S:User-Startup (after IPrefs); the textual order is (a)
ok 15 legw (d): the resident patch answers STATUS (stack held on this Kickstart)
ok 16 base Uninstall runs
ok 17 after base Uninstall: S:User-Startup as it was, byte for byte
ok 18 after base Uninstall: S:Startup-Sequence as it was
ok 19 after base Uninstall: no UP-Term: assign of the Install
ok 20 legw (e): Uninstall removed SYS:WBStartup/Amarchy.info
ok 21 legw (e): Uninstall removed SYS:WBStartup/amarchy-index.info
ok 22 legw (e): S:Startup-Sequence is byte for byte the original
install_rig legw: passed 22 of 22

Note: a first run under 'timeout 560' was killed at step 11 (reboot); legw needs more than 9 minutes.
