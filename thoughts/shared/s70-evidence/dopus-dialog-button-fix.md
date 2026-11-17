# DOpus dialog push buttons flat: fix evidence (agent 42, branch dopus-dialog-flat)

Commits: 2c242f10 (revert of diag 839b4526), aa75bfcc (fix).

Discriminator (dopus5 5.104, rig 68): gadget of the window with exact bevel box; push button = custom (type&7==5), Activation
RELVERIFY set, TOGGLESELECT (0x100) and IMMEDIATE (0x2) clear. List Format check boxes: act 0x101. Scroller arrows: act 0.
Pure: flat_dopus_push_gadget, flat_dopus_fill_match (flat_core.c, host tested).
Interior RectFill measured: box 160,71 47x17 -> fill 161,72..205,86 (inset 1 px); match accepts inset 0..2.
Mechanism: bevel records pending slot (dpb) instead of the outline; flat_rf_c turns the matching interior fill into bb_draw over the
full box, then bb_done. Roles from push buttons of the window.

Tests: make flat-budget OK (flat_rf_c chain 320, headroom 64; first try 356/336 failed, fixed by keeping the draw in flat_rf_c and a
static box); make test ONLY=flat_core 4224 checks 0 failed. Regression: new checks call functions absent on 2c242f10 (does not link).
Rig 68 census: MakeDir Cancel box 483..530 x 325..342: 3e4254 438 px, no 7aa2f7 outline; other buttons 7aa2f7 fill 510 px;
List Format bottom buttons rows 406..417 filled 143/210 px wide runs, shrinking on the last rows (rounded); bhits 3 (MakeDir), +9
(List Format); check box box still outline (63 px 7aa2f7). Unload (Break C: flat=absent) and Run (flat=on) work; 5 NewLister ok.
Unverified: pressed look, label pixel colours, other dopus5 builds, Environment window, lister toolbar, stale slot if a button gets no interior fill.
Pitfall: ami.py put with a relative path resolves in tools/rig; use absolute path.

## agent 43: pending slot fix (commit 8bf59650)

Defect: single static slot dpb released only by the matching fill; a task that never filled blocked all others. Fix: flat_dpend_tab
(4 entries, flat_core.c: put/take/drop), per-owner entry, put never refuses (own, free, oldest), non-push bevel of the same task drops.
bb_take split into bb_take + bb_setup; dopus_fill takes the entry and claims bb_task in one Forbid.
Searched flat_core.c for an existing small-table helper: none (only fixed const tables), new code follows flat_dopus_* naming.
flat-budget PASS: _flat_rf_c own frame 64, chain 320, headroom 64 (unchanged). Host: flat_core 4243 checks 0 failed.
Mutation 1 (put refuses when another owner's stale entry present): FAIL 9 failed; restored: 0 failed.
Mutation 2 (push_gadget accepts TOGGLESELECT): FAIL 1 failed; restored: 0 failed.
Binary build/amiga/amarchy-flat 229360 bytes md5 eaf2c0e269f58631e90aaecf28fa340a.
Rig 68: Break C -> flat=absent, put + Run -> flat=on OK (twice). MakeDir / List Format requester proof NOT obtained: the requester did not
open (rx 'command MakeDir' rc 0 with and without a lister on RAM:, click on Makedir button 754,225: no window, no 3e4254 pixels,
dopus bevel counter 0). Not diagnosed (out of budget). Rig left running with the new build, no requester open.

## agent 44: rig proof 8bf59650
Rig 68, purpose "s71 session: DOpus button slot fix proof". Build 229360 B md5 eaf2c0e2 as RAM:amarchy-flat.
Q1 cause of the missing requester: RIG STATE, not the build. (a) Patcher unloaded (Break C, 0 amarchy-flat processes): rexx 'command MakeDir' and a click on
Makedir 754,225 opened no requester (screenshot: none) -> not caused by the patcher. (c) fresh `rig.py reboot`, dripens pens `0 1 2 0 0 3 0 0 2 2 0 2`,
START_WRITES=3, put build, Break 13 C, Run, flat=on, NewLister, MakeDir: opens. Probable rig-state cause (unverified): DOpus on the old boot had two
listers stacked/earlier modal state; not isolated further.
Q2 after reboot with new build: MakeDir Cancel box 483..530 x 325..342: 3e4254 438 px, 7aa2f7 0 (no outline). OK/other buttons rows 324..343: 7aa2f7 589 px
(wide button) and 510 px (second button), filled, corner rows inset (rounded). bevel counter calls 0 -> 3 (bhits 3). List Format (`command Configure`):
bottom buttons filled and rounded in screenshot, bevel calls 37, bhits 12 (+9); check boxes remain outline. Break C -> flat=absent, Run -> flat=on;
5 NewLister: rig answers `exec Version`, flat calls 7 -> 40. Then 4x `command Close`, 1 NewLister: rig answers Version.
Unverified: exact lister count at end (stacked listers share one screen position, 2 intended); pressed look; only this single reboot tried.
