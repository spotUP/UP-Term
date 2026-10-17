---
date: 2026-10-17
topic: finish-open-plans run, session 10
status: in progress
---

# Handoff: finish-open-plans run, session 10

Ledger (source of truth): thoughts/shared/plans/2026-10-15-finish-open-plans.md, grep `^- \[ \]`.

## Done this session
- Icon launch like Workbench: wbrun NAME, ddb502d5 (pushed).
- SHELLICON on the A1200: steps 1-3 PASS (XCON icon opens XCON shell, also after reboot), 933a17d0.
- ixemul ENAMETOOLONG: ixemul-vtcon 672c60c (pushed, branch feature/ixemul-80). ixnamelen [OK] on rig 3.
  mkdir needed a parent-directory lookup (CreateDir holds an exclusive lock); link the same (a hard link examines as its target); link() set no errno.

## Blocked on the owner
- The A1200 is unreachable: dist/Uninstall Breaks wasabid (ledger item added). The owner must reboot it at the machine.
  Then SHELLICON steps 4-5: check Shell.info against SYS:System/Shell.info.pre-shellicon, `Assign UP-Term: EXISTS`, the User-Startup block, `UP-Term:bin/git --version` = 2.56.0.
  A1200 leftovers to remove: RAM:wbrun, RAM:rununinstall, RAM:un.out, Rushhours:git-safe, Shell.info.pre-shellicon (after step 5); two old full-screen AmigaShell windows (ask before closing).
- FP1.10: owner's "good enough" on the iTerm2 comparison.

## Next, in order
1. Fix Uninstall: check the marker before the wasabid Break, with a rig test.
2. Ship the new ixemul in the kit (make dist), then git clone on RAM: on rig 3 must fail cleanly.
3. The rest of the ledger's open items: FP4.6 kit-stage stack, vsh `cd ..`, rsync FK3/FK4, gcc clr-merge PR, perl module case, rig 3 capacity, ixemul follow-ups (30-char lookup match, O_EXCL EEXIST, get_component overflow).

## Constraints
- Never print the wasabi key. Never run a bare `Assign X:`. Never stop a rig mid-write. Never commit tools/rig/install_rig.py (someone else's change). No attribution lines in commits.
