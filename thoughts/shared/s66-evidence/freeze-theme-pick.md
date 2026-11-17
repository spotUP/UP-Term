# FREEZE theme pick (s66) - trigger PROVEN, wedge site in desk_dos_revert UNPROVEN

Rig 66, KS 3.2 (--os 32 --max), 640x256, font.prefs deleted (topaz 8), build at a8ab857d (amarchy-gui/amarchy-flat rebuilt,
Amarchy 8 Nov 23:47), `amarchy_gui_rig.py --setup --no-build`, `put_flat_prefs(True)`.

## Run 1: pick(keep=False) -> WEDGE (reproduced)
Host frames every ~1.8 s (screencapture -l), wasabi ping (`Echo ok`) every 2 s after pick returned. Times from pick() start:
- 17.6-19.6 s palette goes vantablack (apply).
- 27.4-29.2 s confirm requester "Amarchy desktop theme" ON SCREEN, drawn over the still-open Amarchy menu window
  (freeze-f14-requester-over-menu.png). So the requester is not "never shown": it opens before pick() returns.
- 31.0 s pick() returns (wait_apply_done), then `g.close_menu()` sends ESC. The requester is the active window, so the
  ESC answers it: on 3.2 ESC = the rightmost button = **Revert** (DC_REVERT_USER).
- 32.9 s palette back to tokyo-night, pixel layout unchanged; 34.9 s bar gone, menu gone, plain WB title
  (freeze-f17-after-revert-wedged.png).
- Pings answered at 31.1, 33.3, 35.4 s; the ping sent at 37.4 s never answered. Frames f17..f37 (34.9-74 s) identical:
  guest display static. fs-uae.log: no wild exception (only the boot FPU line), host fs-uae ~53 % CPU (rig 3/71 ~90 %):
  looks like everything Wait()ing, not a CPU loop / no guru / no reset.

## Run 2 (cold restart, same disk): pick(keep=True) -> NO wedge
Keep clicked (AMARCHY_CONFIRM_KEEPS=1, REVERTS_USER=0, REVERTS_TIMEOUT=0); wasabi answered every 2 s for 80 s after pick.
=> The requester open/draw does NOT wedge. The wedge follows the Revert.

## Agents A/B explained
pick(keep=False) always ends with close_menu ESC -> the ESC hits the requester -> Revert. Their "requester never seen at
0-2 s" = already answered by the ESC; "silent 150 s" = the revert wedge. FREEZE is a desk_dos_revert wedge, not a
theme-pick or requester-open wedge. Harness defect too: close_menu() sends ESC blind into whatever window is active.

## Code path (Revert)
desk_confirm_dos.c:61-66 act(DC_REVERT_USER): req_close() then desk_dos_revert() on the Amarchy commodity task, with the
amarchy-gui menu window still open on the Workbench screen (it was open under the requester, f14).
desk_dos.c:1438 desk_dos_revert: dk_wb_restore (:938, runs SYS:Prefs/Workbench USE via dk_editor :124 + dk_wait_iprefs
:367) -> gt_dos_load / amconf_save -> dk_restore_targets(mask) (:1376: WBPattern, Palette USE -> IPrefs Workbench reset ->
patched CloseWorkBench close_hook wbreset_patch.c:55 signals clients, waits <=3 s, calls original) -> dk_flat_sync (:607,
Forbid + amarchy-flat state write + win_refresh_frames).
Every Amarchy-side wait read is bounded (dk_wait_iprefs DK_RESET_SECS loop, close_hook WBR_WAIT_TICKS 150), so the
permanent block is outside these loops: most likely Intuition/IPrefs inside the Workbench reset (CloseWorkBench/OpenWorkBench
or LockIBase/layers held) while a client window (amarchy-gui menu) is still on the screen. Bar hidden and never reopened
(f17) = OpenWorkBench side never completed or the reopen signal never served. UNPROVEN which task holds what.

## Violated invariant (best-supported)
Apply path closes/hides the menu before its Workbench resets (THEME-PICK-MENU-STABLE: "the apply hides and reopens it");
the Revert path (desk_dos_revert) runs the same Workbench-resetting editor USEs without hiding the menu window first.
Invariant: no Workbench reset (Workbench/WBPattern/Palette USE) while an Amarchy-owned window is on the WB screen.
Unverified: whether the menu window is the blocker or the revert's dk_flat_sync/win_refresh_frames under the reset.

## Next probe (one run, decides the site)
pick(keep=False) with close_menu replaced by: wait for timeout revert (no ESC, menu still open) vs. menu closed then ESC
on requester. Plus `rig.py start --debugger` (debug fs-uae) + dbgbreak after the wedge for task list / semaphore owners,
or wasabi snoop on before the pick to log the last Intuition/DOS call of Amarchy and IPrefs.
Fix level (proposed, after probe): desk_dos_revert must take the same hide-clients step the apply takes before its editor
USEs (desk_dos.c, not the harness). Harness: close_menu must not ESC into a foreign window (amarchy_gui_rig.py:253).

## Bug 0 (requester bottom border cut off)
Shares the setting, not proven to share the cause: the requester opens ON TOP of the open Amarchy menu window
(f14, requester bottom edge over the menu's top border area). If the menu redraws after the requester opens, it may paint
over the requester's bottom rows. Unmeasured.

## s66 probe (2026-11-09): menu window is NOT the cause - hypothesis 1 REFUTED, no fix committed
Rig 66, KS 3.2 (--os 32 --max), HEAD 06fe144c (amarchy, amarchy-gui, amarchy-flat current), `amarchy_gui_rig.py --setup --no-build`,
`put_flat_prefs(True)`, font.prefs NOT deleted. Probe script: theme pick (Style > Amarchy theme > next row > Return), wait_apply_done,
then NO Keep and NO ESC: the 15 s timeout revert (DC_REVERT_TIMEOUT) runs on its own. `Echo ok` ping every ~2 s, 5 s timeout.
- A, menu window OPEN at the revert (cold boot): apply done 36.1 s; pings alive to 60.7 s; ping at 62.8 s never answered -> WEDGE.
- B, menu window CLOSED before the revert (cold restart; `Break 1 C` to amarchy-gui, Status: 1 amarchy-gui, 2 amarchy-menu,
  3 Amarchy, 7 amarchy-flat; menu gone 2 s later): apply done 31.6 s; alive to 53.8 s; ping at 55.9 s dead -> WEDGE.
Both wedge ~22-24 s after apply done (= ~7-9 s after the 15 s timeout fires, i.e. inside desk_dos_revert).
Conclusions:
1. The harness ESC (amarchy_gui_rig.py:253 close_menu) is not needed for the freeze: the timeout revert wedges too. close_menu still
   answers the requester blind (harness defect stands, not fixed here).
2. The open amarchy-gui menu window is not the blocker (B wedges with it closed). desk_dos_own_close missing in the commodity process
   is a real gap but not this freeze's cause. Note gui.c is a WBR_DISMISS wbreset client: the CloseWorkBench patch's Ctrl-F already
   dismisses it on a reset.
3. Remaining suspects inside desk_dos_revert (desk_dos.c:1438), commodity process, unproven:
   (a) dk_wb_restore (:938) runs the Workbench editor USE with the BAR still open (dk_windows_close only runs later, in
       dk_restore_targets :1396, and only when the palette is in the mask) - a Workbench-prefs reset with the bar window open is the
       RL15-IPREFS-SYSREQ shape, and the bar's owner (the commodity) is the task blocked in dk_editor, so it can't serve the
       wbreset Ctrl-F;
   (b) dk_restore_targets Palette USE -> patched CloseWorkBench (wbreset_patch.c:55) / IPrefs;
   (c) dk_flat_sync (:607) Forbid + win_refresh_frames under the reset.
Next probe (one run): same timeout revert with a wasabi snoop or sentinels around dk_wb_restore / dk_restore_targets / dk_flat_sync
(am_sentinel per step into ENV:) to see the last step reached, or debug fs-uae + task list after the wedge to see where Amarchy
(task 3) and IPrefs Wait. Candidate root fix if (a): desk_dos_revert closes our windows (dk_windows_close) before dk_wb_restore
and reopens them after dk_flat_sync, as the apply brackets its resets.

## s66 probe 2 (2026-11-09): wedge is in dk_live_pens inside dk_restore_targets - step PROVEN, lock cycle UNPROVEN, no fix committed
Rig 66, KS 3.2 (--os 32 --max), HEAD 06fe144c plus temporary step probes (removed again; desk_dos.c is back to HEAD). Each probe step
wrote AMARCHY_RV_STEP (am_sentinel) and appended "sNN" to VTCX:rvprobe. VTCX: is a host-directory drive, so the file stays readable on the
host after the guest wedges, which a SetVar into ENV: does not. Repro: `amarchy_gui_rig.setup()` --no-build, `put_flat_prefs(True)`,
then `amarchy_desk_rig.pick(keep=False)` with close_menu made a no-op (no ESC), so the 15 s timeout revert ran on its own. Picked
tokyo-night -> vantablack. Apply done at 33.0 s. The revert started at about 45 s. The last ping answered at 52.7 s, then the daemon was
silent for 150 s (frozen).
Steps on the host file, in order:
`s01 s02 s04 s10 s11 s12 s20 s30 s13 s21 s31 s14 s22 s32 s15 s23 s33 s40`
- s02: dk_wb_restore did nothing (no DK_WBPEND). **Suspect (a), Workbench USE with the bar open, is REFUTED.**
- s04 -> s10 -> s11: the palette was in the mask, so dk_windows_close ran and returned before any editor.
- s12/s20/s30 Font, s13/s21/s31 Pointer, s14/s22/s32 WBPattern, s15/s23/s33 Palette: every editor, dk_wait_reset and
  dk_env_fix_file finished. **Suspect (b), Palette USE and the CloseWorkBench patch, is REFUTED** as the place where the wedge stays.
- s40 was reached and s41 never: **the wedge is in `dk_live_pens(...)`, amarchy/desk_dos.c:1430 (call), body :692.** dk_flat_sync (c),
  dk_orig_drop_reverted and dk_marker_clear never ran.
How revert differs from apply: apply calls dk_live_pens only after dk_apply has reopened the windows and waited for IPrefs after Font
(desk_dos_apply :1218, through dk_live_want). Revert calls it inside the windows-closed bracket, straight after the Palette reset. Revert
also runs the editors in the opposite order: Font, Pointer, WBPattern, then Palette. Apply runs Palette first and Font last, and
`dk_wait_iprefs` follows Font there.
Lead mechanism (UNPROVEN, no task list taken): dk_live_pens holds a visitor lock on the Workbench screen (ipd_lock_wb_screen :697). Under
that lock it calls ipd_role_write (iconpen_dos.c:548), which takes `r->ss` and then LockPubScreenList. amarchy-flat's 1 s pens_sync
(flat.c:2109 -> ipd_owner) holds `r->ss` across LockPubScreenList. An IPrefs Workbench reset still running (or queued) that holds the
public-screen list, or waits for the visitor lock to go, closes a cycle. That would block LockPubScreenList machine-wide, which fits the
whole guest freezing (Echo pings dead) rather than Amarchy alone.
Candidate root fix (not done; it needs one rig proof): make the revert's live-pen step follow apply's order. dk_restore_targets sets
dk_live_want and dk_live_flat, and desk_dos_revert calls dk_live_pens after the windows reopen and IPrefs is idle, the same way
desk_dos_apply :1218 does. Also stop dk_live_pens holding the Workbench visitor lock across ipd_role_write. Next probe: debug fs-uae task
list at the wedge (where Amarchy, amarchy-flat and IPrefs Wait), or a probe step inside dk_live_pens before and after ipd_role_write.
Regression test home: tests/test_palette_write_closes_windows.py style (structure test on desk_dos.c, `make test ONLY=palette_closes`).

## s66 fix (2026-11-09): revert defers the live pens; dk_live_pens holds no screen lock across ipd_role_write - FIXED, rig PASS
Invariant (stated before the edit): no task holds a Workbench public-screen lock (LockPubScreen visitor) while it waits for the public
screen list (LockPubScreenList), and nothing sets live role pens inside a dk_windows_close/dk_windows_open bracket while an IPrefs
Workbench reset can still be running. Lock order in the code: ipd_role_write (amarchy/iconpen_dos.c:548) takes the holder semaphore `r->ss`,
then LockPubScreenList; amarchy-flat pens_sync (flat.c:2109 -> ipd_owner, iconpen_dos.c:406) takes them in the same order, so the
`r->ss` pair alone is NOT a cycle (the earlier lead mis-stated that). The cycle edge is dk_live_pens itself: it held the visitor lock
from ipd_lock_wb_screen across ipd_role_write's LockPubScreenList, the order the holder code forbids for the list lock (iconpen_dos.c:320
"No LockPubScreen ... under the list lock"); a Workbench reset (IPrefs -> CloseWorkBench, which waits for the visitors to go) closes it.
Root-cause level: amarchy/desk_dos.c (call order of dk_restore_targets and the lock scope of dk_live_pens), not iconpen/flat.
Cause: amarchy/desk_dos.c dk_restore_targets called dk_live_pens inside the windows-closed bracket straight after dk_wait_reset;
dk_live_pens (desk_dos.c:692) held the WB visitor lock across ipd_role_write.
Fix (both parts): (a) dk_restore_targets now only sets dk_live_want/dk_live_flat (kept when the caller already set one, so
desk_dos_apply -> dk_orig_off keeps the pick's want); desk_dos_revert clears dk_live_want first and calls dk_live_pens after
dk_restore_targets reopened the windows, before dk_rv_theme is cleared - the apply's order (desk_dos_apply). (b) dk_live_pens computes the
colours under the visitor lock, unlocks, then calls ipd_role_write; the old direct SetRGB32 (no holder) re-locks the screen around the
writes only. Tag S66-FREEZE-LIVEPENS.
Rig verdict (rig 66, KS 3.2 --os 32 --max, build of the fix): timeout Revert tokyo-night -> vantablack -> revert: guest answered every
ping to 70 s (old build froze at ~52 s), theme line back, colours 0-3 and DrawInfo pens equal to before, pens 4-7 = 3e4254 0e0e14 9ece6a
0e0e14 = the tokyo-night role pens (252-255); the "before" 6/7 were the graphics defaults 808080/606060 of the fresh boot. Icon pens after
revert dim/knob/inbar 25 27 32 / 31 35 4d / 5a 5e 75; the steady tokyo-night state before the Keep run read 25 27 32 / 2d 36 50 / 5a 5e 75
(knob differs slightly, UNVERIFIED whether that is a fault). Pick + Keep: theme vantablack kept, all colours changed, guest alive.
Scripts: scratchpad rvprobe.py (repro), rvcheck.py revert|keep (pen comparison).
Test: tests/test_revert_live_pens_deferred.py, `make test ONLY=revert_live_pens` (4 tests). Revert-check: against HEAD's desk_dos.c all 4
FAIL; with the fix all pass. `make test ONLY=palette_closes` still passes.
