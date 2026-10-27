# RJ.7 job view rig run, drawn style (rig 7, 060), 2026-10-27

Tree prefs-a1, tested at HEAD 9a81b1f4 (the tree's HEAD moved to eed2cacc and build/amiga was rebuilt by another
session at 15:22, after the run; the md5 lines below are the files that were tested).

## Deployed (VTC:Apps/bin, ENV:up-term/keymap) = build/dist files at test time
- Amarchy 20441c2d073ce98469db9e3cadf6b011
- amarchy-gui 812004521b2cde790e4e4ca2e45a43f3
- amarchy-menu b177712d9c012f8e4a5ec17aae667ec7
- keymap 4a0acd9b2af428df643c3af343a1654e (dist/keymap; rows install.update / install.upgrade use job:)

## Setup
- Rig kit lives on Rushhours:Tools/UP-Term, which holds older Amarchy binaries and the only upkg. UP-Term: was
  assigned VTC:Apps first, kit second (multi-assign), Amarchy commodity restarted from it.
- Path: the commodity inherits the rig shell path (C: only), so `upkg` was "vsh: upkg: not found" (rc 127,
  JOB_DONE 128, JOB_LINES 1) on the first try. Rig limitation (a real session has UP-Term:bin in its path);
  re-run with `Path Rushhours:Tools/UP-Term/bin ADD` before starting Amarchy.
- Feed: UP-Term:etc/upkg.conf (kit file) backed up, pointed at the one-package P7 feed on http://127.0.0.1:50746
  (p7_driver.py). menu-in-terminal = graphical added to ENV:up-term/up-term; menu-style = graphical to ENV:Amarchy/prefs.

## Results (sentinels read with GetEnv; footer text is drawn pixels, not read: see Unverified)
1. Update lists, local feed: JOB_LINES 0 -> 3, JOB_DONE = 1 (rc 0), job file T:amarchy-job.10107dd8 present while
   the view is open, window stays "Amarchy" (no UP-Term window), lists file created
   (pkg/upkg/lists/upterm 268 bytes). Enter closes the view: T file gone, menu rows = Install level
   (Install add-ons, Upgrade all, Update lists, Packages). The job ended inside the first 1.5 s poll, so
   "moves" is seen as 0 -> 3 only. PASS (sentinels).
2. Feed stopped: job output "error: opkg_download_backend: Failed to download http://127.0.0.1:50746/Packages.gz,
   curl returned 7", "@JOBRC 13"; JOB_LINES 6, JOB_DONE = 14 (rc 13, core word "Network error"). PASS (sentinels).
3. Slow feed (python server sleeping 30 s per request): while Running, ps showed vsh, upkg, curl; Esc at ~8 s;
   within 10 s upkg, curl and vsh gone, wasabi ps lists no amarchy-job/upkg/curl; JOB_DONE published 14 afterwards
   (the cancelled child's rc 13, so the footer follows state JC_CANCELLED, not the rc). T file stayed while the
   cancelled view was open, gone after Esc closed it. Menu still answered (Up, Esc went to the root rows). PASS.
4. After all: T: has no amarchy-job.#? files; conf, ENV:up-term/up-term and ENV:Amarchy/prefs restored by copy and
   proved by md5: upkg.conf ac11b5a5ca3404e798cb4c938c8fe852 (same as before), up-term fbb384279d519334b50046b621d3535e,
   prefs c7d44aeee893f340ddcb08b93a3177da (same as before); upkg.conf.rj7bak deleted; UP-Term: assign back to
   Rushhours:Tools/UP-Term. Feed and slow servers stopped. Rig 7 stopped.

## Unverified
- Footer wording ("Done", "Failed ...", "Cancelled") on screen: no text sentinel exists for it (gui.c publishes
  only JOB_LINES and JOB_DONE) and the brief barred a screenshot reader. job_core.c maps state and rc to the
  word (rc 13 -> "Network error", cancelled -> "Cancelled").
- "Menu back on the Update lists row": the selected row is not published; only the level (Install rows) was read.
