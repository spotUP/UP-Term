---
date: 2026-10-13
topic: Grouping ~/Code into per-project umbrellas, and stale worktrees
tags: [layout, repos, worktrees]
status: final
---

# ~/Code grouping proposal (outside UP-Term)

Inventory 2026-10-13: 72 top-level dirs. Classified from git remote, last commit date and
top-level files. "?" = membership guessed, not confirmed.

| Umbrella | Members |
|---|---|
| `UP-Term/` (plan 2026-10-13-source-umbrella.md) | + `reference/`: creep-AmigaTools (ccon, conbench: S2 research), wasabi-eval, rlaunch-eval (both 2026-10-03 rig evaluations, cited in vtcon thoughts) |
| `UpRough/` | Up_Rough_Demo_System, uprough-ec41, -hdfs7, -psub, -s3, -sizing-stamp-verify, -triphop, -uade, urds-prof, urds-tmcrash (all worktrees of up-rough-demo-system), uprough-backups, uprough-releases-server, makerelease (UP-* release archives) |
| `DCTelnet/` | dctelnet-petscii-recovered (main repo), dctelnet-ibmfont, dctelnet-v2, dctelnet-vtcon (worktrees), DCTelnet-PETSCII (no git) |
| `AmiExpress/` | amiexpress-web, amiexpress-doorserver, "amiexpress-typescript-doors " (name ends in a space: rename) |
| `DEViLBOX/` | DEViLBOX, DEViLBOX-backup-2026-10-04.git, ft2-clone (reference) |
| `Emulators/` | amiberry, vAmigaWeb, EmulatorJS, emulatorjs-build, emulatorjs-retroarch, libretro-uae, libretro-uae-ejs, mega-bezel-reflection-shader-web |
| `Web/` | asciiarena, pong-websocket-server, space-blazers, house-rental, hippoplayer.se, retroranks, bratwurst, cygnus-edd-develop, deploypulse-web-hosting |
| `Jobb/` (exists) | + font-ligatures (process_guardtools.py) |
| `Reference/` | "Reference Code", "Reference Docs", amigadeveloperdocs, ArduinoFloppyDiskReader-master, dpaintiii |
| stays alone (owner: own projects) | CGTerm-2.0, demomaniacdev, LVLLVL2ANSI, kling, icon-font-builder; EALLc64bbsads moved by the owner |
| delete | "…" (empty, 0 B), logs (two stray UP-Term-era logs) |

## Move constraints (every umbrella)
- No session, agent or dev server running in the dir.
- Worktrees store absolute paths: move the main repo and its worktrees together, then
  `git worktree repair` from the main repo.
- Claude Code memory is keyed on the cwd (23 project dirs in ~/.claude/projects): copy each
  moved project's memory dir to its new key.
- Hardcoded `/Users/spot/Code/<dir>` paths in each repo: count and fix per repo before the move.

## Stale worktrees and branches (2026-10-13, base = origin default branch)
- vtcon: vtcon-demo-perf (S2, merged, 1 dirty file); wt-m2 (merged now, removable);
  wt-bebbo (K6, keep); wt-k7, wt-y1 (agents running). 7 unmerged local, 3 unmerged remote branches.
- amiga-pi: wt-t2 (merged now, removable).
- ixemul-vtcon: ixemul-vtcon-y1 (Y1 agent). 6 unmerged local, 5 unmerged remote branches.
- up-rough-demo-system: 12 worktrees; merged-but-dirty: hdfs7 (16), s3 (25),
  sizing-stamp-verify (12), uade (55), urds-prof (16); unmerged: ec41, psub, triphop (clean),
  urds-tmcrash (3 dirty), .claude/worktrees/dubgroove (344 dirty), modulation-stage1 (140 dirty),
  agent-a950f41797dba7fb4 (clean). 19 unmerged local, 25 unmerged remote, 2+5 merged branches.
- dctelnet: 3 worktrees, all unmerged into origin/feature/petscii-emulation (its default);
  25 unmerged local, 38 unmerged remote branches.
- DEViLBOX: 42 merged remote branches (deletable), 12 unmerged remote, 7 unmerged local.
- amiexpress-web: 16 merged local + 5 merged remote (deletable), 26 unmerged local, 13 unmerged remote.
Dirty or unmerged = possibly the only copy of work: owner decides per item, nothing removed blind.

## Owner decisions 2026-10-13
- Grouping approved with the corrections above.
- Cleanup: remove clean+merged worktrees and merged branches (local+remote) in all repos; list dirty/unmerged per repo for the owner.
- Owner: cygnus-edd-develop and dpaintiii belong with hippoplayer.se: all three in `HippoPlayer/` (moved 2026-10-13). urds-* are worktrees of Up_Rough_Demo_System (UpRough/).
- Owner: ft2-clone also in `HippoPlayer/` (not DEViLBOX).
