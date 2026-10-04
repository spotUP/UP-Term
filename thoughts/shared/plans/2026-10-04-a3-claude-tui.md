---
date: 2026-10-04
topic: A3 -- C:Claude looks and behaves like Claude Code's terminal UI (plan and progress ledger)
tags: [claude, a3, tui, md, upterm]
status: draft
---

# A3: Claude Code's terminal UI for C:Claude

Owner 2026-10-04, comparing C:Claude with Claude Code in a terminal: "it doesnt really look like
claude cli yet. can we make it have all features like here?" Branch feature/a3-claude-tui off
main dd4762d. Builds on A2 (plans/2026-10-04-a2-native-claude-client.md, merged 98b63c8) and
closes ledger W31 and W32 (plans/2026-09-28-vtcon.md).

Done = in an UP-Term window (xterm dialect, the default), `Claude` owns the screen in raw mode:
transcript above, a framed input box with a status line below, spinner while a turn runs,
Markdown answers, tool calls with collapsed results and diffs, a permission menu, slash
commands with a completion menu, a todo checklist -- every piece asserted on engine screen
cells on the host, one reachability test driving the REPL core over the recorded streams with
a call-count sentinel on the TUI renderer.

## Decisions (made; do not re-litigate)

- Layout: a scroll region (DECSTBM 1..B) holds the transcript; the footer (spinner line, input
  box, status line, or a menu) sits fixed below it. Transcript output is whole lines written at
  the region's bottom, so the engine scrolls the region (lines leaving row 1 go to the
  scrollback, the xterm personality's rule) and the footer is never repainted for text. The
  spinner line and the edited box row are the only per-tick redraws.
- The transcript cursor row is tracked by the client (every transcript write is whole lines it
  measured), so no DECSC/DECRC; the start row comes from a DSR (CSI 6n) answer, with the
  bottom row as the fallback when no answer arrives.
- Raw mode: ACTION_VTCON_TCSETA with ld_make_raw (the termios path vsh and tmux use); the old
  termios is put back at the end. A console that refuses TCSETA (not UP-Term) and the
  one-shot PROMPT form keep the A2 line mode (ui.c's plain path stays, its tests stay).
  This closes W31: the handler's cooked line editor (and its command-word colouring) never
  sees C:Claude's input.
- Keys: the decoder takes the xterm forms (CSI/SS3, modifiers), the kitty CSI u forms (the
  client pushes CSI > 1 u: Esc and Shift+Enter become unambiguous) and modifyOtherKeys
  CSI 27;m;c~ (CSI > 4;1 m pushed too), 8-bit CSI, bracketed paste (?2004). A lone ESC at the
  end of a read is the Escape key.
- Glyphs are the code points render/glyphmap.c draws or stands in for (A1.3): U+23FA bullet,
  U+23BF result corner, U+273B/2722/2733/2736/273D spinner stars, U+276F prompt, U+23F5 mode
  marker, box drawing U+256D-2570 rounded corners. No other non-Latin-1 glyph is printed;
  model text goes through view/'s converter (Latin-1 windows get '?' only for what Latin-1
  lacks, as mdv). Colours: the 16 ANSI colours only.
- Markdown streams by block: view/md.c gets an incremental API (md_open / md_feed / md_close);
  a block is drawn when it closes. Reference definitions in a stream resolve only when they
  come before their use (the whole-document pass 1 is not possible on a stream).
- Permission modes (Shift+Tab): default (A2's rules), accept edits (write_file/edit_file inside
  ROOT run without asking), plan (only read-only tools run; others get an is_error result
  telling Claude it is in plan mode).
- "No, and tell Claude what to do differently": the tool_result says so, the round's results
  are kept (history valid), no further request; the user's next line joins that user message
  (conv_add_user_text, A2's rule).
- /compact: one extra request (no tools change) asking for a summary; a NEW conversation is
  started seeded with the summary (append-only kept: nothing earlier is edited).
- /resume: sessions are the /save JSON; the last session auto-saves after every completed turn
  to ENVARC:Claude/session.json (host tests: a path in the temp tree); /resume [FILE] loads it.
- /init writes AMIGA.md through a turn (the model explores with the tools and writes it);
  AMIGA.md in ROOT, when present, is added to the system prompt at start.
- todo_write tool (TodoWrite-style): {todos:[{content,status}]}, status pending | in_progress |
  completed; no permission (no side effect); shown as a checklist block.
- Context left: last request's input + cache read + cache write + output tokens against the
  model's window (1M for opus/sonnet/fable 5.x, 200K haiku-4-5; unknown model: 200K).

## Checklist (ticked with commit hashes)

- [x] T1 raw-mode TUI owned by Claude (TCSETA raw, restore at exit, DSR start row, scroll
      region, footer fixed); line mode kept for PROMPT=, PLAIN and non-UP-Term consoles; W31
      gone for C:Claude -- ee1d5af
- [x] T2 input box: rounded frame, prompt, multi-line (Shift+Enter, backslash-Enter, Ctrl+J),
      history Up/Down, Ctrl+A/E/K/U/W/Y, arrows/Home/End/Backspace/Delete, Esc interrupts a
      turn, Ctrl+C clears the line / twice quits, bracketed paste as one block -- ee1d5af
- [x] T3 status line: model, effort, ROOT, "ctx: N% left", permission mode; Shift+Tab cycles
      default / accept edits / plan -- ee1d5af
- [x] T4 spinner line: rotating star, verb changing every 8 s, seconds and tokens, "esc to
      interrupt" -- ee1d5af
- [x] T5 messages: bullet, Markdown streamed by block (md stream e494cf5), code coloured,
      tables; user turns dimmed after "> " -- ee1d5af
- [x] T6 tool calls: header, result under the corner, folded at 3 lines, Ctrl+O; edit/write diff
      with line numbers on red/green; run_command output folded -- ee1d5af
- [x] T7 permission prompt: framed menu, 1/2/3, arrows + Enter, Esc = 3 -- ee1d5af
- [x] T8 slash commands with a completion menu: /help /clear /compact /context /cost /effort
      /exit /init /model /resume /save; /model and /effort pick from a menu -- ee1d5af
- [x] T9 todo_write tool shown as a checklist -- ee1d5af
- [x] T10 golden screens on the engine (tests/test_claude_tui.c): idle prompt, slash menu,
      permission menu, spinner, streamed answer, folded result, edit diff, todos, footer growing
      over a full transcript, resize -- ee1d5af
- [x] T11 reachability test (tests/test_claude_repl.c test_screen): repl_screen + repl_run over
      five recorded streams, keys typed into the engine, renderer sentinel, the edit on disk,
      footer shape constant while text streams -- ee1d5af
- [x] T12 docs: dist/README.txt CLAUDE section, RULES.md rows, ledger W31/W32 -- this commit
- [ ] T13 OWNER: on the rig against tools/claude_fixture.py, then with a real key (cheapest
      model: MODEL=claude-haiku-4-5, the owner's instruction for live tests)

Running count: 12 of 13; open: T13 (owner).

## Deviations and notes (from the run)

- The diff of an edit is drawn before the permission menu (under the call's header) and the
  "Updated ... with N additions and M removals" line after it -- Claude Code draws the diff
  inside its dialog; here the dialog stays small and the diff stays in the scrollback.
- Plan mode's marker is "||" (Claude Code's U+23F8 has no stand-in in render/glyphmap.c); accept
  edits uses U+23F5 twice (stand-in '>'). Spinner stars U+2722/2733/2736/273B/273D draw as '*'
  on a bitmap font, U+00B7 as itself; the todo glyphs are U+2714 (stand-in 'v'), U+25A0
  (drawn block), U+25CB (stand-in 'o'). render/ was not touched.
- Emoji in answers are passed through: UP-Term draws its missing-glyph box (W32's open choice).
- The screen needs the xterm dialect (UTF-8, DECSTBM, kitty keys). In the AMIGA dialect or a
  LATIN1 window the output is wrong: use PLAIN there (not detected automatically).
- No mouse reporting: like Claude Code, the window keeps its own selection and copy.
- The session auto-save writes ENVARC:Claude/session.json after every answered turn (a disk
  write per turn; the drawer exists when the kit installed Claude). A failed write is silent.
- W31 for other cooked programs is untouched (handler): see the vtcon ledger entry.

## Log

- 2026-10-04/05: e494cf5 md stream; ee1d5af the screen (keys.c edit.c tui.c show.c, ui.c as
  the facade, tools: modes/stop/preview/result hooks/todo_write, repl: commands, ctx,
  session). make test, make test-ref (149 streams), make amiga clean before each commit.
  Not run on an Amiga (no emulator in this run): main_amiga.c's raw mode (TCGETA/TCSETA,
  SetMode(0) at the end), WaitForChar reads and GWINSZ are compiled by vbcc only.
