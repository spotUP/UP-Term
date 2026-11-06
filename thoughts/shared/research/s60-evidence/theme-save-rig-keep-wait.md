# THEME-SAVE-RIG-KEEP-SECS evidence (host only, no rig run)

Signal: keep_requester(secs, t0, settled) ends False early when `apply_settled()` is true and no requester window exists.
apply_settled (tools/rig/amarchy_theme_save_rig.py, defined above REQUESTER) = ENV AMARCHY_WBCLOSES written (lf_gt_select
writes it after desk_dos_apply returned; deleted before the pick) AND `T:amarchy-revert/pending` absent. desk_dos_confirm_ask
(amarchy/desk_dos.c:1235-1243) writes DK_PENDING inside the apply, before the requester opens, so a finished apply with no
marker is a pick that asks nothing. Bound: KEEP_SECS = 60, then False (callers print the keep line, which says requester not seen).
Callers: amarchy_theme_save_rig.one_select (passes apply_settled), amarchy_desk_rig.pick thread (passes it; APPLY_DONE is deleted
before its pick too), amarchy_confirm_rig (keep_requester(1), no settled, unchanged), one_select's keep_requester(2) calls (unchanged).
RED (unfixed harness): [FAIL] requester at 15 s: Keep not clicked (kept=False clicks=[] at 12.0 s)
GREEN: [OK] test_amarchy_keep_wait; desk_rig_pick_waits and one_select_level still OK.
Rig run must show: first pick on a fresh rig prints `keep ...: requester seen N s` with N above 12, Keep returned True, pick kept.
