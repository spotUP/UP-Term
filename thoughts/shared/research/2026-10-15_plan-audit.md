---
date: 2026-10-15
topic: audit of every open plan item (evidence for the finish-all plan)
tags: [audit, plans, status]
status: final
---

# Plan audit 2026-10-15 (read-only, main checkout /Users/spot/Code/UP-Term/vtcon, HEAD ebe2e143)

Classes: NS = NOT STARTED, IP = IN PROGRESS, DBU = DONE BUT UNTICKED, BOO = BLOCKED ON OWNER, OBS = OBSOLETE/SUPERSEDED.
Counts: I used `grep -c '^ *- \[ \]'` (same as the main session) and re-derived from structure where noted.
Other branches/repos (not judged): feature/bebbossh-1.46.1 (K6), feature/x2-vtcon-1 (X2), feature/r1-rom (W34/R1), feature/cc1-cc2 (W10/CC1-2), feature/demo-perf, feature/dctelnet-root (source-umbrella), feature/race-r5-r12-wip, build/wt-u2 (X1). Hashes marked PI live in ~/Code/UP-Term/amiga-pi (a different repo; I spot-checked 6, not all).

## 1. plans/2026-10-07-amiga-pi.md  (179 open by grep)
Structure count: the 179 are NOT 179 distinct items. Phase bodies list D1..O6 (90 lines, 335-685; N0 only there) and a checklist at lines 811-921 repeats 89 of them (no N0). Distinct open = 90. Plan footer says "Total: 117 items, 0 done" (stale; A, B, C are ticked, the file was never updated after phase C).
Counts (distinct 90): DBU 89, BOO 1, NS 0, IP 0, OBS 0.
Source of truth: ledger plans/2026-10-07-amiga-pi.progress.md (phase checklists, with commits). Both copies of each id need ticking.
- D1-D8 DBU. Ledger "Phase D" `Running count 16 of 16` (line 214), D8 rig step 3 PASS line 255; D parity follow-up 15 of 15 (line 285). PI tools_pi.c, ext_gate.c exist (ls).
- E1-E11 DBU. Ledger line 423 "18 of 18"; E8/E12 rig step 5 PASS 21 checks (line 539); E11 line 538 `pi --export` exit 0 on the Amiga-written file.
- F1-F6 DBU. Ledger "Phase F close-out 16 of 16" (line 807); F6 rig step 6 PASS line 811 (note: step numbers in the ledger were renumbered; plan says step 5 for F6, ledger says 6, plan G5 says 6, ledger says 7 -> H uses 14).
- G1-G5 DBU. Ledger line 618 "5 of 5 + gates"; G5 rig step 6 PASS line 699.
- H1-H8 DBU. Ledger H0-H10 "Phase H 16 of 16" line 921; merged VT f64d727 (on main). H8 = rig step 14 PASS (HF5 line 959). Deviation recorded: plan said step 7.
- I1-I6 DBU. Ledger Queue run 2: I1-I5 PI f89626c (verified exists), I6 VT 0897c66 rig step 8 PASS (on main).
- J1-J4 DBU. PI ecd2a7b (verified), J4 VT 1c7dd69 rig step 9 PASS (on main).
- K1-K8 DBU. Ledger line 721 "Running count: 8 of 8", K1 probe line 724, K8 rig step 10 PASS line 770.
- L1-L10 DBU. L1 VT 402f247 + PI 853b4b4 (verified), L3 ext_lua.c exists, L10 VT 00d359c rig step 11 PASS (on main), L7 caveat: "34 done, 7 not planned" of 41 events (ledger line ~1109) -> tick with the COMPATIBILITY.md rows as the plan allowed.
- M1-M9 DBU. VT 441825e, 7e14a27, d414f31 (all on main); files llm/be_google.c, be_azure.c, be_bedrock.c, sigv4.c, evstream.c exist; M9 closed: step 12 PASS 31/31 (ledger line 1219, df53a0b).
- N0-N8 DBU. Ledger "Phase N 9 of 9" (line 880); N0 line 882 (17 goldens tests/pi/tui); N8 rig step 13 PASS 11/11 (line 890).
- O1-O5 DBU. O1 VT 244bfa8, O2-O5 VT 08af403, d5eb4e4 + PI 1e412cf (verified). 
- O6 BOO. Ledger line 1122: "owner: one real login per provider on the Amiga with AmiSSL ... AmiSSL TLS path only compiled, CA roots unchecked".
(O4 plan text says "tools/claude_fixture.py fake OAuth"; done in d5eb4e4.)

## 2. plans/2026-10-07-amiga-pi.progress.md  (20 open; 461 ticked by grep -ci '^ *- \[x\]'; 481 total)
Counts: DBU 10, BOO 5, IP 1, NS 2, UNCLEAR 2.
- O6 BOO (owner real login; duplicate of plan O6).
- G2 BOO "Owner rig check open" (footer right edge; VT d43d180 fixed the ellipsis case; flush-right held already).
- G3 BOO "Owner rig check open" (outputPad 0 header; PI 2878897 verified, goldens start_w64).
- P2 BOO "Owner check open: SetEnv SAVE ANTHROPIC_CUSTOM_HEADERS ... no 400"; workspace key works end to end per owner 2026-10-13, header path itself not owner-tested. Code merged VT 2f7c082, 390be79.
- X2 IP (+BOO). X2-1 email sent 2026-10-13 (ledger 1213); X2-2.. pending "maintainer reply; X2-5 after" (ledger line 1245). Branch feature/x2-vtcon-1.
- X1 UNCLEAR: stray PI_DEBUG_LOG hunk in build/wt-u2; "asked to report", no later line resolves it. Not checkable from vtcon (amiga-pi worktree).
- K6 DBU: Q3-10 [x]; VT 4ae821c + merge 9e4daeb (both on main). Open line is the older copy. (Mentions feature/bebbossh-1.46.1 f0727e4.)
- L3 DBU for code: Q3-8 merged VT 98e378b, 6f2096b, 60cac54 (on main), llm/be_pi_messages.c + llm/radius.c exist; "Radius live PASS rig 1 2026-10-14 14:19" in ledger. Original text said verify needs account: done by owner.
- P12 DBU: Q3-9 [x]; plan amiga-pi 2026-10-14-pi-1.2.0.md; batch A and B pushed (ledger). Unconfirmed: P12 close step "cut_cases oracle reproducible" (ledger says rides with U3 part 2).
- W-R2 DBU (closed as "not reproduced"): Q3-3 [x], 7 AGA cold boots fine; W43 PAL message deterministic and unexplained (still a loose end).
- W-R4 DBU: Q3-4 VT 7ffc275 (on main).
- W-A3 DBU: Q3-5 VT 0d27243, e1cef76, 6b505b6; 17 commands counted from the Makefile amiga target.
- W-T9 DBU: Q3-1 VT bcaa6d9 (on main). W-T14 DBU: Q3-2 VT c564642 (on main).
- W-T12 DBU: VT 3ff03ee (on main) "invariant one window background", plus c564642 padding colour.
- K2-V DBU: Q3-7 "kit-split plan 46 of 46 ticked" VT 4d06b6b (on main).
- KIT-R DBU: Q3-6 [x]; git 2f206863 "KIT-R rig results", 10fdf1ee "KIT-R done, pushed".
- W-T8 NS: re-measure on a quiet Mac; only ledger commit cd95458f mentions it (git log --grep W-T8; ledger grep: one line). 2nd check: no commit with BENCH numbers.
- W-T11 IP -> cause found (24-bit WB has 32-entry map, 0 free pens; ledger line 1293); the fix is W-T13.
- W-T13 NS: "Own PR". git log -S'RPTAG_FgColor' -- render returns nothing; ledger one line.
Stale headings: "Queue run 2 ... 73 of 92" has 99 ticked/9 open; "wave ... 17 of 32" has 21 ticked/11 open; "Queue run 3 ... 12 of 14" has 41 ticked/0 open; "Phase F Running count: 0 of 16" (line 604) vs 16 of 16; line 15 "phase B 1 of 12" vs "Phase B 12 of 12". Counts in headings cannot be trusted.
DBU count here: K6, L3, P12, W-R2, W-R4, W-A3, W-T9, W-T14, W-T12, K2-V, KIT-R = 11 (not 10; use 11).

Spot-check of 5+ ticked items (ledger vs git): Q1 (70c1ca5 exists, 2026-10-10, message matches), R6 (e2d11f2 on main), G1 (b222bdf TopazPro middle dot, on main), R1 (2e10b02 bash chunks, on main), K3 (9375165 on main), plus Q3-1/Q3-2/Q3-5/Q3-7 hashes, M1/M4/M5 hashes, I6/J4/L10/M9 rig commits: 24 vtcon hashes checked, all `merge-base --is-ancestor main`. File checks: llm/be_google.c, be_azure.c, be_bedrock.c, sigv4.c, evstream.c, be_pi_messages.c, radius.c, tools/rig/amigapi_rig.py exist.
Mismatches in ticked items: none false. Soft ones: (a) M9 was ticked "code only, NOT rig-run" (line 1119) and closed later (line 1219) -> ok; (b) E11 ticked as "owner" item but the owner asked the agent to run it and it ran through `pi --export`, HTML not looked at; (c) Q3-6 KIT-R text says "(not ticked: push open)" yet is [x] (resolved by 10fdf1ee); (d) "Q13 Q14 duplicate" line; (e) stale headings above.

## 3. plans/2026-09-28-vtcon.md  (30 open by grep; grep for any `[ ]` gives 48 because 18 are text mentions)
Structure: file last edited 2026-10-06 in most open rows. Counts: DBU 3, BOO 9, IP 3, NS 11, OBS 0, UNCLEAR 4.
- T3 (AmigaOS 3.2) DBU: plans/2026-10-03-amigaos32.md lines 47, 66 "DONE 2026-10-08 rig 1 --os32 Kickstart 47.115", prefs_rig 3.2 61/61 (prefs-ux plan line 193).
- R1 UP-TERM IN ROM / W34 IP-BOO: "InternalLoadSeg failed (ABI)" commit b3e8aea9; branch feature/r1-rom (other session). Not boot-proven.
- D1 DEMO MENU BOO: "OPEN: OWNER rig check only, menu -> tab -> tour -> tab closes" (built, merged 3e9545f).
- S1 SPEED IP: S2 (UPDemo perf) merged 0912d65, W-T2/T3 644bd8b, 76df0f0; the asm hot-path part has no recorded close (unconfirmed).
- RT1 / W8 NS: "OPEN 2026-10-06: no phase profile of retro32-term against ibmcon found". Second check: git log --grep 'RT1' only the ledger commit f98c493 (as the plan says).
- P7 BOO: "iTerm2 cell-level comparison and chat-style log scrolling ... OWNER to judge".
- Q2 NS: Neovim tui_spec replay; git log --grep tui_spec empty; ls tests has no tui_spec.
- T1 (submodule + swap engine) IP: pre-check 2 (68000 speed, W7) open, "owner test on real hardware".
- W7 NS (A500 rig skipped; ledger T2 "A500 rig config was skipped").
- V4 BOO "owner test on the A1200 (manual)". W3 BOO "OWNER: manual checks gaps-g1 1-10 ... none run".
- W6 UNCLEAR: commit 5f2dc765 "S1 -- CCON 1.2.8b9 at 30.68 s, re-race under creep's conditions" shows part done; main's number not seen.
- W10 (CC1 blitter overlap, CC2 sprite cursor) IP: hardware proof tools/rig/cc_rig.py; branch feature/cc1-cc2 (other session).
- W14 (Unix tool ports) IP: see section 4 (10 of 53 stale; many more done).
- W16 MicroPython NS: git log --grep micropython empty.
- W19 TUI prefs app NS: git log empty; prefs/ has GUI files only (prefs_asl/ui/dos/core).
- W27 CCON techniques IP: research doc exists; rows 5, 7, 10, 12 not done (plan text).
- W39 gcc 16 IP: "one hang is left in gcc 16's string objects"; commit 35bee4a2 ixstackext.
- W40 CPython asm NS ("future"), W38 tiny LLM NS ("LAST"), W55 native multiplexer NS ("not started"), W56 browser thin client NS ("LATER").
- W46 setup as / commands DBU/IP: /term /colors /setup 84f76b9; later /dither, /padding-color (bcaa6d9, c564642); 468f1b03 "W46 progress and what stays open" -> still partly open, treat IP.
- W47 resize reaches ixemul programs DBU: dd4cc166 (SIGWINCH), 54de76b3 resize_rig (tmux and nvim sizes equal TIOCGWINSZ). Plan line 27 says "Open: R7's freeze recipe (resize itself is W47)": rig PASS not re-seen, so DBU with caveat.
- W50 Replay drops off network IP/BOO: install freeze; fix 59dcb9fb (installer $o_dest, 2026-10-08) may be the cause of the Replay's failed boot but W50 freeze not shown closed. Owner on Replay hardware.
- W54 tmux over eriQue's auxcon: BOO "auxcon not released yet" (waiting for eriQue).

## 4. plans/2026-10-04-unix-tool-ports.md  (27 open by grep, 28 any `[ ]`; file says 53 items, 10 done as of 2026-10-06 - stale; ticked now 26)
Counts: DBU 6 (+1 unclear), IP 1, NS 18, UNCLEAR 2.
- 3.1 file, 3.2 tree, 3.4 watch, 3.5 script, 3.6 ncdu, 3.9 fzy DBU (conditional): each says "kit-staged ... open: 1.10"; item 1.10 is [x] "done 2026-10-11 in the kit (vtcon 85a76db)", and upterm-ports pkgs/ has file fzy ncdu script tree watch. UNCONFIRMED whether each is in the last built kit (dist/parts.txt is generic; I did not open the archive).
- 3.3 ps/top DBU (needs owner call checked): vtcon proc/ps_amiga.c, commit c785ca28, proc_rig 7/7 on rig 2; the "open for an owner decision" (649f8b63) was answered ("owner's decision" in the row).
- 3.7 coreutils 9 IP: ../upterm-ports/pkgs/coreutils/recipe.mk (VERSION 9.12, GPG-verified 2026-10-11) is UNTRACKED in upterm-ports (`?? pkgs/coreutils/recipe.mk`); kit still ships 5.2.1. Another session's uncommitted work.
- 4.1 vim, 4.2 mg, 4.4 Lua, 4.5 joe, 4.6 bc, 4.7 jq, 4.8 gawk, 4.9 column/iconv, 4.10 ed, 4.11 ctags NS: 2nd check ls upterm-ports/pkgs: no such dirs (pkgs = awk bzip2 coreutils diffutils file findutils fzy grep gzip ixcompat less libarchive mandoc nano ncdu ncurses patch script sed tree watch xz zip zlib). 4.4 Lua: pi has its own Lua 5.4 inside amiga-pi (ext_lua.c) but no standalone lua binary (unconfirmed shipped).
- 5.1 nc, 5.4 bebbossh patches, 5.5 make, 5.6 git, 5.7 rsync, 5.8 perl, 5.9 tldr NS (same pkgs listing). 5.4: K6 BebboSSH 1.46.1 is related (feature/bebbossh-1.46.1, ledger K6 merged 4ae821c) -> UNCLEAR whether 5.4 counts.
- 5.2 telnet IP/BOO: "C:uptelnet + vshrc telnet() exist, ffa32a4 ... its rig run is OWNER A1.4".
- 5.3 wget function NS/DBU?: `grep -n wget dist/vshrc` -> no hit; curl in dist/net. NS.

## 5. plans/2026-10-06-replay-hardware-tests.md  (12 open, 2 ticked; structure count 14 items)
All BOO (real Replay board 192.168.0.57; "Owner-only checks (eyes, hands)"). No commit since 2026-10-08 touches Replay results (git log --grep replay: 59dcb9fb, 281565d1 only).
H2 Install end (W50 freeze, pinned L393/L408, "Freeze point MOVES"), H3 ixemul 80.1 on Replay, H4 python, H5 nvim (OWNER), H6 ls, H7 icon, H8 UPDemo euro (OWNER), H9 command colouring (OWNER), H10 tmux/screen, H11 serial 0 bytes (OWNER cable), H12 serial login (needs sudo lrzsz, OWNER), H14 Uninstall leaves no block (needs kit rebuilt after 518e65b; kit was rebuilt later, KIT-R 10fdf1ee; not run on Replay). Counts: BOO 12; H3-H7, H10, H14 also depend on H2.
Related: wasabi-migration WM8 "owner M1 on the Replay open".

## 6. plans/2026-10-04-a2-progress.md (4 open)
All BOO ("OWNER"): 1.8 `Claude PING` on rig/A1200 (needs AmiSSL SDK build), 3.8 conversation on rig 77x20 + Ctrl+C, 4.8 rig tool checks, 5.5 rig end to end with A1200/030/060 timings. Note: Claude was run on the Replay (H1 PASS 2026-10-06, to NAS) and NL22 owner pending; the AmiSSL path was built since (O/Q ledger "AmiSSL TLS path only compiled") so 1.8 may be partially covered by rig HTTPS runs (ledger W1 "real HTTPS prompt", owner rig 3 2026-10-13): UNCONFIRMED.

## 7. Single open boxes
- 2026-10-14-nas-claude-local.md NL22 BOO: quoted in handoff 2026-10-15 "NL22 on the real Amiga ... at home"; plan line 391 "Manual (owner only, never ticked by an agent): NL21, NL22". NL21 is done (ledger "NL21 done (owner-delegated)").
- 2026-10-04-highlight-markdown.md R1 BOO: "manual checks on the rig (owner; steps below)".
- 2026-10-04-a1-claude-lan.md A1.4 BOO: "rig + A1200 run (OWNER ...)"; note says H1 on Replay does not cover steps 4-7.
- 2026-10-04-a3-claude-tui.md T13 BOO: "OWNER: on the rig against tools/claude_fixture.py, then with a real key".
- 2026-10-03-screens-and-dctelnet.md P3 NS/IP: "OPEN 2026-10-06: planar fast path exists 7ecdb88, but no cycle-exact comparison ... found" (not re-checked; feature/cc1-cc2 relates).

## 8. Draft plans without checkboxes (status field says draft for all)
- 2026-10-03-amigaos32.md (79 lines): FINISHED in substance. Lines 47, 66 "DONE 2026-10-08 ... Kickstart 47.115"; tail is the ENV: hang fix with test. Status field stale. Leftover: line 59 "OPEN: the real Installer on 3.2" was followed by line 66 DONE (Installer 47).
- 2026-10-13-kit-split.md (919 lines): FINISHED (46/46 ticked per ledger Q3-7, VT 4d06b6b); open tail: "Aminet upload (questions 5 and 6 first)" = owner. Status stale.
- 2026-10-14-prefs-ux.md (208): FINISHED; rig 61/61 on 3.1 and 3.2 (line 193), merged 5ab49b4, e7cb8e2. Open "Not verified": colorwheel on 4-pen Workbench, asl.library V38 floor.
- 2026-10-14-wasabi-migration.md (72): MOSTLY FINISHED; WM items DONE incl. 82d1e58; open tail "Owner: NAS deploy, real Amiga"; WM8 "owner M1 on the Replay open"; handoff 2026-10-15 says NAS deployed on wasabi 0.9 (ledger: NAS image 11, wasabi.rev v0.10) -> real-Amiga check open.
- 2026-10-13-source-umbrella.md (48): FINISHED; ledger L1 [x] ("owner GO 2026-10-14"), UP-Term/ siblings exist (ls /Users/spot/Code/UP-Term). Branch feature/dctelnet-root (other session) carries DCTELNET_ROOT; not judged.
- 2026-10-07-vsh-bash.progress.md (1225): FINISHED/ongoing ledger: "V95 landing run: 95 of 95, status implemented" (line 1209); tail is later fixes (9b7e29d). Status field stale.
- 2026-10-06-meta-repo.md (229): PARTLY IMPLEMENTED: ../upterm has repos.lock, bin, tests, toolchain, README, LICENSES.md (ledger K2-8 "upterm b10af24 repos.lock pins amiga-pi"). Phases beyond Phase 1 not verified; no unticked list to count. Mark as IP/unconfirmed.

## 9. Handoffs newest 5 (by filename): 2026-10-15_nas-deploy-wasabi-0.9, 2026-10-13_queue-run-2, 2026-10-12_queue-run-after-p11, 2026-10-12_px-n-merged, 2026-10-12_phase-h-tm-merged
- 10-15 Next steps 1 NL22 on the real Amiga (6 sub-steps) BOO; 2 real Amiga needs wasabid 0.9 from the kit, maybe pin ba83a43 for OS 3.0/3.1 or MUI BOO/decision; 3 `pairs list` on the NAS after NL22 BOO; 4 wasabi gaps: wait for creep's answer, update research section 6 BOO.
- 10-13 queue-run-2: 1 merge batches (done: I8, J5, L, M all in ledger), 2 O1 owner decision (made; O2-O5 done), 3 L7 L11 L12 M9 M10 B1 T1 Q13 Q14: all DBU/done in ledger. All closed.
- 10-12 queue-run-after-p11: Q13/Q14 ticked (ledger 1045-1046), Q21 [x], merged-queue items R1-R6 [x], phases I J L M O done (section 1). All closed.
- 10-12 px-n-merged: next Phase H (done), I J L M O (done), "colour as cheap as plain text (CCON 1.3.0)" NS/UNCLEAR, "rig runs never done (f-g-merged item 4)" UNCLEAR. Owner checks open: glyphs `▀█` and spinner on rig 3 (G1 b222bdf later fixed middle dot; spinner W1 [x]) UNCLEAR; Enter fix rig 1 and Left Cmd+C/V BOO. Small open items (not in ledger, grep 'Ctrl+C within 500 ms', 'exact-match-first', 'Ctrl+D twice' no hit): rig step 13 sends Ctrl+D twice, Ctrl+C double-press timing, error-text mask, scope_models exact-match, llm_mid_system doc, supportsToolSearch/grammar tools not ported -> all UNCONFIRMED/NS.
- 10-12 phase-h-tm-merged: P11 done (22 of 22); TM5/HF7/HF6-R2/PHS7 etc. are ledger Q-items (Q4 TM5 [x]); owner checks TM1 alt screen (Q14 [x]); three claude.ai connectors to disable by hand (/mcp disable Gmail, Google Calendar, Google Drive) BOO; commit port-amiga-effect skill in ~/Code/Up_Rough_Demo_System BOO; "pi_llm prints 32 [ERROR] cannot open .headers lines" noise UNCONFIRMED.
