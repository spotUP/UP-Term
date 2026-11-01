# upkg-ui loses its top frame row (2026-10-22)

## Cause (measured)
upterm-ports commit 8206576, `leave_alternate_screen()` (pkgs/upkg-ui/src/upkg-ui.c:734-746, called at :984).
newterm sends smcup (`ESC[?1049h` in vtcon.terminfo:85); ncurses then clears and believes the cursor is at 0,0.
upkg-ui then writes rmcup (`ESC[?1049l`) itself, which restores the cursor to where the shell left it
(the prompt row). ncurses is never told: its first row (the top border, row 0) goes out WITHOUT a cup,
so the 100 cells of the border are printed at the shell's cursor position, wrap (am) and are overwritten /
scrolled; row 0 of the window keeps shell text. Every later row is positioned with cup (`ESC[2;1H` ...), so
the rest of the frame is right, only row 0 is lost. Not (a), (b) or the clear: size 30x100 is honoured, and the
clear happens inside the alternate screen, which rmcup throws away.

## Evidence (host reproduction, no rig needed)
Scratch: /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/87a17433-32bd-494e-bac5-37c13673dc52/scratchpad/topRow/
- cap.py: runs build/host/upkg-ui/upkg-ui --menu on a pty 30x100, TERM=vtcon, compiled vtcon terminfo, saves bytes.
- Byte stream starts: `ESC[?1049h ESC[1;30r ... ESC[H ESC[2J ESC[?1049l ESC[?1h ESC= ESC[?25l ESC[39;49m ESC[36m ESC[40m <U+2554 U+2550 ...>`
  i.e. no CUP between 1049l and the first border glyph; first CUP in the stream is `ESC[2;1H`.
- Fed to host engine (cc tests/dump_main.c engine/vtengine.c -Iengine -> vtdump 100 30), with a shell history
  `line1 / line2 / line3 / shell> ` written before the stream: grid row 0 = "line1", row 1 = masthead
  ("║ //// T ..."), row 2 = the T-junction rule. Top border U+2554 is absent from the grid. With the cursor
  at 0,0 (no preceding output) the same stream gives the border on row 0 (control case, o3.bin).
- With the cursor on the last row (typical for a real shell window) the border is printed there and scrolls the
  window: shell text stays visible at the top, masthead on row 1 (o4.bin). Matches the photo.
- The "a" on the bottom row in the dumps is the menu result printed after endwin by my Enter key press, not a frame fault.

## Fix level
In upkg-ui, not the engine (the engine's 1049l restoring the cursor is correct xterm behaviour).
After the rmcup in leave_alternate_screen, make ncurses and the terminal agree on the cursor: either
emit `ESC[H` after rmcup (tputs(cursor_home)) and `clearok(stdscr, TRUE)`/`clear()`, so the first refresh
clears and repaints from 0,0 on the normal screen; or avoid smcup altogether by setting
`SP->_..`-free route: unset `smcup/rmcup` before newterm via a private TERMINFO copy / `setupterm` + `del_curterm`
edit. Simplest correct: after the fputs(rmcup) also `fputs(tigetstr("home"))`, then `clearok(curscr, TRUE)`
(note the current comment says "The next refresh clears the screen" but only ncurses' idea of the screen is
cleared, not the terminal's cursor).
Design question for the owner: on the normal screen the Shell output above is wiped by the clear. If the
intent is to keep scrollback context, draw from the current row instead; as written, a full-window UI clearing
the window is the usual dialog behaviour.

## Regression test
tests (upterm-ports, host): run upkg-ui on a pty (TERM=vtcon, size 30x100) after writing `line1\r\nline2\r\nsh> `
into the same stream, feed the whole byte stream to the vtcon host engine (vtdump), assert grid row 0 starts with
U+2554 and row 1 has the masthead. Fails before the fix (row 0 = "line1"), passes after. Sentinel: also assert the
stream contains `ESC[H` (or a CUP) between `ESC[?1049l` and the first U+2554.
