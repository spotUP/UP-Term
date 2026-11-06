# Font fallback rig proof (s61), origin/main e10a39e2 (contains 52d2fc3a), rig 163, os32, 060, --mmu (JIT off), no Amarchy
Method: vsh script in an XCON: window samples `VTC:ixwinch 1` (cols x rows, TIOCGWINSZ) into RAM:<w>.<n> every 25 s; host edits ENV:up-term/{up-term,theme-active} between samples (watcher live path). Window 640x200. No dialog or requester appeared (`ami.py requesters` empty after each window).
Note: a window spec with spaces needs quotes in Run: `<>"XCON:0/30/640/200/t/FONT topaz 8"`, else AmigaDOS drops the FONT args (first B/C/E run all gave 77x17).
1. Theme font live, then removed: baseline 77x23 (topaz 8); theme-active `font = topaz.font:11` -> 77x17; theme-active rewritten without font line -> 77x23. PASS.
   (ruby.font in this FONTS: is proportional (FPF_PROPORTIONAL rejected in vtwin_set_font, render/vtwin.c:814), so ruby/garnet/emerald never apply: topaz 11 used as the theme font instead.)
2. Profile switch: ENV:up-term/up-term default `font = topaz.font:11` -> 77x17; rewritten with no font (theme without font still active) -> 77x23. PASS. Via the watcher's profile_switch, not the Settings menu.
3. `"XCON:.../FONT topaz 8"` with theme font topaz 11 set: 77x23 (control window without spec: 77x17). PASS.
4. `/FONT courier 13` with theme topaz 11 set: 88x14. PASS. `/FONT ruby 12`, `ruby 8`, `garnet 16`, `emerald 17`: 77x23 (built-in; fonts not accepted as fixed width, or sizes absent) -- not a fallback bug, ruby proportional.
Found: deleting ENV:up-term/theme-active leaves the theme font on the window (m->theme is null on delete, watch_take keeps the old theme_text): run 1, topaz 11 stayed 77x17 after Delete.
