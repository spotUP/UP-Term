# S10: why the rig's Packages level shows 3 rows (2026-10-27)

Verdict: host level is correct; no product edit. Cause is the keymap/binary the rig actually loads.

Evidence (host):
- gui.c:571/1485 call ix_level_t(&km,&ixs,node,filter,it,cap,menu_cond). index_core.c:798-821 -> mn_children_t, then pkg_level (:692) returns n for the group itself ("packages"). No kind/KA_JOB filter.
- menu_tree.c:26-43 row_shown: job: rows need no cond (mn_row_needs only terminal kinds, dopus., workbench.).
- Throwaway test (not committed, reverted per brief): ix_level_t on "packages" with the real dist/keymap, for loaded and unloaded pk state, for every cond 0..MN_COND_ALL, returned 5 rows each time: Browse, Search, Installed, Upgrades (group), Refresh. make test ONLY=keymap: 12225 checks, 0 failed.
- Note: the level is 5 rows with "Upgrades" as a group (packages.upgrades.all is its child), not "Upgrade all" at the top level.
- dist/keymap rows 247-251 exist since 919a829e (15:42), so a 3-row result is not a keymap that is merely "older than the job rows" by that commit; it is a keymap/binary that lacks them.

Where the rig differs (the GUI never reads VTC:keymap):
- config/keymap_load.c:31-33,37: keymap order is ENV:up-term/keymap, then ENVARC:up-term/keymap, then the compiled-in km_default_text (gui.c:1399). The staged build/rig3/vtc/keymap (19:56, 5 rows) is only used if Install copies it to ENVARC/ENV.
- A rig whose ENV: or ENVARC: copy predates the Packages rows (or a binary whose keymap.c lacks job:, e.g. built from the main checkout: config/keymap.c:215 job entry exists only on pg8-text2) draws exactly Browse|Search|Installed|... without the job-only group and Refresh.
- Not checked (rig forbidden): `Type ENV:up-term/keymap` and `Type ENVARC:up-term/keymap` on the rig, and the md5 of the installed amarchy-gui vs build/amiga/amarchy-gui. Those two checks decide it; tools/rig/install_rig.py:1543-1566 shows the install refreshes ENVARC and ENV when the shipped keymap changes.
