---
date: 2026-10-13
topic: Which add-ons stand alone on a stock Workbench (kit-split K2-1)
tags: [dist, kit, packages, claude, amiga-pi, nvim, python, kingcon, ixemul, rig]
status: final
---

# Stand-alone check of the add-ons (K2-1)

Plan: `thoughts/shared/plans/2026-10-13-kit-split.md`, phase K2-1. Owner's definition
(answer 3): an add-on is stand-alone if, with no base, it starts, finishes a
conversation with a tool call, and its keys work in both consoles (a line mode
counts, a garbled screen does not).

## Method (what was run)

- Rig 1, clean installed AmigaOS 3.2 (`rig.py start --os 32 --max`, Kickstart 47.115,
  Workbench 47.2): no `UP-Term:` assign (checked before every run), no ixemul in
  `SYS:Libs`, no `C:vsh`, no `C:Stack`. The rig's own boot hooks put `VTC:`, `XCON:`,
  `PTY:`, `IXPIPE:` and a patched ixemul in front of `LIBS:`; every test window first
  runs a prelude that resets `LIBS:` (none / stock / kit) and `TERM`, `TERMINFO`,
  `TERMCAP` (unset / `TERM=vtcon` with `TERMINFO` at a copy of the kit's terminfo).
- Programs copied to `VTC:sa/` by `tools/rig/standalone_rig.py`: `Claude` (vtcon main
  5c77f8c, `make build/amiga/Claude`, 731216 bytes), `amiga-pi` (~/Code/amiga-pi main
  6134f86, `make amiga`, 1878352 bytes; an earlier pass on the main of 10:41 and vtcon
  cb3b352 gave the same verdicts and was run again on these), `nvim` and `python3`
  trees from the staged kit (`build/dist/UP-Term/Files`, staged 07:55, not rebuilt). Nothing went into
  `C:`, `L:`, `LIBS:` except KingCON (below), taken out again at the end.
- ROM console: `NewShell CON:...`. KingCON: the owner's `L:KingCON-handler`
  (64088 bytes, byte-identical to the `.020` of KingCON 1.3) and his
  `DEVS:KingCON-mountlist`, read out of `nyhd2.hdf` DH0: with `xdftool -r` (read only),
  copied to the clean image's `L:` and `DEVS:`, `Mount KCON: FROM DEVS:KingCON-mountlist`,
  `NewShell KCON:...`. (His DOSDriver was not found: DH0:Devs/DOSDrivers has none; his
  User-Startup mounts it from that mountlist, which is what was used.)
- Claude and amiga-pi talk to `tools/claude_fixture.py` over plain http on
  127.0.0.1 (no key); Claude with `ROOT=RAM:` (trust and `Read`/`Glob`/`Bash` allow in
  `RAM:.claude/settings.json`), amiga-pi with `ENVARC:amiga-pi/models.json`.
  Prompts: "show me the startup (tool)" (a Read and a Glob/ls call), "time limit"
  (Claude: Bash with a 1 s limit) / "shell" (amiga-pi: a bash call only vsh answers),
  "slow" (Claude: Bash `Wait 30`, Ctrl+C after 4 s), "say hello" (amiga-pi, Ctrl+C
  while answering). The first prompt is typed with a typo and corrected with
  Backspace.
- Screen text: not OCR. The glyphs of the console font are learned from a
  calibration window per console (all printable ASCII printed by `Type`, 94 patterns
  found, plus bold and underline variants), then every 8x8 cell of the test window is
  looked up. Escape sequences a console did not take would show as text.
  Screens are saved as `build/rig/standalone/<session>-<step>.txt` and
  `shots/*.png`. No screenshot was looked at by eye; no subagent was used.
- Stock ixemul: GG-Lite 1.0's `ixemul.library` 48.3 (170028 bytes, from
  `~/Downloads/gg-lite`), with its `ixnet.library`. The plan names the rig's own
  3.1 ixemul 48.2 (166972 bytes); that file is not on this machine, so 48.3 stands
  for "stock". Kit: the staged kit's ixemul 80.1 + ixnet.
- Verdict files: `build/rig/standalone/<program>-<console>-<check>[-<libs>-<term>].json`
  (113 files: 53 pass, 60 fail), re-run with
  `python3 -I tools/rig/standalone_rig.py --only <session> --force`.

## Result: Claude and amiga-pi (stand-alone)

| Add-on | Console | start | conversation + tool call | keys (typing + Backspace, Ctrl+C) | Up recalls last prompt | bash tool (AmigaShell) | screen | data in ENVARC: |
|---|---|---|---|---|---|---|---|---|
| C:Claude | ROM CON: | pass | pass | pass | **fail** | pass | pass (line mode) | pass |
| C:Claude | KingCON | pass | pass | pass | pass | pass | pass (line mode) | pass |
| C:amiga-pi | ROM CON: | pass | pass | pass | **fail** | pass | pass (line mode) | pass |
| C:amiga-pi | KingCON | pass | pass | pass | pass | pass | pass (line mode) | pass |

Evidence (all from the fixture's request bodies and the window text):

- Start: the banner is on screen, the process is in `Status`, no requester window
  in UITREE, no `UP-Term:` request, no `Stack` command used (stock 3.2 has none; the
  default Shell stack is enough).
- Conversation: two requests reach the fixture, the second carries the tool results
  (Claude: Glob `RAM Disk:S/Startup-Sequence`; amiga-pi: read plus an unknown `ls`
  tool); the final line "Your Startup-Sequence runs SetPatch first." is on screen.
  The prompt typed as `show mx<BS>e the startup` reaches the fixture as
  `show me the startup`.
- Ctrl+C: Claude, Bash `Wait 30`: the next request comes 1 s after Ctrl+C with
  "The user stopped the command (Ctrl+C). Return code 5. ***Break" and the program
  stays up. amiga-pi: "[INFO] Stopped." and the program stays up.
- Up: in the ROM CON the window is in cooked line mode (`io_amiga` finds no
  UP-Term line discipline: `claude/io_amiga.c:93-121`), so Up recalls nothing and
  Claude writes no `ENVARC:Claude/history` (amiga-pi does write its history). In
  KingCON Up recalls the line (KingCON's own editor) and the second request carries
  the prompt. This is a limit of the plain console, not of the add-on.
- bash tool, vsh absent: both programs tell the model they run in the AmigaShell (the
  system prompt of the first request contains "AmigaShell"). Claude's Bash tool ran
  (`Wait 30` interrupted by Ctrl+C, `List SYS: ALL` moved to the background at its
  1 s limit). amiga-pi's call to a vsh-only command (`echo $((1+2)); ...`) failed in
  the AmigaShell with "Unknown command", rc 10, and the model got that result: the
  fallback works and fails honestly.
- Data: after the run `ENVARC:Claude` holds `projects/<slug>/*.jsonl`, `ENVARC:amiga-pi`
  holds its session file and history; no requester over the whole run.
- Screen: plain text and attributes; no escape sequence shown as text. The cursor and
  the coloured "Tool" label of Claude's tool lines decode as unknown glyphs (`?`), which
  is the reader's limit with styled cells, not stray sequences.
- `Version LIBS:mathieeedoubbas.library` on the clean image: 47.2 (amiga-pi's
  maths library is there).
- Not tested here: C:ClaudeCode and C:uptelnet (the plan lists them with Claude);
  https and AmiSSL (the fixture is plain http); TCP/IP is FS-UAE's `bsdsocket`,
  which the clean `--os 32` image boots with (both programs connected).
- Cosmetic: both banners say "Claude in UP-Term." / "amiga-pi in UP-Term." even
  with no UP-Term present.

## Result: Neovim and Python

Cells are `start / edit-or-repl / screen` for nvim and `start / repl / Up-history / screen`
for python3; P pass, F fail. LIBS: none = no ixemul.library anywhere; stock = 48.3;
kit = patched 80.1 + ixnet.

| Add-on | LIBS: | TERM | ROM CON: | KingCON |
|---|---|---|---|---|
| nvim | none | unset / vtcon | F F F (requester "Need at least version 47 of ixemul.library.") | F F F (same) |
| nvim | stock 48.3 | unset / vtcon | F F F ("Failed to start the in-process UI!") | F F F (same) |
| nvim | kit 80.1 | unset | P P P | P P P |
| nvim | kit 80.1 | vtcon | P P P | P P P |
| python3 | none | unset / vtcon | F F F F (same requester) | F F F F (same) |
| python3 | stock 48.3 | unset | P P F P | P P P P |
| python3 | stock 48.3 | vtcon | F F F F (machine stopped answering) | F F F F (machine stopped answering) |
| python3 | kit 80.1 | unset | P P F P | P P P P |
| python3 | kit 80.1 | vtcon | F F F F (machine stopped answering) | F F F F (machine stopped answering) |

Evidence:

- Neovim with the kit's ixemul (both consoles, both TERM): `nvim -u NONE RAM:t.txt`,
  `ihello<Esc>:wq` leaves `RAM:t.txt` = `hello` and the process ends (it takes up to
  15 s to leave). The screen is the full-screen editor: `hello` on the first row, 23-24 rows of
  `~`, the ruler `RAM:t.txt  0,0-1  All`, no escape sequence shown as text. So README.txt:95
  ("ROM CON: cannot show vtcon's sequences") does not hold for Neovim on this
  image: with `TERM=vtcon` and the kit's terminfo copy, ROM CON and KingCON both drew
  it. Startup messages seen: `E1568: Terminal did not respond to DSR request for
  'background' color` (neither console answers the query; one Enter) and, in one TERM-unset run,
  `E303: Unable to open swap file for "RAM:t.txt"`.
- Neovim needs the patched ixemul: with the stock 48.3 it starts and stops at once
  ("Failed to start the in-process UI!"); with none, the library requester
  appears. (`-u NONE` was kept as in the plan; `-n` was not tried for E303.)
- Python `-c "print(6*7)"` gives `42` with the stock 48.3 and with the kit's 80.1;
  the REPL runs a typed line (`open('RAM:q.txt','w').write('x')` creates the file)
  in both consoles with TERM unset. Python3 needs no `Python3:` assign and no
  `S:User-Startup` block for this: it found `lib/python314.zip` next to the
  executable. So Python needs only an ixemul of version 47 or newer, not the
  patched 80. A stock Workbench has none, hence the requester in the `none` rows.
- Up in the Python REPL (TERM unset): ROM CON does not recall (F); KingCON does (P).
- **Python REPL with `TERM=vtcon` (both ixemul libraries, both consoles): the
  REPL starts, prints a prompt and runs a typed line, then Up leaves it
  unresponsive (later typed text is not echoed) and the next input crashed the
  emulated machine every time (9 crashes logged: 4 sessions of the matrix, repeated
  runs and hand probes).** See Crashes.

## Crashes (the plan asks for the alert number)

- Seen in `python-{rom,king}-{stock,kit}-vtcon`. The guest stopped answering; the agent
  connection was refused or closed; the machine started again by itself about two
  minutes later (`BOOTX:boot.log` rewritten), losing the state of the test window.
  On 2026-10-13 12:23:51 FS-UAE itself died (host SIGSEGV, null write in the
  emulator, report `~/Library/Logs/DiagnosticReports/fs-uae-2026-10-13-122351.ips`,
  copied to `build/rig/standalone/crash-fs-uae-2026-10-13-122351.ips`); the other
  times FS-UAE stayed up; looked at once, its window was a solid cyan screen (the machine
  restarting).
- **No alert number could be read**: no Exec alert ("Software Failure", "Guru
  Meditation") was on the Workbench screen or in the emulator window when looked at,
  the serial port log stayed empty (`build/rig/serial.log` untouched), and FS-UAE was
  started from the `open` command line which keeps no log (one run was started with
  its output to a file: the file stayed empty). So this is "the emulated machine
  stopped, cause not found", not a guru number. Trigger narrowed by hand: Python REPL,
  `TERM=vtcon`, Up pressed, then any further input to that window.
- The script handles it: on a lost connection it reads the emulator window, records
  the session as failed with that text and the last step, waits for the rig (starting
  it again if FS-UAE is gone), and goes on (`crashes.log` in the verdict folder).

## Answers to the plan's unverified claims

| Claim | Answer |
|---|---|
| ROM CON: cannot show vtcon's sequences (README.txt:95) | Not true for Neovim with TERM=vtcon: drawn correctly in ROM CON and KingCON (the DSR background query goes unanswered, one message). Claude and amiga-pi do not use sequences there (line mode). |
| Neovim and Python start with a stock ixemul 48.x, with no TERM, in ROM CON/KingCON | Neovim: not with 48.3 (needs 80). Python: yes with 48.3 and with 80.1, no TERM needed, both consoles. TERM unset is fine for both; TERM=vtcon in a non-UP-Term window crashes Python's REPL. |
| amiga-pi's line mode is usable | Yes in both consoles (start, tool call, Ctrl+C, screen). |
| amiga-pi needs mathieeedoubbas.library; stock 3.2 has it | Present on the clean 3.2 (47.2). |
| `Version FILE <file> <ver> <rev>` sets WARN for an older `$VER:` in a plain file | Yes on 3.2: a file with `$VER: UP-Term 1.5 (1.1.2026)`: `FILE 1 4` and `FILE 1 5` and `FILE 1 0` rc 0; `FILE 1 6` and `FILE 2 0` rc 5 (WARN). 3.1 not tested. |
| The clean `--os 32` image boots with FS-UAE's bsdsocket | Yes: Claude and amiga-pi connected to the fixture over TCP. |
| Where the owner's KingCON lives | `L:KingCON-handler` (64088 bytes) and `DEVS:KingCON-mountlist` on `nyhd2.hdf` DH0:, mounted from his User-Startup (`Mount KCON: from DEVS:KingCON-mountlist`); no DOSDriver. |

## Recommended packages.txt rows (not applied; another agent edits dist/packages.txt)

Rule used: stand-alone = own name, `-` in needs-base, no ixemul. Columns as in plan D1
(`id archive version needs-base ixemul record handed`). Versions and the `handed`
column are unchanged.

| id | Current row | Recommended | Why (evidence) |
|---|---|---|---|
| claude | `UP-Term-Claude 1.0 1.0 - UP-Term:pkg/UP-Term-Claude` | `claude  Claude  1.0  -  -  ENVARC:uppkg/Claude  Files/Claude,Files/ClaudeCode` | Starts, converses with a tool call, keys work, in both consoles, data falls back to `ENVARC:`. One open point for the owner: Up recall fails in ROM CON (line mode). If Up must work in every console the answer is needs-base. Recommended: stand-alone, because the plain console has no recall to offer. Open: C:ClaudeCode and C:uptelnet were not tested. |
| pi | `UP-Term-pi 1.0 1.0 - UP-Term:pkg/UP-Term-pi` | `pi  amiga-pi  1.0  -  -  ENVARC:uppkg/amiga-pi  Files/amiga-pi` | Same evidence as Claude; bash falls back to the AmigaShell and says so to the model; mathieeedoubbas.library is there. Same Up note. |
| nvim | `UP-Term-nvim 1.0 1.0 80` | keep: `nvim  UP-Term-nvim  1.0  1.0  80  UP-Term:pkg/UP-Term-nvim` | Does not start without ixemul (requester) or with the stock 48.3 ("Failed to start the in-process UI!"); works only with the kit's 80.1. The name keeps `UP-Term-` (needs the base's ixemul). |
| python | `UP-Term-python 1.0 1.0 80` | `python  UP-Term-python  1.0  1.0  47  UP-Term:pkg/UP-Term-python` | Runs on any ixemul 47 or newer (48.3 and 80.1 both work, TERM unset, no `Python3:` assign needed) but a stock Workbench has none, so it is not stand-alone under the owner's definition: needs-base stays. The ixemul value can be 47 instead of 80 (the Installer's library check takes the lowest version that works). If the owner wants Python to be installable without the base, the archive must carry an ixemul: that is a different decision (licence, the patched library replacing a user's). Caveat for the base: a ROM CON/KingCON window with `TERM=vtcon` (set globally by the rig's boot, possibly by the base's User-Startup) makes the Python REPL unusable and crashed the emulated machine; the base must not set `TERM=vtcon` outside UP-Term windows, or Python's REPL needs its own TERM. |

If the owner's definition is applied literally, only `claude` and `pi` leave the base's
name; `python` stays `UP-Term-python` and `nvim` stays `UP-Term-nvim`.

## Found (not asked)

- Banner text of Claude and amiga-pi says "in UP-Term" even without it; a
  stand-alone archive should not.
- The clean `--os` image is not stock for add-on tests: `BOOTX:go` puts `VTCX:ixp6`
  (patched ixemul 80) and the ncurses ixlibrary path in `LIBS:`, `TERM=vtcon`,
  `TERMINFO=/VTC/terminfo`, `TERMCAP=/etc/termcap` in `ENV:`. The script resets them
  per window and puts them back at the end (`--cleanup`).
- `E303: Unable to open swap file for "RAM:..."` from Neovim when TERM is unset; the
  edit still worked. Cause not checked.
- Not done: Ctrl+C on the Python REPL, ClaudeCode/uptelnet, https/AmiSSL, a real Amiga.
