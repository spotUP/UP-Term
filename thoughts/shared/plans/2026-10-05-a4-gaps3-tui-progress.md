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
- [x] G4 /color
- [x] G5 Alt+T thinking toggle
- [x] G6 /add-dir and /cd: Tab suggestions of directories
- [x] G7 @agent-name: typeahead and the mention on submit
- [x] G8 askUserQuestionTimeout (focus reports, countdown, auto-continue)
- [x] G9 prompt suggestions in the box
- [x] G10 session recap after being away
- [ ] G11 keybindings.json + /keybindings
- [ ] G12 N/A rows answered (/focus in na_cmds; ledger reasons)
- [ ] G13 audit file rows updated, counts

Running count: 10 of 13.

## Log

- G1, G2: audit rows verified; vim >> << (count, '.', << with nothing to take) tested in vim_visual_dot.
- G3: PERM_BYPASS replaces ASKP_BYPASS (tools.h perm_name / perm_next one place for the names and the cycle); the status row says bypass permissions on in the error colour; --allow-dangerously-skip-permissions (cli.c). Tests: test_claude_tui idle_prompt, test_claude_repl test_gaps3_bypass (three Shift+Tab, a Write unasked), print-mode flag checks.
- G4: /color: the named colours moved from repl.c (agent colours) to theme.c theme_named (one table); tui bar overrides the prompt frame (not on monochrome); no argument picks one of the eight. Test: test_gaps3_color (frame colour per read).
- G5: Alt+T: conv_caps CAP_THINK_ALWAYS (Opus 5.5, Sonnet 5.5, Fable); think_off sends thinking disabled and clamps xhigh/max to high; taken at the turn start (cl_opts copied once a turn). Tests: test_gaps3_think (bodies), test_claude_tui gaps3_think (hint on an always-thinking model).
- G6: /add-dir and /cd: path_token takes the argument (spaces kept) as the token, the @ list machinery (live once a path is begun, Tab at once) with directories only (input.c comp dirs_only). Tests: test_claude_tui gaps3_dirs, test_claude_repl test_gaps3_dirs.
- G7: @agent-NAME: the @ list offers the agents (built-ins and .claude/agents, agent_count/agent_get now public in tools.h) whose name starts with the token, as agent-NAME; on submit input.c adds Claude Code's agent_mention note (a system-reminder; wording from memory, UNVERIFIED against its source). Line mode has no mentions at all (ui_input needs the screen), as for files. Test: test_gaps3_agent_mention.
- G8: askUserQuestionTimeout (60s/5m/10m/never; user settings and --settings only: config.c user_keys, split out of cfg_merge because vbcc refused its size), CLAUDE_AFK_TIMEOUT_MS / CLAUDE_AFK_COUNTDOWN_MS. The client asks for focus reports (?1004; keys.c K_FOCUS); tui_menu counts idle time while the window is not known to be focused, a key restarts it, the last 20 s count down in the menu; ui_choose (CH_AFK, AskUserQuestion only) returns CHOOSE_AWAY; run_ask submits what was ticked and tells Claude the user may be away (wording C:Claude's own). Not built: the /config menu row (slash.c config is the other agent's area; /config key=value writes it). Tests: test_gaps3_afk (focus pause, countdown, the result), test_claude_config test_gaps3, keys() focus decoding.
- G9: prompt suggestions: repl_suggest (print mode's ask moved there; print.c uses it) asked by screen_suggest before the box waits, as a background request (cl_ui.bg: no lines, no spinner, no retries; ui_poll stops it at a typed key, tui_pending keeps the key). Shown dim in the empty box, Tab / Right take it, typing drops it. Skipped: plan mode, a cold cache (cache_read 0), an error, a short conversation; promptSuggestionEnabled / CLAUDE_CODE_ENABLE_PROMPT_SUGGESTION. Not built: the example command at the session start, which Claude Code takes from the repository's history (no version control on the Amiga). The REPL suite turns suggestions and recaps off in its setup (the stub server's scripted answers); the tests that drive them turn them on. Tests: test_gaps3_suggest, test_claude_tui gaps3_suggest, test_claude_config test_gaps3.
- G10: the away recap: screen_idle (the screen's idle hook: the status line's tick, then away_recap) makes /recap's line in the background (slash_recap, now shared with /recap, capped at 400 characters) three minutes after the last answer when the terminal reports it is unfocused, or, where it never reported focus, after three minutes without a key; three prompts or more; never twice without a turn between; shown as "Recap: ...". awaySummaryEnabled, CLAUDE_CODE_ENABLE_AWAY_SUMMARY. Not built: the /config menu row (/config key=value writes it). Tests: test_gaps3_recap (idle path; a focused window makes none).
