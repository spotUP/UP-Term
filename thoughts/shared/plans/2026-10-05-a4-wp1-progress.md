---
date: 2026-10-05
topic: A4 WP1 -- C:Claude input and screen (rows 1.1-1.12 of plans/2026-10-05-a4-claude-parity.md), progress ledger
tags: [claude, a4, wp1, tui]
status: implemented
---

# A4 WP1 progress (branch feature/a4-wp1-input off main a95ec55)

Done = each row built, golden-screen host test on the engine (tests/test_claude_tui.c), and !, @, #,
type-ahead, Ctrl+O driven through the REPL core (tests/test_claude_repl.c test_wp1). make test,
make test-ref, `timeout 900 make amiga` before each commit. Not merged; rig check is the main session's.

## Decisions (made; do not re-litigate)

- Claude Code's docs (interactive-mode, fetched 2026-10-05) win over the plan's wording:
  Ctrl+T is "toggle the task checklist" (up to five todos under the box), not thinking. Thinking:
  the spinner says "Thinking", a dim "* Thinking..." line marks it in the transcript, the text
  itself is in the Ctrl+O transcript view (Claude Code's place for it).
- Inline Ctrl+R (classic renderer): footer row "(reverse-i-search)`q': match"; Ctrl+R again older;
  Tab/Esc accept and edit; Enter accept and send; Ctrl+C cancel (original input back); Backspace on
  an empty query cancels. Searches every project's entries, newest first, duplicates collapsed.
- History file ENVARC:Claude/history: one JSON object a line {"display":..,"project":..}, newest
  last, capped at 500 lines; rewritten whole on submit (cl_sys has no append), never per key.
  Up/Down recall this project's entries (Claude Code: per working directory).
- Esc Esc: text in the box -> cleared and kept in history; empty box -> the rewind menu.
- `!` on an empty box: bash mode (prompt "!", frame in the bash colour); Esc/Backspace/Ctrl+U on
  an empty box leaves it. The command runs through cl_sys run, output under the corner, and the
  context gets <bash-input>/<bash-stdout> text, then Claude answers (respondToBashCommands default).
- `#` at the start: "# note" asks where (project / user memory) and appends "- note".
- Type-ahead: Enter while a turn runs queues the box's text (shown dim under the spinner);
  plain messages go into the same turn after the tool round (conv_add_user_text on the
  tool_result message) else after the turn; commands and ! one at a time after the turn; Up on
  the first row takes them back; Esc stops the turn and the queue is sent next.
- Ctrl+O: the transcript viewer on the alternate screen (?1049), every line as logged in full
  (results unfolded, thinking text); Up/Down/PgUp/PgDn/Home/End, Ctrl+R or / search, n next;
  q, Esc, Ctrl+C, Ctrl+O leave. One-line scrolls use SU/SD + one row.
- Ctrl+L: clear, the last screenful of transcript rewritten from a ring the screen keeps, footer.
- Ctrl+G / Ctrl+X Ctrl+E: the box's text to T:claude-prompt.txt, io->edit (main: $EDITOR via
  GetVar, else Ed) with raw mode off, read back.
- Notifications: BEL + OSC 9 when a turn that took >= 10 s ends and when a permission menu opens;
  OSC 2 title "Claude" / "* Claude - working" / "Claude - needs your permission".
- Themes: Claude Code's names (dark, light, dark-daltonized, light-daltonized, dark-ansi, light-ansi) + monochrome; one cl_theme of
  SGR strings read by tui.c and show.c.
- vim: /vim toggles; INSERT/NORMAL, "-- INSERT --" on the status line; motions h j k l w e b 0 $ ^
  gg G f F t T ; , ; edits x r dd D dw de db cc C cw ce cb s S yy Y p P J u, counts; Ctrl+_ undo in
  both modes.

## Checklist

- [x] 1.1 history persisted (ENVARC:Claude/history, JSONL, per project) + Ctrl+R inline search
- [x] 1.2 @path mentions (file text <= 64 KB, directory listing) + Tab completion list
- [x] 1.3 ! bash mode (box mode, cl_sys run, <bash-input>/<bash-stdout> to Claude, Claude answers)
- [x] 1.4 # memory (box mode, "where" menu, "- note" appended)
- [x] 1.5 type-ahead queue (dim rows under the spinner, into the same turn after a tool round, Up takes back)
- [x] 1.6 Ctrl+O transcript viewer (alternate screen, results whole, thinking, search)
- [x] 1.7 Esc Esc: clear draft / rewind menu (UI + conversation-only stub; code restore is WP3's)
- [x] 1.8 vim mode (/vim, editorMode setting)
- [x] 1.9 thinking: "Thought for Ns (ctrl+o to show thinking)", text in the viewer; Ctrl+T = todo list (docs)
- [x] 1.10 /theme (7 themes incl. monochrome), theme setting
- [x] 1.11 bell + OSC 9 + OSC 2 title (turns >= 10 s, permission questions), title stack kept
- [x] 1.12 Ctrl+L (ring of the transcript's last lines), Ctrl+G / Ctrl+X Ctrl+E (io->edit)

Running count: 12 of 12 built and host-tested; rig check (main session) open for all.

## Not done (deliberate, say so in the report)

- vim: visual mode (v V), '.', quote/bracket text objects.
- Ctrl+Enter / Ctrl+X Ctrl+S "send queued now"; Alt+Y paste ring; Ctrl+S stash; Ctrl+B background.
- the @ list opens on Tab only (no list per keystroke: a directory read per key is slow on a floppy/HD Amiga).
- the transcript viewer ignores a window resize while open.
- thinking text needs the request to ask for it: conv_body must send
  "thinking":{"type":"adaptive","display":"summarized"} (Opus 5.5 default display is "omitted": empty
  text, so only the spinner's "Thinking" shows). Not on Haiku 4.5 (budget_tokens model). WP3/WP4.

## Interfaces for WP3 (stubs here; ui.h "A4 (WP1)" block)

- settings: set r->ui.setting(su, key) / r->ui.set_setting(su, key, value) / r->ui.su before
  repl_screen(). Keys "theme" (theme.h names: dark light dark-daltonized light-daltonized dark-ansi
  light-ansi monochrome) and "editorMode" ("vim" / "normal"). Read once in ui_attach; written by
  /theme and /vim.
- memory: r->ui.memory_files(mu, cl_memfile *out, max) lists the files "# note" may go to (label,
  path); r->ui.memory_changed(mu, path) after an append (reload the system prompt). Stub: project
  <root>/CLAUDE.md (AMIGA.md when only that exists), user ENVARC:Claude/CLAUDE.md.
- rewind: r->ui.rw = { u, count, label, can (RW_CONV|RW_CODE), restore(i, what) }; points are the
  user prompts, oldest first. Stub (input.c input_rewind_stub): conversation-only, conv_rollback to
  before prompt i; ui_attach installs it only when rw.count is unset. After a conversation restore
  the prompt goes back into the box. WP3 should also reset ctx/session after restore.
- history path: r->ui.histfile (main_amiga.c sets ENVARC:Claude/history).

## repl.c touch points (all small)

- repl_line: ui_input() first (!, #, @, /theme, /vim) -> IN_SEND turn(r, out) / IN_DONE return.
- turn(): after a tool round's results, ui_take_queued() -> conv_add_user_text (queued prompts).
- post(): sui.thinking = st_thinking (stream.h got an optional thinking callback).
- repl_screen: ui_attach(&r->ui, r->sys, r->tools.root, &r->conv).
- cmds[]: /theme, /vim rows; help text split in two strings (C89 509-char limit).

## Rig steps (main session, tools/claude_fixture.py)

1. `Claude URL=http://<mac>:8080/v1/messages ROOT=RAM:`; type a prompt, quit, start again: Up shows it;
   Ctrl+R + letters finds it (match in reverse, "(reverse-i-search)" row).
2. `!` on an empty box: frame turns magenta, prompt "!"; `! dir` runs, output under the corner.
3. `# test note` -> menu -> Enter: RAM:CLAUDE.md has "- test note".
4. `@S` + Tab: completion list; `@S/Startup-Sequence` + Enter: "Read S/Startup-Sequence (N lines)".
5. While a fixture answer streams: type + Enter: dim "> ..." row under the spinner, sent after.
6. Ctrl+O: alternate screen with the transcript, q back; Ctrl+T todo list; Ctrl+L redraw;
   Ctrl+G opens Ed (or $EDITOR) with the box text; Esc Esc on empty box: rewind menu.
7. /theme -> Light mode; /vim, then Esc, 0, w, dw, i.
8. A turn > 10 s: the window bells, title "Claude" again.

## Log

