# Release ledger audit (2026-11-04, read-only; todos.md not edited)

Scope: open `- [ ]` rows of thoughts/spot/todos.md matching vtcon-ee / RELEASE / release blocker|gate / INSTALL-CHECK / CON-ASYNCPKT / UPDEMO / S55 / S56 / KITTY- / K4x. 29 rows.
Method: `git log` subjects (last 1500 commits), ticked rows with the same tag, test-register.md, evidence files. Proof of DONE comes from commit subjects, not from re-running anything.
B = blocks the UP-Term release.

| Line | Row | Class | Evidence / what is left | B |
|---|---|---|---|---|
| 7 | AMIBERRY-DEFAULT (s48) | OPEN-CODE | Branch not merged; later commits b958e0b1 (never pause inactive window), 5d3f99d0 (--no-jit) but eb5a629f handoff: Amiberry JIT-off gave no verdict. Hunt for wasabid UI-read hang open. | no (FS-UAE is the rig default) |
| 499 | RELEASE BLOCKER install_rig base check 89 (empty SYS:UP-Term/bin) | STALE | October v1.2 era; only tag is nas-v1. Superseded: 78a5cef8 fresh-all 44 of 44 on rig 91, f270393b "kit uninstall check passes". Unverified: no commit names check 89. | no |
| 893 | Owner A1200 widths test, then real v1.2 upload | OPEN-OWNER | v1.2-test flow predates the 1.7 numbering; owner A1200 check + release upload still ask owner. | owner |
| 959 | NS3.3 SIGWINCH leak (release blocker) | DONE | 93002380 "NS3.3 release recheck: pane_close and host_exit_cycles 10/10 on rig 69". | no |
| 1053 | upkg_command_index short_options literal (patch 0016) | OPEN-CODE | Row says AFTER RELEASE. | no |
| 1088 | RELEASE-1.0 umbrella | OPEN-OWNER | Steps 1-3 done in ledger; (4) repos.lock re-pin, (5) base kit build + dist-checks, (6) release upload (asks owner) not shown done. No release tag besides nas-v1. | owner |
| 3653 | KITTY-FINISH + KITTY-DEMO-APP (s48) | OPEN-CODE | Protocol K30-K41 done (see 3757/3763). Demo apps: icat done (UD4, K41-ICAT-F100 1ed14b5f). timg, chafa+GLib, nnn preview: no commit found (unverified, grep of log only). | no (owner scope) |
| 3706 | s29 AMIBERRY-RIG-HANG | OPEN-CODE | Same hunt as row 7; no fix commit. | no |
| 3718 | RIG-OS32-NO-RTG (W43) | OPEN-CODE | Not started per row; rig tool/3.2 RTG switch. No commit. | no |
| 3719 | BOOT-31-WB-DEADLOCK | DONE | 13955b72 merge s30-wblock; 3a1cafc3 cause recorded. | no |
| 3757 | KITTY-FINISH (s51) K30-K36 | DONE | 192eceb3 (K40d), 9cad5c9b (K36 gap), K35 a6de061d, b0a0a42a ticks K40/K41. | no |
| 3760 | KIT-AMARCHY-ALL (release gate) | OPEN-OWNER | Owner holds release for Amarchy RL15/RL16/RL8; 1230e176 KIT-AMARCHY-FLAT run 3 ok. Amarchy session (vtcon-16) must report READY on main. Not verifiable from vtcon repo. | yes (gate) |
| 3763 | KITTY-TRUECOLOR | DONE | 5ff4b661 plan, K40 + K41 rig 91 3 of 3 b0a0a42a, cbd9e471 AGA pixel-equal. | no |
| 3765 | IXEMUL-EXECVE-SHARED-FH | OPEN-CODE | tmux fixed in tmux-amiga; screen/ptyrun/other dup'd-tty programs open. Ports repo, no vtcon commit. | no |
| 3894 | INSTALL-CHECK30-AMISSL-PREBOOT | DONE | d88e7821 merge s51-check30; 4054 shows rig 96 fresh-all 3.2 installer [PASS] from fb6aa600. | no |
| 3900 | K41-AGA-REF | DONE | cbd9e471 merge s51-k41aga. | no |
| 3915 | K40d RUNNING | DONE | 192eceb3 merge s51-k40d; b0a0a42a. | no |
| 3919 | INSTALL-CHECK30 RUNNING | DONE | Same as 3894 (d88e7821). | no |
| 3925 | RIG-PUSH-STALE-BUILD | DONE | eaf70881 merge s52-rigpush. | no |
| 3929 | INSTALL-CHECK39-SHADOW-LEFTOVER | DONE | 7f3763d0 merge s52-check39; 78a5cef8 44 of 44. | no |
| 4006 | UPDEMO-NEW-FEATURES | OPEN-OWNER | UD1-UD8 all ticked (4007-4014); S56-UD2/UD5 merged (2b9084a6, 69d5da26). Left: owner eye check on UD2 globe over photo (ab9cc6b9: band not reproducible on host, guard only; test-register row 52 rig 110). | owner |
| 4054 | RIG96-FRESH-STOCK-LOOK | OPEN-OWNER | 3.2 redo: backdrop/frames PASS, knob FAIL (S32-KNOB-NONE-FRESH, a7567309 R3a). Knob code fixes since: 85bdf561/7e51272c (Locale), XCON knob. Owner eye check on rig 96 at 7e51272c WAITING (test-register row 50). | owner |
| 4072 | S33-RIG-ONLY-BATCH (9 rows) | OPEN-RIG | Code on main; one shared rig 96 fresh-all pixel pass for FLAT-BUTTON-FILL, FLAT-SCROLLER, FLAT-PENS-NOT-LIVE, WB-BACKDROP+RL3, WELCOME, R1.17-WBRESET, AM-FLAT-WB, S31-START-PALETTE, DESK-CHECK-INVERTED. Partly covered by R3a (backdrop PASS) but not recorded per row. | yes |
| 4073 | STRUT-REFLOW-RESTORE | OPEN-RIG | Code DONE 81279a13 + 00aaf8cf (cap 32, restore). Left: strut_off rig row rides next shared run. | yes (small) |
| 4086 | S56-PALETTE-31-SAFE | OPEN-CODE | Research 386666d6; bisect agent, desk_palette_31safe() in desk_core.c + tests, route desk_dos.c writers, gate lift: none committed. Later hunts (0edf1639, 1f33762b) say Copy crash is guest-side IPrefs AddHead with owner prefs bytes; FS-UAE upstream draft fccc737e. Affects 3.1 palette only. | 3.1 palette only |
| 4096 | S55-RIG-RECHECK-UD3-UD6 | DONE | s56-evidence/ud-recheck.md (rig 104, main 8ccdbb7f): UD3 PASS, UD6 PASS; UD2 and UD5 FAILED there, then fixed by S56-UD2 / S56-UD5 merges. Re-verify of UD2/UD5 after fix: UD2 only rig 109/110 corner + band data, UD5 not re-recorded (unverified). | no |
| 4104 | CON-ASYNCPKT-87000004 | DONE | Duplicate of ticked 4106; fix 4ea3135c, merge d873ce13, guard test in make_hygiene, 0a9eb351 rig 96 boot 3 of 3 clean. | no |
| 4113 | S56-VTCON16 (a)-(d) | DONE (mostly) | (a) FLAT-MARKER-RUN5 confirmed 781752ff (row ticked 4120s); (b) 0a9eb351 + rig96-con-fix-boot.md; (c) stock v3.2.35 retest a312b959 + lldb c1d3a362 + cb830707; (d) 4705d533 is on origin/main (main..origin/main 0 ahead). Remaining: owner knob eye-check on rig 96 (same as 4054). | no |
| 4125 | S56-LOCALE-SAVE-FILL | OPEN-RIG | UNVERIFIED read (test-register row 49): one pixel dump of Save button fill on next shared rig run; also inactive WB knob pen 249/250/251 compare. | small |

## Counts
DONE 13 (959, 3719, 3757, 3763, 3894, 3900, 3915, 3919, 3925, 3929, 4096, 4104, 4113), STALE 1 (499), OPEN-CODE 7 (7, 1053, 3653, 3706, 3718, 3765, 4086), OPEN-RIG 3 (4072, 4073, 4125), OPEN-OWNER 5 (893, 1088, 3760, 4006, 4054). Total 29.

## Release path (what remains)
1. OPEN-RIG 4072 + 4073 + 4125: one shared fresh-all 3.2 run from current main (rig 96, --idle-minutes 0), pixel reads.
2. OPEN-OWNER: eye check on rig 96 (knobs, Locale, UPDemo 15), KIT-AMARCHY-ALL READY from vtcon-16 (RL15/16/8), then 1088 steps 4-6 (repos.lock re-pin, kit build + dist-checks, upload).
3. No OPEN-CODE row blocks a 3.2 release; 4086 only gates the 3.1 palette.
