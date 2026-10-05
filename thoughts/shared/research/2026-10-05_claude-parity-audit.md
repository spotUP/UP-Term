---
date: 2026-10-05
topic: C:Claude against Claude Code's documentation, feature by feature (A4 parity audit)
tags: [claude, a4, parity, audit]
status: draft
---

# C:Claude parity audit (2026-10-05)

Source: Claude Code's docs fetched 2026-10-05 as markdown (code.claude.com/docs/en/<page>.md):
interactive-mode, commands, tools-reference, settings, memory, hooks, slash-commands (= skills),
sub-agents, skills, output-styles, statusline, cli-reference, headless, checkpointing, costs.
Compared against the C code in claude/*.c on main e4bc55d (not against the A4 ledgers' claims).
Sets were counted from their builders: slash commands from `slash_builtin[]` + `repl_line`
(slash.c, repl.c), flags from `opts[]` (cli.c), tools from `defs[]` (tools.c), hook events from
`cfg_hook_events` (config.c), settings keys from `cfg_merge` (config.c), frontmatter keys from
`defs_parse` (commands.c), status-line JSON from `pol_statusline` (policy.c).

Status: **have** (file:line), **partial** (what is missing), **missing**, **N/A** (why not on an
Amiga). "Before" = main e4bc55d; "After" = branch feature/a4-gaps (see
thoughts/shared/plans/2026-10-05-a4-gaps-progress.md). Keys (interactive-mode's shortcut tables,
vim) belong to the parallel branch feature/a4-input-rest and are audited here but not built.

Counts are at the end (counted by tools/... no: by the script in the progress ledger's log, from
the Before/After columns of this file).

## 1. Slash commands (commands.md)

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| cmd | /add-dir <path> | partial | partial | slash.c:714 adds; no Tab suggestions of directories (input.c, other branch) |
| cmd | /advisor [model\|off] | missing | missing | server-side advisor tool; not built: its API beta is not in the claude-api reference used here |
| cmd | /agents | have | have | slash.c:836 lists agents (Claude Code now prints a reminder; ours lists) |
| cmd | /artifact-capabilities, /artifact-diagramming, /artifacts | N/A | N/A | claude.ai artifacts (account, browser) |
| cmd | /auto-mode-setup | N/A | N/A | auto mode needs Anthropic's action classifier (claude.ai plan) |
| cmd | /autocompact [auto\|<tokens>] | partial | have | slash.c:874 on/off only; the window size (auto/tokens) missing |
| cmd | /autofix-pr | N/A | N/A | cloud session watching a GitHub PR |
| cmd | /background, /fork (background copy), /stop, /subtask | N/A | N/A | background sessions need the agent-view supervisor and concurrent sessions (no threads: one task, one conversation) |
| cmd | /batch | N/A | N/A | bundled skill: parallel worktree agents (no git, no threads) |
| cmd | /branch [name] | have | have | slash.c:863 (sess_branch) |
| cmd | /btw [question] | missing | have | side question not added to the conversation |
| cmd | /bug, /feedback | N/A | N/A | send reports to Anthropic's feedback endpoint (claude.ai login); before: unknown command, after: explains |
| cmd | /cd <path> | have | have | slash.c:732 |
| cmd | /chrome, /claude-in-chrome | N/A | N/A | Chrome extension |
| cmd | /claude-api | N/A | N/A | bundled reference skill (megabytes of docs; WebFetch reaches them) |
| cmd | /clear [name] | partial | have | repl.c:1272; name for the previous conversation missing; cost totals not reset |
| cmd | /code-review, /review, /security-review, /ultrareview | N/A | N/A | git diff / PR / cloud review |
| cmd | /color [color\|default] | missing | missing | prompt bar colour: tui.c (feature/a4-input-rest's file) |
| cmd | /compact [instructions] | have | have | repl.c:872 |
| cmd | /config [key=value ...] | partial | have | slash.c:466 `KEY VALUE` only; 7 keys in the menu |
| cmd | /context [all] | partial | partial | repl.c:818 bar + used/left; no per-category grid |
| cmd | /copy [N] | missing | have | Nth-latest answer to the clipboard |
| cmd | /cost (alias of /usage) | partial | have | repl.c:1293 shows cost only |
| cmd | /dataviz, /design, /design-login, /design-sync, /slides | N/A | N/A | Claude Design / artifacts |
| cmd | /debug [description] | missing | have | bundled skill: debug logging on for the session |
| cmd | /deep-research, /workflows, /workflow-authoring | N/A | N/A | Workflow tool (multi-agent orchestration) |
| cmd | /desktop, /mobile, /ide | N/A | N/A | other apps |
| cmd | /diff | missing | have | no git: the files Claude changed this session (checkpoints) against now |
| cmd | /doctor | have | have | slash.c:300 |
| cmd | /effort [level\|auto\|status] | partial | have | repl.c:1258 levels only; auto/status missing |
| cmd | /exit, /quit | have | have | repl.c:1244 |
| cmd | /export [filename] | have | have | slash.c:385 (file or clipboard) |
| cmd | /fast [on\|off] | N/A | N/A | fast mode is a claude.ai-plan feature on specific models |
| cmd | /fewer-permission-prompts | N/A | N/A | bundled skill over transcripts + MCP |
| cmd | /focus | missing | missing | view toggle in tui.c (other branch's file) |
| cmd | /goal [condition\|clear] | missing | missing | not built (needs an evaluator model call after every turn; listed as left) |
| cmd | /heapdump | N/A | N/A | JavaScript heap |
| cmd | /help | have | have | repl.c:1165 |
| cmd | /hooks | have | have | slash.c:648 read-only list |
| cmd | /import | N/A | N/A | Codex / Gemini CLI / Cursor configuration do not exist on an Amiga |
| cmd | /init | have | have | repl.c:1287 |
| cmd | /insights, /team-onboarding | missing | missing | HTML report over all sessions: not built (low value) |
| cmd | /install-github-app, /install-slack-app, /web-setup | N/A | N/A | OAuth browser flows, gh CLI |
| cmd | /keybindings | missing | missing | keybindings file: keys.c (other branch's file) |
| cmd | /list-agents | N/A | N/A | cross-session messaging |
| cmd | /login, /logout | have | have | slash.c:583 (API key; OAuth N/A) |
| cmd | /loop [interval] [prompt] | missing | missing | needs a timer in the input loop (tui.c, other branch) |
| cmd | /mcp | N/A | N/A | MCP stdio servers need Node; MCP over HTTP not built (see 9.) |
| cmd | /memory | have | have | slash.c:522 (no auto-memory toggle) |
| cmd | /model [model] | partial | have | repl.c:1248; not saved as the default for new sessions |
| cmd | /output-style [style] | have | have | slash.c:190 |
| cmd | /passes, /privacy-settings, /rate-limit-options, /upgrade, /usage-credits, /stickers, /radio, /powerup | N/A | N/A | claude.ai subscription / browser |
| cmd | /permissions | partial | have | slash.c:98 text verbs; no interactive editor |
| cmd | /plan [description] | missing | have | plan mode from the prompt |
| cmd | /plugin, /reload-plugins, /plugin-authoring | N/A | N/A | plugin marketplace (Node packages) |
| cmd | /recap | missing | have | one-line summary of the session |
| cmd | /release-notes | missing | have | C:Claude's own changes |
| cmd | /reload-skills | missing | have | re-scan skills and commands |
| cmd | /remote-control, /remote-env, /schedule, /teleport | N/A | N/A | claude.ai cloud |
| cmd | /rename [name] | partial | have | slash.c:856; without a name: no generated name |
| cmd | /resume [session] | have | have | repl.c:1097 |
| cmd | /rewind (aliases /checkpoint, /undo) | partial | have | slash.c:744; no "Summarize from here", no /undo |
| cmd | /run, /verify, /run-skill-generator | missing | missing | bundled skills (launch the app); not built |
| cmd | /sandbox | N/A | N/A | OS sandbox (seatbelt / bubblewrap) |
| cmd | /scroll-speed, /tui | N/A | N/A | fullscreen renderer (C:Claude has one renderer) |
| cmd | /setup-bedrock, /setup-vertex | N/A | N/A | cloud providers' credentials |
| cmd | /simplify | missing | missing | bundled skill with four parallel agents: not built |
| cmd | /skill-doctor | missing | missing | not built |
| cmd | /skills | partial | partial | slash.c:838 plain list; no filter / visibility toggle |
| cmd | /stats (alias of /usage) | missing | have | |
| cmd | /status | have | have | slash.c:237 |
| cmd | /statusline [description] | partial | have | slash.c:879 takes a literal command; no description -> Claude sets it up; "clear" saved as the command (bug) |
| cmd | /tasks, /bashes | have | have | slash.c:844 |
| cmd | /terminal-setup | have | have | slash.c:328 (UP-Term check) |
| cmd | /theme | have | have | input.c:396 |
| cmd | /todos (C:Claude) | have | have | slash.c:674 |
| cmd | /update-config | missing | missing | bundled skill; not built |
| cmd | /usage | partial | have | slash.c:819; no per-model rows, durations |
| cmd | /vim (removed in Claude Code; /config editorMode) | have | have | input.c:398; editorMode in /config after |
| cmd | /voice | N/A | N/A | dictation needs a claude.ai account and audio upload |
| cmd | N/A commands answer with their reason | missing | have | before: "Unknown command" |
| cmd | /skill-name typed by the user | missing | have | slash.c:903 looks up commands only |

## 2. Command line (cli-reference.md, headless.md)

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| cli | `claude`, `claude "query"`, `-p`, piped stdin | have | have | cli.c, print.c:588 |
| cli | -c / --continue, -r / --resume ID or name, picker | have | have | cli.c:869 |
| cli | --resume <file.jsonl> | missing | have | ids and names only |
| cli | --add-dir, --agent, --allowedTools, --disallowedTools, --tools | have | have | cli.c:94-129 |
| cli | --agents JSON | missing | have | |
| cli | --append-system-prompt(-file), --system-prompt(-file) | have | have | cli.c:819 |
| cli | --append-subagent-system-prompt(-file) | missing | have | |
| cli | --autocompact auto\|tokens | missing | have | |
| cli | --bare | missing | have | |
| cli | --betas | missing | have | |
| cli | --dangerously-skip-permissions, --permission-mode | have | have | cli.c:239 (auto refused: classifier) |
| cli | --allow-dangerously-skip-permissions | missing | missing | bypass in the Shift+Tab cycle: tui.c (other branch) |
| cli | --debug, --debug-file | partial | have | DEBUG -> T:Claude.log only |
| cli | --disable-slash-commands | missing | have | |
| cli | --effort, --model, --fallback-model | have | have | fallback chain: first model only |
| cli | --fork-session, --name, --no-session-persistence | have | have | |
| cli | --forward-subagent-text | missing | have | |
| cli | --include-partial-messages, --input-format, --output-format | have | have | print.c |
| cli | --json-schema (structured_output) | missing | have | |
| cli | --max-budget-usd, --max-turns | have | have | |
| cli | --permission-prompts host\|none | missing | have | |
| cli | --replay-user-messages | missing | have | |
| cli | --safe-mode | missing | have | |
| cli | --session-id UUID | missing | have | ids were 8 hex digits |
| cli | --setting-sources | missing | have | |
| cli | --verbose (interactive: full turn-by-turn output) | partial | have | print mode only; screen folds results |
| cli | --version, --help | have | have | |
| cli | --exclude-dynamic-system-prompt-sections, --system-prompt-snapshot | missing | missing | C:Claude's prompt has no per-user dynamic section and is rebuilt per session; not built |
| cli | --include-hook-events, --init, --init-only, --maintenance (Setup hooks) | missing | missing | not built (no Setup hook event) |
| cli | --prompt-suggestions | missing | missing | not built (extra model call per turn) |
| cli | --mcp-config, --strict-mcp-config, --permission-prompt-tool | N/A | N/A | MCP (see 9.) |
| cli | --chrome, --no-chrome, --ide, --desktop, --cloud, --remote, --teleport, --remote-control, --rc, --environment, --ref, --bg, --exec, --tmux, --worktree, --teammate-mode, --channels, --plugin-dir, --plugin-url, --from-pr, --restricted, --ax-screen-reader, --advisor | N/A | N/A | browser, IDE, cloud, background supervisor, git worktrees, plugins, PR hosts; --restricted / --ax-screen-reader: evaluation harness and screen reader are not on AmigaOS |
| cli | subcommands update, install, gateway, agents, attach, logs, respawn, rm, stop, daemon, auto-mode, remote-control, self-hosted-runner, setup-token, ultrareview, plugin, mcp, import | N/A | N/A | Node install, background sessions, cloud, OAuth |
| cli | subcommands auth status/login/logout, doctor, purge | missing | missing | not built (the in-session /login /logout /doctor do it) |
| headless | stream-json: subagent messages with parent_tool_use_id | missing | have | print.c:187 always null |
| headless | stream-json input: image / document blocks | missing | have | text joined, images dropped (decided: base64 pass-through) |
| headless | system/api_retry event | missing | have | retries silent |
| headless | system/init, result shapes, exit codes | have | have | print.c:255, 376 (AmigaDOS 10/20 for 1) |

## 3. Interactive mode (interactive-mode.md) -- keys are feature/a4-input-rest's

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| keys | Ctrl+C, Ctrl+D, Ctrl+G / Ctrl+X Ctrl+E, Ctrl+L, Ctrl+O, Ctrl+R, Ctrl+T, Esc, Esc Esc, Shift+Tab, Up/Down, Ctrl+P/N | have | have | tui.c:1588-1760 |
| keys | Ctrl+A E K U W Y, Alt+B F D, Ctrl+_ undo | have | have | edit.c:373-440 |
| keys | Ctrl+S stash, Ctrl+B background, Ctrl+X Ctrl+K, Ctrl+Enter / Ctrl+X Ctrl+S send queued, Alt+Y paste ring, Alt+P model, Alt+T thinking, Alt+O fast | missing | missing | other branch (feature/a4-input-rest) |
| keys | Ctrl+V image paste | N/A | N/A | no image clipboard path on AmigaOS (IFF -> PNG would be its own feature) |
| keys | Ctrl+Z suspend | N/A | N/A | no job control on AmigaOS |
| keys | ? help panel, : emoji shortcodes, spell check | missing | missing | other branch |
| input | \ + Enter, Shift+Enter, Ctrl+J newline | have | have | edit.c |
| input | / commands, ! shell mode, @ mentions, # memory | have | have | input.c |
| vim | modes, motions, edits, counts | have | have | vim.c |
| vim | visual mode, text objects, '.', >> << | missing | missing | other branch |
| hist | history per project, Ctrl+R search | have | have | hist.c |
| bg | background Bash (run_in_background), /tasks | have | have | shells.c |
| queue | type-ahead queue, Up takes back | have | have | input.c |
| view | transcript viewer (Ctrl+O) | have | have | tview.c |
| view | task list (Ctrl+T) | have | have | tui.c |
| view | prompt suggestions, session recap after away, PR status, issue links, usage-limit wait | missing | missing | recap command built (/recap); the rest N/A (git hosts, claude.ai limits) or other branch |
| view | /diff panel | missing | have | as /diff (no git) |
| view | /btw side questions | missing | have | |

## 4. Tools (tools-reference.md)

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| tool | Agent (named Task here) | partial | have | tools.c:282 named Task; rules/hooks naming Agent did not match Task (config.c:345) |
| tool | Agent: disallowedTools, maxTurns | partial | have | fixed 60 rounds (subagent.c:15) |
| tool | Agent: background, resume via SendMessage, nested | missing | missing | one task, no threads; not built |
| tool | AskUserQuestion | have | have | tools.c:279 |
| tool | Bash | have | have | tools.c:1314 |
| tool | Bash: default timeout 2 min (description said 120000, ran 60 s) | partial | have | repl.c:1658 |
| tool | Bash: cd persists between commands | missing | missing | not built (AmigaShell runs each in the start dir) |
| tool | Bash: read-only commands without a prompt | missing | have | perm_must_ask always asked |
| tool | Bash: output head+tail on failure, spill file | partial | partial | cut at 30000 |
| tool | Edit, Write, MultiEdit | have | have | tools.c:950, 1146 |
| tool | Read | have | have | tools.c:845 |
| tool | Read: images, PDFs | missing | have | binary refused (tools.c:885) |
| tool | Read: notebooks | N/A | N/A | no Jupyter |
| tool | Glob, Grep | have | have | search.c |
| tool | WebFetch | have | have | webfetch.c |
| tool | WebFetch: 15-min cache, preapproved domains | missing | missing | not built |
| tool | WebSearch | have | have | tools.c:414 |
| tool | WebSearch: allowed/blocked domains | missing | missing | not built |
| tool | EnterPlanMode, ExitPlanMode, TodoWrite, Skill | have | have | tools.c |
| tool | TaskCreate/Get/List/Update, TaskOutput/TaskStop, Monitor, Cron*, ScheduleWakeup | missing | missing | not built (TodoWrite, BashOutput, KillShell cover the old ones) |
| tool | NotebookEdit, PowerShell, LSP, EnterWorktree, ExitWorktree, MCP resource tools, ToolSearch, Artifact, RemoteTrigger, PushNotification, SendUserFile, SendFeedback, Workflow, ListAgents | N/A | N/A | Jupyter, pwsh, language servers, git, MCP, claude.ai |
| rules | Bash(cmd *), Read(...)/Edit(...) globs, deny > ask > allow | have | have | config.c:530-686 |
| rules | Agent(x) | partial | have | Task(x) only |
| rules | ~/ in path rules | missing | missing | joined to the root (config.c:611) |

## 5. Hooks (hooks.md)

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| event | PreToolUse, PostToolUse, UserPromptSubmit, Stop, SubagentStop, SessionStart | have | have | policy.c |
| event | SessionEnd | partial | have | reason "exit" is not a documented value |
| event | PreCompact | partial | have | cannot block (hooks.c:136) |
| event | Notification | partial | partial | permission prompts only; no notification_type |
| event | PermissionRequest | missing | have | |
| event | PostToolUseFailure | missing | have | |
| event | SubagentStart | missing | have | |
| event | PostCompact | missing | have | |
| event | StopFailure | missing | have | |
| event | UserPromptExpansion | missing | have | |
| event | CwdChanged, DirectoryAdded | missing | have | |
| event | PreModelSwitch, PostModelSwitch | missing | missing | not built |
| event | InstructionsLoaded, PostToolBatch, ConfigChange, FileChanged, Setup, MessageDisplay | missing | missing | not built |
| event | PermissionDenied, TaskCreated, TaskCompleted, TeammateIdle, WorktreeCreate/Remove, Elicitation* | N/A | N/A | auto mode, task tools, teams, worktrees, MCP |
| config | user / project / local hooks; matchers "" * A\|B | have | have | config.c, hooks.c:47 |
| config | matcher regex (unanchored, ^Edit$) | partial | partial | wildcards only |
| config | `if` field | missing | have | |
| config | type command | have | have | |
| config | types prompt, agent, http | missing | missing | not built |
| config | timeout (default 600 s for command) | partial | have | default was 60 s |
| config | disableAllHooks | missing | have | |
| config | CLAUDE_PROJECT_DIR | missing | have | |
| config | CLAUDE_ENV_FILE, async, statusMessage, once | missing | missing | not built |
| config | /hooks menu | have | have | slash.c:648 |
| input | session_id, transcript_path, cwd, hook_event_name, stop_hook_active, tool_response, trigger | have | have | hooks.c:160 |
| input | permission_mode, tool_use_id | missing | have | |
| exit | 0 / 2 / other, timeout not blocking | have | have | hooks.c:197 |
| json | decision/reason, permissionDecision | have | have | hooks.c:95 |
| json | continue:false for tool hooks | partial | have | UserPromptSubmit and Stop only |
| json | systemMessage | missing | have | |
| json | additionalContext on PreToolUse | partial | have | |
| json | updatedInput (PreToolUse) | missing | have | |
| json | updatedToolOutput, terminalSequence, sessionTitle, watchPaths | missing | missing | not built |
| stop | Stop continuation cap 8 | partial | have | 3 |

## 6. Settings, memory (settings.md, memory.md)

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| files | user / project / local settings, --settings, precedence | have | have | config.c:274 |
| files | managed settings, ~/.claude.json | N/A | N/A | MDM / OAuth state |
| sec | bypassPermissions / auto ignored from project and local files | missing | have | repl.c:1597 took it from any file |
| keys | permissions.*, model, effortLevel, fallbackModel, outputStyle, theme, editorMode, autoCompactEnabled, statusLine, env, hooks | have | have | config.c:253-301 |
| keys | disableAllHooks, verbose, autoMemoryEnabled, claudeMdExcludes, hideVimModeIndicator (statusLine) | missing | have | |
| keys | agent | missing | have | |
| keys | apiKeyHelper, cleanupPeriodDays, attribution, spinnerTipsEnabled, availableModels, askUserQuestionTimeout, bashOutputMaxChars | missing | missing | not built |
| keys | plugins, sandbox, managed-only keys | N/A | N/A | |
| env | ANTHROPIC_MODEL, BASH_DEFAULT_TIMEOUT_MS, BASH_MAX_TIMEOUT_MS, CLAUDE_CODE_SIMPLE | missing | have | |
| env | other CLAUDE_CODE_* variables | missing | missing | not built |
| perm | "don't ask again" kept in settings.local.json | partial | partial | session-only answer (ui.c) |
| reload | settings re-read when a file changes | missing | missing | not built |
| mem | user, ancestors, project, .claude/CLAUDE.md, CLAUDE.local.md, AMIGA.md, nested on read, @imports | have | have | memory.c:153-190 |
| mem | ancestor / nested CLAUDE.local.md | missing | missing | not built |
| mem | HTML comments stripped | missing | have | |
| mem | .claude/rules/*.md (+ paths: frontmatter), ENVARC:Claude/rules | missing | have | |
| mem | claudeMdExcludes | missing | have | |
| mem | auto memory (MEMORY.md) | missing | have | |
| mem | AGENTS.md only without CLAUDE.md | partial | partial | always loaded |
| mem | import approval dialog | missing | missing | not built |

## 7. Subagents, skills, output styles, status line (sub-agents.md, skills.md, output-styles.md, statusline.md)

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| agent | built-ins general-purpose, Explore, Plan; user/project agents; override | have | have | subagent.c:18 |
| agent | built-in statusline-setup | missing | have | |
| agent | built-ins claude, claude-code-guide | missing | missing | not built |
| agent | frontmatter name, description, tools, model | have | have | commands.c:138 |
| agent | frontmatter disallowedTools, maxTurns, effort, skills, permissionMode | missing | have | |
| agent | frontmatter hooks, memory, background, color, initialPrompt | missing | missing | not built |
| agent | frontmatter mcpServers, isolation | N/A | N/A | MCP, worktrees |
| agent | CLAUDE.md for subagents (not Explore/Plan) | missing | have | |
| agent | --agents JSON, agent setting | missing | have | |
| agent | @agent-name mention | missing | missing | input.c (other branch) |
| skill | locations user/project; listing; Skill tool | have | have | commands.c:236, tools.c:1530 |
| skill | $ARGUMENTS / $N substitution in skills | missing | have | body returned as is |
| skill | $N 0-based, $ARGUMENTS[N], \$ (commands too) | partial | have | commands.c:385 1-based |
| skill | ${CLAUDE_SKILL_DIR}, ${CLAUDE_SESSION_ID}, ${CLAUDE_PROJECT_DIR}, ${CLAUDE_EFFORT} | missing | have | |
| skill | !`cmd` in skills | missing | have | commands only |
| skill | allowed-tools of a skill | missing | have | cl_skill had no field |
| skill | model of a skill | missing | have | |
| skill | user-invocable, when_to_use | missing | have | |
| skill | argument-hint shown | partial | have | parsed, never shown |
| skill | context: fork + agent | missing | have | |
| skill | hooks, paths, effort | missing | missing | not built |
| skill | subdirectory namespacing (/a:b) | missing | missing | not built |
| skill | bundled skills | missing | partial | /debug built in; the rest N/A or not built (see section 1) |
| style | Default, Explanatory, Learning | have | have | commands.c:251 |
| style | Proactive, Concise | missing | have | |
| style | keep-coding-instructions honoured | partial | have | parsed, never read |
| style | custom styles, /output-style, outputStyle | have | have | slash.c:190 |
| status | command, padding, refreshInterval, events, 300 ms | have | have | policy.c:420 |
| status | JSON: context_window, effort, vim, version, session_name, agent, durations, display_name | missing | have | |
| status | JSON: lines added/removed, rate_limits, pr, worktree | missing | N/A | rate limits: claude.ai; pr/worktree: git (lines counter: not built -> missing) |
| status | /statusline clear | missing | have | |

## 8. Checkpointing, costs (checkpointing.md, costs.md)

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| cp | snapshot per edit, /rewind code/conversation/both, Esc Esc | have | have | checkpoint.c, slash.c:744 |
| cp | Summarize from here | missing | have | |
| cp | checkpoints persist across sessions | missing | missing | T: snapshots dropped at exit; not built |
| cost | /usage, /cost, /stats | partial | have | |
| cost | /context per-category | partial | partial | |
| cost | /clear resets totals | partial | have | |
| cost | auto-compact, /compact focus | have | have | |

## 9. Not on the Amiga, with the reason (summary)

- MCP stdio servers: need Node/Python processes. MCP over HTTP: possible in principle (http.c,
  sse.c, json.c exist) but not built here; /mcp says so.
- Plugins, marketplace: Node packages.
- claude.ai account features: OAuth login, /usage plan limits, /passes, /privacy-settings,
  /upgrade, /voice, remote control, cloud sessions, routines, artifacts, Claude Design.
- Background sessions, agent teams, parallel subagents: one Amiga task runs one conversation.
- git-based: worktrees, /diff against git, PR tools, /review.
- OS integration: sandbox, Chrome, IDEs, desktop/mobile apps, Ctrl+Z, image paste.

## Counts

(filled at the end of the run)
