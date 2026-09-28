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
| Reference diff vs pyte | `make test-pyte` (needs `make venv` once) |
| Cross build (vbcc, 68020) | `make amiga` |
| Cross build for 68000 (engine only, DCTelnet's case) | `make amiga CPU=68000` |
| Clean | `make clean` |
