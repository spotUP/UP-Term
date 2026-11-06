# TE-9.1 theme editor reachability rig: NOT GREEN (stopped at the tool-call budget)

Rig 170 (os32 060 --max), worktree vtcon-te10-8, branch te9-1, build = HEAD 6ed6aa65 (sentinel commit a03b3c84). Script: tools/rig/amarchy_theme_editor_rig.py --case te91.

## Final step list (what the script does, row labels as the product shows them)
1. Menu open, Settings, Theme Editor (Advanced mode; Mode row cycles Simple to Advanced).
2. Child 4 "Edit Theme..." then prompt "theme name" = tokyo-night (editor must be open: Save, Save As, Duplicate, Apply are GTP_SKIP until then, gthemes_core.c).
3. Reopen menu, Theme Editor child 16 "Presets and Files", row "Duplicate Theme...", prompt "new theme name" = te-test.
4. Reopen, child 4 "Edit Theme...", prompt te-test.
5. Reopen, child 8 "Apply to Desktop"; read AMARCHY_EDIT_APPLIES (before).
6. Reopen, child 9 "Swatches", row background, "Custom...", prompt RRGGBB = ff8800.
7. Reopen, child 5 "Save"; read sentinel (after); measure title strip from a grab.

## Last run (run5): reached step 5, then the Amiga daemon stopped answering for 150 s ("auth failed ... frozen"); script killed.
No [OK]/[FAIL] assert lines from run5 (finish() never ran). Run4 (before the raw-key fix) printed all four as [FAIL] because the editor never opened (tokyo-night typed without the hyphen).
Progress in run5: copy present: True after Duplicate (file created). sentinel before: None after Apply to Desktop.

## Product findings
- Duplicate Theme..., Save, Save As..., Apply to Desktop do nothing until Edit Theme... opened an editor (GTP_SKIP, gthemes_core.c:1758). The brief's order (Duplicate first) is impossible; Edit first.
- ENV:AMARCHY_ROWS is cut at 255 chars (gui.c sentinel_rows, char b[256]): Theme Editor rows from Shape on (Presets and Files, Screens) cannot be found by label. Rig reaches them by index and checks AMARCHY_SEL (16 = Presets and Files).
- The prompt drops '-' typed through ami._type; raw key 0x0B works.
- Edit Theme... and file actions close the menu (GTP_LEAVE), so every step reopens the menu.
- amarchy_gui_rig.py --setup rc=1 "flat prefs off, amarchy-flat BOOT left nothing resident" (also first run; not hunted).
- AMARCHY_EDIT_APPLIES read back None after Apply to Desktop in run5: either Apply was skipped (editor not open for te-test) or the sentinel did not publish. Open. Not yet known whether the whole-file text lacks "amarchy-theme = 2" in the copy (file read not reached).
- Swatch that drives the active title bar: not determined; first run measured strip #1a1b26 = background.
