---
date: 2026-10-26
topic: vtcon-05 session 5. Creep contributor packages landed (ports PR 1, vtcon 1dbcfd12); upkg_wb_rig on main (ade12f33, C3 PASS); wb-ui upkg merge gated on the kit ixemul update
branch: main (ade12f33)
---

# Handover: vtcon-05 session 5 (UP-Term side)

Ledger: `thoughts/spot/todos.md`, section "## 2026-10-26 (vtcon-05 session 5) plan" and the rows appended after it. Trust the ledger over this summary.

## Landed
- Ports PR 1 merged (25bc8cd7): creep feed fields, CI fixes, contributor docs, rig-check-after-merge decision.
- vtcon main 1dbcfd12: pkg_rows (D1-D5). Remote creep branches and their worktrees deleted.
- vtcon main ade12f33: tools/rig/upkg_wb_rig.py plus its RULES row. It now requires a kit install, keeps the base status records and restores the kit upkg.conf. C3 PASS on rig 8 (steps 1-5, 9 rows, up-bitchx installed from the window).

## Findings (term-findings/, all 2026-10-26, rig 8)
- upkg-exit-reset-isolate: the wb-ui upkg window close resets the Amiga with the kit ixemul (f8c21ce3) and not with the fixed one (86fd4374). Stack and wbrun ruled out.
- reboot-loop-0037: any reset on the kit rig (C:Reboot too) loops forever, with either library. Separate open problem: rig/fs-uae config or kit boot state. tools/rig/rig.py in the main checkout carries up-term-00's diff; do not edit it there.
- 0037 (upkg with no TMP:) PASS at 800x600; 640 px untested.

## Open, in order
1. OWNER: the owner of the dirty ixemul main checkout updates feature/ixemul-80 to origin 9f758ea and rebuilds build295 (owner decision 2026-10-26). Check: md5 of build295 ixemul.library is 86fd4374 or a later build with both fixes.
2. Then `make dist`, and rebase plus merge ports `upkg/wb-ui` (6c9a57d, was 5 behind ports main) into ports main. Until then the icon starts the old upkg.
3. The kit-rig reset loop: hunt in a worktree, one sonnet agent, rig 8 only.
4. CP6 rig step: walk it on the first recipe merged after the creep branches (local branches ports creep/walk-figlet c9b0ff6 and vtcon creep/walk-rows 8d76bd63 hold the host walk; delete them once the walk is redone on main).
5. 0037 at 640 px.

## Constraints
- Rig 8 only for this session's slot (UPTERM_RIG=8). Rig 8 is stopped.
- Worktrees only for tools/rig, device/, dist/, ixemul and upterm-ports work.
- Max 2 sonnet agents, briefs in a file in the scratchpad, report once.
