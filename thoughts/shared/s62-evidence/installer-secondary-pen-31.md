# Installer 43.3 secondary push button on KS 3.1 (DOpus Workbench): NOT MEASURED, rig unstable

Status: question unanswered by pixels. Code-read answer below, rig attempt notes for the next try.

## Rig
Rig 97, `rig.py start --max --purpose "Amarchy session: Installer secondary pen on KS 3.1"`, `rig.py rtg` (800x600 depth 24, screen
"Directory Opus 2051912 graphics mem ..." = the DOpus screen is the Workbench screen). exec.library 40.10, `Version`:
Kickstart 40.68, Workbench 40.42. `Version VTC:Installer43` = Installer 43.3 (build/rig-inputs/Installer, 109956 bytes, copied to
build/rig97/vtc/Installer43; script VTC:inst43.script = `CD VTCX:dopus_install` + `VTC:Installer43 "Install DOpus5" NOLOG`).
build/amiga/amarchy-flat rebuilt, 212284 bytes, no errors; `cmp` build/rig97/vtc/amarchy-flat vs build/amiga/amarchy-flat IDENTICAL
(md5 e0f38ace7770df554a2fa537657046bc); on the rig `List UP-Term:bin/amarchy-flat` and `VTC:amarchy-flat` both 212284, same stamp 15:31:32
(kit shadow copy of the HEAD build; Amarchy + amarchy-flat + amarchy-gui + amarchy-menu all listed by Status after boot).

## What happened (why no pixels)
1. `rig.py install` right after start, then `rig.py rtg`: reset loop (watchdog: "10+ CPU resets in 60 s", ROM reset, no exception). Stale-build
   warning listed vtcon-handler, vsh, UPConsole etc. Rig recreated without install; booted and rtg worked.
2. With only amarchy-flat, Amarchy and Installer43 copied into build/rig97/vtc, the rig booted once and ran fine (Status showed the
   kit's Amarchy/amarchy-flat). I then sent `Break` to the Amarchy/amarchy-flat processes and copied files over UP-Term:bin: the guest hung
   (wasabid timeouts). Own mistake: never Break amarchy-flat (it owns the RectFill/frame patches).
3. After stop --keep and start again (kit now shadowed with the HEAD binaries at boot) the rig went into a reset loop:
   watchdog "CPU halted: reason = 3 PC=10b7fd7e" (log build/rig97/reset-loop-20261108-153913.fs-uae.log, deleted with the rig).
   Not isolated which binary (HEAD amarchy-flat or Amarchy booting on KS 3.1 + DOpus, or the stale defaults). That is a
   separate finding: the HEAD Amarchy set may crash at BOOT on KS 3.1 (it ran fine when started after boot in step 2).
No Installer screenshot taken; no STATUS counters, no pen table read. Rig 97 stopped (ps: no fs-uae for rig 97).

## Code-read answer (unverified by pixels)
- 00ff9f is stock wb16 pen 4 colour on the rig's DOpus/WB palette; it is not in any theme (installer-ks32-proof.md: grep of themes and
  amarchy sources finds no 00ff9f).
- amarchy/flat_glyph.c:423 (FLAT-BUTTON-NO-RING, OWNER-2026-11-08) fills the secondary role with `face` = pen_button (3e4254 tokyo-night).
  Rig 94's 00ff9f was an older amarchy-flat binary (before that change; its proof is from commit 05678a64 era), which painted the secondary role
  with raw pen 4. So on a HEAD build the expected KS 3.1 fill is 3e4254, same as KS 3.2 rig 93 (3e4254 measured, installer-ks32-proof.md run 2).
- Still to confirm on a rig: that the Installer opens on the DOpus screen (the only screen up: ami.py screens shows one screen,
  DOpus 800x600x24), and the HEAD binary's STATUS (bhits > 0, frame flat > 0).

## Next try
Start rig with a kit boot that survives (find which of Amarchy/amarchy-flat crashes at boot with `rig.py start` after copying one file at a time),
or start the build by hand after boot (`Run >NIL: VTC:amarchy-flat`) WITHOUT killing the kit's copy: set the kit's off via Exchange Disable first.
