# s75: DOpus new lister gets the flat toolbar (commit a8ba6031, branch s75-dopus-toolbar-default)

Rig 7, OS 3.2, purpose "s75: DOpus lister flat toolbar on new listers". Build/Amarchy installed as VTC:Amarchy, started by tools/rig/amarchy_dopus_toolbar_rig.py.

Host tests: vttest_host keymap 21642 checks 0 failed; tick 14 checks 0 failed; tests/test_dopus_toolbar.py OK.

Rig lines (ARexx text queries, no screenshots):
- before: handles 272295532 279407220 277944980
- `lister new` twice, 7 s wait each; new handles 278254988, 278330412
- `lister query 278254988 toolbar` -> DOpus5:Buttons/Amarchy-toolbar-filled-32t  PASS
- `lister query 278330412 toolbar` -> DOpus5:Buttons/Amarchy-toolbar-filled-32t  PASS

Not proven: the scripted run stopped at the theme-picker GUI step ("enter Style: no stable state within 30s", amarchy_desk_rig pick()), so the "switch theme, then new lister gets the NEW set" case was not run on the rig. The new-lister case ran with the set already current. The earlier listers were also already flat when the script started (the tick had followed them), so this run does not show an upstream-default lister before the follow.
Unit test covers the new-only logic.

Cleanup: rig 7 stopped; build/rig7 removed by rig stop; deleted build/rig-saved/7 (11M); removed the build/dopus-toolbar symlink in the worktree.
