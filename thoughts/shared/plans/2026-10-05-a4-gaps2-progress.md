---
date: 2026-10-05
topic: A4 gaps round 2 -- the audit's "Not built here, possible" rows (C:Claude vs Claude Code docs)
tags: [claude, a4, parity, audit, progress]
status: implemented
---

# A4 gaps 2 progress ledger

Branch feature/a4-gaps2 off main 54c537e. Audit:
thoughts/shared/research/2026-10-05_claude-parity-audit.md; the first round's ledger
thoughts/shared/plans/2026-10-05-a4-gaps-progress.md ("Left"). Docs fetched 2026-10-05 as
Markdown (code.claude.com/docs/en/{tools-reference,hooks,settings,memory,sub-agents,skills,
cli-reference,commands,permissions,env-vars,scheduled-tasks,advisor}.md, agent-sdk/typescript.md
for the tools' input schemas, platform.claude.com advisor-tool.md for the advisor's API).

Parallel: feature/a4-input-rest owns claude/vim.c keys.c edit.c tui.c input.c tview.c -- stay out
except one-line hooks named here. No emulator, no API call, no key. Gate before each commit:
make test, make test-ref, `timeout 900 make amiga` (VTCON_NDK / VTCON_NETINCLUDE / AMISSL_SDK from
the main checkout's vendor/). Do not merge.

Done = each row built, a host test in the CI glob (tests/test_claude_*.c via make test) driving it
through repl_line / print_run, gate green, committed, the audit row updated.

## Checklist (27 of 27)

Tools
- [x] T1 nested subagents (Task inside a subagent; CLAUDE_CODE_MAX_SUBAGENT_SPAWN_DEPTH, 3 layers)
- [x] T2 Bash moved to the background at its time limit ("Command did not complete within its Ns
      timeout and was moved to the background", sleep excepted, bare /
      CLAUDE_CODE_DISABLE_BACKGROUND_TASKS stop instead)
- [x] T3 Bash output head+tail cut (sys.h run keeps the tail; Amiga + POSIX)
- [x] T4 Edit's relaxed stale check (old_string matches the current text: edit, with a note) and
      a Bash cat/head/tail/... of one file counts as a read
- [x] T5 WebFetch: preapproved documentation domains, localhost / dotless host refused, http ->
      https, a redirect to another host returned as text
- [x] T6 WebSearch allowed_domains / blocked_domains (the client WebSearch tool of Claude Code;
      the server tool run in a side request), the session search cap
- [x] T7 TaskCreate / TaskGet / TaskList / TaskUpdate (the task list; TodoWrite with
      CLAUDE_CODE_ENABLE_TASKS=0), TaskStop; background output read with Read
- [x] T8 Monitor (a background command's lines fed to Claude between turns; deadline)
- [x] T9 CronCreate / CronDelete / CronList (session-local, the idle tick, local time via sys.h)
- [x] T10 an Edit allow grants Read; a Read deny blocks Edit / Write

Hooks
- [x] H1 type agent (a subagent with Read/Grep/Glob, ok/reason, 60 s)
- [x] H2 type http (POST through the transport; headers, allowedEnvVars; response handling)
- [x] H3 async hooks (async: true; results on the next turn)
- [x] H4 hooks in skill / agent frontmatter (skill: from its use on, once; agent: while it runs,
      Stop -> SubagentStop)
- [x] H5 FileChanged (matcher file list + watchPaths; change / add / unlink)
- [x] H6 MessageDisplay (screen: per batch of lines; print mode: once per message)
- [x] H7 Stop's last_assistant_message, background_tasks, session_crons
- [x] H8 SessionStart / CwdChanged watchPaths; PreToolUse defer in print mode
- [x] H9 workspace trust: the first-run question per directory, remembered; hooks and project
      allow rules held until then; print mode: project allow rules need trust (warning)
- [x] H10 PermissionRequest's updatedInput

Settings, agents, skills, CLI, memory
- [x] S1 a warning per malformed settings entry (screen at start; Claude doctor)
- [x] S2 the other CLAUDE_CODE_* variables that make sense (one table)
- [x] A1 agents' memory (user / project / local) and color
- [x] A2 skillOverrides, disableSkillShellExecution
- [x] A3 /skill-doctor's use counts; /skills' visibility toggle
- [x] A4 /simplify's agents (one after another on the Amiga, said in the skill)
- [x] C1 --system-prompt-snapshot (recorded on the first request, kept until a compaction)
- [x] M1 memory imports: depth 4, backslash-escaped spaces, quoted paths not imported, the
      external-import approval
- [x] V1 /advisor (the advisor_20260301 server tool: /advisor, advisorModel, --advisor,
      CLAUDE_CODE_DISABLE_ADVISOR_TOOL) -- the docs describe it as a public API server tool,
      so it is built, not N/A

## Decisions (do not re-litigate)

- /advisor is built: advisor.md says it is a server tool of the Messages API
  (platform.claude.com advisor-tool: type advisor_20260301, beta advisor-tool-2026-03-01).
- sys.h grew now (local time since 1978), pause (a wait that sees Ctrl+C), bg_size, bg_file,
  rename; Amiga (DateStamp, Delay, Examine, Rename) and POSIX. A foreground Bash command runs as a
  job polled to its end (shells_run_fg): the output read back head and tail, and at its time limit
  it is not stopped but adopted as a background task (bash_N), unless it starts with sleep or
  Wait, or background tasks are off (--bare, CLAUDE_CODE_DISABLE_BACKGROUND_TASKS). A cd inside a
  moved command: "Session cwd remains ...". sys->run stays for hooks, statusLine, !`cmd`.
- Bash output limits as tools-reference.md: a valid result inline to 30000 (bashOutputMaxChars,
  to 128000), past it the file kept as <job output>.out with the first 2000 characters; a failure
  (rc >= 10, a break) inline to 10000, past it the head and the tail. BASH_MAX_OUTPUT_LENGTH is the
  read-back window (to 150000) and is ignored when bashOutputMaxChars is set (Claude Code).
- Background news: a task that ended, a monitor's lines, a deadline. During a turn they go beside
  the round's results; between turns they are a turn of their own framed "<task-notification> No
  human input has occurred ..." (Claude Code's notice). Print mode: beside results only.
- BashOutput and KillShell are no longer declared (Claude Code dropped them for Read of the output
  file and TaskStop); they still run for old sessions and recordings. Read of a task's output file
  needs no question. TaskStop also stops monitors.
- WebSearch is Claude Code's client tool (query, allowed_domains, blocked_domains; not both): the
  conversation's model is asked in a request of its own with the web_search server tool (max_uses
  8, the domain list on the tool), the result "Web search results for query ... Links: [...]" and
  the model's summary. CLAUDE_CODE_MAX_WEB_SEARCHES_PER_SESSION, 200 by default: past it a notice,
  not an error. The webSearch setting and a WebSearch deny rule still take the tool away.
- Task tools by Claude Code's availability rule: Claude 3.x, Opus 4-4.7, Sonnet 4-4.6, Haiku 4.5
  get TaskCreate/Get/List/Update (TodoWrite with CLAUDE_CODE_ENABLE_TASKS=0); newer models get none
  unless CLAUDE_CODE_ENABLE_TODO_TOOLS=1 or --allowedTools / --tools names one. So C:Claude's
  default model (Opus 5.5) has no task tools now; the system prompt says "when you have them". The
  task list feeds the screen's list and /todos as TodoWrite's did.
- Monitor runs a command (one event per line); a ws watch is refused (no WebSocket client here).
  Its deadline: 5 minutes, at most 30 (10 in print mode). Bash rules cover it.
- Cron: 5-field expressions in local time (vixie-cron day rule), one-shot or recurring (7-day
  expiry, last fire then gone), jitter from the id (recurring up to half the interval / 30 min,
  one-shot at :00/:30 up to 90 s early), 50 jobs, 8-character ids. Fired between turns only: the
  screen's idle wait (tui.h wake; the one-line hook in tui.c tui_read:
  `if (t->wake && !t->ed.n && (q = t->wake(t->iu)) != 0) { ... return ...; }`, only with an empty
  box), the line mode before it waits for a line, never in print mode. durable: true also in
  <root>/.claude/scheduled_tasks.json (loaded at the start); the session's jobs in
  <session dir>/<id>.cron, restored on a resume. CLAUDE_CODE_DISABLE_CRON turns them off.
- WebFetch: a host without a dot refused before any request (Claude Code's error text, "curl"
  made "an HTTP client such as curl or wget"); http upgraded to https unless the URL names a port
  of its own (a local plain server: the fixture's) -- a deviation, Claude Code upgrades always;
  the preapproved documentation hosts fetch without a question unless a rule decides -- the docs
  name the set without listing it, the list is Claude Code's shipped one (UNVERIFIED against the
  docs).
- Edit's relaxed check: an unread file may be edited by a model newer than Opus 4.6 / Haiku 4.5
  when Read would need no question (inside the working directories, no deny/ask rule: pol_can_read);
  a file changed since its read is edited when old_string matches, the result says so. A Bash
  view of one file (cat nl bat head tail sed -n grep egrep fgrep rg, AmigaDOS Type; no pipe or
  redirection) counts as its read.
- Rules: an Edit allow covers Read/Grep/Glob on its paths; a Read deny covers Edit/Write/MultiEdit.
  A hook's "if" matches a compound Bash line when any part matches (hooks.md table:
  cfg_rule_match_any); allowed-tools keeps every-part.
- Nested subagents: child depth = parent + 1, Task declared while depth < max (3); a subagent's
  background commands stop when it ends (shells_end_owner).
- Stop / SubagentStop input: last_assistant_message (the text of the last answer), background_tasks
  (running tasks: id, type shell|monitor, status, description, command), session_crons (id,
  schedule, recurring, prompt cut at 1000 with "... [+N chars]").

- Hooks: an http hook POSTs the event's JSON through the same transport (cl_net, http.c);
  header values interpolate only the variables named in allowedEnvVars (others become empty);
  a 2xx with a JSON body is read as a command hook's answer, an empty 2xx is success, any other
  status or a connection failure is a non-blocking error. An agent hook is a subagent with Read,
  Grep, Glob (not on PermissionRequest) answering {"ok", "reason"}. async: true starts the command
  as a background job; its answer (additionalContext / systemMessage) reaches Claude beside a
  later round's results or as a turn of its own between turns.
- Frontmatter hooks: YAML in the frontmatter converted to the settings' JSON (commands.c
  yaml_json); a skill's from its first use to the session's end ("once" honoured only there); an
  agent's while it runs, its Stop run as SubagentStop; a project agent's only in a trusted folder.
- FileChanged: polled (2 s) between rounds and before a wait for a line -- no dos notify; the
  matcher names files in the start directory, SessionStart / CwdChanged / FileChanged watchPaths
  add absolute paths. MessageDisplay: the screen and the line mode hand batches of whole lines to
  the hook and draw its displayContent instead; print mode runs it once per message (final).
- defer: print mode only, and only when the round has one call (Claude Code); the run stops with
  stop_reason tool_deferred and deferred_tool_use; --resume ID (no prompt) runs the pending call
  through PreToolUse again and continues with CLAUDE_CODE_RESUME_PROMPT ("Continue from where you
  left off." by default).
- Workspace trust: asked at the first interactive start in a folder not trusted (nor inside one
  that is); no ends the program before any hook; yes is kept in <home>/claude.json
  projects[path].hasTrustDialogAccepted (the home directory: this session only). Print mode never
  asks: hooks run, the project's allow rules and additionalDirectories wait, a warning on stderr.
- PermissionRequest allow + updatedInput: the call runs again with the new input, the rules
  deciding anew (a deny still wins); the conversation keeps the original tool_use.
- Settings warnings: one line per skipped entry (file: what), shown at the start under "Settings
  Warning" and in /doctor; the rest of the file stays in effect.
- Env table: repl.c repl_env and its comment list every CLAUDE_CODE_* variable read. Not built,
  with reasons: CLAUDE_CODE_SHELL_PREFIX (AmigaDOS has no wrapper convention; Execute takes the
  line as is), CLAUDE_CODE_DISABLE_ATTACHMENTS (no @-file attachments outside input.c, the other
  branch), CLAUDE_CODE_DISABLE_TERMINAL_TITLE (the window title is tui.c, other branch). The
  SessionEnd 1.5 s budget (CLAUDE_CODE_SESSIONEND_HOOKS_TIMEOUT_MS) is not applied: hooks keep their
  own timeouts.
- Agents: memory user (<home>/agent-memory/NAME), project (.claude/agent-memory/NAME), local
  (.claude/agent-memory-local/NAME); MEMORY.md (200 lines / 25 KB) in its prompt, Read/Write/Edit
  there without a question; needs auto memory on. color: the call's header in that colour.
- skillOverrides (on, name-only, user-invocable-only, off) in every settings file; /skills NAME
  STATE writes settings.local.json. A skill Claude may not call is not listed to it, and the
  Skill tool answers "Unknown skill". disableSkillShellExecution replaces !`cmd` in non-bundled
  skills and commands with "[shell command execution disabled by policy]".
- /skill-doctor: counts kept in <home>/skill-usage.json (a use = the Skill tool or /name), the
  bundled skills not counted, the costliest first, the unused with a cost flagged.
- /simplify: four review agents one after another (one Amiga task: no parallel agents), said in
  the skill text.
- --system-prompt-snapshot: the --system-prompt / --append-system-prompt text recorded with the
  session (<session>.sys) on its first request and used by --continue / --resume until a
  compaction; off uses each launch's flags; --bare records only with on.
- Memory imports: four hops; "\ " is a space in a path; @"..." / @'...' and code spans are not
  imports; an import from a project file resolving outside the start directory is held until the
  dialog's yes (asked once per project, both answers kept in claude.json).
- Tests on a fake clock with real processes: sys_posix.bg_hold reports background jobs running
  so the screen's wiring test cannot race "Wait 2" (it flaked about 1 run in 8 under load).

## Log

- c1 Phase A (T1-T10, H7) + the hook plumbing of phase B (types http/agent, async, frontmatter
  hooks, FileChanged watch -- tested in c2): claude_repl test_gaps2_tools (print mode and the line
  mode through run_print / repl_line / repl_run); claude_tools: declared set, availability by
  model, the search tool JSON, depth limit; claude_config: Edit/Read cross rules. Golden:
  print_stream system/init tools (no BashOutput/KillShell/TodoWrite on Opus 5.5; WebSearch,
  TaskStop, Monitor, Cron*). Mutations: sched_news off, nesting off (child depth 3), the relaxed
  unread edit off -- each fails the suite. Gate: make test (43 OK), make test-ref (149, 0
  failed), timeout 900 make amiga rc 0.

- c2 Phase B/C (H1-H6, H8-H10, S1, S2, A1-A4, C1, M1, V1): claude_repl test_gaps2_hooks and
  test_gaps2_more (print mode, line mode, repl_run with the trust and import dialogs; an http
  hook's server stubbed on cl_net). The wiring test's 1-in-8 flake fixed (sys_posix.bg_hold).
  Mutations, each failing the suite: agent hook, http hook, async, skill hooks, agent hooks,
  FileChanged, MessageDisplay display, defer, trust "no", updatedInput rerun, settings warning,
  EXTRA_BODY, agent memory, agent color, skillOverrides listing, no-shell, use counts (typed),
  snapshot load, import depth 5, external import hold, advisor tool. Gate: make test (43 OK),
  make test-ref (149, 0 failed), timeout 900 make amiga rc 0.

## Audit counts

Before (main 54c537e): have 173, partial 8, missing 32, N/A 34 (247 rows).
After (feature/a4-gaps2): have 200, partial 2, missing 13, N/A 34 (249 rows: --advisor split out
of the N/A flag row, ScheduleWakeup out of the task-tools row). Counted from the tables' After
column by script, not by grep of the counts row.
