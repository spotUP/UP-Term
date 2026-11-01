# S10: Packages level shows only Browse|Search|Installed on the rig

Question: why are Upgrades and Refresh missing from ENV:AMARCHY_ROWS at the Packages level?

Finding: the product is right; the rig's keymap copy is stale or overridden. Not verified on the rig (no rig touched).

Evidence (host):
- dist/keymap:247-251 define packages.browse, .search, .installed, .upgrades.all, .refresh (all added together in 919a829e, so no
  shipped keymap ever had only three).
- mn_children_t (amarchy/menu_tree.c:152) lists a group for the dotted id packages.upgrades.all (menu_tree.c:178) and the job: row
  packages.refresh (KA_JOB, mn_row_needs == 0 so every cond shows it; menu_tree.c:26-32).
- ix_level_t (amarchy/index_core.c:798) with a loaded index: pkg_view_id (index_core.c:619) only converts the three view rows to groups;
  pkg_level (index_core.c:692) returns n unchanged for the group node itself ("the keymap rows"). Nothing drops rows.
- Both styles use ix_level_t: text menu amarchy/menu.c:295, GUI amarchy/gui.c:571 and :1485. Neither misses the rows.
- Test: tests/test_keymap.c (pkg_enter + pkg_level on "packages" with a loaded index) now asserts 5 rows. It passed before any code
  change (make test ONLY=keymap, 12184 checks, 0 failed). The existing x==0 test (line ~1251) already asserted 5.

So a rig showing 3 rows reads a keymap without lines 250-251: ENV:up-term/keymap (the rig installs/puts a copy, e.g.
tools/rig/amarchy_clash_rig.py:152 puts dist/keymap there) or ENVARC:up-term/keymap older than 919a829e, or a keymap.user that
replaces [menu]. Check: md5 of ENV:up-term/keymap and ENVARC:up-term/keymap against dist/keymap
(tools/rig/a1200_drift.py checks the same files), and `grep packages ENV:up-term/keymap*`.
No product change; rig fix = reinstall dist/keymap (make wasabi / install), no C rebuild.
