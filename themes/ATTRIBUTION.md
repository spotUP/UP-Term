# Attribution

The 112 profiles in this directory are other people's work. They are not part
of vtcon and carry no vtcon copyright.

## Where they came from

Downloaded from [terminalcolors.com](https://terminalcolors.com/), which
collects each scheme in five formats (Terminal.app, iTerm2, Alacritty, Warp,
Ghostty) and publishes no licence of its own — its `/license/` and `/terms/`
pages serve the home page. The downloads are free, but "free to download" is
not the same as a grant to redistribute, so treat this directory as a
convenience copy rather than as the authoritative source.

Each file was converted from the site's Warp `.yaml` export by
`tools/theme_import.py`; the colour values are unchanged.

## The authors

Every scheme here was written by somebody else and is published by them under
its own licence. Nord is by Arctic Ice Studio, Dracula by Zeno Rocha,
Gruvbox by morhetz, Solarized by Ethan Schoonover, Catppuccin by the
Catppuccin org, Tokyo Night by enamded, and so on. Most are MIT or OFL, which
allow redistribution provided the licence and copyright notice travel with
the work.

**Check an individual scheme's licence at its upstream repository before
redistributing vtcon with this directory included.** The table below gives
where each one is developed; the licence is in that project's own LICENSE
file. Nothing here overrides those terms, and none of them are vtcon's to
relicense.

| Profile | Upstream |
|---------|----------|
| ayu-* | https://github.com/ayu-theme |
| catppuccin-* | https://github.com/catppuccin/catppuccin |
| dracula-* | https://github.com/dracula/dracula-theme |
| everforest-* | https://github.com/sainnhe/everforest |
| gruvbox-* | https://github.com/morhetz/gruvbox |
| kanagawa-* | https://github.com/rebelot/kanagawa.nvim |
| moonfly-* | https://github.com/bluzrush/synthwave-84-schemes |
| nord-* | https://github.com/nordtheme/nord |
| one-* | https://github.com/atom/one-light-syntax |
| rose-pine-* | https://github.com/rose-pine/rose-pine |
| seoul256-* | https://github.com/milksigit/Seoul256 |
| solarized-* | https://github.com/altercation/solarized |
| tokyo-night-* | https://github.com/enamded/tokyo-night-theme |
| zenbones-* | https://github.com/zenorocha/zenbones.nvim |

Themes with no entry above follow the same pattern: the scheme's home page on
terminalcolors.com links to its repository.

## If that is too much

Delete this directory. Nothing in vtcon depends on it — a profile is a
readable text file, and `tools/theme_import.py` rebuilds the lot from a fresh
download in one loop. Users who want a theme convert it themselves.