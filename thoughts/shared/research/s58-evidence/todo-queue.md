# vtcon todo queue (s58, 2026-11-05)

Source: thoughts/spot/todos.md, 468 open `- [ ]` lines (grep). Triage by a sonnet agent with a 40-call budget, so classification is from each row's first ~150 chars plus a keyword match against commit messages and `[x]` rows. Treat "done?" flags as leads, not verdicts. Sizes are estimates.

## Exclusions (counts)
- 106 rows: vtcon-c1 / Amarchy session (S57-AMARCHY-READY A2-A8, AMARCHY-*), TE-*, S57-BOOT5-HANG, S57-FREEZE-2IN9, S57-ICL-PEN*, S57-PENS47*, BACKBURNER, POST-RELEASE (keyword match, one grep).
- ~25 rows other repos (MusicPlayer/Selecta: lines 3095, 3106, 3511, 3585, 3647 SELECTA-*; upterm-ports/ixemul-vtcon-only rows 3034-3049 PR26 kill, 3220, 3222) -> hand to those sessions.
- 4224-4232 S57-START-POINTER-HANG / double-mount: in flight (rig 141 agent, vtcon-8c) -> exclude.
- Stale-suspect: ~270 rows are progress narrative from 2026-10-19..11-03 ("agent running", "next:", "P2..P6", IDE-speed A2-D3 lines 551-601, upkg test1/test2 lines 958-1063, DOpus D-rows 1992-2044, upkg wb-ui 2052-2260, sqlite/fts5 2296-2329, CI feed 2433-2440, FLAT s25-s28 3097-3430, release RL1-RL17 3786-3839). Many have matching `[x]` rows (e.g. FLAT-SCROLLER, FLAT-SOLID-FILL, FLAT-BUTTON-FILL, R1.17-WBRESET, F3-NEWTAB, ROUND-CORNERS, IXEMUL-SIGRC-DEPLOY, AMIBERRY-DEFAULT, NVIM-*, RIG-BATCH-DAILY all have `[x]` hits). NOT queued as work; see Phase 0.

## Phase 0 (do first): ledger reconcile, haiku, 2 agents, read-only + tick edits
Q00a haiku: lines 1-1500. Q00b haiku: lines 1500-3500. Q00c haiku: 3500-4300 (after a/b). For each open row: `grep` ID in `git log --all --grep`, `[x]` rows, and file existence; tick done rows with the commit hash, list the survivors in s58-evidence/reconcile-<n>.md. Output feeds this queue; budget 30 calls each.

## Queue (ROW-ID | ledger line | class | size | blocked-by | model)

### A. Host-only, unblocked, S
Q01 DESK-CHECK-INVERTED | 2990 | host-only fix (desk_dos.c:132 inverted test, cause known) | S | - | sonnet
Q02 DESK-CURRENT-MARKER | 2991 | host-only fix (re-pick same theme skips desk apply) | S | - | sonnet
Q03 AMCONF-NO-PROFILE-HEADER | 2998 | host-only (decide design vs bug, then fix) | S | - | sonnet
Q04 RIG-IMPORT-REFRESH | 3878 | host-only fix (import must not refresh a running rig) | S | - | sonnet
Q05 RIG-REFRESH-CTAGS | 4051 | host-only fix (FileNotFound in refresh) | S | - | sonnet
Q06 MAKE-DIST-NO-STAGED-FROM | 3931 | host-only fix | S | - | sonnet
Q07 RIG-CLONE-NEW-NUMBER | 2897 | host-only fix (rig.py start --fresh needs build/rigN) | S | - | sonnet
Q08 TMUX-NCURSES-PATH | 3766 | host-only fix (Makefile path) | S | - | haiku
Q09 test_rig_kit_stamp shadow_pass | 3134 | host-only test fix (pre-existing red) | S | - | sonnet
Q10 ARCHIVE-COMPLETION | 1923/1925 | host-only (owner said yes: complete rules for lha/lzx/unzip/tar/xz/gzip in dist/vshrc) | S | - | sonnet
Q11 VSH-BANG-PID | 3041 | host-only (owner decided: $! = real Task address) | S-M | - | sonnet
Q12 FOLLOW-UP patch 0016 short_options | 1053 | host-only (upterm-ports patch, other repo: confirm owner lane) | S | ports lane | sonnet
Q13 TMUX-KIT-REFRESH | 3767 | host-only (repos.lock/dist pickup) | S | after Q08 | haiku

### B. Host-only, M
Q14 JOB-ENV-UNSET | 3001 | host-only (JOB_DONE/JOB_WORD unset) | M | - | sonnet
Q15 AMIGA-IN-VSH (endcli exits UP-Term, other Amiga cmds) | 1118 | host-only | M | - | sonnet
Q16 VSH-BG-IGNORES-TERM | 3040/3062 | host-only (confirmed on rig 9, cause = task address; overlaps Q11) | M | after Q11 | sonnet
Q17 UPDEMO-NEW-FEATURES | 4007 | host-only demo/updemo.c + tour (research s54-updemo-gap.md exists) | M | - | sonnet
Q18 SHELL-TUTORIAL | 40 | host-only feature (needs reachability test) | M | - | sonnet
Q19 S56-FLAT-BUTTON-FILL-TINT | 4153 | host-only fix (idle fill 8d8d8d pen 3, held bebebe) | M | verify on rig after | sonnet
Q20 S57-LOCALE-LIST-NO-SELECT | 4188 | host-only fix, rig verify later | M | - | sonnet
Q21 S57-LOCALE-GADGET-BLACK + S57-GADGET-LINE-MISSING | 4189, 4159 | host-only draw fixes | M | - | sonnet
Q22 S32-SCROLLBAR-PROP / S32-SHELL-ACTIVE-GADGETS / S32-GREEN-LINE-96 / S32-INACTIVE-COLOUR | 4046, 4045, 4043, 4050 | host-only draw fixes (check [x] first; 4050 has 2 `[x]` hits) | M | Phase 0 | sonnet
Q23 BAR-BEHIND | 3169 | host-only (bar covers WB windows) | M | Phase 0 (has [x] hit) | sonnet
Q24 INSTALLER-CHECK10 | 3744 | host-only (4 commit hits, likely fixed) | M | Phase 0 | sonnet
Q25 IXEMUL-LINK / IXEMUL-FGETWC / IXEMUL-EINTR | 1367, 1368, 2470 | host-only (ixemul-vtcon repo, other checkout: owner lane) | M | ixemul owner | sonnet
Q26 MC port research | 4133 | research (findings file only) | S | - | sonnet
Q27 Port next terminal apps | 862 | research then port | M each | Q26 | sonnet

### C. Rig batches (one shared rig run each, 060, purpose text = "vtcon queue: <batch>")
R-A "3.2 verify-only" (one rig, one boot): Q19, Q20, Q21, Q22, S56-FLAT-PENS-NOT-LIVE-PIXELS (4155, needs pixel PASS from measured shot), S31-START-PALETTE (3848), RIG96-ASSIGN-LOST (4168, 3.2 rig 96 flow), K36-ANIM-BLINK (4022, rig 91 kitty). Blocked by Q19-Q22 landing on main. sonnet runs fixed steps, haiku reads shots.
R-B "3.1 batch" (S56-31-BATCH, 4156): FLAT-BUTTON-FILL 3.1, R1.17-WBRESET 3.1, AM-FLAT-WB, FLAT-PENS-NOT-LIVE 3.1, DEEP-PENS47-31 (3544, check overlap with excluded S57-PENS47), F11-31 (3555). One `--os 31` rig. sonnet.
R-C "Kitty": R6 K34-K35 (3786), K36 (in R-A), KITTY-FINISH (3653). sonnet.
R-D "rig/installer reliability": IPREFS-SYSREQ (3827), INSTALL-AMISSL-STALL (3738), RIG51-IXEMUL48-REQ (3737), RIG-APPICONS-TEARDOWN-REQ (3734), RIG-OS32-NO-RTG (3718), RIG-REBOOT-KEEPSYS (4093). Each has a repro on a stated rig; verify whether still reproducing before any fix. sonnet.
R-E "A1200/Amiberry final" (owner said A1200 down; Amiberry stands in): RL18 OWNER-DEMO-RIG + release build (3839) after all above.

### D. Hunts (L, one agent each, own stop point)
Q30 S57-RIG96-FREEZE | 4243 | rig hunt | L | - | opus
Q31 S57-RIG-REBOOT-HANG | 4259 | rig hunt (boot 1 dead after full install) | L | share rig with Q30? no, separate | opus
Q32 RIG96-DRAG-COPPER | 4198 | rig hunt (vtcon-8c may own: ask first) | L | owner lane check | opus
Q33 S56-WB-RESET-BLOCKER | 4152 | rig hunt, needs owner window list | M-L | owner input | opus
Q34 IXEMUL-EXECVE-SHARED-FH | 3765 | hunt + ixemul fix | L | ixemul lane | opus
Q35 NEWSHELL-BEEP | 1084 | rig hunt | M | - | opus
Q36 RIG3-BOOT-GURU / RIG3-CPU-RESETS / RIG-RESET-LOOP family | 3007, 2999, 3025, 2171 | rig hunt, check if superseded by boot-console capture (606dafcd) | L | Phase 0 | opus
Q37 SYNCTERM-PORT | 54 | port | L | owner scope | sonnet
Q38 IDE-SPEED-POST10 | 3788 | perf | L | post-1.0 label, ask | opus

### E. Owner-decision rows (need AskUserQuestion; one line each)
D1 ROUND-CORNERS (3375): run the 1-2 day hyperlayers test, yes or no.
D2 D13/D12 DOpus (2005, 2008, 2016, 2022): who installs the filetype, default editor joe vs nvim, shell launchers via vsh, install DOpus 5.104 on a rig.
D3 UHC-UPSTREAM (2653): ask Patrik upstream, yes or no.
D4 R10/R11 release (3790, 3791) and RL18 (3839): owner OK to cut release.
D5 S56-WB-RESET-BLOCKER (4152): owner supplies the window list.
D6 FLAT-OWN-PATCH (3195) and FLAT-DOPUS (3205): scope, whole own flat-UI system patch yes or no.
D7 ICON-AA question (3189) / AM-ICONS-FILLED-SET (3540): set choice.
D8 IDE-SPEED-POST10 and other "post-1.0" rows: still wanted before release?

## Counts (of 362 non-excluded rows)
Queued: A 13, B 14, C 5 batches (~25 rows), D 9, E 8 decisions. Stale-suspect, handled by Phase 0: roughly 270.
