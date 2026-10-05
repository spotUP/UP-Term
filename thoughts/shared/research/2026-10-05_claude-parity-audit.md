---
date: 2026-10-05
topic: C:Claude against Claude Code's documentation, feature by feature (A4 parity audit)
tags: [claude, a4, parity, audit]
status: final
---

# C:Claude parity audit (2026-10-05)

Source: Claude Code's docs fetched 2026-10-05 as Markdown (code.claude.com/docs/en/<page>.md):
interactive-mode, commands, tools-reference, settings, memory, hooks, slash-commands (= skills),
sub-agents, skills, output-styles, statusline, cli-reference, headless, checkpointing, costs, and
model-config for the auto-compact window. Compared against the C code in claude/*.c, not against
the A4 ledgers. Sets were counted from their builders: slash commands from `slash_builtin[]` and
`repl_line` (slash.c, repl.c), flags from `opts[]` (cli.c), tools from `defs[]` (tools.c), hook
events from `cfg_hook_events` (config.c), settings keys from `cfg_merge` (config.c), frontmatter
keys from `defs_parse` (commands.c), status-line JSON from `pol_statusline` (policy.c). Two
sub-audits (tools/hooks/settings/memory; agents/skills/styles/status line/headless/checkpoints/
costs) were read from the code by agents and spot-checked (the $N off-by-one and the project
bypassPermissions rule were confirmed against the docs and the code before acting on them).

Status: **have**, **partial** (what is missing), **missing**, **N/A** (why not on an Amiga).
"Before" = main e4bc55d; "After" = branch feature/a4-gaps 57613ae, and for the rows whose evidence
starts "gaps2:" branch feature/a4-gaps2 (ledger thoughts/shared/plans/2026-10-05-a4-gaps2-progress.md), "gaps3:" branch feature/a4-gaps3-loop (ledger thoughts/shared/plans/2026-10-05-a4-gaps3-loop-progress.md). Evidence for "After" is the
commit and the host test (tests/test_claude_*.c, all in `make test`); the progress ledger
thoughts/shared/plans/2026-10-05-a4-gaps-progress.md has the decisions. Keys (the shortcut
tables, vim) belong to the parallel branch feature/a4-input-rest and are audited, not built.

## 1. Slash commands (commands.md)

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| cmd | /add-dir <path> | partial | partial | slash.c add_dir; no Tab suggestions of directories (input.c, other branch) |
| cmd | /advisor [model\|off] | missing | have | gaps2: slash.c advisor, the advisor_20260301 server tool + its beta (tools.c, repl.c post), advisorModel, CLAUDE_CODE_DISABLE_ADVISOR_TOOL, the pairing table (conv.c conv_advisor_ok); test_gaps2_more V1 |
| cmd | /agents | have | have | slash.c list_defs |
| cmd | /artifact-capabilities, /artifact-diagramming, /artifacts, /design, /design-login, /design-sync, /slides, /dataviz | N/A | N/A | claude.ai artifacts and Claude Design (account, browser); typed, they say so |
| cmd | /auto-mode-setup | N/A | N/A | auto mode needs Anthropic's action classifier (claude.ai plan) |
| cmd | /autocompact [auto\|<tokens>] | partial | have | slash.c autocompact, autoCompactWindow, test_gaps_print |
| cmd | /autofix-pr, /ultrareview, /schedule, /remote-control, /remote-env, /teleport, /web-setup | N/A | N/A | cloud sessions, routines, GitHub through gh |
| cmd | /background, /fork (as a background copy), /stop, /subtask, /list-agents | N/A | N/A | background sessions need a supervisor and concurrent sessions: one Amiga task, one conversation (/fork is /branch here) |
| cmd | /batch | N/A | N/A | parallel worktree agents (no git, no threads) |
| cmd | /branch [name] | have | have | slash.c sess_branch |
| cmd | /btw [question] | missing | have | slash.c btw via repl_side; test_gaps_commands |
| cmd | /bug, /feedback | N/A | N/A | Anthropic's feedback service needs a claude.ai login; typed, they say so and point to /export |
| cmd | /cd <path> | have | have | slash.c cd (+ CwdChanged) |
| cmd | /chrome, /claude-in-chrome | N/A | N/A | the Chrome extension |
| cmd | /claude-api | N/A | N/A | bundled reference of megabytes; WebFetch reaches the docs |
| cmd | /clear [name] | partial | have | repl.c: names the old session, totals reset |
| cmd | /code-review, /review, /security-review | N/A | N/A | a git diff or a PR; no git on AmigaOS (/diff shows this session's changes) |
| cmd | /color [color\|default] | missing | missing | the prompt bar's colour is tui.c (other branch) |
| cmd | /compact [instructions] | have | have | repl.c repl_compact (+ PreCompact block, PostCompact) |
| cmd | /config [key=value ...] | partial | have | slash.c config: key=value, 11 keys in the menu |
| cmd | /context [all] | partial | have | repl.c context_parts: by category |
| cmd | /copy [N] | missing | have | slash.c copy_ (code-block picker) |
| cmd | /cost (alias of /usage) | partial | have | slash.c usage |
| cmd | /debug [description] | missing | have | slash.c debug (log path from main) |
| cmd | /deep-research, /workflows, /workflow-authoring | N/A | N/A | the Workflow tool runs many agents at once |
| cmd | /desktop, /mobile, /ide | N/A | N/A | other apps |
| cmd | /diff | missing | have | slash.c diff: the checkpoints' first copies against now (no git) |
| cmd | /doctor | have | have | slash.c doctor |
| cmd | /effort [level\|auto\|status] | partial | have | repl.c, repl_effort |
| cmd | /exit, /quit | have | have | repl.c |
| cmd | /export [filename] | have | have | slash.c export_ |
| cmd | /fast [on\|off] | N/A | N/A | a claude.ai plan feature |
| cmd | /fewer-permission-prompts | missing | have | bundled skill (commands.c); the audit's first draft had it N/A -- it reads Bash calls, not only MCP |
| cmd | /focus | missing | missing | a view toggle in tui.c (other branch) |
| cmd | /goal [condition\|clear] | missing | have | slash.c goal, repl.c goal_check (Haiku judges) |
| cmd | /heapdump, /radio, /stickers, /powerup | N/A | N/A | JavaScript heap, browser |
| cmd | /help | have | have | repl.c show_help (+ the N/A list) |
| cmd | /hooks | have | have | slash.c hooks_list |
| cmd | /import | N/A | N/A | Codex / Gemini CLI / Cursor do not run on an Amiga |
| cmd | /init | have | have | repl.c |
| cmd | /insights, /team-onboarding | missing | have | bundled skills (prompts) |
| cmd | /install-github-app, /install-slack-app | N/A | N/A | OAuth in a browser, gh |
| cmd | /keybindings | missing | missing | keys.c (other branch) |
| cmd | /login, /logout | have | have | slash.c (API key; OAuth N/A) |
| cmd | /loop [interval] [prompt] | missing | have | gaps3: Claude Code's bundled skill (commands.c sk_loop, alias /proactive; its cloud offer N/A); slash.c loop_ adds the default prompt (.claude/loop.md, ENVARC:Claude/loop.md, else the built-in maintenance prompt); interval: the model's CronCreate; no interval: ScheduleWakeup; print mode and CLAUDE_CODE_DISABLE_CRON refuse; test_gaps3_loop L1-L11 (ledger thoughts/shared/plans/2026-10-05-a4-gaps3-loop-progress.md) |
| cmd | /mcp, /plugin, /reload-plugins, /plugin-authoring | N/A | N/A | MCP stdio servers and plugins are Node/Python; typed, they say so |
| cmd | /memory | have | have | slash.c memory (+ auto on/off) |
| cmd | /model [model] | partial | have | kept as the default, availableModels, Pre/PostModelSwitch |
| cmd | /output-style [style] | have | have | slash.c output_style |
| cmd | /passes, /privacy-settings, /rate-limit-options, /upgrade, /usage-credits, /voice | N/A | N/A | claude.ai subscription and account |
| cmd | /permissions | partial | have | slash.c perm_editor (menus at the screen) |
| cmd | /plan [description] | missing | have | slash.c plan |
| cmd | /recap | missing | have | slash.c recap |
| cmd | /release-notes | missing | have | slash.c notes |
| cmd | /reload-skills | missing | have | slash.c reload_skills |
| cmd | /rename [name] | partial | have | slash.c rename_ (Claude names it) |
| cmd | /resume [session] | have | have | repl.c resume (+ FILE.jsonl) |
| cmd | /rewind (/checkpoint, /undo) | partial | have | Summarize from here / up to here, /undo |
| cmd | /run, /verify, /run-skill-generator | missing | have | bundled skills |
| cmd | /sandbox, /scroll-speed, /tui, /setup-bedrock, /setup-vertex | N/A | N/A | OS sandbox; fullscreen renderer; cloud providers' signing |
| cmd | /simplify | missing | have | gaps2: four review agents (Task), one after another -- one Amiga task runs no parallel agents, the skill says so; test_gaps2_more A4 |
| cmd | /skill-doctor | missing | have | gaps2: use counts kept in <home>/skill-usage.json (policy.c pol_skill_used), bundled skills not counted, unused flagged; test_gaps2_more A3 |
| cmd | /skills | partial | have | gaps2: /skills NAME on\|name-only\|user-only\|off and the screen menu, saved to settings.local.json skillOverrides; test_gaps2_more A3 |
| cmd | /stats | missing | have | slash.c usage |
| cmd | /status | have | have | slash.c status |
| cmd | /statusline [description] | partial | have | statusline-setup agent, clear, `command CMD` |
| cmd | /tasks, /terminal-setup, /theme, /todos, /vim | have | have | slash.c, input.c |
| cmd | /update-config | missing | have | bundled skill |
| cmd | /usage | partial | have | per model, durations |
| cmd | the N/A commands answer with their reason | missing | have | slash.c na_cmds, not_here |
| cmd | /skill-name typed | missing | have | slash.c slash_custom, menu_build |

## 2. Command line (cli-reference.md, headless.md)

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| cli | `claude`, `claude "query"`, `-p`, piped stdin | have | have | cli.c, print.c |
| cli | -c, -r ID or name, the picker | have | have | cli.c cli_session |
| cli | --resume FILE.jsonl | missing | have | session.c sess_load_file |
| cli | --add-dir, --agent, --allowedTools, --disallowedTools, --tools | have | have | cli.c |
| cli | --agents JSON (or a file) | missing | have | commands.c defs_add_agents_json |
| cli | --append-system-prompt(-file), --system-prompt(-file) | have | have | cli.c |
| cli | --append-subagent-system-prompt(-file) | missing | have | subagent.c agent_run |
| cli | --autocompact auto\|tokens | missing | have | cfg_window_parse, repl_compact_at |
| cli | --bare (CLAUDE_CODE_SIMPLE) | missing | have | repl_load, cli_apply |
| cli | --betas | missing | have | repl.c post |
| cli | --dangerously-skip-permissions, --permission-mode | have | have | cli.c (auto refused: classifier) |
| cli | --allow-dangerously-skip-permissions | missing | missing | bypass in the Shift+Tab cycle is tui.c (other branch) |
| cli | --debug, --debug-file | partial | have | main_amiga.c logname |
| cli | --disable-slash-commands | missing | have | repl_defs |
| cli | --effort, --model, --fallback-model | have | have | the fallback chain: its first model |
| cli | --fork-session, --name, --no-session-persistence | have | have | |
| cli | --forward-subagent-text | missing | have | print.c f_sub |
| cli | --include-partial-messages, --input-format, --output-format | have | have | print.c |
| cli | --json-schema (structured_output) | missing | have | StructuredOutput tool, policy.c structured |
| cli | --max-budget-usd, --max-turns | have | have | |
| cli | --permission-prompts host\|none | missing | have | tools.c tl_gate |
| cli | --replay-user-messages | missing | have | print.c replay |
| cli | --safe-mode | missing | have | repl_load |
| cli | --session-id UUID | missing | have | session.c sess_use_id |
| cli | --setting-sources | missing | have | cfg.skip |
| cli | --verbose (the screen: results unfolded) | partial | have | show.c body |
| cli | --exclude-dynamic-system-prompt-sections | missing | have | the auto memory section goes with the first prompt |
| cli | --system-prompt-snapshot | missing | have | gaps2: repl.c snap_record / snap_load (<session>.sys, until a compaction; off, --bare); test_gaps2_more C1 |
| cli | --include-hook-events | missing | have | print.c f_hook |
| cli | --init, --init-only, --maintenance (Setup hooks) | missing | have | repl_setup, print.c |
| cli | --prompt-suggestions | missing | have | print.c suggestion |
| cli | --mcp-config, --strict-mcp-config, --permission-prompt-tool | N/A | N/A | MCP |
| cli | --chrome, --no-chrome, --ide, --desktop, --cloud, --remote, --teleport, --remote-control, --environment, --ref, --bg, --exec, --tmux, --worktree, --teammate-mode, --channels, --plugin-dir, --plugin-url, --from-pr, --restricted, --ax-screen-reader | N/A | N/A | browser, IDE, cloud, background supervisor, git worktrees, plugins, PR hosts, evaluation harness, screen reader |
| cli | subcommands update, install, gateway, agents, attach, logs, respawn, rm, stop, daemon, auto-mode, remote-control, self-hosted-runner, setup-token, ultrareview, plugin, mcp, import | N/A | N/A | Node installer, background sessions, cloud, OAuth |
| cli | --advisor MODEL | N/A | have | gaps2: cli.c, refused at launch when it cannot advise the model; test_gaps2_more V1 |
| cli | subcommands doctor, auth status / login / logout, purge | missing | have | print.c print_subcommand |
| headless | stream-json: subagent messages (parent_tool_use_id) | missing | have | print.c f_sub |
| headless | stream-json input: image / document blocks | missing | have | base64 passed through (decided over N/A) |
| headless | system/api_retry | missing | have | repl.c request_retry |
| headless | system/init, result shapes, exit codes | have | have | print.c (AmigaDOS 10/20 for 1) |

## 3. Interactive mode (interactive-mode.md) -- keys are feature/a4-input-rest's

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| keys | Ctrl+C, Ctrl+D, Ctrl+G / Ctrl+X Ctrl+E, Ctrl+L, Ctrl+O, Ctrl+R, Ctrl+T, Esc, Esc Esc, Shift+Tab, Up/Down, Ctrl+P/N | have | have | tui.c |
| keys | Ctrl+A E K U W Y, Alt+B F D, Ctrl+_ | have | have | edit.c |
| keys | Ctrl+S, Ctrl+B, Ctrl+X Ctrl+K, Ctrl+Enter / Ctrl+X Ctrl+S, Alt+Y, Alt+P, Alt+T, Alt+O | missing | missing | other branch |
| keys | Ctrl+V image paste | N/A | N/A | no image clipboard path on AmigaOS (IFF -> PNG would be its own feature) |
| keys | Ctrl+Z | N/A | N/A | no job control on AmigaOS |
| keys | ? help panel, : emoji shortcodes, spell check | missing | missing | other branch |
| input | \ + Enter, Shift+Enter, Ctrl+J; / ! @ # | have | have | edit.c, input.c |
| vim | modes, motions, edits, counts | have | have | vim.c |
| vim | visual mode, text objects, '.', >> << | missing | missing | other branch |
| hist | history per project, Ctrl+R | have | have | hist.c |
| bg | background Bash, /tasks | have | have | shells.c |
| queue | type-ahead queue | have | have | input.c |
| view | transcript viewer, task list | have | have | tview.c, tui.c |
| view | prompt suggestions in the box | missing | missing | the box is tui.c (print mode has --prompt-suggestions) |
| view | session recap after being away | missing | missing | needs the idle tick in tui.c to draw; /recap is built |
| view | PR review status, issue links, usage-limit wait | N/A | N/A | git hosts, claude.ai limits |
| view | /diff | missing | have | slash.c diff |
| view | /btw | missing | have | slash.c btw |

## 4. Tools (tools-reference.md)

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| tool | Agent (declared as Task) | partial | have | rules and matchers naming Agent cover it (config.c tool_covers, hooks.c) |
| tool | Agent: disallowedTools, maxTurns, effort, skills, permissionMode | partial | have | subagent.c agent_run |
| tool | Agent: background, resume via SendMessage | missing | N/A | one task, no threads: an agent runs to its end |
| tool | Agent: nested subagents | missing | have | gaps2: subagent.c (depth, CLAUDE_CODE_MAX_SUBAGENT_SPAWN_DEPTH); test_gaps2_tools T1 |
| tool | AskUserQuestion | have | have | tools.c |
| tool | Bash | have | have | tools.c run_bash |
| tool | Bash: 2-minute default, BASH_DEFAULT_ / BASH_MAX_TIMEOUT_MS | partial | have | repl.c, tools.c |
| tool | Bash: cd persists | missing | have | tools.c cd_line |
| tool | Bash: read-only commands without a question | missing | have | tools.c bash_read_only |
| tool | Bash: output cap; head+tail on failure | partial | have | gaps2: shells.c shells_run_fg (a polled job: head and tail; large output kept as a file); test_gaps2_tools T3 |
| tool | Bash: moved to the background at its time limit | missing | have | gaps2: shells.c (sleep / Wait excepted; --bare, CLAUDE_CODE_DISABLE_BACKGROUND_TASKS stop it); test_gaps2_tools T2 |
| tool | Edit, Write, MultiEdit | have | have | tools.c |
| tool | Edit: relaxed stale check, a Bash cat counts as a read | missing | have | gaps2: tools.c rs_check_edit, bash_read_note; test_gaps2_tools T4 |
| tool | Read | have | have | tools.c run_read |
| tool | Read: images and PDFs | missing | have | tools.c read_media |
| tool | Read: notebooks | N/A | N/A | no Jupyter |
| tool | Glob, Grep | have | have | search.c |
| tool | Grep/Glob: .gitignore | N/A | N/A | no git |
| tool | WebFetch | have | have | webfetch.c |
| tool | WebFetch: 15-minute cache | missing | have | webfetch.c cache_find |
| tool | WebFetch: preapproved domains, localhost refusal, http -> https | missing | have | gaps2: webfetch.c; the host list is Claude Code's shipped one (UNVERIFIED: the docs do not list it); http kept when the URL names its own port (deviation); test_gaps2_tools T5 |
| tool | WebSearch | have | have | the API's server tool |
| tool | WebSearch: allowed / blocked domains | missing | have | gaps2: the client WebSearch tool, the server tool in a side request (webfetch.c websearch_run), the session cap; test_gaps2_tools T6 |
| tool | EnterPlanMode, ExitPlanMode, TodoWrite, Skill | have | have | tools.c |
| tool | TaskCreate/Get/List/Update, TaskStop, Monitor, CronCreate/Delete/List | missing | have | gaps2: tasks.c, shells.c, sched.c (TaskOutput: removed in Claude Code, Read of the output file instead); test_gaps2_tools T7-T9 |
| tool | ScheduleWakeup (/loop's self-paced mode) | missing | have | gaps3: tasks.c schedule_wakeup, the wakeup a one-shot job of the cron table (60..3600 s, no jitter, in CronList and session_crons, not resumed); fires as "Claude resuming /loop wakeup" (sched.c); stop, the 20-minute fallback, seven days, Esc / Ctrl+C on the idle box cancels; quiet (noop) iterations in a row fold into one line on the screen (tui_takeback); test_gaps3_loop, test_loop_screen |
| tool | NotebookEdit, PowerShell, LSP, worktrees, MCP resource tools, ToolSearch, Artifact, RemoteTrigger, PushNotification, SendUserFile, SendFeedback, Workflow, ListAgents | N/A | N/A | Jupyter, pwsh, language servers, git, MCP, claude.ai |
| rules | Bash(cmd *), Read/Edit globs, deny > ask > allow | have | have | config.c |
| rules | Agent(x) | partial | have | config.c tool_covers |
| rules | ~/ in path rules | missing | have | config.c path_match (HOME, else SYS:) |
| rules | an Edit allow grants Read; a Read deny blocks Edit | missing | have | gaps2: config.c rule_match; test_claude_config, test_gaps2_tools T10 |

## 5. Hooks (hooks.md)

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| event | PreToolUse, PostToolUse, UserPromptSubmit, Stop, SubagentStop, SessionStart | have | have | policy.c |
| event | SessionEnd | partial | have | reasons prompt_input_exit / clear / other |
| event | PreCompact | partial | have | blocks /compact |
| event | Notification | partial | have | notification_type permission_prompt, idle_prompt |
| event | PermissionRequest, PostToolUseFailure, SubagentStart, PostCompact, StopFailure, UserPromptExpansion, CwdChanged, DirectoryAdded | missing | have | policy.c; test_gaps_hooks |
| event | PreModelSwitch, PostModelSwitch, InstructionsLoaded, PostToolBatch, ConfigChange, Setup | missing | have | policy.c; test_gaps_more |
| event | FileChanged, MessageDisplay | missing | have | gaps2: watch.c (polled between rounds and turns, 2 s), repl.c md_batch; test_gaps2_hooks H5 H6 |
| event | PermissionDenied, TaskCreated, TaskCompleted, TeammateIdle, WorktreeCreate/Remove, Elicitation* | N/A | N/A | auto mode, task tools, teams, worktrees, MCP |
| config | user / project / local; matchers | have | have | config.c |
| config | matcher rules (exact list, regex unanchored) | partial | have | hooks.c hooks_match + regex.c |
| config | `if` | missing | have | hooks.c |
| config | type command | have | have | |
| config | type prompt | missing | have | hooks.c prompt_hook (Haiku) |
| config | types agent, http | missing | have | gaps2: hooks.c model_hook (a subagent with Read/Grep/Glob), http_hook (POST through the transport, allowedEnvVars); test_gaps2_hooks H1 H2 |
| config | timeout (600 s command, 30 s prompt) | partial | have | config.c |
| config | disableAllHooks | missing | have | |
| config | CLAUDE_PROJECT_DIR | missing | have | hooks.c project_cmd |
| config | CLAUDE_ENV_FILE | missing | have | policy.c env_file |
| config | statusMessage, once | missing | have | hooks.c |
| config | async / asyncRewake | missing | have | gaps2: hooks.c async_hook (a background job, its answer on a later round or turn); test_gaps2_hooks H3 |
| config | the same handler from several files once | missing | have | config.c hooks_of |
| config | hooks in skill / agent frontmatter | missing | have | gaps2: commands.c YAML to JSON, policy.c pol_skill_hooks / pol_agent_hooks (Stop as SubagentStop, once); test_gaps2_hooks H4 |
| config | workspace trust before project hooks | missing | have | gaps2: trust.c, repl.c repl_trust (asked once per folder, inherited by subfolders, kept in <home>/claude.json; print mode: a warning, project allow rules wait); test_gaps2_hooks H9 |
| config | /hooks | have | have | |
| input | session_id, transcript_path, cwd, hook_event_name, stop_hook_active, tool_response, trigger | have | have | |
| input | permission_mode, tool_use_id | missing | have | policy.c tool_event |
| input | Stop's last_assistant_message, background_tasks | missing | have | gaps2: policy.c stop_members (+ session_crons); test_gaps2_tools H7 |
| exit | 0 / 2 / other; JSON read on every exit code | partial | have | hooks.c |
| json | decision/reason, permissionDecision, continue:false, systemMessage, additionalContext (PreToolUse too) | partial | have | hooks.c, policy.c |
| json | updatedInput (PreToolUse), updatedToolOutput (PostToolUse), terminalSequence | missing | have | |
| json | SessionStart sessionTitle, initialUserMessage, reloadSkills | missing | have | |
| json | SessionStart watchPaths, permissionDecision defer | missing | have | gaps2: hooks.c json_answer, print.c tool_deferred + resume; test_gaps2_hooks H5 H8 |
| json | PermissionRequest updatedInput | missing | have | gaps2: policy.c pol_call runs the call again with it; test_gaps2_hooks H10 |
| stop | Stop cap 8 (CLAUDE_CODE_STOP_HOOK_BLOCK_CAP) | partial | have | repl.c stop_cap |

## 6. Settings, memory (settings.md, memory.md)

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| files | user / project / local, --settings, precedence | have | have | config.c |
| files | managed settings, ~/.claude.json | N/A | N/A | MDM, OAuth state |
| sec | bypassPermissions / auto ignored from project and local files | missing | have | config.c cfg_merge |
| keys | permissions.*, model, effortLevel, fallbackModel, outputStyle, theme, editorMode, autoCompactEnabled, statusLine, env, hooks | have | have | |
| keys | disableAllHooks, verbose, agent, autoMemoryEnabled, claudeMdExcludes, autoCompactWindow, statusLine.hideVimModeIndicator | missing | have | config.c |
| keys | apiKeyHelper, availableModels, bashOutputMaxChars, cleanupPeriodDays | missing | have | config.c, repl.c (cleanup only when set) |
| keys | askUserQuestionTimeout | missing | missing | needs a timer in the menu (tui.c) |
| keys | attribution, includeGitInstructions, spinnerTipsEnabled | N/A | N/A | git commit trailers; there are no spinner tips |
| keys | plugins, sandbox, managed-only keys | N/A | N/A | |
| env | ANTHROPIC_MODEL, BASH_DEFAULT_TIMEOUT_MS, BASH_MAX_TIMEOUT_MS, BASH_MAX_OUTPUT_LENGTH, CLAUDE_CODE_SIMPLE, CLAUDE_CODE_AUTO_COMPACT_WINDOW, CLAUDE_CODE_STOP_HOOK_BLOCK_CAP, CLAUDE_CODE_DISABLE_AUTO_MEMORY | missing | have | |
| env | the other CLAUDE_CODE_* variables | missing | have | gaps2: repl.c repl_env (one table, the list in its comment); not built: CLAUDE_CODE_SHELL_PREFIX, DISABLE_ATTACHMENTS, DISABLE_TERMINAL_TITLE (see the gaps2 ledger); test_gaps2_more S2 |
| perm | "Yes, and don't ask again" kept in settings.local.json | partial | have | ui.c ASK_PROJECT, policy.c pol_keep_rule |
| reload | settings read again when a file changes | missing | have | repl.c settings_changed (+ ConfigChange) |
| warn | a warning for one malformed entry | missing | have | gaps2: config.c cfg_warn, Settings Warning at the start, /doctor; test_gaps2_more S1 |
| mem | user, ancestors, project, .claude/CLAUDE.md, CLAUDE.local.md, AMIGA.md, nested on read, @imports | have | have | memory.c |
| mem | CLAUDE.local.md in ancestors and nested directories | missing | have | memory.c dir_files |
| mem | AGENTS.md only where there is no CLAUDE.md | partial | have | memory.c dir_files |
| mem | HTML comments left out | missing | have | memory.c strip_comments |
| mem | .claude/rules, ENVARC:Claude/rules, paths: | missing | have | memory.c rules_in, mem_rules |
| mem | claudeMdExcludes | missing | have | |
| mem | auto memory (MEMORY.md) | missing | have | memory.c mem_auto |
| mem | import depth 4, backslash-escaped spaces, the external-import dialog | partial | have | gaps2: memory.c imports, repl.c repl_ext_imports; test_gaps2_more M1 |

## 7. Subagents, skills, output styles, status line

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| agent | general-purpose, Explore, Plan; user/project agents override | have | have | subagent.c |
| agent | built-ins statusline-setup, claude-code-guide, claude | missing | have | subagent.c builtins |
| agent | frontmatter name, description, tools, model | have | have | commands.c |
| agent | disallowedTools, maxTurns, effort, skills, permissionMode, initialPrompt | missing | have | commands.c, subagent.c, cli.c |
| agent | hooks, memory, color | missing | have | gaps2: subagent.c agent_memory, policy.c pol_agent_hooks, ui.c name_sgr; test_gaps2_more A1, test_gaps2_hooks H4 |
| agent | background, isolation, mcpServers | N/A | N/A | threads, worktrees, MCP |
| agent | CLAUDE.md for agents but Explore / Plan | missing | have | subagent.c gets_memory |
| agent | --agents JSON, "agent" setting | missing | have | |
| agent | @agent-name in the prompt | missing | missing | input.c (other branch) |
| skill | locations, listing, the Skill tool | have | have | |
| skill | expanded as a command ($ARGUMENTS, $0, $ARGUMENTS[N], $name, \$, ${CLAUDE_*}, !`cmd`) | missing | have | commands.c cmd_expand_vars, policy.c ext_skill |
| skill | allowed-tools and model; effort when typed | missing | have | pol_turn_tools, slash.c |
| skill | user-invocable, when_to_use, argument-hint, context: fork + agent | missing | have | |
| skill | paths: | missing | have | policy.c skills_for |
| skill | nested .claude/skills | missing | have | policy.c skills_for |
| skill | commands in a subdirectory as dir:name | missing | have | commands.c load_dir_as |
| skill | hooks; skillOverrides; disableSkillShellExecution | missing | have | gaps2: policy.c pol_tools / ext_skill, commands.c; test_gaps2_more A2, test_gaps2_hooks H4 |
| skill | bundled skills | missing | partial | eight as prompts; the others N/A (see section 1) |
| style | Default, Explanatory, Learning, Proactive, Concise | partial | have | commands.c |
| style | keep-coding-instructions; outputStyle case-sensitive | partial | have | repl.c repl_system, repl_load |
| style | custom styles, /output-style | have | have | |
| status | command, padding, refreshInterval, events, 300 ms | have | have | policy.c |
| status | JSON: display name, durations, lines added/removed, context_window, effort, thinking, vim, agent, session_name, version, project_dir | missing | have | policy.c status_rest |
| status | JSON: rate_limits, pr, worktree, git fields | N/A | N/A | claude.ai limits, git |
| status | hideVimModeIndicator | missing | have | tui.c one-line hook |
| status | /statusline clear | missing | have | |

## 8. Checkpointing, costs

| Area | Feature | Before | After | Evidence / what is missing |
|---|---|---|---|---|
| cp | a snapshot per edit; /rewind code / conversation / both; Esc Esc | have | have | checkpoint.c |
| cp | Summarize from here / up to here | missing | have | repl.c repl_summarize |
| cp | checkpoints across sessions | missing | have | checkpoint.c cp_session (T:, until a reboot) |
| cost | /usage, /cost, /stats | partial | have | |
| cost | /context by category | partial | have | |
| cost | /clear resets the totals | partial | have | |
| cost | auto-compact, /compact focus | have | have | |

## 9. Not on the Amiga, with the reason (summary)

- MCP stdio servers need Node/Python processes; MCP over HTTP is possible in principle (http.c,
  sse.c, json.c exist) but is a project of its own and not built; /mcp says so.
- Plugins and their marketplace: Node packages.
- claude.ai account features: OAuth, plan limits, /passes, /privacy-settings, /upgrade, /voice,
  remote control, cloud sessions, routines, artifacts, Claude Design, fast mode, auto mode.
- Background sessions, agent teams, parallel agents: one Amiga task runs one conversation.
- git: worktrees, PR tools, /review and /security-review, .gitignore.
- OS integration: sandbox, Chrome, IDEs, desktop and phone apps, Ctrl+Z, image paste.

## Counts

249 rows (a row may group several features of one kind; the counts are rows). Round 2 split
two rows: --advisor out of the N/A flag list (built), ScheduleWakeup out of the task-tools row
(with /loop, not built); the first round's counts are of the 247 rows then.

| | have | partial | missing | N/A |
|---|---|---|---|---|
| Before (main e4bc55d), 247 rows | 58 | 37 | 119 | 33 |
| After (feature/a4-gaps), 247 rows | 173 | 8 | 32 | 34 |
| After round 2 (feature/a4-gaps2), 249 rows | 200 | 2 | 13 | 34 |

Left after round 2: the 13 missing and the 2 partial rows are the input branch's (keys, vim,
/color, /focus, /keybindings, the box's suggestions, @agent-name, /add-dir's Tab suggestions,
--allow-dangerously-skip-permissions in the Shift+Tab cycle), the menu timer
(askUserQuestionTimeout), /loop with ScheduleWakeup, the session recap, and the bundled skills
that stay N/A.

