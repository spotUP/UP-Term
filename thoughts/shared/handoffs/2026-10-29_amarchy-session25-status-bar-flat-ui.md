---
date: 2026-10-29
session: Amarchy s25
topic: P2 status bar close-out, R1.17 desk theme, F3 theme to tabs
status: handed off 2026-10-30
---

# Amarchy session 25 handoff

Plans: thoughts/shared/plans/2026-10-29-amarchy-p2-status-bar.md, thoughts/shared/plans/2026-10-28-omarchy-ai.md. Ledger: thoughts/spot/todos.md (s25 rows).

Correction to the s24 handoff: it asks the owner for a Pointer editor hand test. That is obsolete. R1.17 guards Pointer by editor version instead (b375c835): desk_dos_apply skips SYS:Prefs/Pointer when its major version is below 47.

## Done (origin/main)

- R2.13a/c super+b (8a99c34f, 8489f069, 3b795e73).
- R2.10 network: rig half (1caaa456, ed772e9d); A1200 down/up toggle PASS, bar goes "replayeth up" then "down" then "up", no Guru (5b82e738).
- R2.5b (0622dde7, 354eda1f).
- R1.17 Pointer version guard (b375c835), harness (05b7b821), harness fixes for style Down count and prefs put (57475a04).
- A1200 halves of R2.1/R2.2 (0445ec04); research doc says FPGA Replay with real 68060 (e720b80d).
- R2.12 tray: bar.conf order, on/off and run rows drive the live bar; tray click runs via proclaunch with stack 262144 (5e9fce25, 525ee11a).
- Bar overlap: tray segments drop after Agents; width sweep test (550e8f79).
- R2.15 docs: man page STATUS BAR section, README, Uninstall keeps bar.conf, parts.txt row (e42f707b, 7836ec42).
- F3 reopened with evidence (b00b6c21). Owner decision: an Amarchy theme switch recolours all open console tabs, agent tabs included, over hand-set tab themes.

## Pending at handoff time

- F3 bridge landed (8dfcfe69, 7fe6f04c): a theme switch recolours all open console tabs.
- F3 new windows landed (47a809a4): apply_profile returned early when the config had no profile section, so new windows skipped the theme colours. Colour logic moved to apply_profile_colours, called on that path too. Rig 6 p2 rig PASS; revert proof FAIL (101010). Not yet on the A1200.
- R1.17 WB reset LANDED 38923fcf: Amarchy drawn menu window held IPrefs palette; now hidden around the apply, pens waited for, editors sole ENV: writers. Rig 7 3.2 PASS, revert FAIL. Open: dk_put own revert proof + 3.1 desk run.
- 3.1 palette crash hypothesis refuted (rig 5); RIG31 reset loop is intermittent and follows a mode switch.
- A1200 deployed from e74f71a4: 10 deploy.list files, drift 0, Amarchy md5 fce801e6, not rebooted. amarchy-menu differs on the A1200 and is not in deploy.list (open: add it). Backups *.bak-20261029 kept (first deploy's copies, not overwritten). Owner test results pending.
- F3-THEME-BOOT LANDED e74f71a4: Amarchy writes ENV:up-term/theme-active at start. Rig 6 p2 18/18, revert FAIL. Note: tokyo-night bg maps to 101010 on a 16-colour WB, same as no file, so the boot write is only visible for themes whose bg is near a WB pen.

- R1.17-DKPUT-PROOF LANDED 2eaa9282: desk rig step "no Program Error requester from a stock prefs editor after the desk apply" plus ENV:Sys/font.prefs present. Rig 7 3.2 PASS 32/0; old dk_put FAIL 7.
- amarchy-menu uploaded to the A1200 (md5 7330151e, backup amarchy-menu.bak-20261029 66da7457). Still not in deploy.list.
- Agents running at handoff time (check ledger rows): BAR-BEHIND + BAR-TWO-ROWS (bar fix, evidence scratchpad bar-fix.md), THEME-PICKER-PENS (evidence picker-pens.md).

## Open, in order

1. R2.17 final A1200 check after F3 and R1.17 land. A1200 Amarchy is the 354eda1f build; backup UP-Term:bin/Amarchy.bak-20261029.
2. Desk rig steps default to 3.2 (owner decision).
3. Selecta writer P1B-M1 once MusicPlayer 75ea939c is on origin; THEME ARexx send waits for the peer's syntax (I676b).
4. R2.16 per-segment revert proofs.
5. Bar still overlaps below clock + System + gap width (clock and System never drop).
6. Untested tab theme entry points: chord super+shift+t, menu "Theme...".
7. W43 second PAL path; p3_keys snap flake; rig 8 boot loop / RIG3-BOOT-GURU; clock-format rig assertion; wasabid "rename into place failed" root cause; mkmanifest not run after parts.txt change.

8. AM-FLAT-WB (owner decision): Amarchy turns off WB icon borders and the WB window frame by default, user toggle keeps stock look, revert restores the snapshot. Lever: ARexx MENU WINDOW ROOT INVOKE WORKBENCH.BACKDROP (3.2 only, a toggle: read state first). Icon borders field unverified.
9. THEME-PREVIEW-SHELL (owner decision): live shell recolour on every highlight keypress, Esc restores; reuse the F3 bridge. After THEME-PICKER-PENS.
10. DESK-31-CRASH: 3.1 desk run, wasabi ConnectionRefused at "bar open again before the desk apply". Hunt after bar fixes; first probe Stack 262144 + wasabi guru.
11. DESK-IPREFS-FLAKE: "no System Request from IPrefs" failed once on good code.
12. Add amarchy-menu to deploy.list.

## Gotchas

- Use `--os 32` (tree build/rig/os-322) for 3.2; never build/rig/os32.
- Main checkout holds another session's files: detached worktrees only; ledger rows are the exception.
- Worktree branch delete needs -D (HEAD is the diverged local main).

## Added late s25

- aa989997 BAR-TWO-ROWS + BAR-BEHIND: title set on win->Title (WA_Title with WA_Borderless drew a title band); bar opens WA_Backdrop via TW_OPEN_BACKDROP. REGRESSION: bar clicks open no panels (BAR-CLICK-BACKDROP); fix agent running at handoff time (evidence bar-click.md).
- f4e785a2 THEME-PICKER-PENS: menu_text pen cache released on theme change (amarchy/pen_cache.h, mt_theme_changed). Host test 39 fail reverted, 0 fixed. Rig check pending in the live-shell agent.
- A1200 runs origin f4e785a2 (Amarchy 2f3a13ea, amarchy-gui e0d49706, amarchy-menu 7330151e), backups *.bak-20261029b. Owner chose to keep it despite dead bar clicks. Drift DIFF on vtcon-handler, pty-handler, ixkill unexplained (ledger row).
- ICON-AA parked: owner says build nothing; they are weighing letting themes replace all icons on the Amiga.
- Deleted stale scratchpad 3570c2eb (owner approved) to get disk to 2.2 GiB.
- 726d1687 THEME-PREVIEW-SHELL: preview_sync (gui.c) calls gt_preview_term (gthemes_core.c) on every highlight move; leaving the list or cleanup() restores the saved theme. Host test fails 2 with the bridge off; rig 6 --preview 4 FAIL old / ok new. Unverified: tab pixels, selected-row contrast. Not on the A1200.
- df540b9d MENU-BACKSPACE-CLOSE (host only): Backspace auto-repeat on an empty query reached mn_back; 0.4 s guard. Rework running: GUI path uses IEQUALIFIER_REPEAT instead (no double-tap penalty); console menu.c keeps the time guard.
- New ledger rows: MENU-SEARCH-DEEP (root search finds submenu leaves), REXX-DEAD-AFTER-THEME (port dead after theme switch; suspect dk_windows_close rexx_send self-block), FLAT-OWN-PATCH (own flat-UI patch, spike running).
- Next A1200 redeploy: Amarchy, amarchy-gui, amarchy-menu after the bar-click and Backspace fixes; backups .bak-20261029c. Owner hand tests after it: bar clicks open panels; hold Backspace to empty query keeps menu open, one more press goes up/closes; theme list highlight recolours shells and tabs, Esc restores; selected label readable on catppuccin-latte, ethereal, everforest, flexoki-light; super+shift+t and menu "Theme..." work; desk theme changes WB colours; tray click runs its program; new console opens in the theme.
- 2d31b53c FLAT-OWN-PATCH spike doc thoughts/shared/research/2026-10-29-amarchy-flat-ui-patch.md: feasible in layers (P1 pens, P2 class dispatchers, P3 gadtools, P4 shape if patchable). Owner chose rig measurement first (section 8, agent running at handoff time).
- 5b1398be Backspace rework: GUI uses IEQUALIFIER_REPEAT, no double-tap penalty; console keeps 0.4 s guard.
- a3ac2be0 BAR-CLICK-BACKDROP fixed: bar plain window again, bar_is_desktop() decides raise. Repro needs `--no-dopus`. Open: right_launch FAIL, behind/two_rows order dependence, WB drawer check (agent running at handoff time).

## Added 2026-10-30

- 5ab6d787, 067d3879/60e08dba, 5fc0ce75: flat-UI plan restructured (pens first, patch only the measured residue).
- 47f2d53b MENU-SEARCH-DEEP: typing at the root lists every leaf with its path. Owner check on the A1200: type 'reboot', expect System/Reboot.
- b40604b0 Fable review of the flat plan; 4aa4aa99 owner OD answers (OD-F1 b, F3 both 3.1 and 3.2, F4 a, F5 a, F6 c, F7 a, F8 a, F9 runtime patch); 73d330fa FLAT-D1 DROPPED, FLAT-D4 runtime DOpus glyph patch (version- and checksum-gated, inert on unknown builds). Flat UI is PLANS ONLY until the owner says go.
- f3e87c02 REXX-DOPUS: rexx_send bounded at 10 s with reap; rig test tools/rig/rexxbound_rig.py PASS twice, old code FAIL. Not proven to be the A1200 hang cause.
- 86745a7f winprobe (tests/amiga/winprobe.c). BAR-ZOOMED-DRAWER not reproduced on rig 6 3.1: zoomed full-width window to front gives raises 0 -> 1. Likely stale pre-a3ac2be0 binary. Owner names drawer + zoom action if it recurs after the redeploy.
- 7f2edbcf wasabi pinned to caf038f (spotUP/wasabi mui-group-buttons) for musicplayer-55. Main checkout fast-forwarded to it. make wasabi NOT yet run: waits for musicplayer's I716 run on rig 2 (it needs build/wasabi host client). Keep build/rig2/boot/wasabid-i713e until musicplayer deletes it.
- AM-BAR-COLUMNS (owner): status bar columns too close, items jump with text length, text past right edge. Fix: fixed-width slots per segment, minimum gap, clip at right edge (amarchy/bar_core.c layout, bar.c draw), extend the 550e8f79 width sweep test.
- RIG-PORT-CLASH: rig 2 emulator held 1241 (rig 8's wasabi port). RIG-OS32-NODOPUS: --os 32 ignores --no-dopus (rig.py ~1548).
- Disk: MusicPlayer scratchpads deleted with approval (1.1 -> 3.0 GiB). UpRough scratchpad (1.5 GiB, /private/tmp/claude-501/-Users-spot-Code-UpRough-Up-Rough-Demo-System/eaea0c13-...) refused by the classifier: owner deletes by hand.
- dopus5 reference clone ~/Code/Reference/dopus5 (96ede09).
- A1200 redeploy from 7f2edbcf (Amarchy, amarchy-gui, amarchy-menu, backups .bak-20261029c): see ledger row.

## Next session, in order

1. Owner hand tests on the A1200 (needs Amarchy restart or reboot to load the 7f2edbcf build): bar clicks open panels; hold Backspace to empty the query, menu stays open; theme list highlight recolours shells and tabs, Esc restores; selected label readable on catppuccin-latte, ethereal, everforest, flexoki-light; super+shift+t and menu "Theme..." work; desk theme changes WB colours; tray click runs its program; new console opens in the theme; root search "reboot" finds System/Reboot; bar stays over a zoomed drawer. Ask for results; ledger each.
2. make wasabi in the main checkout once musicplayer-55 says its I716 rig 2 run ended; tell musicplayer-55 when built.
3. AM-BAR-COLUMNS fix (ledger row).
4. Add amarchy-menu to deploy.list.
5. Rest of the open list above: R2.16, R2.17, DESK-31-CRASH, DESK-IPREFS-FLAKE, AM-FLAT-WB, Selecta P1B-M1, W43, p3_keys, RIG3-BOOT-GURU, RIG-PORT-CLASH, RIG-OS32-NODOPUS, wasabid rename, mkmanifest, A1200 drift on vtcon-handler/pty-handler/ixkill.

Gotcha: the wasabi client said "bad key" with AMI_TOKEN in the env; pass --host 192.168.0.74 --port 1234 --key <key> explicitly. The A1200 has no Md5 command: verify by wasabi get plus a local md5.
