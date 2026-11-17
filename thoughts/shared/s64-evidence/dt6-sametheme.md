# DT-6: why the toolbar rig applied "white" twice

Verdict: hypothesis (1), by exclusion and mechanism. NOT reproduced on a fresh rig 64 (os 32, DOpus 5.104, 640x256); (2) and (3) are refuted.

## Mechanism
- "(current)" is the saved theme: menu.c:361-363 / gthemes_dos.c:280-293 call gt_dos_current (gthemes_dos.c:82-92 = ap->val[AM_GTHEME]) -> gt_label (gthemes_core.c:399-401).
- AM_GTHEME is persisted only when the Keep/Revert requester is answered Keep (desk_dos.c:1332 "Keep: prefs persisted to ENVARC now, not before"); an unanswered requester reverts after 15 s (desk_confirm_dos.c:37-41; FP-1).
- Rig: amarchy_desk_rig.py:200-242 d.pick() starts keep_requester in a daemon thread (l.228-231), `kept` is a list the function never reads (l.229, return l.242). A missed Keep click is silent. keep_requester (amarchy_theme_save_rig.py:112-140) returns False without clicking when `settled` (apply_settled, l.95-100) is true and no window is seen, and swallows every ui_click exception (l.136-139).
- A missed Keep reverts the desktop (and AM_GTHEME) to vantablack after 15 s, but the DOpus brushes/toolbar bank are written by a separate path and stay white. That matches DT-9: marker stays on vantablack (cursor 21), second pick = row 22 = white again, CMAP bytes equal.

## Rig evidence (rig 64, today)
Toolbar rig, 2nd run: pick 1 "cursor on 20 (tokyo-night (current))" -> vantablack; pick 2 "cursor on 21 (vantablack (current))" -> white; `[OK] second theme changed the CMAP bytes`, RESULT: PASS. (1st run died earlier: "Down to Style: no stable state", menu not up right after install; unrelated.)
Probe of 3 picks (d.pick + t.keep_line):
- pick 0: cursor 22 (white (current)) -> vantablack | keep: requester seen yes at 9.4 s | Keep returned True at 9.4 s | requester gone yes
- pick 1: cursor 21 (vantablack (current)) -> white | requester seen 10.3 s | Keep True 10.3 s | gone yes
- pick 2: cursor 22 (white (current)) -> vantablack | seen 12.2 s | Keep True at 12.6 s | gone yes
The marker followed every pick: Amarchy updates "current" correctly when Keep lands. (My ENV:Amarchy/gtheme read was the wrong path; the marker in the list is the proof.)
Not proven: the dt9 run's own Keep log was never printed, so the missed click on rig 63 is inferred. Likely cause there: rig 63 was shared/in use by the owner's eye-check window, or the requester timing (seen 9-12 s into the pick) raced `settled`.

## Recommended fix: rig helper layer, not Amarchy
- d.pick() (amarchy_desk_rig.py:228-242) must check `kept` after kt.join and raise (or print t.keep_line(name)) when keep=True and Keep was not clicked while the apply asked one. Also assert after the pick that the list's "(current)" row (t.env AMARCHY_ROWS) names the picked theme; fail loudly, never silently re-pick the same row.
- keep_requester: do not return False on `settled and not requester_open()` before PENDING/APPLY_DONE has been seen once, and log swallowed ui_click errors.
- Amarchy itself: no change needed (no product bug found).
