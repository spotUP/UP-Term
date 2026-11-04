# Rig 96 eye-check on AmigaOS 3.2, run 2 (s54)

Rig build: worktree .claude/worktrees/r96-s54 at a13b01e8 (origin/main had moved; run with --allow-stale-base, 4 unrelated commits skipped: UPDemo icat scene UD4, AGENT-TASK-DASH, TAB-FILLPEN, a merge).
Install: fresh-all [PASS], reboot back in 20 s.
Version L:vtcon-handler -> "vtcon-handler 1.0" (expected 47.x: MISMATCH, flagged). Version -> Kickstart 47.115, Workbench 47.2.

| Row | Verdict | Evidence |
|---|---|---|
| a S31-START-PALETTE | PASS | backdrop 0E0E14, frames 7AA2F7 active, no EE9900/6688BB/000000/FFFFFF |
| b window knob colour | NOT RUN | tool budget used up |
| c FLAT-SCROLLER | NOT RUN | tool budget |
| d AMARCHY-WB-BACKDROP / RL3 | NOT RUN | tool budget |
| e BAR-STRIP-RESERVE | NOT RUN | tool budget |
| f FLAT-BUTTON-FILL | NOT RUN | tool budget |
| g FLAT-PENS-NOT-LIVE | NOT RUN | tool budget |
| h R1.17-WBRESET | NOT RUN | tool budget |
| i AM-FLAT-WB | NOT RUN | tool budget |
| j strut_prebar | FAIL | clamp counter rose, but harness found no window at left 20 with top <= 246, before and after BAR OFF (run twice). Product vs harness filter (r_strut_prebar in amarchy_bar_rig.py) undetermined |
| k UPDemo scenes | PARTIAL | the scene that ran shows the status "Demo 19/32 Kitty graphics: place, move, delete and relate placements": ovals drawn, wider than tall, no tall circle (shot r96r2-updemo-18.png). Scene numbering: UPDemo 18 shows Demo 19/32 (the argument is 0-based vs the status 1-based; check). Scenes 15 (animation, globe over photo) and 20 (Nerd Font glyphs) NOT VERIFIED: the later shots were stale (typed text queued at the shell prompt, then wasabi connection refused) |
| optional btnbg --prefs-magenta, gui --welcome | NOT RUN | tool budget |

Rig state at the end: wasabi connection refused, no fs-uae process: rig 96 is DOWN (probably the 15-idle-minute watchdog; not stopped by this agent).
Shots: ~/Desktop/upterm-shots/r96r2-*.png (r96r2-updemo-15.png, -20.png, -15-b, -20-b are not valid evidence).
