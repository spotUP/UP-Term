---
date: 2026-11-26
session: up-term-b3
ledger: UP-Term/thoughts/spot/todos.md row SPLIT-PATINA-REPO
status: plan only (read-only agent, nothing built or changed); vtcon main 7420d8a4
supersedes: patina-split-plan.md sections 2-5 and DECISIONS 1, 5, 6, 8 (rejected 2026-11-26)
keeps: S1-S4 on main (44e85f60); owner answers D2 (merge, no force-push) and D4 (merge or park 5 worktrees first)
---

# Patina split v2: patina with zero vtcon dependency

Owner 2026-11-26 23:20: "it should not depend on vtcon". Patina must build, test and release with no
vtcon pin, snapshot, submodule, include path or Makefile reference. vtcon MAY depend on patina.

Rule used below: the dependency points one way, vtcon -> patina. Nothing in the patina repo names a vtcon
path, variable or revision in a build, test or CI input. Commit messages and human notes may mention vtcon.

## 1. What patina uses from vtcon today (verified)

Verified from two structures:
1. vtcon.mk `PATINA_VTCON_SRC` (40 .c) and `PATINA_VTCON_HDR` (43 .h), the lists the S3 build uses.
2. An include closure computed from the sources: every `#include "..."` followed from all 178 tracked
   patina/ files plus the 40 sources (script output: scratchpad patinasplit2/closure.txt). It reaches 98
   non-patina files: the 40 + 43 above, plus 15 headers the lists do not name (they are included only
   inside the shared .c files or by patina headers): demo/updemo.h, device/romspan.h, engine/vtengine.h,
   handler/be32.h, handler/clipfmt.h, handler/kcmode.h, handler/pubscr_core.h, handler/slash.h,
   install/fssm.h, prefs/prefs_ui.h, render/glyphmap.h, render/otag.h, render/pic_pens.h,
   render/unifont.h, shell/vsh_quote.h. Every listed file is reached (none stale).
3. `git grep` of `#include "<dir>/` in patina/ gives 43 distinct direct includes (spelling check only).

Non-source inputs, from vtcon.mk and the patina rules in the Makefile (lines 1121-1309, 2232-2245):
- toolchain: tools/vbcc-aos68k.cfg.in, VC flags, VTCON_NDK = vendor/ndk-3.2r4-Include_H (untracked,
  installed by hand per RULES.md), third_party/roadshow-netinclude (71 tracked files, 636 KB)
- generated: KEYMAP_DEFAULT_INC (tools/gen_keymap_default.py + dist/keymap), UPTERM_VERSION_H/GITREV
- linked: NOBREAK = handler/nobreak.c; tools/upkgui.rev (a pin into upterm-ports, not vtcon)
- tools: mkkit.py, mkicon.py, mkversions.sh, gen_emoji.py, gen_jbmono.py, ilbm_decode.py, pkgs.py
- dist: dist/instssblock.sh, the up-patina row of dist/packages.txt
- tests: tests/harness.c and harness.h (test_main_patina.c includes harness.h)
- rig: ami, condev_rig, paths, verdicts, stage_rig, tabstate (1793 lines) imported by 36 patina_* rig files

Who in vtcon uses each shared .c (one pass over 449 vtcon .c/.h, header include count; scratchpad
patinasplit2/users.txt): the 5 render cores and cxdesc are used in vtcon only by upprefs, vttest_host and
flat-budget; config/amconf_core.c includes render/desk_core.h. All handler/, claude/, install/, prefs/ and the
other render/ files are terminal code with 1-69 vtcon users.

Churn, last 30 days: 164 commits touched the 98 files, 43 of them also touched patina/. So the shared code
moves often and is co-edited with patina about one commit in four. Any model below pays for that.

## 2. Where each file goes

### 2.1 MOVES to patina (patina owns it; vtcon then compiles it from the patina pin)
The Patina look and config cluster, 16 files:
- render/desk_core.c/.h, render/flat_core.c/.h, render/flat_shared.h, render/flat_glyph.h,
  render/iconpen_core.c/.h, render/gthemes_core.c/.h, render/gthemes_scan_dos.c/.h
- config/cxdesc.c/.h
- config/amconf_core.c/.h, config/amconf_dos.c/.h, config/amconf_keys.c (amconf = the old Amarchy config
  format). Gate in S5: if a vtcon user of amconf_* turns out to be terminal logic, not Patina settings
  read by handler/prefs, amconf falls back to 2.2 (copy). Not read line by line yet.
Plus everything the old plan moved: patina/ (178), the patina_host suites and patina-only python tests,
tools gen_patina_help.py, gen_patina_themes.py, gen_lucide_icons.py, preview_lucide_icons.py,
gen_menu_icons.py, the 40 patina-only dist files, man/patina.1.
Why: these files are Patina's look rules (D13 kept them in vtcon only because patina pinned vtcon).
Moving them makes the dependency one-way: upprefs and vttest_host read them from patina.

### 2.2 COPIED into patina as `upcore/<same path>` (vtcon stays the owner)
- the remaining 24 shared .c, their headers and the 15 transitive headers (about 80 files)
- handler/nobreak.c, tools/vbcc-aos68k.cfg.in, third_party/roadshow-netinclude/
- tools/gen_keymap_default.py, dist/keymap, tests/harness.c/.h
- tools mkkit.py, mkicon.py, mkversions.sh, gen_emoji.py, gen_jbmono.py, ilbm_decode.py, dist/instssblock.sh
Patina compiles with `-I$(UPCORE)` where `UPCORE ?= $(PATINA)/upcore`, so `#include "config/x.h"` (S2)
needs no edit. A later third repo can lift upcore/ out with no include change.
Why copy, not move: these are terminal code (keymap_act, upactive, pubscr, upconf, prefs) with many vtcon
users; moving them would put vtcon's handler in another repo. Why copy, not third repo: see DECISION 1.
How drift is caught (all on the vtcon side, which may depend on patina):
- vtcon tools/patina-upcore.txt lists every copied path; tools/patina_sync.py copies them from the vtcon
  tree into $(UPTERM_ROOT)/patina/upcore and prints a diff stat (the patina commit is made by hand).
- vtcon test tests/test_patina_upcore.py compares each listed file with the pinned patina source
  (build/patina/<rev>/src/upcore/<path>). In `make test` drift prints WARN lines. In dist-check it FAILS
  for contract files: handler/keymap_act.h, handler/menu_ids.h, handler/tabprog.h, config/upassign.h,
  config/upconf*.{c,h}, config/keymap*.{c,h}, prefs/settings_def.*, prefs/prefs_core.*, dist/keymap
  (DECISION 2). Other files may drift: patina binaries are static, so drift there changes no contract.
- Patina never reads anything outside its tree; upcore/README.txt only says where the copy came from.

### 2.3 Patina gets its own copy of the vtcon.mk parts it needs (moves into patina's Makefile)
VBCC cfg write, VC, CPU, `NDK ?= $(PATINA)/vendor/ndk-3.2r4-Include_H` (own vendor/, gitignored,
same hand install as vtcon; CI does not need it), its own GITREV/patina_version.h, tools/upkgui.rev
(own copy of the upterm-ports pin; vtcon test compares the two rev files).

### 2.4 STAYS in vtcon
- the 36 tools/rig/patina_* files and tools/test_rig_patina_conf.py (DECISION 6: kit integration tests)
- the 19 other rig files, kit-wide tests (dist_manifest, dist_addons, dist_stack, make_link_lists), the
  vtcon halves of the S4-split tests, base install.dos patina lines (D12), packages.txt, pkgs.py,
  gen_dopus_*, thoughts/ (D16), the vtcon history of everything (D4)

### 2.5 What vtcon then depends on
vtcon depends on patina at a tag: tools/patina.rev holds `tag`, `commit`, and sha256 of up-patina.lha and
the ipk. vtcon takes:
- source: `git -C $(UPTERM_ROOT)/patina archive <commit>` into $(MAIN_CHECKOUT)/build/patina/<commit>/src
  (the upkgui.rev pattern, shared by all worktrees). upprefs, vttest_host, flat-budget and the amconf users
  compile the 2.1 cluster from there with `-I$(CURDIR) -I$(PATINA_SRC)` (vtcon headers win for upcore paths).
- binaries: up-patina.lha and the ipk from the spotUP/patina release <tag>, sha256-checked, into
  $(BUILD)/dist (DECISION 7). dist-check and the kit tests unpack the lha. Fallback with no network:
  `make -C $(PATINA_SRC) dist-patina` (works, since patina needs no vtcon).

## 3. Steps S5-S10 for this model

All builds: `~/.claude/bin/buildq --purpose "<session>: <task>" --result <scratch>/<task>.rc -- nice -n 19 make <target>`,
one build per app. Each step's gate runs only what it reaches; the last commit of S8 and S9 also runs full
`make test` + `make dist` + dist-check (stated reason: global build change).

- **S0b Housekeeping (vtcon)**, unchanged: commit both plan files, delete the stray `--help`, merge or park
  the 5 worktrees touching patina/ (welcome-border, wt-c8scroll, wt-combo, wt-palleft2, wt-prefsflat2;
  owner D4), freeze notice for patina/ and the 2.1 cluster in the ledger.
- **S5 patina.mk in vtcon, already stand-alone shaped.**
  - Do: move the patina blocks (old plan 1.1) into patina.mk. Every path in it is `$(PATINA)/...` or
    `$(UPCORE)/...`; its toolchain section (2.3) is self-contained. vtcon's Makefile does
    `include patina.mk` with `PATINA=$(CURDIR) UPCORE=$(CURDIR)`. Write tools/patina-move-set.txt (2.1),
    tools/patina-upcore.txt (2.2), tools/patina-paths.txt (move set + `--follow` earlier names + amarchy/).
    Check the amconf gate (2.1).
  - Gate: `grep -n 'VTCON\|vtcon\.mk' patina.mk` prints nothing; patina_host suites + flat_core, keymap,
    amconf, gthemes, settings_def, prefs pass; `make amiga`; `make dist-patina` `lha l` list unchanged;
    `git log --oneline -- $(cat tools/patina-paths.txt) | wc -l` >= 870.
- **S6 Create the repo (scratch clone, old plan section 3 method).**
  - Do: `git clone --no-local` vtcon into scratch; `git filter-repo --paths-from-file tools/patina-paths.txt`
    (the 2.1 cluster is in the list, so its history comes along; upcore/ files are not filtered in, they
    are copies). Then 3 commits: `git mv patina.mk Makefile` + VERSION; `python3 tools/patina_sync.py`
    output as upcore/ ("Copy the shared UP-Term sources Patina compiles"); .gitignore vendor/ build/.
    Join origin/main with `git merge --allow-unrelated-histories` (owner D2, no force-push).
  - Gate (stand-alone proof): clone the result to scratch patinasplit2/alone/patina, a directory with no
    vtcon sibling, and with UPTERM_ROOT pointing at that empty parent: `make test` passes and
    `grep -rn vtcon Makefile tools tests .github` has no build input hit. Commit count >= 870;
    `--follow patina/patina.c` reaches 2026-10-22; origin/main README commits still present.
    Push only after this gate and an owner go (DECISION 5).
- **S7 Patina CI and version (patina repo).**
  - Do: .github/workflows/test.yml on ubuntu-latest: checkout patina only, pip install fontTools,
    fetch the icon fonts the generators need (unverified which), `make test`. No vtcon checkout, no
    token. Add patina_version.h generated at parse time from VERSION + patina git rev, used in the $VER of
    all 8 programs (DECISION 4). Own dist/package.txt holds the up-patina row; patina is its authority.
  - Gate: CI green on first push; `make amiga` locally (NDK in patina/vendor) builds 8 programs;
    patina-menu links libupkgui from the own upkgui.rev; `Version` shows patina VERSION.
- **S8 vtcon consumes patina (vtcon).**
  - Do: tools/patina.rev (2.5); targets `patina-src` (archive + stamp) and `patina-assets` (download +
    sha256); upprefs, vttest_host and flat-budget take the 2.1 cluster from $(PATINA_SRC);
    tests/test_patina_upcore.py (drift, 2.2); test that packages.txt up-patina row equals the pin's
    dist/package.txt row; rig.py, stage_rig and a1200_drift read PATINA_BUILD (the unpacked release tree).
  - Gate: up-patina from the release and from the in-tree build have the same `lha l` list;
    tests/test_dist_ipk.py passes; upprefs builds from the pin.
- **S9 Remove patina from vtcon.**
  - Do: `git rm` the move set, including the 2.1 cluster; drop the patina.mk include and the patina
    programs from `amiga:`; dist-check runs the drift test strict; consumes becomes `patina,vtcon`.
    Copies stay in vtcon as the owner's files (they are vtcon code).
  - Gate: `make test` (vtcon only); `make dist` + dist-check; upkg-index.txt lists up-patina;
    `rig.py install` plus one patina rig smoke row (purpose "up-term-b3: patina split smoke"); base kit
    installs and boots on a rig (owner eye check, rig and screen named then).
- **S10 Release flow.**
  - Patina: bump VERSION, commit, tag vX.Y, `make dist-patina` locally, `gh release create vX.Y -R
    spotUP/patina --draft` with up-patina.lha + ipk, check 2 assets, then `--draft=false --latest`.
  - vtcon: one commit bumps tools/patina.rev (tag, commit, 2 sha256), the packages.txt row, and, if the
    drift test warns, a `patina_sync.py` round goes into patina first. The vtcon release still carries
    up-patina.lha + ipk as 2 of its 44 assets, because dist/upkg.conf serves
    github.com/spotUP/vtcon/releases/latest/download (DECISION 7).
  - Recipe changes (memory vtcon-release-recipe): step 0 new: patina.rev names a published patina tag;
    step 1: `make amiga` no longer builds patina, `make patina-assets` before `make dist`; check the
    patina $VER shows the tag with no -dirty; asset count stays 44.
  - Close S95-EXTRACT as superseded.

## 4. Risks
- R1 Copy drift: 164 commits/month touch the shared files. Contract drift fails dist-check, so a vtcon
  release cannot ship a patina built on an older keymap/upconf/settings contract. Non-contract drift is
  allowed and listed as WARN.
- R2 Co-edits (43 of 164) become two-repo changes: vtcon commit, patina_sync.py, patina commit + tag,
  vtcon pin bump. Order: vtcon first, patina second, pin third.
- R3 The 2.1 cluster now lives in patina: an upprefs change that needs a core change goes through a
  patina tag. That is the price of the one-way rule; it touched 6 files, measured low vtcon use.
- R4 filter-repo path list wrong: caught by the >= 870 commit gate; the clone is scratch.
- R5 spotUP/patina is public with a Latest v1.0: push only after the S6 stand-alone gate and a go.
- R6 Disk: scratch clone about 150 MB, plus build/patina/<commit>; delete the clone after push.

## 5. Not checked
- amconf_* ownership (2.1 gate), and which fonts CI must download for gen_lucide_icons/gen_menu_icons.
- Whether upkg/upkgui can read two feeds (only matters for DECISION 7 option b).
- Nothing was built or run; the S6 stand-alone gate is the first real proof.

## 6. Owner decisions
- DECISION 1: Model for the shared code | options: (a) move the 16-file Patina look/config cluster to patina, copy the other ~80 shared files into patina/upcore with a vtcon-side drift test; vtcon pins patina; (b) a third repo spotUP/upcore holding all 98 shared files, pinned by both vtcon and patina; (c) copy all 98 into patina, move nothing | recommended: (a) patina is stand-alone, vtcon's handler stays in vtcon, no third repo/CI/pin to run; (b) puts vtcon's terminal code in another repo and every vtcon worktree spans two repos
- DECISION 2: Drift test strictness | options: (a) WARN in make test, FAIL in dist-check for the contract files only; (b) FAIL make test on any drift | recommended: (a) 164 commits/month would block daily work under (b), and only contract files can break users
- DECISION 3 (old D3): Patina VERSION at the first release from the new repo | options: (a) 1.1; (b) 2.0 to mark the stand-alone repo | recommended: (a) v1.0 is published, nothing user-visible changes
- DECISION 4 (old D7): Version stamp in patina binaries | options: (a) generated patina_version.h (VERSION + patina git rev) in all 8 programs' $VER; (b) keep the 4 hard-coded $VER strings | recommended: (a) the only way to prove a kit ships the tagged patina
- DECISION 5 (old D9): Separate owner go for the S6 public push and each S10 release | options: (a) yes, each is its own approved step; (b) one approval for S6-S10 | recommended: (a) the repo is public with a Latest release
- DECISION 6 (was 11-24 D9): Where the 36 patina rig scripts live | options: (a) stay in vtcon as kit integration tests, reading the unpacked patina release; (b) move to patina with copies of 6 rig modules (1793 lines) | recommended: (a) the rig boots the whole UP-Term kit, which is vtcon's harness; (b) adds a large copy that drifts
- DECISION 7 (was D8): Release feed and where vtcon gets up-patina | options: (a) vtcon release keeps serving up-patina (44 assets); vtcon downloads the 2 assets from the patina release by tag and sha256; (b) add a second `src/gz` feed in dist/upkg.conf for spotUP/patina releases, so patina reaches users without a vtcon release (needs a kit update and an upkgui two-feed check) | recommended: (a) now, no client change; (b) later if patina releases outpace kit releases

## Owner answers (2026-11-27 00:xx)

- DECISION 1: owner: "patina should still be able to set the themes etc in up-term/vtcon but it needs to be a separate standalone app you probably know how to do this best". Chosen: (a) move the 16-file look/config cluster to patina, copy the ~80 others into patina/upcore, vtcon pins patina. Patina keeps setting UP-Term themes through the contract files (upconf, settings, prefs, keymap, menu ids) that vtcon reads; those are the files the drift test fails on in dist-check.
- DECISION 2: (a) warn in make test, fail in dist-check for contract files only.
- DECISION 3: (a) 1.1.
- DECISION 4: (a) generated patina_version.h in all 8 programs.
- DECISION 5: (a) separate owner go for the S6 public push and each S10 release.
- DECISION 6: (a) rig scripts stay in vtcon.
- DECISION 7: (a) vtcon release keeps serving up-patina (44 assets), fetched by tag + sha256.
- Earlier: D2 merge no force-push; D4 merge or park the 5 patina worktrees before S5.
