---
date: 2026-11-13
session: s78
topic: v1.1 feed check on a rig, opkg remove-reason rebase, renamed-package upgrade question
status: two background jobs were still running at handoff; collect them first
---

# s78 handoff

## Owner decisions this session
- No A1200 test. The v1.1 feed-upgrade check (UP-Term row = vsh banner + `bin %` prompt; close all terminals, Agents > Launch the agent = one Claude window, no tab bar) runs on a rig instead.

## In flight at handoff (collect first)
1. **DONE before handoff: both PASS** on rig 9 with the live v1.1 up-term.lha (installed with install.dos, not through upkg; the feed has no base package). The installed Patina, patina-gui and patina-menu md5s match the live lha and the s77 build. Evidence is in `thoughts/shared/s78-evidence/feed-upgrade-rig.md`, and the register row is added. Rig 9 is stopped and build/rig9 is removed. `build/rig-saved/9/` (388 files) is left: delete it if disk is tight. Original brief, for the record: **Rig 9 feed agent** (sonnet, purpose "s78 vtcon session: v1.1 feed upgrade check"). Brief: install/upgrade from the LIVE v1.1 feed (or the downloaded live assets served from the Mac), md5 the installed Patina binaries against the live up-term.lha, run both checks with text evidence, write `thoughts/shared/s78-evidence/feed-upgrade-rig.md`, append a test-register row, stop rig 9.
   - Next session: if that evidence file exists with both verdicts, report them to the owner. If not, the agent died with the old session: run `LC_ALL=C ps -Ao pid,pcpu,command | grep fs-uae`. If rig 9 is still up, stop it (`UPTERM_RIG=9 python3 tools/rig/rig.py stop`, confirm by ps). Also stop any `http.server` it left. Then re-brief a new sonnet agent from this section.
   - Rig 2 was running at s78 start and is NOT ours; leave it.
2. **Host opkg build** in worktree `/Users/spot/Code/UP-Term/upterm-ports-opkg-msg` (`nice -n 19 make host-opkg JOBS=2`). Log: `/private/tmp/claude-501/-Users-spot-Code-UP-Term/4407d024-0ac1-4736-a8e8-01c398b47fdc/scratchpad/host-opkg.log`. Grep it for `[FAIL]`, `[OK]`, `error:`. Check `build/host/opkg/inst/bin/upkg` exists. If the build did not finish, run it again the same way.

## opkg fix 5fa49b1 (handoff item 1)
- Branch `opkg-remove-reason` in upterm-ports (worktree `upterm-ports-opkg-msg`). It is local only, not on origin.
- It was rebased onto main ccca4fd ("Rename Amarchy to Patina"), clean, now **d08dc14**.
- Remaining: once the host build passes, run `sh pkgs/opkg/check/remove_reason.sh build/host/opkg/inst/bin/upkg` and the other opkg checks the host build runs. Then merge to main, push upterm-ports (spotUP), and re-pin the meta-repo `repos.lock` (RULES "Cross-repo changes"). Then remove the worktree and branch (worktree-hygiene skill).

## Renamed packages (handoff item 2): already partly done, question left
- f686c417 already writes `Replaces`/`Conflicts`/`Provides` of the old name (`dist/package-renames.txt`, `tools/pkgs.py _rename_fields`). The LIVE feed has them, checked: `up-hexed 1.0` with `Replaces/Conflicts/Provides: up-hexedit`.
- Open question (unmeasured): the old names shipped as **1.0** in the first v1.1 upload (2026-11-12 00:42, before the rename). The new names are also 1.0. Does `upkg upgrade` move an `up-hexedit` install to `up-hexed`? Suspected (inferred, not measured): no. Upgrade walks installed names, and the versions are equal. If so, only `upkg install up-hexed` migrates.
- Probe ready: `thoughts/shared/s78-evidence/rename_probe.sh UPKG NEWVERSION CHECKDIR`. Example: `sh thoughts/shared/s78-evidence/rename_probe.sh ../upterm-ports-opkg-msg/build/host/opkg/inst/bin/upkg 1.0 ../upterm-ports-opkg-msg/pkgs/opkg/check`, then the same with `1.1`. It prints list-upgradable, the upgrade output and the installed set after `upgrade` and after `install up-hexed`.
- After the measurement, put the options to the owner (AskUserQuestion). Candidates: bump the three to 1.1, a transitional old-name package that depends on the new one, an opkg patch so upgrade follows Replaces, or a manual `upkg install` documented. Turn the winning probe into a real check in pkgs/opkg/check or tests/test_dist_ipk.py.

## Rest of the list (unchanged order, from s77 handoff)
- MENU_TAB_NEW with no terminal opens a vsh window (uterm_line path).
- port/htop: rebuild build/kit/userland. upterm-ports main checkout is on branch port/htop with untracked `pkgs/htop/check/cases`.
- IPREFS-REQ-ON-THEME-SWITCH (desk_dos.c:863): research, then options to owner.
- Old-name worktrees vtcon-dopus-env-0c, vtcon-s48-amiberry-default, vtcon-te6c-3: check owners and unmerged work, remove merged ones.
- Dedicated test session: test_tutorial_wiring p1, claude_repl flake, 21 old make test rows.
