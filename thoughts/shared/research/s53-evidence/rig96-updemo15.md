# rig96 UPDemo 15 eye check (vtcon-ee s53) -- STOPPED before reinstall
- main cf831f46 includes a51c35bc and d7270e60. Host: fs-uae rigs 96/35 plus a DEViLBOX tsx job, no vtcon test run.
- kit-stage and `UPTERM_PORTS=...kit-main make dist` both rc=0.
- BUG: build/dist/up-term/Files has only a directory `updemo` (earth.anim, kanim-stream.bin, photo.png). The UPDemo PROGRAM
  (build/amiga/UPDemo, 'Copy UPDemo C:UPDemo CLONE' at dist/install.dos:607) is missing: on case-insensitive APFS the file
  Files/UPDemo and the directory Files/updemo (install.dos:610-612) are the same name; the dir won.
- Result: a fresh install would give "UPDemo: not found" again. Reinstall NOT done (needs a code change: rename the data dir
  in the dist, e.g. Files/updemo-media, or fix the dist copy order).

## 2026-11-03 retry after d7b1cc28
- make dist rc=0; tests/test_dist_updemo_names.py 5/5 OK; Files has only UPDemo (file, 51764) + Files/share/updemo/{earth.anim,kanim-stream.bin,photo.png}.
- installer_rig ONLY fresh-all (rig 96, --max --os 32, INSTALLER_KEEP=1): [PASS] twice. Host: another session ran installer_rig fresh-all FORCE at the same time (rig 35).
- FINDING (upgrade path): first install left C:UPDemo a DIRECTORY (earth.anim + UPDemo inside) from the earlier buggy install in the reused build/rig96/os-32 image; the kit's `Copy UPDemo C:UPDemo CLONE` then copied INTO that dir, so `UPDemo` was not runnable. The auto classifier denied `rig.py stop` (host folder wipe); I instead ran `Delete C:UPDemo ALL` on the rig and re-ran fresh-all. Real users who installed the buggy kit have the same stale C:UPDemo dir; install.dos should remove a directory at C:UPDemo before the Copy.
- After reinstall: C:UPDemo is a file (51764). `Version C:UPDemo` -> "Could not find version information" (no version string in UPDemo).
- `List UP-Term:share/updemo`: photo.png 46442, kanim-stream.bin 65885, earth.anim 983803 (3 files - 1070K bytes).
- vsh: `which UPDemo` -> c:UPDemo (resolves). Default window: 640x235 pixels, `stty size` = 27 rows x 77 cols ($COLUMNS=77 $LINES=27): above 76x20, no bigger window needed.
- UPDemo 15 grabs (10, in ~/Desktop/upterm-shots/rig96-updemo15-01..10.png; grabs ran slower than 0.3 s): 01-03 blank demo window (loading), 04-05 NASA photo Earth (real shaded globe with stars, not squares/coded), 06-10 had advanced to "Demo 16/30 Kitty truecolour". No partly drawn frame seen; Earth complete in 04 and 05.
- After `q` the demo closed; the banner above the prompt showed repaint garbage at the top (colour streaks in the logo) until cleared: minor, unverified cause.
- icat UP-Term:share/updemo/photo.png (rig96-updemo15-icat.png): colourful concentric-ring gradient picture, drawn whole at top-left. Type kanim-stream.bin (rig96-updemo15-kanim.png): one solid blue square, no text/garbage. The title-bar text appears blanked in both shots.
- Rig 96 left running, demo closed.
