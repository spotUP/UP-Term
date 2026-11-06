# S7 rig run: vsh script mode vs 3.2 Shell (2026-11-06)

Rig 98, OS 3.2, --max, main bbdb399f; harness tools/rig/vshexec_rig.py case 7 (commit 4727bcb4, branch worktree-agent-ab64437c5e85e3dcd).
Each item: `Execute RAM:rc/x` in the Shell vs `VTC:vsh -c "Execute RAM:rc/x"`; the wrapper line `VTC:vsh failed returncode` removed.

| item | Shell 3.2 | vsh | verdict |
|---|---|---|---|
| R23 | `[l1 l2 l3]` | `[]` from a script file; typed `vsh -c "Echo [`Type f`]"` gives `[l1 l2 l3]`; `[`Echo hi`]` and `[`List ..`]` in a script are fine | DIFFERS: multi-line capture lost only inside a script file |
| R6 | `<NIL:` rc 0 empty; `<reply` file: `foo: Unknown command` rc 10 | `execute: Bad arguments` rc 20, no template printed, no reply read (both stdin forms) | DIFFERS |
| R27a `Run C:Wait 1` | `[CLI 3]` | `[CLI 7]` | MATCH (banner; number is the slot) |
| R27b `Run <S-bit script>` | `[CLI 8]` | `s27 ran` (in-process, no banner) | DIFFERS |
| S7b-2 `CD RAM:` and `cd RAM:`, then `Type marker` | `inram` | `inram` both | MATCH |
| X2 bare `CD` after nested CD | `RAM Disk:rc` | empty (Unix cd prints nothing) | DIFFERS (probably by design) |
| R2 | `a` / `<x>` | `Echo: unable to open redirection file` rc 10 (quoted "<x>" taken as redirect) | DIFFERS (regression) |
| R17 | `inner` / `back` | extra first line `Quit failed returncode 10` | DIFFERS |
| R15 | `rc=0` | `rc=1` | DIFFERS |
| R24, R3, R9, R13, R14, R16, R19, R11 | | identical text and rc | MATCH |
| R1 R4a R4b R5 R7 R8 R10 R12 R18 X1 X1b X3 R21 R22 | | identical | MATCH |
| R26 / R26b | not valid: S bit not set in this run (harness gap) | | recheck |

DefIcons: windows and requesters of all screens listed after every item R1..R26b. Only the Workbench window existed; no requester.
"DefIcons: Cannot load preferences because" NOT reproduced on bare 3.2 (matches rig 99).
