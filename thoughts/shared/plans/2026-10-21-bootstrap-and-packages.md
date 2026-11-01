---
date: 2026-10-21
topic: Minimal bootstrap archive plus ipk packages for everything else; one full archive installed offline by upkg
status: reviewed
---

# Bootstrap and packages

Sources: `thoughts/shared/research/2026-10-21_bootstrap-and-packages.md` (part inventory, upkg runtime needs,
sizes), `thoughts/shared/research/2026-10-21_upkg-upgrade-base.md` (Update path, upupdate, in-use files),
`thoughts/shared/plans/2026-10-13-kit-split.md` (D1-D8, the per-archive install.dos / record / handed machinery
this plan reuses). Repos: V = /Users/spot/Code/UP-Term/vtcon, P = /Users/spot/Code/UP-Term/upterm-ports.

## Owner decisions (2026-10-21, fixed)

1. Bootstrap archive = vtcon-handler, up-console.device, vsh, upkg + upkg-ui, curl, the lha capability, libs
   (ixemul), AmiSSL (installed only when absent or older), plus what upkg needs to run: awk, head, rm, ixemul.
2. Everything else is an ipk in the feed.
3. Full install = one archive: bootstrap plus the default ipks, installed offline by upkg from the archive.
4. The base ships lha: bsdtar if it reads lha on the Amiga (spike S1), else a lhasa port.
5. Core upgrade runs the archive's own Installer in Update mode, interactive, reboot after.
6. Reverses kit-split answer 1 ("Userland: whole in the base") and RULES.md:25 ("the base is not a package").

## Finish line

Done = all checklist items B1..B49 ticked, the three reachability rig tests (R1 bootstrap + typed `upkg install`,
R2 full archive offline, R3 core upgrade over 1.5) pass with their sentinels and asserted numbers, `make
dist-check` passes, RULES.md rewritten, and the A1200 manual list is handed to the owner under "Blocked on me".

## Decisions made in this plan (owner may veto)

Owner review 2026-10-21: V1 (one ipk per port plus up-default), V2 (gg coreutils optional as up-coreutils-gg), V5 (`upkg core`) and the 20 MB unpacked bootstrap ceiling are confirmed. The other V items stand as written; none was vetoed.

- V1. Userland splits one ipk per ports package (`up-<p>`, e.g. `up-coreutils`, `up-nano`, `up-jq`), staged by
  the ports build itself (`kit-stage.sh --per-package`), not by hand-written globs. A meta-package `up-default`
  (no files) carries the default set in its Depends. Alternative vetoed here: a few hand-grouped ipks (editors,
  archivers, ...), which needs a second table to keep in step with the ports.
- V2. The gg coreutils 5.2.1 copy (copy-coreutils part) is dropped. Its four unique commands (hostname, kill,
  pinky, who) move into `up-coreutils-gg` (optional, not default).
- V3. Bootstrap-owned files are never in any ipk. A ports package that ships a bootstrap command (coreutils: head,
  rm, cat; awk; libarchive: tar; sed) has that file subtracted from its ipk; the bootstrap copy updates only
  with a core upgrade.
- V4. The installed core is visible to opkg as a status-only package `up-term` (Version = UP-Term.version),
  seeded by the bootstrap record step. Every ipk then states `Depends: up-term (>= <needs-base>)`; opkg enforces
  it natively. The ipk preinst needs-base check stays only for the transition (removed in B27 once 1.6 is the
  oldest base the feed serves).
- V5. Core upgrade is a new upkg subcommand `upkg core` (patch pattern of P patch 0016): check the feed's
  `up-term` core stanza against UP-Term:UP-Term.version, download up-term.lha with curl, verify sha256 with
  libopkg, extract with libarchive, then start the kit's Installer. `upkg upgrade` and upkg-browse only announce
  "UP-Term <ver> is out: type upkg core". Nothing is copied over the core outside the Installer.
- V6. Add-on lha archives (up-nvim, up-python, up-pi, up-claude) keep shipping unchanged in this plan. Retiring
  them is a separate decision.
- V7. The Installer option pages for UNIFONT, EMOJI, SSH, BEBBOGET, WASABI, SERIAL become one "Packages" page of
  checkboxes generated from packages.txt (default column ticked). CONSOLE, DEVICE, SHELLICON, CPU040/CPU060 and
  CURL variants stay Installer options of the bootstrap.
- V8. The bootstrap-only archive installs no ipks; its last page says: type `upkg install up-default`. Only the
  full archive (`up-term-full.lha`) runs the packages step.
- V9. `lha` on the bootstrap is read-only (list, test, extract). Creating lha archives is out of scope.

## Phase 0: rig spikes

All spikes on a rig started as 060: `python3 tools/rig/rig.py start --max` (UPTERM_RIG=2 for a second rig;
never stop rig 1). Results go to `thoughts/shared/research/2026-10-2x_bootstrap-spikes.md`, one section per
spike with the verbatim rc and output lines. Stop the rig at the end of the spike session (`rig.py stop`, after
5 s of quiet: never mid-write). The base used is the 1.5 kit from `make dist` installed by
`python3 tools/rig/install_rig.py --only base` (its existing case).

### S1: bsdtar reads up-term.lha on the Amiga

```
python3 tools/rig/ami.py put build/up-term.lha VTC:spike/up-term.lha
python3 tools/rig/ami.py exec "UP-Term:bin/tar tf VTC:spike/up-term.lha >VTC:spike/list.txt" 600
python3 tools/rig/ami.py exec "UP-Term:bin/tar xf VTC:spike/up-term.lha -C VTC:spike/x" 900
python3 tools/rig/ami.py get VTC:spike/list.txt build/spike/list.txt
```

Pass: rc 0 both times; `list.txt` has 1504 entries, equal to `bsdtar tf build/up-term.lha | wc -l` on the host;
then on the host, every MANIFEST line of `VTC:spike/x/up-term/Files/MANIFEST` matches the extracted file's size
and crc (`python3 tools/mkmanifest.py --check <dir>`: new flag, B6, reused by R2).
- Pass: the lha capability is bsdtar, shipped as UP-Term:bin/tar plus a vsh wrapper `UP-Term:bin/lha`
  (x / e / l / t onto `tar xf / xf / tvf / tf`). upkg core extracts with its static libarchive.
- Fail (no lha reader compiled in, or wrong content): first check `P/pkgs/libarchive/recipe.mk` flags and the
  configure log for `ARCHIVE_READ_SUPPORT_FORMAT_LHA`; if the reader is simply not linked, fix the recipe (root
  cause) and rerun S1. If it is compiled and still fails on the 68k, port lhasa as `P/pkgs/lhasa` (ISC: confirm
  COPYING says ISC before porting, B7) and ship its `lha` binary; upkg core then runs `lha x` instead of the
  libarchive call.

### S2: `upkg install <local .ipk>` on the 68k build

```
python3 tools/rig/ami.py put build/ipk/up-claude_1.1_m68k-amigaos.ipk VTC:spike/up-claude.ipk
python3 tools/rig/ami.py exec "UP-Term:bin/sh -c 'upkg install /VTC/spike/up-claude.ipk'" 900
python3 tools/rig/ami.py exec "UP-Term:bin/sh -c 'upkg list-installed'" 60
```

Pass: rc 0; `up-claude` listed; `UP-Term:pkg/upkg/info/up-claude.list` exists; the add-on record
`ENVARC:upkg/up-claude/MANIFEST` exists (proves postinst ran). Also try the AmigaDOS form `VTC:spike/up-claude.ipk`.
- Pass: B33 uses the form that passed.
- Fail: the root cause is in libopkg path handling (`opkg_prepare_url_for_install`, P build
  `libopkg/opkg_download.c:481`) or the short-name patches 0003/0004/0007. Fix it as a new P patch with a host
  test of the path parser, rebuild, rerun S2. No wrapper script.

### S3: `file:` feed on the 68k build

Write `VTC:spike/off.conf` = dist/upkg.conf through `upkg_rig.conf_for(conf, 'file:///VTC/spike/feed')` (and,
second try, `file:VTC:spike/feed`). Put build/ipk/{Packages,Packages.gz,*.ipk} into VTC:spike/feed.

```
python3 tools/rig/ami.py exec "UP-Term:bin/sh -c 'upkg -f /VTC/spike/off.conf update'" 300
python3 tools/rig/ami.py exec "UP-Term:bin/sh -c 'upkg -f /VTC/spike/off.conf install up-pi'" 900
```

Pass: rc 0 both; the network is unplugged for the run (rig with no bsdsocket started: `ami.py exec "Status"`
shows no stack; or curl renamed for the run) so curl cannot be the transport; `up-pi` listed installed.
- Pass: the full archive's packages step writes that src form (B30).
- Fail: same root-cause rule as S2 (`opkg_download_file`, `opkg_download.c:136-140`); fix in a P patch.

### S4: which userland commands the bootstrap needs

On the 1.5 install, rename `UP-Term:bin` to `UP-Term:bin.all`, make a new `UP-Term:bin` holding only the
candidate set (vsh as sh, upkg, upkg-ui, upkg-ask, upkg-browse, curl, tar, awk, head, rm, sed, cat), then:

```
python3 tools/rig/ami.py exec "UP-Term:bin/sh -c '. /ENVARC/up-term/vshrc' >VTC:spike/rc.txt" 60
python3 tools/rig/ami.py exec "UP-Term:bin/sh -c 'upkg -f /VTC/spike/off.conf install up-claude up-pi' >VTC:spike/pk.txt" 900
python3 tools/rig/ami.py exec "UP-Term:bin/sh -c 'upkg remove up-claude up-pi' >>VTC:spike/pk.txt" 300
python3 tools/rig/ami.py exec "UP-Term:bin/sh -c 'upkg-browse --list' >>VTC:spike/pk.txt" 120
```

Also open a window with `UP-Term:UPTerm` and check the prompt and `tutorial` hint render with no themes drawer
and without hl/ls (the vshrc aliases `cat='hl -p'` and `ls --color`). Host side first: list every external
command word in dist/vshrc, dist/upkg-ask, dist/upkg-browse, dist/tutorial and the `_scripts` output of
tools/pkgs.py (B8 writes `tools/cmdwords.py` for this; reused by B23's test).
- Pass: no "not found" / "unknown command" line in rc.txt or pk.txt; rc 0. The candidate set is the bootstrap.
- Fail: each missing command is added to the bootstrap list in dist/packages.txt (B11) and S4 reruns; a missing
  `hl`/`ls`/theme in vshrc is fixed in vshrc with a `command -v` guard (B22), not by adding the program.
  Restore `UP-Term:bin.all` afterwards (Rename back; never `Delete` the drawer).

### S5: upupdate without the Installer

```
python3 tools/rig/ami.py exec "VTC:kit16/Files/upupdate UP-Term:MANIFEST VTC:kit16/Files/MANIFEST VTC:kit16" 300
python3 tools/rig/ami.py exec "GetEnv UPTUPDKB" 10
```

Pass: rc 0, one summary line, ENV:UPTUPDKB set, T:UPTUPD-<part> scripts exist. (upupdate.c:1-22 says it only
writes env vars and T: scripts, so this is expected to pass; the spike confirms it on a 1.5 to bootstrap diff with
handed lines.)
- Pass: `upkg core` prints the update size ("Update copies N KB") before it starts the Installer (B37).
- Fail: `upkg core` starts the Installer without a size line. The Installer path itself is unchanged in both.

### S6: upkg run from the Installer's packages step

Build a throwaway kit (`make dist-full`, B29) and run its Install through `tools/rig/installer_rig.py` twice:
on a clean rig (`rig.py start --max --os 31 --fresh`), and over the 1.5 install without a reboot in between
(old ixemul.library resident).
- Pass: in both runs the packages step's upkg rc is 0 and `upkg list-installed` lists `up-default` and its
  Depends. The packages step runs inline.
- Fail on the over-1.5 run only: the packages step copies Files/ipk to `UP-Term:pkg/upkg/offline/`, writes
  `UP-Term:etc/upkg-offline.conf`, and the Installer's last page says to reboot and type
  `upkg -f UP-Term:etc/upkg-offline.conf upgrade`. R3 then asserts that page and the typed command.
- Fail on the clean run: blocker; root cause before going on (ixemul or assign order in install.dos).

## Package split table

Sizes are staged KB of the 1.5 build (research doc 1); "est." marks a sum not yet measured from a build. B12
measures every row with tools/kitsize.py and writes the numbers into dist/packages.txt comments.

| Part today (install.dos label) | Goes to | Default | Depends | KB |
|---|---|---|---|---|
| drawer (assign, User-Startup, reassign.sh) | bootstrap | - | - | <10 |
| system: vtcon-handler, pty-handler, XCON, PTY, unstartup.sh, termcap.vtcon, terminfo, up-term.conf, upkg.conf, vsh, ixkill, UP-Term Prefs | bootstrap | - | - | est. 900 |
| system: themes | up-themes | default | up-term (>= 1.6) | 452 |
| system: tmux, tmux.conf | up-tmux | default | up-term (>= 1.6) | 1072 |
| system: screen, screenrc | up-screen | default | up-term (>= 1.6) | 472 |
| copy-coreutils (gg 5.2.1) | dropped; hostname kill pinky who to up-coreutils-gg | optional | up-term (>= 1.6) | est. 120 |
| copy-userland: upkg, upkg-ui, upkg-ask, upkg-browse | bootstrap | - | - | 872+ |
| copy-userland: tutorial | bootstrap (vshrc hint points at it) | - | - | <10 |
| copy-userland: awk, head, rm, sed, cat, tar (+ S4 additions) | bootstrap | - | - | est. 1600 |
| copy-userland + userland (rest of bin, share, etc, licenses) | one `up-<p>` per ports package (V1) | per row in packages.txt | up-term (>= 1.6) + recipe runtime deps | 28488+6692+312+672 total |
| copy-unifont | up-unifont | default | up-term (>= 1.6) | 2612 |
| copy-emoji | up-emoji | default | up-term (>= 1.6) | 532 |
| tools: VERSIONS, UP-Term.version, UPTerm | bootstrap | - | - | <100 |
| tools: sz rz hl mdv ps top uptelnet UPDemo net | up-tools | default | up-term (>= 1.6) | est. 400 |
| tools: upgetty + boot SERIAL block | up-serial | optional | up-term (>= 1.6) | <100 |
| tools SSH: bebbossh*, libcryptossh.library (060/020) | up-ssh | default | up-term (>= 1.6) | 788 |
| tools BEBBOGET | up-bebboget | default | up-term (>= 1.6) | 532 |
| tools CURL: curl.060/040/020 | bootstrap (all three; CPU option picks) | - | - | 3500 |
| fonts | bootstrap | - | - | 44 |
| unix: sh, GG:, TMP: assigns | bootstrap | - | - | <10 |
| desktop: icons, SHELLICON | bootstrap | - | - | <100 |
| boot: vshrc, banner | bootstrap | - | - | <20 |
| boot: wasabid + key | up-wasabi | optional | up-term (>= 1.6) | 352 |
| libs: ixemul/ixnet patched | bootstrap | - | - | 216 |
| mount: ixpipe-handler, IXPIPE | bootstrap | - | - | <50 |
| console: UPConsole, up-console.device | bootstrap | - | - | <100 |
| AmiSSL (Install-AmiSSL, P_AMISSL) | bootstrap (installed only when absent/older) | - | - | 9728 |
| (new) up-default | meta, no files | - | every default row above | 0 |
| existing nvim git perl vim claude pi python | unchanged ipks | optional | `up-term (>= 1.0)` from needs-base | as today |

Bootstrap estimate: about 17 MB unpacked, AmiSSL 9.7 MB of it; B12 replaces the estimate with the measured sum
and test_dist_kitsize asserts it under a stated ceiling (owner sets the number at B12 review; plan default
20 MB unpacked).

## Data model

### dist/packages.txt (D1 stays the single table)

New columns after `consumes`: `default` (`yes`/`no`), `depends` (comma list, opkg syntax), `part` (the kit
parts.txt part the row's files come from, or `ports:<p>` for a per-ports-package row). New rows: one per split
package. The bootstrap row keeps id `base`, archive `up-term`, record `UP-Term:`. Per-ports rows are not
hand-written: `tools/pkgs.py rows()` expands one `userland` row into `up-<p>` rows from
`build/kit-pp/<p>/` (the per-package stage, B4); the default flag for those comes from a short
`dist/userland-default.txt` (one ports package name per line; everything else optional).

```
# id      archive      version needs-base ixemul record   handed consumes default depends part
base      up-term      1.6     -          -      UP-Term: -      ...      -       -       -
tmux      up-tmux      1.6     1.6        80     -        -      tmux-amiga yes   -       system:tmux
unifont   up-unifont   1.6     1.6        -      -        -      -        yes     -       copy-unifont
ssh       up-ssh       1.6     1.6        -      -        -      -        yes     -       tools:ssh
wasabi    up-wasabi    1.6     1.6        -      -        -      -        no      -       boot:wasabi
userland  up-          1.6     1.6        80     -        -      upterm-ports per  -       ports:*
default   up-default   1.6     1.6        -      -        -      -        -       @default -
```

`@default` expands to every row with default `yes`. Depends always gets `up-term (>= <needs-base>)` prepended
by pkgs.py; a ports row also gets `up-<dep>` for each runtime dependency that recipe.mk names (`<p>_DEPENDS`,
B5).

### Ownership invariant (test_dist_split, B13)

Every file under the staged base kit belongs to exactly one of: bootstrap MANIFEST, one ipk's data, or the
"not installed" list in parts.txt (`-`). No file in two; none in neither. Counted from the builders
(mkmanifest.manifest + pkgs.build_ipk file lists), not from a grep.

## Per-package install, uninstall and record (reuse of kit-split)

- Each split ipk is built exactly as today's add-on ipks (pkgs.py `build_ipk`, `_scripts`): data stages into
  `T/upkg-stage/<archive>/`, postinst runs `Execute T:upkg-stage/<archive>/Files/install.dos`, prerm runs the
  record's Uninstall.
- The install.dos of a split package is assembled by tools/mkinstaller.py (D4 `; @include`, `@NAME@`) from:
  `dist/common/@CHECKFRAG@` (check.dos), one copy fragment, and `dist/common/record-addon.dos`. Copy fragments
  are extracted from the monolithic dist/install.dos into `dist/common/part-<label>.dos`, one per label
  (system-tmux, system-screen, system-themes, copy-unifont, copy-emoji, tools-ssh, tools-bebboget, tools-tools,
  boot-wasabi, boot-serial, userland-copy). The monolithic install.dos then includes the bootstrap fragments
  only, so each copy body exists once (single source).
- Per-ports rows share one template `dist/common/userpkg.dos` (copy `Files/userland` to `UP-Term:`) and a
  generated uninstall body (tools/slim.py `uninstall` generalised: B16), the way P9 slim did for git/perl/vim.
  dist/slim.txt and `slim.py remove` are replaced by the per-package stage (B17).
- Options an ipk needs (CPU060 for libcryptossh, SERIAL device name) come from `ENVARC:up-term/opts`, which the
  bootstrap record writes today (D3); the fragment reads `ENV:UPTOPT<keyword>` exactly as the STEP path does
  (install.dos:28-100).
- Boot blocks (wasabi, serial) are User-Startup blocks named per package; uninstall removes the block with
  `unstartup.sh <name>` (D6).
- Record: each ipk keeps an add-on record drawer (D3: MANIFEST, VERSIONS, opts/, Uninstall copy) under
  `UP-Term:pkg/<id>/`. The bootstrap record (dist/common/record.dos) additionally:
  - writes `UP-Term:pkg/lastinstall` = `mode <install|update> <version>` (sentinel for R1/R3);
  - seeds the opkg status stanza `up-term` (V4) with `dist/common/seed-status.awk` run by vsh:

```
# seed-status.awk: replace or append the up-term stanza in upkg's status file
# usage: awk -v ver=1.6 -f seed-status.awk UP-Term:pkg/upkg/status > T:status.new
BEGIN { RS=""; ORS="\n\n" }
$0 !~ /^Package: up-term\n/ { print }
END { print "Package: up-term\nVersion: " ver "\nStatus: install user installed\nArchitecture: m68k-amigaos" }
```

  (record.dos then `Copy T:status.new UP-Term:pkg/upkg/status`; exact status path checked in B25 against
  dist/upkg.conf.)

## Full archive

`up-term-full.lha` = the bootstrap kit plus `Files/ipk/` (every default ipk and its Depends closure,
`Packages`, `Packages.gz`). Same Install, same install.dos; the new last label `packages` runs only when
`<kit>/Files/ipk/Packages` exists:

```
Lab packages
If NOT EXISTS "<DIR>/ipk/Packages"
  Skip done
EndIf
Echo "src/gz offline file:<FORM-FROM-S3>" >T:upkg-offline.conf   ; plus the dest/lists lines of upkg.conf
UP-Term:bin/sh -c "upkg -f /T/upkg-offline.conf update && upkg -f /T/upkg-offline.conf install $UPTPKGS"
```

`UPTPKGS` is the package list from the Installer's Packages page (V7), or on an Update the list derived from
`ENVARC:up-term/opts` plus every `up-*` record drawer already present (migration below). The Installer's
P_STEP list gains `packages` after `record` (Install.installer:553). The S6 fail branch replaces this label's
body as described there.

## Migration of a monolithic 1.4/1.5 install

- The 1.6 bootstrap MANIFEST carries a `handed` line (D7 form `handed 0 00000000 handed#Files/<path>`) for every
  kit path that 1.5 installed and 1.6 hands to an ipk. tools/mkmanifest.py `handed_paths` gets them from the
  split table (rows with a `part`), not from a list. upupdate leaves those installed files alone and sets
  UPTHANDED; the record step copies the old MANIFEST to `UP-Term:pkg/legacy` (D7, existing).
- Adoption = installing the ipks. Each split ipk's install.dos finds the legacy record and runs
  `upupdate UP-Term:pkg/legacy/MANIFEST <kit>/Files/MANIFEST <kit> ONLY=<its handed paths>` (D7, existing
  addon path), so only its own changed files copy; opkg records ownership in `info/<pkg>.list`, which seeds
  upkg status. Full archive: the packages step adopts in the same Install. Bootstrap-only archive: files stay in
  place and working; the last page says `upkg install up-default` (plus the optional ones from opts) adopts them.
- The adopt list on Update: `up-default` plus `up-ssh` if opts has SSH, `up-bebboget` BEBBOGET, `up-wasabi`
  WASABI, `up-serial` SERIAL, `up-unifont` UNIFONT, `up-emoji` EMOJI; a 1.5 install whose opts lacked
  UNIFONT/EMOJI does not get them (opts decide, not the default flag).
- gg coreutils files from 1.4/1.5 that V2 drops: listed as gone in the 1.6 MANIFEST diff, deleted by the Update
  as any gone file is today, except hostname kill pinky who, which are handed to up-coreutils-gg and kept.
- Uninstall: the bootstrap Uninstall runs `upkg remove` for every installed `up-*` package first (prerm runs
  each record's Uninstall), then the legacy record's Uninstall (record-legacy.dos, existing), then its own
  MANIFEST. Bootstrap files are never in a package list (B13), so no double delete.

## Core upgrade

- The feed gains a core stanza in `Packages`: `Package: up-term-core`, `Version: <base version>`,
  `Filename: up-term.lha`, `SHA256sum`, `Size`, `Description: UP-Term core (Installer Update)`. pkgs.py `feed`
  writes it from the `base` row; build/upkg-index.txt is dropped (no consumer, research doc 2).
  opkg must not try to install it: it carries `Architecture: core`, which no `arch` line in upkg.conf names
  (verified in B34 test with opkg's arch filter).
- `upkg core` (P patch, pattern of 0016: command table row, help row, env override `UPKG_CORE_URL`):
  1. read the feed's `up-term-core` stanza; compare with `UP-Term:UP-Term.version`; equal or older: print
     "UP-Term <ver> is current." rc 0;
  2. `curl` the lha into `T:upkg-core/` (needs 2-3x its size free: check with the same free-space call the
     downloads use, abort with the number if short);
  3. verify sha256 with libopkg's `file_sha256sum_alloc`;
  4. extract with libarchive (or `lha x` on the S1 fail branch);
  5. S5 pass: run upupdate dry and print "Update copies N KB";
  6. start the Installer on the kit: `Run >NIL: SYS:Utilities/Installer T:upkg-core/up-term/Install
     APPNAME "UP-Term"` (exact form checked in B36 on the rig); the Installer offers Update (ENVARC opts) as for
     any older base, ends with its reboot request. upkg exits rc 0 after the Installer started.
- `upkg upgrade` and upkg-browse print/show "UP-Term <ver> is out: type upkg core" when the stanza is newer
  (upkg-browse's existing base-stale line, upkg_browse_rig.py:67-72).
- In-use files (upkg, vsh, vtcon-handler, ixemul) are replaced by the Installer's copy, which already handles
  them for an Update today; the reboot request is the existing one.

## Release and test rework

- Makefile:
  - `dist-base` stages the bootstrap only (bootstrap parts), `dist-split` stages each split package into
    `build/kit-<id>/` (ADDON_STAGE macro), per-ports rows via `P/tools/kit-stage.sh --per-package
    build/kit-pp`;
  - `dist-ipk` builds every ipk (split + existing add-ons + up-default) and the feed with the core stanza;
  - `dist-full` = bootstrap kit + `Files/ipk/` from dist-ipk's default closure, archived as up-term-full.lha;
  - `dist` = dist-base + dist-split + addons + dist-ipk + dist-full + dist-src (dist-index removed);
  - `dist-check`: per-archive loop also covers up-term-full.lha (requires Files/ipk/Packages); runs
    test_dist_split, test_dist_depends, test_dist_full; drops the index cmp.
- RULES.md: line 25 and the dist-ipk row ("the base is not a package") rewritten to "the base is a bootstrap;
  everything else is an ipk; the full archive is the bootstrap plus the default ipks". Release steps
  (RULES.md:101-137): step 7 uploads `build/up-term.lha build/up-term-full.lha build/up-*.lha build/ipk/*.ipk
  build/ipk/Packages build/ipk/Packages.gz` (no upkg-index.txt). New step: `upkg core` on the rig against the
  release feed before the upload (R3 is that check).
- Readmes: `dist/aminet/up-term.readme.in` says "bootstrap: the console, vsh, upkg; type `upkg install
  up-default` for the full set, or take up-term-full.lha"; new `dist/aminet/up-term-full.readme.in`;
  tools/mkreadme.py lists the default package set from packages.txt.
- Release notes wording (template in RULES.md release step 6): "UP-Term is now a small core plus packages. Over
  1.5: run Install, choose Update, reboot; your tools stay and become packages."
- Tests to change: test_dist_installer (Packages page choices from packages.txt), test_dist_kitsize (bootstrap
  ceiling), test_dist_addons (split rows), test_dist_ipk (Depends, core stanza), test_dist_slim (replaced by
  test_dist_split), test_dist_manifest (handed lines from split table), test_dist_packages (new columns),
  test_upkg_ask / test_rig_browse_base (core line).
- Rig harness: tools/rig/install_rig.py `case_table` (:1609) gains `bootstrap`, `full`, `core-upgrade`; `base`
  becomes `bootstrap`; `all` installs the full archive; `legacy` keeps its 1.0 kit and adds the adopt step.
  tools/rig/upkg_rig.py `Release`/`conf_for`/`bumped_feed` publish the split ipks; `case_needsbase` asserts
  opkg's Depends refusal text instead of preinst's. upkg_browse_rig.py's older-base install (l.116-134) uses
  the new full archive.

## Reachability tests (one per feature, through the real entry point)

- R1 `tools/rig/bootstrap_rig.py` (case `bootstrap` in install_rig): clean rig, `--os 31 --fresh`; run
  up-term.lha's Install through installer_rig.py with defaults; reboot; open a window with UP-Term:UPTerm; type
  `upkg install up-tmux` (keys via `ami.py type`, not exec) against the host feed (upkg_rig `Release`).
  Sentinel: `UP-Term:pkg/lastinstall` reads `mode install 1.6`; `UP-Term:pkg/upkg/info/up-tmux.list` exists;
  `tmux -V` typed in the same window prints the build's tmux version. Numbers: installed bootstrap file count ==
  MANIFEST file lines of build/kit/Files/MANIFEST (exact); `up-tmux.list` lines == data entries of the built
  ipk (from pkgs.build_ipk); `upkg list-installed` == exactly 2 lines (up-term, up-tmux).
- R2 `full` case: clean rig, network stack not started; up-term-full.lha Install with defaults. Sentinel:
  `T:upkg-offline.conf` was the only conf (its `src/gz` line asserted file:) and curl ran zero times (curl
  replaced for the run by a counting stub that writes `VTC:curl.count`; asserted absent/0). Numbers:
  `upkg list-installed` count == 1 + size of up-default's Depends closure from build/ipk/Packages; every path
  in every `info/*.list` exists with the size in the ipk; total installed file count == bootstrap MANIFEST +
  sum of ipk data entries.
- R3 `core-upgrade` case: install the 1.5 release kit (legacy kit fixture, monolithic) with all options; publish
  a feed with core 1.6; in a UP-Term window type `upkg core`; drive the Installer's Update with
  installer_rig.py; reboot. Sentinel: `UP-Term:pkg/lastinstall` reads `mode update 1.6` and ENV:UPTHANDED was
  non-empty (record writes it into lastinstall's second line). Numbers: UP-Term.version == 1.6; every handed
  file's crc unchanged from 1.5 before adoption; after the adopt step `upkg list-installed` == the adopt list
  derived from the 1.5 opts (exact set); `UP-Term:pkg/legacy/MANIFEST` exists; bootstrap files' crcs equal the
  1.6 MANIFEST.

Unit tests where the behaviour lives: tools tests for pkgs.py rows expansion / Depends / core stanza,
mkmanifest handed from split table, mkinstaller fragment assembly, seed-status.awk on the host with sample status
files (append, replace, empty file), cmdwords.py; the upkg core version compare and stanza parse as a host test
in P if P has a host build of libopkg (B35 checks; otherwise R3 is its only test, said in the B35 entry).

Knobs that must move output (each asserted in a test): packages.txt `default` (changes up-default Depends and
the Installer page ticks), `depends`, `part` (changes which ipk owns a file and the handed lines),
dist/userland-default.txt (changes up-default), Installer Packages page choice (changes UPTPKGS and installed
set: R2 variant with one box unticked asserts that package absent), `UPKG_CORE_URL` (changes the fetched URL).

## Checklist

Phase 0: spikes
- B1 S1 bsdtar reads up-term.lha on the rig; record result and branch taken.
- B2 S2 local .ipk install; branch taken (P patch if fail).
- B3 S3 file: feed; record the working src form.
- B4 S4 bootstrap command set; final list into the research spike doc.
- B5 S5 upupdate headless; branch taken.
- B6 S6 upkg from the Installer step, clean and over-1.5; branch taken.

Phase 1: data model and per-package staging
- B7 `tools/mkmanifest.py --check <dir>` (used by S1 and R2) with a unit test. (Ordered before B1 in practice.)
- B8 `tools/cmdwords.py` + test (used by S4 and B23).
- B9 P: `tools/kit-stage.sh --per-package <dir>` reusing the same staging code path; test in P.
- B10 P: `<p>_DEPENDS` runtime deps in recipe.mk where a binary needs another package's files; recipe-var.sh reads it.
- B11 dist/packages.txt new columns `default depends part`, split rows, bootstrap row; test_dist_packages.
- B12 Measure every split package and the bootstrap with tools/kitsize.py; write sizes; owner confirms ceiling.
- B13 test_dist_split: ownership invariant counted from builders.
- B14 dist/userland-default.txt with the default ports set. Initial list = every ports package 1.5 shipped
  (1.5 was all-in, so the full archive installs the same set); trimming it is a later owner call.
- B15 lha capability per S1 branch (bsdtar + vsh `lha` wrapper, or P/pkgs/lhasa with licence confirmed).

Phase 2: ipk build for split packages
- B16 Extract copy fragments from dist/install.dos into dist/common/part-<label>.dos; install.dos includes them.
- B17 tools/slim.py generalised (uninstall body from per-package stage); dist/slim.txt and `slim.py remove` retired.
- B18 tools/pkgs.py: row expansion, Depends with `up-term (>= x)`, up-default meta, Priority, test_dist_ipk.
- B19 Split-package install.dos / Uninstall / record via mkinstaller; test_dist_assemble and test_dist_addons.
- B20 CPU and SERIAL options read from ENVARC opts in ssh / serial fragments; test.
- B21 V3: bootstrap-owned files subtracted from ipks; asserted by B13.

Phase 3: bootstrap kit
- B22 vshrc guards (`command -v hl`, `ls --color` only when ls exists, themes absent) + test.
- B23 Bootstrap command set from S4 in dist-base; test asserts every cmdwords.py word is a vsh builtin or in the
  bootstrap MANIFEST.
- B24 dist-base stages bootstrap parts only; AmiSSL step unchanged.
- B25 record.dos writes `UP-Term:pkg/lastinstall` and runs seed-status.awk; host test of the awk.
- B26 Bootstrap Installer: Packages page generated from packages.txt (V7); old option pages for those options
  removed; test_dist_installer.
- B27 opkg Depends replaces preinst needs-base for split ipks; preinst check kept for old add-ons until 1.6 is the
  oldest supported base (dated entry in todos.md).
- B28 Bootstrap Uninstall removes installed up-* packages first; test_dist_modes.

Phase 4: full archive
- B29 `make dist-full` builds up-term-full.lha with Files/ipk closure; test_dist_full (closure equals
  up-default Depends closure from Packages).
- B30 install.dos `packages` label with the S3 src form and S6 branch; P_STEP list gains it.
- B31 Packages page choice becomes UPTPKGS; knob test.

Phase 5: migration
- B32 handed lines for split paths from the split table (mkmanifest handed_paths); test_dist_manifest.
- B33 Split ipk install.dos runs the legacy `upupdate ... ONLY=` path (D7) for its handed paths.
- B34 Adopt list from opts on Update; gg coreutils handling (V2); test.

Phase 6: core upgrade
- B35 P patch `upkg core` (version compare, curl download, sha256, extract, dry upupdate per S5, Installer start).
- B36 Rig check of the Installer start form from a shell (feeds B35's string).
- B37 `upkg upgrade` and upkg-browse announce a newer core; test_rig_browse_base / upkg_browse_rig update.
- B38 pkgs.py feed writes the `up-term-core` stanza (Architecture core); upkg-index.txt and dist-index removed;
  opkg never offers it to install (test).

Phase 7: release, rules, docs
- B39 Makefile dist / dist-split / dist-ipk / dist-full / dist-check rework.
- B40 RULES.md:25, Commands dist-ipk row, release steps 1-7 rewritten.
- B41 Readmes: up-term, up-term-full; mkreadme.py default set.
- B42 Release-notes template wording.
- B43 install_rig.py case_table: bootstrap, full, core-upgrade; legacy adopt step.
- B44 upkg_rig.py / upkg_browse_rig.py / upkg_ask_rig.py / userland_rig.py / installer_rig.py moved to the
  split packages; case_needsbase asserts opkg's Depends text.

Phase 8: reachability and manual
- B45 R1 passes (sentinel + numbers).
- B46 R2 passes, plus its one-box-unticked knob variant.
- B47 R3 passes.
- B48 `make dist-check` green; type-check/lint of touched Python (`python3 -m pytest tests/test_dist_*.py`).
- B49 Manual A1200 list handed to the owner (below).

## Automated verification

B7-B13, B16-B19, B21-B23, B25-B26, B29, B31-B32, B34, B38 by `make dist-check` and the named host tests;
B1-B6, B15, B20, B24, B27-B28, B30, B33, B35-B37, B43-B47 by rig runs (`rig.py start --max`), each through
install_rig.py `--only <case>` (resumable, recorded verdicts; never a full sweep without a stated reason).

## Manual verification (A1200, 192.168.0.74, 68020)

1. Fresh: copy up-term.lha, run Install from Workbench, defaults, reboot. Open UP-Term, type
   `upkg install up-default`. Pass: completes; `tmux` and `nano` start. Time it (68020 + network).
2. Full: up-term-full.lha on a second partition or after Uninstall; Install, reboot. Pass: same tools present
   with no network cable.
3. Upgrade: over the owner's 1.5 install, run up-term-full.lha Install, choose Update, reboot. Pass: own
   settings (ENVARC:up-term, vshrc edits) kept; `upkg list-installed` shows the adopted set.
4. `upkg core` from a window once 1.6.1 is in the feed. Pass: Installer opens in Update mode; reboot asked.
Long runs on the A1200 go through `ami.py long` (visible window), never a redirected Run.

## Unresolved (settled by Phase 0 only)

- lha reader on the 68k (S1), local ipk path form (S2), file: src form (S3), exact bootstrap command list (S4),
  dry update size in `upkg core` (S5), inline vs deferred packages step on Update (S6). Each branch is decided
  above.
- Not settled by a spike, marked unverified: lhasa licence text (B15 reads COPYING before any port); whether P
  has a host build of libopkg (B35); exact opkg status file path under dist/upkg.conf (B25).
