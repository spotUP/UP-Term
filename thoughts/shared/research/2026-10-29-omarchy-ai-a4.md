# OMARCHY-AI A4: Ask AI (Agent command and /ask)

Plan row: thoughts/shared/plans/2026-10-28-omarchy-ai.md line 92. Redirected by the coordinator: no text box,
the Omarchy way is a shell command plus a slash command.

## Positional prompt support

- C:Claude: claude/main_amiga.c:3-11 (usage `Claude [options] [prompt]`; a PROMPT starts the interactive
  conversation with it; PRINT is the separate print mode). main_amiga.c:403 `r->first = cli.setup ? "/setup" : cli.prompt`.
  Command line via cli_parse_line(GetArgStr()) at main_amiga.c:234-236; the tokenizer at claude/cli.c:18-48 reads
  `"..."` with the escapes `*"`, `**`, `*N`, `*E`.
- amiga-pi (sibling repo amiga-pi/amiga-pi): args.c:187-198 appends any word not starting with `-` to a->prompt;
  app.c:1468 `pi_ui_run(&s, pl->io, a.prompt[0] ? a.prompt : 0)` starts the interactive UI on it.

## Route

`Agent "<task>"` (install/agent.c, C:Agent) sends `/ask <task>` through the same window route as C:UPTerm
(handler/upcmd.h `upterm_window_command`, DoPkt ACTION_VTCON_COMMAND). The handler runs slash `ask`
(SLASH_ASK) -> `tab_command_arg(c, MENU_AGENT, arg)` -> `host_command` -> MENU_AGENT -> `tab_spawn(..., task)`.
The tab script quotes the task onto the agent command (`tabprog_task_cmd`, using `vsh_put_word` in
shell/vsh_quote.h, the one escape routine vsh_command_line also uses). No new IPC.

## Tests

- tests/test_tabprog.c `agent_task_is_one_quoted_argument`: both agents, quote/star/newline escapes, empty task, too small buffer.
- tests/test_slash.c: `/ask` parse (trimmed argument, empty argument); table stays sorted.
- tests/test_agent_wiring.py: 7 source-wiring checks (handler is Amiga-only, no host harness runs it).
- tests/test_tutorial_wiring.py: regexes updated for the new tab_spawn argument and the shared loop in upcmd.h.

## Open

- Live check on a rig not done: `Agent "list the files"` should open a tab whose agent starts on that task.
- amiga-pi reads a task starting with `-` as an option (args.c:187, no `--`). Claude's cli.c not checked.
- How amiga-pi tokenizes the quoted command line (ixemul argv) is unverified.
- Pre-existing failures on main: tests/test_dist_addons.py 3 tests, tests/test_amarchy_docs.py 2 tests.
