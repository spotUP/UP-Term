---
date: 2026-10-05
topic: A4 gaps -- the parity audit's missing and partial rows built (C:Claude vs Claude Code docs)
tags: [claude, a4, parity, audit, progress]
status: final
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
- [x] X1 the Skill tool expands as a command does (args, !`cmd`, allowed-tools, model)
- [x] X2 $N 0-based, $ARGUMENTS[N], \$, ${CLAUDE_SKILL_DIR|SESSION_ID|PROJECT_DIR|EFFORT}
- [x] X3 skill user-invocable, when_to_use, context: fork + agent
- [x] X4 agent disallowedTools, maxTurns, effort, skills, permissionMode
- [x] X5 CLAUDE.md for subagents (not Explore / Plan)
- [x] X6 Agent = Task for rules and hook matchers
- [x] X7 built-in statusline-setup agent
- [x] X8 output styles Proactive, Concise; keep-coding-instructions honoured
- [x] X9 status line JSON fields, hideVimModeIndicator

Phase 4 -- hooks, settings, memory, tools (hooks.c config.c policy.c memory.c tools.c)
- [x] H1 events PermissionRequest PostToolUseFailure SubagentStart PostCompact StopFailure
      UserPromptExpansion CwdChanged DirectoryAdded
- [x] H2 hook "if"
- [x] H3 systemMessage, PreToolUse additionalContext, updatedInput, continue:false on tool hooks
- [x] H4 disableAllHooks, CLAUDE_PROJECT_DIR, timeout 600 s, Stop cap 8, PreCompact blocks,
      SessionEnd reason, permission_mode + tool_use_id in the input
- [x] H5 bypassPermissions / auto from project or local settings ignored
- [x] H6 Bash default 120 s, BASH_DEFAULT_TIMEOUT_MS / BASH_MAX_TIMEOUT_MS, ANTHROPIC_MODEL
- [x] H7 read-only commands run without a question
- [x] H8 Read: images and PDFs as base64 blocks
- [x] M1 HTML comments stripped   - [x] M2 .claude/rules (+ paths:), ENVARC:Claude/rules
- [x] M3 claudeMdExcludes   - [x] M4 auto memory (MEMORY.md)

Phase 5 -- the rest the Amiga can do (added after phase 4; the audit's "not built" rows)
- [x] P1 bundled skills as prompts: simplify, update-config, fewer-permission-prompts, insights,
      team-onboarding, run, verify, run-skill-generator; /skill-doctor
- [x] P2 hooks: prompt type, once, statusMessage, Claude Code's matcher rules (exact list /
      regex), Notification types + idle_prompt, PreModelSwitch / PostModelSwitch,
      InstructionsLoaded, PostToolBatch, ConfigChange (+ settings re-read on change),
      CLAUDE_ENV_FILE, Setup with --init / --init-only / --maintenance, --include-hook-events,
      the same handler from several files once
- [x] P3 settings: apiKeyHelper, availableModels, bashOutputMaxChars (+ BASH_MAX_OUTPUT_LENGTH),
      cleanupPeriodDays; "don't ask again in this project" kept as a rule; ~/ in path rules
- [x] P4 memory: AGENTS.md only where no CLAUDE.md is; CLAUDE.local.md in every directory
- [x] P5 agents: claude-code-guide and claude built-ins; initialPrompt
- [x] P6 CLI: --exclude-dynamic-system-prompt-sections, --prompt-suggestions; Claude doctor,
      auth status / login / logout, purge
- [x] P7 /context by category; /skills TEXT with sizes; /goal
- [x] P8 Bash: a cd persists; WebFetch 15-minute cache
- [x] P9 checkpoints kept across a restart (T:Claude-cp/<hash of the session file>/index)
- [x] P10 SessionStart's sessionTitle / initialUserMessage / reloadSkills; terminalSequence
      (allowlist); PostToolUse updatedToolOutput
- [x] P11 commands in subdirectories as dir:name; nested .claude/skills loaded on the way; a
      skill's paths:; a typed skill's effort

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
- The Skill tool still asks permission (tools-reference: "Permission required: Yes"; skills.md's
  "Claude can invoke any skill" is about disable-model-invocation); Skill / Skill(name) rules decide.
- Agent = Task: the tool keeps its declared name Task (the recordings, the fixture); rules and hook
  matchers naming Agent cover it, as tools_mask already did for --tools / agents' lists.
- A custom output style without keep-coding-instructions drops sys_d (the way of working: todo
  list, concise answers) and keeps sys_a..sys_c (the machine's facts: paths, shell, speed).
  outputStyle in a settings file is case-sensitive (Claude Code); /output-style is not.
- hideVimModeIndicator: one line in tui.c (feature/a4-input-rest's file), named here:
  `} else if (t->ed.vim == VIM_INSERT && !t->hide_vim) {` at the "-- INSERT --" hint, and the
  field hide_vim in tui.h. Set by repl.c from the setting.
- Status line JSON: total_lines_added/removed count a Write/Edit/MultiEdit only when it succeeds
  (the preview's counts kept until the result).
- Hooks: JSON is read on every exit code (hooks.md); exit 2 still blocks. "if" is a permission
  rule matched against the tool call (cfg_rule_match). updatedInput replaces the input and the
  rules decide again on it. continue:false on a tool hook ends the turn after the round
  (tools.stop). PermissionRequest runs before any question, in print mode before nobody's no;
  its updatedInput is not applied (the call's input is fixed by then: partial). PreCompact exit 2
  blocks /compact. CwdChanged runs with the hooks of the directory being left (the new one's
  settings are read after). SessionEnd's reason: prompt_input_exit after /exit, else other.
  CLAUDE_PROJECT_DIR = the launch directory, set for the command and substituted as text
  (${CLAUDE_PROJECT_DIR} and $CLAUDE_PROJECT_DIR: the AmigaShell knows no ${}).
  The default hook timeout is Claude Code's 600 s.
- Read-only commands (List Dir Type Info Which Echo Version Search Avail Date Status, ls cat head
  tail grep wc pwd file cmp diff whoami uname) run without a question, also in plan mode, when every
  part of the line is one and nothing is redirected into a file or substituted; an explicit ask
  rule still asks (tools.rule_ask). Bash's default time is 120 s (was 60).
- Read: PNG/JPEG/GIF/WebP/PDF by their first bytes, read whole to 3.75 MB (the API's 5 MB of
  base64), sent as an image / document block in the tool_result; the screen says "Read image (N KB)".
- Memory: HTML comments a line starts with are left out (code blocks kept); rules from
  ENVARC:Claude/rules and <root>/.claude/rules (*.md, 3 levels of subdirectories), paths: rules
  loaded when a Read/Edit file matches (relative to the root, or the whole path); auto memory on
  by default (autoMemoryEnabled false or CLAUDE_CODE_DISABLE_AUTO_MEMORY turn it off): MEMORY.md's
  first 200 lines, the directory named in the system prompt, Write/Edit/Read there need no
  question; /memory auto on|off.
- Bundled skills are built-in DEF_SKILL definitions (src built-in): prompts adapted to the Amiga
  (no git: simplify reviews the files changed in the conversation). /review, /code-review,
  /security-review, /batch stay N/A (git, worktrees, parallel agents).
- Prompt hooks ask claude-haiku-4-5 (or their "model") through agent_query; ok:false blocks
  like exit 2 (PreToolUse: the call is refused and the turn goes on -- Claude Code's
  continueOnBlock behaviour, its default ends the turn); impossible:true lets a Stop end.
  http and agent hook types are not built.
- Hook matchers follow hooks.md: "" / "*" all; letters, digits, _ - space , | an exact list;
  anything else a regular expression (claude/regex.c), unanchored.
- ConfigChange: the three files' times are taken at each load and compared at each typed line;
  a change runs the hooks and (unless blocked) reads the settings again. Our own writes count too.
- idle_prompt: from the screen's idle tick, 60 s after a turn ended, once per wait.
- cleanupPeriodDays is honoured when set; C:Claude does not apply Claude Code's default of 30
  days (a decision: no session is deleted unless the user asks for it in the settings). The
  clock is a new T: file's time (sys.h has no clock of seconds).
- "Yes, and don't ask again in this project" (screen: 3rd option for non-edit tools; line mode:
  p) writes Bash(<first word> *), WebFetch(domain:<host>) or the tool's name to
  .claude/settings.local.json.
- ~/ in a path rule is HOME (vsh sets it), else SYS:.
- --exclude-dynamic-system-prompt-sections moves the auto memory section (the per-user part)
  into the first prompt.
- Subcommands are recognised when the whole prompt is "doctor", "auth status [--text]",
  "auth login", "auth logout", "purge [dir]" (not in print mode). purge asks y/n first and removes
  the project's directory under ENVARC:Claude/projects (sessions, auto memory) and its history lines.
- A Bash "cd DIR" alone sets where the next commands run (a "cd" line prefixed to each script;
  the AmigaShell's script and vsh both take it); cd is in the read-only set.
- WebFetch keeps 4 pages for 15 minutes (the page's Markdown; the prompt is answered anew).
- Checkpoints: each session's snapshots live in T:Claude-cp/<8 hex digits of a hash of the
  session file's path> with an index rewritten at each change; a saved session keeps them at
  the end (an unsaved one deletes them); a resume (and --session-id) switches to the session's
  own directory, so /rewind reaches back before the restart. T: is RAM: on most Amigas: a reboot
  ends them (Claude Code keeps them 30 days on disk).
- The tools JSON is built at the start of each round of a turn (a skill loaded by a read in the
  round must not leave a freed pointer behind -- found by ASan in the nested-skill test).
- terminalSequence: OSC 0/1/2/9/99/777 and BEL only, written to the console (not in print mode).
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

- c3 Phase 3 (X1 X3 X4 X6 X8 X9): claude_repl test_gaps_ext (print mode: Skill expanded + its
  allowed-tools letting Bash run with nobody to ask, context: fork through a subagent, an
  --agents agent's effort / preloaded skill / plan mode, a deny rule Agent(x) on Task, the five
  built-in styles and keep-coding-instructions, the status line JSON); claude_config: style count
  6, hooks_match Agent. Mutations: Skill provider off, Agent alias off, keep-coding off each fail.
  Gate: make test (43 OK), make test-ref (149, 0 failed), make amiga rc 0.

- c4 Phase 4 (H1-H4 H6-H8 M1-M4): claude_repl test_gaps_hooks (the eight new events through the
  REPL and print mode, if, updatedInput, additionalContext, systemMessage, continue:false,
  PreCompact block, CLAUDE_PROJECT_DIR, Read of a PNG and a PDF, Bash's default and max time,
  HTML comments, rules always / by paths:, claudeMdExcludes, auto memory and its writes);
  claude_tools: read-only commands. Tests that used "echo" as a command needing a question now
  use makedir / printf (echo is read-only now). Mutations: if off, PermissionRequest off,
  comment stripping off, the updatedInput swap off, rules off each fail. Gate: make test (43 OK),
  make test-ref (149, 0 failed), make amiga rc 0.

- c5 Phase 5 (P1-P8): claude_repl test_gaps_more (prompt hook on Stop, once, matchers,
  CLAUDE_ENV_FILE, InstructionsLoaded, idle_prompt, PostToolBatch, Pre/PostModelSwitch,
  ConfigChange, --init-only, --include-hook-events, --prompt-suggestions,
  --exclude-dynamic-system-prompt-sections, the kept rule, apiKeyHelper, availableModels,
  bashOutputMaxChars, cleanupPeriodDays, ~/ rule, AGENTS.md / CLAUDE.local.md, built-in agents,
  /simplify, /skills TEXT, /skill-doctor, /context, /goal, cd, WebFetch cache, doctor / auth
  status / purge, initialPrompt); claude_config: matcher rules; goldens: tools gained Skill
  (bundled skills), slash_commands /goal /skill-doctor. Mutations: prompt hooks' model off,
  PostToolBatch stop off, the WebFetch cache off each fail. Gate: make test (43 OK), make
  test-ref (149, 0 failed), make amiga rc 0.

- c6 Phase 6 (P9-P11): claude_repl test_gaps_more grew (a resumed session's /rewind takes back
  a file the earlier run wrote; SessionStart's fields in print mode; terminalSequence on the
  console, a CSI not; updatedToolOutput; paths: and nested skills offered after the read; a
  typed skill's effort; /grp:cmd); claude_config: git/commit.md is git:commit. Mutation: the
  resume's cp_session off fails the suite. Gate: make test (43 OK), make test-ref (149, 0
  failed), make amiga rc 0.

## Audit counts (thoughts/shared/research/2026-10-05_claude-parity-audit.md, 247 rows)

Before (main e4bc55d): have 58, partial 37, missing 119, N/A 33.
After (57613ae): have 173, partial 8, missing 32, N/A 34.

## Left (the 32 missing and 8 partial rows), and why

- feature/a4-input-rest's files (vim.c keys.c edit.c tui.c input.c tview.c): /color, /focus,
  /keybindings, /loop (a timer in the input loop), --allow-dangerously-skip-permissions (the
  Shift+Tab cycle), the keys rows (Ctrl+S, Ctrl+B, Alt+P/T/O, Ctrl+Enter, Alt+Y, ? panel, :
  emoji), vim visual mode / text objects / '.', prompt suggestions in the box, the session recap
  after being away, @agent-name, askUserQuestionTimeout (a timer in the menu), /add-dir's Tab
  suggestions.
- Not built here, possible: nested subagents; Bash moved to the background at its time limit and
  a head+tail cut (sys.h run returns only the start); Edit's relaxed stale check; WebFetch's
  preapproved domains / localhost refusal / http->https; WebSearch domain lists; the Task* /
  Monitor / Cron tools; "an Edit allow grants Read"; hook types agent and http, async hooks,
  hooks in skill/agent frontmatter, FileChanged / MessageDisplay, Stop's extra input fields,
  watchPaths / defer; workspace trust; a warning per malformed settings entry; the other
  CLAUDE_CODE_* variables; agents' hooks / memory / color; skills' hooks / skillOverrides /
  disableSkillShellExecution; --system-prompt-snapshot; /skill-doctor's use counts; /skills'
  visibility toggle; /simplify's four parallel agents (one pass here); memory imports' depth 4 /
  escaped spaces / approval dialog; PermissionRequest's updatedInput.
- Not guessed: /advisor (the advisor tool's API was not in the reference this work had).
- MCP over HTTP: possible in principle, a project of its own; /mcp says it is not built.

## Rig steps (main session)

Not run here (no emulator). Fixture answers (tools/claude_fixture.py) exist only for its own
prompts; steps that need a new answer say so.

1. `Claude ?` -- the template has the new keywords after HELP/S (SESSION-ID/K ... the second
   line of the template). `Claude doctor` prints the checks; `Claude auth status` prints
   {"loggedIn":...}.
2. In the screen: `/help` ends with "Not on the Amiga (type one to see why): /mcp /plugin ...";
   `/mcp` and `/bug` say why. `/release-notes`, `/stats`, `/context` (By category), `/skills run`,
   `/skill-doctor`.
3. "please edit" (ROOT=RAM:, fixture: Edit of RAM:claude-test.txt), `/exit`, `Claude -c ROOT=RAM:`,
   `/rewind 1 code`: the file is back (checkpoints across a restart; T:Claude-cp/<hash>/index).
4. `/permissions` with no argument: the menus (Allow -> Read -> This project, only me): RAM:.claude/
   settings.local.json has "Read"; a Bash question offers "Yes, and don't ask again in this project".
5. Esc Esc on an empty box: the rewind menu offers "Summarize from here" and "Summarize up to here"
   (needs a model answer to complete).
6. ENVARC:Claude/settings.json {"statusLine":{"type":"command","command":"cat"}} under vsh: the
   row shows the JSON (context_window, effort, version ...); add "hideVimModeIndicator": true and
   /vim, i: no "-- INSERT --".
7. A Read of a PNG (needs a fixture answer with Read {"file_path":"x.png"}): the screen says
   "Read image (N KB)"; --dump shows an image block in the next request.
8. "show me the startup" with `List S:` asked by a fixture answer: no question (read-only).
9. `Claude -p --output-format stream-json --verbose --include-hook-events hi` with a SessionStart
   hook: hook_started / hook_response lines.
