# Side worktrees: unique work check (2026-11-01)

Method: all 7 worktrees have empty `git status --porcelain`. `git cherry -v` against origin/main and s28-crash-hunt; for each `+` commit the added/removed code lines (amarchy/, tests/) were diffed against the same-subject commit on origin/main (difflines=0 means identical change).

| worktree | branch | `+` on both bases | code equal on main | verdict |
|---|---|---|---|---|
| aa30c38af15f68cf9 | s28-flat-integration | none (all on crash-hunt) | yes | safe to remove |
| a550003839e8b4215 | s28-icon-noborder-fix | f2833970 AM-ICON-NOBORDER (desk_core/desk_dos .c/.h, test_desk.c) | identical to main 8554d788 | safe to remove |
| aafad7eaed72903c0 | s28-rig8-recheck | none (all on crash-hunt) | yes | safe to remove |
| a19eb3932992a25c3 | worktree-agent-a19eb... | none | yes (1e9b0571, c237a656) | safe to remove |
| afae81eda156283c6 | worktree-agent-afae81... | none | yes (67328573) | safe to remove |
| a6d14f78c9752b241 | s28-flat-button-disabled | bf06d8d Flat: disabled primary button (flat.c, flat_core.c/.h, test_flat_core.c) | identical to main 31386d1f | safe to remove |
| a7572bf374e60709e | s28-verify-rig8 | f8d5a32 (code identical to main 8554d788); dd5f4a9 s28 rig 8 evidence (2 .md files) | f8d5a32 yes; dd5f4a9 is notes only | unique: dd5f4a9 |

Unique commit: dd5f4a9 "s28 rig 8 evidence: round buttons pass with emulated flat pens; icon border not verifiable (no Workbench.prefs on 3.2 tree)". Files: thoughts/shared/research/s28-evidence/button-round.md (+8), icon-noborder.md (+5). Notes only, no code. These rig-8 result sections are not on origin/main or s28-crash-hunt. Note: none of the s28-evidence notes exist on origin/main at all; they exist on s28-crash-hunt.
