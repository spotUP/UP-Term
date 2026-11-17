# DOpus lister toolbar in README shot (s75)
Verdict: (c), closest to (a)+(b) hybrid. Package installs the flat banks; nothing selects them as DOpus's default.
Only the Amarchy theme applier switches listers that are ALREADY OPEN, once per theme apply.
Chain:
- Generate: Makefile:2169-2175 (gen_dopus_toolbar.py -> build/dopus-toolbar/{Images,Buttons}; build/ has Amarchy-toolbar-filled-16/24/32 + t). Needs fontTools (todos.md:422 FONTTOOLS-MISSING, only matters on regeneration).
- Pack: Makefile:2186-2187 dist-dopus copies Images+Buttons into KIT_dopus/Files/toolbar.
- Install: dist/addons/dopus/install.dos:56-64 copies to DOpus5:Images/Amarchy/ and DOpus5:Buttons/Amarchy-toolbar-<set>. No DOpus config/env edit (grep: no default-toolbar setting).
- Select: amarchy/menu_theme.c:208-262 dopus_toolbar_flat, called at :292 from the theme apply; sends `lister set <h> toolbar DOpus5:Buttons/Amarchy-toolbar-<set>` (menu_theme.h:93-94) only for handles from `lister query all` whose path is the upstream default. A lister opened afterwards starts on upstream Buttons/Toolbar (classic glyphs) until the next apply.
- Shot: thoughts/shared/s74-evidence/readme-screenshots.md:37: `rx "... lister new 120/70/560/440 Rushhours:Tools/UP-Term"`, no theme apply after it, no toolbar check (only "flat (no bevels)"). Shot is therefore classic by construction.
Missing link: no step makes a NEW lister (or the DOpus default) use the flat bank; the shot recipe never re-applied the theme.
Unknown: whether Amarchy re-applies on new listers (grep found no hook outside menu_theme.c); the 32 px bank is unverified on a rig (todos).
Fix: shot recipe: apply the Amarchy theme (or rerun dopus_toolbar_flat) after `lister new`, then retake. Product: set the flat bank as DOpus's default lister toolbar (config/install) or apply on lister open.
Rig check: boot kit rig, `lister new`, `lister query <h> toolbar` (expect upstream path); apply theme, query again (expect DOpus5:Buttons/Amarchy-toolbar-filled-24), file listing of DOpus5:Buttons shows the banks.
