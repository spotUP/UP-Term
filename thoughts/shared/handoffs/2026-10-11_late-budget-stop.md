---
date: 2026-10-11
topic: Budget stop - state of amiga-pi phases, kit, upupdate, amiagent 0.13.2
tags: [handoff, amiga-pi, amissl, kit, budget]
status: final
---

# Start here (2026-10-11 late, budget stop)

25% of the weekly budget went in one day (5 parallel agents, 3 on Opus, plus a long
Opus main session). Rule from now: at most 2 agents, a cost line before any extra,
Opus only for judgement (design, audits, root cause), Sonnet for implementation.

## Merged to local main (nothing pushed)

- vtcon main: amiga-pi Phase E (merge d6e323f), upupdate stack fix (32aeaa8) and its
  audit follow-ups (sanitizer-free stack driver tests/updiff_stack_drv.c bounded at 3072,
  `long __stack = 16384` in install/upupdate.c, 80000004 = illegal instruction comments),
  rig.py --stock finds amiagent's NP_StackSize by tag (554163a).
- amiga-pi main: Phase E (2dd72b1). Phase D parity 15/15 was already on main.

## Branches (WIP, not merged) - each has a pause note in its ledger

| Work | Branch / worktree | State |
|---|---|---|
| Phase F model switch | amiga-pi `phase-e` (build/wt-phase-e), vtcon `feature/amiga-pi-phase-e` | stopped; also owns E7's thinking-level restore (audit finding 3) |
| Phase G instructions/trust | amiga-pi `phase-g` (build/wt-phase-g), vtcon `feature/amiga-pi-phase-g` | stopped early |
| Phase K MCP | amiga-pi `phase-k` (build/wt-phase-k), vtcon `feature/amiga-pi-phase-k` | finishing (6 of 8 at 23:11) |
| Phase E audit fixes | amiga-pi `e-audit-fixes` (build/wt-e-audit) | finishing: (1) list reads only - done; (2) compaction trigger point as pi |
| Kit + AmiSSL | vtcon `feature/amissl-bundle` (.claude/worktrees/amissl) | stopped; upupdate fix cherry-picked; LIBS:AmiSSL probe reworked |

Read the final reports of K and the audit fixes in the session that ran them, or their
ledger sections. Each phase gets an Opus audit before merge (Phase E audit found 3 medium
bugs that the Sonnet ledger had ticked).

## Open, owner

- E11: on the Amiga `amiga-pi -p hello`, copy the session file from
  `UP-Term:var/amiga-pi/sessions/<dir>/` to the Mac, `pi --session <file>`. PASS: pi shows
  the conversation.

## Open, queued

- amiagent 0.13.2 installed in main build/rig, rig2, rig3 (old kept as amiagent-0.13.0);
  worktree rig copies still 0.13.0. 0.13.2 runs commands with pr_WindowPtr -1, so rig
  checks for "no requester" (install_rig.py:430, installer_rig.py:311) pass vacuously.
  Needed before rig 1 moves to 0.13.2: a tiny tests/amiga tool running a command with
  pr_WindowPtr 0, used by those checks.
- Phase H (print/JSON/RPC) after F; I/J after G; L, M, N, O later.
- E11 rig run (owner asked 2026-10-12): Sonnet, rig 3, merged-main amiga-pi, step 5a keeping
  the file, then `pi --export <file> out.html` (pi 1.0.4 installed at /opt/homebrew/bin/pi).
- Amiga IDE spike (approved): research/2026-10-12_amiga-ide-brainstorm.md, section Spike.
  After E11, the Phase F audit and the Enter bug.
- BUG (owner 2026-10-12, rig FS-UAE): holding Enter at the shell, the cursor sometimes
  jumps up some rows and carries on from there, as if half the screen was cleared (rows
  below the cursor gone). Not a flash: the output continues from the higher row. Suspects
  to tell apart by a recording of the bytes the console receives (not by reading code):
  the line editor's redraw moving up by a stale row count (ghost text / wrapped prompt
  height, handler/lineedit.c), the console's own scroll/blank-scroll skip or jump scroll
  losing rows (renderer), or a race in the paced flush. First step: reproduce on rig 1
  with a key-repeat driver and log vt_feed input around the jump. Regression test named
  after the symptom.
- Colour at no extra cost (owner 2026-10-12: "ccon draws colors at no extra cost we
  should do that as well"). Done = conbench colour rows (sgr-perchar etc.) cost what the
  plain-text row costs, as in CCON, on the stock A1200 rig (SYNC SCALE 1 REPS 3).
  Starting points: research/2026-10-04_ccon-b9-techniques-vs-ours.md rows 5-7 (exact
  plane mask, per-plane (bits AND A) XOR X, pre-shifted glyphs), plan W27,
  handoffs/2026-10-04_speed-campaign-updemo-wasabi.md Next Steps 1-2 (mask left wide
  after colour; cell_style/pen_for/vt_resolve_colors per cell in draw_rows). Opus design
  (measure first: which part of a colour row costs more), then Sonnet implementation.
  Queued behind the Phase F and K-audit agents (2-agent limit).
  The answer is in CCON 1.3.0 (github.com/creep-ltx/AmigaTools, ccon/; our research
  covered b9/b10 only). Its README: "a C engine inside the E handler and direct drawing
  into the screen (... colour nearly as cheap as plain text)". Commits to read first:
  0e45b7b (b11-b12 leaner text path, rows remember their length), ce4c21b (b15 the
  direct painters), 1ec662d (1.3.0, direct drawing), plus ccon/todo.md and conbench's
  colour-run workloads. Research doc first: what direct drawing does per colour run
  that we do not.
- Kit: remaining rig cases (amissl-present, novice, dest, update on 3.2.2 and 3.1), make
  test, kit to Desktop. Guest dies mid-install ~3 of 8 runs (undiagnosed).
- `make test` in a vtcon worktree needs UPTERM_ROOT=/Users/spot/Code.

## Late reports
- Phase G stopped 0/5: vtcon f715a59 (memory.c candidate list as parameter, pi trust.json; C:Claude suites unchanged, +2,052 bytes), amiga-pi WIP 68ac41f not compiled/tested; pause note vtcon 14d53a1. Next: mkoracle_g.py misses `dirname` import (gen_resources.ts fails).
- Kit paused at a90d01b (feature/amissl-bundle): volume-name abort works on the rig; Installer `exists` sees AmiSSL libs after `Assign LIBS: AmiSSL:Libs ADD` (rig BOOTX:go drops the ADD: rig artifact); fixtures snapshot added. amissl case 3.2.2 fails only check 41 (AmiSSL still there after Uninstall), cleanup hangs on a Volume Request. Not run: other cases, 3.1, make test, tls-check, Desktop kit. RULE BREAK: cd7b4db committed with core.hooksPath=/dev/null; re-run the hooks on it before any merge.
- Phase F paused ~12 of 16: vtcon feature/amiga-pi-phase-e 02b0f4b, 2e01cad, c135ea2 (WIP); amiga-pi phase-e cc0375c, 77b3bff. pi_model 456 checks, vtcon make test rc=0. Works: registry, models.json, credential order, handoff (12 pi-oracle cases), thinking on the wire, /model, /thinking, resume restores thinking level (audit item 3). Bundled model catalog hand-made (pi's generated data not in its repo): audit it. Open: make amiga both trees, rig 2 --step 6 (written, unrun), claude-tls-check, ARCHITECTURE rows.
- Phase E audit fixes MERGED to main in both repos (amiga-pi 05b1862 list reads only, df680da compaction at pi's three points, oracle = npm @earendil-works/pi-coding-agent@1.0.4).
- Phase K DONE 8/8, NOT merged: vtcon feature/amiga-pi-phase-k (.claude/worktrees/phase-k, 7 commits to 51c40be), amiga-pi phase-k (build/wt-phase-k, ad00ed5..03c3a3b). All gates green; rig 3 step 10 PASS over LAN http. K1: only PTY: gives a spawned child line I/O with EOF. amiga-pi 1,030,012 bytes (+51 KB). Defaults chosen (in COMPATIBILITY.md): exposure direct when unnamed (pi's codemode not built; codemode servers via tool_search); .pi/mcp.json only for trusted projects (needs G3); no Authorization header over plain LAN http; BM25 in fixed point. NEXT: one Opus audit of Phase K, then merge.
