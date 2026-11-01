---
date: 2026-11-01
session: amarchy 28
topic: FLAT-ON wild-call hunt, wasabi upstream PRs, own ports kit worktree, Installer bottom strip
status: handoff
---

# Amarchy session 28: state at end

Ledger: thoughts/spot/todos.md (s28 rows). Evidence: thoughts/shared/research/s28-evidence/
(on branch s28-crash-hunt; only side-worktrees.md and rig9-flat-gap.md sit uncommitted in main).

## Latest state (2026-11-01 late, read this first; supersedes "Open" item 0)

Session 29 (2026-11-01), current state:
- LANDED on origin/main: BAR-FONT-MISMATCH (6342584d, 02723ad5, A1200 evidence d45e87cc), ICONS-SKIP-NAMES (e25d5fb2, 0ac3482c, b3bbc2fd: ia_skip_note + tests/test_icons_skip_glue.py, ONLY=icons_skip), FLAT-PENS47 (ca98536c fix, c0ef8042 evidence: holder checks the ColorMap via is_free, follow path sets live role pens). Their worktrees and branches are removed.
- LANDED: WB-VOLUME-GAUGE-OFF 837befac (rig 12 OS 3.2: flat ON removes the gauge gadget, flat OFF restores byte-identical ENV:Sys/Workbench.prefs). ENVARC: is not written by the apply (persist step R1.10).
- Rig 8 runs main c0ef8042, flat ON, pens 4-7 theme colours; left running for the owner's look (idle-minutes 0, owner stops it). amarchy-flat runs from RAM:.
- ICON-AA-WRONG-BG is unblocked (pens47 landed); fold in ICONPEN-THEME-SWITCH-RECOLOUR.

Owner stopped new spawns for a restart. Agents possibly still running or killed by restart; check each worktree/branch first:
- AM-ICONS-SET-CHOICE: DONE 2ebe797e (Filled default, outline.dat, Flat page "Icon st_yle" cycle, RESTORE 176/176 byte-identical on rig 12). Rig folder build/rig12 left, needs owner OK. Settings window row not looked at.
- ICONPEN-MAIN-BOOT: DONE 4d9fe2d2 + 433ad263 (rig 16: boot 0 held, Amarchy start 16 refs=1, RESTORE 0). WB reopen unproven live (CloseWorkBench rc 10). build/rig16 left, needs owner OK.
- FLAT-PENS47-RESET-HUNT: cause found, evidence ab92a8c7; fix NOT landed. Blocker = holder screen identity (ip_holder_stale address+depth). See ledger row; it is queue item 0. build/rig14 left.
Unmerged + dirty worktree = only copy of that work: keep it, finish or land it. Rig folders 14/16 stay until owner OK (never delete rigs without OK; briefs must say so).

Landed this round: 946ec9f6 + 092a380a (bar-gap evidence), 2c684f70 (flat-pen5-land evidence), b23f4e22 (wasabi pin creep-ltx main b9d1812).
A1200 runs origin/main 2c684f70 Amarchy (backups *.bak-s28bar2 beside each file in UP-Term:bin).

Next queue (ledger thoughts/spot/todos.md):
0. FLAT-PENS47 fix (opus or sonnet with the written cause): holder identity first, then the follow patch, rig 14.
1. BAR-FONT-MISMATCH (implementer): bar window draws topaz 8, height/text_w use screen font (helvetica 13 on A1200) -> 6 empty rows. One font for measure + draw; host test; rig `--overscan` with 13 px font; owner looks at A1200.
2. ICON-AA-WRONG-BG after icon-style lands (SetRGB32 ramp 32-47 live per theme switch, blend to backdrop RGB; reuse ip_hold).
3. AMIBERRY-RIGS (message dctelnet-17 when ready), haiku check s28-f11-32.png, AM-ICONS-DISK-ALL, F11-31, icon colour hunt, rig 8 restart (owner OK), WASABI-SHORTHELP-MUI3.

## Done

- FLAT-BUTTON-FILL on main 86074610 (earlier in s28; host test in tests/test_flat_core.c).
- INSTALLER-RIG-DH0 e2efd04b: `sys_free_bytes` accepts `DH0` and `DH0:`
  (tests/test_installer_rig_free.py, ONLY=rig_installer_free).
- Wasabi upstream PRs open (author spotUP): creep-ltx/wasabi #2 (MUI button tooltip labels),
  #3 (Selecta lists, stacked on #2), #4 (UIACT MutualExclude). Peer duplicates closed.
- Side worktrees: 6 removed, branches deleted with owner OK (hashes thoughts/spot/deleted-branches-s28.txt).
- FLAT-TITLE-ACCENT c036836a: JAM2 title Text repainted BgPen 0 over the band; flat_title.bg_pen
  now set/restored around it (tests/test_flat_core.c, revert fails at :1034). Rig 11 band PASS.
  Inactive dim pen 5 correct, RGB unverified until FLAT-PENS-NOT-LIVE-AGAIN is fixed.
- L7 icon pens 887a0459: amarchy-flat holds its 16 pens for its whole run (ip_hold_wanted);
  test_free_list_model in tests/test_iconpen.c. Rig 10 pens-held 16 through ON/OFF x3.
  Not run: Ctrl-D path, boot ON/OFF, on-screen icon colours. Route: s28-evidence/l7-pen-route.md.
- L6 backdrop d6c4d79a: desk_backdrop_pen() uses desk_pen_plan_for(PT_DEPTH_MAX).backdrop.
- FLAT-BUTTON-ROUND code on main 67328573 (host tests). Rig check on rig 9 did NOT exercise it:
  frame hook calls=0, dri_Pens stock after the pick (evidence 8b288a26, branch s28-button-round).
- Not defects: UTILITIES-ONLY-2 (3.2 Utilities has only Clock and MultiView), PORTS-KIT-STALE
  (guard at Makefile:1640 refused correctly).

- FLAT-PENS-NOT-LIVE-AGAIN 19888120: not a product bug; amarchy_desk_rig.pick() returned before
  desk_dos_apply finished. pick() now waits for ENV:AMARCHY_WBCLOSES (ONLY=desk_rig_pick_waits).
- FLAT-RIG-RECHECK 08a220da (rig 11 depth 8): button round corners PASS; inactive title glyphs pen 5.
- INSTALLER-FRESHALL-36 69e24c8a: base Uninstall keeps AmiSSL block by design; check expects it.
- INSTALLER-LAYOUT-BOTTOM-STRIP d6bdedaf: last page split, max 13 lines (rig 9 depth 8 PASS).
- INSTALLER-FRESHALL-10: not reproduced on rig 10 (40/40); check hardening in FRESHALL-CHECK10-STRONGER.
- FLAT-PEN5-MAGENTA cause 20ec541d (s28-evidence/flat-palette.md). Owner decision 2026-11-01: flat ON
  writes theme colours into the Workbench palette; flat OFF restores the user's saved palette.

## Open, in order

0. Agents running at compaction (check their branches first):
   - FLAT-PEN5-MAGENTA fix (opus, rig 11, branch s28-flat-palette): per-depth entry-to-pen map
     (IPrefs maps palette.prefs 4-7 to pens 252-255 at depth > 3); flat ON writes theme, OFF restores.
   - THEME-PICK-MENU-STABLE hunt (opus, rig 10, branch s28-pick-menu, s28-evidence/theme-pick-menu.md).
   - RESTORE byte-for-byte check (sonnet, rig 9, branch s28-restore-check, s28-evidence/restore-bytes.md)
     plus lucide.dat shipping check.
   Also landed this round: d21b3ae3 (fresh-all check 10 stronger), f09a7c34 + 4c8b3f10 (wasabi
   SOURCE.txt generated from tools/nas/wasabi.rev), a71134d3 (`make amiga` builds amarchy-flat/icons).
   Then: FLAT-BUTTON-ROUND-BG, FLAT-TITLE-BAND-RESIDUE (29 of 459 band px stay pen0),
   SYS:Utilities.info drawer icon, FLAT-OFF "Down to Style" timeout, flat OFF desk map check
   (may be moot after owner decision), FLAT-BACKDROP at depth 2, OWNER-ICONS-RIG.

1. **FLAT-ON-LOOP** wild `jsr -156(a6)` with bogus a6 from the tick hook chain
   (amarchy/amarchy.c:497-512; suspect tw_tick, amarchy/themewin.c:196-212; 28 KB of stack holds
   0x34340021 pattern, probable overrun). Opus hunt part 3 on rig 10, branch s28-crash-hunt
   (also carries de007867 FLAT-PREFS-NOT-NOTIFIED fix and tests/amiga/addrowner.c). When fixed:
   review, land s28-crash-hunt, cherry-pick dd5f4a9 (notes) from worktree a7572bf374e60709e and
   remove it, then land L7 glue (branch worktree-agent-a2fcac77ccd084175, 5b02d3d8) and rig-check
   pens-held 16/0/16.
2. INSTALLER-LAYOUT-BOTTOM-STRIP: done d6bdedaf. Rig builds use UPTERM_PORTS=<kit-amarchy>.
3. WASABI-PIN-UPSTREAM: after #2-#4 merge, re-pin vtcon (83acd194) and UP-Term (f59c839 on fork
   fix/uiact-subitem-mutualexclude) to creep-ltx main, then delete old fork branches.
4. (moved to item 0: RESTORE check, theme-pick hang, SYS:Utilities.info icon.)
5. Older: L11, L13, skipped-io (3 on 3.2), emboss
   rect -3 vs -1, icon border lost on reboot, TAB-FILLPEN, FLAT-SCROLLER-KNOB-SAME-AS-TRACK,
   AM-ICON-NOBORDER-31, AMARCHY-31-PALUSE-REBOOT (parked, not reproduced), DESK-RIG-FLAT-LEFTOVER,
   4-colour fallback, FLAT-MARKER-HELD.
6. Active again (owner 2026-11-01): FLAT-TITLE-ACCENT + inactive title dim (one hook), FLAT-BUTTON-ROUND.
   Deferred: window round corners only (after Amarchy is finished).

## Rig notes

- Rig 8 (3.2, main 5a15f927) left running for the owner's icon view; owner stops it.
- 3.2 via `--os 32`. Measure from the binary you built.
