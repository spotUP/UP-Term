# S32 start look: Amarchy start applies none of the desktop look (rows S32-STALE-GADGETS, S31-START-PALETTE)

Date 2026-11-03, agent of session vtcon-16. Branch s32-start-look (worktree ../vtcon-wt-startlook), not merged.

## Root cause
Amarchy and `amarchy-flat BOOT` start from S:User-Startup, which on 3.x runs before C:LoadWB opens the
Workbench screen. Every screen-dependent start step uses `ipd_lock_wb_screen()` (amarchy/iconpen_dos.c:95), which
since 5145f5a5 (BOOT-31-WB-DEADLOCK) never opens a closed Workbench and returns NULL. Each step ran once and gave up:

1. amarchy/flat.c class_slot (853) -> hook_install (879) returns -100 (no class). main() printed "sysiclass hook not
   installed" to NIL: and never called rf_install / tx_install; the 1 s pen watch never retried. STATUS: draws=0,
   rectfill installed=no, text installed=no.
2. amarchy/amarchy.c:604-606 ran desk_dos_flat_pens_refresh / marker_recover / start_apply right after wbm_begin,
   not gated on the screen (only start_windows/welcome go through wb_gate). desk_dos_flat_pens_refresh returns 0 at
   `if (!before ...)` (dk_wb_screen, desk_dos.c:217) -> no theme palette, no solid backdrop, no T:amarchy-revert.

## Rig 96 read-only evidence (OS 3.1, KS 40.68 / WB 40.42)
- amarchy-flat STATUS: flat=on calls=0 chained=0; rectfill installed=no calls=0; text installed=no; clone draws=0 fallback=0.
- ENV:Sys: font, WBPattern (boot copy, 314 bytes), screenmode, input, wbconfig. No Palette/pointer/icontrol prefs.
- No T:amarchy-revert. ENVARC:Amarchy/prefs header only (93 bytes).
- Sentinels AMARCHY_DESK_START_PLAN=0, AMARCHY_PENS=6, AMARCHY_ICON_RESTORED=0.
- User-Startup order: shadows-up, Run Amarchy, amarchy-index, Run amarchy-flat BOOT, ... (LoadWB after, in Startup-Sequence).

## Secondary (owner decision, not the timing bug)
pointer.prefs and icontrol.prefs are only planned with desktop-theme on (default "off", prefs/settings_def.c:263)
or flat-border-size thin (default stock, kept stock by e6a3a5bf). With a header-only prefs file they are never
written, whatever the timing. The brief's expected ENV:Sys pointer/icontrol proof conflicts with the defaults.

## Fix (branch s32-start-look)
- flat_core: flat_install_due(held, installed, last_rc) -> retry only for FLAT_ERR_NOCLASS. flat.c: install block
  moved to install_all(); the 1 s pen watch retries it while due.
- desk_core: desk_start_due(done, wb_screen_up, tick_ok). amarchy.c: desk_start_gate() runs the three start steps
  once the Workbench screen is up (now, or from a tick hook); no tick = old one-shot.
- Tests: tests/test_flat_core.c test_boot_before_workbench_retries_install, tests/test_desk.c
  test_desk_start_waits_for_workbench.

## Status at hand-back
- Commit 31ce61a5 on branch s32-start-look (worktree /Users/spot/Code/UP-Term/vtcon-wt-startlook). Not merged.
- Host: [OK] desk 2236 checks, [OK] flat_core 1344 checks. With the old logic restored: FAIL test_desk.c:1398
  (desk_start_due(0,0,1)=1) and FAIL test_flat_core.c:1417 (flat_install_due(...NOCLASS)=0). Cross-build of
  Amarchy and amarchy-flat clean (vbcc, warnings as errors).
- NOT DONE: rig 67 proof (full install, reboot, STATUS draws>0, backdrop pixel 14,14,20). Rig 67 never started; the
  call budget ran out on the slow host test builds. Next agent: build the branch in the worktree, rig 67 install, reboot.
- Expect pointer.prefs / icontrol.prefs still absent after the fix unless desktop-theme = on (defaults question).
- Open risk: Workbench windows LoadWB opens before the 1 s retry are drawn stock until they refresh.
