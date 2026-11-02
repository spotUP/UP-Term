# Orphan branches triage (2026-11-02, read-only)

Ledger row: thoughts/spot/todos.md:3701 (ORPHAN-BRANCHES). Conflict counts from `git merge-tree --write-tree --name-only main <b>`.

## rig/mute-option (f1041fd2): SUPERSEDED
- Branch adds `MUTE = "--mute" in sys.argv or RIG_MUTE=1` and `volume = %d` (0 or 100) in the FS-UAE config.
- Main replaced it: rig is silent by default, `--sound` gives volume 100, flag file ~/.config/upterm/rig-mute wins (tools/rig/rig.py:11, 549-563, 1275-1276). Opposite default (branch: audible unless --mute; owner decision 2026-10-26 reversed).
- Merge-tree: 1 conflict (tools/rig/rig.py). Delete.

## spike/upkg-wb-ui-r0 (16ba2882): DELETE-CANDIDATE (owner decision, spike)
- Only tests/amiga/ixwbpipe.c (278 lines) + 5 Makefile lines; not on main. Merge-tree: 0 conflicts.
- Ledger todos.md:1898 (R0 spike), :1933 (keep until UPKG-WB-UI decides whether the probe stays as a test), :1970 (R0 ticked 1 of 10; implementation continues on ports branch upkg/wb-ui). Spike result is the doc; the probe is optional. Delete unless owner wants the probe kept as a test.

## vsh/archive-complete (03b1a3e5, b8a24a48): SUPERSEDED
- todos.md:1943: landed on main as 5692511e (squash), branch to be deleted. Main dist/vshrc:129-156 has _archive_match/_archive_complete/`complete -F _archive_complete`; tests/test_vshrc_complete.py exists.
- Merge-tree: 1 conflict (dist/README.txt), only because it is already in main in another form. Delete.

## feature/amissl-bundle (9 commits ca488088..a90d01ba): SUPERSEDED (by main design), keep nothing but possibly two rig fixes
- Main implements AmiSSL differently: dist/common/amissl.installer + P_AMISSL in dist/Install.installer:57-74, dist/parts.txt:17 (Files/AmiSSL), tests/test_install_dos_amissl.py (Makefile:224), tools/rig/installer_rig.py has amissl cases (:213, :369-401). Main commit 20e38141 (last page says when AmiSSL did not install). The branch's dist/amissl/ drawer and Run-Install-AmiSSL do not exist on main.
- Ledger: todos.md:381 "AmiSSL kit-finish (paused)" is stale; CURL-HTTPS-TLS rows (2724, 2850) done: curl+AmiSSL merged in ports PR #16; handoff 2026-10-12_px-n-merged.md:63 "a90d01b kept, on no remote".
- Two generic commits look separable: a3d86f0c (UPTERM_ROOT in a git worktree; main now has `paths.py main-checkout`, Makefile:22: superseded) and 414fca10 (rig.py writes config every start, refuses empty hard_drive_0; main rig.py:1261 builds hard_drive_0, partial check only: verify before delete).
- Branch commit cd7b4dbc was made with hooks off (handoff 2026-10-11_late-budget-stop.md:101).
- Merge-tree: 10 conflicted files. Do not merge. Delete (local only, no remote) after confirming 414fca10 guard on main.

## feature/demo-perf-nas-skill (7b15bf9c): SUPERSEDED
- Branch rewrites tools/nas/skills/amiga-upterm/SKILL.md (wasabi tools, which Amiga, ask-first). Main already has these sections (SKILL.md:3 description, :10 wasabi_mcp.py, :21-51 session daemon, secrets, ask-first), via fb316233 and e338b72b (NL17), 98f75949.
- Merge-tree: 1 conflict (SKILL.md). Delete.

## feature/cc1-cc2 (4 commits, remote exists): ABANDONED by measurement, keep branch for CC2 retry or DELETE after owner call
- Not on main: render/chips.c/.h, tests/test_chips.c, tools/rig/cc_rig.py absent. Hunks 644 lines in render/amiga_render.c; main has had ~62 commits to that file since.
- Plan 2026-09-28-vtcon.md:1295-1300: "CC1/CC2 NOT MERGED": conbench 49.56 s main vs 50.94 s with branch (slower). Next step for CC2 (no frame-clock watch) never done. Plan :689-692 CC1/CC2 and todos.md:676 RT1 still list them unticked. Handoff 2026-10-16_finish-run-session-5.md:35 notes cc_rig.py only on this branch.
- Merge-tree: 5 conflicted files (Makefile, handler/vtcon_handler.c, render/amiga_render.c, render/vtwin.c, tests/test_painter.c).
- Verdict: not mergeable as-is (slower, large conflicts). Owner decision: keep remote as archive, or delete both. Option: salvage only tools/rig/cc_rig.py (87 lines) and pure render/chips.c decisions.

## feature/r1-rom (2 commits, remote exists): STILL WANTED? Paused, unfinished (owner decision)
- Not on main: device/uprom.c, uprom_tag.s, upc_switch.c/.h, tools/mkrom.py, research 2026-10-04_upterm-in-rom.md absent.
- Plan :231 R1 open; :1379 W34 "boot it on the rig then merge"; :1599 rig 2026-10-05: stub RomTag runs, but "InternalLoadSeg failed" (ABI wrong on 40.63), needs register probe. Handoff 2026-10-16_finish-run-session-5.md:36 (FP3.8: another session's). Handoff 2026-10-17_finish-run-session-8.md:24: owner removed ROM module from machine; only in `make rom` custom Kickstart.
- Merge-tree: 3 conflicted files (Makefile, RULES.md, device/upconsole.c) plus plan file auto-merged.
- Verdict: not mergeable (image does not boot). Contains the only R1 research doc and mkrom.py: keep branch (and remote) until the owner drops R1, else DELETE-CANDIDATE.
