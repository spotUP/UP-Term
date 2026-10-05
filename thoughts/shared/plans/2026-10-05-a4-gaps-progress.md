---
date: 2026-10-05
topic: A4 gaps -- the parity audit's missing and partial rows built (C:Claude vs Claude Code docs)
tags: [claude, a4, parity, audit, progress]
status: draft
---

# A4 gaps progress ledger

Branch feature/a4-gaps off main e4bc55d. Audit:
thoughts/shared/research/2026-10-05_claude-parity-audit.md (docs fetched 2026-10-05 as
markdown from code.claude.com/docs/en/*.md). Parallel: feature/a4-input-rest (claude/vim.c
keys.c edit.c tui.c input.c tview.c) -- stay out of those files except one-line hooks named
here. No emulator, no API call, no key. Gate before each commit: make test, make test-ref,
`timeout 900 make amiga` (VTCON_NDK / VTCON_NETINCLUDE / AMISSL_SDK from the main checkout's
vendor/). Do not merge.

Done = each row built, a host test in the CI glob (tests/test_claude_*.c via make test), the
reachability through the REPL core (repl_line / print_run), gate green, committed.

## Checklist

The audit's "After" column holds the TARGET of each row until its item here is ticked; the
column is reconciled with this list at the end (an unticked item's rows go back to Before).

Phase 1 -- command line, print mode (cli.c print.c session.c repl.c subagent.c)
- [x] G1 --resume FILE.jsonl (and /resume FILE.jsonl)
- [x] G2 --session-id UUID (file named by its first 8 hex digits: FFS 30-char names)
- [x] G3 --json-schema -> StructuredOutput tool, structured_output in the result
- [x] G4 --replay-user-messages
- [x] G5 stream-json input: image / document blocks passed through (base64, as sent)
- [x] G6 subagent messages in stream-json (parent_tool_use_id) + --forward-subagent-text
- [x] G7 --bare (+ CLAUDE_CODE_SIMPLE)
- [x] G8 --safe-mode
- [x] G9 --agents JSON + "agent" setting
- [x] G10 --append-subagent-system-prompt(-file)
- [x] G11 --disable-slash-commands
- [x] G12 --setting-sources
- [x] G13 --betas
- [x] G14 --autocompact and /autocompact auto|TOKENS
- [x] G15 --debug-file
- [x] G16 --permission-prompts host|none
- [x] G17 system/api_retry events
- [x] G18 --verbose at the screen (+ "verbose" setting): results unfolded

Phase 2 -- slash commands (slash.c repl.c)
- [x] S1 the N/A commands answer with their reason (/mcp /plugin /bug /feedback ...)
- [x] S2 /btw   - [x] S3 /copy [N]   - [x] S4 /diff   - [x] S5 /plan [text]
- [x] S6 /recap   - [x] S7 /release-notes   - [x] S8 /reload-skills
- [x] S9 /usage fuller (per model, durations); /cost and /stats its aliases
- [x] S10 /clear [name], totals reset
- [x] S11 /config key=value (and more keys: editorMode, verbose, ...)
- [x] S12 /effort auto|status   - [x] S13 /model kept as the default
- [x] S14 /permissions editor (menus)   - [x] S15 /rename without a name
- [x] S16 /rewind "Summarize from here", /undo
- [x] S17 /statusline DESCRIPTION (statusline-setup agent), /statusline clear
- [x] S18 /debug   - [x] S19 /skill-name typed, skills in the menu, argument-hint shown

Phase 3 -- skills, agents, styles, status line (commands.c subagent.c tools.c policy.c)
- [ ] X1 the Skill tool expands as a command does (args, !`cmd`, allowed-tools, model)
- [x] X2 $N 0-based, $ARGUMENTS[N], \$, ${CLAUDE_SKILL_DIR|SESSION_ID|PROJECT_DIR|EFFORT}
- [ ] X3 skill user-invocable, when_to_use, context: fork + agent
- [ ] X4 agent disallowedTools, maxTurns, effort, skills, permissionMode
- [x] X5 CLAUDE.md for subagents (not Explore / Plan)
- [ ] X6 Agent = Task for rules and hook matchers
- [x] X7 built-in statusline-setup agent
- [ ] X8 output styles Proactive, Concise; keep-coding-instructions honoured
- [ ] X9 status line JSON fields, hideVimModeIndicator

Phase 4 -- hooks, settings, memory, tools (hooks.c config.c policy.c memory.c tools.c)
- [ ] H1 events PermissionRequest PostToolUseFailure SubagentStart PostCompact StopFailure
      UserPromptExpansion CwdChanged DirectoryAdded
- [ ] H2 hook "if"
- [ ] H3 systemMessage, PreToolUse additionalContext, updatedInput, continue:false on tool hooks
- [ ] H4 disableAllHooks, CLAUDE_PROJECT_DIR, timeout 600 s, Stop cap 8, PreCompact blocks,
      SessionEnd reason, permission_mode + tool_use_id in the input
- [x] H5 bypassPermissions / auto from project or local settings ignored
- [ ] H6 Bash default 120 s, BASH_DEFAULT_TIMEOUT_MS / BASH_MAX_TIMEOUT_MS, ANTHROPIC_MODEL
- [ ] H7 read-only commands run without a question
- [ ] H8 Read: images and PDFs as base64 blocks
- [ ] M1 HTML comments stripped   - [ ] M2 .claude/rules (+ paths:), ENVARC:Claude/rules
- [ ] M3 claudeMdExcludes   - [ ] M4 auto memory (MEMORY.md)

## Decisions (do not re-litigate)

- --session-id: the UUID is the id; its file is <first 8 hex digits>.jsonl (FFS's 30-character
  names cannot hold a 36-character UUID + .jsonl); the head line and the index keep the whole id.
  An 8-digit id is its own first 8 characters, so old sessions are unchanged.
- --resume X.jsonl: a path (absolute as given, else from the start directory); the session goes
  on in that file.
- --json-schema: Claude Code's StructuredOutput tool (input_schema = the schema), checked with
  schema.c; a mismatch goes back to Claude as is_error; no call -> up to 3 reminders, then
  subtype error_max_structured_output_retries. Text output still prints the answer's text.
- Images in stream-json input: base64 pass-through (the blocks go to the API as sent; text,
  image and document blocks kept). Decided over N/A: the Amiga only relays them.
- Subagent messages in stream-json: the prompt (user), then tool_use / tool_result blocks; text
  and thinking only with --forward-subagent-text (headless.md). Nested subagents do not exist here.
- --bare: no CLAUDE.md, hooks, commands, skills, agents (files); tools Bash, Read, Glob, Grep,
  Edit, Write, MultiEdit; settings files still read (permissions). CLAUDE_CODE_SIMPLE=1 the same.
  --safe-mode: the same minus the tool limit, plus no status line command.
- SessionStart "startup" runs at the first line / start(), once the command line is applied
  (was in repl_init, before --bare could say no); a resume makes it "resume".
- cli_apply resets the model and effort to the defaults before its repl_load (else a model from a
  file --setting-sources leaves out survived from repl_init's first reading).
- autoCompactWindow is saved top-level (Claude Code before v2.1.288; newer ones save it per model
  under modelSettings); CLAUDE_CODE_AUTO_COMPACT_WINDOW > --autocompact > the setting. A window
  is the context size at which it compacts (capped at the model's window); none: 92 % as before.
- A subagent's permissionMode: acceptEdits / plan / default are its tools' own mode;
  dontAsk / bypassPermissions its own ask policy (cl_tools.ask_policy); the parent's mode is
  never changed by it. Explore and Plan get no CLAUDE.md (Claude Code), the others do.
- $N is 0-based ($0 the first), as Claude Code; a missing index stays as written; the test that
  encoded $1-as-first was changed with it.
- /cost and /stats are /usage (Claude Code: aliases); /usage adds the per-model rows and the times.
- /btw /recap /rename (no name) and /rewind's summaries are one side request (repl_side): the
  conversation copied, tools listed with tool_choice none (the cached prefix kept), nothing added
  to the conversation; the cost counts.
- /diff without git: each file's first checkpoint of the session against the file now (screen:
  the edit preview's diff; line mode: - / + lines). Bash's changes are not tracked (as /rewind).
- /statusline TEXT asks Claude to use the statusline-setup agent (Claude Code's way), with the
  JSON fields listed in the prompt; "/statusline command CMD" is C:Claude's direct form; clear,
  delete, remove, off take the key out.
- /model writes "model" to the user's settings (Claude Code keeps it for new sessions); /effort
  auto sends no effort (the model's own).
- The commands Claude Code has that cannot be here are a table in slash.c; each typed one says why,
  /help lists them in one line. The rewind menu (Esc Esc) got "Summarize from here / up to here".
- A skill typed as /name runs as a turn like a command (context: fork applies when Claude calls
  the Skill tool; typed, it runs inline).

## Log

- c1 Phase 1 + X2 X5 H5 (+ settings keys parsed, agent/skill frontmatter keys, statusline-setup
  agent): suites claude_repl test_gaps_print / test_gaps_verbose, claude_config test_gaps, the
  $N test changed to 0-based. Mutations checked (each failed the suite): feed->sub unwired,
  StructuredOutput intercept off, bare/safe memory skip off, sess_use_id off. Gate: make test
  (43 suites OK), make test-ref (149 streams, 0 failed), timeout 900 make amiga rc 0.

- c2 Phase 2 (S1-S19): claude_repl test_gaps_commands (line mode through repl_line) and
  test_gaps_perm_menu (the screen); the screen test looks at the edit's rows before /cost (the
  longer /usage output scrolls them away); print_stream.jsonl golden: slash_commands grew. Fixed on
  the way: /permissions said "allowRead" (no space). Mutations: not_here off, /clear's usage
  reset off, /effort auto sending "auto" each fail the suite. Gate: make test (43 OK), make
  test-ref (149, 0 failed), make amiga rc 0.

## Rig steps (main session)
