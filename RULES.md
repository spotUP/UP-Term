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
| Reference diff vs libvterm + pyte | `make test-ref` (needs `make venv` once; clones libvterm into `build/`) |
| Re-record streams from real programs | `make capture` |
| pcansi vs DCTelnet term-engine on BBS art | `make te-diff` (`DCTELNET=`, `ART=`; `build/te_diff -f file` names the first diverging byte) |
| vttest on the rig, screen by screen against the engine | `python3 tools/rig/vttest_rig.py --out <dir> [vttest-mX-sYY.80x24.bin ...]` (rig up; needs `make amiga`) |
| terminfo proof: recapture with TERM=vtcon, check | `make test-terminfo` |
| Amiga build with the handler trace (RAM:vtcon.log) | `make amiga DEBUG=1` |
| Reachability test on the rig (V3) | `make test-rig` (rig up first) |
| 256-colour cube on the rig, every cell vs the xterm palette | `python3 tools/rig/cube_rig.py` (rig up; handler + vsh in VTC:, kit not installed) |
| GNU screen on the rig: 256 colours inside screen, colours done in 20 s | `python3 tools/rig/screen_rig.py` (rig up; VTC:screen from ~/Code/screen-amiga/src, kit not installed) |
| Userland ports on the rig: each package's check cases through vsh, diffed against the Mac's outputs (0.5; verdicts in build/rig/userland/, passed ones skipped) | `python3 tools/rig/userland_rig.py [--only <pkg> [--case <name>]] [--failed] [--force]` (rig up; `make build/amiga/vsh`; `make <pkg>` and `make host-<pkg>` in `~/Code/upterm-ports`, `UPTERM_PORTS=` to move it) |
| Pipes into vfork + exec children (screen printcmd path), no IXPIPE: requester | `python3 tools/rig/ixpipe_rig.py` (fresh rig boot for the requester part) |
| PTY: on the rig (P5, self-checking) | `python3 tools/rig/ptytest_rig.py` (rig up; `make amiga` first) |
| BSD ptys via patched ixemul (P6.4, self-checking) | `python3 tools/rig/ixpty_rig.py [--orig]` (rig up; `make amiga build/amiga/ixpty`; ixemul from ~/Code/ixemul-vtcon `sh docker/build.sh`) |
| Signal mask across ixemul's startup stack extension (self-checking; A/B on gcc 16 libraries) | `[IXEMUL=<ixemul.library> IXNET=<ixnet.library>] python3 tools/rig/stackext_rig.py` (rig up; `make build/amiga/ixstackext`) |
| SIGWINCH and TIOCGWINSZ after an XCON: resize (W47, self-checking) | `[IXEMUL=<ixemul.library>] python3 tools/rig/winch_rig.py` (rig up, handler installed; `make amiga build/amiga/ixwinch`) |
| pty-handler with a serial trace (build/rig/serial.log) | `make build/amiga/pty-handler DEBUG=1` |
| Rig (FS-UAE A1200, amiagent) | `python3 tools/rig/rig.py setup|start|install|stop|status`; drive with `tools/rig/ami.py` |
| Cross build (vbcc, 68020) | `make amiga`. Needs the AmigaOS 3.2 SDK headers once: unpack `NDK3.2R4` to `vendor/ndk-3.2r4-Include_H` (gitignored), or pass `make amiga VTCON_NDK=<path-to-Include_H>` |
| Cross build for 68000 (engine only, DCTelnet's case) | `make amiga CPU=68000` |
| hl and mdv for the host terminal (build/hl, build/mdv; suites `ONLY=hl`, `ONLY=md`) | `make view-host` |
| Install kit (build/UP-Term.lha: Install with Installer, or Files/install.dos) | `make dist` |
| Install kit on the rig: Install, check, Uninstall | `python3 tools/rig/install_rig.py` (rig up; `make dist build/amiga/iconprobe build/amiga/wbrun` first) |
| Width tables (engine/vtwidth.h) from the Unicode database, glibc's wcwidth rules | `make widths` (`UNICODE=16.0.0`; fetches the UCD into build/ucd once) |
| Mac end for uptelnet (A1, LAN only, unencrypted; password in ~/.config/uptelnetd/password, 0600) | `python3 tools/uptelnetd.py [--command 'tmux new -A -s claude claude']` |
| Its tests alone | `make test ONLY=uptelnetd` |
| Re-record Claude Code's screen with the engine answering (asks the model once) | `make build/vtreply && python3 tools/capture_claude.py --dir <a directory Claude Code trusts>` |
| uptelnet alone (Roadshow headers: `VTCON_NETINC=`, default DCTelnet's copy) | `make build/amiga/uptelnet` |
| Claude client (A2) host suites | `make test ONLY=claude_repl` (the reachability test, line mode, A3 screen, the A4 tools, A4 print mode and command line; also `claude_http`, `claude_json`, `claude_stream`, `claude_tools`, `claude_match`, `claude_cli` (both argument syntaxes)) |
| Print mode's golden output written again (after a deliberate format change; check the diff) | `make build/vttest_host && CL_UPDATE_GOLDEN=1 build/vttest_host claude_repl` (tests/claude/print_*) |
| C:Claude's command line | `Claude --help`, `Claude ?` (the AmigaDOS template); `Claude -p "prompt" --output-format json`, `Type file \| Claude -p explain`, `Claude PRINT OUTPUT-FORMAT=json prompt` |
| C:Claude's screen (A3): keys, editor, golden screens on the engine | `make test ONLY=claude_tui` (`CL_DUMP=1 build/vttest_host claude_tui` prints the screens) |
| C:Claude cross build | `make build/amiga/Claude` (in `make amiga`). Needs Roadshow's netinclude once: copy `NDK3.2R4/SANA+RoadshowTCP-IP/netinclude` to `vendor/ndk-3.2r4-netinclude` (or `VTCON_NETINCLUDE=`). https needs the AmiSSL 5 SDK: unpacked to `vendor/amissl-5.27/` it is found by itself (`gh release download 5.27 -R jens-maus/amissl -p AmiSSL-5.27-SDK.lha`, `lha x`), else `AMISSL_SDK=<AmiSSL 5 SDK dir>`; without it the build refuses https |
| AmiSSL layer, OpenSSL half, on the host | `make claude-tls-check` (Homebrew OpenSSL 3; `OPENSSL_INC=`) |
| Recorded Claude answers for the rig (no key, no Anthropic) | `python3 tools/claude_fixture.py`, then on the Amiga `Claude URL=http://<this Mac>:8080/v1/messages ROOT=SYS:` (prompts with "startup": Read + Glob; "edit": TodoWrite, Read, Edit of RAM:claude-test.txt, start with `ROOT=RAM:`; "grep", "search the web", "fetch", "agent", "question", "plan", "background", "monitor", "schedule", "time limit" (Bash moved to the background at its time limit), "slow" (Bash Wait 30: Ctrl+B, Esc), "loop test" (type `/loop loop test`: ScheduleWakeup every minute, wakeups shown as "Claude resuming /loop wakeup", quiet ones in a row folded into one line, Esc cancels the next): one A4 tool each -- the script's docstring lists them) |
| Clean | `make clean` |

## Cross-repo changes

A change that crosses two or more UP-Term repos is one commit per repo, all with the
same subject line, plus one `repos.lock` update in the `upterm` meta-repo (re-pin with
`bin/upterm-bootstrap --update`) carrying that subject line too. Release step:
`upterm-bootstrap --update`, then `make dist` in vtcon; run `bin/upterm-doctor` first.
