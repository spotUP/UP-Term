---
date: 2026-10-05
topic: A4 gaps round 2 -- the audit's "Not built here, possible" rows (C:Claude vs Claude Code docs)
tags: [claude, a4, parity, audit, progress]
status: draft
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

## Checklist (11 of 27)

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
- [ ] H1 type agent (a subagent with Read/Grep/Glob, ok/reason, 60 s)
- [ ] H2 type http (POST through the transport; headers, allowedEnvVars; response handling)
- [ ] H3 async hooks (async: true; results on the next turn)
- [ ] H4 hooks in skill / agent frontmatter (skill: from its use on, once; agent: while it runs,
      Stop -> SubagentStop)
- [ ] H5 FileChanged (matcher file list + watchPaths; change / add / unlink)
- [ ] H6 MessageDisplay (screen: per batch of lines; print mode: once per message)
- [x] H7 Stop's last_assistant_message, background_tasks, session_crons
- [ ] H8 SessionStart / CwdChanged watchPaths; PreToolUse defer in print mode
- [ ] H9 workspace trust: the first-run question per directory, remembered; hooks and project
      allow rules held until then; print mode: project allow rules need trust (warning)
- [ ] H10 PermissionRequest's updatedInput

Settings, agents, skills, CLI, memory
- [ ] S1 a warning per malformed settings entry (screen at start; Claude doctor)
- [ ] S2 the other CLAUDE_CODE_* variables that make sense (one table)
- [ ] A1 agents' memory (user / project / local) and color
- [ ] A2 skillOverrides, disableSkillShellExecution
- [ ] A3 /skill-doctor's use counts; /skills' visibility toggle
- [ ] A4 /simplify's agents (one after another on the Amiga, said in the skill)
- [ ] C1 --system-prompt-snapshot (recorded on the first request, kept until a compaction)
- [ ] M1 memory imports: depth 4, backslash-escaped spaces, quoted paths not imported, the
      external-import approval
- [ ] V1 /advisor (the advisor_20260301 server tool: /advisor, advisorModel, --advisor,
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

## Log

- c1 Phase A (T1-T10, H7) + the hook plumbing of phase B (types http/agent, async, frontmatter
  hooks, FileChanged watch -- tested in c2): claude_repl test_gaps2_tools (print mode and the line
  mode through run_print / repl_line / repl_run); claude_tools: declared set, availability by
  model, the search tool JSON, depth limit; claude_config: Edit/Read cross rules. Golden:
  print_stream system/init tools (no BashOutput/KillShell/TodoWrite on Opus 5.5; WebSearch,
  TaskStop, Monitor, Cron*). Mutations: sched_news off, nesting off (child depth 3), the relaxed
  unread edit off -- each fails the suite. Gate: make test (43 OK), make test-ref (149, 0
  failed), timeout 900 make amiga rc 0.

## Audit counts

Before (main 54c537e): have 173, partial 8, missing 32, N/A 34.
