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

## Colour themes

A window's colours are a profile in `ENVARC:up-term/up-term` (see
`dist/README.txt`). Themes downloaded for another terminal convert to one:

```sh
tools/theme_import.py Apprentice.itermcolors      # prints the profile text
tools/theme_import.py --name night --out night.conf theme.toml
```

It reads Terminal.app (`.terminal`), iTerm2 (`.itermcolors`), Alacritty
(`.toml`), Warp (`.yaml`) and Ghostty (flat `key = value`), picking the format
from the file's content rather than its name. All five carry the same sixteen
ANSI colours and round-trip exactly through `upconf_palette_parse`. Run
`tools/theme_import.py --self-test` for the checks.

Download **`.yaml`** when a site offers a choice. Measured over a 36-theme
set: all five formats agree exactly on `fg`, `bg` and all sixteen palette
colours, so colour fidelity is a tie - but the flat, `.itermcolors` and
`.toml` exports set the cursor to the foreground on 28 themes of 36, throwing
away the accent the author picked (Dracula's pink, Nord's frost). The `.yaml`
and `.terminal` exports keep it. `.terminal` is the runner-up and carries the
same accent, but one theme in 36 ships malformed XML that will not parse.

`selection-bg` and `selection-fg` theme the selected cell; either left out
keeps that half of the flip, which is what a terminal did before. Worth
knowing: the Warp `.yaml` export carries no selection colours at all, so a
scheme downloaded from there needs them typed in by hand. The Alacritty
`.toml` and Ghostty exports do carry them, at the cost of setting the cursor
to the foreground.

Colours are exact on a true-colour or AGA screen. On OCS/ECS a window has 16
pens and the renderer takes the nearest one per colour, so a theme whose
colours sit close together collapses - Apprentice, for one, pairs colour 2
with colour 7 at 33 levels apart and runs four greys through an 80-level
range, landing on about five distinct pens. A scheme built for 256 colours
shows its true self on 4-bit hardware.
