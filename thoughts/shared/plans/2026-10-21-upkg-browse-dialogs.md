---
date: 2026-10-21
status: approved 2026-10-21 (second review recorded; Phase 0 done; Phase 1 next)
ledger: thoughts/spot/todos.md BROWSE-DIALOGS
release: 1.7 (base only)
---

# upkg browse: dialogs inside the browser, coloured rows

## Goal (owner's words)

"i dont want to leave the browser when i manage files in it, i want it to open dialogs with info and
actions keyboard navigatable". Plus colour per state: upgrade yellow, installed green, new cyan,
available in the normal text colour.

## Owner decisions (2026-10-21, fixed)

- Order: this plan first (1.7), before the bootstrap split and before the splits NS3.3 hunt.
- "new" = in the feed now and not in the list before the last `upkg update` (opkg patch 0023, `.prev`). First update ever: nothing new.
- Enter on a row: info box (scrollable `upkg info` text) with a button row of only the actions that apply. The i, r, u hotkeys stay on the list.
- Install, upgrade, remove output: programbox inside the frame. A package whose script is interactive runs in the plain window, marked by a control field. Phase 0 surveys whether any such package exists.

## Owner decisions, second review (2026-10-21, fixed)

- Live output: opkg patch 0024 makes upkg output line buffered through a pipe, so the programbox shows progress while the command runs.
- "new" lasts until the next `upkg update` (the `.prev` compare as above). No "seen" file.
- After the programbox OK: back to the list, cursor on the same row, lists and colours refreshed. Not back to the info box.
- The programbox always ends with an [OK] or [ERROR] line and an OK button; it never closes by itself.
- A new row says `new 1.3` instead of `available 1.3`, so a mono terminal shows the same state as colour.
- No "upgrade all" key in 1.7; `upkg upgrade` in the Shell covers it.
- Stray `q` after info (A1200, 1.5): the owner re-tests on 1.6. Still there: fix it with a check/keys regression test before Phase 1.

## What exists

- upkg-ui (upterm-ports/pkgs/upkg-ui/src/upkg-ui.c) is ncursesw. Widgets: --menu, --yesno, --inputbox
  (strcmp chain at :490, enum at :31). Colour attrs built by make_attr (:53) in init_theme (:65). The menu
  row draws a_sel or a_ink, item text a_dim (draw_menu :269). --yesno is two fixed buttons (:256, :368).
  Esc exits 255 before any widget (:447). No input flush. Tests: check/cases (usage), check/tty +
  check/keys/* (scripted keys on a pty), test_chrome (unit).
- dist/upkg-browse runs upkg-ui, exits it, then runs `upkg CMD` in the plain window and waits for
  Return ("Press Return to go back to the list."). Messages ("already installed") go to the Shell text.
- opkg keeps one Packages file per feed in VARDIR/lists, overwritten by `upkg update`. Nothing records
  which packages are new.

Full map: scratchpad upkg-ui-map.md (copy into thoughts/shared/research/ before phase 1).

## Design

Every screen stays inside upkg-ui's frame (title, backtitle, rail). upkg-browse stays the driver: it
calls upkg-ui once per screen, as today, so no curses nesting and no new long-lived process.

### New upkg-ui widgets (dialog-compatible names and arguments)

| Widget | Use in browse | Answer |
|---|---|---|
| `--msgbox TEXT H W` | "up-vim is already installed.", "[OK] upkg install up-vim" | exit 0 on Return/Space, 255 on Esc |
| `--textbox FILE H W` with `--buttons "Install,Back"` | Enter on a row: the `upkg info` text, scrollable (Up Down PgUp PgDn Home End), a button row moved with Left Right Tab | button label on stderr, exit 0; Esc 255 |
| `--programbox TEXT H W` | install, upgrade, remove: the command's output scrolls in a box, then an OK button | exit 0 |
| `--colors` (menu) | coloured rows | dialog's `\Z` codes in item text: `\Z3` yellow, `\Z2` green, `\Z6` cyan, `\Zn` reset |

The info box offers only the actions that apply: available `Install, Back`; installed `Remove, Back`;
upgrade `Upgrade, Remove, Back`. Remove keeps its yes/no question (No default).

`--buttons` generalises draw_yesno/key_yesno to an index over N labels; --yesno becomes `Yes,No` on
the same code, so its keys and look stay as they are (check/keys cases prove it).

### Colour of a row

upkg-browse prefixes the item text: upgrade `\Z3`, installed `\Z2`, new `\Z6`. upkg-ui with --colors
maps the digit to a make_attr pair on the panel background. Without colour (mono terminal) the row
keeps its state word, so nothing is lost.

### "new"

An opkg patch (0023): `upkg update` copies each feed's old Packages to `<feed>.prev` before writing the
new one. A new command `upkg list-new` prints the packages in the lists and not in any `.prev`. First
update ever (no .prev): nothing is new. browse reads `list-new` with the other three lists.

### Install output and package scripts

Decided (owner, programbox with fallback): `--programbox` reads the command's output through a pipe. A package postinst that
draws its own full screen or asks a question would break inside the box. Phase 0 checks every
postinst/prerm in the feed (upterm-ports pkgs, dist/addons). If none is interactive: programbox always.
If some are: those packages run in the plain window as today (a `UP-Term-Interactive: yes` control
field), the rest in the box.

### Typed keys while the script is busy

The A1200 1.5 report (`q` printed in the Shell after info) is re-checked on 1.6 first. If it stays:
fault and regression test in check/keys before the redesign starts (the fixing-bugs route), not as part
of it.

## Phases

0. Spikes: postinst survey (above); `\Z` parse cost on a 68030 for 7 to 100 rows; does `upkg install`
   output through a pipe still flush line by line on the Amiga (programbox needs it).
1. upkg-ui: --buttons (yesno on it), --msgbox, --textbox, --programbox, --colors. check/keys case per
   widget; check/cases for usage errors.
2. opkg patch 0023 + `upkg list-new`; check script in pkgs/opkg/check.
3. upkg-browse: drive the new screens, no `pause`, no Shell text. tests/test_upkg_browse.py: FAKE_UI
   learns the new widgets; a test that the browser never prints to the window between screens.
4. Reachability: upkg_browse_rig.py steps become Enter on up-vim, Install in the info box, programbox
   OK, Remove in the info box, Yes, q. Sentinel UP-Term:pkg/up-vim/Uninstall as today. Colour checked
   from a screen dump of cell attributes, not a screenshot.
5. Release 1.7 (base only), owner's A1200 run.

## Success criteria

Automated: upkg-ui check/keys cases pass for every widget; test_upkg_browse passes; the rig case
installs and removes up-vim with no Shell text between screens (window text dump).
Manual (owner, A1200): every action from the list without leaving the frame; colours as chosen.

## Phase 0 results (2026-10-21)

Postinst survey: done, see "Postinst survey (2026-10-21, from source)" below.

Spike B (measured, Replay 68030, DH1:UP-Term/bin, vsh -c): `upkg download up-python 2>&1 | while read l; do echo $(date +%T) $l; done`.
Start 18:06:24; the .ipk file was complete in RAM: at 18:07:39; BOTH output lines ("Downloading ...", "Downloaded ...") reached the
reader at 18:08:01, the second after exit, with the first stamped the same second. The first line was printed ~75 s before it
arrived: through a pipe upkg output is block-buffered until exit. (`upkg update` through the same pipe: two lines, same second,
no gap to see.) No setvbuf anywhere in pkgs/opkg/patches; only fflush(st->fp) in the wrap code of patch 0010 (flush_one), which
runs at exit, before a child runs, and before the other stream writes. Consequence for Phase 1/3: a programbox cannot show
progress until opkg output is line buffered. Add an opkg patch (0023 is taken by list-new; use 0024 or renumber): setvbuf(stdout,
NULL, _IOLBF, 0) in upkg main, and make the wrap code in 0010 emit and fflush at every newline when the width is 0 (pipe).
Child scripts (postinst, curl) write through their own libc: check them with the same pipe test after the opkg patch.
Test to add: pkgs/opkg/check, a producer with a delay, reader timestamps, first line arrives before exit.

Spike A (estimate, NOT measured on the device): upkg-ui has no `\Z` parsing and no --colors yet (only has_colors() at :67).
upkg-ui cannot be driven from `ami.py exec` (needs a tty), and there is no m68k cross compiler on this Mac, so no timing run was
made. draw_menu (:269) draws at most `room` rows (LINES-4-y, about 20 on a 640x256 screen) per redraw whatever the entry count, so
7, 30 and 100 rows cost the same per redraw if the \Z scan is done at draw time on visible rows only: roughly 20 rows x 60 bytes
= 1200 bytes scanned, a few ms on a 68030. Scanning all 100 rows once at load (~6 KB) is the same order (under about 10 ms).
Both are small next to the ncurses refresh. Phase 1: parse \Z at draw time on the visible rows only; measure on the Replay in
the Phase 1 rig (key-scripted run, 100 rows, time from start to first draw) before relying on the estimate.

Amiga state: wrote only RAM:dl and RAM:t0-t3, all deleted. No package installed or removed; `upkg update` refreshed the list.

### Postinst survey (2026-10-21, from source)

Every add-on ipk's scripts are written by tools/pkgs.py `_scripts()`: preinst (needs-base check, prints only),
postinst (Execute the archive's Files/install.dos), prerm (Execute the record's Uninstall). None of the
add-on install.dos files (dist/addons/*/install.dos) asks anything (no Ask, RequestChoice, RequestString,
read, upkg-ui). The only interactive step is upkg-ask, which postinst runs for an archive with
Files/questions: today only up-claude (dist/addons/claude/questions); it draws upkg-ui screens.
The base's install.dos asks twice (lines 803, 868), but the base is not an ipk and browse never runs it.
upterm-ports pkgs carry no postinst/prerm of their own (only the opkg check scripts).
So: programbox for every package except one whose archive has install questions; that one runs in
the plain window as today. The flag can come from the questions themselves (pkgs.py writes
`UP-Term-Interactive: yes` into the control of an archive with Files/questions), so no hand-kept list.

## Phase 1 build design (2026-10-21, owner approved the plan; implementation brief)

File: upterm-ports/pkgs/upkg-ui/src/upkg-ui.c (one file; chrome.c only if a pure helper is needed, then with a test_chrome case).

- Buttons: one generic button row replaces draw_yesno/key_yesno. State: labels[] + bsel. Draw at x=4, each
  button " Label " (len+2), 3 cells between, selected a_sel, others a_ink (Yes/No keeps today's cells
  exactly: " Yes " at 4, " No " at 12). Keys: Left/Right move (no wrap past the ends? yesno today toggles,
  so wrap), Tab next with wrap, Enter accepts, a letter equal to a label's first letter (case-insensitive)
  accepts that button at once (keeps y/n on yesno). --yesno = labels "Yes,No", exit code = index (0/1),
  nothing on stderr, --defaultno sets bsel 1.
- `--buttons "A,B,C"`: labels for textbox and msgbox (usage error on any other widget). Answer of textbox
  and msgbox with --buttons: the chosen label on stderr, exit 0. Default labels: msgbox "OK", textbox "OK",
  programbox "OK" (no stderr answer when the default single OK is used).
- `--msgbox TEXT H W`: the prompt text wrapped as today (draw_prompt), button row below it. Space also accepts.
- `--textbox FILE H W`: read FILE (usage error, exit 2, if unreadable, before initscr), split into lines,
  each wrapped to COLS-4 with ch_wrap_words (an empty line stays one row). Text area rows 3..LINES-6, button
  row LINES-4... pick a layout that leaves the footer at LINES-2 and the rule at LINES-3. Up/Down scroll
  one row, PgUp/PgDn a page, Home/End top/bottom. Footer hints: Up/Down Scroll, Left/Right Choose,
  Enter Accept, Escape Back.
- `--programbox TEXT H W`: TEXT is a heading line (draw_prompt). Output comes on stdin (a pipe). curses
  must not use stdin: open /dev/tty and use newterm(NULL, stdout, ttyfp) + set_term for every widget
  (ixemul-vtcon supports /dev/tty, also under ptyrun). Read stdin with blocking fgets per line, append,
  wrap, keep the view at the bottom, refresh after each line (no ticks while reading). At EOF: flushinp()
  (drops keys typed while the command ran), show the OK button, then scroll keys as textbox, Enter/Space/Esc
  end (exit 0, Esc 255). Lines are stored, no limit except memory (realloc).
- `--colors` (menu): item text (and tag) may hold dialog codes `\Z0`..`\Z7` (fg = that curses colour on
  bg 0, pairs made once in init_theme with make_attr), `\Zn` reset; any other `\Zx` is stripped. Parse at
  draw time, visible rows only. Selected row stays a_sel throughout. No colour support: codes stripped.
  Without --colors the text is drawn as given (dialog's behaviour).
- Esc: 255 for every widget, as today.
- check: check/cases gains usage rows (buttons on menu, textbox missing file, programbox missing args);
  check/tty + check/keys gain: msgbox, textbox (scroll then Right then Enter, answer file), textbox letter
  hotkey, programbox (`printf 'one\ntwo\n[OK] done\n' | upkg-ui --programbox ...`, then Enter), colours menu
  (`\Z3`, `\Z2`, `\Z6` rows, Down, Enter, answer file); the existing yesno/menu/hotkey/input cases must give
  byte-identical streams to before the change (compare build/expected/upkg-ui before and after).
