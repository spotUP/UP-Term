# S83 await verify (rig 21, patina-gui c4603fdd, 14:12 build)
Verdict: PASS (picker). Method: patina_theme_save_rig.py --selects 1-1 (drawn menu Style > Patina theme, not DOpus Style menu; no flat toggle done).
Sentinels before -> after: WBCLOSES unset->1, DEFERRED unset->1, DRAWS 1->9, CONFIRM_KEEPS unset->1, TEARS 0->0. Theme tokyo-night -> vantablack.
Script checks passed: menu window present after apply, theme list recoloured, PATINA_THEME, header accent, (current) row. Script FAILed only at final wb_pens_check (Workbench colours 0-3 read empty; WBPENS=1 WBREFUSED=0) - separate issue, not the picker.
No IPrefs reset requester seen. Keep requester answered by script.
Screenshot after reopening the theme list: fully drawn, vantablack (current) highlighted. Screenshots deleted. Logs: save.log, save2.log here (small).
Rig 21 restarted with purpose "S83 session: eye check A theme pick, gui await fix", hold 4h, menu open on theme list.
