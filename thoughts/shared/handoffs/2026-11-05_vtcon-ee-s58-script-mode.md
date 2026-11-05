---
date: 2026-11-05
session: vtcon-ee (s58)
topic: vsh AmigaDOS script mode S1-S5, boot EndCLI fix, rig 96 waiting
---

# vtcon-ee s58 handoff

## Done
- RIG-IPREFS-SYSREQ: boot shell ends with `EndCLI >NIL:` after bootcap on both boot paths (c9b31361), test in tests/test_rig_boot_console_blind.py. 5-boot series clean.
- VSH-AMIGA-SCRIPT-MODE (design thoughts/shared/research/s58-evidence/vsh-script-mode-design.md, owner D1/D2/D3 decided):
  - S1 lexer + `execute` builtin (950912b8)
  - S2 Quit, FailAt, Set/Unset/Get locals (650b3146)
  - S3 If/Else/EndIf, $RC/$? expand to last rc (30d10f12)
  - S4 Skip/Skip BACK/Lab/EndSkip (f870aeb7)
  - S5 .key mini ReadArgs, dot directives, <key$def>, <$$> (52a1da8a)
  - Host `make test ONLY=sh_exec` 651/0, `make build/amiga/vsh` rc 0 (S5 merged-main run: see push state below).
  - No rig check of any step yet. Every "unverified vs C:Execute" choice is listed in todos.md line ~4283 (SCRIPT-MODE row); S7 checks them.

## Open (in order)
1. Push state: main had S4+design+S5 merges local; push after the sh_exec + Amiga build run on merged main is green (check `git status -sb`, then `git ls-remote origin refs/heads/main`).
2. S5b backticks (D2), own host test + R-check vs real Shell. One sonnet agent; brief smaller than S5 (S5 took ~78 calls): split parse vs run if needed.
3. S6 S-bit sentinel (D3: S-bit script runs inside vsh).
4. S7 rig checks R1-R20 + vshexec_rig case 7, one shared rig run, compare every unverified choice against real C:Execute.
5. Rig 96 rebuild (owner eye-check, gates R10/RL18): only after vtcon-c1's freeze fix ceebb596 (S57-FREEZE-2IN9) is on main. fresh-all `--os 32 --max --idle-minutes 0 --keep`, Assign check, 5 boots, then owner steps.
6. Later: rig batches R-A..R-E, drag attempt 5, Q18 L-C1.

## Gotchas
- Agent worktrees start at origin/main, not local main: brief says `git merge --ff-only <local main hash>` first.
- `grep -c '[m]ake test'` wait loops self-match any shell whose command line contains "make test" (my own loop did). Check runners by hand or match `/make test` with pid exclusion.
- Never touch rigs 1, 66, 68, 70, 91, 121-123, 127, 128, 133-136, 141-146. Leave s31-evidence/installer-check10.md and test-register.md uncommitted changes alone (not mine).
- Branch s58-guru-survives: delete needs owner OK.
