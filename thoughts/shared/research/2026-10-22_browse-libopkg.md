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
