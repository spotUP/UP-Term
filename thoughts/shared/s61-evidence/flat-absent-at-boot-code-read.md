# flat=absent at boot with flat pens live: code read (no rig run)

1. Who writes the flat pens at boot. Not amarchy-flat. Two writers, neither checks amarchy-flat residency:
   - palette.prefs already on disk (ENVARC:Sys/Palette.prefs) that IPrefs loads at boot, written by an earlier Amarchy apply.
   - Amarchy's start steps, run once the Workbench screen is up: amarchy/amarchy.c:679 desk_start_gate -> desk_dos_flat_pens_refresh
     (desk_dos.c:1148-1156: flat = amconf_desk_flat_pens; if palette file stale, rewrite with desk_pens_flat) and
     desk_dos_start_apply (desk_dos.c:1584+, 763: flat && stale -> sync). Pens come from desk_pens_for(flat) (desk_core.c:182).
   - The only code that starts amarchy-flat from Amarchy is dk_flat_sync_theme (desk_dos.c:580, `else if (!r && (flat||gad)) pl_start(... BOOT)`),
     called from dk_flat_sync (1183 desk apply, 1428 marker recover). The start steps above never call it. So at boot the pens are
     written with no check that amarchy-flat is resident or will start.
2. Rig install path. rig.py does not run install.dos's User-Startup step and never mentions Amarchy-Flat (grep of tools/rig/rig.py,
   user_startup.py: no hit). It boots the master image's S:User-Startup as the earlier Installer run left it (rig.py:879-887, 1002-1009)
   and only inserts rig blocks (shadows, wasabid, solidwin: user_startup.py RIG_BLOCKS; rig.py:338-340, 388). The shadow pass copies the
   build's amarchy-flat binary over UP-Term:bin, not the startup block. install.dos:589-600 adds the BOOT block only if
   `Search S:User-Startup "  Run >NIL: UP-Term:bin/amarchy-flat BOOT"` fails. So a rig has the BOOT line only if its master image was
   installed/updated by an install.dos that contains that block. UNVERIFIED (needs `Type S:User-Startup` on rig 93 or xdftool read of the image).
3. Defaults. Both ON: prefs/settings_def.c:217-222 ("flat-system-look" "on", "flat-window-gadgets" "on"); amarchy/amconf_core.h:33-34
   ("default on: a header-only prefs file after install is the full flat look"). flat.c:1771 boot_wanted() is true on a fresh install, so
   BOOT would not return early. Hence prefs-off is NOT the cause. (Unless the rig prefs file sets them off: not checked.)
4. Written invariant. None found that ties pens to a resident amarchy-flat. What exists:
   - desk_core.h:30 "FLAT-P1-1 (OD-F4 a): ... SHINEPEN, SHADOWPEN and FILLPEN = the surface pen ... so bevels vanish".
   - flat_core.h:162-166: skip bevels only "when the screen pens really are flat; otherwise stock pens keep stock bevels" (amarchy-flat's
     draw decision depends on pens; the converse, pens depend on amarchy-flat, is not stated).
   - flat_core.h:237 S32-START-LOOK: BOOT runs from User-Startup before LoadWB, retries install from the 1 s watch (flat.c:1904,2036).
   - amarchy_gui_rig.py:57-58: "The kit's S:User-Startup starts it ... Amarchy itself loads it only when a flat switch flips on while
     none is resident (desk_dos dk_flat_sync_with)". This documents the gap: the only fallback start is on a flip, not at boot.
5. Verdict. Root cause layer: Amarchy desk start (desk_dos.c start steps), which makes the pens flat without making sure the
   program that draws the flat bevels is running. Trigger on this rig is probably (GUESS) a User-Startup with no Amarchy-Flat block
   (stale master image, rig does not refresh the kit's startup blocks) or the Run failing; with the line present, BOOT would have
   left a resident or a "held" STATUS, not "absent" (flat.c:1954 only exits early when prefs off). Fix shape: in the start path
   (desk_dos_start_apply / flat_pens_refresh, after the screen is up) call the same "flat on and no Amarchy.Flat record -> pl_start
   amarchy-flat BOOT" step dk_flat_sync_theme already has (extract it, call from dk_flat_sync at start), so the invariant "flat pens
   written => amarchy-flat resident or held" is enforced in one place regardless of User-Startup. Plus a reachability test: boot with
   no BOOT line, flat prefs on, assert STATUS not absent. Separately make the rig (or its master image refresh) run the install.dos
   Amarchy-Flat block so rigs match a real install. A stale image alone would be a rig fix; the start-path call is the product fix.
