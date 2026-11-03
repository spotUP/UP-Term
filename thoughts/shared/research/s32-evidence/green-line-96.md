# S32-GREEN-LINE-96 read-only probe (rig 96), 2026-11-03

Live probe NOT possible: mcp__amiga__amiga_info -> "cannot reach 127.0.0.1:1234, connection refused" (wasabi MCP not bound to rig 96). Text sources only.
ENV: is RAM, so the live palette.prefs / DrawInfo pen 11 / pen 253 RGB are unread. Cause NOT proven.

Proven
- Depth is deep: build/rig96/retry.log "S 0 640x256 depth=8 Workbench Screen" (install-time poll, 5 samples); Env-Archive/Sys/screenmode.prefs = ID 0x29000, 640x256, depth 8. So pen map 5 -> screen pen 253, palette entry 5 (desk_core.c:464) is the right target. The W43 "stayed PAL" is a monitor-mode note, not a depth change.
- Installed Amarchy / amarchy-flat / amarchy-gui in build/rig96/vtc are byte-identical (md5) to build/amiga/*; c91ea918 is an ancestor of 3880f642. (Not proven that build/amiga was built after c91ea918: no WB-TITLE-GREEN string exists in binaries, comment only.)
- No palette.prefs in ENVARC:Sys (only Pointer, screenmode, WBConfig). dk_run_editor uses "USE" (desk_dos.c:107) -> ENV: only, RAM. The themed entry 5 therefore exists only while ENV:Sys/palette.prefs does.
- Env-Archive/Amarchy/prefs holds only the header comment (defaults: desktop-theme on, desktop-palette on, flat-system-look on). desk-original/ENV-Sys-palette.prefs.absent (12:45) shows the install-time apply did reach the palette part once.
- Boot: User-Startup runs amarchy-flat BOOT and Amarchy; the session shows "Welcome to Amarchy" = a post-install boot, ENV: empty at that boot.

Suspected (ranked)
1. Boot-time re-apply (desk_dos.c ~1398-1430, S31-START-PALETTE -> desk_start_palette -> desk_palette_theme) did not write entry 5, or ran before/under the Workbench reset so IPrefs rebuilt pens from stock. Rig 16 proof was taken in the SAME session as the apply, not after a cold boot. Compare: rig 16 shot was post-apply; rig 96 owner views post-reboot.
2. dk_palette_base(0, pens, dk_screen_depth(), ...) at start uses the screen at that moment; if the WB screen is not yet open it returns d=2 (dk_screen_depth default, desk_dos.c:126), desk_palette_pen(5,2) = -1 (desk_core.c:278), so entry 5 is skipped silently. Note dk_start at 1406 clamps depth to 3 for the plan but 1419 uses the real one.

Next probe (needs owner OK, not read-only): wasabi to rig 96: read ENV:Sys/palette.prefs entry 5 (expect theme dark, not 55dd55), GetRGB32 pen 253, DrawInfo pen[11]; amarchy-flat STATUS; check start-time depth value.
Fix direction: if (2): make start wait for / lock the WB screen (ipd_lock_wb_screen retry) before dk_screen_depth, or read depth from screenmode.prefs; add a test where depth lookup returns the default and assert entry 5 still set. If (1): run desk_start_palette AFTER LoadWB.
