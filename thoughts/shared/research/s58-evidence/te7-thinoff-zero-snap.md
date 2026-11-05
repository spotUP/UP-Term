# TE-7-BAR-THINOFF-ZERO-SNAP evidence

Fix: thin off restores only when desk_icontrol_is_ours(cur, snap): cur == desk_icontrol(snap, geometry_of(cur)) byte for byte and cur != snap.
desk_icontrol_decide / desk_start_icontrol take (snap, snaplen) instead of have_snap; desk_dos.c reads the snapshot at both call sites (dk_read_icontrol_snapshot replaces dk_have_icontrol_snapshot). Snapshot never deleted or moved; dk_icontrol_restore and dk_restore_targets untouched.

Runs (make test ONLY=...):
- RED (predicate temporarily back to "geometry bytes not all zero"): desk_start_apply 76 checks, 2 failed
  - test_desk.c:1968 zero-geometry thin write over user file: got 0 (NONE), want RESTORE (defect A)
  - test_desk.c:1970 live == snapshot with non-zero user geometry: got RESTORE, want NONE (defect B)
- GREEN (fix): desk_start_apply 76 checks, 0 failed; desk 2311 checks, 0 failed
- REVERT proof: the red run is the revert (same predicate edit, done by hand, no stash); fix copied back afterwards.
- m68k: make build/amiga/Amarchy build/amiga/amarchy-menu build/amiga/amarchy-gui rc=0 (these three link desk_dos.c; amarchy-flat does not).
