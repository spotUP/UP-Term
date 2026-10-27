---
date: 2026-10-27
topic: Amarchy "Packages" submenu fed by `upkg --tsv`; Enter on an installed row with a launch path runs it
status: approved scope (owner, 2026-10-27), in progress (3 of 8)
ledger: thoughts/spot/todos.md row AMARCHY-LAUNCH-FIELD7
research: thoughts/shared/research/2026-10-27-amarchy-packages-submenu.md
---
# Amarchy Packages submenu

## Owner decision (2026-10-27)
This session builds an Amarchy "Packages" submenu that reads `upkg --tsv` and launches field 7 for installed rows.
upkg browse Enter=Launch (plan 2026-10-26-upkg-browse-launch.md L4-L6) stays with the UP-Term session.

## Design (copies the Style > Theme rows pattern)
- TSV: nine tab-separated fields, no header: name, version, installed (1/0), upgradable, size, section, launch,
  src (upkg/uhc), description. A row shows only if installed = 1 and launch is not empty.
- Rows come from `upkg list-installed --tsv` (no feed request) run per menu open into a T: file, the same way
  index.c writes IX_TMP; parse is pure (new pkgs_core.c), DOS part separate (pkgs_dos.c).
- Index: IX_KIND_PKG, IX_PKGS_NODE, a kstart slot pair, `ix_add_pkgs`; dist/keymap row
  `install.packages = Packages | menu:install.packages` (under Install, beside Install add-ons: the [menu] table
  has no single-segment rows, and trigger.clipboard is the same nested pattern). Rows carry path `pkg:<launch>`;
  an empty list shows one inert row (path IX_PKG_NONE, "No installed package has a program to start").
- Run: lf_open_index gets a `pkg:` branch; it starts the program detached with its own `$STACK` cookie
  (seg_stack, shared helper extracted from act.c srv_start_proc so both use one code path).
- Both styles: the load call sits next to gt_dos_load_rows in menu.c and gui.c.

## Checklist
- [x] P1 pure TSV parser + host tests (fields, installed gate, empty launch, short line, CRLF): amarchy/pkgs_core.c,
  tests/test_pkgs.c, suite pkgs 16 checks 0 failed.
- [x] P2 index kind / node / ix_add_pkgs + test_keymap case install_packages_lists_the_package_rows (rows, paths,
  no icon file, inert empty row, earlier adders drop the package rows); keymap 11385 checks 0 failed.
- [x] P3 dist/keymap row install.packages, icon fa-cube in dist/menu-icons (every menu node needs one).
- P4 DOS loader (SystemTags into T:, read, delete) in menu.c and gui.c.
- P5 seg_stack launch helper shared with act.c; `pkg:` branch in lf_open_index.
- P6 check `list-installed --tsv` exists in feat/uhc-repo upkg and carries field 7; else plain `--tsv`.
- P7 rig 7 reachability: feat/uhc-repo upkg on the rig, hotkey, Packages, Enter on an installed package with
  launch, sentinel proves the program ran, both styles.
- P8 A1200: after up-uhc 1.4 lands (vtcon-05 asked to leave UHC state alone until then).

## Results
(filled as measured)
