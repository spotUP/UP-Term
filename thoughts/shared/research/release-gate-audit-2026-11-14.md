# Release gate audit, 2026-11-14

Gate (thoughts/spot/todos.md:4820, set 2026-11-09): FREEZE, bug 0, 0c, A5, W8a, W8b, DT-11. DT-12 moved out (ledger ~4869).

Branch state: `main` = 07b1e6a0, `origin/main` = 1293c2d3. Local main is 1 commit ahead (the s80 handoff), 0 behind.

Stale SHAs: most SHAs the ledger names for gate fixes (851be2cb, 25a22daf, f7d1250c, 06fe144c, e2932d76, b238db7c, 8b8468fb, 8f7a8a50) are NOT ancestors of main. A rebase rewrote them. Main carries equivalent commits with the same subjects (`git cherry` marks them equivalent). Proof SHAs below are the ones on main.

| item | verdict | proof commit | ledger lines | what remains |
|---|---|---|---|---|
| FREEZE (theme Revert wedges KS 3.2) | CLOSED | 7e9269e1 (ledger 851be2cb / 8f7a8a50) | 4821, 4839, 4854 | Rig pass named: KS 3.2 timeout Revert alive past 70 s, pick + Keep works (4839). No owner pass named. Probe lines 4833-4835 are superseded. |
| bug 0 (theme requester bottom border; DOpus Environment cut-off) | OPEN | none | 4820, 4842, 4846 | Not reproduced on rig 166 (sampled 29-37 s after pick only). Not covered: before ~29 s and DOpus Environment. No fix commit. Whether the FREEZE fix covers the requester wedge is unverified. |
| 0c (DOpus gadget frames under flat pens) | OPEN | 9b964b8d (ledger 06fe144c), edf78743 (ledger e2932d76) | 4826, 4850, 4876 | Rig 67 FAIL (4850). After the D4 body hook, rig 68: string field framed, but Use/Save/Cancel and checkbox 218 edges still 000000 (4876). No rig pass after the hook. |
| A5 (Installer window too short; owner 2026-11-09 10:06) | OPEN (fix on main, eye check owed) | e3e4151f (ledger f8a63b97; patina/inst_core.c, strut_patch.c inst_window) | 4852, 4864, 4867-4875 | "Not reproduced" verdict withdrawn. Owner chose the Installer margin patch (4875). S82 correction: fix IS on main as e3e4151f; rig 172 measured margin 4 / gap 4 (s66-evidence/a5-installer.md "## Margin patch"). No owner eye check. Working-tree state not checked. |
| W8a (legw on KS 3.2) | CLOSED | a70906a6 (ledger f7d1250c); ccffa724 (ledger 8b8468fb); 8cc0f2d0 (ledger b238db7c) | 4837, 4843, 4848, 4854 | legw PASS 22/22 on a fresh rig, `rig.py start --os 32 --max` (4848). Open note: legw takes over 10 min. Install.installer failure page not seen on screen (owner check optional). |
| W8b (empty SYS:UP-Term/bin after reboot + Uninstall) | OPEN | none (root cause only) | 4802, 4859, 4861-4862 | Root cause on rig 77 (4861): patina-flat bin match, Patina ignores Ctrl-C and ends only on MS_QUIT, PATINA port not answering. Fix not made: `rebooted=True` skip still present. Commit d6f53f90 is not a fix for this. |
| DT-11 (far-right "<<" arrow not themed, rig 63) | OPEN | none | 4807, 4828-4834 | Arrow path is hard-coded (`DOpus5:Images/ToolbarArrow.image`). Options recorded, none chosen. No commit on main for it. Rig 63 check not done. |

Gate status: CLOSED 2 of 7 (FREEZE, W8a). OPEN 5 (bug 0, 0c, A5, W8b, DT-11). Release gate not met.
