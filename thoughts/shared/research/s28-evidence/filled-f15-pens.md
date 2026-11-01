# F15: icon pens 32..47 per state (OS 3.2, rig 12, depth 8, aga256)

Commit under test: origin/main 458249d0 (filled icon set b9a7f933 + shared holder H1). Probe: `tests/amiga/iconpens.c` (new; reads
the `Amarchy.IconPens` record and GetRGB32 of pens 32..47), driver `tools/rig/amarchy_iconpens_rig.py`.
Expected RGB: `ip_colours()` of the built-in default theme (tokyo-night) compiled on the host. Owner bits: FLAT=1, ICONS=2.

Pens (identical in states 1-5): 32 494d60, 33 a9b1d6, 34 a9b1d6, 35 7aa2f7, 36 3a486b, 37 787e9a, 38 7d84a4, 39 5974b0,
40 272a3c, 41 414868, 42 535973, 43 292e42, 44 1a1b26, 45 343952, 46 1a1b26, 47 1a1b26. All equal the expected values.

| State | Owners | Record | Pen RGB vs theme | Verdict |
|---|---|---|---|---|
| 0 before APPLY (reference) | none | absent | pen 32 reads ffffff, not theme (all 16 differ) | baseline |
| 1 flat off (not running), APPLY done | 2 (ICONS) | held=16 refs=1 first=32 count=16 have=1111111111111111, screen = Workbench | 16 of 16 match | PASS |
| 2 amarchy-flat running, switch ON | 3 (ICONS+FLAT) | held=16 refs=2, same screen | 16 of 16 match | PASS |
| 3 switch OFF, amarchy-flat still running | 3 (ICONS+FLAT) | held=16 refs=2 | 16 of 16 match | PASS (see note) |
| 3b amarchy-flat quit (Ctrl-C) | 2 (ICONS) | held=16 refs=1 | 16 of 16 match | PASS |
| 4 icons RESTORE | 0 | record freed (HOLDER none) | pens released; the colour map still reads the theme RGB (ReleasePen does not reset the colour) | PASS |

Note on state 3: the task expected owners = ICONS after "flat off". By design (`ip_hold_wanted`, amarchy/flat.c:804-812) the
FLAT owner follows the amarchy-flat PROCESS, not the flat switch, so owners stay ICONS|FLAT with the switch off and drop to ICONS only
when amarchy-flat quits (3b). Not a defect; no product code changed.

Also seen: APPLY rc text `written=254 ... fallback=190`, RESTORE `restored=254 restore-failed=0`. `rig.py aga256` printed
"[WARN] the Workbench stayed PAL (W43)" but the screen was depth 8 in every state.
Raw run log: reproduce with `UPTERM_RIG=<n> python3 tools/rig/amarchy_iconpens_rig.py --expect <PEN lines>`.
