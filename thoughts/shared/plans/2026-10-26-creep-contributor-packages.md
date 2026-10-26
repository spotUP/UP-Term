---
date: 2026-10-26
topic: CREEP, outside contributors add packages to the upkg feed through recipe PRs to upterm-ports
tags: [upkg, upterm-ports, ipk, feed, contributing, ci, generator]
status: plan-only
---

# CREEP: contributor packages (plan, nothing built)

Owner decision 2026-10-26 (not re-opened): contributors send recipe PRs to spotUP/upterm-ports; the owner builds, rig-checks and
releases. Own/third-party feeds are deferred until a signed index exists (research: thoughts/spot/term-findings/creep-packages-2026-10-26.md,
section 4 option B, section 5).

Finish line: every item CP1-CP9 ticked, and CP8 green. Execution state is ledgered in this file (tick the boxes).

## 0. Facts that shape the plan (measured 2026-10-26)

- pkgs/tree is a BASE-kit program: no row in dist/packages.txt, no dist/slim.txt rows, no dist/addons/tree. It is the right worked example
  for the RECIPE (20 lines, smallest) and the wrong one for the vtcon side. The vtcon side is already committed for pkgs/bitchx
  (packages.txt:30, slim.txt:17-20, dist/addons/bitchx/, Makefile KIT_bitchx / dist-bitchx, SUMMARY['bitchx'] pkgs.py:141, test_dist_slim.py:24).
  So: the doc walks tree for the recipe and bitchx for the feed rows; the reachability test (CP8) uses bitchx, not tree.
  The request said "pkgs/tree"; tree cannot satisfy a rows-match assertion because it has no committed rows.
- Recipe metadata already in recipe.mk (documented in upterm-ports mk/common.mk:80-87): _VERSION, _URL, _SHA256, _LICENSE,
  _LICENSE_FILES, _DEPS, _BINS, _DATA (extra trees, bitchx: lib/bx), _STACK, _NO_KIT. Missing for the feed: the one-line Description
  (today SUMMARY dict in tools/pkgs.py:133) and the feed package id/archive name.
- Existing parsers reused, none written new: tools/recipe-var.sh (`var P FIELD` = `make -s print-P_FIELD`, upterm-ports), tools/slim.py
  (reads dist/slim.txt, imported by pkgs.py), tools/pkgs.py parse()/rows()/addons() (packages.txt, columns pkgs.py:19), pkgs.py
  index()/feed(). Searched: tools/ in both repos, Makefile print-% rule, mk/common.mk, tests/test_dist_slim.py, test_recipe_var.sh.
- CI: none. vtcon has no .github/workflows; upterm-ports has no .github (checked ls -a). "Existing CI" to extend = none; the host-side
  entry points that exist are `make test` (test_m68k_scan.py, test_check_bin.sh, test_kit_stage.sh, test_recipe_var.sh) and `make host-<pkg>`.
- Hand-written, not derivable from the recipe: dist/addons/<id>/{install.dos, Install.installer, README.txt, parts.txt, uninstall-body.dos,
  LICENSES.txt}, ixemul/needs-base/handed columns. The generator must emit them from a template (bitchx's files are the template) and
  the recipe fields; anything it cannot derive is a documented flag with a default.

## 1. Items

### CP1  Add feed metadata to the recipe format (ports repo)
- Files: upterm-ports/mk/common.mk (document in the field comment block, lines 80-87), upterm-ports/pkgs/bitchx/recipe.mk (migrate),
  upterm-ports/RULES.md (rule 1 lists the fields).
- New optional fields: `<p>_FEED_ID` (default up-<p>), `<p>_SUMMARY` (one line, becomes the Packages Description), `<p>_FEED_MIN_BASE`
  (default the current base version). Recipes without `_FEED_ID` stay base-kit programs (tree remains one).
- Done: `make -s print-bitchx_SUMMARY` prints the text now in pkgs.py SUMMARY['bitchx']; `make -s print-tree_FEED_ID` is empty.
- PROGRESS 2026-10-26: DONE, upterm-ports branch creep/feed-fields (worktree .claude/worktrees/creep-fields) commit b832339; not pushed. Fields documented in mk/common.mk and RULES.md, bitchx migrated; print-bitchx_SUMMARY equals pkgs.SUMMARY, print-tree_FEED_ID empty.

### CP2  Generator `tools/pkg_rows.py` in vtcon
- Files: vtcon/tools/pkg_rows.py (new), vtcon/tools/test_pkg_rows.py (new). Imports slim and pkgs, shells out to upterm-ports
  `make -s print-<p>_<FIELD>` (the recipe-var.sh mechanism), no new recipe parser.
- Modes: `pkg_rows.py emit <pkg> --ports DIR` writes the rows and the dist/addons/<id>/ set from templates taken from dist/addons/bitchx
  into a scratch dir (or `--write` into the tree); `pkg_rows.py check <pkg> --ports DIR` diffs the generated rows against the committed
  ones and exits non-zero on drift, naming the file and row.
- Emits: packages.txt row, slim.txt rows (from _BINS, _DATA, licenses/<pkg>), dist/addons/<id>/ six files, Makefile snippet (KIT_<id>,
  dist-<id>, the dist: and dist-index prerequisites), SUMMARY line. For the Makefile and pkgs.py the generator prints a patch hunk for the owner
  to apply (editing two shared code files from a script is the fragile part; see CP3).
- Done: `python3 tools/pkg_rows.py check bitchx --ports ../upterm-ports` exits 0; changing one slim.txt row makes it exit 1 naming that row.
- PROGRESS 2026-10-26: DONE, vtcon branch creep/pkg-rows (worktree .claude/worktrees/creep-pkgs), commit see git log -1 there; tools/pkg_rows.py + tools/pkg_rows_tpl/ + tools/pkg_rows_prose/bitchx.json + tools/test_pkg_rows.py. `check bitchx --ports <ports>` exits 0, changed slim row exits 1. Deviation: the Installer/README prose and the man-page row cannot come from the recipe, so they sit in tools/pkg_rows_prose/<pkg>.json (generic fallback from _SUMMARY).

### CP3  Remove the code edit per package (pkgs.py SUMMARY, Makefile targets)
- Files: tools/pkgs.py (SUMMARY lookup falls back to / is replaced by a `Summary` column or the recipe field), Makefile (dist-<id> and
  KIT_<id> become a loop over slim.txt ids instead of one hand-written target per package; slim.txt ids already drive SLIM_STAGE, Makefile:1395).
- Why: otherwise the generator can only print hunks and a contributor PR touches shared code. Target state: a package is data only
  (recipe + rows + addons dir).
- Done: removing the `bitchx` line from SUMMARY and the dist-bitchx target leaves `make dist` output (ipk bytes, Packages) identical
  (byte-equal rebuild is already a pkgs.py property); tests/test_dist_slim.py:24 list is read from slim.txt, not hard-coded.
- PROGRESS 2026-10-26: CP3 done in creep-pkgs worktree, commit dd7e4b49 (pkgs.summary() falls back to recipe _SUMMARY; SLIM_IDS loop for KIT_/dist-/prereqs; pkg_rows prints no hunks; ipk+Packages md5 identical for bitchx, make -n dist-bitchx/dist-git identical). Not done: git/perl/vim/vbcc SUMMARY lines stay until their recipes get _SUMMARY.
- PROGRESS 2026-10-26: CP3b done. git/perl/vim/vbcc _FEED_ID/_SUMMARY/_FEED_MIN_BASE in ports creep-fields; pkgs.summary(pkg_id) has no default and raises SystemExit naming id and recipe path when the recipe is missing (regression test NoPortsCheckout in tests/test_pkg_rows.py, fails on old pkgs.py). SUMMARY dict keeps claude, pi, nvim, python (no recipes). ipk+Packages md5 identical, 9 packages.

### CP4  Contributor doc `docs/adding-a-package.md` + CONTRIBUTING.md in upterm-ports
- Files: upterm-ports/docs/adding-a-package.md (new), upterm-ports/CONTRIBUTING.md (new, short, links to it), upterm-ports/README.md (link).
- Content: (1) scope and the trust statement: owner builds and releases, a merged recipe is code that runs on users' Amigas (postinst);
  (2) worked example pkgs/tree, line by line (_VERSION/_URL/_SHA256, _LICENSE + _LICENSE_FILES, _DEPS ixcompat, _BINS, _STACK 65536,
  configure/build/install cmds, the HOST_* commands, pkgs/tree/check cases); (3) rules from RULES.md (no fork, ixemul-vtcon, stack cookie,
  max 4 jobs, patches via `make <pkg>-patches`); (4) how a recipe becomes a feed package: set _FEED_ID/_SUMMARY, run
  `pkg_rows.py emit`, show bitchx as the finished result; (5) licence checklist (LICENSES.txt row, GPL source offer via SOURCES.txt);
  (6) PR checklist and what the owner will do (CP6); (7) what is NOT accepted yet: own feeds, binary-only packages, packages needing a libc change
  (those go to ixemul-vtcon first).
- Done: a reader with only the ports checkout and the doc can run `make host-tree` and `pkg_rows.py check bitchx` by copying commands;
  every command in the doc is run once by me and the output pasted into the doc's "expected" blocks.
- PROGRESS 2026-10-26: DONE (unverified on GitHub only for CI links), upterm-ports creep/feed-fields commit see log "Contributor guide"; docs/adding-a-package.md, CONTRIBUTING.md, README link. Commands run once: make host-tree (20 expected outputs), recipe_lint.sh tree, pkg_rows.py check bitchx (silent, rc 0), emit tree/bitchx.

### CP5  PR template
- Files: upterm-ports/.github/pull_request_template.md (new).
- Checklist mirrors CP4 (6): recipe, sha256 from the fetched file, licence fields, check cases, host build output, rows generated, no
  binaries committed.
- Done: file exists; the CP7 workflow fails a PR whose body lacks the ticked licence line (optional, see Decisions D4).
- PROGRESS 2026-10-26: DONE, .github/pull_request_template.md (checklist, licence text/URL and redistribution terms asked per D6); upterm-ports creep/feed-fields.

### CP6  Owner review-build-release runbook
- Files: vtcon/thoughts/shared/plans/ (this section) and a short "Owner steps" block in upterm-ports/docs/adding-a-package.md.
- Steps, in order:
  1. Read the diff for the postinst/install.dos (the code that runs on users' machines) and the patches; check the licence rows and URL host.
  2. Fetch the tarball yourself, compare sha256 with the recipe.
  3. `make <pkg>` (JOBS=2 if busy), then `make host-<pkg>`.
  4. In vtcon: `pkg_rows.py emit <pkg> --write`, apply the hunks (until CP3), `python3 -m unittest` on the touched test files via `make test ONLY=`.
  5. Rig check with `rig.py start --max --purpose "<session>: check <pkg> ipk"`: `make wasabi`, `make amiga`, `rig.py install`, install the ipk
     with upkg, run the `check` cases, uninstall, confirm the drawer is gone. Stop the rig after.
  6. `make dist`; confirm `build/ipk/<id>_*.ipk` and Packages carry the new stanza.
  7. Merge the PR, commit the vtcon rows, bump the version, `gh release create` per RULES.md:152-153 (release upload still asks).
  8. Install from the live feed on a clean rig: `upkg update && upkg install <id>`.
- PROGRESS 2026-10-26: text done, walk open. Owner steps in upterm-ports docs/adding-a-package.md section 7 match steps 1-8 with real commands; upterm-ports creep/feed-fields.
- Done: runbook walked once on a trivial recipe (candidate: a small public-domain tool) by the owner or a rig session; defects fed back to CP4.

Decision 2026-10-26 (owner): the rig check runs after the merge on the built .ipk, before the release upload (runbook step 6 now, docs commit 69d57e7). CP6 host walk done (figlet), defects D1-D5 fixed (vtcon ba9f770c, ports f52fb02); the rig step is walked on the first merged recipe.
### CP7  PR CI in upterm-ports (host-side only)
- Files: upterm-ports/.github/workflows/pr.yml (new). Existing workflows found: none in either repo (searched .github in both).
- Jobs on pull_request, ubuntu-latest, no emulator, no m68k toolchain (a cross build needs the pinned gcc and is not cheap):
  1. recipe lint: new `tools/recipe_lint.sh` checks every changed pkgs/*/recipe.mk defines _VERSION _URL _ARCHIVE _SHA256 _LICENSE
     _LICENSE_FILES (+ _BINS or a library marker), the URL is https, patches apply order, `check` dir present for programs.
  2. sha256 fetch: download _URL, compare to _SHA256 (this is the "sha256 fetch" of the research).
  3. `make test` (existing host tests).
  4. host build, only if the recipe has `_HOST_BUILD_CMD` and the job stays under ~10 minutes: `make host-<pkg>` for changed packages
     (Linux host build of the same source is a smoke test, not the Amiga build).
  5. `pkg_rows.py check` against vtcon main (clone read-only) for changed packages that set _FEED_ID.
- Security: pull_request (not pull_request_target), no secrets, read-only token. The fetch job runs untrusted recipe make code, so it
  runs inside the runner with no secrets; never self-hosted.
- Done: a PR with a wrong sha256 fails job 2 with the expected hash printed; a good bitchx no-op PR is green.
- PROGRESS 2026-10-26: DONE locally, UNVERIFIED on GitHub (not pushed): .github/workflows/pr.yml (actionlint clean, YAML loads) + tools/recipe_lint.sh (new; no lint existed). Jobs: changed, lint, sha256 (tools/fetch.sh), make test, host build (continue-on-error, only recipes with HOST_ cmds), feed rows (pkg_rows.py check vs spotUP/UP-Term main, skipped while it lacks the tool). Wrong sha256 shown to print want/got locally; make test on Linux not run.

### CP8  One reachability test
- Files: vtcon/tests/test_pkg_rows.py (new).
- Drives the generator's top-level entry (`pkg_rows.py emit bitchx` through `main(argv)`), with a call counter on the recipe-field reader
  proving the ports Makefile was actually asked (sentinel: the `print-bitchx_SUMMARY` answer is in the output), and asserts the produced
  packages.txt row, slim.txt rows (4), six addons files and SUMMARY text equal the committed ones byte for byte; plus one negative case
  (a mutated _BINS changes the slim rows). tree is asserted to produce NO feed rows (no _FEED_ID), which covers the user-named example.
- Skips (not fails) with a clear message when ../upterm-ports is absent. Needs make; no emulator.
- Done: test fails with the generator's template step removed, passes restored; runs in `make test ONLY=pkg_rows`.
- PROGRESS 2026-10-26: DONE, tests/test_pkg_rows.py (python3 -I, UPTERM_PORTS overrides ../upterm-ports); 4 tests OK; broken template makes 2 fail. Needs the ports branch merged (or UPTERM_PORTS pointing at the creep-fields worktree). Not yet wired into make test (CP9).

### CP9  Wire and ledger
- Files: vtcon/RULES.md (Commands table row for pkg_rows.py), vtcon/Makefile test target, upterm-ports/RULES.md Commands row, thoughts ledger.
- Done: both Commands tables list the new commands; CP1-CP8 ticked here with commit ids.
- PROGRESS 2026-10-26: wiring DONE: vtcon creep/pkg-rows (Makefile ONLY=pkg_rows line + RULES.md row), upterm-ports creep/feed-fields (RULES.md recipe_lint.sh row). Ticking CP1-CP8 with commit ids still open.
- COMMITS 2026-10-26: vtcon creep/pkg-rows: CP2+CP8 f9629ec9, CP3 dd7e4b49, CP3b 0eef9104, CP9 18a98b35. upterm-ports creep/feed-fields: CP1 b832339, CP3b 8d3ecaf, CP4 52605fa, CP5 e7b73fd, CP7 62af036 (+ mutt URL 48f4c8a, lint skips _LOCAL 269fc0c), CP9 c7cea09. All unpushed. Open: CP6 walk on rig 8; CP7 first GitHub run; CP8 `make test ONLY=pkg_rows` from main checkout.

## 2. Order and size
CP1 -> CP2 -> CP8 -> CP3 -> CP4/CP5 -> CP7 -> CP6 -> CP9. One agent each for CP2+CP8 (sonnet), CP3 (sonnet), CP4-CP7 (sonnet);
design review of CP1/CP3 stays with the owner session. Rough size: a day of agent time.

## 3. Risks
- A contributor recipe's make code runs on the owner's machine at build time and on CI: review before any local `make`, build in a throwaway
  checkout.
- Name clashes with official ids: the generator refuses an id already in packages.txt without a Replaces decision.
- Two repos: rows land in spotUP/vtcon, recipe in upterm-ports; a PR merged in one without the other leaves the feed unchanged (safe) but the
  doc must say the owner lands the vtcon half.

## Decisions for owner
- D1  Worked example. Recommended: tree for the recipe and bitchx for the feed rows (tree has no feed rows; a rows-match test on tree is impossible).
  Alternative: first promote tree to a feed package (adds an ipk nobody asked for).
- D2  CP3 (data-only packages, no Makefile/pkgs.py edit per package). Recommended: do it before opening to contributors; otherwise every
  accepted package needs an owner code edit (the bottleneck the decision wants to avoid). Alternative: generator prints hunks only.
- D3  Where the generator lives. Recommended: vtcon tools/ (it reads vtcon tables and templates; ports stays recipe-only). Alternative: ports
  repo, needs a vtcon checkout.
- D4  PR template enforcement. Recommended: checklist only, CI does not parse the PR body. Alternative: CI fails on unticked boxes.
- D5  Host build in CI (CP7 job 4). Recommended: only for recipes with HOST commands, advisory (non-blocking) because Linux differs from macOS
  where the HOST_* commands were written. Alternative: required, or skip entirely.
- D6  Licence policy. Recommended: accept OSI-approved and GPL (with source offer) only; reject non-redistributable. Alternative: case by case.
- D7  New recipe field names (_FEED_ID, _SUMMARY, _FEED_MIN_BASE). Recommended as written; alternative: put them in a pkgs/<p>/feed.txt.

## Owner answers (2026-10-26)
- D1: tree for the recipe walk-through, bitchx for the feed rows and the CP8 test (factual; tree has no feed rows).
- D2: YES, CP3 (data-only packages) lands before opening to contributors.
- D3: generator in vtcon tools/ (default kept).
- D4: PR template is a checklist only; CI does not parse the PR body (default kept).
- D5: host build in CI is advisory and runs only for recipes with HOST commands.
- D6: licence policy is CASE BY CASE (not the recommended OSI+GPL rule). The PR template asks for the licence text/URL and redistribution terms; owner judges each PR. CP4 doc and CP5 template must say this.
- D7: new fields are recipe variables _FEED_ID, _SUMMARY, _FEED_MIN_BASE.
