# Inactive bar colour on rig 35 (S32-INACTIVE-BAR) - INCONCLUSIVE on final cause, inputs verified

Measured with tools/rig/tests/amiga/dripens (build/amiga/dripens, put to RAM:, deleted after) via ami.py, rig 35.

- Theme in use: tokyo-night (prefs file has no gtheme key -> GT_DEFAULT; ENV:up-term/theme-active = tokyo-night). UP-Term:Amarchy/Themes/tokyo-night.theme: background 1a1b26, foreground a9b1d6 (NOT c0caf5; c0caf5 is bright_foreground), no `inactive` key.
- So the design value is gt_mix(1a1b26, a9b1d6, 45) = #5A5E75 (not #656a83; that used fg c0caf5).
- Screen palette: pen0=1A1B26, pen2=A9B1D6 (match theme), pen249=858BAA, pen250=747A94, pen251=323F5F (also pen36=323F5F, 37=747A94, 38=858BAA: the same trio allocated twice).
- Request colour c is therefore 323F5F; flat_inbar_colour(pen0,pen2) would give 5A5E75 and desk_flat_inbar would give 5A5E75. Neither path reproduces 323F5F. No pair of palette entries / shipped-theme colours mixed at any pct 1..99 gives 323F5F exactly (323F5F is about 27 % bg->accent, or about 56 % of 5570AA).
- Rig binaries UP-Term:bin/amarchy-flat and Amarchy are byte-identical (md5) to build/amiga (16:56, after commit 27607459 at 16:53). Code is HEAD.
- The dim pen (249=858BAA) is also not 60 % of any current button pen colour (3E4254 -> 252733), so the held pens 249-251 look stale or rewritten: allocated under some other screen palette/state, or their palette entries were changed after allocation. STATUS says refs=2.
- inbar_rgb zero/set: cannot be read (STATUS prints only pen_inbar). Deduction: neither branch of dim_refresh yields 323F5F from current inputs.

Not established: which earlier state produced 323F5F. Next probe: force dim_refresh (Signal CTRL_D / theme re-apply) and re-read pen 251 palette; if it becomes 5A5E75 the bug is stale pens not refreshed after a theme/palette change.
