# Rig 96 reinstall from main 952d2457 (session 32)

Install: installer_rig.py ONLY fresh-all FORCE, [PASS]. Two reboots after install (second via rig.py reboot, ami.py reboot timed out at host load ~125).
Measured after second reboot (2026-11-03 ~01:12):
- a: ENVARC:Amarchy/prefs holds only the header line. PASS (header only).
- b: root backdrop pixel (700,400) and (20,300) = (170,170,170), not (14,14,20). FAIL. (14,14,20) only inside the vtcon terminal window.
- c: md5 UP-Term:bin/amarchy-flat = build/amiga = 2b63f8124cbb2444173993827f09bf6f. STATUS after both reboots: flat=on calls=0, rectfill installed=no, text installed=no, clone draws=0 fallback=0, frame=absent. FAIL (draws=0, patches not installed). Not fixed, per brief. Gadget glyph pixel compare not done.
- d: ENV:Sys has font.prefs (01:10:29 boot copy), no pointer.prefs, no icontrol.prefs. FAIL / unconfirmed that desk_dos_start_apply wrote them.
- e: no wasabi block in S:User-Startup; Status FULL shows 2 BOOTX:wasabid processes (stk 16384 and 262144). Block PASS, single process FAIL/unclear.
- f: window "Welcome to Amarchy" 503x363 open and active after boot. PASS (title read from uitree; text content not read).
- g: L:vtcon-handler md5 47c684cb04eb77987535418d0de29a90 = build/amiga; $VER 952d2457_dirty. PASS.
- h: SolidWin block in S:User-Startup, C:SolidWin present (7136 bytes), process running. Drag test NOT done (unverified).
- i: Workbench menu not read as text (menus call gave no Backdrop line). Root window W3 800x573 at (0,14), empty title, suggests backdrop on. State file candidate ENV:Sys/wbconfig.prefs (54 bytes). Unverified.
Screenshots: ~/Desktop/upterm-shots/s32-rig96-1.png, s32-rig96-2.png
