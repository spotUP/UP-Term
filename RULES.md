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
| Pipes into vfork + exec children (screen printcmd path), no IXPIPE: requester | `python3 tools/rig/ixpipe_rig.py` (fresh rig boot for the requester part) |
| PTY: on the rig (P5, self-checking) | `python3 tools/rig/ptytest_rig.py` (rig up; `make amiga` first) |
| BSD ptys via patched ixemul (P6.4, self-checking) | `python3 tools/rig/ixpty_rig.py [--orig]` (rig up; `make amiga build/amiga/ixpty`; ixemul from ~/Code/ixemul-vtcon `sh docker/build.sh`) |
| pty-handler with a serial trace (build/rig/serial.log) | `make build/amiga/pty-handler DEBUG=1` |
| Rig (FS-UAE A1200, amiagent) | `python3 tools/rig/rig.py setup|start|install|stop|status`; drive with `tools/rig/ami.py` |
| Cross build (vbcc, 68020) | `make amiga`. Needs the AmigaOS 3.2 SDK headers once: unpack `NDK3.2R4` to `vendor/ndk-3.2r4-Include_H` (gitignored), or pass `make amiga VTCON_NDK=<path-to-Include_H>` |
| Cross build for 68000 (engine only, DCTelnet's case) | `make amiga CPU=68000` |
| Install kit (build/UP-Term.lha: Install with Installer, or Files/install.dos) | `make dist` |
| Install kit on the rig: Install, check, Uninstall | `python3 tools/rig/install_rig.py` (rig up; `make dist build/amiga/iconprobe build/amiga/wbrun` first) |
| Width tables (engine/vtwidth.h) from the Unicode database, glibc's wcwidth rules | `make widths` (`UNICODE=16.0.0`; fetches the UCD into build/ucd once) |
| Clean | `make clean` |
