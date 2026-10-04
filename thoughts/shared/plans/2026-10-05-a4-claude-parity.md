---
date: 2026-10-05
topic: A4 -- C:Claude at feature parity with the Claude Code CLI, as far as an Amiga allows
tags: [claude, a4, parity, upterm]
status: draft
---

# A4: C:Claude = Claude Code, minus what an Amiga cannot do

Owner 2026-10-05: "Ok don't stop until claude cli is finished." / "Definition of done is when our
claude cli clone has all feats of the real claude cli, at least what is possible on the amiga."

DONE = every row below is [x] (built, host-tested in the CI glob, and seen on the rig against
tools/claude_fixture.py) or [N/A] with its reason. Real-API checks use Haiku 4.5 (owner rule).
Source of the feature list: code.claude.com docs (interactive-mode, commands, tools-reference,
memory, settings-reference, sessions, cli-reference, headless), fetched 2026-10-05.

Already there (A2/A3, merged 9792bee + 898f5aa): streaming chat, input box, status line, spinner,
Markdown, diffs, todo list, permission menus, Shift+Tab modes (default/accept edits/plan), Esc,
Ctrl+C twice, /help /clear /compact /context /cost /effort /exit /init /model /resume /save, tools
read_file list_dir grep write_file edit_file run_command todo_write.

Amiga mapping: ~/.claude -> ENVARC:Claude/ (user), project .claude/ -> <root>/.claude/ (FFS takes
the dot), CLAUDE.md also as AMIGA.md (A3 /init writes AMIGA.md: keep both names, CLAUDE.md first);
shell = vsh when present, else AmigaShell (SystemTags); paths AmigaDOS and /Unix both accepted.

## WP1 input and screen (claude/keys.c edit.c tui.c show.c)
- [ ] 1.1 history persisted across sessions (ENVARC:Claude/history), Up/Down, Ctrl+R reverse search
- [ ] 1.2 @-file mentions: `@path` inlines the file (size cap), Tab completes paths after `@`
- [ ] 1.3 `!` bash mode: `! cmd` runs it, output shown and added to the context
- [ ] 1.4 `#` memory shortcut: `# note` appends to a chosen memory file (user / project)
- [ ] 1.5 type-ahead: input accepted while a turn runs, queued and sent after it
- [ ] 1.6 Ctrl+O transcript view (full tool output, thinking), Ctrl+R search in it
- [ ] 1.7 Esc Esc: the rewind menu (pick an earlier user message: restore conversation and/or files)
- [ ] 1.8 vim mode (/vim, setting): normal/insert, hjkl w b 0 $ x dd i a A o, Esc
- [ ] 1.9 thinking display: "thinking..." line, Ctrl+T toggles showing thinking text
- [ ] 1.10 /theme: dark / light / 16-colour palettes for the TUI
- [ ] 1.11 notifications: bell + window title when a turn ends or a permission waits (OSC/title)
- [ ] 1.12 Ctrl+L redraw, Ctrl+G edit the prompt in $EDITOR (vsh's EDITOR, else Ed)
- [N/A] image paste: no image clipboard path to the API from AmigaOS (IFF ILBM -> PNG conversion
       would be a separate feature; revisit if the owner asks)

## WP2 tools (claude/tools.c path.c)
- [ ] 2.1 Read: offset/limit, line numbers (cat -n style), size cap with truncation note, binary refuse
- [ ] 2.2 Write: create/overwrite, must-read-first rule for existing files, diff preview kept
- [ ] 2.3 Edit: replace_all, unique-match error text as Claude Code's; MultiEdit (sequential edits, atomic)
- [ ] 2.4 Glob: patterns (** * ? [..]), AmigaDOS #? too, sorted by date, result cap
- [ ] 2.5 Grep: regex (POSIX ERE subset), -i, -n, -A/-B/-C, glob/type filter, output_mode
       content / files_with_matches / count, head_limit
- [ ] 2.6 Bash: timeout, output cap, exit code; run_in_background + BashOutput + KillShell (AmigaDOS
       Run + a pipe/T: file, Signal for kill)
- [ ] 2.7 WebFetch: https GET through AmiSSL, redirects, HTML -> text/Markdown, size cap, the prompt
       applied by a small model call (Haiku) as Claude Code does
- [ ] 2.8 WebSearch: the API's server tool web_search (no client code beyond declaring it, showing
       its results); a setting to turn it off
- [ ] 2.9 AskUserQuestion: the framed choice menu (1-4 options + other)
- [ ] 2.10 plan mode proper: ExitPlanMode tool shows the plan and asks to leave plan mode;
       EnterPlanMode
- [ ] 2.11 Task / subagents: a nested conversation with its own tools subset and system prompt,
       run sequentially (no threads), summary returned; .claude/agents definitions (WP3)
- [ ] 2.12 Skill tool + skills (.claude/skills/<name>/SKILL.md, frontmatter) (with WP3 loader)
- [ ] 2.13 SlashCommand tool: the model may run a custom command
- [N/A] NotebookEdit (no Jupyter on the Amiga), LSP, computer use, Chrome, Artifact, ToolSearch for
       MCP, SendMessage/Workflow (multi-session orchestration)

## WP3 memory, config, sessions (claude/conv.c repl.c + new config.c session.c)
- [ ] 3.1 CLAUDE.md hierarchy: ENVARC:Claude/CLAUDE.md, <root>/CLAUDE.md (+AMIGA.md, AGENTS.md),
       <root>/.claude/CLAUDE.local... and nested ones when files there are read; @imports (depth 5)
- [ ] 3.2 settings.json at user / project / local level, merged: permissions allow/deny/ask rules
       (Tool(pattern) syntax), env, model, effort, outputStyle, statusLine, hooks, theme
- [ ] 3.3 permission rules applied before asking; /permissions view and edit
- [ ] 3.4 custom slash commands: .claude/commands/*.md (user + project), $ARGUMENTS/$1, frontmatter
       (description, allowed-tools, model), !`cmd` and @file inside
- [ ] 3.5 output styles: built-in Default/Explanatory/Learning + .claude/output-styles; /output-style
- [ ] 3.6 hooks: PreToolUse PostToolUse UserPromptSubmit Stop SessionStart SessionEnd PreCompact
       Notification, as AmigaDOS commands with JSON on stdin and the exit-code protocol (2 = block)
- [ ] 3.7 sessions as JSONL under ENVARC:Claude/projects/<root>/, names (/rename), /resume picker,
       /branch (fork), --continue; auto-save without a disk write per keystroke
- [ ] 3.8 auto-compact at a threshold (/autocompact), /compact with focus instructions
- [ ] 3.9 checkpoints: file snapshots before each edit/write (T:-backed, capped), /rewind restores
       files and/or conversation
- [ ] 3.10 /status /usage /export (file, clipboard via clipboard.device) /config (menu over settings)
       /memory (edit memory files in $EDITOR) /add-dir /doctor (AmiSSL, bsdsocket, key, network,
       console checks) /statusline /terminal-setup (UP-Term settings check) /login (key entry,
       stored ENVARC:Claude/key) /logout /agents /skills /commands /hooks /tasks /todos /cd
- [ ] 3.11 model aliases (opus sonnet haiku fable -> current ids), --fallback-model
- [ ] 3.12 prompt caching kept on (system + tools + history breakpoints)
- [N/A] OAuth /login with a claude.ai account (browser flow), /bug /feedback upload, plugins
       marketplace, MCP stdio servers (need Node); MCP over HTTP: optional later

## WP4 CLI and print mode (claude/main_amiga.c repl.c)
- [ ] 4.1 `Claude "prompt"` starts with it; `-p`/PRINT one-shot, stdin piped in
- [ ] 4.2 --output-format text/json/stream-json; --input-format stream-json
- [ ] 4.3 -c/--continue, -r/--resume NAME, -n/--name, --fork-session, --no-session-persistence
- [ ] 4.4 --model --effort --permission-mode --allowedTools --disallowedTools --tools --add-dir
       --append-system-prompt(-file) --system-prompt(-file) --settings --max-turns --max-budget-usd
       --verbose --agent
- [ ] 4.5 both forms: Unix flags and AmigaDOS ReadArgs keywords (MODEL=, PRINT/S ...)
- [ ] 4.6 Haiku 4.5 requests valid: no fields it rejects (effort/output_config, fallbacks beta)
- [N/A] --chrome, --worktree (no git), --cloud/--teleport/--remote-control, --ide, update/install

## Order and agents (owner cap: max 3 running, one new per finished)
Round 1 (parallel, separate files): WP2 tools; WP3 memory/config/sessions; WP1 input/screen.
Round 2: WP4 CLI/print mode + 4.6; then whatever rows are left. Main session: merges, rig checks
(fixture), one real-API smoke test on Haiku 4.5 once the owner has a key on the rig.
