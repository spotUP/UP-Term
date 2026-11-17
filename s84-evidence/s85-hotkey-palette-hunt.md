# S85 hotkey palette hunt (rig 84), 2026-11-15

Verdict: H1 (variant). Theme files are present (GG:Patina/Themes == VTC:Apps/Patina/Themes, vantablack.theme + starship.theme, 593 bytes),
but the assign UP-Term: was NOT defined on the rig (`Assign UP-Term: EXISTS` -> "UP-Term: not assigned"; `List UP-Term:` -> volume not mounted).
GT_SHIPPED_DIR is "UP-Term:Patina/Themes" -> unreadable -> gt_load falls back to builtin tokyo-night; ENVARC:Patina/themes does not exist either.
Consequence: the apply writes the same bytes as the current palette; dk_put skips (H2 symptom, not cause): PATINA_DESK_PUT_SKIPS 1 -> 3.
Also: the apply raised an IPrefs "System Request" (Retry|Cancel, task UP-Term:bin/patina-icons) = the missing-volume requester; Cancelled.

Run A (UP-Term: unassigned), F9 vantablack, Keep at 0.7 s:
 before: DESK_APPLY_R=15 DESK_PUT_SKIPS=1 DEFERRED=1 THEME=tokyo-night, WBPENS/WBREFUSED/DESKS/DESKMASK/WBCLOSES unset
 after+14s: WBPENS=0 WBREFUSED=0 DESKS=1 DESKMASK=15 APPLY_R=15 PUT_SKIPS=3 DEFERRED=1 WBCLOSES=2 THEME=tokyo-night
 pens rgb 1A1B26 414868 A9B1D6 ... unchanged; palette.prefs 434 B 21:37:01 unchanged.
Run B after rig-side `Assign UP-Term: VTC:Apps` (same as boot/go line 74), F9 vantablack, Keep at 1.2 s:
 after+14s: DESKS=2 DESKMASK=15 APPLY_R=15 PUT_SKIPS=4 (unchanged by this run: skipped only for 1 of the 4 desks) DEFERRED=1 WBCLOSES=3 WBPENS=0 WBREFUSED=0
 pens rgb 000000 7A7A7A BEBEBE 000000 000000 8D8D8D 000000 000000 BEBEBE BEBEBE 000000 BEBEBE = vantablack, palette.prefs rewritten 21:49:40.
 No requester in run B. F10 not run (palette changed).
Open: PATINA_THEME stays tokyo-night (terminal theme env, not the gtheme). Why UP-Term: was unassigned on rig 84 not found
(boot/go lines 72-76 should assign it; a kit-less rig whose VTC:Apps exists). Deployed Patina 412824 B to both bin paths; Patina restarted.
Rig state left: UP-Term: assigned to VTCX:Apps (rig-side, not persistent), vantablack kept, Patina running, no requesters, no screenshots.
Scripts: scratchpad dep.py, st.py, ls.py (run with UPTERM_RIG=84).
