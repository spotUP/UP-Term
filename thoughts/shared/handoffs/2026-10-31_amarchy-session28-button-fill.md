---
date: 2026-10-31
session: amarchy 28
topic: FLAT-BUTTON-FILL landed; 3.1 Palette USE reboot with Amarchy running found
status: handoff
---

# Amarchy session 28: button fill landed

## Done

- **FLAT-BUTTON-FILL** on main 86074610 (pushed). Commits: 4ef708a6 (solid raised fill in
  the theme lighter background, accent fill when pressed), 614b0954 (graphics Text patch,
  pressed label in pen 0), 86074610 (root-cause fix: arm keyed on the drawing surface, the
  layer or else the bitmap, not the RastPort pointer; Intuition draws the pressed frame and
  the label through two RastPorts of the same layer). Host test in tests/test_flat_core.c
  ("label drawn through a second rastport of the same layer gets pen 0") fails with the fix
  reverted. Rig 8 PASS on 3.2 and 3.1, numbers in s28-evidence/button-fill-rig.md.
  Binary md5 9aebbeaa427e9b841ad5a99f153541d2. Worktree and branch removed.
- s27 Open list 1-2: rig 8 checks PASS (s28-evidence/rig8-checks.md). ROUND-CORNERS research
  done (s28-evidence/round-corners*.md); owner deferred round corners until Amarchy is
  finished. openamigartg and Aminet util/sys/layers: nothing to reuse.

## Open, in order

1. **AMARCHY-31-PALUSE-REBOOT** (todos.md row). On 3.1, stock `Palette FROM x USE` while the
   Amarchy bar, amarchy-gui and amarchy-menu run reboots the Amiga within 2 s (software
   reset into Kickstart ROM). Stopped with `Break <n> ALL`, the same USE works. `Break <n> C`
   alone does not stop the three on a fresh 3.1 boot (second defect). Same family as the
   R1.17-WBRESET rows (bar blocks the IPrefs Workbench reset; 3.2 shows Retry|Cancel).
   Next: fresh rig 8 3.1 boot with `--mmu`, Amarchy under Stack 262144, bisect bar / gui /
   menu; read the Guru before the reboot. Evidence s28-evidence/rig8-31-palette-fault.md.
2. DESK-RIG-FLAT-LEFTOVER (rig script fixes, "already resident" after Break).
3. 4-colour screen fallback makes buttons near-invisible (decision later).
4. Deferred by owner: FLAT-TITLE-ACCENT, inactive title dim (#505050), round corners.

## Rig notes

- Rig 8 only; 3.2 via `--os 32`. Measure from the binary you built (one agent first used a
  stale main-checkout build, md5 differs).
