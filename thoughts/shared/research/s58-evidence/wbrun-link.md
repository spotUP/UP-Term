# wbrun / jobprobe link failure (gte_* undefined)

Claim true. `make rig-helpers` on origin/main b1903203: rc=2.
Decisive line: `Error 21: ... Reference to undefined symbol _gte_dirty.` (also _gte_row, _gte_row_label), vlink of build/amiga/wbrun failed.

Cause: commit 7938e589 (TE-4.3) made patina/leaf_core.c call gte_dirty/gte_row/gte_row_label, defined in patina/gthemes_core.c. Targets that link leaf_core.c but not gthemes_core.c broke.

Fix: add patina/gthemes_core.c to the wbrun and jobprobe link lists (prereqs and vc line). Not put into PATINA_LEAF_DEPS: flat and icons already list gthemes_core.c, so that would duplicate symbols.

| target | links leaf_core.c | gthemes_core.c before | after |
|---|---|---|---|
| host vttest (VTTEST_HOST_SRC) | yes | yes | yes |
| PATINA_SRC (Patina) | yes | yes | yes |
| PATINA_MENU_SRC (patina-menu) | yes | yes | yes |
| PATINA_GUI_SRC (patina-gui) | yes | yes | yes |
| jobprobe | yes | NO (latent) | yes |
| wbrun | yes | NO (failing) | yes |

Green: make rig-helpers rc=0; build/amiga/jobprobe rc=0; Patina patina-gui patina-menu patina-index patina-icons patina-flat upprefs rc=0.

## Regression guard
tests/test_make_link_lists.py, wired in make test under make_hygiene. Expands the link recipes with make -n -B (no compile), checks every link line naming patina/leaf_core.c names patina/gthemes_core.c exactly once, needs at least 5 such lines.
- green on 270655ca: [OK] make_link_lists: 5 link lines name leaf_core.c, each with gthemes_core.c once
- red with the Makefile fix reverted: [FAIL] make_link_lists: build/amiga/jobprobe and build/amiga/wbrun link patina/leaf_core.c without patina/gthemes_core.c
- green after restore: [OK] make_link_lists: 5 link lines name leaf_core.c, each with gthemes_core.c once
