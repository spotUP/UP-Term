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
