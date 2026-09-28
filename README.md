# vtcon

A modern console for AmigaOS 3.x (68020+): one terminal engine with three
personalities, so that Amiga programs and Unix ports both render right.

- `amiga`  -- the ROM console.device dialect (RKM Devices, console chapter),
  including the private Amiga sequences and pen semantics.
- `xterm`  -- what ixemul/libnix ports of bash, vim, less and mc expect
  (`TERM=vtcon`, see `terminfo/`).
- `pcansi` -- ANSI.SYS / BBS art: bold means bright, iCE colours, CP437.

The same bytes mean different things in the Amiga and xterm dialects (for
example `CSI n u` sets the line length on the Amiga and restores the cursor
in xterm), so a window carries exactly one personality at a time.

| Directory | Content |
|-----------|---------|
| `engine/` | `vtengine`: parser, cell model, personalities. Portable C, no OS calls, runs on the host and on a plain 68000 (DCTelnet builds it for 68000). |
| `render/` | Amiga renderer behind the engine's damage/scroll callbacks. |
| `handler/` | `vtcon-handler`, the DOS handler (`XCON:`, later `CON:`/`RAW:`). |
| `device/` | Later: the console.device replacement. |
| `terminfo/` | The terminfo entry that matches the xterm personality exactly. |
| `tests/` | Host suites (`make test`) and rig scripts. |
