---
date: 2026-10-05
topic: A4 WP2 -- C:Claude's tools at Claude Code parity (progress ledger)
tags: [claude, a4, wp2, tools, progress]
status: draft
---

# A4 WP2 progress ledger

Plan: thoughts/shared/plans/2026-10-05-a4-claude-parity.md, rows 2.1-2.13. Branch
feature/a4-wp2-tools off main a95ec55. Rules for this run: no emulator, no real API call, no key;
stay in claude/tools.c, claude/path.c and new files; repl.c / show.c / ui.c only small hooks.
WP1 (keys edit tui show) and WP3 (conv repl config session) run in parallel.

## Decisions (do not re-litigate)

- Tool names and input schemas are Claude Code's: Read Write Edit MultiEdit Glob Grep Bash
  BashOutput KillShell WebFetch TodoWrite AskUserQuestion ExitPlanMode EnterPlanMode Task Skill
  SlashCommand, plus the API's web_search server tool. The A2 names (read_file ...) are NOT
  declared any more: saved sessions only replay history, they never run a tool, so they do not
  need them; a call of an old name gets an is_error naming the new tool.
- One source of truth for the schemas: the declared tools JSON. Validation walks the declared
  input_schema (types, required, additionalProperties false, enum, array items); tool-specific
  range checks are in code.
- Read-only tools still ask once per session (A2's owner rule), one answer covers all reads.
- Grep: own regex engine (claude/regex.c), Thompson/Pike NFA simulation -- linear time, no
  backtracking blow-up on a 68k. Glob: claude/glob.c, ** * ? [..] {a,b} and AmigaDOS #? (a|b);
  case-insensitive as AmigaDOS names are.
- Bash: vsh when C:vsh exists, else the AmigaShell (Execute of the T: script). rc >= 10 is an
  error result (5 is WARN on AmigaDOS). Background shells: the output file is opened
  MODE_READWRITE (shared lock) so BashOutput can read it while it grows.
- Providers for .claude/agents, skills and commands are an interface (claude/ext.h) that WP3
  fills; built-in agents (general-purpose, Explore, Plan) live in subagent.c.

## Checklist (host-tested; [rig] = seen against tools/claude_fixture.py, main session's)

- [x] 2.1 Read: cat -n, offset/limit, 2000-line default, long lines cut, 256 KB whole / 2 MB with
      offset, binary refused, empty and short-file reminders -- a5a8c5b [rig open]
- [x] 2.2 Write: create/overwrite, Read-first and unchanged-since checks, diff preview, Latin-1
      kept on replace -- a5a8c5b [rig open]
- [x] 2.3 Edit (replace_all, Claude Code's messages, snippet, create with empty old_string),
      MultiEdit (sequential, atomic) -- a5a8c5b [rig open]
- [x] 2.4 Glob: * ** ? [..] {a,b}, #? (a|b), case-insensitive, newest first, 100 shown -- a5a8c5b
- [x] 2.5 Grep: regex.c (ERE + \d\w\s\b (?i), linear time), -i -n -A/-B/-C, glob, type,
      content / files_with_matches / count, head_limit, offset, multiline -- a5a8c5b
- [x] 2.6 Bash (vsh else AmigaShell, timeout ms, 30000-char cap, rc), run_in_background +
      BashOutput (filter) + KillShell (break), stopped at exit -- a5a8c5b [rig open: Amiga side
      compiled only]
- [x] 2.7 WebFetch: GET over net.h (AmiSSL on the Amiga), same-host redirects, other host
      reported, HTML -> Markdown, 100000-char cut, Haiku 4.5 answers the prompt -- a5a8c5b
- [x] 2.8 WebSearch: web_search_20260209 (20250305 for Haiku) declared, server_tool_use and
      web_search_tool_result shown and echoed, pause_turn resumed; switch = cl_tools.web_search
      (WP3 wires the setting) -- a5a8c5b
- [x] 2.9 AskUserQuestion: 1-4 questions, 2-4 options, multiSelect, own answer; framed menu
      (screen) or numbered (line mode) -- a5a8c5b
- [x] 2.10 ExitPlanMode (plan shown as Markdown, auto-accept / manual / keep planning),
      EnterPlanMode -- a5a8c5b
- [x] 2.11 Task: nested conversation via repl.c api_send (the turn's stream kept aside), agent's
      tools subset + prompt + model alias, built-ins general-purpose / Explore / Plan, provider
      agents (ext.h) -- a5a8c5b
- [x] 2.12 Skill: provider skills listed, SKILL.md body without frontmatter, args -- a5a8c5b
      (loader: WP3)
- [x] 2.13 SlashCommand: provider commands with a description listed, expand() -- a5a8c5b
      (loader: WP3)

Running count: 13 of 13 built and host-tested; rig sighting open for all (main session).

## Interface for WP3 (claude/ext.h)

cl_ext { agents(), skills(), commands(), expand() } -- set `r->tools.ext = &ext` after
repl_init (before the first turn: the tools JSON is built once; free(r->tools.json),
r->tools.json = 0 to rebuild after a reload). Settings hooks: `r->tools.web_search = 0` (the
WebSearch switch), `r->tools.allowed` (bit per T_*: --tools / --disallowedTools for WP4).

## Files touched outside the WP2 set (for the merge)

- claude/repl.c: tools_json(&r->tools, model) x2, pause_turn continue, sui.stop + st_stop,
  B_SERVER status, tool_choose / tool_plan / api_send, tools_init/tools_free, system prompt and
  /init prompt name the new tools.
- claude/show.c/.h: names via tools_title, header via tools_args, default result via
  tools_summary, show_server; T_* renamed.
- claude/ui.c/.h: ui_choose, ui_server; tui_ask titles via tools_title.
- claude/stream.c/.h: B_SERVER (server_tool_use input collected and echoed), ui.stop callback.
- claude/sys.h, sys_posix.c/.h, sys_amiga.c/.h: dirent mtime, mtime(), bg_start/read/kill/drop,
  AmigaShell fallback when C:vsh is absent.
- claude/main_amiga.c: a second net_amiga for WebFetch (r->tools.web).
- Makefile (new sources, tests/test_claude_match.c), tests/test_main.c (suite claude_match),
  RULES.md (suite list, fixture prompts), tools/claude_fixture.py.

## Rig steps (main session; fixture: python3 tools/claude_fixture.py)

Echo hello >RAM:claude-test.txt, Claude URL=http://127.0.0.1:8080/v1/messages ROOT=SYS: unless
said otherwise. Expect each tool's header and result as in Claude Code:
1. "show me the startup" -> Read S/Startup-Sequence + Glob * in S (menu "2" allows reads).
2. "grep" -> Grep ^Set\w+ in S, content with line numbers.
3. "please edit" (ROOT=RAM:) -> TodoWrite, Read, Edit diff, RAM:claude-test.txt changed.
4. "search the web" -> Web Search("Amiga 1200 accelerator cards"), "Did 1 search: 2 results".
5. "fetch the page" -> Fetch(http://127.0.0.1:8080/page): fixture logs GET /page, then a
   claude-haiku-4-5 request; the answer comes back in the result.
6. "send an agent" -> Task(Find the startup files), nested Search (Grep) asked, report returned.
7. "a question" -> AskUserQuestion menu (two options + Type something else).
8. "plan it" -> Plan Mode question, then the plan drawn and the three-way approval.
9. "run in background" -> Bash(Wait 2) id bash_1, then BashOutput: running/completed.
