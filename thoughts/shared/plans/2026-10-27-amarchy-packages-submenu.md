---
date: 2026-10-27
topic: Amarchy "Packages" submenu fed by `upkg --tsv`; Enter on an installed row with a launch path runs it
status: approved scope (owner, 2026-10-27), in progress (6 of 8)
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
- [x] P4 DOS loader amarchy/pkgs_dos.c (pl_run into T:amarchy-pkgs.<task>, read, delete) in menu.c and gui.c,
  lazy: only when the menu enters IX_PKGS_NODE (ix_pkgs_due, host tested in install_packages_lists_the_package_rows). e0acf363.
- [x] P5 amarchy/proclaunch.c (pl_start / pl_run, cookie vs floor) shared with act.c srv_start_proc and leaf.c
  start_detached; `pkg:` branch in lf_open_index; lf_command_program host tested. e0acf363.
- [x] P6 feat/uhc-repo f30fc35 pkgs/opkg/doc/upkg-tsv.txt: `upkg list-installed --tsv [glob]`, nine fields, field 7
  launch (installed ipk: its control Launch: line; UHC: full ExtBin path), no network request.
- P7 rig 7 reachability: feat/uhc-repo upkg on the rig, hotkey, Packages, Enter on an installed package with
  launch, sentinel proves the program ran, both styles.
- P8 A1200: after up-uhc 1.4 lands (vtcon-05 asked to leave UHC state alone until then).

## Results
- Rig 7 (060), 0 packages installed: `upkg list-installed --tsv` median 312 ms (min 306, max 405) against a bare
  `Echo` 204 ms, about 108 ms of upkg per run. Too much for every menu open, so the rows load lazily on entering
  Install > Packages (P4). Evidence: session scratchpad upkg-timing.md.
- leaf.c start_detached started every detached line on 32768 bytes; vsh's cookie is 262144 (RULES.md rule 6).
  Fixed with P5: pl_start reads the program's cookie.
- Open: run_in_uterm in leaf.c gives vsh NP_StackSize 65536, below its 262144 cookie (not part of this plan).
