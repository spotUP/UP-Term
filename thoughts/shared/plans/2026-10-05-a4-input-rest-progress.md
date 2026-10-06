---
date: 2026-10-05
topic: A4 input rest -- what WP1 left out of C:Claude's keys, progress ledger
tags: [claude, a4, wp1, tui, vim, keys]
status: implemented
---

# A4 input rest (branch feature/a4-input-rest off main d3683b4)

Source: plans/2026-10-05-a4-wp1-progress.md "Not done", and code.claude.com/docs/en/interactive-mode
(fetched 2026-10-05). Done = built, golden-screen host test in tests/test_claude_tui.c, the
reachability path driven once through the REPL core (tests/test_claude_repl.c test_input_rest).
make test, make test-ref, `timeout 900 make amiga` before each commit. Not merged; rig check is the
main session's. Parallel agents: WP4 (main_amiga.c), wiring (repl.c ext, tui.c status line row):
tui.c's status_row is left alone except the vim mode word (one condition).

## Decisions (made; do not re-litigate)

- Ctrl+B = Claude Code's: a foreground Bash (the tool, and a `!` command) becomes a background
  shell (bash_N, WP2's shells.c) and the tool returns at once. Mechanism: with the screen, a
  foreground command runs as a shells.c job the tool waits on (tools.wait -> ui_wait -> tui_wait
  reads keys every 100 ms); Esc / Ctrl+C sends it a break (as before), Ctrl+B leaves it running
  under its id. Without the screen (line mode, tests' tools suite) sys->run as before.
  Idle (no command running), Ctrl+B stays the editor's "back one character".
- Ctrl+Enter (kitty CSI 13;5u, modifyOtherKeys 27;5;13~) and Ctrl+X Ctrl+S = "send queued now":
  the draft is queued behind the queue; a `!` queued first -> the turn is interrupted; a command
  running in the foreground -> moved to the background (same turn reads the messages); else the
  turn is interrupted (Esc's path) and the queue goes next. In bash mode the key only queues.
  Ctrl+Enter is no longer a newline (Shift/Alt+Enter still are).
- Ctrl+S: text in the box -> stashed (text, cursor, box mode) and the box cleared; empty box ->
  the stash back. A second stash replaces the first.
- Alt+Y: a ring of the last 8 kills (Ctrl+K/U/W, Alt+D, Alt+Backspace); right after Ctrl+Y or
  Alt+Y it replaces the pasted text with the next older kill.
- @ list opens and narrows while typing; each directory's listing cached in the screen for the
  prompt being typed (one disk read per directory per prompt; a submit drops the cache).
  Tab / Enter take the selected row (Enter sends when the row is what is typed); Esc closes it
  until the text changes. Bash mode: a token holding '/' gets the same list; Tab on a token
  without one completes from earlier ! commands of this project.
- Ctrl+O viewer: a resize while open re-reads the size, keeps the top line, redraws.
- vim: visual v / V (motions extend; d x y c s p r ~ u U > < J o, text objects, v/V toggle or
  leave, Esc leaves), the selection in reverse in the box, "-- VISUAL --" / "-- VISUAL LINE --"
  where "-- INSERT --" is; `.` repeats the last change (its keys recorded; a count replaces the
  original count; a visual change repeats over the same extent from the cursor); text objects
  iw aw iW aW i" a" i' a' i( a( i) a) ib ab i[ a[ i] a] i{ a{ i} a} iB aB (counts on w objects).

## Checklist

- [x] R1 vim visual mode v / V (test_claude_tui vim_visual_dot; expected strings are vim 9.1's)
- [x] R2 vim `.` repeat (same test)
- [x] R3 vim quote / bracket text objects, counts (same test)
- [x] R4 Ctrl+Enter / Ctrl+X Ctrl+S send queued now (keys_rest)
- [x] R5 Ctrl+S stash (keys_rest)
- [x] R6 Alt+Y paste ring (keys_rest)
- [x] R7 Ctrl+B background (Bash tool and ! commands, shells.c jobs) (keys_rest, test_input_rest)
- [x] R8 @ list as you type, cached per directory (+ bash mode path list and ! history Tab) (at_completion)
- [x] R9 Ctrl+O viewer follows a resize (transcript_resize)
- [x] R10 other keys from the docs that were missing (list below)
- [x] R11 reachability through the REPL core (test_claude_repl test_input_rest: Ctrl+B on Claude's Bash
      and on a ! command, Alt+P without an echo)

- [x] R12 (owner, mid-run: "the cute claude mascot") the start screen as Claude Code's: the mascot
      (block elements, accent colour) beside "C:Claude for the Amiga", model, directory
      (welcome_mascot). The owner's screenshot (Image #19) did not reach this agent: built from
      Claude Code's compact start header as known; the boxed "Welcome back / Tips / Recent activity"
      variant is NOT built -- owner to say which one the screenshot shows.

Running count: 12 of 12.

## Other keys in the interactive-mode docs (R10)

Built:
- [x] `?` on an empty prompt: the shortcuts panel under the box (any key closes it)
- [x] Alt+P: the model picker, the draft kept (busy: queued like /model; Claude Code runs it at once
      -- our REPL has no re-entry mid-turn)
- [x] Shift+Tab on a file's permission question: "Yes, and don't ask again this session"
- [x] bash mode: Tab completes from earlier ! commands of the project; a token with '/' gets the file list
- [x] Tab on a permission question: a comment field on Yes / No (No with a comment: declined with it,
      the turn goes on; Yes with one: added to the call's result) (keys_rest, test_claude_tools)
- [x] vimInsertModeRemaps setting ("jj" -> Esc within one second; user settings and --settings only)
      (test_claude_config, vim_visual_dot)

Not applicable here (reason):
- Ctrl+X Ctrl+K: stops background subagents; C:Claude's subagents run in the foreground only (no threads).
- Ctrl+V / Alt+V image paste: no image path to the API (plan's 1.x N/A).
- Ctrl+Z: Unix job control; no such thing in an AmigaDOS shell.
- Left/Right on dialog tabs: C:Claude's menus have no tabs.
- Alt+T extended thinking: no effect on Opus 5.5 (the default), Sonnet 5.5, Fable (docs); thinking
  requests are WP3/WP4's (the WP1 ledger's note).
- Alt+O fast mode: a research-preview speed mode of the hosted service; not in the API C:Claude uses.
- Ctrl+T inside /theme (syntax highlighting toggle): show.c has no syntax highlighting to toggle.
- Transcript viewer `?`, `{` `}`, `[`, `v`: fullscreen renderer only (C:Claude is the classic one);
  Ctrl+E "show all" (classic): the viewer already shows everything in full, nothing is folded.
- Voice dictation (Space), `:` emoji shortcodes (no emoji in the fonts; owner rule: no emoji).
- Ctrl+B pressed twice under tmux: no tmux on the Amiga.

## Unverified (said so, not folded in)

- The tool result text for Ctrl+B ("Command was manually backgrounded by user with ID: bash_N") and
  for comments ("the user declined this tool call and said: ...") are written from memory of Claude
  Code's wording, not checked against its source.
- Amiga: Ctrl+C inside the raw console arrives as a key (handled) or a break signal (io->sleep
  returns 1 -> stop); both paths stop the command; only the host saw them.

## Rig steps (main session, tools/claude_fixture.py)

1. `slow` prompt -> Bash "Wait 30" asks; Yes; spinner; Ctrl+B -> result "manually backgrounded ...
   bash_1" at once; BashOutput later. Again with Esc -> "The user stopped the command".
2. `! Wait 30` then Ctrl+B -> "Moved to the background as bash_N."
3. /vim, type "one two three", Esc, `0wviwd` -> "one  three"; `u`; `x` then `.`; "-- VISUAL --" shows.
4. Type "@S/" -> list appears without Tab, narrows per key; a floppy/HD reads the dir once.
5. Ctrl+S with text: box empties, hint; Ctrl+S again: back. Ctrl+W twice, Ctrl+Y, Alt+Y cycles.
6. During a fixture answer: type, Ctrl+X Ctrl+S -> answer stops, the queue goes. `?` on empty box.
7. Ctrl+O, resize the window, the viewer redraws to the new size. Alt+P -> model picker, draft kept.
8. A permission question: Tab on Yes, type a comment, Enter.
9. Start: the mascot in the accent colour at the top.

## Log

- 2026-10-05: R1-R9, R11 built; make test, make test-ref (149 streams), make amiga green (c0aa1b2).
- 2026-10-05: R10 rest (Tab comments, vimInsertModeRemaps), R12 mascot start header, fixture "slow".
- 2026-10-05: main 3513693 (wiring, WP4, gaps) merged in. Conflicts: tools.h (wait kept beside
  main's added/call/agent_* fields), tools.c run_bash (tools_run_fg with main's bashOutputMaxChars
  cap, then main's persistent cd), ui.c (No with a comment -> ASK_NO kept; main's 4th option
  ASK_PROJECT kept), tui.c (vim_label + main's hideVimModeIndicator), repl.c (main's repl_ask
  route kept; tool_wait kept). repl_ask got note/cap; policy.c's ask-rule question now takes
  Tab's comment too (test_claude_repl test_rule_ask_comment; fails with the note dropped).
  Gate: make test (43 OK), make test-ref (149, 0 failed), timeout 900 make amiga rc 0.
