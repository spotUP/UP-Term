---
date: 2026-11-02
topic: OS 3.2 glyph clone under the Amarchy desktop with a fresh resident amarchy-flat
status: not reproduced (clone works)
---

# Glyph clone under the Amarchy desktop on 3.2

- Rig 55, fs-uae, `--os 32`: exec.library 47.13, intuition.library 47.53.
- Build: main 453ddd0b plus glyph-clone-diag.patch (diag counters only), worktree s30-glyphdesk; patch reverted before commit.
- Desktop: `amarchy_gui_rig.py --setup` from the worktree (Amarchy, amarchy-menu, amarchy-gui from the worktree build,
  running from VTCX:Apps/bin; port AMARCHY answers; Status lists Amarchy, amarchy-menu, amarchy-gui). The kit has no
  amarchy-flat, so no older resident existed (Status shows no amarchy-flat before start).
- Resident: `RAM:amarchy-flat BOOT` put from the worktree build after prefs desktop-theme/flat-system-look/flat-window-gadgets = on.
  md5 of the file fetched back from the rig = md5 of build/amiga/amarchy-flat = 17d6e24ac5ab1332a0e3a2f3e0fc6886.
  (No md5sum on the rig; the file was read back with ami.py get. The STATUS line prints the diag-only counters, so the
  running code is the diag build.)
- Window: `NewShell "CON:40/40/420/160/glyph clone/CLOSE"` with the desktop running.

STATUS: `clone draws=13 fallback=0 nest=0 empty=0 nzdirect=180 nzdis=0 rc=1`

Pixels (shot ~/Desktop/upterm-shots/s30-glyph-clone-desk-32.png, 640x256, script-counted):
- close zone x40-57,y40-49: 155 px band colour (102,136,187), 25 px glyph pen (170,170,170); 5x5 core x47-51,y43-47 all 25 glyph pen.
- depth zone x440-459: 126 band, 74 glyph pen; neighbouring gadget zone x420-439: 135 band, 65 glyph pen (glyph present, not a solid block).

Verdict: NOT reproduced. Under the Amarchy desktop on 3.2 with a verified-fresh resident, the clone draws
(draws=13, fallback=0), identical to the clean-Workbench run. The earlier fallback=46 was a stale resident (boot race),
as 2026-11-02-flat-gadgets-invisible.md:95-120 concluded. Caveat: the resident was started by hand after the desktop
was up, not by Amarchy's own boot start of UP-Term:bin/amarchy-flat.
