# s62 shared rig run, rig 165 (os32, 060 max, fresh full install origin/main 319fd963)
Method: NewShell "XCON:0/30/640/200/thm/CLOSE" then `vsh` typed in it (cwd RAM:, RAM:tabprobe_zzz contains "x"). Keys via ami.py (Return 0x44, Tab 0x42, Esc 0x45). Profile `[profile default] bg = 204020 / fg = FFFF80` appended to ENV:up-term/up-term (original restored at end). theme-active = tokyo-night profile (bg 1A1B26). Screenshots in ~/Desktop/upterm-shots/s62-*.png.

## 1 TAB-RETURN-ACCEPT (a994d26e): PASS
a) `Type tabprobe_z` with dim ghost `zz`, Return -> console `RAM Disk: % Type tabprobe_zzz` then output `x` (shots s62-c4/c5). No "can't open".
b) `Type tabprobe_z`, ghost shown (s62-d1), Esc clears ghost (s62-d2), Return -> `TYPE can't open tabprobe_z` / `object not found` (s62-d3). PASS.
Side observation: typing `CD RAM:` + Return in a fresh vsh accepted the ghost and ran `CD RAM:rig-libs-done` (the accept works on any ghost, also an unwanted one the user did not look at).

## 2 TAB-SINGLE-INLINE: FAIL
`Type tabprobe_z` then Tab (0x42): line stays `Type tabprobe_z` + highlighted `z` + dim `z` (ghost only), identical to before Tab (s62-e1, e2); a second Tab also no change. Not inserted, no `Type tabprobe_zzz `. Beep/DisplayBeep: not observable by this rig (no audio/flash capture), unverified.

## 3 THEME-RECREATE-NO-TAKE: PASS
Pixels (PIL, 640x256 grab, window thm open throughout):
- before: (26,27,38) = 1A1B26
- after `Delete ENV:up-term/theme-active` (+4 s): (32,64,32) = 204020 profile
- after `Copy RAM:ta.bak ENV:up-term/theme-active` (+6 s): (26,27,38) = 1A1B26; `Type` shows `[profile tokyo-night] fg = A9B1D6 / bg = 1A1B26 / cursor-color = 7AA2F7 / palette = ...` (valid)
- newly opened XCON window new2 (0/60/640/150): (26,27,38) = 1A1B26.
The s61 "does not take until reboot" does not reproduce. f1host (pre-existing 600x200 window) was covered by thm and not measured separately.

## 4 Font fallback after theme-active delete: PASS
Window new2 150 px high, `RAM:ixwinch 1` (cols x rows): theme-active without font 77x17; theme-active + `font = topaz.font:11` (copy of tokyo-night + that line) -> 77x12; `Delete ENV:up-term/theme-active` -> 77x17 (back to built-in). s61 found the font stuck; fixed.

## Register row
2026-11-06 | rig 165 os32 060 max, origin/main 319fd963 fresh full install | tab Return accept (a994d26e) PASS both; Tab single-candidate inline insert FAIL (ghost only, no insert); theme-active delete/recreate takes theme again PASS (204020 -> 1A1B26, open + new window); font fallback after delete PASS (77x17 / 77x12 / 77x17) | shared-rig-run.md
