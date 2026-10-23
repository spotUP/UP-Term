---
date: 2026-10-23
topic: amarchy-a merged to main; AM-OSD O1-O6 done on amarchy-osd; O3b, O7 and A1200 checks open
branch: main (e75adab3), amarchy-osd (worktree .claude/worktrees/agent-a8d806b793688db1e, 344d45ff)
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

## Open, in order

1. O3b: upkg browse pixel compare before/after the O3 lib (an agent was running it at handoff time;
   see the ledger for its verdict).
2. Merge amarchy-osd into main. Expect a conflict in amarchy/menu.c: main added the qmark_argv
   block at the top of main(). Then make test, push.
3. amarchy_osd_rig.py leaves its "osdhost"/"osdother" windows open; close them at the end.
4. Ctrl-\ does not end a cooked Copy waiting in a read. The cooked_key/service_reads guess is ruled
   out (both callers call service_reads). Needs a rig repro.
5. O7 and the A1200 checks: owner.

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
