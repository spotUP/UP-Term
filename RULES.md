# vtcon project rules

The global rules (`~/.claude/CLAUDE.md`) apply; this file adds the project's.

1. **The engine stays portable C89.** No OS calls, no `//` comments, declarations
   at block start, no `stdint.h`, no 020-only code: vbcc compiles it for a plain
   68000 inside DCTelnet. Memory goes through `VT_MALLOC` / `VT_FREE`.
2. **The conformance matrix is the spec.**
   `thoughts/shared/research/2026-09-28_console-conformance-matrix.md` says what each
   sequence does per personality. A behaviour change updates the matrix and a
   test in the same commit.
3. **Cells hold Unicode.** Every personality decodes to code points on input
   (Latin-1 for amiga, CP437 for pcansi, UTF-8 for xterm); the renderer maps code
   points to the font. Colour resolution per personality lives in one function,
   `vt_resolve_colors`, and nowhere else.
4. **One reachability test** drives `XCON:` on the rig (see the plan); every other
   behaviour is tested in the engine suites on the host.

## Commands

| Task | Command |
|------|---------|
| Host tests (CI) | `make test` |
| One suite | `make test ONLY=<name>` (e.g. `ONLY=xterm`) |
| Amarchy keymap: parser, user overrides, clash refusals (Left Amiga + M/N, ctrl), matcher, menu tree, viewer rows (host) | `make test ONLY=keymap` |
| Amarchy commodity stays under its 32 KB resident target (needs the Amiga build, not part of `make test`) | `make amarchy-size` |
| Amarchy on a rig: the commodity's hotkeys, ARexx port (STATUS, RELOAD, RUN, SHOW, QUIT), Exchange Disable; the key table through the handler; the quick menu and key viewer (rig up, `make amiga amarchy-menu`; `UPTERM_RIG=<n>`) | `python3 tools/rig/amarchy_rig.py`, `python3 tools/rig/keymap_rig.py`, `python3 tools/rig/amarchy_menu_rig.py` |
| Amarchy docs and release wiring (README key list generated from dist/keymap, lesson, man page, kit rows, startup block, RULES rows; run by `make dist-check`) | `python3 tests/test_amarchy_docs.py` (README list: `python3 tools/keymap_doc.py`) |
| The .ipk feed for the opkg-based upkg (P10.4): one .ipk per add-on row of dist/packages.txt (the base is not a package; data under ./T/upkg-stage/<archive>, preinst holds needs-base) plus Packages and Packages.gz in build/ipk (postinst runs the archive's own Files/install.dos; `IPK_REQUIRE=1 python3 tests/test_dist_ipk.py` holds the feed to packages.txt and the files, also run by `make dist-check`) | `make dist-ipk` |
| The renderer's draw wiring without a rig: render/amiga_render.c itself against a fake RastPort (tests/render_host/gfx_fake.c logs every graphics.library call and draws Text/RectFill/ScrollRaster into a pixel canvas; NDK structs from `VTCON_NDK`; tests/test_render.c: the scrollback blit draws only the exposed rows, the cursor glyph under jump scroll) | `make test ONLY=render` (own binary `build/render_host`; a new renderer behaviour with a draw-call effect gets its test here, not a rig step) |
| Reference diff vs libvterm + pyte | `make test-ref` (needs `make venv` once; clones libvterm into `build/`) |
| Re-record streams from real programs | `make capture` |
| pcansi vs DCTelnet term-engine on BBS art | `make te-diff` (`DCTELNET=`, `ART=`; `build/te_diff -f file` names the first diverging byte) |
| vttest on the rig, screen by screen against the engine | `python3 tools/rig/vttest_rig.py --out <dir> [vttest-mX-sYY.80x24.bin ...]` (rig up; needs `make amiga`) |
| terminfo proof: recapture with TERM=vtcon, check | `make test-terminfo` |
| Amiga build with the handler trace (RAM:vtcon.log) | `make amiga DEBUG=1` |
| The `amiga` MCP server for Claude Code on the Mac (wasabi's wasabi_mcp.py, pinned in tools/nas/wasabi.rev, stdlib only; `.mcp.json` runs tools/rig/wasabimcp.py; default rig 1 (port 1234), `UPTERM_RIG=2` for rig 2, `AMI_HOST`/`AMI_PORT`/`AMI_TOKEN` (or `WASABI_HOST`/`WASABI_PORT`/`WASABI_KEY`) for a real Amiga; approve the server in Claude Code on first start; see tools/rig/README.md) | `make wasabi` (remove: `make wasabi-clean`) |
| Reachability test on the rig (V3) | `make test-rig` (rig up first) |
| UPDemo BENCH on the rig, frames a second per scene, UP-Term against CCON in the same window (S2; recorded in build/rig/updemo/<label>-<tag>.txt, recorded runs skipped, `--force` again) | `python3 tools/rig/updemo_rig.py [upterm|ccon128b9|ccon128b10 ...] [--tag T] [--force]` (rig up, `rig.py install` after `make amiga`; CCON's handlers and mount files in VTC:). One scene alone: `VTC:UPDemo BENCH <n>` in an XCON: window; with a `make amiga PROF=1` handler its close writes the frame split (render, rows, fills, composed-frame blits or planar copies, the planar copies' waits for the beam, vt_feed) to build/rig/serial.log as `PROF` lines |
| 256-colour cube on the rig, every cell vs the xterm palette | `python3 tools/rig/cube_rig.py` (rig up; handler + vsh in VTC:, kit not installed) |
| GNU screen on the rig: 256 colours inside screen, colours done in 20 s | `python3 tools/rig/screen_rig.py` (rig up; VTC:screen from $UPTERM_ROOT/screen-amiga/src, kit not installed) |
| Userland ports on the rig: each package's check cases through vsh, diffed against the Mac's outputs (0.5; verdicts in build/rig/userland/, passed ones skipped) | `python3 tools/rig/userland_rig.py [--only <pkg> [--case <name>]] [--failed] [--force]` (rig up; `make build/amiga/vsh`; `make <pkg>` and `make host-<pkg>` in `$UPTERM_ROOT/upterm-ports`, `UPTERM_PORTS=` to move it) |
| vsh against bash 5, probe by probe (host; oracle `$BASH_ORACLE`, default /opt/homebrew/bin/bash, refuses bash < 5; verdicts in build/bashdiff/, diffs in build/bashdiff/diff/) | `make bashdiff [ONLY=<area>] [PROBE=<name>] [FAILED=1]` (`make build/vsh_host` is the host shell, `build/vsh_host --hits` prints the SH_HITS counters; the `make test` gate is `python3 tools/bashdiff.py --gate`, ratchet probes only; probes in tests/bash/probes/AREA/NAME.sh, classes in tests/bash/divergences.txt and ratchet.txt) |
| The bash probes on the rig (a second case source; expected outputs from `make bashdiff`; not run yet) | `python3 tools/rig/userland_rig.py --bash <area> [--case <name>]` (rig up; `make build/amiga/vsh`) |
| One bash probe again and again on the rig, for a failure that comes now and then (each pass recorded in build/<rig>/userland/repeat/<label>.log, resumes; a hang gets `VTC:taskdump` (every task's state, signals, saved stack) and the open windows saved, then a reboot) | `python3 tools/rig/repeat_rig.py --bash <area> --case <name> [--passes 100] [--text '...' --label <l>] [--fresh] [--go-on]` (rig up; `make build/amiga/vsh build/amiga/taskdump`; `VSH=` another binary) |
| vsh pipelines with a builtin or function stage end (subshell on malloc memory; each case detached, 20 s watchdog; restart the rig after a FAIL) | `python3 tools/rig/vshpipe_rig.py` (rig up; `make build/amiga/vsh`, `VSH=` another binary) |
| Add-ons on a stock Workbench with no UP-Term kit (K2-1: C:Claude, amiga-pi, Neovim, Python 3 in a ROM CON: and a KingCON window, three LIBS: states, TERM unset or vtcon; verdicts in build/<rig>/standalone/, recorded ones skipped; python sessions press Ctrl-C at the REPL: an ixemul without the Y1 sendsig fix resets the emulated machine there, the script restarts the rig) | `UPTERM_RIG=<n> python3 -I tools/rig/standalone_rig.py [--only <session>] [--failed] [--force] [--list] [--cleanup]` (rig started `rig.py start --os 32 --max`; `make build/amiga/Claude`, `make -C $UPTERM_ROOT/amiga-pi amiga`, `make dist`) |
| The real opkg-based `upkg` on the rig (P10.5-5) against build/ipk/ served over http by this Mac: version (sentinel "based on opkg 0.9.0"), update+list, install, upgrade (a bumped nvim feed), remove, needs-base and bad checksum refused; config UP-Term:etc/upkg.conf checked equal to dist/upkg.conf, run with `-f T:upkg-rig.conf`; verdicts in build/<rig>/upkg/, passed ones skipped | `UPTERM_RIG=<n> python3 tools/rig/upkg_rig.py [--only <case>] [--failed] [--force] [--list]` (rig started `rig.py start --max`, base installed from the new kit; `make dist dist-ipk` first) |
| A program under `/bin/sh -c` or `vsh -c` at a vsh prompt reads the console (sed, cat, the read builtin; titled windows, typed line, Ctrl-\\ ends it; ctrld-*: Ctrl-D ends it under vsh, sends a partial line) | `python3 tools/rig/shstdin_rig.py [case ...]` (rig up, handler installed; `make build/amiga/vsh`, the kit's sed built in `$UPTERM_ROOT/upterm-ports`) |
| The console's history file: an old ENVARC:vtcon.history moved to UP-Term:var/vtcon/history, a vsh window's lines in its HISTFILE only (rig scripts that type into vsh open it with `tools/rig/vshwin.py`: HISTFILE=RAM:rig.history) | `python3 tools/rig/histfile_rig.py` (rig up, current handler and vsh installed; host: `make test ONLY=histfile`) |
| ps and top (Unix tool ports 3.3; top's CPU from a 50 Hz VBlank sampler, proc/vbsample.s): ps lists input.device, idle under 15 %, a busy loop 85 % or more, the VBlank server gone after -n, Ctrl-C and q | `python3 tools/rig/proc_rig.py` (rig up; `make build/amiga/ps build/amiga/top build/amiga/busyloop build/amiga/intlist`; host: `make test ONLY=proc`) |
| A pipe Read answers with what the pipe holds (PTY: pipe, vsh's pipes; PIPE: does not) | `make build/amiga/pipeprobe build/amiga/pty-handler`, `rig.py install`, then on the rig `VTC:pipeprobe PTY:pp/w PTY:pp/r` prints `read 4096: 6` (`VTC:pipeprobe` alone: PIPE:, `no answer`) |
| FIONREAD on a vsh pipe: the bytes it holds, 0 at end of file (`echo y \| cat -n` printed nothing; ixemul `__fioctl.c`) | `make build/amiga/ixfionread`, copy it and ixemul ($UPTERM_ROOT/ixemul-vtcon `sh docker/build.sh`) into VTC:, reboot the rig, then on the rig `VTC:vsh -c "echo hello \| VTC:ixfionread"` prints `... 5, at end of file 0: PASS` |
| Pipes into vfork + exec children (screen printcmd path), no IXPIPE: requester | `python3 tools/rig/ixpipe_rig.py` (fresh rig boot for the requester part) |
| PTY: on the rig (P5, self-checking) | `python3 tools/rig/ptytest_rig.py` (rig up; `make amiga` first) |
| BSD ptys via patched ixemul (P6.4, self-checking) | `python3 tools/rig/ixpty_rig.py [--orig]` (rig up; `make amiga build/amiga/ixpty`; ixemul from $UPTERM_ROOT/ixemul-vtcon `sh docker/build.sh`) |
| Signal mask across ixemul's startup stack extension (self-checking; A/B on gcc 16 libraries) | `[IXEMUL=<ixemul.library> IXNET=<ixnet.library>] python3 tools/rig/stackext_rig.py` (rig up; `make build/amiga/ixstackext`) |
| SIGWINCH and TIOCGWINSZ after an XCON: resize (W47, self-checking) | `[IXEMUL=<ixemul.library>] python3 tools/rig/winch_rig.py` (rig up, handler installed; `make amiga build/amiga/ixwinch`) |
| Characters typed after a Ctrl-C that ended a read (a child killed by ^C must not eat the next line; self-checking) | `[IXEMUL=<ixemul.library>] python3 tools/rig/intr_rig.py [line|raw|vsh]` (rig up, handler installed; `make amiga build/amiga/ixintr`) |
| tmux and Neovim follow a resized XCON: window (self-checking) | `python3 tools/rig/resize_rig.py [tmux] [nvim]` (rig up, fresh boot, handler installed; `make amiga build/amiga/ixwinch`; tmux-amiga and `make dist` in neovim-amiga) |
| Fullscreen size change keeps the window the whole screen (FULLSCREEN-MENU; verdicts in build/rig/fullscreen/, passed ones skipped) | `python3 tools/rig/fullscreen_rig.py [--rerun <case>|all]` (rig up, handler installed; `make amiga`) |
| pty-handler with a serial trace (build/rig/serial.log) | `make build/amiga/pty-handler DEBUG=1` |
| amiga-pi on the rig, one numbered step per plan phase (CON: scripts; step 13 types into an UP-Term window and reads the screen back as text through the clipboard; verdicts of step 13 in build/<rig>/amigapi13.verdicts.json, passed checks skipped, `--force` runs all) | `UPTERM_RIG=3 AMIGA_PI_BIN=<amiga-pi build/amiga/amiga-pi> python3 -I tools/rig/amigapi_rig.py --step 13 [--force]` (rig up, handler installed; `make amiga build/amiga/snipprobe` here, `make amiga` in `$UPTERM_ROOT/amiga-pi`; mathieeedoubbas.library is in the rig's DH0:Libs, so LIBS: has it) |
| Glyphs above U+00FF on the rig (UT1: UP-Term: assigned to VTC:Apps, the Unifont pages in VTC:Apps/unifont; "?" and U+280B must draw differently) and a selection ending where output overwrites it (UT5) | `python3 tools/rig/glyph_rig.py` (rig up, handler installed; `make amiga build/amiga/snipprobe`, `make unifont`, `rig.py install`; `rig.py setup` once for the assign) |
| MuForce on a rig (crash feedback: hit reports with PC, address, task in `build/<rig>/vtc/muforce/hits.log`; needs the MMU rig and the NDK's MuTools + mmu.library from the Mac, nothing of them in the repo; `tools/rig/README.md`) | `python3 tools/rig/rig.py start --max --mmu`, then `python3 tools/rig/rig.py muforce on|off|log [--raw]`; probe `make build/amiga/muhit`, `ami.py exec VTC:muhit`; host test `make test ONLY=rig_muforce` |
| wasabi on a rig: call trace (snoop), KPrintF output (debug), Guru report, tasks with stack headroom (ps); which tool answers which question, MuForce included: tools/rig/README.md "Debugging on a rig" (logs in build/rig<N>/wasabi/; rig N on port 1234 + N - 1; host test `make test ONLY=rig_wasabi`) | `make wasabi` once (pinned clone + Bebbo build in build/wasabi; `make wasabi-clean`), then `python3 tools/rig/rig.py wasabi on|off|snoop start|stop|log|debug start|stop|log|guru|ps` (rig up; `UPTERM_RIG=<n>`) |
| UP-Term Prefs on the rig (PX15: every gadget of the table in the window, screen mode / font / colour requesters, KingCON, Save / Use / Cancel and the Discard request, profiles New / Rename / Delete, an open window taking a new colour live; ids read from prefs/prefs_ui.h, the page of each from prefs/prefs_ui.c; the asl.library requesters of Prefs and of the handler's Tab / Font / Theme are the one module prefs/prefs_asl.c) | `UPTERM_RIG=3 python3 tools/rig/prefs_rig.py --ui` (rig up, `make build/amiga/upprefs`; host: `make test ONLY=prefs`) |
| Rig (FS-UAE A1200, wasabid; what it needs, flags, what each script proves: `tools/rig/README.md`) | `python3 tools/rig/rig.py setup|start|install|stop|status` (start boots the 68060 at full speed; `--a1200` the stock 68020 at real speed, timing runs only; stop waits for the running command and flushes before it quits FS-UAE); drive with `tools/rig/ami.py` |
| Set up the whole workspace from zero (host tools, toolchains, repos, kit, rig, real Amiga, NAS) | `$UPTERM_ROOT/upterm/README.md`, section "Set up the whole thing" |
| Cross build (vbcc, 68020) | `make amiga`. Needs the AmigaOS 3.2 SDK headers once: unpack `NDK3.2R4` to `vendor/ndk-3.2r4-Include_H` (gitignored), or pass `make amiga VTCON_NDK=<path-to-Include_H>` |
| Cross build for 68000 (engine only, DCTelnet's case) | `make amiga CPU=68000` |
| hl and mdv for the host terminal (build/hl, build/mdv; suites `ONLY=hl`, `ONLY=md`) | `make view-host` |
| Install kit, every archive (kit split K2, `dist/packages.txt`: base `up-term.lha` + `up-nvim` (needs the base) and the stand-alone `Claude`, `amiga-pi`, `Python3`, and `up-term-src.lha`; each with Install with Installer, or Files/install.dos) | `make dist` (one archive per `dist/packages.txt` row, in `build/`) |
| One archive | `make dist-<id>` (id = `python3 tools/pkgs.py names`: base, claude, pi, nvim, python) |
| Archives checked: each lists Install, Uninstall, Files/VERSIONS; per-archive manifest, versions, modes | `make dist-check` (every archive; runs tests/test_dist_versions.py too) |
| Install kit on the rig: Install, check, Uninstall (cases listed at the top of tools/rig/install_rig.py; verdicts recorded, passed cases skipped) | `UPTERM_RIG=<n> python3 -I tools/rig/install_rig.py [--only <case>] [--failed] [--force]` (rig up; `make dist build/amiga/iconprobe build/amiga/wbrun` first); add-ons and migration from an old kit through the Installer: `python3 -I tools/rig/installer_rig.py ONLY addon-<id>` / `ONLY migrate` |
| Which add-on works without UP-Term (ROM CON: and KingCON, one verdict per add-on and console; K2-1) | `python3 -I tools/rig/standalone_rig.py [--failed]` (rig up; `make dist`) |
| hl and mdv on the rig (highlight-markdown R1: `hl --list`, line numbers and colours, a 100 KB timing, mdv's table fitted to the window and again after a resize, a ROM CON: window; hlp/mdp page through the kit's less: `userland_rig.py --only less` first) | `UPTERM_RIG=<n> python3 -I tools/rig/hlmd_rig.py [listing startup timing mdv pager romcon]` (rig up on the RTG screen: `rig.py rtg` if the Workbench is PAL; `rig.py install`; `make amiga build/amiga/snipprobe`) |
| Help > Demo tour opens a tab, the tour plays and the tab closes, by Esc and at its own end (D1) | `UPTERM_RIG=<n> python3 -I tools/rig/demo_rig.py [key|end]` (rig up, handler installed; `end` waits up to `TOUR_SECS`, default 300) |
| Shell tutorial on the rig: the 13 lessons' `--selftest` in vsh (12 with the fake window state, 13 needs tmux), the Help > Shell tutorial and `/tutorial` tabs, lesson 1 typed, lesson 12 done for real in the tab (counters move, T:tutorial removed), `:quit` then `next` resumes, progress file, first-run hint, the all-finished prompt (verdicts in build/rig<n>/tutorial.verdicts.json, recorded passes skipped) | `UPTERM_RIG=<n> python3 -I tools/rig/tutorial_rig.py [--only <case>]... [--force]` (rig up, kit installed; cases: selftest-01..13, hint-before, typed, menu, slash, lesson12, lesson1, progress, resume, hint-after, check-a..check-d) |
| UPDemo's scroll-region and tmux scenes read back as pixels: every palette colour of the scene on 20 or more pixels, with `--hog` after VTC:rgbprobe took every free pen of the 24-bit Workbench (W-T11 / W-T13) | `UPTERM_RIG=<n> python3 -I tools/rig/demo_colours_rig.py [--hog] [6] [7]` (rig up on the RTG screen, handler and UPDemo in VTC:; `make build/amiga/rgbprobe`, copy it to VTC:) |
| A program vsh starts after a slash command sees it: `/term xterm-256color` gives TERM, `/colors rgb` gives COLORTERM=truecolor, `/colors 16` gives TERM=xterm-16color (vtcon-amiga-16color in an AMIGA window) with no COLORTERM, and both 16-colour terminfo entries are in VTC:terminfo (W46) | `UPTERM_RIG=<n> python3 -I tools/rig/slashenv_rig.py` (current `make dist` kit installed with `rig.py install`, rig rebooted) |
| The emulator's FPU keeps Inf: `fmove.x` of +/-Inf stores exponent 7fff with a zero mantissa, `fcmp` Inf,Inf sets Z, NaN compares unordered (stock FS-UAE 3.1.66 on arm64 fails; the patched build at fix/arm64-fpu-inf passes) | `make build/amiga/fpuinf`, then `UPTERM_RIG=<n> python3 -I tools/rig/fpu_inf_rig.py` |
| Launch an icon as a Workbench double-click does (Directory Opus replaces Workbench on the rigs and the A1200, so OpenWorkbenchObjectA and C:WBRun fail): `wbrun NAME` resolves a project's default tool, uses the icon's stack and TOOLPRI; the test opens SYS:Utilities/Clock and SYS:System/Shell and checks each new window in wasabi's window list | `make build/amiga/wbrun`, then `UPTERM_RIG=<n> python3 -I tools/rig/wbrun_rig.py` (the A1200: AMI_HOST/AMI_PORT/AMI_TOKEN as for ami.py) |
| C:uptelnet and vshrc's telnet() against tools/uptelnetd.py on this Mac (password not echoed, stty size = window, resize follows, Ctrl-] ends) | `UPTERM_RIG=<n> python3 -I tools/rig/uptelnet_rig.py [amigados|vsh]` (rig up; `make amiga build/amiga/ixwinch build/amiga/uptelnet`; it copies the build to C:uptelnet) |
| upkg on the rig: the real opkg binary, cases version, updatelist, install, upgrade, remove, needsbase, badsum against build/ipk/ (P10.5-5; see the upkg_rig row under Commands) | `UPTERM_RIG=<n> python3 -I tools/rig/upkg_rig.py [--only <case>] [--failed] [--force]` (rig up with the base installed; `make dist dist-ipk`) |
| Aminet readme of one archive (Short at most 40 characters, Version and Requires from `dist/packages.txt`; `--release` refuses while a placeholder is left) | `python3 tools/mkreadme.py <id> [--release]` writes `build/<archive>.readme`; `make test ONLY=packages` |
| Width tables (engine/vtwidth.h) from the Unicode database, glibc's wcwidth rules | `make widths` (`UNICODE=16.0.0`; fetches the UCD into build/ucd once) |
| Mac end for uptelnet (A1, LAN only, unencrypted; password in ~/.config/uptelnetd/password, 0600) | `python3 tools/uptelnetd.py [--command 'tmux new -A -s claude claude']` |
| Its tests alone | `make test ONLY=uptelnetd` |
| Re-record Claude Code's screen with the engine answering (asks the model once) | `make build/vtreply && python3 tools/capture_claude.py --dir <a directory Claude Code trusts>` |
| uptelnet alone (Roadshow headers: third_party/roadshow-netinclude, or `VTCON_NETINCLUDE=`) | `make build/amiga/uptelnet` |
| Claude client (A2) host suites | `make test ONLY=claude_repl` (the reachability test, line mode, A3 screen, the A4 tools, A4 print mode and command line; also `claude_http`, `claude_json`, `claude_stream`, `claude_tools`, `claude_match`, `claude_cli` (both argument syntaxes)) |
| Print mode's golden output written again (after a deliberate format change; check the diff) | `make build/vttest_host && CL_UPDATE_GOLDEN=1 build/vttest_host claude_repl` (tests/claude/print_*) |
| C:Claude's command line | `Claude --help`, `Claude ?` (the AmigaDOS template); `Claude -p "prompt" --output-format json`, `Type file \| Claude -p explain`, `Claude PRINT OUTPUT-FORMAT=json prompt` |
| C:Claude's screen (A3): keys, editor, golden screens on the engine | `make test ONLY=claude_tui` (`CL_DUMP=1 build/vttest_host claude_tui` prints the screens) |
| C:Claude cross build | `make build/amiga/Claude` (in `make amiga`). Roadshow's netinclude (NDK3.2R4 `SANA+RoadshowTCP-IP/netinclude`) is in the tree at `third_party/roadshow-netinclude` (or `VTCON_NETINCLUDE=`). https needs the AmiSSL 5 SDK: unpacked to `vendor/amissl-5.27/` it is found by itself (`gh release download 5.27 -R jens-maus/amissl -p AmiSSL-5.27-SDK.lha`, `lha x`), else `AMISSL_SDK=<AmiSSL 5 SDK dir>`; without it the build refuses https |
| AmiSSL layer, OpenSSL half, on the host | `make claude-tls-check` (Homebrew OpenSSL 3; `OPENSSL_INC=`) |
| Recorded Claude answers for the rig (no key, no Anthropic; its routes pinned by `make test ONLY=claude_fixture`) | `python3 tools/claude_fixture.py`, then on the Amiga `Claude URL=http://<this Mac>:8080/v1/messages ROOT=SYS:` (prompts with "startup": Read + Glob; "edit": TodoWrite, Read, Edit of RAM:claude-test.txt, start with `ROOT=RAM:`; "grep", "search the web", "fetch", "agent", "question", "plan", "background", "monitor", "schedule", "time limit" (Bash moved to the background at its time limit), "slow" (Bash Wait 30: Ctrl+B, Esc), "loop test" (type `/loop loop test`: ScheduleWakeup every minute, wakeups shown as "Claude resuming /loop wakeup", quiet ones in a row folded into one line, Esc cancels the next): one A4 tool each -- the script's docstring lists them) |
| Clean | `make clean` |

## Cross-repo changes

A change that crosses two or more UP-Term repos is one commit per repo, all with the
same subject line, plus one `repos.lock` update in the `upterm` meta-repo (re-pin with
`bin/upterm-bootstrap --update`) carrying that subject line too. Release step:
`upterm-bootstrap --update`, then `make dist` in vtcon; run `bin/upterm-doctor` first.

## Release steps (kit archives)

Each archive has its own `<major>.<minor>` in `dist/packages.txt` (first release 1.0; an
add-on names the lowest base version it needs in the same table). Aminet readmes are made
from `dist/aminet/<id>.readme.in` by `tools/mkreadme.py`.

1. Bump `dist/packages.txt`: the changed archive's minor (major when an add-on needs
   something the base did not have before). Base first when an add-on needs new base
   behaviour, then that add-on's `needs-base`.
2. Pins: `bin/upterm-doctor`, then `bin/upterm-bootstrap --update` in `$UPTERM_ROOT/upterm`, commit
   `repos.lock` (see Cross-repo changes). `amiga-pi` is pinned there too.
3. `make dist dist-ipk dist-check` (and `make test`): `dist-check` needs the ipk feed
   (`build/ipk/`), which `make dist` does not build.
4. Rig: the cases of the changed archives only (`install_rig.py --only <case>`, never the
   whole set by reflex); the owner's real-Amiga check stays manual.
5. Fill what the readmes still leave open: `python3 tools/mkreadme.py <id> --release` fails
   while any `PLACEHOLDER` is left (today: the UP-Term licence line, ledger K4; the uploader
   address).
6. Upload `up-term.lha` and `up-term-src.lha` together, then the add-ons, each with its
   `build/<archive>.readme`. Directories (owner 2026-10-13): base and source `comm/term`,
   up-nvim `text/edit`, Python3 `dev/lang`, Claude and amiga-pi
   `comm/misc`. The upload is the owner's.
7. GitHub release for upkg (the opkg binary, an UP-Term:bin command): one release per base
   version, holding every archive of `dist/packages.txt` and `build/upkg-index.txt` (written
   by `make dist`, checked by `dist-check`) from the same `make dist`, so upkg's
   `releases/latest/download/` URL finds them together. The release goes to the release-only
   repo spotUP/vtcon (the feed URL in `dist/upkg.conf`; v1.0 kits read it), not spotUP/UP-Term,
   and carries the opkg feed too: `gh release create v<base version> -R spotUP/vtcon --latest
   build/upkg-index.txt build/*.lha build/ipk/*.ipk build/ipk/Packages build/ipk/Packages.gz`.
   `make dist` deletes every `build/*.lha` that `dist/packages.txt` and the source archive do not list
   (`tools/pkgs.py prune`, printed), so the glob above holds the current archives only.
   Tag the built commit `v<base version>` in spotUP/UP-Term. Never an index without its
   archives: upkg refuses a download whose size or sha256 differ from the index.
