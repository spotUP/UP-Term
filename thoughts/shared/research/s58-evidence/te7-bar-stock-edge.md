---
date: 2026-11-05
topic: TE-7-BAR-STOCK-EDGE thin + title bar height 11 writes all-zero GUI geometry
verdict: confirmed by host test, fixed in desk_icontrol_decide
---
# Analysis (written before the product edit)

Measured before the fix (tests/test_desk.c, test_thin_with_stock_bar_height_does_not_rewrite_or_resnapshot):
  FAIL tests/test_desk.c:1909: desk_start_icontrol(all, &t, w, wn, 1, &snap) = 1, want 0
  FAIL tests/test_desk.c:1910: snap = 1, want 0
  FAIL tests/test_desk.c:1915: snap = 1, want 0
  [FAIL] desk_start_apply: 61 checks, 3 failed

Violated invariants: (1) a file that already holds the wanted geometry is not rewritten; (2) the saved pre-thin snapshot is never
overwritten by a later apply. Cause: icontrol_ours() infers "written by Amarchy" from "geometry non-zero"; thin + bar 11 wants
geometry {0,0,0,0} (= stock), so our own file reads as foreign, the decision is WRITE + snapshot=1 on every start.

Searched: icontrol_ours / desk_icontrol_decide callers (desk_dos.c:406 only), desk_icontrol writer (sets ICB_VERSIONED, version 2,
geometry; no other marker), NDK 3.2r4 prefs/icontrol.h (IControlPrefs: ic_Reserved "System reserved", ic_Pad, no free field).

Levels:
1. "ours" marker in the file (ic_Pad or reserved bytes): needs a format decision not verifiable from the NDK (IPrefs may read or
   reject them; header calls reserved "System reserved"). Also does not help files already written. Rejected.
2. Geometry source (avoid bar 11 -> zero): cannot, 0 is the correct byte for 11.
3. Decision function (root: the decision's own inputs are the file geometry, the wanted geometry and the existing snapshot):
   - file geometry == wanted geometry: nothing to change (NONE), regardless of who wrote it. Covers a stock file with wanted==stock.
   - snapshot only when no snapshot exists yet (have_snap). A snapshot is the user's original file; ownership of the current
     file cannot overwrite it. Chosen: no format change, no caller special case, one function.
Not covered: thin off with a zero-geometry file of ours and a leftover snapshot is not restored (indistinguishable from stock;
the geometry is already stock, other fields of the pre-thin file are not restored). Unverified on a rig.

# Results
Before fix: [FAIL] desk_start_apply: 61 checks, 3 failed (lines above).
After fix:  [OK] desk_start_apply: 63 checks, 0 failed
Fix reverted by patch: [FAIL] desk_start_apply: 63 checks, 5 failed
Fix restored, ONLY=desk: [OK] desk: 2311 checks, 0 failed
m68k: make build/amiga/Amarchy build/amiga/amarchy-gui build/amiga/amarchy-menu rc=0 (first try rc=137 killed, rerun rc=0)
Behaviour change: a stock file with thin on and bar height 11 now plans nothing (was WRITE + snapshot every start).
