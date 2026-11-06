# TE-7b.2-HANDLER rig proof (s60, rig 95, 060, --os 32, branch s60-te7b2h ff8358ba, handler md5 61fa7377)
Sentinel: CSI 18t reply (rows;cols) read into RAM:ans from an Execute script in the XCON: window.
Files: ENV:up-term/up-term = [profile default] font = topaz.font:8; theme-active = [profile amarchy] fg bg + font line.
- Open tab, profile font topaz 8, window 640x256: 30 rows x 77 cols.
- Theme font = topaz.font:11 (put theme-active, notify path): 22 rows x 77 cols. PASS (live, grid follows).
- Theme font = topaz.font:9: 27 rows x 61 cols. PASS.
- Theme without font line: 30 x 77 again (profile font back). PASS.
- New window 600x200 opened while theme = topaz.font:11: 17 rows x 72 cols (11px cells). PASS.
- ruby.font:15 and garnet.font:16 as theme font: no change (font not accepted as fixed width by vtwin_set_font; DisplayBeep path, window keeps its font). Not a handler-wiring fault; same path as profile fonts.
- Not rig-tested: window-spec FONT / slash /font beating the theme (font_spec flag; code only).
- Known gap: theme font set, then theme without font AND profile without font: window keeps the theme font (no built-in font to fall back to).
- Host: make test ONLY=upconf 251 checks 0 failed; make amiga rc 0.
