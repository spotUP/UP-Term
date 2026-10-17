---
date: 2026-10-17
topic: FP1.10 P7 iTerm2 cell-level comparison (objective numbers only)
status: measured; verdict "good enough" is the owner's
---

# iTerm2 vs vtcon engine, cell by cell, 80x24

Method: `tools/iterm_compare.py record` (screen 2 splits + copy-mode scrollback; tmux 2 panes + copy-mode
scrollback; chat log via tools/chatsim.py) -> `build/p7/*.80x24.bin`; `replay` cats each into its own new iTerm2
window set to 80x24 and writes a vtdump-format grid; `table` runs `build/vtdump` on the same bytes and diffs via
`tools/refdiff.py` (`iterm_columns`). Colours are palette index / default / rgb in the engine's encoding, so the
theme is not compared. Only the visible 80x24 screen at the end of each stream is compared, not scrollback lines.

| scenario | cells compared | text differs | attribute differs (bold/underline/inverse) | colour differs after mapping | cursor differs | top differing cells (row,col: vtcon / iTerm2) |
|---|---|---|---|---|---|---|
| screen, 2 splits, scrollback | 1920 | 0 | 0 | 2 | 1 (x same 9; y 22 vs 94) | colour (1,0) and (4,0): fg none / fg default |
| tmux, 2 panes, scrollback | 1920 | 303 | 52 | 250 | 0 | text (12,0) ' ' / '➜'; (12,1) '#' / ' '; (12,3) 'm' / 'v'; attr (12,0) bold no / yes; colour (12,0) (none,4) / (2,256); (12,1) (7,4) / (none,256) |
| chat-style log | 1920 | 3 | 0 | 86 | 0 | text (22,0) ' ' / '9'; (22,1) '[' / ']'; colour (0,27..32) bg 4 / bg default (53 cells on row 0) |

## Where they sit
- tmux: all text differences are rows 12-22 of the left pane (cols 0-39). vtcon shows the chat log
  ("#amiga: vtcon test channel", "12:33 <lars> ok"); iTerm2 shows the shell prompt `➜  vtcon git:(main) ✗`
  followed by empty cells (NUL) on row 12 and empty cells on rows 13-22. The right pane agrees. Cause not
  investigated (engine vs iTerm2 handling of some sequence in the tmux stream; unverified which one is right).
- chat log: row 0 background colour 4 vs default (53 cells, a title bar), row 22 text and colour (31 cells).
- Colour on blank cells: engine reports fg none for a blank, iTerm2 reports fg default (256); shows as the 2
  colour cells on rows 1 and 4 in every scenario. No visible effect.
- Cursor row for screen: iTerm2's `cursor_coord.y` was 94 (scrollback-absolute), engine 22; treat as a reading
  artefact of the capture, unverified. Column matches.
- Not compared: font, theme colours, cursor shape, bold-as-bright; the capture reads the cell styles only.
