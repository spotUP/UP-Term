---
date: 2026-11-04
session: Amarchy vtcon-16 (s56, second half)
status: handoff
reason: weekly limit approaching
---
# Amarchy session 56b handoff

Follows 2026-11-04_amarchy-session56-locale-knobs-flat-budget-palette-jit.md.

## Done (on main, pushed)

- S56-PALETTE-31-SAFE: 65fdf327, 22e714e7, b5ad1c1d, dec81865. Every palette.prefs Amarchy writes is 3.1-safe (12 pens + ffff per PALT list); a 3.2-format ENVARC palette.prefs is converted at Amarchy start after a .32bak backup; the 3.1 palette gate is lifted. Rig 84 (3.1 --os 31) rows (a) start conversion, (b) menu picks (ENV + ENVARC md5 6abeac3b), (c) reboot with converted file (WB 40.42 alive) PASS; 3.2 no-change PASS. Evidence s56-evidence/pal31-rig-pass.md. Desk suite 2285/0.
- S56-LOCALE-SAVE-FILL: PASS (fill is no longer pen 0).
- Pixel evidence of the regression below: 0292023f, s56-evidence/accent-cycle-pixels.md.

## REGRESSION on main (fix first)

37b5e3d2 (S56-PRIMARY-ACCENT-FILL / cycles keep old fill) makes every GadTools push button FLAT_ROLE_PLAIN. Rig 86 (3.2, depth 8): Locale, ScreenMode, Input, Serial, Sound prefs Save/Use/Cancel fill pen_button 4 with label pen 2, 0 px accent pen 3; Cancel lost its pen 0 + outline secondary look. Cause: `flat_push_label()` (flat_core.c) rejects GadTools buttons in `role_of()` (amarchy/flat.c ~455); exact GadgetText layout unverified. Owner decisions stand: primary buttons get accent fill; only real push buttons, cycles keep pen_button fill.

In flight at session end (opus agent): branch `s56-pushfix`, worktree ../vtcon-s56-pushfix. Brief: probe gadgets with s34-evidence/gadprobe.c on a rig (88), fix the discrimination at the root, regression test in tests/test_flat_core.c with measured values, flat-budget headroom, rig pixel re-check, evidence s56-evidence/push-role-fix.md, commit local (not pushed). Next session: check that branch/worktree; if a commit with evidence exists, review it, rebase on origin/main, run `nice -n 19 make test-one SUITE=flat_core`, `make flat-budget`, `make amiga`, push HEAD:main. If no commit: rerun the brief. Check rig 88 is stopped (`rig.py status`); stop + clear it if left up.

## Open (in this order)

1. Push button regression above.
2. S56-DESK-RIG-31-STALE: branch `s56-deskrig` (worktree ../vtcon-s56-deskrig) commit 81f68ac4 updates tools/rig/amarchy_desk_rig.py 3.1 expectations (DESKMASK 15, IPrefs reset required on 3.1, palette compare vs desk_core, new pal_pen_counts 12+ffff row). Unverified: a sonnet agent was building the worktree and running the full script on rig 89 (3.1) at session end. Check for a register row / extra commit on the branch, then merge the same way. Stop + clear rig 89 if left up.
3. S56-WB-RESET-BLOCKER: owner rig 84 WB reset blocked by some app, no crash. Not reproduced (3 picks, no requester). Need the owner's open-window list when the requester shows. screennotify.library only helps listeners; it does not unblock.
4. Owner eye check on rig 96 (new-shell knob, Locale knobs, Cancel corners): only after the regression fix is on main; rebuild rig 96 from main first.
5. Older: Palette-47 flat-pens gates (dk_flat_pens ~:704, flat start refresh ~:1049) can probably lift now; .32bak files never removed; wasabi pin bump with a rig check; rig proof of the textmenu fix; flat budget headroom tight.

## Ledger

thoughts/spot/todos.md rows: S56-PRIMARY-ACCENT-FILL (regression note), S56-PALETTE-31-SAFE (done), S56-DESK-RIG-31-STALE, S56-WB-RESET-BLOCKER.

## Note

The shared checkout is on branch fix/ud2-globe-band (vtcon-ee session). Commit Amarchy work to main from a worktree; never switch the shared checkout. Push failures: check the push exit code before removing a worktree.
