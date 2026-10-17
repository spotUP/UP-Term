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

**Correction 2026-10-17:** the tmux and chat-log rows above came from a replay with ONLCR on (see "tmux left pane cause"). The harness now runs `stty -echo -opost` (tools/iterm_compare.py `replay_command`, regression test in tools/test_iterm_columns.py). Rerun with -opost: tmux text 0, attribute 0, cursor 0, colour 77 (blank-cell fg none vs default); chat-log text 0, attribute 0, cursor 0. Chat-log row 0 colour cells are EL-erased cells that the iTerm2 API cannot report (style_at None), not an engine difference.

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

## tmux left pane cause

Verdict: capture artifact in the replay harness. The engine is right; iTerm2 is not wrong either. Not an engine bug, no
code changed.

- Cause: `tools/iterm_compare.py replay` plays the stream with `cat` into a pty whose line discipline still has OPOST/ONLCR
  on, so every bare LF (0x0A) is delivered to iTerm2 as CR LF. tmux writes to a raw tty and relies on bare LF keeping the
  column. The left pane's last repaint is a column-42 run `ESC[13;42H ESC[K LF ESC[K LF ...` (stream offset 17003,
  right pane, 10 rows); with LF turned into CR LF each `ESC[K` ran from column 0 and erased the left pane's row.
  The earlier shell prompt on row 12 (drawn at offset 8587 when the pane was still full width) was what stayed.
- Evidence 1, bisect on iTerm2 with stream prefixes (scratchpad `bis.py`): left rows 12-22 are correct up to offset
  17020 (cut after the first `ESC[K LF`), row 13 goes blank at 17023, rows 13-22 blank by 17063.
- Evidence 2, same bytes in pyte: left pane shows the chat log, cols 0-39 rows 12-22, same as vtdump.
- Evidence 3, replay with `stty -echo -opost` (patched copy of the harness, results in scratchpad, tools/ not changed):
  tmux scenario text differs 303 -> 0, attributes 52 -> 0, cursor 0; chat-log text differs 3 -> 0 (the row 22 diff was
  the same LF issue); screen scenario unchanged. Remaining tmux colour cells (77) are the known blank-cell fg none vs
  default (256) reading difference.
- Fix for the harness (not applied, owner decides): `stty -echo -opost` in the replay command in
  `tools/iterm_compare.py` (line ~87). The numbers in the table above for tmux and chat-log are invalid until then.
- pyte raises on `CSI ? 996 n` unless its `report_device_status` is overridden; tmux live re-run was not needed.

### Chat-log row 0 colour (engine bg 4 vs iTerm2 default, cols 27+)
The title bar is `ESC[37m ESC[44m text ESC[K`: cols 0-26 are text, cols 27-79 are cleared by EL with background 4 (BCE).
Probe in iTerm2 (scratchpad `bce.py`): after `ESC[44m ab ESC[K` the API returns `style_at(x) = None` and `string_at`
raises IndexError for the EL-cleared cells, while explicitly written spaces report bg 4. So the difference is the
iTerm2 Python API not exposing erased-cell colour past the written length (harness maps None to default). It is not
evidence that iTerm2 renders them default; whether iTerm2 paints them blue was not checked visually (unverified).
The engine behaviour matches xterm BCE and the existing quirk-bce-erase-colour stream. The 53 cells on row 0 are this
API limit, not an engine difference.

## FP1.10b: three more scenarios (vim, man through less, 256 colour / truecolor)

Added to `SCENARIOS` in tools/iterm_compare.py (same record / replay / table path, replay with `-opost`):
`p7-vim-c-scroll` (vim, `+syntax on`, engine/vtengine.c, three page-downs, half-page down/up, `/scroll_up`, one more page;
alt screen + scroll region), `p7-man-ls` (`man ls` through less, MANWIDTH 80, two pages down, one back; bold and
underline via overstrike turned into SGR by less), `p7-colour256` (tools/colorsim.py: htop is not installed; 16 standard,
6x6x6 cube as bg and fg, grey ramp, three truecolor gradients, 38;5 / 38;2 / colon-form SGR, bold, underline, inverse,
dim, italic, strike). Visible 80x24 grid at the end of each stream, same columns as above.

| scenario | cells | text differs | attr differs | colour differs | cursor differs | what the nonzero cells are |
|---|---|---|---|---|---|---|
| vim on a C file, syntax colour, scrolled | 1920 | 0 | 0 | 60 | 1 | colour: all 60 are blank cells (rows 0,1,2,4,5,11,12), engine fg none vs iTerm fg default (256). Same class as the 2 cells in screen-split and the 77 in tmux: engine does not keep a foreground on a blank, iTerm reports default. Not visible (a blank draws only its bg). Harness/dump convention, not an engine bug. Cursor: x same (4); y 0 vs 117, iTerm API reports a scrollback-absolute row. Consistent with iTerm keeping lines scrolled off the alt screen in its history (default setting); unverified, scrollback length not read |
| `man ls` through less | 1920 | 0 | 0 | 0 | 1 | cursor: x same (1); y 23 vs 69, same absolute-row reading artifact (offset 46 = lines that left the screen). Bold and underline (attr 0 of 1920) and colours agree on every cell |
| 256 colour + truecolor screen | 1920 | 0 | 0 | 6 | 0 | colour: all 6 on row 11 cols 37-42, the `SGR 7;38;5;27` run ("inv 27"). Engine keeps fg 27 + inverse flag; iTerm reports the resolved colours (fg default, bg 27) with the inverse flag also set. API convention (refdiff.visible handles the same swap for blanks); the cells paint identically. Col 40 is the space inside the run: engine bg 27 (swapped), iTerm bg default. Unverified whether that is the API dropping the style of the blank or iTerm drawing it unswapped; needs a screenshot. Every 256-palette, truecolor, colon-form, bold, underline, dim, italic, strike cell elsewhere matches |

No engine bug found in these three scenarios (text 0 and attribute 0 on all 5760 cells). The only open item is the
inverse-blank cell (row 11 col 40), unverified. Run: `build/iterm2-venv/bin/python tools/iterm_compare.py record|replay|table`
(`pip install pyte` into that venv for `table`).
