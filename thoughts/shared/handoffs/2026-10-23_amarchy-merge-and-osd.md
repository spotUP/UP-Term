---
date: 2026-10-23
topic: amarchy-a and amarchy-osd merged to main; O7, the Ctrl-\\ bug and the A1200 checks open
branch: main (f14afcab)
---

# Handoff: amarchy-a merge and the AM-OSD quick menu

Night-run ledger (untracked): thoughts/spot/night-run.md. Plan: thoughts/shared/plans/2026-10-23-amarchy-osd.md.

## State

- Handler rasterizer R6: fix 958f7f84, hires rig 5/5, plan ticks a2b1abf5. A1200 checks open (owner).
- amarchy-a merged into main as e75adab3, pushed; make test passes. The merge updated stale tests
  (bebbossh dist-base regex, tutorial lesson 14, lesson rx stub, wiring regexes for menu_do and the
  /state font fields) and added the `?` helper to Amarchy and amarchy-menu (test_qmark_commands).
  amarchy-a branch and worktree deleted.
- amarchy-osd (vtcon): O1 392f6092, O2 ee88b76a, O4 75011583, O5 41559439, O6 89b7f86c + 344d45ff.
  344d45ff: close_gadget hangs up the line discipline (ld_hangup) for termios-mode windows, so a raw
  curses reader sees EOF on CLOSEINACTIVE. tools/rig/amarchy_osd_rig.py passes 13/13 on rig 3.
- upterm-ports amarchy-osd: O3 d6f758b (libupkgui OSD style). Sysroot lib must be the O3 build
  (sha 8e6a7469, copy in the night session's scratchpad o3/); a main upkg-ui rebuild overwrites it.

## Update 2026-10-23 05:05

- amarchy-osd merged into main as f14afcab and pushed; make test passes, the handler and amarchy-menu build.
- The merge put Amarchy 1200 B over its 32 KB budget. amarchy/menu_tree.c (node-path code, mn_osd_size no
  longer recursive) and handler/upmru.c (the MRU functions upactive.c needs) split out; Amarchy is now
  28288 B resident.
- O3b passed (0 px differ outside the masked clock corner). upterm-ports main fast-forwarded to d6f758b, so a
  main upkg-ui rebuild now builds the OSD lib.
- The OSD rig script closes its windows (4a7c6c83). Branches and worktrees for amarchy-osd and osd-o3 are gone.

## Open, in order

1. Ctrl-\ does not end a cooked Copy waiting in a read. The cooked_key/service_reads guess is ruled
   out (both callers call service_reads). Needs a rig repro first, then the fix and a regression test.
2. O7 and the A1200 checks: owner.

## Owner questions (ask through AskUserQuestion)

- A1200 R6 check: Nerd Font icons two cells wide and full size; powerline separators fill the cell on
  hires and RTG; R6.2b prompt columns line up.
- NOBORDER drops the gadgets now; should it also drop the scroll bar? Recommended: yes.
- OSD filter line shows the prompt only. Add type-to-filter?

## Gotchas

- In worktrees, make needs UPTERM_ROOT=/Users/spot/Code/UP-Term IXEMUL_SRC=/Users/spot/Code/UP-Term/ixemul-vtcon
  (the Makefile defaults resolve inside .claude/worktrees; worth fixing).
- After rig.py install, stop and start the rig so the new handler is resident.
- The PreToolUse load hook refuses test runs while any rig's fs-uae is busy, including your own agent's.
