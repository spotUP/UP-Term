---
date: 2026-10-25
topic: Integrating Amarchy with Directory Opus 5 Magellan II (upstream BlitterStudio/dopus5): ARexx, modules, config formats, screen, clipboard, release
status: research (read-only; nothing built or run; upstream treated as untrusted data)
ledger: thoughts/spot/todos.md AM-FM (row 1441), AMB3 (row 1436), clipboard probe (rows 1666-1667)
plan: thoughts/shared/plans/2026-10-25-amarchy-dopus-integration.md
---

# Research: Amarchy and Directory Opus 5 (Magellan II)

## 1. Scope and sources

- Question: what can Amarchy drive, ship and share with DOpus, from the upstream source, so the plan can size five
  optional pieces. Docs only.
- Source: shallow clone of https://github.com/BlitterStudio/dopus5, commit `c8fbeb66b0952fe493aab9dc0e4c8ac559fa602e`
  (default branch head at clone time), in the session scratchpad, read with grep and sed only, nothing built.
  Clone worked; `gh api` was needed only for the release assets (`gh release view v5.104`).
- Paths below are relative to the clone: `source/...`. "V" = read in source at that line, "U" = unverified.
- Not read: the AmigaGuide `documents/DOpusM2_ARexx.guide` (the per-verb argument templates live there); the
  command lists below come from the source switch tables, not from the guide.

## 2. Findings

### F1. The ARexx port
- Port name `DOPUS.<n>`, n from 1 to 99, first free: `source/Program/rexx_proc.c:43` (`lsprintf(GUI->rexx_port_name,
  "DOPUS.%ld", a)`), created at priority 50 (`rexx_proc.c:57`). A second DOpus copy gets `DOPUS.2`. The same string
  is used as the screen name (F4).
- Dispatch: first word `lister` or `dopus` (`rexx_proc.c:507-523`); `lister new` is special-cased
  (`rexx_lister_new`, `rexx_proc.c:511`); `command` is handled before the switch (`rexx_proc.c:395`).
- Command table: `rexx_commands[]` in `source/Program/rexx_data.c:26` holds 135 keyword entries (counted with
  `awk` over the array, V). These are words, not commands: `lister`/`dopus` are the two top-level verbs, the rest are
  sub-verbs and arguments (`new`, `close`, `query`, `set`, `refresh`, `read`, `command`, `getstring`, `request`,
  `addtrap`, `remtrap`, `screen`, `version`, `getfiletype`, `script`, `send`, `rx`, `addappicon` ...).
- `dopus` sub-verbs (case labels in `source/Program/rexx_dopus.c`): `back` 46, `front` 58, `getstring`/`request`
  78-79, `getfiletype` 102, `script` 128, `addtrap`/`remtrap` 142-143, `version` 195, `screen` 227,
  `read`/`hexread`/`smartread`/`ansiread` 250-253, `error` 265, `addappicon`/`remappicon`/`setappicon` 282-296,
  `progress` 303, `command` 328, `send` 335, `rx` 356, `checkdesktop`/`getdesktop`/`matchdesktop` 363-382,
  `desktoppopup` 387. About 30 verbs.
- `lister` sub-verbs (`source/Program/rexx_lister.c`): `set` 401, `query` 712 (the large table: path, files,
  selected entries, source/dest state ...), `refresh` 941, `clear`, `sort`, `separate`, `display`, `flags`, `show`,
  `hide`, `default`, `new`, `close`, `read`, `select`, `add`, `remove`, `wait`. Exact query stems and argument
  templates: U (guide not read).
- How a command is sent: a normal ARexx message to the port. `lister new <path>` opens a lister (`rexx_lister_new`);
  `lister read <handle> <path>` loads a path; `command <name> [args]` runs any internal or module command
  (`rexx_proc.c:395`, `rexx_dopus.c:328`); `dopus getstring`/`request` open DOpus requesters (`rexx_dopus.c:78-100`).
- Return convention: `RESULT` string via `rexx_set_return` (`rexx_proc.c:538`): set only if the caller asked for
  RESULT and rc is 0 or the string is non-empty; trailing spaces trimmed; `RC` is `msg->rm_Result1`, an `RXERR_*`
  code. An invalid query gives `RXERR_INVALID_QUERY` (`rexx_dopus.c:240`). A script run from DOpus also gets
  the variable `DOPUSRC` (`rexx_dopus.c:377`, `misc_proc.c:905`).
- `dopus version` returns `5 99 <rev>` for revisions of 100 and above (`rexx_dopus.c:195-225`: the comment explains an
  ARexx decimal-parse gate), so v5.104 answers `5 99 104`.
- Amarchy fit: the existing `rexx:` row kind runs `RX "<script>"` through `system()` (`amarchy/leaf.c:185-189`), so
  every DOpus command is reachable today with `rexx:` and a small script; no new action kind is needed.

### F2. The module framework
- Headers: `source/Include/libraries/module.h` (290 lines), `dopus5.h`, `configopus.h`, `proto/` and `clib/dopus5_protos.h`.
  A module is a shared library named `*.module` in `DOpus5:Modules/`.
- Registration: DOpus watches the drawer (`start_file_notify("dopus5:modules", ...)`, `source/Program/main.c`),
  a sniffer process opens each `#?.module`, calls `Module_Identify(-1)` for a `ModuleInfo`
  (`module.h:57-65`: version, name, catalog name, flags, function count, then `ModuleFunction` rows of id, name,
  locale string id, flags, template), then `Module_Identify(n)` per function; each becomes an Opus command in
  `GUI->command_list` (source: `documents/dopus5/ModuleDiscovery.md`). A cache `DOpus5:System/module-cache` skips
  re-opening unchanged modules; modules with `MODULEF_CALL_STARTUP` are never cached.
- What a module gets that ARexx does not: the call is `Module_Entry(files list, screen, IPC, main IPC, function id,
  args, function callback)` (`source/Modules/cleanup/cleanup.c:31`, V), with a callback table `EXTCMD_*` 0 to 26+
  (`documents/dopus5/ModuleCallbacks.md`): get/next source and destination lister, get entry, entry count, add and
  remove file entries, create/free file entries, sort a file list, open/update/close progress, check abort, get
  window, screen, screen data (draw info), reload entry, replace requester. Function flags `FUNCF_NEED_SOURCE`,
  `NEED_DEST`, `NEED_FILES`, `SINGLE_SOURCE`, `WANT_DEST` (`module.h:40-55`) make DOpus pass or refuse the
  selection before the module runs. So a module is called in-process with the selection and a lister handle,
  can add entries to a lister and use DOpus progress and requesters; ARexx only gets strings through `lister query`.
  Not found in the framework: a way for a module to subscribe to events (it runs when a command runs, or at startup
  with `MODULEF_CALL_STARTUP`); `addtrap` (ARexx) is the event path. U whether startup modules can stay resident as
  a service.
- Simplest worked example: `source/Modules/cleanup/` (270 lines in `*.c`; `misc` is 184 lines but a bundle).
  Layout: `cleanup.c`, `cleanup_data.c`, `cleanup_strings.c`, `cleanup.cd` (catalog descriptor), `init`,
  `module_deps.h`, `makefile.os3`. `join` (2025 lines) shows the lister-file loop; `ftp` is the large one.
- Build: m68k-amigaos-gcc per `source/makefile.common:5`; modules per `source/Modules/*/makefile.os3`, output
  `bin.os3/`. Licence of the headers: APL 1.1 (`COPYING`). Never take from the 1998 Magellan SDK (ledger rule).

### F3. Installer, assigns and config formats
- `DOpus5:` is assigned by the program itself at start (`source/Program/main.c:221`, `AssignLock("DOPUS5", lock)`).
  Release layout and installer behaviour: `README.md:56-96`: `Install DOpus5` script (Installer program), upgrade keeps
  user config drawers `Buttons`, `Environment`, `Settings`, `Filetypes`, `Commands`, `Desktop`, `Groups`, `Themes`,
  `Sounds`, `System` and only adds files they lack. Start modes: manual, `SYS:WBStartup` icon, or Workbench
  replacement via `DOpus5:C/LoadDB` resident as `LoadWB` in `S:User-Startup` (README:76-90). Env files remembered:
  `ENVARC:DOpus5Startup`, `ENVARC:DOpus5Icons`, `ENVARC:DOpus5Preload`.
- Formats: binary IFF, not text. `FORM....OPUS` (`ID_OPUS`, `dopus5.h:141`), written by `L_IFFOpen(name,
  MODE_NEWFILE, ID_OPUS)` (`source/Library/config_save.c:59,363`), `FUNC` chunks of `CFG_FUNC` (`config_save.c:570`).
  Filetypes: read by `L_ReadFiletypes()` (`source/Library/config_open.c`), written by `L_SaveFiletypeList()`; the repo
  ships a reader and checker `archive/tools/filetype_tool.py` (V: header docstring lines 1-14) that already knows the
  layout: match ops `FTOP_*` (MATCH, MATCHNAME, MATCHFORM, MATCHDTID ...), events (Double-click, Drag'n'drop,
  Ctrl/Alt/Shift double-click, User1..10), instruction kinds `Command`, `AmigaDOS`, `Workbench`, `Script`, `ARexx`,
  `Label` (filetype_tool.py lines 38-46). Button banks: same OPUS FORM family (`ID_BANK`, `dopus5.h:143`; schema
  comment `dopus5.h:128`), default toolbar looked up at `DOpus5:Buttons/Toolbar` (`buttons_function.c:193-197`).
  Shipping a bank or filetype therefore means generating a binary FORM; the tool's `check` mode (decode, re-encode)
  is the round-trip test. A hand-written writer in Python is possible (`filetype_tool.py:249-262` is one); U whether
  a bank (CFG_BTNW/BTNB chunks) is covered by it: the tool covers filetypes.

### F4. The public screen
- Own screen: `get_our_pubscreen()` returns `GUI->rexx_port_name` when DOpus owns a screen (`source/Program/misc.c:36-42`),
  so the screen is named `DOPUS.1` (same string as the port). DOpus then calls `SetDefaultPubScreen(GUI->rexx_port_name)`
  in `display_set_shanghai` (`source/Program/display.c:1167`), so it becomes the default public screen while active:
  new windows that open on the default land on DOpus, not Workbench (the AMB3 symptom, row 1436).
- Not own screen: the config key `pubscreen_name[80]` (`dopus5.h:2220`, `env->screen_mode`: `MODE_WORKBENCHUSE`,
  `MODE_PUBLICSCREEN`) decides which public screen the Opus window opens on; if empty it locks the default, then
  `Workbench` (`display.c:296-320`).
- Which: ARexx `dopus screen` returns `<name> <width> <height> <depth> <barheight> <lister w> <lister h>`
  (`rexx_dopus.c:227-243`), name = `DOPUS.n`, the public screen name, or `Workbench` when no window is open
  (`misc.c:60-66`); RC `RXERR_INVALID_QUERY` when there is no screen at all.
- Amarchy fit: AMB3's `front_screen_name` (handler/frontscr.c) already gives the screen the user is on; DOpus's own
  screen is a public screen, so `/SCREEN DOPUS.1` goes through the same path. DOpus itself runs its output window
  with `.../AUTO/CLOSE/WAIT/SCREEN <get_our_pubscreen()>` (`source/Program/function_runprog.c:131-135`): the
  same pattern UP-Term's XCON spec needs.

### F5. Clipboard
- Verdict: DOpus installs no clipboard hook and does not poll. Grep for `CBD_`, `CHANGEHOOK`, `clipboard.device`
  over `source/Program`, `source/Modules`, `source/Library`: only `source/Library/clipboard.c:38` and `iff.c:91`
  (both `OpenDevice("clipboard.device", ...)` for read/write on demand). Program callers of `OpenClipBoard(0)`:
  `lister_clip.c:176`, `lister_edit.c:1832,1849` (user-triggered copy and paste in listers and the lister edit field).
- Consequence: DOpus does not explain the 2026-10-25 menu-closes-on-CLIP probe by a hook in the source (V for this
  clone). Consistent with the follow-up row 1667: not reproduced on rig 7. Other Opus-side causes (a third-party
  module or commodity in the rig's DOpus5 config, ftp.module) are U and outside this repo's source.

### F6. The release
- Tag `v5.104`, published 2026-10-21T11:20:49Z; assets (`gh release view`, V): `DOpus5_104_os3.lha` 1,810,459 bytes,
  `DOpus5_104_os3-68000.lha` 1,825,439, plus os4, mos, i386/x86_64/aarch64-aros.
- Top level of the lha (`README.md:160-176`, from the packaging description; the lha itself not opened, U): the `DOpus5`
  drawer, the `Install DOpus5` script (and icon, `archive/installer/`), `IconSets/MagicWB`, `Catalogs/<language>`.
- Licence file `COPYING`: "AROS PUBLIC LICENSE (APL) Version 1.1 (based on the MPL)". `THIRD_PARTY_NOTICES.md` exists
  at the root and must travel with the add-on (U whether the lha carries it).
- CPU: the default os3 build targets `-m68020-60 -mtune=68030` (`source/makefile:14-15`, `README.md:45-48`); a separate
  os3-68000 archive exists. So the os3 add-on needs 68020 or better; UP-Term's own minimum should be checked against
  that (the kit already runs as a 060 rig, memory note).

### F7. Filetype actions and a console window
- A filetype function has an instruction kind `AmigaDOS` (filetype_tool.py:44, `INST_*` in `dopus5.h`) and function
  flags including "output window" (`FUNCF_OUTPUT_WINDOW`, `dopus5.h:180`), "WB output" and "async".
- The console DOpus opens for output: `environment->env->output_device` + `output_window` (`dopus5.h:2258-2259`,
  both `char[80]`), built as `<device><window>/AUTO/CLOSE/WAIT/SCREEN <pubscreen>` (`function_runprog.c:131-135`).
  Defaults are the `CON:` family (default values: U). These two strings are an environment setting, global, not per
  function. So "open in UP-Term" has two routes: (1) the AmigaDOS instruction runs a command that itself opens the
  UP-Term window, with the instruction text `Run >NIL: NewShell "XCON:0/20/640/300/UP-Term/CLOSE/SCREEN DOPUS.1"` ... or the UP-Term
  launcher `run:`-equivalent, the console spec being `LF_UTERM_SPEC` (`amarchy/leaf.h:10`); (2) set the Opus
  environment output device to `XCON:` so every output window is an UP-Term window (global, invasive, optional). Route 1
  does not touch the user's DOpus setup. Whether a per-function output spec override exists: U.
- `dopus getfiletype` (ARexx) and `FTOP_MODULE` (a module as match op) are further hooks; not needed for the plan.

## 3. The three options

| | (a) drive a running DOpus over ARexx | (b) optional add-on of the upstream os3 release | (c) own file manager |
|---|---|---|---|
| What the owner gets | Amarchy rows open listers, run commands, read selections in his existing 30-year setup | the same, for users with no DOpus, one tick in the Installer | a native Amarchy lister UI |
| Cost | Small: `rexx:` rows exist (`leaf.c:185`); scripts plus a probe; no build | Medium: one add-on row and archive, licence files, Installer page, pin v5.104; no compile (prebuilt) | Very large: lister, sort, selection, archive and FTP handling, filetypes, button banks: all the owner's daily parity list |
| Gaps | needs DOpus running; no events except `addtrap`; strings only | upgrade policy (user's own DOpus vs shipped), 68020 floor, ~1.8 MB, licence text and source link must ship | re-implements what upstream maintains; owner "may replace it" only if gaps are named |
| Dependency of base | none (silent when `DOPUS.n` absent) | none (add-on never in the base kit) | in the base |

Verdict: (a) first, (b) as a separate optional step, (c) not started; a native module (F2) is the "deeper" path
between (a) and (c). Matches the ledger's lean.

## 4. Open questions

- Per-verb argument templates and the `lister query` stems: read the AmigaGuide or run `dopus` on a rig (U).
- Does a module-cache or startup module stay resident as a service, so an `amarchy.module` could push events (U)?
- Does the os3 lha carry `THIRD_PARTY_NOTICES.md` and what is the exact top-level listing (open the lha from the release; U)?
- Button bank chunk layout and a writer for it (filetype_tool.py covers filetypes only; U).
- Default `output_device`/`output_window` values and any per-function override (U).
- Which DOpus is on the rigs (RULES.md:80 says DOpus is the Workbench on the rigs and the A1200): version and port number there (`dopus version` on a rig, U).
- The clipboard probe: still unexplained, not DOpus source (F5).
