# TMUX-PANE-PATH (s53)
Root cause: tmux/spawn.c:331 sets PATH=/usr/bin:/bin (_PATH_DEFPATH) in a pane when the server has none; ixemul's default is /usr/bin:/bin:/c. dist/vshrc set the kit PATH only `[ -z "$PATH" ]`, so the pane vsh kept a PATH without /UP-Term/bin.
Fix: dist/vshrc case statement treats those two defaults (and empty) as unset; any other PATH is kept. Commit 959de1fc on s53-tmux-path.
Test: tests/test_vshrc_path_inherited_default.py (host vsh sources real dist/vshrc); failed before (/usr/bin:/bin stayed), passes after.
Rig 41 (tmux started with PATH unset, pane runs sleep 1):
 old vshrc: PATH=/usr/bin:/bin:/c / vsh: sleep: not found ... / sleeprc=127
 new vshrc: PATH=/UP-Term/bin:... / sleeprc=0 / UP-Term:bin/sleep
Note: a tmux started from a vsh with the kit PATH exported inherits it and never shows the bug.
