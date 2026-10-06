# vtcon

Licence: all rights reserved for the own code, no licence chosen yet; see LICENSE.

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
| `view/` | `hl` (a source in syntax colours, 22 languages) and `mdv` (Markdown formatted for the window): a table-driven lexer and a renderer in portable C, AmigaDOS and host front ends (`make view-host`). |
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

No single format is best on its own, so take two. Measured over a 36-theme
set, all five agree exactly on `fg`, `bg` and all sixteen palette colours, so
colour fidelity is a tie - but they differ on two other things. The Warp
`.yaml` and Terminal.app `.terminal` exports keep the accent the author chose
for the cursor; the flat, `.itermcolors` and `.toml` exports set it to the
foreground on 28 themes of 36, which says nothing. Conversely the Warp
`.yaml` carries no selection colours at all, while the Alacritty `.toml`
does, and it is the only readable format with a theme's name in it.

So convert from the `.toml` and pass `--cursor-from` a folder of the same
themes in `.yaml`, which supplies the name and restores the accent:

```sh
python3 tools/theme_import.py --cursor-from themes Nord.toml
```

The tool only takes the fallback's cursor where this one's is merely the
foreground, so a theme that genuinely chose the foreground as its cursor
keeps it. `.terminal` is the runner-up for the accent, but one theme in 36
ships malformed XML that will not parse.

Colours are exact on a true-colour or AGA screen. On OCS/ECS a window has 16
pens and the renderer takes the nearest one per colour, so a theme whose
colours sit close together collapses - Apprentice, for one, pairs colour 2
with colour 7 at 33 levels apart and runs four greys through an 80-level
range, landing on about five distinct pens. A scheme built for 256 colours
shows its true self on 4-bit hardware.
