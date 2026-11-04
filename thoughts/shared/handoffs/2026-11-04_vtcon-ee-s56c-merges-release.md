---
date: 2026-11-04
session: vtcon-ee
topic: s56c branch merges, release status, rig 111 retired
status: open
---

# vtcon-ee s56c: merges, release status

## Done
- main dabb4317 (pushed): merged s51-muigroup (MUI group-button fixture + Makefile target) and worktree-agent-a57f03ea (Installer recheck evidence).
- Owner merge decisions are recorded in the todos.md row POST-RELEASE MERGES: r1-rom lands after the 3.2 release, rebased; s48-amiberry-default is held until the Amiberry assign hang is fixed; WIP and backup branches are kept; s56-deskrig is left to its owner session.
- Rig 111: rebuilt from 31ca8e69 for an install check (33/33 install checks passed, md5 of handler, UPDemo and earth.anim match dist), then stopped and cleared.
- UD2 globe band: BACKBURNER (owner: still glitching after 230f94bf).

## Open, in order
1. Rig 96 owner eye check (knobs, Locale, UPDemo 15) once vtcon-16 has rebuilt rig 96 from current main. Start the rig before asking the owner.
2. OPEN-RIG 4072, 4073 and 4125: one fresh install of everything on 3.2 from main, with pixel reads (release-ledger-audit.md, section "Release path").
3. KIT-AMARCHY-ALL READY from vtcon-16 (RL15, RL16, RL8), then 1088 steps 4-6: re-pin repos.lock, kit build + dist checks, upload (upload needs the owner's OK).
4. todos.md row INSTALLER-RIG-REBOOT: the automatic reboot in installer_rig.py fails, and its restart has no --purpose.
5. Triage the open ledger rows outside the audit (RL3, RL17-RL21, R10, R11): which of them block the release?

## Gotchas
- Worktrees vtcon-s51-muigroup and .claude/worktrees/agent-a57f03ea still hold the merged branches. They belong to other sessions or agents: ask before removing them.
- Commit 31ca8e69 carries a Co-Authored-By line (pushed, left as is). Never add attribution lines.
