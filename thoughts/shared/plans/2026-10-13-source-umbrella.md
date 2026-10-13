---
date: 2026-10-13
topic: Group the UP-Term source repos under ~/Code/UP-Term
tags: [layout, repos, build]
status: draft
---

# Source umbrella: ~/Code/UP-Term

Owner 2026-10-13: too many UP-Term dirs scattered in ~/Code; group them per project.

## Decisions
- Umbrella: `~/Code/UP-Term`.
- Members: vtcon, amiga-pi, upterm, ixemul-vtcon, cpython-amiga, neovim-amiga, tmux-amiga,
  screen-amiga, upterm-ports, amiga-gcc, amiga-gcc15. Worktrees follow their repo
  (vtcon-demo-perf, ixemul-vtcon-y1).
- Not members: dctelnet-* (DCTelnet is its own project with its own scattered dirs; a
  separate umbrella is a later, separate task), CGTerm-2.0 (own project),
  amiexpress-doorserver (BBS project).
- The build already resolves siblings through `UPTERM_ROOT`; its default becomes
  `~/Code/UP-Term`. dctelnet-v2 and amiexpress-doorserver, which the build reads today via
  `$(UPTERM_ROOT)/...`, get their own variables (DCTELNET_ROOT, AMIEXPRESS_ROOT) defaulting to
  their current paths.

## Constraints
- Only when no agent runs and no rig runs (rigs boot from build/rig*/ paths; FS-UAE configs hold
  absolute paths). Stop rigs with the safe stop first; rig 3 (owner) only with the owner's ok.
- `git worktree` stores absolute paths: after each move run `git worktree repair` in the main
  repo and each worktree.
- Hardcoded `/Users/spot/Code` paths: vtcon 9 files, amiga-pi 99 files (counted with
  `git grep -l` outside thoughts/, 2026-10-13); replace with UPTERM_ROOT or repo-relative paths,
  not with the new absolute path.
- Claude Code memory is keyed on the cwd: copy `~/.claude/projects/-Users-spot-Code-vtcon` and
  `-Users-spot-Code-ixemul-vtcon` memory dirs to the new keys.
- SDK install (`~/opt/amiga`) and toolchain prefixes: check for paths into the old dirs.

## Steps
1. Branch per repo with the path fixes (UPTERM_ROOT default, new vars, hardcoded paths); gate on
   old layout (must still pass with UPTERM_ROOT=/Users/spot/Code).
2. Merge, push. `mv` the repos into ~/Code/UP-Term; `git worktree repair`.
3. Gate on the new layout with no UPTERM_ROOT set: vtcon + amiga-pi `make test`, `make amiga`,
   `make dist`, `make dist-check`; one rig boot (060) per rig config.
4. Copy memory dirs; update RULES.md / ledger / handoff paths; new session started from
   ~/Code/UP-Term/vtcon.
