# AM-ICON-NOBORDER frame pixels (s31, OS 3.2, FS-UAE rig 56, main 904b0451 build)

Verdict: ON PASS (frame measured gone), OFF FAIL (frame does not come back after flat off, 28 s later).

Method: rig 56 `--max --emu fsuae --os 32`; `amarchy_gui_rig.py --setup`; RAM:amarchy-flat started with flat prefs off;
`put_flat_prefs(True)` + `pick()` (menu Style > Amarchy theme, entry desk_dos_apply) then `put_flat_prefs(False)` + `pick()`
(tools/rig/amarchy_desk_rig.py helpers). Screenshots via ami.grab (raw RGB, 640x256), saved as PNG, measured with numpy.
Icon measured: the third backdrop icon, region x22-62 y101-133; icon cluster = pixels differing from the backdrop colour.

Baseline (ENV:Sys/Workbench.prefs absent, stock frame): bbox x22-62 y101-133 = 41x33, 1283 non-backdrop px.
Flat ON: ENV:Sys/Workbench.prefs 84 bytes, WBNC body border word (offset 20) = 0001; bbox x25-59 y104-130 = 35x27, 932 px.
   Frame ring (3 px each side) = 1283 - 932 = 351 px with flat off/baseline, 0 px with flat on.
Flat OFF (after second pick): file gone; bbox x25-59 y104-130, 932 px, also 20 s later (off202 shot): ring 0 px. Frame NOT back.

Control (SYS:Prefs/Workbench FROM RAM:wbN.prefs USE, same call dk_editor makes): border word 0000 -> 1283 px / 41x33;
0001 -> 932 px / 35x27; 0000 again -> 1283 px (12 px diff from the first, noise). So the frame is measurable and USE with
border 0 does bring it back; the product's off path does not (file deleted, workbench keeps the borderless state).
First pair of runs (no baseline) gave the same 932 / 932.

Open: why off leaves the frame: desk_wbborder(flat=0, ...) either did not USE border 0 or the later delete of ENV:Sys/Workbench.prefs
undid it before workbench.library re-read. Not diagnosed (no code edits in this task).
Screenshots: ~/Desktop/upterm-shots/s31-noborder-{base,on,off}.png

## Re-proof after 2ae68838

Verdict: FAIL. Flat OFF still does not bring the icon frame back. Build: 2ae68838 sources, amarchy-menu/Amarchy/amarchy-gui/amarchy-flat
rebuilt 14:30 (amarchy-menu was stale before; `make build/amiga/amarchy` is a different target). Rig 56, OS 3.2, FS-UAE, `--setup`, same helpers (put_flat_prefs + pick()).
Region x22-62 y101-133, non-backdrop pixels:
- Baseline (ENV:Sys/Workbench.prefs absent): bbox x22-62 y102-133 = 41x32, 990 px against a white reference pixel (the helper reference was a frame pixel; a mode-colour count is unreliable on the patterned backdrop). Frame present.
- Flat ON: file 84 bytes, WBNC body present (border word read as 0001 earlier method; not re-decoded); bbox x25-59 y104-130 = 35x27, 932 px.
- Flat OFF right after pick: ENV:Sys/Workbench.prefs absent (expected); bbox x25-59 y104-130 = 35x27, 932 px.
- Flat OFF 20 s later: file absent; 35x27, 932 px. Frame ring still 0 px.
OFF equals ON in count and bbox: the stock-prefs USE before the delete did not bring the frame back.
Screenshots: ~/Desktop/upterm-shots/s31-noborder-fix-{base,on,off,off20}.png

## Root cause and fix

Commit 68d939ac. Fix 2ae68838 made dk_wb_absent USE the stock Workbench.prefs before it deleted ENV:Sys/Workbench.prefs, but the delete still came too soon.

Evidence (wasabi snoop on rig 56, OS 3.2, OFF pick with the 2ae68838 build):

```
14:40:23.342 SYS:Prefs/Workbench RunCommand("FROM T:amarchy-desk/workbench.prefs USE") = rc 0
14:40:23.508 VTCX:Apps/bin/amarchy-gui DeleteFile("ENV:Sys/Workbench.prefs") = ok
14:40:25.023 « IPrefs » Open("ENV:Sys/workbench.prefs", read) = fail (Error 205: Object not found)
```

IPrefs reads the file on its change notice 1.5 to 7 s after the USE (the ON read came 6.7 s later). Amarchy deleted the file 0.17 s after the USE, so workbench.library never got the stock border setting and the icons stayed frameless. In addition, dk_orig_off ran dk_env_fix_file(DK_ORIG, DESK_WBPREFS_ENV) with an absent original, which would also delete the ENV file.

Fix (amarchy/desk_dos.c):
- dk_wb_absent no longer deletes ENV:Sys/Workbench.prefs. The stock file written by the editor USE stays in ENV: for IPrefs. ENVARC: carries the persistent "absent" state, because dk_orig_restore_arc deletes the ENVARC copy.
- dk_orig_off runs the final env fix only when an original file exists (`wbfix = dk_exists(name)`), not for an absent original.

Test: tests/test_wb_absent_restore.py (`make test ONLY=wb_absent`), 4 tests. Against the old desk_dos.c: 3 of 4 fail. With the fix: 4 of 4 pass. `make test ONLY=desk`: 2207 checks, 0 failed.

Pixels on rig 56 (OS 3.2, FS-UAE, icon region x22-62 y101-133, ink bounding box and pixel count):

| Run | base | ON | OFF | OFF +20 s |
|---|---|---|---|---|
| before fix (old build) | 41x32, 990 | 35x27, 932 | 35x27, 932 | 35x27, 932 |
| run 2, user had no Workbench.prefs | (frameless leftover) | 35x27, 932 | 41x33, 991 | 41x33, 991 |
| run 3, clean cycle | 41x33, 991 | 35x27, 932 | 41x33, 991 | 41x33, 991 |

Run 2: the ENV file stays 84 bytes with WBNC border word 0000 (ON shows 0001); `14:44:46.395 « IPrefs » Open("ENV:Sys/workbench.prefs", read) = ok`; ENVARC:Sys/Workbench.prefs stayed absent. Screenshots: ~/Desktop/upterm-shots/s31-frame-rc-{base,on,off,off20}.png (run 3).

Notes:
- Run 3 started with a stock ENV file and no ENVARC file, so the ON apply saved the ENV copy as the original (OD14) and OFF restored it; ENVARC then held an 84-byte stock file, written about 23 s after OFF (writer not traced, likely the confirm step). Same effect as stock.
- After a revert of an apply whose original was absent, dk_orig_drop_reverted no longer drops the .absent original, because ENV now holds the stock file. Harmless: a later "off" re-USEs the stock file once.
