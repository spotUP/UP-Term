# PICK-FREEZE rig check (commit b6cfa365, branch pick-freeze)

Rig 68, KS 3.2 (--os 32, --max), DOpus 5.104 on the Workbench screen (DOPUS.1 present), Amarchy + amarchy-gui built from the
worktree vtcon-pick-freeze (menu_icons.inc copied from main checkout build/amiga; fontTools missing, env issue). amarchy-flat from the same
worktree, started from RAM: after deleting ENVARC:Amarchy/flat-starting; `amarchy-flat STATUS` read flat=on before every pick.
Driver: scratchpad pf.py (d.pick(keep=False), g.close_menu disabled so no key reaches the requester; theme vantablack).

Freeze test: `Version` through wasabid answers (<=0.4 s), amarchy-flat STATUS calls rising, DOPUS.1 port present, window list readable, 40 s after the pick.
Timeout revert proven by sentinel AMARCHY_CONFIRM_REVERTS_TIMEOUT +1 per run (the Keep requester is not seen by ami.requesters()).

| Run | Pick | 15 s timeout Revert fired | Froze | How determined |
|-----|------|---------------------------|-------|----------------|
| 1 | vantablack | NO: a USER revert fired (REVERTS_USER 0->1; a stray key hit the requester, close_menu active) | no | Version 0.3 s, DOPUS.1 up. Not counted as a timeout run |
| 2 | vantablack | yes (TIMEOUT 0->1) | no | Version 0.4 s, flat calls 90 rising, DOPUS.1 up |
| 3 | vantablack | yes (1->2) | no | Version 0.1 s |
| 4 | vantablack | yes (2->3) | no | Version 0.3 s, DOPUS.1 up |
| 5 | vantablack | yes (3->4) | no | Version 0.3 s, DOPUS.1 up |

Verdict: PASS. 4 of 4 timeout-Revert runs with flat on and DOpus on the Workbench screen did not freeze (before the fix: 2 of 3 froze).
Caveats: the Keep requester window was never seen by the poller (sentinels used); one revert-by-user run extra; probability of a miss at the old 2/3 rate over 4 runs is about 1% (if the rate were 2/3 each).
