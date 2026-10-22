# upkg browse in libopkg: state after P1 (2026-10-22)

Branch browse-snappy in /Users/spot/Code/UP-Term/upterm-ports-snappy, HEAD 45d8ff4 (P1 only).

## P1 done
- pkgs/upkg-ui/src/ui.c + ui.h = libupkgui.a (widgets, ui_suspend/ui_resume, ui_program_*), chrome.o in same lib.
- upkg-ui binary no longer has browse. Host run-cases + run-tty streams identical to baseline (0 diffs), test_chrome 71/0, keyfd/tite ok. Cross `make upkg-ui` exit 0.

## P2 plan (not started in code)
- New libopkg/upkg_browse.c + patch 0025. browse cmd flags PFM_DESCRIPTION|PFM_SOURCE; hash already loaded by main().
- Actions: call opkg_cmd_exec(opkg_cmd_find("install"|"remove"|"upgrade"|"update"),1,[tag]) in-process.
  DEVIATION: deprecated libopkg API (libopkg/opkg.c, WITH_LIBOPKG_API=OFF) opkg_install_package has no solver (installs one package, no dependency resolution) and opkg_re_read_config_files uses saved_conf set only by opkg_new(), which main never calls. After update: pkg_hash_deinit + pkg_hash_load_feeds + pkg_hash_load_status_files.
- Sinks: opkg_config->opkg_vmessage hook exists (opkg_message.c); add sink for upkg_vfprintf (upkg_wrap.c); xsystem.c: pipe capture of child stdout/stderr into sink when set.
- Rows: upkg_list_find/print_pkg, upkg_load_prev/upkg_is_new (static, opkg_cmd.c:542-624), opkg_solver_list_upgradable. Info: pkg_formatted_info into open_memstream.
- Interactive: scan UPKG_LISTS_DIR as WIP interactive() (/private/tmp/upkg-ui.wip.c ~974-1010), ui_suspend/ui_resume.
- Links: libupkgui + ncursesw; recipe.mk opkg deps += upkg-ui ncurses. Remove pkgs/opkg/check/browse_lists.sh, browse_dispatch.sh.

## vtcon files referencing dist/upkg-browse
Makefile, dist/upkg-browse, tests/test_dist_manifest.py, tests/test_upkg_browse.py, tools/rig/upkg_browse_rig.py, tools/rig/upkg_browse_extra_rig.py (kit lists: check Makefile entries).

## P2a result

Commit on branch browse-snappy in upterm-ports-snappy (see git log). opkg patch 0025 (browse in C, `libopkg/upkg_browse.c`, helpers `upkg_browse_list` and `upkg_browse_info` in `opkg_cmd.c`, `list-browse` and the script dispatch removed, CMake links libupkgui and ncursesw). Host check `pkgs/opkg/check/browse_tty.sh` passes: rows and statuses on a pty, info box and back with zero child processes (preload shim `check/spawn_count.c`, positive control counts 1 for a postinst install), install from the list turns the row to installed. Cross build `make opkg` exits 0. upkg size 594480 -> 887504 bytes (+293024, libupkgui + ncursesw). Old checks browse_lists.sh and browse_dispatch.sh removed.

Notes:
- `make opkg-patches` regenerates 0001-0024 with new hashes and header numbering; they were restored from git, only 0025 is new. Building (`make host-opkg`) re-extracts build/src/opkg and deletes uncommitted work there: commit in build/src/opkg before building.
- Action output is captured by dup2 of fds 1 and 2 to a temp file, shown in the program view.
- Not run on a rig or the Amiga.

## P2b TODO

- upkg_vfprintf sink in libopkg/opkg_message.c through the opkg_config->opkg_vmessage hook.
- xsystem.c: capture child output through a pipe into the sink.
- Replace the fd-redirect capture in run_action.
- dup2 behaviour on ixemul/Amiga is untested.
- No `update` key, so no reload helper (pkg_hash_deinit, reset prev statics, pkg_hash_load_feeds, pkg_hash_load_status_files); skipped on purpose, it would be dead code.
- vtcon files still referencing dist/upkg-browse: Makefile, dist/upkg-browse, tests/test_dist_manifest.py, tests/test_upkg_browse.py, tools/rig/upkg_browse_rig.py, tools/rig/upkg_browse_extra_rig.py.

## P2b result

Commit on browse-snappy in upterm-ports-snappy (amends opkg patch 0025; 0001-0024 untouched). `upkg_set_sink` (upkg_wrap.c) takes everything upkg_vfprintf and upkg_wrap_puts would write to stdout/stderr; `xsystem` pipes script and curl child output (stdin /dev/null) into the same sink. upkg_browse.c assembles lines and calls ui_program_line, so the box shows a script's first line while the script runs. The dup2 capture is gone. Box ends with `[OK] upkg <cmd> <pkg>` or `[ERROR] ... (exit N)` and waits for OK. Interactive packages (feed control `UP-Term-Interactive: yes`): ui_suspend (def_prog_mode + endwin), plain output, `[OK]`/`[ERROR]`, "Press Return to go back to the list.", ui_resume; cursor stays on the add-on (default_item). After any change `hash_reload` (pkg_hash_deinit, load_feeds, load_status_files); .prev statics are re-read by the list code on every call, nothing to reset. No update key, no update action.
Tests: check/browse_tty.sh (a) live: postinst echo, waits for the probe to see the line on screen, then second line; (b) interactive route and list back; (c) Enter+Back 0 children. `make host-opkg` passes, `make opkg` exits 0, upkg 888496 bytes (P2a 887504).
Keys on the test terminal: Down is `\eOB` (application keypad), not `\e[B`.
Not verified: dup2 in a vfork child on ixemul; no rig or Amiga run.

## P3a result

- Regression proof: the old check (a) PASSED against a buffer-until-exit build (script timed out waiting, never touched second.ran, so the "early" test never fired). Check was weak; fixed (upterm-ports 3df6a83: postinst touches `timeout` when the probe never saw LIVEFIRST while it ran; check fails on it). Buffered build: `[FAIL] live: the script waited in vain, its first line never reached the screen while it ran`. Restored build: `[OK] upkg browse: ... live progress box ...` passes.
- Landed: upterm-ports main fast-forwarded to 3df6a83 and pushed (origin was fa377be, rebase was a no-op). host-upkg-ui + host-opkg checks pass (24 OK lines).
- vtcon: dist/upkg-browse, tests/test_upkg_browse.py, Makefile cp + test line removed; test_dist_manifest now asserts no upkg-browse script; rigs drive `upkg browse` only (no upkg-browse/upkg-ui copy, list-browse case removed from upkg_rig listnew). Rig scripts not run (no rigs).
- Kit rebuilt: kit-stage rc 0, `make dist dist-ipk` ok (first attempt hit a flaky `rm -rf build/dist/up-term` "Directory not empty"; removing the dir by hand and rerunning worked), dist-check OK, test_dist_versions + test_dist_manifest 25 passed 1 skipped. VERSIONS: `UP-Term 1.0 28c8563a+dirty 2026-10-22 (upterm 0b6c1eb)`; kit upkg 888496 bytes. Note: upterm hash 0b6c1eb is the pin recorded by the kit, not 3df6a83 (check pin before release).

## P3b result

Rig 4 only, `rig.py start --max`. Fresh path: build/rig4/sys.hdf deleted and re-copied by `rig.py setup` (the copy of the source system disk, which still carries an older UP-Term: base: the script logged "base installed ... over an older base"), then the 1.0 kit from build/dist/up-term installed by upkg_browse_rig.py (all options ticked). Kit not rebuilt.

`UPTERM_RIG=4 python3 -I tools/rig/upkg_browse_rig.py`: Verdict FAIL (8 of 12 checks), 6m17s.
- [PASS] step 0 upkg update rc 0; before: up-vim not installed, no sentinel
- [PASS] install: sentinel UP-Term:pkg/up-vim/Uninstall appears; after install: up-vim in list-installed
- [PASS] no Shell text on the screen, all three screens
- [FAIL] colour: green pixels rise after install 0 -> 0
- [FAIL] remove: sentinel gone
- [FAIL] after remove: up-vim not in list-installed
- [FAIL] colour: green pixels fall after remove 0 -> 0 (start 0)
End-of-run window text: the programbox still shows "[OK] upkg install up-vim" and the OK button, i.e. the remove never ran (the Remove / "Remove up-vim?" / y step did not take effect). Not diagnosed. Screenshots (unread) in the run's out dir.

Not run: upkg_browse_extra_rig.py (stopped at the first failing case as instructed), so the live-lines check of the vfork/dup2 sink is still UNVERIFIED. The install itself completed through the browser. Rig 4 was stopped by the script at the end.

## P3b diagnosis

Rig 4 only, 4 probe runs (a copy of upkg_browse_rig.py's flow up to the OK box, then text reads via typewin.screen_text; probe script in the session scratchpad, not committed). Diagnosis only, no fix.

Evidence
- Reproduced: after the install the box shows the pdone footer ("Up/Down: Scroll  Left/Right: Choose  Enter: Accept ...") and Return, Return, Escape (0x45) and q change nothing (probe 1 and probe 4). Output reaches the screen: the "[OK] upkg install up-vim" line and the OK button are drawn after the action, so the parent's fd 1 (newterm(NULL, stdout, keyfp)) is NOT clobbered.
- Rig 4 after the runs: `Info` shows DH0: System 106M, 217220 blocks used, 2 free, 100% full (and RAM: 502K, 0 free, 100%). UP-Term: is System:UP-Term (`Assign LIST`), i.e. the install target is on the full volume. build/rig4/sys.hdf is 1.5 GB, but its DH0 partition is 106M (copy of SRC_HDF, tools/rig/rig.py:453-456 `shutil.copyfile(SRC_HDF, RIG / "sys.hdf")`); base 1.0 plus the leftovers of earlier runs fill it.
- Probes 2-4: the sentinel UP-Term:pkg/up-vim/Uninstall never appeared (500 s), the box still ended in the pdone state: the install failed on the full volume. Probe 2 additionally showed "Wait: Command running" with a Copy process alive for minutes.
- The "volume System is full" requester text itself was NOT captured: rig 4 had to be restarted before the owner's message arrived, and the requester watch (ami.py) only runs inside EXEC calls, not while keys are typed. UNVERIFIED which requester was on screen.

Verdict on the lead hypothesis (vfork/dup2 clobbers the parent's fds): not supported.
- ixemul/library/vfork.c:265-314: the vfork child gets its own user area `mu` with its own u_ofile table (bcopy of the parent's, f_count++ per file); xsystem.c:70-73 dup2 only replaces entries in the child's table. The parent's fd 0/1/2 are untouched.
- ui.c:743-760 open_keys: keys come from /dev/tty (or a dup of fd 1 for a programbox), not from fd 0, so /dev/null on the child's fd 0 cannot starve the key loop. Parent fd 1 is intact (drawing works).

Root cause (most likely, rig fixture): System: (DH0:, 106M) is full, so the install (and any write: T:, hash files, status files) hits a DOS "volume full" requester (Retry/Cancel) on the Workbench screen; it takes the keys (the Return/Esc/q presses never reach the vsh window) and blocks the writing task. This fits every failing run; it does not by itself explain why probe 1 had a sentinel and still dead keys (the volume probably filled during the later hash/status writes) - UNVERIFIED until the requester is read as text on a rig with a full System:.
- Fixture: tools/rig/rig.py:453-456 copies a 106M-partition image; tools/rig/upkg_browse_rig.py:setup (lines ~123-160) installs the full 1.0 base onto System:UP-Term and never checks `Info DH0:` free space first; run-to-run leftovers (Copy-over of an older base, up-vim files) are never freed.

Fix direction
1. Rig: before the run, assert free blocks on the UP-Term: volume (Info) above the kit size plus the package, fail with a clear message otherwise; use a roomier system partition or put UP-Term: on a big volume (Rushhours has 1 GB free); check requesters (ami.py requesters) at each key step so a blocked run reports the requester text instead of "no effect".
2. Product (a question for the owner): upkg browse should not hang silently on a full disk. The install goes through DOS requesters; setting pr_WindowPtr = -1 in the upkg process (and child) turns them into error returns, which libopkg reports as an install failure ("[ERROR] upkg install up-vim (exit N)") in the box. Recommended.
3. To close the UNVERIFIED parts: free ~20 MB on rig 4 System:, rerun upkg_browse_rig.py; if the remove step then works the diagnosis is confirmed and the vfork lead is dead for good.
