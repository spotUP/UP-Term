---
date: 2026-09-29
topic: vtcon -- first session, engine to a working XCON: on the rig
tags: [vtcon, console, handoff]
status: final
---

# vtcon -- handoff after the first session

## Tasks
Build a new AmigaOS 3.x (020+) console: engine with xterm/amiga/pcansi personalities, a DOS
handler (XCON:) first, a console.device replacement later, DCTelnet adopting the engine.
Ledger (read first): `thoughts/shared/plans/2026-09-28-vtcon.md`. Approved plan:
`~/.claude/plans/the-amigas-console-device-is-dreamy-karp.md`.

## Critical references
- Spec: `thoughts/shared/research/2026-09-28_console-conformance-matrix.md` (712 lines, 7 RKM errata).
- 68k ports: `thoughts/shared/research/2026-09-29_68k-unix-ports-for-testing.md` (no screen/tmux/irssi).
- Engine `engine/vtengine.c|h`; renderer `render/amiga_render.c`, `render/glyphmap.c`; handler
  `handler/vtcon_handler.c`, `handler/lineedit.c`, `handler/clip.c`.
- `RULES.md` `## Commands` for every command.

## State (all committed on main, no remote)
- Host: `make test` (7 suites), `make test-ref` (147 streams vs libvterm+pyte), `make test-terminfo`.
- Rig: `tools/rig/rig.py start`, `make test-rig` PASS. XCON: runs Shell, less, nano, BitchX.
- Speed on par with ROM CON: for `type`.
- Kit: `make dist` -> `build/vtcon.lha` (installs with Execute Install).

## Learnings (the expensive ones)
- DOS delivers the first Open to dn_Task: set it in startup, clear after that Open.
- All windows run one loaded segment: NO mutable statics in handler/render/engine.
- A handler's own DOS calls wait on its packet port: trace only when the port is empty, never
  per loop iteration (it re-armed the DOS signal and filled RAM:).
- vbcc -O2 can loop forever on a file with a compile error; build -O0 to see the error.
- Rig: ixemul there is the FPU build (fpu = 68882); ixnet needs AmiTCP:libs/usergroup.library.
- The rig window is visible on the owner's screen; stop it when done (rig.py stop).

## Next steps (ordered)
1. Owner: test the kit on the real A1200 (V4) -- steps in the chat report.
2. Owner decisions: screen/tmux port (no 68k builds; ixemul has no ptys); DCTelnet adoption (T1/T2
   touch the DCTelnet repo and its upstream PR stack); D phase starts only after V4.
3. H1 AUTO / WINDOW 0x / SIMPLE; H3 Tab completion; R1 planar fast path; E4 ROM probes Q1/Q2.
