# Themes

112 colour schemes in vtcon's own profile format, one file per theme. Paste a
file's contents into `ENVARC:up-term/up-term` (or append it), then open a
window with that profile:

```
NewShell "XCON:0/20/640/300/dracula/PROFILE Dracula Default/CLOSE"
```

A window with no `PROFILE` option uses the profile named `default`. Each file
is a complete, self-contained `[profile <name>]` section:

```
[profile Dracula Default]
fg = F8F8F2
bg = 282A36
cursor-color = FF79C6
palette = 0,21222C,1,FF5555,2,50FA7B,3,F1FA8C,4,BD93F9,5,FF79C6,6,8BE9FD,...
```

Only the ANSI sixteen, the default foreground and background and the cursor
are set. Everything else - font, scrollback size, bell, cursor shape - stays
at the built-in default, so a theme changes the colours and nothing else.

All 112 set `selection-bg` and `selection-fg`, which the Warp export never
carried.

## Where they came from

Converted from the Alacritty `.toml` export of each theme with
`tools/theme_import.py`, taking the name and the cursor accent from the Warp
`.yaml` of the same theme -- the `.toml` sets the cursor to the foreground,
which carries no information. See `README.md` for the measurements behind
that. Both sources are kept: `toml/` is the export these were converted
from, and the `.yaml` files beside them supplied the name and the cursor.

## Regenerating

Point it at a folder of downloaded themes and every `.yaml` in it is rewritten:

```sh
for f in ~/Downloads/newthemes/*.toml; do
    python3 tools/theme_import.py --cursor-from themes "$f" \
        > "themes/$(basename "$f" .toml).conf"
done
```

## Licence

These schemes are the work of their authors (Nord, Dracula, Gruvbox,
Catppuccin, Solarized and so on), not of vtcon, and each carries its own
licence - mostly MIT or OFL. They are here for convenience. Check an
individual scheme before redistributing it.