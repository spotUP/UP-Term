---
date: 2026-10-15
topic: Finish run, session 3 (ixemul compat, ports batch B, requester watch)
tags: [finish-run, ixemul, ports, rig, handoff]
status: final
---

# Finish run, session 3

## Task(s)
Run plan `thoughts/shared/plans/2026-10-15-finish-open-plans.md` to 100%.
Ledger: `thoughts/shared/plans/2026-10-07-amiga-pi.progress.md`, sections
"## Finish run 2026-10-15" and "## ixemul compat + column + jq 2026-10-15".
Trust the ledger over this file where they differ.

## Recent changes (all local, nothing pushed)
- vtcon `68216fb1`: `tools/rig/ami.py` requester watch. A command that is
  blocked by a requester that opens during it (System Request, ixemul.library
  message, or an answer-buttons-only window) ends with
  `ERR: <cmd> is blocked by the requester ...`; it is logged to
  `rig/requesters.log`. `ami.py requesters` lists the open requesters.
  Tests in `tools/test_rig_ami.py` (`make test ONLY=rig_ami`).
- ixemul-vtcon `feature/ixemul-80` (local): merge `f6a0486` of compat-gaps
  (libixcompat strtonum, reallocarray, isnormal, strptime, single-thread
  pthread subset, getline/getdelim; selfcall_check -O0 jsr form), then
  `ae2daba` (pthread.o builds against the tree's `<pthread.h>`), then a
  `.gitignore` commit for `__pycache__/`. Strptime review fixes are in `c24b682`.
  `make -C compat test` and `make -C tests/host test` pass.
- upterm-ports `main` fast-forwarded to `5d4f810` (was fix/finish-fp4): column,
  jq (rig still FAILS, parked), joe, mg, nc. `make test` passes.
  The fp4 worktree and branch are removed.
- vtcon `79bd9a34`: tldr and wget as vshrc functions (`make test ONLY=vshrc_net`,
  `tools/rig/wget_rig.py` PASS 7 of 7 on rig 3).

## Learnings
- Every jq run that the heap-corrupt requester aborts leaves rig 2's wasabid stuck;
  restart the rig between probes.
- compat `%.o` rule has no `-I../include`; a new header that the toolchain also
  ships (newlib) is shadowed. New compat objects that need the tree's headers
  need an explicit rule.
- A vsh background job plus `sleep` in one script left `sleep` stuck and twice
  killed wasabid on rig 3 (nc loopback case, not in check/cases). Not chased.
- "Please insert volume UP-Term:" was not reproduced on rigs 2 or 3. The watch
  in ami.py now names any such requester when it blocks a command.
- An agent with 154 tool calls (batch B) far exceeded the ~40 cap; split rig
  batches to 1-2 ports per agent.

## Next steps (ordered)
1. Owner decision: jq instrumented malloc/free build (research note
   `thoughts/shared/research/2026-10-15_jq-heap-corrupt-parked.md`) or keep parked.
2. make dist + dist-check (FP4.2 batch B left them for the coordinator).
3. W46 (c) 16-colour mode, then (b) Prefs gadgets, (d) rig check.
4. FP4.3-4.6, FP1.6 rest, FP1.10, FP3.x, Phase 5.
5. Move the per-recipe libc gaps (asprintf, strndup, strtonum, getaddrinfo,
   strlcpy in pkgs/nc/compat and others) into ixemul-vtcon compat.
6. ixemul getopt returns EOF silently for `--version` (column case dropped).
7. Push as spotUP after the owner says so: vtcon main (~35 ahead),
   upterm-ports main, ixemul-vtcon feature/ixemul-80.

## Other notes
- upterm-ports has an untracked `pkgs/coreutils/recipe.mk` on main that no
  commit and no agent of this session made. Left untouched.
- Untested: strtonum on ixemul goes through `strtoq`; ERANGE there unverified.
- Owner items: FP0.3 (no AmigaOS 3.2 tree), real-key half of T13, CPython/Neovim
  gcc 16 rebuild queue, eyeball check on rig 2 (`VTC:rgbprobe HOG`,
  `VTC:UPDemo FULL 6/7`).
