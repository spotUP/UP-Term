# Host-only queue reconcile (s58, 2026-11-05)

Method: ledger rows read by ID, commit subjects (git log --all, dumped to a file because the host was at load 40-70), merge-base --is-ancestor against main, code read. No rig, no code change. 17 ledger lines ticked (DONE-ON-MAIN).

| Q | ID | Verdict | Evidence |
|---|----|---------|----------|
| Q01 | DESK-CHECK-INVERTED | DONE-ON-MAIN | 9265f711 (desk_plan in desk_core); desk_dos.c now calls desk_plan |
| Q02 | DESK-CURRENT-MARKER | OWNED-ELSEWHERE | amarchy/gthemes_core.c:343 marks "(current)" from the saved theme; menu.c:375. Amarchy (vtcon-c1) |
| Q03 | AMCONF-NO-PROFILE-HEADER | OWNED-ELSEWHERE (lean: by design) | amconf_core.h:58-59 documents keys live in "[profile default]"; upconf_str(c,"default",..) reads only that section. Owner/c1 to close as design |
| Q04 | RIG-IMPORT-REFRESH | DONE-ON-MAIN | 53281064 (owner.json, gate refreshes own rigs only); s56-riggate merged |
| Q05 | RIG-REFRESH-CTAGS | DONE-ON-MAIN | 3697b596; s56-rigrefresh merged (not in --no-merged) |
| Q06 | MAKE-DIST-NO-STAGED-FROM | NEEDS-OWNER | Makefile:1819 refuses on missing STAGED-FROM on purpose; row says R10 runs kit-stage first. Close as by-design? |
| Q07 | RIG-CLONE-NEW-NUMBER | DONE-ON-MAIN | 07f3e368 |
| Q08 | TMUX-NCURSES-PATH | DONE-ON-MAIN | tmux-amiga 99e6377 (verified ancestor of tmux-amiga HEAD), vtcon dc402a15 |
| Q09 | test_rig_kit_stamp shadow_pass | OPEN (cause known, changed) | stamp test now passes; test_rig_kit_shadow_keymap fails with FileNotFoundError (see brief) |
| Q10 | ARCHIVE-COMPLETION | DONE-ON-MAIN | 5692511e, dist/vshrc:160 `complete -F _archive_complete lha lzx unzip tar gunzip unxz` (gzip/xz compressors not listed; gunzip/unxz are) |
| Q11 | VSH-BANG-PID | DONE-ON-MAIN | 8f21e192 (rig PASS rig 9 in ledger); 71d3d893 is the local pre-push hash |
| Q12 | patch 0016 short_options | NOT CHECKED | upterm-ports lane, budget; hand to ports session |
| Q13 | TMUX-KIT-REFRESH | DONE-ON-MAIN | dc402a15 + 5c669ea9 (tools/tmuxfresh.py gate) |
| Q14 | JOB-ENV-UNSET | OWNED-ELSEWHERE | only amarchy/job_dos.c(.h) references JOB_LINES; "Not looked at"; vtcon-c1 |
| Q15 | AMIGA-IN-VSH | OPEN (part b+survey) | (a) endcli/endshell done: sh_exec.c:4794,6876. Remaining: survey of Shell built-ins + Unix aliases, owner picks the list; needs owner pick, not a bug |
| Q16 | VSH-BG-IGNORES-TERM | DONE-ON-MAIN | Cause was stale VTC:bin/sh (not vsh); rig.py:1688 04b827cc; $! fix is Q11 |
| Q17 | UPDEMO-NEW-FEATURES | DONE-ON-MAIN (eye check open) | scenes a-d, themes-more, tabs-live 5e3b2e98 all ancestors of main; 8be36706 help scene. Owner eye check pending |
| Q18 | SHELL-TUTORIAL | FIXED partly on main | dist/tutorial + 14 lessons tracked; plan items L-C1 (facts check) and L-R (rig check) still unticked. Rig-bound remainder |
| Q19 | S56-FLAT-BUTTON-FILL-TINT | OWNED-ELSEWHERE | amarchy/flat_core.c ~560, row says Amarchy session implements |
| Q20 | S57-LOCALE-LIST-NO-SELECT | OWNED-ELSEWHERE | row says sent to vtcon-c1 |
| Q21 | S57-LOCALE-GADGET-BLACK, S57-GADGET-LINE-MISSING | OWNED-ELSEWHERE | sent to vtcon-c1; branch s57-title unmerged (not mine) |
| Q22 | S32-SCROLLBAR-PROP | OWNED-ELSEWHERE (probably superseded) | later [x] S32-KNOB-NONE-FRESH, S32-STALE-HELD-PENS, fef96a81 knob box, 7e6f8754 prop gadget |
| Q22 | S32-SHELL-ACTIVE-GADGETS | OWNED-ELSEWHERE (probably superseded) | febc293d S32-CLONE-CLIP on main, b05443e4 rig 33 proof |
| Q22 | S32-GREEN-LINE-96 | DONE-ON-MAIN (rig proof open) | bfce8a3b |
| Q22 | S32-INACTIVE-COLOUR | OWNED-ELSEWHERE (probably superseded) | bd1c1ddc epoch fix, 27607459 inactive bar; no closing line found for the rig 35 reboot reproof |
| Q23 | BAR-BEHIND | DONE-ON-MAIN | aa989997 (second row at 3182 already [x]) |
| Q24 | INSTALLER-CHECK10 | DONE-ON-MAIN | a51c35bc, 4c0200d7, 6c405484, fdcec89d |
| Q26 | MC port research | DONE-ON-MAIN | thoughts/shared/research/s56-evidence/mc-port-research.md tracked, 779cb757 plan |
| Q27 | Port next terminal apps | NEEDS-OWNER | follows MC go/no-go; ledger line not located (grep miss) |
| Q25 | IXEMUL-* | NOT CHECKED | other repo, out of budget |

Counts (27 rows incl. Q22 x4): DONE-ON-MAIN 13 rows (Q01,04,05,07,08,10,11,13,16,17,23,24,26; Q22 GREEN-LINE also ticked), OPEN 3 (Q09, Q15, Q18 remainder), OWNED-ELSEWHERE 9 sub-rows, NEEDS-OWNER 2 (Q06, Q27), NOT CHECKED 2 (Q12, Q25). Ticked: 17 ledger lines.

## OPEN fix briefs

### Q09 test_rig_kit_shadow_keymap (S)
Files: tests/test_rig_kit_shadow_keymap.py (lines 93, 125), tools/rig/rig.py:1786 (stage_kit_media: `shutil.copyfile(ROOT/"dist"/src, dst)`). Cause: the test builds a temp ROOT and runs rig.py install; stage_kit_media now copies dist/updemo/earth.anim (+ fjord.png, photo.png, kanim-stream.bin) which the fixture root does not contain, so FileNotFoundError. The older row text (amnotify missing from the expected Amarchy list) is already fixed in the test (line 102 lists amnotify); test_rig_kit_stamp passes. Invariant: the test fixture root must hold every file the install stage copies, or stage_kit_media must be driven by the same KIT_MEDIA table (tools/rig/a1200_drift.py:38) the fixture reads. Test: the failing test itself, after adding the media files (copied from real dist/updemo or tiny stubs) to the fixture; verify it fails again with a media path removed. Do not edit rig.py to skip missing media (that would hide a real kit gap).

### Q15 AMIGA-IN-VSH remainder (M, owner input)
Files: shell/sh_exec.c (amiga_internals at 6876, builtin table 4794), dist/vshrc, Install S:Shell-Startup block. Invariant: every AmigaDOS Shell built-in not a file in C: either works in vsh or is aliased; Unix aliases in the AmigaDOS shell never shadow a C: name. Steps: survey table (Alias, Ask, Failat, Fault, Get/Getenv/Set/Setenv, Lab/Skip, Prompt, Quit, Resident, Stack, Why...) with measured results on a rig, then owner picks. Test: one vsh script-driven test per added builtin via the shell test suite, plus a table-completeness test over the survey list.

### Q18 SHELL-TUTORIAL remainder (S host, M rig)
Plan thoughts/shared/plans/2026-10-19-shell-tutorial.md: L-C1 (each lesson's facts checked on vsh, host) and L-R (rig check). Host part: run the existing tutorial lesson verifier over dist/tutorial-lessons/01..14 and tick L-C1; L-R needs a rig (not host-only), so move to the rig batch.
