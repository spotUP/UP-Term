---
date: 2026-10-28
author: spot (Amarchy s20)
status: scope approved, implementation pending
ledger: thoughts/spot/todos.md row OMARCHY-AI
research: thoughts/spot/research/2026-10-28-omarchy-ai-integrations.md (section "Existing code to reuse")
---

# Amarchy AI integration (OMARCHY-AI)

## Owner decisions (s20)

- First cut takes all five gaps: one-key launch, Amarchy skill + ARexx pack, crash
  diagnosis, local LLM setup, usage row.
- Front end: C:Claude is the default; an Amarchy preference switches to amiga-pi
  (local models, MCP, Lua extensions). Both are in the kit already and share
  vtcon's engine (llm/, http, tls, hooks, tools).
- Reuse first: no new agent loop, no new HTTP, no new model config format.
  Everything below wires Amarchy to what C:Claude and amiga-pi already do.

## Design

### A. Launch (gap 2)
- ALREADY THERE (s20 owner question): `super+l` and Apps > Claude send
  `menuid:MENU_CLAUDE` (dist/keymap:124,149); vtcon handles it at
  handler/vtcon_handler.c:6223 with `tab_spawn(..., TAB_CLAUDE)`: Claude in a
  new tab of the current window. `/claude` slash does the same (handler/slash.c:74).
  amiga-pi has no menu row or key.
- So A is only: a `TAB_PI` beside `TAB_CLAUDE` (same tab_spawn path, its own
  menu id, Apps row, menu-icons entry), and the `ai-agent` pref picks which one
  `super+l` opens. No new launch path, no proclaunch window.
- "Ask AI": one line from Amarchy's existing text requester, then the same tab
  spawn with the prompt passed through (tab_spawn needs a prompt argument;
  check how TAB_CLAUDE builds its command line first).

### B. Amarchy skill (gap 1)
- `dist/amarchy-skills/amarchy/SKILL.md`, built from `amiga-pi/skills/arexx/SKILL.md`
  plus the real Amarchy command list (act.c actions, `win:` actions, ARexx port
  name, keymap file format, prefs keys, theme files). Generated parts come from the
  source tables where possible, so the skill cannot drift from the code.
- First find the skills search path of C:Claude (commands.c / datadir_amiga.c) and
  of amiga-pi; the kit installs the skill where both look (one copy, or one
  install step that writes both paths if they differ).

### C. Crash diagnosis (gap 3)
- Research step first (sonnet, no code): what an Amiga leaves after a Guru or a
  MuForce/Enforcer hit that a program can read after the fact (ExecBase LastAlert
  across the reset, MuForce output file/serial, the project's ownermap / wasabi
  guru tooling). Owner checks the findings before any watcher is written
  (owner knows the hardware).
- Then: Amarchy checks at start (and on the hit file changing) and shows a
  notification row "Last crash: <alert> - Diagnose"; Diagnose starts the agent in
  print mode with the crash text and a `crash-diagnosis` skill (from the
  project's amiga-debugging know-how).

### D. Local LLM (gap 4)
- Check whether C:Claude uses the shared llm/ backends yet ("B5",
  amiga-pi ARCHITECTURE.md:79). If not, local models are amiga-pi only, and the
  Setup row says so.
- Setup row "Local Model": asks base URL and model name, writes or updates one
  provider entry in amiga-pi's models.json (existing pi format,
  llm/models_json.c), and sets `ai-agent` to `pi`.

### E. Usage row (gap 5)
- A `Stop` hook (hooks.c protocol) installed in the agent's settings writes
  tokens + cost of the session to `ENV:Amarchy/ai-usage` (one line).
- Amarchy's status/OSD row shows it when the file exists. Same for amiga-pi if
  its hooks support Stop; else from its session3 JSONL at session end.

### F. UI goal = what Omarchy does (owner, s20: "how does Omarchy do it? that should be the goal")
Omarchy keeps every agent in its terminal TUI; it adds no chat window. Its
"rich UI" is around the agents, and Amarchy copies exactly that:
- Theme sync (Omarchy item 7): the active Amarchy theme writes the agents'
  colours, so C:Claude and amiga-pi follow theme switches. Hook into the existing
  theme apply path (gthemes_*); the agents read one colours file. Check first
  what colour config tui.c already reads.
- Agents panel (Omarchy item 3): an Amarchy menu panel (existing menu_* code,
  themed) under the AI row: default agent, plan + 5-hour + weekly limit meters,
  session token use (from E), and starter "Make something" prompts that open the
  agent with that prompt (A's Ask path).
- Agent picker in Setup > Defaults > Agent (= A1 `ai-agent` pref).
- Later rows, owner to confirm per row when reached: multiple accounts with
  switch (item 4); Install > AI rows for agents beyond Claude/pi (item 1/9,
  only what runs on the Amiga). Dictation (item 11) is not feasible on 68k: out.
- Not done: native chat window (Omarchy has none).

## Checklist

- [x] A1 `ai-agent` pref: hidden amconf key, /agent slash, Setup > Amarchy rows (369c39a2; no Setup > Defaults menu exists)
- [x] A2 TAB_PI + MENU_PI, /pi, Apps row + icon (369c39a2)
- [x] A3 `super+l` = MENU_AGENT, tabprog_agent_tab table (369c39a2)
- [x] A4 DONE (11eb55f5 code, 01b01b82 tests, f45d957c deps; host tests + revert proof, rig check in A5; task starting with - is ledger AGENT-TASK-DASH) "Ask AI", Omarchy way (owner s20, 2026-10-29): no text box; shell command `Agent "<task>"` opens the default agent in a new UP-Term tab started on that task (like `omarchy agent prompt`); prompt passed into tab_spawn
- [x] F1 theme sync: theme apply writes agent colours; both agents read them. OWNER (s20, revised): agents follow the UP-Term console theme. C:Claude ANSI-follow mode maps every role to palette indices 0-15 (built-ins dark-ansi/light-ansi exist, claude/theme.c); amiga-pi gets one index-only "console" theme file. No per-theme files. DONE (2026-10-29): C:Claude was already palette-only except /color orange and pink (now bright red and magenta); amiga-pi defaults to its palette-only `system` theme, so no console file is needed. Test palette_follow in tests/test_claude_tui.c, revert-proven. Live recolour of text already on screen is unverified (F3)
- [ ] F2 Agents panel menu: default agent, limit meters, token use, starter prompts. OWNER (2026-10-29): like Omarchy 4, the panel opens from an agents icon at the right end of the Amarchy bar and drops down under the right corner (left click = panel, right click = launch the default agent); no chat window. Later, once Amarchy has plugins: a chat sidebar like github.com/atif-1402/ai-panel (ledger AMARCHY-PLUGINS) UPDATE 2026-10-29: merged at 4086d352. The bar sits at the bottom of the screen, so the panel opens upward from the right corner. F2 depends on R2 (bar window). Open: the rig step right_launch does not yet prove a tab opened, and there is no rig revert proof.
- [x] F3 rig check (closed by P2 R2.5b, 0622dde7: helper pens retaken on theme switch and bar redrawn, proven on rig 6 via BAR PENS; the agent tab itself was not run): theme switch recolours an open agent tab on next start; panel rows open the agent with the prompt. Colour half not run in A5 (Claude stuck at the key prompt): next run needs a dummy ENVARC:Claude/key or URL= as in tools/rig/standalone_rig.py
- [x] A5 DONE (rig 7, 2026-10-29, thoughts/shared/research/2026-10-29-omarchy-ai-a5-f3-rig.md: Agent "<task>", /ask, /agent pi one argv word, super+l both agents) host tests + reachability: key press on rig starts the agent with the prompt (taskpath/ps shows the command line). Claude showing the task on screen is unverified: the rig has no API key, the setup wizard took the screen
- [x] B1 skills search path of C:Claude and amiga-pi found (file:line): no shared dir, kit installs two copies (research 2026-10-28-omarchy-ai-b1-d1.md, c40ed131)
- [ ] B2 Amarchy SKILL.md, command list generated from source tables
- [ ] B3 kit installs it where both agents load it; rig check: agent lists the skill
- [ ] C1 research: crash traces readable after the fact (doc, owner review)
- [ ] C2 watcher + notification row
- [ ] C3 Diagnose launches print mode with crash text + crash-diagnosis skill
- [ ] C4 rig reachability: forced test alert -> row -> diagnosis output
- [ ] D1 B5 status checked (c40ed131: partial, C:Claude Anthropic only, local models = amiga-pi only; check apiKey optional at agent.c:1679); Setup row text matches
- [ ] D2 "Local Model" Setup row writes models.json entry + sets ai-agent=pi
- [ ] D3 rig check: amiga-pi lists the new model
- [ ] E1 Stop hook writes ENV:Amarchy/ai-usage
- [ ] E2 Amarchy row shows it
- [ ] E3 rig check: one print-mode run updates the row
- [ ] Z1 each fix/feature has a revert-proven test
- [ ] Z2 ledger + handoff

## Order

A then B (small, unblock daily use), then E, D, C (C waits on the owner's review
of C1). One sonnet agent per letter, in a worktree; rig checks batched per two
letters on rig 7.
