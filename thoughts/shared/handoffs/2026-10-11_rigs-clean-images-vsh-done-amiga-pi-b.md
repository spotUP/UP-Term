---
date: 2026-10-11
topic: Session handoff - vsh plan done, Installer on clean OS images, amiga-pi phase B done, phase C started, console history move
tags: [handoff, vsh, installer, rigs, clean-images, amiga-pi, llm, console-handler, ports]
status: final
---

# Session handoff 2026-10-09 .. 2026-10-11

Continues `2026-10-08_vsh-bash-installer-update-ports.md`. Three agents were still running when this
was written (see "In flight"). The ledger `thoughts/spot/todos.md` (gitignored) has every dated note
and every owner answer of these days; trust it over this file for anything newer.

## Task(s)

| Work | State |
|---|---|
| vsh bash plan (`plans/2026-10-07-vsh-bash.md`) | **95 of 95, implemented** (`f24f264`). V74 without CONFIG_SHELL, V94 (owner + rig), V95 landing run (bashdiff 993/999, the 6 classified; userland_rig 239/239 on rig 2 --max) |
| Console-device plan (`plans/2026-09-30-console-device.md`) | 36 of 36. DV5 Kickstart 3.1.4 fixes `ac71868`, `c899bc6`; owner approved dropping TopazPro 16 for CON: windows (`bf10290`) |
| Unix tool ports (`plans/2026-10-04-unix-tool-ports.md`) | 12 of 53 ticked. Phase 1/2 tools pass on the rig and are staged, waiting for 1.10 (kit install). Phase 3: 3.1 file, 3.2 tree, 3.4 watch, 3.5 script, 3.6 ncdu, 3.9 fzy pass on rig 3 and are staged (upterm-ports). 3.3 ps/top decided, not started. 3.7 coreutils 9 not started |
| 1.10 kit + Installer verification | In flight (Installer agent). Pass 2 passed (old 020 rig). Pass 3 fixes in the rig script. Clean-image runs on 3.1 and 3.2.2 still open |
| amiga-pi (`plans/2026-10-07-amiga-pi.md`, ledger `...amiga-pi.progress.md`) | Phase A 6/6, **phase B 12/12**. Phase C in flight. Plan overall 36 done / 198 open |
| Console history move + HISTFILE, Ctrl-D EOF | In flight (vsh agent), uncommitted |
| Kit on the Desktop | **Not built.** Still only `~/Desktop/UP-Term-d1a2e89.lha` (old) |

## In flight at writing (agents may finish after this file)

1. **vsh agent** - handler history moves from `ENVARC:vtcon.history` to `UP-Term:var/` (reuse
   `claude/datadir.c`, migrate the old file), the handler honours vsh's `HISTFILE` via the existing
   console-arming packet, rig scripts use a throwaway HISTFILE; Ctrl-D on an empty line = EOF,
   non-empty = flush, only while vsh runs a program in the window. Uncommitted files:
   `handler/histfile.[ch]`, `tests/test_histfile.c`, `tools/rig/histfile_rig.py`,
   `tools/rig/vshwin.py`, `handler/complete.[ch]`, `lineedit.[ch]`, `vtcon_handler.c`,
   `vtcon_packets.h`, Makefile lines.
2. **Installer agent** (rig 1) - queue in order: boot hook creates every assign before first use +
   no-requester boot check for every `--os`; Installer default + update on clean 3.1 and 3.2.2;
   Uninstall var/ rig check (code landed `cdcbec3`); tick ports 1.2-1.10; then `make test`,
   `make dist`, kit to `~/Desktop/UP-Term-<sha>.lha`.
3. **amiga-pi phase C agent** - phase C (minimal agent loop) plus three phase-B leftovers: OpenAI
   recordings vs the real schema (`b652cde` addresses part), pi's OpenAI-compat host list,
   Anthropic refusal -> error stop checked against pi.

Backup of the whole uncommitted tree at the network stop (2026-10-11 morning):
`thoughts/spot/notes/2026-10-11-uncommitted-after-network-stop.patch` (78 KB). A stray untracked file
named `-` sits at the vtcon root: owner unknown, not deleted.

## Critical References

- Ledger: `thoughts/spot/todos.md`, sections "Owner answers 2026-10-07 evening" and
  "Owner answers 2026-10-10" - **read before briefing any agent** (a missed entry caused a wrong
  amiga-pi D4 on 2026-10-10).
- Rigs: `tools/rig/rig.py`. `--max` = 68060-NOMMU + FPU, 1 GB Z3 (rig.py:159-169). New:
  `--os <version>` boots a clean installed Workbench image, own copy per rig (`772b964`,
  `cfdbc89`, `76df73b`).
- Clean images (read-only originals, never modify): `~/Downloads/clean-wb/Clean Workbench Installs/`
  `Workbench_{1.0..3.9}.hdf` (3.1, 3.1.4, 3.2, 3.2.1, 3.2.2, 3.5, 3.9 among them). Little free space:
  data goes on a mounted directory drive.
- Data-directory rule (C:Claude, amiga-pi, soon the console history): `claude/datadir.c`.
  `UP-Term:var/<program>`; without `UP-Term:` the old `ENVARC:` place, never a volume requester.
- amiga-pi: `~/Code/amiga-pi` (local repo, no remote) `ARCHITECTURE.md`, `COMPATIBILITY.md`;
  shared layer `vtcon/llm/`; rig check `tools/rig/amigapi_rig.py`.
- `repos.lock`: `~/Code/upterm/repos.lock`, pinned in `cc1bac8`.

## Recent changes (since 68962d8)

vsh / ixemul:
- `/bin/sh` and `/bin/bash` are vsh: vsh `2eb63a1`, `#!` only when LoadSeg fails `812cd77`;
  ixemul `06baad5` (execve maps both to `_PATH_BSHELL`).
- `sh -c` child stdin on a pty: `9b7e29d` (own handle opened at the stream's handler), rig check
  `5b5792a` (shstdin_rig.py 15/15).
- ixemul from the ports work: `8ff7a77`, `a0cb0bd`, `47b6874`, `a105524`, `69feef4`.

Kit / Installer / rigs:
- Kit file modes by one rule, `make dist` stops on a file without u+w (`6d2e990`) - the read-only
  less COPYING/LICENSE.
- install_rig watched window (`5151ed3`, `e5de9bb`, `a78b0f2`, `ed37b2c`, `7bd94a0`). The
  "object not found / return code 10" error was the rig loop, not the kit.
- Uninstall asks about `UP-Term:var` (default No), README paths (`cdcbec3`).

C:Claude / llm:
- Growing files out of `ENVARC:Claude` to `UP-Term:var/Claude` with migration (`c30c530`, rig
  `7acb201`). Long-TMPDIR ASan bug fixed (`70c1ca5`).
- C:Claude runs through `llm/` (`73f95f8`); request goldens (`4623580`), fixture routes in make test
  (`4a4870e`), openai-completions backend (`d8d7779`), B12 rig check (`ab24b17`).

## Learnings

- **Every rig run uses `--max`** (owner since 2026-10-05; forgotten in briefs on 2026-10-10, cost
  hours). Rigs run only while in use: start right before, stop after. Both are in memory
  `feedback_rig-060-max`.
- Ask the owner every question with AskUserQuestion, options, recommended first (memory
  `feedback_questions-as-options`).
- Three volume requesters in one day (ENV:, UP-Term:, VTC:) all came from boot order on new rig
  setups. The fix is one boot hook that makes every assign before first use, plus a no-requester
  check - in flight.
- A program must never touch `UP-Term:` (or any assign) without a no-requester probe: C:Claude now
  does this; a requester blocks the rig run and the agent waiting on it.
- 3.1's Startup-sequence copies all of ENVARC: into RAM every boot: no growing files in ENVARC:.
- Rig test commands land in the console's shared history and show as suggestions in owner checks;
  the HISTFILE work fixes it. A "bug" in V94 was only that.
- The console handler's own line history ignored HISTFILE (`handler/complete.h:38`).
- On rig 2 ixemul's `/bin/sh` is `GG:bin/sh`, a separate copy of vsh; refresh it after a vsh build.
- Ctrl-\ ends input in an XCON window; Ctrl-D did not (being changed under vsh).
- `make test` outside `~/Code/vtcon` needs `UPTERM_ROOT=/Users/spot/Code`.
- Agents sharing one tree must stage only their own hunks; one agent committed another's Makefile
  lines (`ab24b17`, undone in `30dd0e9`).
- Weekly rate limit (2026-10-09) and a network outage (2026-10-11) both killed all agents mid-run:
  resume them with SendMessage; check rigs and the uncommitted tree first.

## Artifacts

- Plans: vsh-bash (+progress, implemented), console-device, unix-tool-ports, amiga-pi (+progress),
  installer-update.
- New rig tools: `shstdin_rig.py`, `claude_datadir_rig.py`, `amigapi_rig.py`, histfile_rig.py
  (uncommitted), `tests/amiga/binshprobe.c` (ixemul), `tests/amiga/closeread.c`.

## Next steps (ordered)

1. Collect the three agents' reports; check their commits; the stray `-` file.
2. Installer agent queue (see "In flight" 2) to the kit on the Desktop.
3. After the kit: pin neovim-amiga (86a429a) and tmux-amiga (9436832) in repos.lock (owner: with the
   next tested kit); tick ports 1.2-1.10 and phase-2/3 items.
4. Move rigs 2 and 3 to clean images too (owner decision); `rig.py install` must provide what the old
   system disks had (ixemul build295, handler/device, vsh, GG:, GG-Lite for rig 2).
5. 3.3 ps/top: VBlank-interrupt sampler (owner decision), when the Makefile `amiga:` target is free.
6. amiga-pi phase C onward.
7. Push vtcon (18 unpushed at writing) after `make test` at the pushed SHA.
8. Later queue in `thoughts/spot/todos.md` (subshell history copy, alias quoting, tmux slowness,
   -Os speed check, LIBS: loss on rig 1, 3.7 coreutils 9, AmigaSynergy port).

## Owner checks (Blocked on owner)

- Replay: install the next kit, then Install again with a newer kit and choose Update; only changed
  files may get new dates.
- Push go-ahead for vtcon's unpushed commits.

## Other Notes

- Pushed 2026-10-10: ixemul-vtcon, upterm-ports, cpython-amiga at HEAD; vtcon up to `c30c530`
  (make test green); upterm `cc1bac8`.
- The owner allowed three agents at once this session (global rule says two).
