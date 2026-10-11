---
date: 2026-10-11
topic: Start here - state for the fresh session after the 2026-10-09..11 coordination session
tags: [handoff, kit, installer, amiga-pi, rigs, tokens]
status: final
---

# Start here (fresh session, 2026-10-11 evening)

Older detail: `2026-10-11_rigs-clean-images-vsh-done-amiga-pi-b.md`,
`2026-10-11_installer-clean-images.md`, and on branch `amiga-pi-parity`
`2026-10-11_amiga-pi-parity-paused.md`. Ledger: `thoughts/spot/todos.md` (gitignored), sections
"Owner answers" (all dates) and "after restart: queue".

## State at writing (16:00)

- vtcon pushed at `6bc54f0` (make test green). ixemul-vtcon, upterm-ports, cpython-amiga pushed
  earlier; upterm `repos.lock` pinned `cc1bac8` (neovim-amiga / tmux-amiga pins wait for the kit).
- Plans: vsh 95/95, console-device 36/36 (+ DV4 fix `6bc54f0`), ports 1.x/2.x ticked, phase 3 has
  3.7 coreutils 9 open. amiga-pi A, B, C, D done; parity follow-up running.
- Two agents were still running when this was written. If they finished, their commits are in
  `git log`; if not, check:
  1. Kit agent (rig 1): built `build/dist` 15:07, Installer runs on clean 3.1 / 3.2.2 (--max,
     --fresh). First clean-3.1 run failed `.orig`/ixnet checks and the Uninstall check on
     `VTC:Apps/UP-Term`; second run started 15:51. Goal: kit on `~/Desktop/UP-Term-<sha>.lha`, then
     pin `repos.lock` (neovim-amiga, tmux-amiga + the shas used).
  2. amiga-pi parity (worktree `.claude/worktrees/pi-parity`, branch `amiga-pi-parity`): Q1, Q2
     done; working on read's image shrink / BMP->PNG. Rig 2.

## Open queue (order)

1. Finish/verify the kit (above). The owner then tests Install + Update on the Replay.
2. amiga-pi parity to 15/15, then merge.
3. coreutils 9 (`thoughts/spot/notes/2026-10-11-coreutils-stop.patch`, GPG check result unread).
4. ixemul-vtcon `make test`: `own_c99_link` fails (`lldiv_t`), compiles against old toolchain headers.
5. Rig 3 `sys.hdf` hangs in devverify DV2 romprobe (old device too; clean images pass).
6. Known user-facing limitation (owner's choice): deep screen with a full 32-entry map snaps colours.

## Working rules that cost days when forgotten

- Every rig run `--max`; rigs up only while used. Read "Owner answers" before briefing agents.
- At most 2 agents; agents that edit vtcon use their own worktree.
- Ask with AskUserQuestion, recommended option first. Red `diff` TOKENS note in replies.
- Images: never in the main context (hooks enforce it; "latest shot" = newest in
  ~/Pictures/claude-inbox via a haiku subagent).
- Main session on Sonnet for coordination (`/model sonnet`).
