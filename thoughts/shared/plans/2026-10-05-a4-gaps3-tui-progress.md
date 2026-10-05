---
date: 2026-10-05
topic: A4 gaps 3 (TUI side) -- the audit's remaining screen rows, progress ledger
tags: [claude, a4, parity, tui, keys]
status: draft
---

# A4 gaps 3, TUI side (branch feature/a4-gaps3-tui off main bd75f8d)

Source: thoughts/shared/research/2026-10-05_claude-parity-audit.md (the rows below), Claude Code's docs
fetched 2026-10-05 (interactive-mode.md, keybindings.md, commands.md, settings-reference.md,
tools-reference.md, env-vars.md, sub-agents.md). Done = built as Claude Code does it, a host test in
`make test` per behaviour, `make test` and `timeout 900 make amiga` green before each commit. No
emulator here; the rig check is the main session's. A parallel agent owns /loop + ScheduleWakeup
(slash.c, tasks.c, sched.c, tools.c): slash.c edits here are new table entries and their handlers only.

## Step 1: the audit's rows re-verified against the code (main bd75f8d)

Counted from the structures: the slash table `slash_builtin[]` (slash.c:15-64, 50 entries) and its
dispatch `slash_run` (slash.c:1758), the N/A table `na_cmds[]` (slash.c:908), the key decoder
(keys.c, a decoder only, no binding table), the key dispatch `handle()` (tui.c:1707-2025, a fixed
switch), `tui_menu` (tui.c:2155), the mode names `tui_mode_names[3]` (tui.c:30), the flags `opts[]`
(cli.c:84-), the vim dispatch (vim.c).

| Audit line | Row | Verified | Evidence |
|---|---|---|---|
| 33 | /add-dir: Tab suggestions of directories | missing | tui.c:1347 path_token: only "@path" (prompt) or a '/' token (bash mode) opens the list |
| 50 | /color [color\|default] | missing | not in slash_builtin, not in slash_run, not in na_cmds |
| 66 | /focus | missing | not in any table |
| 75 | /keybindings | missing | not in slash_builtin; keys.c has no bindings; handle() hard-codes every key |
| 120 | --allow-dangerously-skip-permissions | missing | cli.c:118 has only --dangerously-skip-permissions; tui.c:1218 cycles `% 3` (no bypass) |
| 156 | Ctrl+S | have | tui.c:1982 stash |
| 156 | Ctrl+B | have | tui.c:1985 (busy: bg_req; idle: back a character) |
| 156 | Ctrl+X Ctrl+K | missing (consumed, does nothing) | tui.c:1739 |
| 156 | Ctrl+Enter / Ctrl+X Ctrl+S | have | keys.c code_key (CSI 13;5u), tui.c:1823, tui.c:1730 send_now |
| 156 | Alt+Y | have | edit.c:302 kill ring |
| 156 | Alt+P | have | tui.c:2007 |
| 156 | Alt+T | missing | no K_ALT 't' anywhere |
| 156 | Alt+O | missing | no K_ALT 'o' |
| 159 | ? help panel | have | tui.c:1916, help_rows tui.c:760 |
| 159 | : emoji shortcodes | missing | |
| 159 | spell check | missing | |
| 162 | vim visual, text objects, '.', >> << | have | vim.c:908 (v/V), vim.c:602 text_object, vim.c:911 '.', vim.c:783 >> << ; tests vim_visual_dot (>> << in normal mode had no test) |
| 167 | prompt suggestions in the box | missing | print.c:369 (print mode only) |
| 168 | session recap after being away | missing | slash.c:1047 /recap only; no idle trigger |
| 258 | askUserQuestionTimeout | missing | config.c has no key; tui_menu has no timer |
| 287 | @agent-name in the prompt | missing | input.c:384 mentions: files only; input_complete: paths only |

## Decisions (made; do not re-litigate)

- /focus: N/A. Claude Code offers the focus view only in its fullscreen renderer ("Focus view needs
  the fullscreen renderer", settings-reference viewMode); C:Claude is the classic renderer, made for
  UP-Term (the audit's /tui row is N/A for the same reason). Typed, /focus says so (na_cmds).
- `:` emoji shortcodes: N/A. UP-Term's fonts (Topaz and the bitmap pairs) have no emoji glyphs, and
  the owner's rule is no emoji anywhere in the UI.
- Spell check: N/A. Claude Code runs aspell / hunspell / ispell as a long-running child it talks to
  over pipes; a stock AmigaOS 3.x has none of them, and C:Claude's process layer (sys.h run, shells.c
  jobs) has no two-way pipe to a child; a process per word on a 68020 is seconds a word.
- Alt+O (fast mode): N/A, as /fast (a claude.ai plan feature). Ctrl+X Ctrl+K: N/A, it stops
  background subagents and C:Claude's subagents run in the foreground only (no threads); the chord
  stays consumed.
- Bypass: one source of truth -- the session's permission mode (cl_perm.mode) gets PERM_BYPASS; the
  separate ask_policy ASKP_BYPASS goes (it could not be cycled away). Shift+Tab cycles default ->
  acceptEdits -> plan -> bypassPermissions (only when started with --dangerously-skip-permissions or
  --allow-dangerously-skip-permissions) -> default.
- Keybindings: ENVARC:Claude/keybindings.json (<home>/keybindings.json), Claude Code's format. The
  binding table lives in keys.c (defaults + the file's blocks, chords, null unbinds, validation
  warnings); tui.c resolves each key to an action for the active contexts and dispatches on the
  action. Reloaded when the file changes (checked while the screen waits).
- "Away" (recap, question timeout): Claude Code uses the terminal's focus reports (?1004). UP-Term
  sends them; the client asks for them. Unknown focus (a terminal that never reported) counts as away
  after the idle time with no key.
- Background side requests (prompt suggestion, away recap): no spinner, no error lines, no retries;
  a key typed cancels the request and stays for the box.

## Checklist

- [x] G1 audit rows verified (above); ledger
- [x] G2 vim >> << normal-mode test
- [x] G3 bypassPermissions in the Shift+Tab cycle, --allow-dangerously-skip-permissions
- [ ] G4 /color
- [ ] G5 Alt+T thinking toggle
- [ ] G6 /add-dir and /cd: Tab suggestions of directories
- [ ] G7 @agent-name: typeahead and the mention on submit
- [ ] G8 askUserQuestionTimeout (focus reports, countdown, auto-continue)
- [ ] G9 prompt suggestions in the box
- [ ] G10 session recap after being away
- [ ] G11 keybindings.json + /keybindings
- [ ] G12 N/A rows answered (/focus in na_cmds; ledger reasons)
- [ ] G13 audit file rows updated, counts

Running count: 3 of 13.

## Log

- G1, G2: audit rows verified; vim >> << (count, '.', << with nothing to take) tested in vim_visual_dot.
- G3: PERM_BYPASS replaces ASKP_BYPASS (tools.h perm_name / perm_next one place for the names and the cycle); the status row says bypass permissions on in the error colour; --allow-dangerously-skip-permissions (cli.c). Tests: test_claude_tui idle_prompt, test_claude_repl test_gaps3_bypass (three Shift+Tab, a Write unasked), print-mode flag checks.
