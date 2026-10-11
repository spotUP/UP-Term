---
date: 2026-10-11
topic: amiga-pi tools to pi parity (Phase D follow-up), paused for a session restart
tags: [amiga-pi, phase-d, parity, handoff]
status: final
---

# amiga-pi parity follow-up: handoff

Resumed and finished the same day: see the ledger section "Phase D parity follow-up" (every item
ticked with its evidence). This page is the pause's record only.

## Task(s)
The coordinator reviewed Phase D (done, 16 of 16, vtcon bbc604f on main, amiga-pi aca7948) and asked
for its differences from pi to be closed, because the scope is all of pi:
1. edit: pi's exact matching (NFKC fuzzy fallback, CRLF, BOM), tested with pi's own cases.
2. grep: honour .gitignore as pi does (rg).
3. find: pi's result format and order (fd).
4. read: pi's output format (no line numbers).
5. Gate "Always in this project": stored per project, under the data-directory rule.
6. bash: `vsh <script>` against D3's `vsh -c`, with evidence, or switch.
7. Rig: prove the bash tool ran vsh; the vsh probe must raise no requester without UP-Term:.
Also: the full vtcon `make amiga`. Status: 0 of 15, paused at the owner's request.

## Critical references
- Ledger: thoughts/shared/plans/2026-10-07-amiga-pi.progress.md, section "Phase D parity
  follow-up" (checklist Q1-Q12, G1-G3, pi facts already read, the open owner questions).
- pi at f10993bc: packages/coding-agent/src/core/tools/{edit-diff,edit,grep,find,read,ls,truncate}.ts,
  packages/coding-agent/test/tools.test.ts (edit fuzzy 1108-1333, CRLF/BOM 1335-1425, grep 876-927,
  find 929-982, read 80-258). Fetch from raw.githubusercontent with a python script (the shell's
  sandbox refuses command lines that contain "git").
- Phase D code: amiga-pi/tools_pi.c, truncate.c, ext_gate.c; vtcon claude/tools.c (pi_rules),
  claude/shells.c, claude/edit_orig.c, sources.mk CLAUDE_PI.
- vsh lookup: config/upassign.h upassign_vsh (pr_WindowPtr -1 around the Locks); claude/sys_amiga.c
  ~line 135 (the `vsh <script>` line), ~437 (info "vsh").

## Recent changes (branch amiga-pi-parity, WIP 7d4c5c3, not merged)
- tools/gen_unorm.py -> claude/unorm_tab.h (Unicode 16.0 via Python unicodedata).
- claude/unorm.{c,h}: un_nfkc. Compiles under -std=c89 -pedantic -Werror; untested; not in sources.mk.

## Learnings
- Oracles on the host: node 24.10 (Unicode 16.0, can run pi's own JS), Python unicodedata 16.0.0,
  ripgrep 14.1.1, git check-ignore. fd is not installed.
- pi's countOccurrences always counts in the fuzzy-normalised text, even for an exact match.
- Only pi's bash writes a temp file; read gives offset hints; grep/find/ls cut by bytes only, with
  notices joined by ". " inside one [...].
- fd's and rg's output order is unspecified (parallel walk): "pi's order" can only be documented.
- Rig 3 has no UP-Term: assign and rig.py install does not add one; `List UP-Term:...` through
  amiagent hangs on the insert-volume requester (amiagent's command, not amiga-pi's code).
- The rig --os clean images are 12 MB, the likely cause of the Installer's disk-full (relayed to
  main; rig.py belongs to the Installer agent).

## Artifacts
- vtcon bbc604f (Phase D, on main), amiga-pi aca7948, this branch's 7d4c5c3.

## Next steps (in order)
1. Q1: tests/test_unorm.py against unicodedata (every code point + random combining/Hangul
   sequences); add claude/unorm.c to CLAUDE_PI.
2. Q2: port pi's edit (BOM, CRLF, fuzzy, preserved lines) into claude/edit_orig.c; port pi's cases.
3. Q3-Q5: claude/ignore.c (gitignore, checked against git check-ignore), then grep and find in pi's
   formats under pi_rules (search.c), differential against rg.
4. Q6-Q7: read and ls notices as pi.
5. Q8: the gate's project rules, stored per project in the data directory.
6. Q9-Q11 on rig 3 --max: get UP-Term: there via the Install/boot hook (not by hand), a command
   only vsh answers (e.g. `echo $((1+2))`), compare `vsh -c` with `vsh <script>`.
7. Q12 + gates, then rebase and merge.

## Other notes
- Owner decisions open: image resizing / BMP->PNG in read (needs codecs); keep Latin-1 in edit
  (recommended).
- The worktree's vendor/ is a symlink to the main tree's; remove it before `git worktree remove`.
