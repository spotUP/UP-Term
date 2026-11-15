---
date: 2026-11-14
session: up-term-e8 (S81)
status: handoff
---

# S81 handoff: DOpus splash hold, wallpaper kept, desk_expect

Ledger: `thoughts/spot/todos.md` (S81 lines at the end). Trust it over this file.

## Landed on local main (not pushed; origin/main is 1293c2d3)

- 24b1c1da Directory Opus unticked in the installer add-on list.
- 22f772f1 Theme keeps the user's Workbench pictures in wbpattern.prefs (owner: keep pictures).
- 87750c4b desk_expect rig helper links again (stub dropped, real prefs/settings/kcmode/slash sources).
- a3963768 DOPUS-SPLASH-HOLD: start apply waits while the "Directory Opus" splash window is open, max 30 ticks
  (owner: defer until splash gone). Desk test 2391/0.

Push is not done. Before pushing: type-check/lint per RULES.md, ssh ServerAlive check, gh account spotUP.

## Rig evidence

- Wallpaper (rig 9): file check PASS, screen check INCONCLUSIVE. `thoughts/shared/s80-evidence/wallpaper-kept-rig.md`.
  Owed: eye check with a coloured ILBM set through the real WBPattern editor, then apply a theme.
- DOpus hold: NOT RUN (machine saturated, rig start stuck in make -q). Retry steps:
  `thoughts/shared/s80-evidence/dopus-hold-rig.md`. Run `make amiga` through buildq first, when the queue is idle.
  Unknown: whether `rig.py dopus --os 32` installs the Patina WBStartup icon and the new build.

## Open, in order

1. DOpus hold rig repro (above). Then push main.
2. BLOCKPEN branch `s81-blockpen` (a2191d04, worktree ../vtcon-s81-blockpen): live rig check, then owner OK to merge.
   It may conflict with a3963768 in patina/desk_dos.c (dk_foreign_windows became dk_wb_windows).
3. W8b branch `s81-w8b` (49f0f920): legw 23/25. Open: patina-flat blocked on a DOS requester for a vanished volume
   (FAIL 21), FAIL 15 flat=absent. Kit built on a shim ports tree: do not ship. Owner OK needed to merge.
4. Gate items bug 0, 0c, A5, DT-11 (`thoughts/shared/research/release-gate-audit-2026-11-14.md`).
5. test_dist_ipk stale feed (make kit-stage + dist-ipk, heavy).
6. Owner calls: super+t with no terminal (cx.c:43 dead gate); stale worktrees vtcon-te6c-3, vtcon-s48-amiberry-default.
7. Small: ncurses/zlib mirror URLs, opkg Replaces for renamed packages, 21 old make test rows, test_tutorial_wiring.

## Gotchas

- `thoughts/shared/research/test-register.md` is modified by another session: never stage it.
- `.claude/worktrees/agent-a81292c12d8643813` and rig 81 belong to others.
- zsh does not word-split `$S`; use bash -c for source-list loops.
