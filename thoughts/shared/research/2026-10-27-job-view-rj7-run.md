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

## Part 2 (text footer and drawn style, rig 7, 060)

Built from HEAD e4ed3649 (branch prefs-inline; the branch has moved on to 919a829e since, not deployed).
Deployed to VTC:Apps/bin: Amarchy 20441c2d073ce98469db9e3cadf6b011, amarchy-gui e7f1671ad0e6483edc9f2de225608405,
amarchy-menu ba463f85287b47a860c368ad3dceaffa; ENV:up-term/keymap 4a0acd9b2af428df643c3af343a1654e.
Setup: `Path Rushhours:Tools/UP-Term/bin ADD` + `Path SYS:Rexxc ADD` + `Run >NIL: VTC:Apps/bin/Amarchy` in one c.run
(path does not persist across c.run calls); no assign trick needed. Local feed only (upkg.conf pointed at 127.0.0.1).

### A. Text style (menu in terminal, 35 columns), Install > Update lists
- A1 local feed: PASS. JOB_LINES 3, JOB_DONE 1. Footer `|Enter: Accept  Escape: Back    Done|`. Body "[PROGRESS 1/1]
  update upterm / Downloading http://127.0.0.1:52068/Packages.gz. / Updated source 'upterm'." Enter closes, T file
  gone, menu on the Install rows; a second Enter re-ran Update lists (JOB_LINES 6), so the cursor was on that row.
- A2 feed stopped: PASS on content, DEFECT in footer. JOB_LINES 9, JOB_DONE 14, "error: opkg_download_backend:
  Failed to download ... curl returned 7". Footer renders `|Enter: Accept  Escape:NBackrk error|`: left hint and
  right status ("Network error") overlap at 35 columns.
- A3 slow feed (30 s), Esc at about 8 s: FAIL (3 of 3 runs). Footer `|Enter: Accept  Escape: Backancelled|` at once
  (overlap again), but vsh/upkg/curl stay alive: 4 processes still at 13.9 s and 6.1 s after Esc, gone 22.4 s after
  Esc (when the slow server answered); JOB_DONE 14 published only then. Part 1 saw the children die within 10 s.
  T file lingers if the view is closed while the children live; removed once they end.

### B. Drawn style (menu-style = graphical, menu-in-terminal = graphical)
Sentinels: B1 Done JOB_LINES 22, JOB_DONE 1; probe after close JOB_LINES 25 (cursor on Update lists, re-run);
B2 failure JOB_LINES 28, JOB_DONE 14; B3 cancel JOB_LINES 30, JOB_DONE '' (children still alive, as in A3).
- Done: PASS. Footer "Enter: Accept  Escape: Back  Done" (green), title "Update lists ........ Amarchy".
  rj7b-drawn-done.png. After Enter the Install level is back with the "Update lists" row highlighted:
  rj7b-drawn-done-closed.png. T file gone after close.
- Failed: PASS on content, DEFECT in footer: "Escape:NBackrk error" (red), same overlap as the text style.
  rj7b-drawn-failed.png, rj7b-drawn-failed-closed.png.
- Cancelled: footer "Backancelled" (red), same overlap; children not killed (see A3). rj7b-drawn-cancelled.png.
- Other shots: rj7b-drawn-install-level.png, rj7b-drawn-running.png (all in ~/Desktop/upterm-shots/).
  The drawn footer is wide enough for "Done" but not for "Network error" or "Cancelled".

### Defects
1. Footer: left hint and right status overlap when the status is longer than the free space ("Network error",
   "Cancelled"), in both styles.
2. Cancel does not stop vsh/upkg/curl within 10 s (22.4 s, only when the server replied), 3 of 3 runs here.
3. T:amarchy-job.* lingers when the view is closed while cancelled children still run.

### Restore
UP-Term:etc/upkg.conf ac11b5a5..., ENV:up-term/up-term fbb38427..., ENV:Amarchy/prefs c7d44aee... restored by copy and
md5 proved equal to the backups. T: has no amarchy-job files, no upkg/curl processes. Feed and slow servers killed.
Amarchy QUIT, rig 7 stopped. The deployed copies stay in rig 7's VTC:Apps/bin until the next install.
