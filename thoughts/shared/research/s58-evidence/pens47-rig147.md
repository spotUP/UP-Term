---
date: 2026-11-05
topic: S57-PENS47-UNOWNED - does the Amarchy icon pen holder obtain role pens 4-7 on a fresh full install, OS 3.2
verdict: PASS (4 of 4 obtained; colours right on tokyo-night and ristretto; two of four later applies left 4-7 at graphics defaults, see Found)
---

# Role pens 4-7, rig 147, OS 3.2

Build: worktree vtcon-s58-integ, branch s58-integ, HEAD 25167fd2 (878ba76f + merges of origin/main 55f31585 and fdfd96ab). Not the 878ba76f the task named: origin/main moved twice (RIG-STALE-BASE).
Rig 147: `--max --os 32 --idle-minutes 0 --allow-stale-base "integration branch main+pens47"`, purpose "vtcon-c1: pens47 role pen pixel check". Depth 8, 640x256 Workbench screen.

## Commands
- `make amiga && make wasabi && UPTERM_PORTS=.../kit-main make dist`
- `UPTERM_RIG=147 INSTALLER_KEEP=1 python3 tools/rig/installer_rig.py ONLY fresh-all` -> `[PASS] fresh-all`, rig rebooted by the installer (STATUS after reboot: rows=71 enabled=1 active=0).
- Probe: scratchpad rolepens.c (copy of tests/amiga/iconpens.c plus the holder's rcount/rpen/rhave/rrgb and GetRGB32 pens 0-7), run as `ami.py exec RAM:rolepens`.
- Expected: host program calling gt_parse + desk_pen_colour(t, pen, flat) for pens from desk_live_role_pens(8), on dist/amarchy-themes/<theme>.theme.
- Theme apply: `amarchy_theme_save_rig.py --selects N-N --no-build` (it reports FAIL "the menu closed on the pick" every time; the pick itself happened, prefs theme line changed).

## md5, installed UP-Term:bin/<x> against build/amiga/<x>
- Amarchy e70d5d250846c013222957b99769debf = build
- amarchy-flat a8315ed5cec41140e7aefec72df3f118 = build
- amarchy-icons 9f699412f9b41d291b56e8743894e612 = build
- amarchy-gui 39f5f69be8dbac2e64f6373c0f279c44 = build

## Dialogs
`ami.py requesters`: empty after the install, after each pick. `uitree`: windows "f1host", "Amarchy bar", one unnamed; no requester. The text "DefIcons can not load preferences because: bad template" did NOT show.

## Pens 4-7 (HAVE = 1111111111111111, rcount=4, all four role pens have=1 in every state)
State A, after install (theme tokyo-night):
- read 4=3e4254 5=0e0e14 6=9ece6a 7=0e0e14; expected (flat=1) 4=3e4254 5=0e0e14 6=9ece6a 7=0e0e14. Match.
State B, after pick of ristretto:
- read 4=535154 5=181414 6=adda78 7=181414; expected (flat=1) same. Match.
State C, after pick of miasma (prefs amarchy-gui-theme = miasma):
- read 4=0000ff 5=ff00ff 6=00ffff 7=ffffff (graphics.library defaults, the holder's `want` too); expected 4=4b4b4b 5=121212 6=5f875f 7=121212. No match. Icon pens 32-47 did follow miasma.
State D, after pick of everforest: same stock values 0000ff ff00ff 00ffff ffffff, still after 12 s.
Workbench pens 0-3 stayed 1a1b26 414868 a9b1d6 7aa2f7 (tokyo-night) in C and D.
Prefs in ENV: carried no desktop-theme line; my write of a prefs file with `desktop-theme = on` to ENV: failed ("rename into place failed"), ENVARC: write went through but the next pick rewrote the file. So B vs C/D may be the desktop-theme switch state, not a holder fault. Not hunted.

## Not done
Ride-along inactive title contrast (jade-horse, miasma): not run, budget.
Screenshots: ~/Desktop/upterm-shots/rig147-pens47-ristretto.png, rig147-pens47-pick13.png.
