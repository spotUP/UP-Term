---
date: 2026-10-28
author: spot (Amarchy s20)
topic: OMARCHY-AI B1 skills search path, D1 local model status, F1 agent colour config
---

# B1 Skills search path

## C:Claude (claude/commands.c defs_load, :771-)
Skill directories, project first (hides user's of the same name):
1. Project: `<root>/.claude/skills/<name>/SKILL.md` (commands.c:~800 path_join(root,".claude"), loop `src` 1 then 0). Plus nested `.claude/skills` between root and a read/edited file (policy.c:363-384 skills_for).
2. User: `<data>/skills/<name>/SKILL.md` (commands.c:~806: for user skills `data` replaces `home`). `data` = repl.c:3066 data_dir(): `UP-Term:var/Claude` with the kit (datadir.h dd_dir), else `ENVARC:Claude`; `CLAUDE_CONFIG_DIR` overrides both (data = home).
   `home` = `ENVARC:Claude` (repl.h:35 CL_HOME; repl.c:3142). Other def types (commands, agents, output-styles) still use home, only skills use data.
3. Built-in "bundled" skills (commands.c bundled(): simplify, loop, ...).
Only `<dir>/<name>/SKILL.md` (commands.c:551); loose .md files are NOT skills for C:Claude.
Frontmatter keys read (commands.c defs_parse ~:395-440): name, description, arguments, allowed-tools, tools, model, argument-hint, disallowed-tools, skills, paths (+ `when`, `initial`, `hooks` fields exist).

## amiga-pi (../amiga-pi/amiga-pi/pkgres.c, skills.h:1-20)
Order (first of two same-named wins): settings.json "skills" paths (project .pi/settings.json, then `<agent dir>/settings.json`), `<cwd>/.pi/skills` (trusted), `.agents/skills` in cwd and ancestors up to .git, `<agent dir>/skills` (pkgres.c:1327), `<agent dir>/agents-skills` (:1335), `<data dir>/skills` (:1344-1351), then `--skill` paths.
Amiga: agent dir = `ENVARC:amiga-pi` (main_amiga.c:159-161, PI_CODING_AGENT_DIR overrides); data dir = `UP-Term:var/amiga-pi` with kit else `ENVARC:amiga-pi` (main_amiga.c:164, dd_dir).
Format (skills.c:155-245): SKILL.md frontmatter `name`, `description` (required, <=1024 chars), `disable-model-invocation`; name lowercase a-z0-9-, <=64 (warns only). Also loose .md in `.pi/skills` root style.

## Common path
Both read `<dir>/<skill>/SKILL.md` with `name` + `description` frontmatter; SAME FORMAT for those two keys. No single directory is read by both: C:Claude `UP-Term:var/Claude/skills` (or ENVARC:Claude/skills), amiga-pi `UP-Term:var/amiga-pi/skills` (or ENVARC:amiga-pi/skills). Project-level, `<project>/.claude/skills` (Claude) vs `<project>/.pi/skills` or `.agents/skills` (pi): `.agents/skills` is not read by C:Claude. So the kit needs TWO copies (or one install step writing both): `UP-Term:var/Claude/skills/amarchy/SKILL.md` and `UP-Term:var/amiga-pi/skills/amarchy/SKILL.md`. Note settings.json "skills" in amiga-pi could point at one shared dir, but C:Claude has no equivalent: two copies is simplest. Keep frontmatter to name+description (+ allowed-tools harmless to pi? pi only warns on unknown).
amiga-pi's existing arexx skill: `../amiga-pi/skills/arexx/SKILL.md` (siblings amigados, amiga-c-build, amiga-autodocs).

## Sources an Amarchy skill's command list can be generated from
- Keymap action prefixes: config/keymap.c:213-218 `prefixes[]` (menu, menuid, tab, slash, run, job, rexx, open, detach, scan, win, gtheme, settings); tab verbs :220-; key names `knames[]` config/keymap.c:32; parser `keymap_parse` :837, `keymap_overlay` :852; loader config/keymap_load.c.
- `win:` verbs: closed list in amarchy/win_core.c:32 `bare[]`, :37 `withArg[]`, direction lists :41-43; validator config/keymap.c:245.
- act.c (amarchy/act.c, act.h:8-22) is a dispatcher (act_run, act_rexx, act_in_window, act_menu_open), not an action table; keymap dispatch is handler/keymap_act.c:6 `ids[]` (name -> id).
- ARexx port: `AMARCHY` (amarchy/amarchy.c:309 rexx_host_open); command table in amarchy/rexx_host.c (see its dispatch).
- Prefs keys: prefs/settings_def.c (`sd_amarchy_key()`; amarchy/amconf_keys.c:13 consumes it).

# D1 Local model status
- B5 status: LANDED in part. claude/http.c no longer names a provider (grep anthropic|x-api-key: no match; amiga-pi ARCHITECTURE.md:35 expected it at http.c:112-142). C:Claude posts via `llm_post` (claude/repl.c:418) and decodes via `llm_anth_init` (claude/stream.c:13), both in vtcon/llm/. D8 (ARCHITECTURE.md:16) and :79.
- BUT C:Claude uses only the Anthropic backend: no use of llm registry, models.json, openai backends, or a base-URL variable anywhere in claude/*.c (grep llm_reg|registry.h|openai|ANTHROPIC_BASE_URL: no hit). So C:Claude cannot talk to a local OpenAI-compatible model today (unverified on a running rig; static grep only).
- amiga-pi can. models.json path: `<agent dir>/models.json` = `ENVARC:amiga-pi/models.json` (models.c:87; main_amiga.c:159-161). Parsed by llm/models_json.c (pi model-config schema, comments and BOM allowed): `{"providers":{ID:{baseUrl, api, apiKey, headers, authHeader, compat, models:[{id,name}], modelOverrides}}}`.
- Example (tests/pi/model/resolve.models:50-62):
  `{"providers":{"ollama":{"baseUrl":"http://localhost:11434/v1","api":"openai-completions","models":[{"id":"qwen2.5-coder:7b","name":"Qwen2.5 Coder 7B"}]}}}`
  (apiKey needed? check llm/models_json.c / agent.c checkAuth :1679 before the Setup row; a dummy value is likely required.)

# F1 prep colour config
- C:Claude tui.c: no colour file of its own. It reads (a) settings.json key `"theme"` (claude/config.c:515) = one of the fixed built-in names in claude/theme.c (cl_themes table, theme_get :34; claude/theme.h); `/theme` writes it via input.c:248 set_setting. (b) pi-style theme objects `cl_pitheme` (theme.h:90 pitheme_load; tui.c:929 pitheme_sgr(role)), roles PR_BORDER, PR_ACCENT, PR_SCROLLBAR_*...; used by tui.c. Per-prompt colour via `/color` (theme_named, theme.c:44: red blue green yellow purple orange pink cyan). `settings.json` lives in ENVARC:Claude (CL_HOME).
- amiga-pi: yes. Setting `theme` in `<agent dir>/settings.json`; theme files `<data dir>/themes/<name>.json` or `<agent dir>/themes/<name>.json` (app.c:375-392), else built-in dark/light, else "system" (terminal palette); pi theme JSON format via pitheme_load (claude/theme.c:259, shared). Built-ins amiga-pi/themes_builtin.h; okcolor.c.
- Not verified: whether C:Claude loads pitheme files from a themes dir (tui.c:150 only theme_get(0) seen; grep pitheme_load in claude/repl.c: no hit).
