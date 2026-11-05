# wbrun / jobprobe link failure (gte_* undefined)

Claim true. `make rig-helpers` on origin/main b1903203: rc=2.
Decisive line: `Error 21: ... Reference to undefined symbol _gte_dirty.` (also _gte_row, _gte_row_label), vlink of build/amiga/wbrun failed.

Cause: commit 7938e589 (TE-4.3) made amarchy/leaf_core.c call gte_dirty/gte_row/gte_row_label, defined in amarchy/gthemes_core.c. Targets that link leaf_core.c but not gthemes_core.c broke.

Fix: add amarchy/gthemes_core.c to the wbrun and jobprobe link lists (prereqs and vc line). Not put into AMARCHY_LEAF_DEPS: flat and icons already list gthemes_core.c, so that would duplicate symbols.

| target | links leaf_core.c | gthemes_core.c before | after |
|---|---|---|---|
| host vttest (VTTEST_HOST_SRC) | yes | yes | yes |
| AMARCHY_SRC (Amarchy) | yes | yes | yes |
| AMARCHY_MENU_SRC (amarchy-menu) | yes | yes | yes |
| AMARCHY_GUI_SRC (amarchy-gui) | yes | yes | yes |
| jobprobe | yes | NO (latent) | yes |
| wbrun | yes | NO (failing) | yes |

Green: make rig-helpers rc=0; build/amiga/jobprobe rc=0; Amarchy amarchy-gui amarchy-menu amarchy-index amarchy-icons amarchy-flat upprefs rc=0.
Regression guard: none added; no existing test reads Makefile link lists (tests/test_make_*.py cover other things).
