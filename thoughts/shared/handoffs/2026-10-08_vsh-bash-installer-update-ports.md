---
date: 2026-10-08
topic: Session handoff - vsh bash plan, Installer update mode, 3.2 Installer, Unix tool ports, ixemul argv
tags: [handoff, vsh, installer, ixemul, ports, console-device, rigs]
status: final
---

# Session handoff 2026-10-08

The owner stopped the session at about 23:40. All three rig agents were stopped mid-task. Nothing was pushed after `4ccd812`.

## Task(s)

| Work | State at stop |
|---|---|
| vsh bash plan (`plans/2026-10-07-vsh-bash.md`) | 92 of 95. Open: V74 (real configure), V94 (owner window check), V95 (landing run, after V94) |
| Unix tool ports (`plans/2026-10-04-unix-tool-ports.md`) | 12 of 53 ticked. 14 tools pass their rig checks and are staged. Kit step 1.10 committed (`85a76db`); its rig verification was interrupted |
| Installer update mode (`plans/2026-10-08-installer-update.md`) | Done and rig-tested on 3.1 and 3.2 (U1-U5). Owner test on the Replay is still open |
| Console-device plan (`plans/2026-09-30-console-device.md`) | T3.5, T3.6, H5.3, DV5, DV6 ticked. Three Kickstart 3.1.4 differences are being fixed; the fix is uncommitted |
| Next kit on the Desktop | Not built. The last kit there is `~/Desktop/UP-Term-d1a2e89.lha` (old) |

## Critical References

- Ledger (gitignored): `thoughts/spot/todos.md`. Every decision of the day is appended there under 2026-10-08.
- Installer ledger: `thoughts/spot/notes/2026-10-08-installer-ledger.md`.
- vsh progress: `thoughts/shared/plans/2026-10-07-vsh-bash.progress.md` (analyses for V85, a051, V44, V74, argv design).
- Ports ledger: `~/Code/upterm-ports/thoughts/shared/plans/2026-10-08-phase1-progress.md`; kit patch handoff `~/Code/upterm-ports/thoughts/shared/handoffs/2026-10-08_vtcon-kit-userland.patch` (applied as `85a76db`).
- Rigs: `tools/rig/paths.py`. `UPTERM_RIG` = 1 (build/rig, port 7846), 2 (build/rig2, 7847) or 3 (build/rig3, 7848). Rig 3 needs `--max` (xz needs 94 MB). 3.2 runs use `UPTERM_RIG_FLAGS=--os32`.
- Kickstart 3.1.4 ROM: `~/Downloads/kickstart 3.1.4/kick.a1200.46.143`, used through `UPTERM_ROM`. Never copy it into a repo.

## Recent Changes (local commits)

Unpushed: vtcon main 57, ixemul-vtcon feature/ixemul-80 27, upterm-ports main 21, cpython-amiga main 1.

### vsh
- `a9200ce` / `ac23942`: PTY: serves pipes, and every vsh pipe uses them (V85 `coproc cat`).
- `fff0540`: V93 programmable completion. V92 bind/.inputrc (`7251052`), V42 COLUMNS/LINES (`55d942a`), V59 (`3727524`).
- `702c190`: 256 KB stack.
- `0ebfb56` / `61a6e8d`: V44 array memory, 23.64 B/element, measured by `v44mem_rig.py`.
- `2c406f2`: one locked heap for all vsh processes (fixes the a051 crash); V8 ticked.
- `4dee027`: shared-state audit, 6 fixes.
- `650ed6c` V84, `50f59d1` / `731e64e` copy-on-write variables/functions/aliases for waited subshells, `541d7c4` heredoc quotes, `65be119` V78 QUIT trap, `64fcf82` V69.
- `3dbf72f`: a drawer named like a command is not run as the command.
- `5ff6c44` / `840aac3`: ixemul programs get their argv out of band (after the line's newline, or the local variable `__ixargv` for Run/Resident). `PROGDIR:` is set to the command's drawer, so python3 works from vsh with no PYTHONHOME.
- `c7e857c`: the host harness uses posix_spawn (`posix/special_errors` 4.63 s -> 1.96 s).

### ixemul-vtcon
- `5e4b457`: FIONREAD on pipes (`cat -n` over a pipe).
- `74c77ac` -> `5cc91cd` -> `39f6760` -> `2cda2f6`: argument parsing. Final rule: inside quotes `*` is an ordinary character, and vsh's out-of-band argv is used when present and its hash matches.
- `2767cac`: each C99 function is its own libixcompat archive member.
- The ports agent's POSIX fixes, `03d9a2f..6279020`.

### Installer / kit
- T3.6 3.2 Installer (`8f38b21`, `1f86d9c`, `53e71c4`).
- Page-text check and limits (`1d7d7dd`, `659cdb0`, `b020eb9`).
- Update mode (`b499ee0`, `e23a546`, `de372e1`, `988351e`, `dedf80c`). Update takes 60 s, a full install 467-746 s.

### Toolchain
- The fixed cc1 is installed in `~/opt/amiga`. Backup: `~/opt/amiga/libexec/gcc/m68k-amigaos/6.5.0b/cc1.pre0004-2026-10-08`.

## Uncommitted work at stop (NOT tested, NOT committed)

Saved out of tree: `thoughts/spot/notes/2026-10-08-uncommitted-at-stop.patch` (18 KB) and `thoughts/spot/notes/2026-10-08-closeread.c`.

1. **Rig 3 agent, DV5 3.1.4 fixes, mid-work.** Files: `device/upc_core.[ch]`, `device/upcon_device.c`, `device/upcon_unit.c`, `render/vtwin.c`, `tests/test_upcon.c`, `tools/rig/devverify_rig.py`, the console-device plan, `Makefile` (closeread rule) and the untracked `tests/amiga/closeread.c`. The three differences are:
   - a STANDARD unit with full scrollback uses 68,112 bytes, over the 65,536 limit;
   - `cu_Mask` is 0xFFFFFFFF in the ROM and 1 in ours;
   - the ROM's CON: window reports 25 rows, ours 12.

   The agent was adding the check to `devverify_rig.py`. Review the diff, then finish, test and commit it or discard it.
2. **Rig 1 agent, `tools/rig/install_rig.py` (+78 lines) and `installer_rig.py` (+2).** This is the 1.10 kit verification. install_rig pass 2 was 83 of 83 (python printed 42 and 8). Pass 3 (`--move`) had not run. The owner also saw an Installer error on a rig, "object not found / Skip / failed, return code 10"; its cause is not yet known.

## Learnings

- AmigaDOS ReadItem treats `*x` inside quotes as an escape. Unix users type `"*.c"` and `"2**3"` in the AmigaShell, so ixemul must not follow ReadItem there. vsh therefore passes argv out of band.
- vbcc's vc.lib `malloc` has no lock, and vsh subshells share it. Any static or hidden library state is shared by all vsh processes; count it from the vlink map, not from grep.
- `$(Avail)` measures memory inside a subshell that holds a copy of the array, so it double-counts.
- 3.2 links `ENV:` to `ENVARC:`. Copying into it and then `Delete ENV:x ALL` hangs the RAM disk.
- 3.2's native PAL screen doubles the pointer's y; click through `ami.click_px`.
- FS-UAE's VTC: drawer can keep names the Mac deleted. Restart the rig, or delete from the Amiga side.
- `make dist` stops on a kit file that `dist/parts.txt` does not claim.
- `build/dist` is shared, so rig runs should use a private kit copy.
- Do not run Python rig scripts from the scratchpad directory (a `bisect.py` there shadows the stdlib). Use `python3 -I`.
- The owner wants Amiga-side tests in visible, titled windows (memory `feedback_visible-rig-runs`).
- The rogue completed agent `a8688a2e4db277b73` kept waking up. Ignore it.

## Artifacts

- Plans: `thoughts/shared/plans/2026-10-07-vsh-bash.md` (+ progress), `2026-10-08-installer-update.md`, `2026-10-04-unix-tool-ports.md`, `2026-09-30-console-device.md`, `2026-10-07-amiga-pi.md` (not started).
- New rig tools: `repeat_rig.py`, `v44mem_rig.py`, `substmem_rig.py`, `vshpath_rig.py`, `trapsig_rig.py`, `ixargv_rig.py`, `bind_rig.py`, `vshcomp_rig.py`; Amiga probes in `tests/amiga/` (taskdump, ixfionread, readitem, pipeprobe).

## Next Steps (ordered)

1. Decide what to do with the uncommitted patch (item 1 above). If kept: finish the DV5 fixes, run devverify/condev/concon under the 3.1 and 3.1.4 ROMs on rig 3, `make test`, commit.
2. Finish 1.10. Find the "object not found / return code 10" Installer error. Run install_rig pass 3 (`--move`) and installer_rig default + update on 3.1 and 3.2. Tick the port items (1.2-1.10 and the staged phase-2 items) with evidence.
3. Build the next kit: `make test`, `make dist` with build295 at ixemul HEAD (contains `2cda2f6`), copy to `~/Desktop/UP-Term-<sha>.lha`.
4. V74 on rig 2. GG-Lite gcc 2.95.3 is in `build/rig2/vtc/gglite` (GG: assign is live-session only; the PATH notes are in the agent's notes in the progress file). The configure script is `build/rig2/vtc/cfg/run5.sh`. The second attempt (with GG ncurses 5.5) was running; collect `RAM:less-done.txt` if rig 2 is still up, else re-run. Pass = configure exits 0 with Makefile, defines.h and config.status, and no vsh-caused difference from the host run in `scratchpad/lessh/hrun.txt` (the scratchpad dies with the session: re-create it with bash on the host if needed).
5. Owner checks, then V95 landing run.
6. Push when the owner says so (four repos, counts above).
7. Later queue (from `thoughts/spot/todos.md`):
   - subshells still copy history and the hash table;
   - vsh prints `alias x=echo X` where bash quotes it;
   - the 3.2 ReadItem measurement;
   - tmux slowness and `-V` hang;
   - the LIBS: loss on rig 1;
   - the -Os speed check;
   - amiga-pi.

## Owner checks (Blocked on owner)

- **V94 on rig 2:** run `NewShell "XCON:0/20/640/300/V94/CLOSE"`, then `VTC:vsh`:
  1. `echo one two`, then `echo !!`;
  2. `complete -W 'alpha beta bravo' kw`, `kw al`, Tab;
  3. `set -o vi`, edit with `0wcwxyz`.
- **Replay:** install the next kit, then run Install with a newer kit and choose Update. Only changed files may get new dates.
- **The Kickstart 3.1.4 behaviour differences:** accept the fixes once they are reviewed.

## Other Notes

- FS-UAE for rig 1 and rig 2 were still running at stop; rig 3 was stopped. Stop them with `UPTERM_RIG=N python3 tools/rig/rig.py stop` when nothing needs them.
- The owner allowed three agents at once this session (one per rig); the global rule is two.
- The Mac ran at load 70-170 from another project (furnace headless renders, node), which slows all rigs.
