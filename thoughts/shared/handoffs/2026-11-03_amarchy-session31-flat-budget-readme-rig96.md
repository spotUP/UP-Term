---
date: 2026-11-03
session: amarchy 31
topic: flat-budget frame fix, start palette and welcome merged, README key list, rig 96 reinstall
status: handoff
---

# Amarchy session 31: state at end

Ledger: thoughts/spot/todos.md (rows S31-FLAT-BUDGET, S31-START-PALETTE, S31-RIG96-REDO,
INSTALLER-FLAT-OFF, RL8, RL18, RL19-RL21). Plan: thoughts/shared/plans/2026-11-02-amarchy-welcome.md.

## Landed on origin/main
- 7e4ad79f flat_glyph.c clone_ground(): `_flat_clone_map` frame 152 -> 80, `make flat-budget` PASS (chain 300/384).
- 6aad428b welcome/help window, cf0d408e start palette (DESK_START_PALETTE plan bit) merged in d11e29ee.
- fad8f020 README key list: keymap_doc.py WORDS 'slash' -> '/'; dist/README.txt regenerated; test_amarchy_docs asserts it.
- b6b27d0b welcome text: one whole sentence per line (owner: 'Press Escape' line lacked its full stop); test_welcome_lines_end_whole_sentences; man page regenerated.
- 34dd32ed rig 96 freeze evidence (thoughts/shared/research/s31-evidence/rig96-freeze.md): cause unproven.
- Suites green on the merge: flat_core, desk, help_core, keymap, menu_text, help_man_sync.

## Open, in order
1. Rig 96 is UP (fs-uae started 00:06 with --keep-sys, purpose "Amarchy session: rig 96 owner eye-check full install").
   It runs the d88e7821 build, NOT b6b27d0b: the new welcome text is not on it. Flat prefs were written by hand
   (INSTALLER-FLAT-OFF), backdrop measures (14,14,20). The owner said the welcome screen looks nice
   (AMARCHY-WELCOME-LAYOUT ticked). Next: reinstall rig 96 from main (brief: block on installer_rig until it exits,
   never end the turn early; stop only with `rig.py stop --keep`), message vtcon-ee start and end, owner looks.
   The idle watchdog (60 min) may stop rig 96 the plain way and delete build/rig96: check what it does first.
2. S31-RIG96-FREEZE: intermittent post-reboot hang. On the next hang, probe before any restart (commands in the ledger row).
   Top suspect RIG-KIT-WASABID-DUP (kit wasabid binds host port 1234 beside BOOTX:wasabid): fix in the rig install path.
3. INSTALLER-RIG-TIMEOUT-VERDICT: installer_rig.py:1077 must write a fail verdict on any exception.
4. INSTALLER-FLAT-OFF: real install leaves the Amarchy prefs with only the header; installer default fix.
5. W13 welcome rig proof (tools/rig/amarchy_welcome_rig.py) on rig 96, then W14 ledger ticks.
6. S31-START-PALETTE follow-up: pointer, font and icontrol parts are not applied at start either.
7. RL8 FLAT-COLOUR-OVERHAUL, RL18 AmiSSL check order, RL19-RL21 SELECTA AA rows (owner: this release).
   Major-feature list given to the owner this session: RL19-21, RL8, RL3, RL16, RL10, RL11, KITTY-TRUECOLOR, then roadmap P1-P4, LX, OMARCHY-AI.

## Cleanup owed
- Worktree agent-a57f03ea92143ba93 has an unmerged branch: owner approval needed to delete.
- Worktree agent-aeb5fae7eaa8a4a2e is locked by pid 1796 (branch already deleted, HEAD merged); remove when unlocked.
- Agent worktree agent-a7cf09fc119d1a42e (no commits): its build is the one on rig 96; keep it until rig 96 is reinstalled
  from main, since any ami.py call from another tree pushes that tree's build onto the rig.
- Agent worktree agent-a5a9345429ae2dd5e: its one commit is cherry-picked as 34dd32ed; remove it and its branch.
