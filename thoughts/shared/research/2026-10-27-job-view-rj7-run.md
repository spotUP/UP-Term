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

## Part 3 (rerun of A2, A3 on HEAD cdf7fb5d, rig 7, 060, text style; drawn style NOT run)

Build md5s (deployed to VTC:Apps/bin by rig.py install, equal to build/amiga): amarchy-menu 87d567c930be29b859358e55e8ef8e21,
amarchy-gui 13db2903efe8f6d4c4b1365ee8a842b3, amarchy-index 0f87cb3dea57551f41dc3797082cea86, Amarchy 20441c2d073ce98469db9e3cadf6b011,
keymap ffeb01db8113b4d212be29502cec9fe7 (= dist/keymap). Start printed no [WARN] drift line.
Feed: local P7 feed only (127.0.0.1). Rig menu root now has a top-level Packages group (Browse, Search, Installed, Upgrades, Refresh).

- A1 Update lists, text: PASS. JOB_LINES 3, JOB_DONE 1. Footer `|Enter: Accept  Escape: Back    Done|`; T file gone after close.
- A2 feed down, text: PASS (footer), note. JOB_LINES 6, JOB_DONE 14, body "curl returned 7". Footer `|Enter: Accept         Network error|`
  No overlap, but the "Escape: Back" hint is dropped to make room (the status word does not overwrite a hint; the hint is absent).
- A3 cancel, text: FAIL. Esc at 8 s of a 30 s slow feed. Footer after Esc `|Enter: Accept             Cancelled|` (no overlap).
  Children vsh(job)/upkg/curl still alive 22.3 s after Esc (polled every 0.7 s), gone only when the slow server answered at 30 s.
  An earlier run showed the same. T:amarchy-job.1010c3f8 stayed until the view was closed with Enter, then gone; JOB_DONE stayed empty until the children ended.
  The Esc cancel fix does not stop the children promptly.
- B2, B3 drawn: NOT RUN. The hotkey over the terminal gave the text menu; the drawn menu could not be opened (no Workbench-focus
  click geometry known; AMARCHY_ROWS never published). Drawn footer is pixels and was not read.
- PG.8 Packages group, text: PARTIAL. Before Update lists: Browse "No packages listed (is upkg insta...", AMARCHY_PKGS_ROWS 0, LOADS 1.
  After Update lists + Refresh: PKGS_ROWS 1, LOADS 2; Browse > other > `[upkg] up-nano  1.0`. Search "nano" lists up-nano (the Search prompt
  footer reads `|Enter: Accept  Escape: Cancel  Done|`). Search with no match, Install, Remove confirm (No first), Upgrades, failing job,
  and the whole drawn style: NOT RUN. A mis-aimed key started tmux (killed) and typed a stray word into the terminal.

### Restore (md5 live on rig vs backup)
UP-Term:etc/upkg.conf ac11b5a5ca3404e798cb4c938c8fe852 = backup; ENV:Amarchy/prefs c7d44aeee893f340ddcb08b93a3177da = backup;
ENV:up-term/up-term fbb384279d519334b50046b621d3535e = backup. ENVARC files never written. Feed and slow servers killed, rig 7 stopped.

## Part 4 (root cause of the slow Esc cancel; rig 7, 060, HEAD 567c9887, no Amarchy change)

Verdict: the cause is NOT in Amarchy. Amarchy's jb_cancel signals CTRL_C to a task of the job tree (any of them would do);
every task of the tree receives it, and both upkg and curl swallow it and go back to waiting on the network.

Setup: local slow server rj7b_slow2.py (127.0.0.1:53986, sleeps 30 s, 404); UP-Term:etc/upkg.conf `src/gz upterm
http://127.0.0.1:53986` (backup md5 ac11b5a5ca3404e798cb4c938c8fe852). The job reproduced outside Amarchy as the same line
jb_start runs: `Run >T:a1x.out VTC:vsh -c "upkg update; echo @JOBRC $?"`. Ctrl-C sent by `wasabi kill <addr>` (Signal CTRL_C).

Task tree (VTC:taskdump, 6 s in):
- `Background CLI` cmd VTC:vsh: wait 8000d000 (port | CTRL_C|E|F); vsh os_wait forwards CTRL_C to its job (shell/vsh.c:1219-1226).
- `vsh job` cmd upkg: wait 80001000 recvd 00000100.
- `vfork()'d process` cmd curl: wait 40001000 recvd 10000100.

Ctrl-C at 7 s, time until all three are gone:
- to vsh: 23 s (gone when the server answered). - to upkg alone: 23 s. - to curl alone: 24 s.
- taskdump 2 s after the Ctrl-C to curl, and again after one to upkg: both back in the same Wait (same sp, same mask),
  CTRL_C absent from recvd: they took the signal and ignored it.
- curl alone, native, no upkg/ixemul parent: `Run >T:a1c.out UP-Term:bin/curl -fsSL -o T:a1c.bin http://127.0.0.1:53986/x`,
  Ctrl-C at 5 s: gone after 25 s. So curl ignores CTRL_C by itself.

Root causes (two, both outside Amarchy):
1. curl: the vendored Aminet binary dist/net/curl-8.22.0/curl.0?0 (curl 8.22.0, clib2, AmiSSL; vendored in vtcon, not built here).
   Blocked in the socket wait, CTRL_C wakes it (clib2 sets the bsdsocket break mask, the binary has ___break_signal_mask and
   "Interrupted system call") but the EINTR is treated as an empty wakeup and the transfer goes on. Invariant violated: an
   AmigaDOS command ends on SIGBREAKF_CTRL_C. Fix needed: curl built from source (8.22.0, same clib2 toolchain) with lib/select.c
   Curl_poll / the tool's transfer loop ending the transfer on an EINTR from WaitSelect (or the tool checking the break after each
   poll wakeup and exiting 130). Exact source line unverified (binary only).
2. upkg (opkg 0.9.0, upterm-ports): Ctrl-C to upkg while xsystem waits for curl is swallowed. libopkg/xsystem.c (kit-main build src,
   lines ~121-123) loops `waitpid` while errno == EINTR and never passes the break to the child; `update` installs no SIGINT handler.
   Why ixemul's default SIGINT action did not end upkg is unverified. Fix needed (upterm-ports, owner vtcon-05): in xsystem, on
   an interrupted waitpid / read with SIGINT pending, forward it to the child (kill(pid, SIGINT), which for a native child must
   reach it as SIGBREAKF_CTRL_C), reap it, and exit 128+SIGINT.
With 1 alone fixed, Amarchy's jb_cancel already works when task_find_by_output picks curl; with 1 and 2 it works for whichever
task it picks (vsh forwards to upkg, upkg to curl).

Also in scope, no defect of its own: T:amarchy-job.* stays while the view is open because the view's reader keeps it until jb_end
(close); JOB_DONE is empty during the wait because the job really still runs (the children ignore the break). Both follow from 1+2.
No Amarchy edit, no regression test: a test in tests/test_job.c would pin no Amarchy behaviour that is wrong.

## Part 5 (drawn style rerun, rig 7, HEAD 8ab1ed79)

Build md5s, deployed by rig.py install and read back from VTC:Apps/bin (equal to build/amiga): Amarchy ea68492cb80207f7f5fa7c61b06a438c,
amarchy-gui 63526aa017a50bfb907a1f6ce06c19d0, amarchy-menu 66da7457de424907afa4b787d809917e. Start [WARN]: 11 files in VTC: differed from the
build (Apps/bin/Amarchy, Claude, UPTerm, amarchy-gui, amarchy-menu, engbench, pty-handler, up-console.device, upprefs, vtcon-handler, keymap);
rig.py install fixed that. Setup: menu-style and menu-in-terminal = graphical in ENV:Amarchy/prefs, menu-in-terminal = graphical in ENV:up-term/up-term.
Feeds local only: good feed = python http.server on build/ipk (port 53985), slow feed = tools/probes/jobrun/slow_feed.py (53986).
Sentinels are AMARCHY_JOB_LINES / AMARCHY_JOB_DONE (not JOB_*). The shell path is C: only; no manual Path ADD anywhere.
Rig layout: `Assign UP-Term:` listed only Rushhours:Tools/UP-Term (the kit, with bin/upkg 926016 bytes, older Amarchy binaries running at boot).
To run HEAD I quit the kit Amarchy, set UP-Term: = VTC:Apps + kit (ADD) and started VTC:Apps/bin/Amarchy. UP-Term:bin/upkg exists (kit part).

- P1 path proof: PASS. Packages > Refresh (Update lists), drawn: body "[PROGRESS 1/1] update upterm / Downloading http://127.0.0.1:53985/Pa... /
  Updated source 'upterm'.", AMARCHY_JOB_DONE 1 (rc 0), not 128. Note JOB_LINES read 1 although 4 body lines are drawn (counts at job end;
  Part 2 saw 3 for a text run; unverified what it counts). rj7c-drawn-refresh-running.png, rj7c-drawn-refresh-end.png.
- B1 drawn Done: FAIL on the spec text. Footer shows only "Done" at the left, no "Escape" hint (and no Enter hint): hints are gone, no
  overlap. Part 2 had "Enter: Accept  Escape: Back  Done". Pixel crop of the footer row held only the word "Done".
- B2 drawn failure: PASS for overlap. Footer "Network error" (orange), AMARCHY_JOB_DONE 14, JOB_LINES 6, body "error: opkg_download_backend: Failed ...".
  All hints dropped, as in B1. rj7c-drawn-failed.png.
- B3 drawn cancel: PASS for the footer, FAIL for children not measured exactly. Footer "Cancelled" (orange), no overlap, hints dropped.
  Esc at 7 s of the 30 s slow feed. Before Esc: UP-Term:bin/vsh, upkg, curl alive. My poll (every 1 s, 45 s window) never saw an empty
  list because it also matched the terminal's own VTC:vsh; at the end only VTC:vsh remained, so the children were gone inside 45 s, the
  second was not recorded. JOB_DONE was empty 1 s after Esc, 0 at the end of the poll. rj7c-drawn-cancelled.png.
- G1 Packages group (drawn): PASS. AMARCHY_ROWS root: Apps|Dopus|Settings|Style|Window|Screens|Learn|System|Install|Packages|Trigger|Demos|Music;
  Packages = Browse|Search|Installed|Upgrades|Refresh. Search opens the prompt "Search packages (text):" (typed "zzqq" shows as zzqq_).
  Enter with no match: the menu closes and nothing is shown (no "no packages" message, no empty list), AMARCHY_ROWS stays at the Packages level
  values. rj7c-drawn-packages-group.png, rj7c-drawn-search-typed.png, rj7c-drawn-search-nomatch.png.
- Not run: Install > Update lists (the same job is reached as Packages > Refresh).

### Restore
UP-Term:etc/upkg.conf ac11b5a5ca3404e798cb4c938c8fe852, ENV:Amarchy/prefs c7d44aeee893f340ddcb08b93a3177da, ENV:up-term/up-term
fbb384279d519334b50046b621d3535e restored by write-back and md5 read back equal to the backups. ENVARC never written. T: had no amarchy-job files.
Good and slow feed servers killed, rig 7 stopped (fs-uae for rig 7 gone; rigs 2 and 8 are others').

### Review of Part 5 (vtcon-a1, 2026-10-27)

- B1 is not a FAIL. The job-view plan (thoughts/shared/plans/2026-10-27-amarchy-job-view.md, line 35) gives the drawn
  footer as the status word only: "Running...", "Done", "Failed (code n)". `jc_footer` (amarchy/job_core.c) has
  returned only that word since it was written. The "Escape: Back" hints come from libupkgui's text chrome
  (`ch_footer_layout`); no vtcon source has that string, so the Part 2 overlap ("Escape:NBackrk error") was the text
  style. Whether the drawn footer should also carry key hints is an open product question, not a defect.
- G1 Search "zzqq" closing the menu is a real defect in every Search, not only no-match: `gui_ask_value` ran a nested
  event loop whose Return/Escape set `stop` and left `mode = M_INPUT`, so the outer loop ended too. Ledger row
  GUI-SEARCH-CLOSES; fix in amarchy/gui.c (restore mode, head, top; clear stop), regression case
  `tools/rig/amarchy_gui_rig.py --pkg-search`.

## Part 7 2026-10-29 (A3/B3 Esc cancel rerun 3, rig 7, origin/main fa104c44)
Setup: ixemul.library 882811dc086ca77b233aaa0d3e4e8448 on VTCX:ixp6, LIBS:, SYS:Libs; curl adf34ce1ec4bcf5137767eef3d2c4ce2 on Rushhours:Tools/UP-Term/bin, VTCX:kit-userland, VTCX:Apps/bin, UP-Term:bin. IXEMUL/IXNET unset. Start [WARN]: only DOpus 5.104 not installed (and "Workbench stayed PAL"). No Assign command run (EXISTS only).
Feed: slow_feed.py (30 s per request) on 127.0.0.1:53986; Rushhours:Tools/UP-Term/etc/upkg.conf overwritten with scratchpad slow.conf for the runs (the original md5 ac11b5a5... restored afterwards; rig started --fresh each time).
Method: Menu > Packages > Refresh, Esc at t=8 s while vsh(job), upkg, curl are alive (wasabi Status polled in a loop). Clean-screen check (windows + requesters via wasabi) before every run. Three starts hit an IPrefs "System Request" (Retry|Cancel) at boot; each was restarted, the runs are on clean screens only.
| case | style | run | vsh(job) gone | upkg gone | curl gone | JOB_LINES | JOB_DONE | JOB_WORD | T:amarchy-job after close |
| A3 | text  | 1 | <=0.7 s | <=0.7 s | <=0.7 s | 6 | 0 | Cancelled | gone |
| A3 | text  | 2 | <=0.7 s | <=0.7 s | <=0.7 s | 2 | unset | unset | gone |
| B3 | drawn | 1 | <=0.2 s | <=0.2 s | <=0.1 s | 5 | 0 | Cancelled | gone |
| B3 | drawn | 2 | <=0.2 s | <=0.2 s | <=0.1 s | 2 | unset | Cancelled | gone |
Time = first poll (Esc + poll latency) at which the process was no longer listed; the text polls are about 0.7 s apart, so those are upper bounds. In text run 1 the first sample after Esc already showed no upkg/curl.
Text footer after Esc (screen text): window "Downloading http://127.0.0.1:53986/Packages.gz" was still on the screen at 0.5 s in both text runs; footer text itself was not read in these runs (not needed for the question).
Verdict: PASS in all 4 runs (text and drawn): curl, upkg and the job vsh end well inside upkg's 3 s grace (under 1 s). Part 6 (old ixemul): upkg 3 s, curl ~27 s (FAIL). The ixemul 627d6ce build fixes the curl wait.
Notes: JOB_DONE/JOB_WORD were unset in text run 2 and JOB_DONE in drawn run 2 at the read (sentinel published late or not yet); JOB_LINES varies 2 to 6. Not related to process exit; not investigated.
Run script rj7r2-run.py, logs rj7r2-{text,drawn}-*.log and rj7r2-all.log in the scratchpad (text 1 log: rj7r2-text-1.log).
