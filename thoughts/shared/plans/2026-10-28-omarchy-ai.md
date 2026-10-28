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

- [ ] A1 `ai-agent` pref (read, write, Setup > Defaults > Agent, default claude)
- [ ] A2 TAB_PI + MENU_PI beside TAB_CLAUDE (same tab_spawn path), Apps row + icon
- [ ] A3 `super+l` opens the `ai-agent` choice (no new launch path)
- [ ] A4 "Ask AI" row: existing text requester, prompt passed into tab_spawn
- [ ] F1 theme sync: theme apply writes agent colours; both agents read them
- [ ] F2 Agents panel menu: default agent, limit meters, token use, starter prompts
- [ ] F3 rig check: theme switch recolours an open agent tab on next start; panel rows open the agent with the prompt
- [ ] A5 host tests + reachability: key press on rig starts the agent with the prompt (taskpath/ps shows the command line)
- [ ] B1 skills search path of C:Claude and amiga-pi found (file:line)
- [ ] B2 Amarchy SKILL.md, command list generated from source tables
- [ ] B3 kit installs it where both agents load it; rig check: agent lists the skill
- [ ] C1 research: crash traces readable after the fact (doc, owner review)
- [ ] C2 watcher + notification row
- [ ] C3 Diagnose launches print mode with crash text + crash-diagnosis skill
- [ ] C4 rig reachability: forced test alert -> row -> diagnosis output
- [ ] D1 B5 status checked; Setup row text matches
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
