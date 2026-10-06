---
date: 2026-10-05
topic: A4 wiring -- WP1/WP2/WP3 seams joined (ext provider, WebSearch setting, status line row, /tasks, read default)
tags: [claude, a4, wiring, progress]
status: implemented
---

# A4 wiring progress ledger

Branch feature/a4-wiring off main d3683b4. Parallel: WP4 (main_amiga.c, CLI/print), input
leftovers (vim.c keys.c edit.c) -- stay out of those files. No emulator, no API call, no key.
Gate before each commit: make test, make test-ref, `timeout 900 make amiga`. Do not merge.

Done = each item wired, a host test in the CI glob proving it, one reachability test through
the REPL core (repl_run) covering the wires.

## Checklist

- [x] W1 ext provider: r->tools.ext filled from r->defs (agents user+project beside the
      built-ins, tools/model/prompt; skills listed + loaded; commands via SlashCommand with
      cmd_expand); tools JSON rebuilt on repl_load; disable-model-invocation honoured
- [x] W2 WebSearch switch: permissions.deny "WebSearch" and explicit "webSearch": false ->
      r->tools.web_search (+ /config key)
- [x] W3 status line: r->status_text drawn in its own footer row (under the box, SGR kept,
      multi-line), refreshed on Claude Code's events (start, assistant message, /compact,
      mode / vim change, command change, refreshInterval), 300 ms throttle, one-row update
- [x] W4 /tasks: shells.c's list is the one list (r->bg_list hook removed)
- [x] W5 read default: Read/Glob/Grep run without asking inside ROOT and added dirs; outside
      asks; rules and modes still decide (tl_resolve knows the added dirs; pol_pre's in_added
      gone)
- [x] W6 leftovers from the progress files (see below)
- [x] reachability test through repl_run (project agent via Task, Skill, SlashCommand,
      status line, read default)
- [x] W7 (coordinator, rig run): WebFetch's screen line is Claude Code's "Received N bytes
      (200 OK)" (cl_tools.brief -> cl_show.brief); the answer goes to Claude only
- [x] merge main (d455068: claude_fixture.py --dump, tools/rig/claude_rig2.py) into the branch
      -- bc6dde5, clean (no C files in it); claude_rig2.py still fits the read default (Read is
      allowed by its rule there anyway)

Running count: 9 of 9 host-side; rig sighting open (main session).

## Decisions

- The provider lives repl-side (policy.c pol_attach_tools / pol_tools): arrays of cl_agent / cl_skill /
  cl_command built from r->defs at repl_load, so ext.h stays WP2's and commands.c stays the
  loader. A project/user agent with a built-in's name hides the built-in (Claude Code: built-ins
  are the lowest priority).
- A command run through SlashCommand gets its allowed-tools for the rest of that turn, as a typed
  /command does (slash_custom). Its model is NOT switched mid-turn (the turn's thinking blocks
  belong to the model that started it); a typed /command still switches it for its turn.
- Subagent calls go through the same policy as the conversation's (cl_tools.call); the screen
  callbacks read r->at (the tools whose call runs).
- The status line runs on Claude Code's events with a 300 ms throttle (a held-back event runs at
  the next idle tick: synchronous here, no cancel of an in-flight run); a failure or no output
  blanks it; up to 4 lines, SGR kept, other escapes dropped; COLUMNS/LINES set.
- WebSearch: Claude Code has no settings switch; its way is permissions.deny ["WebSearch"]. The
  explicit toggle is C:Claude's own key "webSearch" (true/false), shown and set in /config.

## Leftovers found in the progress files and the seams (W6)

- Subagent tool calls bypassed policy.c (deny rules, PreToolUse/PostToolUse hooks, checkpoints,
  nested memory): wired through cl_tools.call = pol_call for the conversation and subagents alike.
- A subagent's calls were drawn with its Task's header (ui used r->tools.cur): r->at is the tools
  whose call runs.
- SubagentStop hook was parsed but never fired: cl_tools.agent_stop (exit 2 sends the agent on).
- The tools JSON was never rebuilt after a reload (/permissions, /config): pol_tools drops it.
- WP1 asked WP3 to reset the context after a conversation rewind: ctx_used estimated from the
  remaining bytes (4 a token) until the next request says.
- Left for WP4 (main_amiga.c, not touched here): --add-dir / --disallowedTools WebSearch must go
  into r->cfg (dirs / a session deny rule) and then call pol_tools (repl_int.h) or repl_load;
  r->tools.allowed for --tools; a keyless start straight into /login.
- Not wired (no interface for it, say so): a skill's allowed-tools (Claude Code allows them while
  the skill is active) -- ext.h's cl_skill has no tools field; run_skill reads the file itself.

## Log

- c1 (this commit): W1-W7 + tests: claude_repl test_wiring (screen, reachability: project +
  user agents in Task, project agent's prompt/model/tools, rule denies inside the agent, Skill,
  SlashCommand + its allowed-tools, /tasks, status line row + schedule, read default, WebSearch
  deny), test_fetch_screen (golden line); claude_tui status_line_row (row, SGR, one-row update,
  padding, multi-line); claude_config WebSearch switch + statusLine fields; claude_tools read
  default, added dirs, provided agent hides a built-in. Mutations checked: r->at, subagent
  policy routing, read default, ext unwired, web_search switch, status row, Fetch brief.
- 6ff2c80 the wiring; bc6dde5 merge of main d455068. Gate on 6ff2c80: make test (42 suites OK),
  make test-ref (149 streams, 0 failed), timeout 900 make amiga (exit 0; NDK/netinclude/AmiSSL
  from the main checkout's vendor/ via VTCON_NDK / VTCON_NETINCLUDE / AMISSL_SDK).

## Rig steps for the main session (fixture, not run here)

1. RAM:.claude/agents/x.md (name, description, tools: Read) -> "send an agent" prompt lists it
   (the fixture's Task asks for Explore; check /agents and the request body via --dump).
2. RAM:.claude/skills/s/SKILL.md and a command with a description: the first request body (--dump)
   declares Skill and SlashCommand with them.
3. ENVARC:Claude/settings.json {"statusLine":{"type":"command","command":"Echo WIRED"}}: the row
   "WIRED" under the box, above the facts line; Shift+Tab re-runs it.
4. permissions.deny ["WebSearch"]: "search the web" body has no web_search tool.
5. "show me the startup" with no allow rule: Read and Glob run with no menu.
6. "fetch the page": under Fetch(...) the line "Received N bytes (200 OK)".
7. "run in background" then /tasks: bash_1 listed with its state.
