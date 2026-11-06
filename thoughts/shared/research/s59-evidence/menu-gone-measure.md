# Menu gone after Keep: rig 150 measurement (verdict: A1 REFUTED)

Build: s58-integ fac5b709, rig 150, OS 3.2, 060 (--max --os 32 --idle-minutes 0), `installer_rig.py ONLY fresh-all` INSTALLER_KEEP=1 [PASS].
amarchy-gui md5 1bae23e9742bf595ccc329ce6734cd39 and Amarchy 2bfd910df15e6a4cedd9f4190847f414: rig == build/amiga.
Picks via scratchpad menugone/pick.py (wrapper of amarchy_theme_save_rig.py --selects N-N --no-build, KEEP_SECS=40).

## Pick 1 miasma (select 7), pick 2 ristretto (select 3)
Both: requester seen at ~11 s, Keep returned True, then [FAIL] "no menu window after the pick". Menu gone in both picks (reproduced 2 of 2).
Windows at Keep+5, +30, +60 s (identical both picks): S "Amiga Workbench Release 3.2 ..." with W "Amarchy bar", W 1 (640x235 active, untitled), W "f1host". No "Amarchy" menu window.
Status: `Process 11: Loaded as command: Empty-12MB-FFS:UP-Term/bin/amarchy-gui` (still listed with amarchy-menu 1, Amarchy 5, amarchy-flat 7).

## Break 11 C, 3 s, Status
Both picks: process 11 STAYS (same Status line before and after). Not the A1 discriminator: process 11 is the resident server (gui.c ~2040 loop,
Wait(port | CTRL_C) then SetSignal clears CTRL_C: "a stray toggle must not end the server"), so Ctrl-C never ends it.
Also A1's premise (event_loop with g->win == 0) would dereference g->win->UserPort at gui.c:1408 and crash, not hang.

## Reopen
`UP-Term:bin/amarchy-gui menu settings.theme` (foreground exec ran past 60 s, i.e. blocked on an open menu; then via `Run >NIL:`):
uitree shows `W 0 140 25 360x206 active "Amarchy"` (screen title "Workbench Screen"), new Process 13 = amarchy-gui client; server 11 unchanged.
Reopen works after both picks. Break 13 C closes it again.

## Verdict
A1 REFUTED as stated: the server is alive and serves a fresh menu request. The menu is simply not reopened after Keep (the reopen step after the
Workbench reset does not run or does not show), not a server wedged in event_loop. Next look: who is supposed to reopen after Keep (client process 13 path) and why it is skipped.
