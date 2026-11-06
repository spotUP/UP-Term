# Small todos burn 2 (read-only triage, ledger lines 2900-3250 and 3760-4045)

Method: one `git log main -n 6000` dump grepped by row id and topic, `git merge-base --is-ancestor` on every hash cited, grep of amarchy/ code. Not claimed without proof.

| ID | ledger line | verdict | proof |
|---|---|---|---|
| AMTILE-RIG-MONOCLE-PROBE | 2926 | OPEN | 49404108 (research: monocle raises on plain activation) is on main; rig step still uses ui_front, no commit changes it |
| AMARCHY-WB-BACKDROP | 2935 | RIG-ONLY | code: 177b000e, e741cb4d, c3073e07 (S32-WB-BACKDROP, backdrop tick at start) all on main; real-install check = RL3 / S32-EYECHECK-96 (a) PASS frameless WB + pattern; check darker-tone contrast on a fresh install |
| DESK-CHECK-INVERTED | 2990 | RIG-ONLY | desk_dos.c no longer has `!desk_check` (grep: desk_check only in desk_core.c:731,1220 + tests/test_desk.c); decision moved to desk_core; RX1 rig re-run result not in main log |
| DESK-CURRENT-MARKER | 2991 | OPEN | no commit mentions it; no proof of fix |
| DESK-RX1-COMPARE | 2994 | OPEN | no commit; 2eaa9282 only adds failure checks on requesters/font.prefs |
| DESK-POINTER-CRASH | 2997 | STALE | b375c835 (skip Pointer editor below v47, on main) + 70b702a6 (byte-exact pointer.prefs fix); later rows (R1.17 v8 3160) replace the symptom with Program Error requester |
| AMCONF-NO-PROFILE-HEADER | 2998 | OPEN | design question, no commit |
| s23 AMARCHY-WELCOME | 2980 | RIG-ONLY | RL16 W1-W12 on main: dda77bd3, 9e9ce3b8, ed94e6ab, b6b27d0b (hidden welcome-shown key in settings_def.c, window in gui.c); check: fresh rig start shows window, tick box, restart, absent |
| R1.17-WBRESET (+v3, v8 rows 3141, 3160) | 3105 | RIG-ONLY | fe80eec4 (CloseWorkBench/OpenWorkBench patch), 1fb65b1a, 439ccceb on main; check: 3.2 desk apply on rig, no IPrefs System Request (RL15 3826 still open) |
| R1.17-PEN11 / v1 | 3108 / 3112 | OPEN | no commit; 81615760 (RL8 step 2 pen maps) may supersede, unproven |
| R1.17-DKPUT-PROOF | 3162 | OPEN | no revert proof commit |
| SELECTA-THEME-SEND | 3106 | OPEN | no commit on main; blocked on MusicPlayer push |
| F3-NEWTAB v2/v3/v5 | 3136/3139/3156 | OPEN | work only on local wip branches (wip/newtab-*), none on main; v3 blocked on owner pen policy |
| AM-FLAT-WB | 3166 | RIG-ONLY | dk_wbborder in amarchy/desk_dos.c; frameless WB PASS in S32-EYECHECK-96 (a); icon-border part and 3.1 behaviour unchecked; check on 3.1 and 3.2 rig |
| ICON-AA | 3189 | OPEN | parked by owner decision; related ICON-AA-WRONG-BG done b31fbc67/dea43f24 (different row) |
| REXX-DEAD-AFTER-THEME | 3191 | OPEN | no commit, never probed |
| FLAT-OWN-PATCH | 3195 | OPEN | only research/plan: 2d31b53c, a19651ec; no code, owner decisions OD-F* open |
| FLAT-DOPUS | 3205 | OPEN | 011ab65e sends 11 lister pens (host done); rig needs DOpus build run; plan-only otherwise |
| AMARCHY-DESK-EXTRAS / DOPUS / MUI / VISUALPREFS, AMARCHY-PLANS, AMARCHY-DOPUS-DESKTOP | 2937-2951 | OPEN | plan-level rows; partial 3bd44cd3, 1361059b, 011ab65e only; no per-row closure |
| RL3 FLAT-WB-BACKDROP | 3797 | RIG-ONLY | same code as AMARCHY-WB-BACKDROP; needs real-install proof on fresh full install |
| RL8 FLAT-COLOUR-OVERHAUL | 3805 | OPEN | steps 1-2 on main (24b59aaf, 81615760); S32-EYECHECK-96 (c)/(e) still open, owner review pending |
| RL17 INSTALLER-RECHECK-9e351b55 | 3833 | OPEN | rerun needed, no commit |
| RL20 SELECTA-AA-RICHUI | 3840 | OPEN | "Not started" in row; no commit |
| RL21 SELECTA-AA-ICONTEXT | 3841 | OPEN | only probe 4f50c407; patch not written |
| S31-START-PALETTE | 3845 | RIG-ONLY | e7e321e2 merged 1b0eb507 (on main), test fails when stubbed; check: rig 96 reinstall, start path shows palette/pointer/font |
| S32-EYECHECK-96 | 3859 | OPEN | (c)/(e) S32-STALE-GADGETS still open |
| S31-FLAT-BUDGET | 3872 | DONE | 7e4ad79f on main (ancestor check ok); row says make flat-budget PASS, pushed d11e29ee. Row can be ticked |
| AMI-PUT-TRUNC | 3873 | OPEN | no commit |
| INSTALL-CHECK30-AMISSL-PREBOOT | 3891 | OPEN | vtcon-ee/Installer; not triaged further |

Counts (rows or row groups listed): DONE 1, STALE 1, RIG-ONLY 7, OPEN 20.
Not triaged (budget): KITTY-TRUECOLOR 3762 (kitty, out of scope).
