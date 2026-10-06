---
date: 2026-10-05
topic: A4 WP3 -- memory, settings, permissions, commands, hooks, sessions, checkpoints -- progress ledger
tags: [claude, a4, wp3, progress]
status: implemented
---

# A4 WP3 progress ledger

Plan: thoughts/shared/plans/2026-10-05-a4-claude-parity.md, rows 3.1-3.12. Branch
feature/a4-wp3-config off main a95ec55. Parallel: WP1 (keys.c edit.c tui.c show.c), WP2
(tools.c path.c + new tool files). WP3 stays in conv.c, repl.c, new files; sys.h /
sys_posix.c / sys_amiga.c get optional new fields (NULL-checked) and main_amiga.c the
minimal wiring. No emulator, no real API call, no key.

## Checklist (ticked when built + host-tested in the CI glob; the rig check is the owner's)

- [x] 3.1 CLAUDE.md hierarchy + @imports + nested on read
- [x] 3.2 settings.json user/project/local merged
- [x] 3.3 permission rules before asking; /permissions
- [x] 3.4 custom slash commands
- [x] 3.5 output styles
- [x] 3.6 hooks
- [x] 3.7 sessions JSONL, /rename /resume picker /branch, CONTINUE
- [x] 3.8 auto-compact, /compact focus
- [x] 3.9 checkpoints + /rewind
- [x] 3.10 slash commands (/status /usage /export /config /memory /add-dir /doctor /statusline
      /terminal-setup /login /logout /agents /skills /commands /hooks /tasks /todos /cd)
- [x] 3.11 model aliases, fallback model (FALLBACK=, fallbackModel)
- [x] 3.12 prompt caching kept on
- [x] reachability test (CLAUDE.md in body, rule skips question, command expands, hook blocks)
- [x] coordinator addition: thinking adaptive + summarized, fields per model, body per alias
- [x] WP1 interfaces (main 9bc4289 merged in): ui.setting/set_setting (theme, editorMode,
      kept in the user's settings), ui.memory_files + memory_changed (reload), ui.rw with
      RW_CODE from the checkpoints (policy.c pol_attach_ui); /memory uses io->edit

## Decisions (do not re-litigate)

- Paths: user dir = ENVARC:Claude (CLAUDE_CONFIG_DIR overrides, as in Claude Code; the tests
  use it), temp = T: (CLAUDE_CODE_TMPDIR overrides); project = <root>/.claude. Files:
  settings.json, settings.local.json (project only), CLAUDE.md, commands/, agents/,
  skills/<name>/SKILL.md, output-styles/, projects/<slug>/<id>.jsonl + projects/<slug>/sessions.
- Memory order (as Claude Code): user CLAUDE.md, then ancestors of the root (volume root
  first; CLAUDE.md and AMIGA.md), then the root's CLAUDE.md, AMIGA.md, AGENTS.md,
  .claude/CLAUDE.md, CLAUDE.local.md; imports after their file. All of it goes into the system
  prompt (stable for the session: inside the cached prefix). Nested CLAUDE.md below the root
  is added when a file there is read, as a text block after the tool_result blocks
  (append-only: earlier history is never edited).
- Permission rules and PreToolUse hooks are applied in policy.c before tools_run (a deny or a
  block answers the tool_use itself with is_error); an allow is given through the ask
  callback (tools_run asks -> r->rule_now answers ASK_ONCE). No tools.c change is needed.
  Rule tool names are Claude Code's; A2 names map (read_file->Read, list_dir->LS, grep->Grep,
  write_file->Write, edit_file->Edit, run_command->Bash, todo_write->TodoWrite).
  deny > ask > allow. Edit(...) covers Write/Edit/MultiEdit; Read(...) covers Read/LS/Grep/Glob.
  A compound Bash line is allowed only when every part is.
- Hooks: the event JSON goes to a temp file and the command runs as `<command> < <file>`
  through sys->run (vsh on the Amiga, sh on the host). One output stream (stdout+stderr);
  exit 2 = block with the output as the reason; exit 2 on SessionStart/End, Notification,
  PreCompact is shown, not blocking (Claude Code's table). JSON answers understood:
  decision/reason, continue:false, hookSpecificOutput.permissionDecision/additionalContext.
- Sessions: one JSONL file per conversation, one line per message with its index, the
  content array byte-exact; appended once per completed turn (sys->append). A line whose
  index is not past the end replaces that message and drops the rest: a prompt added to a
  trailing tool_result message, /compact, /rewind are all one more line. /clear starts a new
  session. The picker reads the small index file, not the sessions. /resume FILE.json still
  reads the A2 format; ENVARC:Claude/session.json is taken over when no session exists.
- Checkpoints: snapshot before a write/edit taken in policy.c (idempotent per turn+path),
  under T:Claude-cp/, capped at 256 KB total (oldest turns dropped, a file larger than half
  noted as not restorable).
- Caching (3.12): the A2 layout is the documented robust combination (claude-api skill,
  prompt-caching): explicit breakpoint on the system prompt (caches tools + system) +
  top-level automatic caching for the tail; each request adds two messages, inside the
  20-block lookback. Everything per-turn (nested memory, hook context) goes after the
  history, never into system. The reachability test asserts the system bytes are equal on
  every request.
- Request body (coordinator 2026-10-05): thinking {type adaptive, display summarized} for
  every model that takes it; Haiku 4.5 gets neither thinking nor effort (conv_caps).
- /memory edits through WP1's io->edit (Ctrl+G's launcher, raw mode off meanwhile); only
  without one does it fall back to $EDITOR (else Ed) through sys->run.
- Merge with WP1: the A3 cmds[] table is gone; /theme and /vim are in slash.c's table
  (input.c handles them). A prompt from ui_input (! output, @ files) also goes through the
  UserPromptSubmit hook. memory.h's record is cl_memsrc (cl_memfile is ui.h's).

## Done

- 301e6b7 request body per model (coordinator addition).
- WP3 commit (git log feature/a4-wp3-config): rows 3.1-3.12.
- Tests: suite claude_config (settings, rules, aliases, body per alias, memory, defs,
  command expansion, hooks' exit codes, sessions, checkpoints); claude_repl test_wp3
  (reachability) and test_wp3_commands (every new slash command, auto-compact,
  UserPromptSubmit context, fallback model). Wiring checked by mutation: unwiring pol_pre,
  the memory in the system prompt, custom commands, auto-compact or the fallback each fails.

## Open / for other WPs

- Rig check (owner): every row is host-tested only; rig steps in the final report.
- WP1 interfaces (above).
- WP2: agent / skill lists via defs_nth(&r->defs, DEF_AGENT|DEF_SKILL, i) (commands.h);
  SlashCommand tool via defs_find(DEF_COMMAND) + cmd_expand; rules and checkpoints are
  applied in policy.c before tools_run (nothing to call; cfg_decide and
  checkpoint_before_write exist). /tasks reads r->bg_list. "file_path" is understood.
- No key at start: repl_init still refuses an https URL without a key, so /login works only
  once started; a keyless start straight into /login is for WP4 (CLI).
