---
date: 2026-11-06
topic: S57-PENS47-UNOWNED follow-up - why pens 4-7 read graphics stock and pens 0-3 stay on the install theme after every pick (rig 147 sequence)
verdict: not a regression of s58-pens47. The measured states are the FP-1 confirm TIMEOUT REVERT, not the pick. The pick harness never clicked Keep. The revert restores palette.prefs (pens 0-3 = previous theme) but writes pens 4-7 from desk_live_baseline (graphics stock), because pens 4-7 on a deep screen live in no snapshot file and the theme name is not part of the snapshot.
method: code reading of worktree vtcon-s58-integ (HEAD 3778082a) against origin/main, plus the rig 147 pick logs in the session scratchpad (pens47-seq/sel-*.log, seq.sh). No rig run, no host probe.
---

# Pens 4-7 stock after a theme pick: cause

## What actually happened on rig 147
Every pick log (scratchpad pens47-seq/sel-*.log, sel3.log, selB.log) reads
`[FAIL] select N: the menu closed on the pick (it must stay open and recolour) (stable-box wait 0.0 s)`.
`open_wait = 0.0` is only returned by the `except TimeoutError` branch of `one_select` (tools/rig/amarchy_theme_save_rig.py:322-327),
which returns BEFORE `wait_apply_done()` and `keep_requester()` (line 330). That branch is reached after `wait_stable(STEP_SECS=30, ...)`
timed out (line 31: STEP_SECS = 30), so at least 30 s passed after the Return that picked the theme, then seq.sh slept 6 s more before
reading pens. The FP-1 requester "Amarchy desktop theme" (amarchy/desk_confirm_dos.c:32-40) reverts after 15 s unanswered
(DC_REVERT_TIMEOUT -> desk_dos_revert, desk_confirm_dos.c:60-66). So every measured state is "after timeout revert", and
`ami.py requesters` was empty because the requester had already closed (req_close before desk_dos_revert).
The earlier rig 147 run that recorded ristretto as matching went through keep_requester (the apply was kept), which is why it matched.

## Q1. Regression of s58-pens47, or on origin/main too?
Answer: not a regression; origin/main has the same behaviour for pens 0-3 and writes the same stock values to pens 4-7 on the revert.
- Pick path: menu Return -> gt_dos_apply_saved (amarchy/gthemes_dos.c:231) -> desk_dos_apply(&th) (desk_dos.c:1124) -> dk_apply.
- Prefs after install are all defaults (header-only prefs file): desktop-theme on, all four parts on, flat-system-look on
  (amconf_core.h:28-33). So in dk_flat_pens (desk_dos.c:702): flat = 1, own = flat && !backdrop-part = 0, theme = flat && !palette-part = 0,
  restore = !flat && ... = 0. dk_flat_pens returns early (or only re-syncs) and does not touch dk_live_want.
- dk_apply writes the theme palette and sets dk_live_want = 1 (desk_dos.c:443); desk_dos_apply then calls dk_live_pens(th, flat) (1142):
  the pick itself writes the correct theme colours to pens 4-7. dk_orig_off has nothing to do (no part off). Then dk_confirm_ask opens the 15 s confirm.
- Timeout -> desk_dos_revert (desk_dos.c:1343) -> dk_restore_targets(mask, DK_REVERT) -> after the palette reset `dk_live_pens(0, 0)` (1339).
- origin/main (bbdb399f, which contains e79a4705): the same `dk_live_pens(0, 0)` in dk_restore_targets (main line 1338) and the same
  `desk_live_baseline(...)` when th == 0 (main line 638). The only difference of s58-pens47 is that the write goes through ipd_role_write
  (desk_dos.c:639), which also stores the colour as the holder's `want` (ip_role_claim, iconpen_core.c). On main the SetRGB32 is direct.
Confidence: high for "same path on main"; the main-build pens 4-7 values after a timeout revert were not measured (on main the holder's
pens_sync on a reopened screen may race the direct write and leave the new theme's 4-7 instead of stock).

## Q2. Why pens 4-7 are stock, and why the holder's want holds them
- dk_live_pens(0, 0) computes `desk_live_baseline(saved, slen, pen, depth)` (desk_dos.c:638). On an 8-plane screen palette.prefs entries
  4-7 land on the last four pens, so pens 4-7 are in no palette.prefs entry; desk_live_baseline returns its constant table
  0000ff ff00ff 00ffff ffffff (desk_core.c:344-351).
- ipd_role_write -> ip_role_claim sets `h->rrgb[i] = rgb` for each held role pen, then SetRGB32 (iconpen_dos.c ipd_role_write). That is the
  measured `want` = stock. Nothing re-derives it afterwards: desk_dos_revert ends with dk_flat_sync, no ipd_recolour, and ip_recolour would
  skip anyway until the theme changes.
Confidence: high (code), consistent with all five measured states. Unverified: the sentinel AMARCHY_CONFIRM_REVERTS_TIMEOUT was not read on
the rig (seq.sh/read.sh did not capture it); the timing argument (>= 36 s after the pick, timeout 15 s) carries this.

## Q3. Why pens 0-3 keep the install theme
dk_snapshot (desk_dos.c:73, called in dk_apply before the palette write) copies the palette.prefs in force (tokyo-night, written by the
Amarchy start plan after install) to T:amarchy-revert. The timeout revert feeds that copy to the Palette editor (dk_restore_targets,
desk_dos.c:1316-1331), IPrefs resets Workbench with it: pens 0-3 = tokyo-night again. Each later pick snapshots the reverted tokyo-night
file, so every state reads the install theme. The prefs theme line is not in the snapshot (desk_snapshot/collect lists only the Sys/ prefs:
desk_core.c:1145-1148 plus icontrol), so ENV/ENVARC keep the picked name.
Confidence: high.

## Q4. Violated invariant, level, fix
Invariant: "Revert puts the previous look back" (the requester's own text, desk_confirm_dos.c:36-37), and the Workbench colours have one
source: the theme in force. After a revert, pens 0-3 come from the restored palette.prefs (previous theme), pens 4-7 from
desk_live_baseline (no theme), and the prefs theme line, consoles, menu and icon pens 32-47 stay on the new theme. Three different looks.
Level: desk_dos.c revert (FP-1 snapshot / dk_restore_targets), not iconpen and not the s58-pens47 routing. The snapshot unit (Sys/ prefs
files) is smaller than the apply unit (theme name + files + derived live pens 4-7). Pens 4-7 on a deep screen are derived state
(desk_live_follow of the theme in force), so they can only be restored by restoring the theme they derive from.
Fix (one design): the FP-1 snapshot also records the theme name in force before the apply (next to the files in T:amarchy-revert and in
the marker dir, so desk_dos_marker_recover gets it too). desk_dos_revert puts that name back into the prefs theme line (ENV and ENVARC),
and dk_restore_targets for a revert sets the live role pens from that theme through the same gate as the holder
(desk_live_follow(prev_theme, flat, mask, depth, ...)), falling back to desk_live_baseline only when the gate is off (flat off and palette
part off: then the user's palette rules). The theme line coming back also makes the holder's later pens_sync on a reopened screen
(desk_live_follow of the current theme) agree with pens 0-3. dk_orig_off's use of dk_live_pens(0, 0) (a part turned off) stays as is:
there the user's palette does rule.
Note for the owner: reverting the theme line means a timeout also puts consoles, menu and DOpus back on the previous theme. That is what
"goes back to the previous look" says; if only the Workbench parts were meant to revert, the requester text and this design change.
Host test that fails before the fix: extract the pure decision `desk_revert_role_pens(const gtheme *prev, int flat, unsigned mask,
const unsigned char *saved, long savedlen, int depth, int pens[4], uc_u32 rgb[4])` into desk_core.c (desk_live_follow when the gate is on,
desk_live_baseline otherwise). Test in tests/test_desk.c: prev = tokyo-night, flat = 1, depth 8, saved = tokyo-night palette.prefs ->
pens 4-7 = 3e4254 0e0e14 9ece6a 0e0e14 (today's path, desk_live_baseline, gives 0000ff ff00ff 00ffff ffffff). Plus a source-shape check
in the style of test_desk.c:1499 that dk_restore_targets' revert branch calls it with the snapshot theme, and that dk_snapshot/desk_dos_revert
write and restore the theme line. Reachability: the rig pick script with Keep NOT clicked, read pens 0-7 after 20 s = previous theme on all 8.

## Also found (harness)
amarchy_theme_save_rig.py one_select returns from the TimeoutError branch without keep_requester, so any "menu closed" FAIL leaves an
unconfirmed apply that reverts 15 s later; every measurement taken after such a FAIL measures the revert, not the pick. The pick
sequence verdict in pens47-pick-sequence.md therefore says nothing about the pick path; the keep path (earlier ristretto run) matched.

## Not verified / where I looked
- No rig run: AMARCHY_CONFIRM_REVERTS_TIMEOUT / AMARCHY_DESK_CONFIRM_ASKS values on rig 147 not read.
- origin/main pens 4-7 after a timeout revert not measured (holder pens_sync race, see Q1).
- Read: desk_dos.c 73-88, 384-496, 600-791, 1100-1160, 1298-1424, 1570-1660; desk_core.c 336-372, 841-848, 1145-1208; gthemes_dos.c
  128-246; desk_confirm_dos.c 30-70; amconf_core.h 26-40; branch diff origin/main...s58-pens47 (desk_dos.c, iconpen_core.c, iconpen_dos.c);
  amarchy_theme_save_rig.py 30-31, 93-108, 300-345.
