# Release open-row triage (ledger thoughts/spot/todos.md lines 3737-4010 + 1088), 2026-11-03

Method: ID grep through the ledger for later [x]/DONE lines, git log --all --grep=ID, merge-base --is-ancestor against main (tip b873cf84). Nothing edited, no rig.
Owner: ee = vtcon-ee (UP-Term/vtcon side), 16 = vtcon-16 (Amarchy).

| line | ID | verdict | evidence | owner |
|---|---|---|---|---|
| 1088 | RELEASE-1.0 | OPEN | umbrella; R10/R11 below still open, no release tag besides nas-v1 | ee |
| 3738 | KIT-AMARCHY-ALL | OPEN | gate: depends on RL8/RL17/RL19-21 and a final AMARCHY READY/VTCON READY cut | 16 |
| 3741 | KITTY-TRUECOLOR | DONE | K37 a14fe089, K38 ceacf685, K39 eaa21b90, K41 merged 1ed14b5f/cbd9e471, ledger 3900 "K41 DONE" | ee |
| 3743 | IXEMUL-EXECVE-SHARED-FH | OPEN | row says not a blocker unless screen ships; no commit | ee |
| 3744 | TMUX-NCURSES-PATH | OPEN | Makefile path in tmux-amiga repo, no commit/ledger close | ee |
| 3745 | TMUX-KIT-REFRESH | DONE | dc402a15 / 5c669ea9 dist-base refuses tmux older than tmux-amiga sources (recheck at R10) | ee |
| 3752 | R1 Kitty T-E | DONE | AGA half PASS rig 80, ledger 3761 [x] RIG-AGA-HANG, 2d52e64e | ee |
| 3757 | R3b subst fix | OPEN | s51-subst a9976a6d/c5352892 NOT in main (merge-base fails); rig vshpipe_rig froze twice; S52 queue says merge after vtcon-16 suites | ee |
| 3760 | TMUX-PANE-PATH | UNCLEAR | aec39407 fixes only the rig phtmux start path (via vsh); no commit/ledger line says kit pane vsh gets the kit PATH | ee |
| 3763 | R5 Kitty T-F K31-K33 | DONE | fe4af6c9 "Tick K33 ... phtmux PASS rig 86"; leftover: scroll-with-text rig item (minor) | ee |
| 3764 | R6 K34-K35 / R7 K36 | UNCLEAR | K35 merged a6de061d; K36 host part merged c63dcb4f, rig proof open (S53 agent); K34 not found | ee |
| 3766 | IDE-SPEED-POST10 | OPEN | explicitly post-1.0 | ee |
| 3768 | R10 dist bump/dist-check/rig/mkreadme | OPEN | release step; its blockers DIST-CHECK-RED-ON-MAIN (3880 [x]) and README-KEYS fixed | ee |
| 3769 | R11 owner OK + gh release | OPEN | last step, needs owner OK | ee |
| 3776 | RL3 FLAT-WB-BACKDROP | DONE | superseded by S32-WB-BACKDROP (+FOLLOWUPS) ledger 3846/3847 [x], b5100f3f, 2fa638a7 | 16 |
| 3784 | RL8 FLAT-COLOUR-OVERHAUL | OPEN | steps 1+2 [x] (3786/3787, c91ea918); pointer (step 4) and remaining elements open, see 4001 | 16 |
| 3789 | ICONS-APPLY-DOUBLE | OPEN | lock fix 42d86857 in main; rig proof + quadratic manifest rewrite open | 16 |
| 3790 | RL10 AM-ICONS-FILLED-SET | OPEN | OS 3.1 check open, no later close | 16 |
| 3792 | RL11 AM-ICONS-DISK-ALL | OPEN | 397a811a partial measurement; fe2405a7 network volumes icon; no close | 16 |
| 3802 | RL12 ICONLIB-PETERK | OPEN | code on main (d0651c1a, d340e8cb); only rig proof on RL18 left | 16 |
| 3805 | RL15 IPREFS-SYSREQ | DONE | b20f7072 in main, pushed in fe7bdb7c (ledger 3816) | 16 |
| 3809 | RL16 AMARCHY-WELCOME | UNCLEAR | phases 1-3 in main (fe7bdb7c); row still [~], no line saying remaining phases done | 16 |
| 3812 | RL17 INSTALLER-RECHECK | OPEN | recheck agent ended without verdict; later install checks (INSTALL-CHECK30/39 fixes) may cover it, no explicit close | 16 |
| 3814 | SHOT-ASPECT | OPEN | tooling nicety, no commit | 16 |
| 3815 | RL18 OWNER-DEMO-RIG + release build | OPEN | after RL1-RL17 | 16 |
| 3818 | RL19 SELECTA-AA-ROUND | OPEN | "Not started" | 16 |
| 3819 | RL20 SELECTA-AA-RICHUI | OPEN | "Not started" | 16 |
| 3820 | RL21 SELECTA-AA-ICONTEXT | OPEN | only probe 4f50c407, patch not written | 16 |
| 3824 | S31-START-PALETTE | UNCLEAR | e7e321e2 + 1b0eb507 (theme pointer/font/icontrol at start) in log; later 3844 reinstall note shows flat calls=0; no explicit close | 16 |
| 3825 | S31-RIG96-REDO | DONE | superseded: later reinstalls on rig 96 recorded (ledger 3844, rig96-reinstall.md) | 16 |
| 3826 | S31-RIG96-FREEZE | OPEN | same freeze signature on rig 91 at ledger 3968 (s53); hunt commits 2f3dd38c, 89a707ed only record findings | ee |
| 3838 | S32-EYECHECK-96 | OPEN | (a),(c step2) done; (b) flat pointer = RL8 step 4 open; green line see 4001 | 16 |
| 3840 | S32-STALE-GADGETS | OPEN | needs reinstall proof clone draws>0; related 4000 | 16 |
| 3851 | S31-FLAT-BUDGET | DONE | 7e4ad79f in main; ledger 3880 [x] DIST-CHECK-RED-ON-MAIN | 16 |
| 3852 | AMI-PUT-TRUNC | OPEN | no commit | ee |
| 3853 | RIG-IMPORT-REFRESH | OPEN | no commit (memory note says still live hazard) | ee |
| 3870 | INSTALL-CHECK30-AMISSL-PREBOOT | DONE | ledger 3904 [x]; d88e7821 | ee |
| 3876 | K41-AGA-REF | DONE | ledger 3888/3890 [x], cbd9e471 | ee |
| 3891 | K40d | DONE | ledger 3897 [x], 192eceb3 | ee |
| 3894 | DIST-CHECK README-KEYS | DONE | fad8f020 README key list; ledger 3898 [x] DIST-CHECK-FULL | 16 |
| 3895 | INSTALL-CHECK30 | DONE | d88e7821 | ee |
| 3901 | RIG-PUSH-STALE-BUILD | DONE | eaf70881, ledger 3910 | ee |
| 3905 | INSTALL-CHECK39-SHADOW-LEFTOVER | DONE | 7f3763d0, ledger 3915; 44 of 44 on rig 91 | ee |
| 3906 | MAKE-DIST-NO-STAGED-FROM | UNCLEAR | row says R10 runs kit-stage first; no commit or close; verify at R10 | ee |
| 3908 | S52 queue | OPEN | check39, rigpush, tmuxkit, wasabi, k36-host done; left: K36 rig proof, subst merge, rows a-f on rig 96 | ee |
| 3913 | S32-PLAN | UNCLEAR | reinstall-from-main step evidently happened (3844); no close line | 16 |
| 3931 | UPDEMO-GLOBE-PARTIAL-FRAME | UNCLEAR | likely same cause as K36-ANIM-BLINK fix c7f7b3d4 (in main); not re-checked by owner | ee |
| 3948 | S53 queue | OPEN | K36 rig proof + STAGE-DEDUPE agents pending | ee |
| 3961 | animation clock like real kitty | UNCLEAR | K36 host part c63dcb4f merged; no line closing the kitty-style scheduling | ee |
| 3973 | UPDEMO-NEW-FEATURES | UNCLEAR | step 2 DONE (3975); commits 3a1a571f, 9e2be165, d7270e60 (animation+photo media), b873cf84; rest of scene list not verified | ee |
| 3980 | K36-ANIM-BLINK | DONE | fix c7f7b3d4 in main (rows drawn with their text); owner eye-check on rig still wanted | ee |
| 4000 | S32-CLONE-CLIP | OPEN | no commit; opus hunt not run | 16 |
| 4001 | S32-GREEN-LINE-96 | OPEN | rig 96 from 3880f642 still green; pen-profile hunt pending | 16 |

## OPEN grouped

Real release blockers (release gates R10/R11, RL18 "after RL1-RL17", owner-stated "in THIS release"):
- 3769 R11, 3768 R10 (release steps), 1088 umbrella, 3738 KIT-AMARCHY-ALL, 3815 RL18
- 3784 RL8 (steps 3-4, pointer), 4001 S32-GREEN-LINE-96, 4000 S32-CLONE-CLIP, 3840 S32-STALE-GADGETS, 3838 S32-EYECHECK-96
- 3818/3819/3820 RL19-RL21 (owner: in this release), 3812 RL17
- 3757 R3b subst (unmerged correctness fix, planned before release)
- 3826 S31-RIG96-FREEZE (rigs freeze, blocks the eye-check/dist proof)
- 3948/3908 queue remainder: K36 rig proof

Release-gated by RL18 but proof-only (code on main): 3790 RL10, 3792 RL11, 3802 RL12, 3789 ICONS-APPLY-DOUBLE rig proof.

Post-release / not blocking: 3743, 3744, 3766, 3814, 3852, 3853, 3789 quadratic manifest rewrite.

## UNCLEAR resolved

Checked 2026-11-03 against code, git log and ledger (read-only).

| Ledger | Row | Verdict | Evidence |
|---|---|---|---|
| 3760 | TMUX-PANE-PATH | OPEN-BLOCKER | dist/tmux.conf only has `set -g default-shell /C/vsh` (line 4), no PATH/environment setting; no commit after aec39407 (rig phtmux path only) addresses the kit pane PATH; users hit it per the row. Needs fix + rig proof. |
| 3764 | R6 K34-K35 / R7 K36 | DONE for code (K34 f9ca3bee via 9a018549, K35 a6de061d, K36 99656f6d + 562f588e + 9cad5c9b gap-1S AGA+RTG); remaining proof is row 3961 |
| 3809 | RL16 AMARCHY-WELCOME | OPEN-BLOCKER (gated by RL18) | Welcome/Help commits in main (dda77bd3, 9e9ce3b8, ed94e6ab, b6b27d0b); row still [~], no line closing it, and ledger 3844 shows the rig 96 reinstall had no Amarchy start look, so no pixel/eye proof of the welcome window. Close only after the s32 eye-check. |
| 3824 | S31-START-PALETTE | OPEN-BLOCKER | Code fixes in main (cf0d408e/d11e29ee, e7e321e2/1b0eb507) but the 2026-11-03 rig 96 reinstall (ledger 3844) measured flat calls=0, grey backdrop (170,170,170) not (14,14,20); opus start-look hunt (s32-evidence/start-look.md) not shown closed. Same family as S32-PLAN result. |
| 3906 | MAKE-DIST-NO-STAGED-FROM | OPEN-NOT-BLOCKING | Guard is deliberate (a2398557, 133cd0c0; DIST-STAGED-FROM 3721 = NOT A BUG). R10 runs make kit-stage first; the rig 91 eye-check dist already needed a kit-stage workaround (3996). Verify at R10, no code change. |
| 3913 | S32-PLAN | DONE | Plan was the order of work: INSTALLER-FLAT-OFF + RIG-TIMEOUT-VERDICT fixed, merged, then the reinstall from main happened (3844, main 952d2457). Its failure is tracked under S31-START-PALETTE, not here. |
| 3931 | UPDEMO-GLOBE-PARTIAL-FRAME | DONE (probably; owner re-look pending, non-blocking) | K36-ANIM-BLINK (3965-3969) is the same symptom (upper ~20% of the picture, rest blank); root cause draw_rows_now painted images after all text, fixed by c7f7b3d4 (2026-11-03 09:22). Owner's rig 91 sighting predates it (row is before ANIM-BLINK, rig 91 refreshed 12:10 after). Put on the owner eye-check list. |
| 3961 | kitty animation clock | DONE | 92d580cf (one switch per wake), 562f588e proof AGA+RTG, 72f474ac, 9cad5c9b K36-GAP-1S PASS on RTG and AGA. The row's checkbox is just stale: tick it. |
| 3973 | UPDEMO-NEW-FEATURES | OPEN-NOT-BLOCKING | Scenes (a) 585678ad, (b) 180af2c1, (c) 81e4754c, (d) 3a1a571f, themes 9e2be165, media d7270e60 all in main; K36-ZOVER-ROWS fixed d80ac51e + 63b1c7e6. Only the owner eye check on rig 91/96 is outstanding; demo content is not a release gate. |
