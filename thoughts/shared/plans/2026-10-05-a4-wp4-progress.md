---
date: 2026-10-05
topic: A4 WP4 -- C:Claude's command line and print mode (rows 4.1-4.5 + keyless start), progress ledger
tags: [claude, a4, wp4, cli, progress]
status: draft
---

# A4 WP4 progress ledger

Plan: thoughts/shared/plans/2026-10-05-a4-claude-parity.md, rows 4.1-4.5 (4.6 is not in this
package). Branch feature/a4-wp4-cli off main d3683b4. Parallel: an agent on wiring (repl.c ext
hooks, tui status line) and one on input leftovers (vim.c keys.c edit.c): repl.c edits stay
small and are listed below. No emulator, no real API call, no key. Not merged here.

Done = built, host-tested in the CI glob (suite claude_cli: parsing both syntaxes, each output
format against golden JSON, print mode through the REPL core with recorded streams, the
--max-turns / --max-budget-usd stops, --continue restoring a session), make test, make
test-ref, `timeout 900 make amiga` green. Rig check: main session's.

## Checklist

- [x] 4.1 `Claude "prompt"` starts the session with it; -p / PRINT one-shot; stdin piped in
- [x] 4.2 --output-format text|json|stream-json (+ --include-partial-messages); --input-format stream-json
- [x] 4.3 -c/--continue, -r/--resume [NAME], -n/--name, --fork-session, --no-session-persistence
- [x] 4.4 --model --effort --permission-mode --allowedTools --disallowedTools --tools --add-dir
      --append-system-prompt(-file) --system-prompt(-file) --settings --max-turns --max-budget-usd
      --verbose --agent --fallback-model
- [x] 4.5 both forms: Unix flags and AmigaDOS keywords (MODEL=, PRINT/S ...), old keywords kept
- [x] K  keyless start: no key -> straight to /login (screen and line mode); print mode: error result
- [x] T  tests: suite claude_cli in the CI glob, golden files tests/claude/print_*.{txt,json,jsonl}
- [x] D  RULES.md commands, this ledger, rig steps

## Decisions (do not re-litigate)

- One portable parser (claude/cli.c) for both syntaxes, over the raw argument line (AmigaDOS
  GetArgStr; quoting "..." with *" *N **): a token starting with - is a Unix flag (--x=v or
  --x v); otherwise a template keyword (case-insensitive, '-' ignored: OUTPUT-FORMAT= and
  OUTPUTFORMAT= both) as KEY=v or KEY v; otherwise the prompt. As ReadArgs' PROMPT/F, once the
  prompt has started the rest of the line is prompt for keywords; `--` ends the flags. `?`
  prints the template and reads a line, as ReadArgs does. Not ReadArgs itself: it cannot
  parse --flags and is not host-testable.
- --allowedTools / --disallowedTools / --add-dir take several values (Claude Code's
  variadic flags): following tokens up to the next flag. Items split on , and blanks outside
  parentheses ("Bash(git log *)" is one item).
- The command line becomes a settings layer: --settings (file or inline JSON) and then a JSON
  built from the flags (model, effortLevel, fallbackModel, permissions allow/deny/defaultMode/
  additionalDirectories) are merged at CFG_SESSION after the three files, on every repl_load
  (so /cd keeps them). --model etc. thereby win over every file.
- Bare names in --disallowedTools and --tools restrict the declared tools (r->tools.allowed,
  web_search bit); scoped --disallowedTools rules are deny rules.
- Permission modes: default/manual, acceptEdits, plan (PERM_*), dontAsk and bypassPermissions as
  r->ask_policy (a question is denied / answered yes); auto is refused (needs Claude Code's
  classifier). Print mode has nobody to ask: a question is denied and listed in
  permission_denials -- except a read-only tool inside the start directory, which runs (Claude
  Code: reads in the working directories need no approval; the A2 ask-once-for-reads rule is for
  a person at the screen). Explicit ask rules still ask in bypass mode (Claude Code).
- Print mode output follows Claude Code's SDK shapes (Agent SDK TypeScript reference, fetched
  2026-10-05): system/init, assistant, user, stream_event, result (success / error_max_turns /
  error_max_budget_usd / error_during_execution). An API failure is subtype success with
  is_error true and the error text as result (as Claude Code prints "Invalid API key ..."). Exit
  code 0, else 10 (AmigaDOS ERROR), bad arguments 20.
- stream-json output needs --verbose in print mode (Claude Code's rule, same error text).
- Text piped in: prompt, newline, stdin text (as Claude Code joins them).
- The transcript notes of a print run are not on stdout; with --verbose they go to the error
  stream (pr_CES on the Amiga).
- --max-turns / --max-budget-usd apply in print mode only (Claude Code); checked after a tool
  round, before the next request; the tool results are kept (history stays valid).
- --fallback-model takes Claude Code's comma list; C:Claude keeps one fallback: the first.

Running count: 8 of 8 built and host-tested; rig check open (main session).

## Not done / partial (say so in the report)

- --verbose outside print mode is accepted and does nothing more (Ctrl+O already shows the
  whole transcript); Claude Code's interactive --verbose unfolds tool output in place.
- stream-json: a subagent's own messages (parent_tool_use_id set) are not forwarded; the
  partial events are the conversation's requests only (as Claude Code's).
- --input-format stream-json: image blocks are dropped (their text blocks joined); control
  messages are ignored. --replay-user-messages, --json-schema, --session-id, --bare,
  --permission-prompts, --mcp-config: not implemented (not in this package).
- --resume with a .jsonl path: not taken (ids and names are).
- Unverified against Claude Code itself (the docs do not say): the piped text joined after the
  prompt with one newline; the text-mode line "Error: Reached max turns (N)"; is_error true on
  error_max_turns; one assistant message per API response (newer Claude Code splits per block).
- --agent finds custom agents through the wiring's provider (feature/a4-wiring merged in,
  125265e); only the built-in Explore is host-tested.

## repl.c touch points (for the parallel agents' merge)

- repl.h: cl_feed, ASKP_*, TURN_*, new cl_repl fields (feed ask_policy no_person max_turns
  budget_* turn_rc n_responses api_ms quiet_req cur_id sys_replace sys_append layer[2] first),
  repl_need_key.
- on_event: feed->event (unless quiet_req). post(): no request without a key over https.
  request(): renamed request_retry + a wrapper adding api_ms. api_send: quiet_req.
- tool_ask -> repl_ask (exported in repl_int.h, with repl_denied); policy.c's ask-rule path
  calls repl_ask too and keeps RULE_ASK in rule_now. tool_choose: -1 when no_person.
- run_tools: r->cur_id. turn(): turn_rc, n_responses, feed->message after each conv_add,
  the --max-turns / --max-budget-usd stop after a tool round.
- repl_system: sys_replace / sys_append. repl_load: layer[] merged at CFG_SESSION; dontAsk,
  bypassPermissions, manual modes. repl_init: no refusal without a key. repl_free: frees the new
  strings. repl_run: start() (login first when keyless, else r->first); repl_line's await_key
  branch sends r->first after the key.
- Also: conv.c/h per-model usage (modelUsage) and "[]" tools left out; subagent.c tools_mask
  and tools_agent public (tools.h); session.c writes a title set before the first save into the
  file (a -n name was lost on resume -- regression test test_wp4_session).

## Log

- 2026-10-05 rows 4.1-4.5, K, T, D built; suites claude_cli (parser) and claude_repl test_wp4
  (print mode text/json/stream-json/partial, stream-json input, limits, permissions, sessions,
  apply, start/keyless). Mutation-checked: max-turns stop, settings layer, bypass, feed
  messages, start(), session title fix each fail the suite when removed.
- c76a6fe WP4; bdc5344 main (d455068) merged; c63a6fb bare --disallowedTools names as deny
  rules; 125265e feature/a4-wiring merged (coordinator: wiring goes to main): repl_ask through
  r->at, cur_id in pol_call, --tools without WebSearch kept off across reloads by a deny rule.
  Gates after the merge: make test, make test-ref, make amiga green.


## Rig steps (main session; fixture: python3 tools/claude_fixture.py, U = URL=http://<mac>:8080/v1/messages)

1. `Claude ?` prints the template; answer `PRINT U ROOT=SYS: show me the startup`: the answer
   text alone, then the Shell prompt (rc 0: `Echo $RC`).
2. `Claude -p --output-format json U ROOT=SYS: show me the startup`: one JSON result line;
   `--output-format stream-json --verbose`: init, assistant, user, assistant, result lines.
3. Piped: `Claude -p U ROOT=SYS: show me the startup <S:Startup-Sequence` (and through vsh:
   `cat S:Startup-Sequence | Claude -p ...`): the request carries the file after the prompt
   (T:Claude.log with DEBUG).
4. `Claude -p --max-turns 1 U ROOT=SYS: show me the startup`: "Error: Reached max turns (1)",
   `Echo $RC` 10.
5. `Claude -c U ROOT=SYS:`: the screen opens with the last conversation replayed;
   `Claude -n test U "show me the startup"`: the screen starts with that prompt; later
   `Claude -r test U`.
6. Keyless: `UnsetEnv ANTHROPIC_API_KEY`, rename ENVARC:Claude/key away, `Claude`: the screen
   opens at "Paste the API key"; `Claude -p hi` prints "Not logged in: ..." with rc 10.
