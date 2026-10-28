---
date: 2026-10-28
session: Amarchy s15 (peers know it as vtcon-32)
topic: UHC demo video for Patrik - rig and shot list ready
status: in progress (owner records)
---

# Handoff: UHC demo for Patrik

Ledger row: thoughts/spot/todos.md, UHC-DEMO-PATRIK (grep the id).
Shot list: thoughts/spot/uhc-demo-shotlist.md (status READY).

## Done
- pg8-text2 merged into main; main pushed at 2daab3ba; worktree and branch removed.
- make test green on main. Stale tests fixed at the root:
  - b4be8100 test_dist_slim asserts the recipe-generated git rows (8a21a1fa).
  - 770f5202 test_qmark_commands classifies scrprobe as a rig probe.
  - 5f203c1a test_tutorial_lessons kills the lesson's tmux server in a cleanup.
  - 2daab3ba bashdiff: the bare no-command drawer case is now probe
    builtins/autocd_bare_drawer, a documented divergence (autocd from d7b76e11).
- make test on this machine needs UPTERM_PORTS=<ports main checkout>, because
  ../upterm-ports sits on another session's branch (port/htop).
  Used: /Users/spot/Code/UP-Term/upterm-ports/.claude/worktrees/kit-main (detached 62dbb86).
- Kit rebuilt from main (make kit-stage in kit-main, then UPTERM_PORTS=<kit-main> make dist-base),
  build/up-term.lha 28 Oct 12:59.
- Dry run of the whole flow on rig 7: PASS (install up-uhc, Browse, install/run/Remove pipe).

## Open
- Launch row and live theme change: sonnet agent check on rig 7 (see ledger row for the verdict).
- Fresh rig 7 for the owner's recording:
  UPTERM_RIG=7 python3 tools/rig/rig.py start --max --purpose "Amarchy s15: UHC demo recording (owner)"
- Product gap: UHC:ExtBin reaches the path only after a reboot.
- Not in the take: https UHC repos (CURL-HTTPS-TLS).
- No A1200 run of this flow on record.

## State at 2026-10-28 afternoon (s15, before fresh session)
- main 8cd93f0e pushed (8cd93f0e: parts.txt claims Files/amarchy-themes; dist-base passed, lha has 23 themes + JetBrainsMono.ttf; kit-main released to vtcon-05): 5ce5b6fd AA JetBrains Mono menu, e88b020e Amarchy themes in kit (revert-checked), 7e03386a up-uhc MakeDir + Path UHC:ExtBin in install shell, 3bc1fb3e themes test in make test (installer group passed).
- All s15 agent worktrees merged and removed.
- Kit rebuilt OK (kit-stage at b8f4175, no 0059). Sonnet rig 7 check agent running; findings: s15 scratchpad agent-rig7check.md.
- Then: UPTERM_PORTS=<kit-main> nice -n 19 make dist-base; check lha has Files/amarchy-themes/*.theme (23) and Files/fonts/JetBrainsMono.ttf.
- Then sonnet rig 7 check, purpose "Amarchy s15: AA font + themes + UHC path check": AMARCHY_AA_GLYPHS > 0 (gui.c sentinel), haiku shot of antialiased menu (copy to ~/Desktop/upterm-shots), Style > Amarchy theme lists many themes, Pipe by name in install shell and whether a new shell finds it, `upkg remove uhc:pipe`. Agent stops rig 7 at end.
- Then fresh rig 7 for owner: UPTERM_RIG=7 python3 tools/rig/rig.py start --max --purpose "Amarchy s15: UHC demo recording (owner)"; read [WARN] drift line; update shot list step 4 (theme) if themes listed; message vtcon-f6 verdict.
- Ledger rows: UHC-DEMO-BLOCKERS, UPKG-REMOVE-BARE-UHC (vtcon-05 owns), UHC-EXTBIN-LIVE-PATH (needs design), AMARCHY-AUTOTILE (next big track), AMARCHY-PLUGINS, OMARCHY-AI.

## State at 2026-10-28 end of s15
- Rig 7 check (agent, findings in the s15 scratchpad agent-rig7check.md):
  - Passed: AA menu font (AMARCHY_AA_GLYPHS 69), 23 Amarchy themes listed, `upkg remove uhc:pipe`.
  - Failed only on a stale up-uhc ipk (27 Oct, before 7e03386a). The half-height bottom menu row is the designed scroll cue, not a clip.
- Rebuilt up-uhc ipk at 13:45 (`make dist-uhc dist-ipk`). ExtBin on the install shell path is still untested on a rig with this package.
- Rig 7 is running for the owner's recording. Nothing is installed through upkg.
  - Feed: http://127.0.0.1:18907, python http.server pid 37928 serving build/ipk.
  - The rig's UP-Term:etc/upkg.conf src/gz line points at the feed. The original is UP-Term:etc/upkg.conf.orig.
- After the take:
  1. Restore the .orig file and kill 37928.
  2. Message vtcon-f6; it then moves kit-main to upterm-ports main (0059 merged, 75989e1) and checks bare `upkg remove pipe`.
- Shot list updated: theme step back (picking one untested), scroll-cue note, feed and restore steps.
