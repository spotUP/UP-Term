---
date: 2026-11-12
status: design (not started)
topic: upkg updates the base kit (release 1.2)
---

# upkg updates the base kit

Owner decision 2026-11-12: after the first install, upkg alone updates the base kit (no Installer, no manual
download); `upkg upgrade` says a newer base exists and asks before updating it.

Design pass only: nothing built, nothing run on a rig. Every statement carries file:line evidence or is marked
(unverified).

## Recommended design

A base package `up-term` in the feed, built by `tools/pkgs.py` like every add-on, whose data is the staged kit tree
(build/dist/up-term) unpacked to a stage drawer on the UP-Term: volume, and whose postinst runs a new headless
driver `Files/update.dos` that does exactly what the Installer's Update does: restore the recorded options into
ENV:, run `upupdate` against UP-Term:MANIFEST, run only the changed install.dos parts with STEP, write the record
last, delete the stage. The ask lives in two places: a new upkg patch makes `upkg upgrade` ask before it downloads
the base (1.2 and later), and the base package's preinst asks too (the only ask a 1.1 upkg can show).

This is the main session's candidate idea, with three corrections: the stage must not be T: (RAM:), the driver must
not run install.dos without STEP (that path asks and re-copies everything), and the base must be registered in
upkg's status by every base install so that `upkg upgrade` sees it at all.

### Alternatives judged

- A. Base ipk, kit tree as data, postinst runs the update (recommended). Uses opkg's download, sha256 check,
  Installed-Size room check and status bookkeeping as they are; works with a 1.1 upkg (it is just a package).
  Cost: opkg keeps a .list of every staged path (thousands of lines; the stage is deleted afterwards, as for
  add-ons today).
- B. upkg fetches up-term.lha and runs its Update path (new C in upkg). Needs `lha` on the Amiga, which the base
  does not ship (staged kit userland/bin has curl, gzip, sha256sum, tar, upkg, no lha: checked in
  build/dist/up-term/Files/userland/bin); duplicates download, verify, room check and state that opkg already has;
  a 1.1 upkg has none of it, so the first step is still manual. Rejected.
- C. Small `up-term` ipk whose postinst curls up-term.lha itself and checks a sha256 written into the script at
  build time (pkgs.py index already computes it, tools/pkgs.py:110-122). Lets the postinst pick the volume with the
  most room, keeps opkg's .list small. Also needs `lha` on the Amiga, and bypasses opkg's room check. Fallback if
  the .list size of A proves a problem on the rig.

## The six open points

### 1. How the base update works today

- Two install paths. The Installer (dist/Install.installer) runs install.dos one part at a time with
  `STEP=<part>` (dist/common/procs.installer:50-61); a Shell install runs install.dos with no STEP and every part
  falls through in order (dist/install.dos:117-126).
- The Update is Installer-only (dist/common/update.installer:1-10): with UP-Term:MANIFEST and the recorded options
  (ENVARC:up-term/opts) present, `Files/upupdate OLD NEW KIT` (install/upupdate.c:1-22) writes ENV:UPTUPD<part>
  (KB per part to run), ENV:UPTVEROLD/NEW, ENV:UPTHANDED and T:UPTUPD-<part> copy scripts (new and changed files
  copied, removed ones deleted). The Installer then copies the options back to ENV:
  (dist/Install.installer:506-508), runs only the named parts (P_WANT, dist/common/procs.installer:40-48; the
  record part always), runs the T:UPTUPD-<part> script instead of copyfiles for copy parts
  (dist/common/procs.installer:70-82), and finishes with the record part (dist/Install.installer:675-678).
- Part order (dist/Install.installer:579-678): drawer, system, libs, copy-coreutils, copy-userland, userland,
  copy-unifont, copy-emoji, tools, fonts, unix, desktop, boot, mount, console, record; then P_AMISSL
  (dist/Install.installer:806).
- Questions: in STEP mode install.dos takes every option from ENV:UPTOPT<keyword> (dist/install.dos:58-116) and
  asks nothing. The only Asks are CON:/RAW: and console.device (dist/install.dos:1256, 1321), and only when neither
  keyword of the pair is set; the recorded options always carry one of each (the Installer writes CONSOLE or
  NOCONSOLE, DEVICE or NODEVICE: dist/Install.installer:510-511, 548-549). So STEP mode with restored options runs
  without questions.
- What it needs: the recorded options. record.dos writes ENVARC:up-term/opts only when ENV:UPTOPTDEST exists
  (dist/common/record.dos:16-23), i.e. only after an Installer install. A Shell install, which is what
  Get-UP-Term does (dist/Get-UP-Term: `Execute UPTGET:up-term/Files/install.dos UPTGET:up-term/Files`, no options),
  records none. The Installer reconstructs missing options by probing S:User-Startup blocks and files
  (dist/common/oldopts.installer); that logic exists only as Installer code.
- tools/mkinstaller.py substitutes the record paths; for the base the options drawer is ENVARC:up-term/opts
  (tools/mkinstaller.py:125).
- upupdate, UPBigVol and install-amissl.dos are kit-only (part "-", dist/parts.txt:15, 54), never copied into
  UP-Term:. The driver must run them from the stage.

### 2. How upkg knows the two base versions; where upkg lives

- upkg is opkg 0.9.0 with UP-Term patches, built in the peer repo upterm-ports: recipe
  /Users/spot/Code/UP-Term/upterm-ports/pkgs/opkg/recipe.mk:1-30, patches 0001-0027 in pkgs/opkg/patches, host
  checks in pkgs/opkg/check. The browse UI is libupkgui (pkgs/upkg-ui), pinned by tools/upkgui.rev. The kit ships
  the binary as UP-Term:bin/upkg in the userland part (dist/install.dos:400-404).
- Config: dist/upkg.conf (one feed, `src/gz upterm .../releases/latest/download`, dest root "/", state under
  UP-Term:pkg/upkg). Its own header says "The base is not a package" (dist/upkg.conf:4), and tools/pkgs.py:194-197
  drops the base row (record UP-Term:) from the ipk build.
- Installed base version: first line of UP-Term:VERSIONS, second field; the needs-base preinst reads it that way
  (tools/pkgs.py:238-250). install.dos copies VERSIONS in the "tools" part (dist/install.dos:632-633), not in the
  record part.
- Feed base version: dist/packages.txt:26 (`base up-term 1.1 ...`). It reaches the feed today only through the
  index (`upkg index 1`, tools/pkgs.py:110-122) and Get-UP-Term; there is no `up-term` stanza in Packages, so opkg
  never sees it.
- Upgrade today: stock opkg upgrade over installed packages (status file), patched only for messages and counting
  (patch 0021: upkg_upgrade_attempts) and for a failing old prerm (patch 0015). It never asks. The base is not in
  the status file, so `upkg upgrade` cannot see it (follows from tools/pkgs.py:194-197; status content on a real
  install unverified).
- Downloads: curl `-fsSL -o` (patch 0001, hunk at its lines 156-163); sha256 checked against Packages (patch 0002,
  check/sha256_feed.sh). No signature (no gpgme, recipe.mk:8).

### 3. Files in use during the update

- install.dos copies over everything with `Copy ... CLONE` and stops nothing, except amiagent and wasabid, which it
  Breaks before replacing (dist/install.dos:828-834, 1144-1146). amarchy-flat (started at boot from a block in
  S:Startup-Sequence, dist/install.dos:724-738), the handlers in L: (dist/install.dos:305-307) and
  up-console.device are copied over while loaded.
- ixemul.library is replaced on disk (dist/install.dos:453-465) while upkg, vsh and curl (all ixemul programs) have
  it open; the copy in memory stays until reboot.
- Overwriting a running program's file is safe on AmigaDOS because LoadSeg does not keep the file open (unverified
  here; standard AmigaDOS behaviour, to be confirmed on the rig with upkg overwriting itself and amarchy-flat
  running).
- The Installer ends by offering a reboot because handlers, XCON: and assigns only start at the next boot
  (dist/Install.installer, comment above P_END near line 560). So yes, a reboot is needed. The postinst must NOT
  reboot: opkg writes its status after the postinst returns. It prints "Reboot to finish the update" instead;
  between update and reboot, new UP-Term:bin programs run against the old ixemul in memory (risk below).

### 4. Space and where to stage

- Sizes (host, measured): build/up-term.lha 34 213 792 bytes; staged kit build/dist/up-term 67 MB (FFS block
  overhead on the Amiga adds more, unverified how much). Get-UP-Term asks for about 110 MB for archive plus
  unpacked kit (dist/Get-UP-Term header).
- T: is not usable: add-on ipks stage under ./T/upkg-stage (tools/pkgs.py:279-290, dist/upkg.conf:2-4), and T: is
  often RAM:, which cannot hold 67 MB on most machines.
- Recommended stage: the UP-Term: volume, data paths `./UP-Term/pkg/upkg-stage/up-term/...` (ixemul maps /UP-Term
  to UP-Term:, dist/upkg.conf header and recipe.mk:5-6). The ipk download lands in cache_dir
  UP-Term:pkg/upkg/cache on the same volume. Peak extra room on that volume: about 34 MB ipk plus about 67 MB
  stage. The install itself replaces files in place, so it needs little more.
- Room check for free: build_ipk writes Installed-Size from the staged size (tools/pkgs.py:381), and opkg refuses
  when the volume lacks it; patch 0013 measures the volume holding info_dir (UP-Term:pkg), which is the stage
  volume. Whether opkg also counts the cached download is unverified. Whether opkg deletes the cached ipk after a
  successful install with cache_dir set is unverified.
- UPBigVol (install/bigvol.c:1-15) picks the local volume with most room; it is only needed if alternative C is
  chosen.

### 5. Failure safety

- Cut-off or tampered download: curl -f fails on HTTP errors, and opkg checks sha256 against Packages before
  unpacking (patch 0002), so nothing is unpacked. Packages itself is trusted only through TLS. Whether the Amiga
  curl verifies the server certificate (CA bundle via AmiSSL) is unverified; Get-UP-Term's bebboget does check
  certificates against ENVARC:certs (dist/Get-UP-Term header and its installcerts step). Rig test 7 settles it; if
  curl does not verify, patch 0001's argv gets `--cacert` pointing at the bundle the base installs.
- Failure while unpacking (disk full): only the stage drawer is touched; the live base is unchanged. For a first
  install of the package (the 1.1 path) opkg returns to not-installed (patch 0006); for an upgrade the comment in
  patch 0006 says the package "still ends hosed" (status only, the files are untouched). The driver deletes any
  stale stage at its start.
- Failure during the parts: today's Installer aborts and leaves "what was installed so far"
  (dist/common/procs.installer:58-61). The driver makes the update re-runnable instead: the record part (MANIFEST,
  options) stays the commit point and runs last; VERSIONS moves from the tools part into the record part, so a
  half-run update still reports the old version and needs-base checks stay honest; on any part failing, the
  postinst keeps the stage, exits 20, and prints the line to run it again. Because UP-Term:MANIFEST is still the
  old one, upupdate recomputes the same work and the re-run converges. Re-running via `upkg configure up-term` is
  unverified for opkg 0.9; the printed line therefore runs the driver straight from the stage.
- Power loss mid-copy: no atomic swap is possible, since the base writes C:, L:, LIBS:, DEVS:, S: as well as
  UP-Term:. Same exposure as the Installer today; the re-run above is the recovery.

### 6. Reaching an installed 1.1

- A 1.1 upkg has no new code, and the base is not in its status file, so `upkg upgrade` on 1.1 will not offer the
  base. The 1.1 user's only route is installing the package once: `upkg install up-term` (or from upkg browse;
  browse listing a not-installed base is unverified).
- Reach: the needs-base refusal text is written by the 1.2 build into each 1.2 add-on's preinst
  (tools/pkgs.py:238-250), so a 1.1 user who upgrades an add-on that needs 1.2 can be told the exact command,
  with no change to the 1.1 upkg. The preinst of the up-term package asks the confirm question, which works with
  any upkg version.
- Which patches the 1.1 upkg carries (0002 sha256, 0006 fresh-install unwind) is unverified; check the 1.1
  release's upterm-ports pin before relying on them for the first hop.

## Changes by file

1. tools/pkgs.py: build an `up-term` ipk from the base row (stop excluding it at :194-197 for the ipk/feed
   commands, keep excluding it from the add-on lists). Data under `./UP-Term/pkg/upkg-stage/up-term/` instead of
   `./T/upkg-stage/`. Own scripts: preinst (ask, skipped when ENV UPKG_BASE_OK=1 is set by the new upkg; refuse
   when the drawer of ENVARC:up-term/Dir is missing), postinst (`Execute <stage>/Files/update.dos <stage>/Files`,
   keep stage on failure), prerm a no-op on upgrade and exit 1 on remove (never runs Uninstall). One variant only
   (the base picks CPU builds by option, not by ipk). Needs-base message of add-ons names `upkg install up-term`.
2. dist/update.dos (new, kit-only, part "-" in dist/parts.txt): the headless Update. Copy ENVARC:up-term/opts/#?
   to ENV:, or, when there are none, rebuild them with a DOS port of dist/common/oldopts.installer (S:User-Startup
   block probes, ENVARC:up-term/Dir); run upupdate; run install.dos STEP=<part> for each ENV:UPTUPD<part> in the
   Installer's order, T:UPTUPD-<part> for copy parts; install-amissl.dos when `Version LIBS:amisslmaster.library 5`
   fails (as dist/install.dos:536-550); record last; delete ENV:UPTOPT#?, ENV:UPTUPD#?, T:UPTUPD-#?; print old and
   new VERSIONS and "Reboot to finish". Shares its part list with Install.installer; tests/test_dist_installer.py
   (named at dist/install.dos:120-121) extends to hold the three in the same order.
3. dist/install.dos: (a) a Shell install writes its options as ENV:UPTOPT<keyword> before the record part, so
   record.dos keeps them (Get-UP-Term installs then become updatable without probing); (b) move the VERSIONS and
   UP-Term.version copies (:632-636) into the record part; (c) the record part registers the base in upkg's state
   (a status stanza `Package: up-term`, Version from VERSIONS, `Status: install ok installed`, plus
   info/up-term.control and a no-op info/up-term.prerm) through a vsh helper like instssblock.sh, replacing an old
   stanza. When the update runs inside opkg, opkg's own status write afterwards wins (unverified ordering; check on
   the rig).
4. dist/common/record.dos: unchanged logic; it already writes the options when ENV:UPTOPTDEST exists (:16-23).
5. upterm-ports pkgs/opkg: new patch 0028: in `upgrade`, when `up-term` is upgradable, print installed and feed
   version and ask "Update the UP-Term base now? y/N" before any download; No (or no console) skips only the base,
   the add-ons still upgrade; Yes sets UPKG_BASE_OK=1 for the preinst. A `--base`/`--no-base` flag for scripts
   (owner decision 3). Host check in pkgs/opkg/check. Bump tools/upkgui.rev is not needed (opkg, not libupkgui;
   unverified whether the browse UI's upgrade path goes through the same function).
6. dist/upkg.conf: comment ":4 The base is not a package" rewritten; no config change needed.
7. Makefile: the feed target adds the base ipk; kit-stage copies update.dos to Files/.
8. dist/Get-UP-Term: unchanged, apart from benefiting from 3a and 3c.

## Owner decisions

1. How a 1.1 user gets the first update.
   - (Recommended) Release note plus the needs-base refusal text in 1.2 add-ons naming `upkg install up-term`.
     No surprise downloads; reaches users only when they read or hit the message.
   - Every 1.2 add-on `Depends: up-term (>= 1.2)`: a 1.1 `upkg upgrade` pulls the base automatically (the preinst
     still asks). Reaches every add-on user, but a 34 MB download starts before the question, and users with no
     add-ons are not reached.
   - Re-run Get-UP-Term by hand for 1.1 users. Simplest, but breaks "no manual download" for that one hop.
2. Where the base update stages its files.
   - (Recommended) The UP-Term: volume, fixed by the package (opkg room check works; about 100 MB extra peak).
   - The local volume with most room, chosen by UPBigVol (alternative C): fits small UP-Term: volumes, but needs
     lha on the Amiga and its own room check.
3. What a non-interactive `upkg upgrade` (no console, scripts, browse) does with the base.
   - (Recommended) Skips it and says so; `upkg upgrade --base` updates without asking.
   - Updates it without asking. Convenient, but a script could replace the base unseen.
4. Reboot after the update.
   - (Recommended) Print "Reboot to finish" and leave it to the user. Safe for opkg's status write.
   - Offer the kit's C:Reboot after upkg exits (needs a hook after opkg finishes, i.e. more upkg C code).
5. A 1.1 install made from a Shell has no recorded options.
   - (Recommended) The driver rebuilds them by the oldopts probes (same rules the Installer uses today).
   - Refuse and tell the user to run the Installer once. Simpler, but breaks "upkg alone" for those users.

## Risks

- Between update and reboot, new UP-Term:bin programs run against the old ixemul.library in memory; a 1.2 binary
  that needs a new ixemul call fails until reboot (unverified whether 1.2 changes ixemul).
- opkg's .list for up-term holds every staged path (thousands of lines); list and upgrade speed with it is
  unverified (patch 0017 shows file indexes can be slow on an A1200).
- Status written by install.dos (change 3c) and by opkg must agree on format; a malformed stanza breaks every upkg
  command. Needs host checks against opkg's parser.
- The upgrade path ("hosed" on preinst failure, patch 0006 comment) must never be reached with the 0028 ask in
  front; on a 1.1 upkg it is a fresh install, which unwinds cleanly.
- curl certificate verification on the Amiga is unverified (point 5).
- The Installer's Update and update.dos are two drivers of one part list; the test must hold them together or they
  drift.

## Rig test plan

Each rig started with a purpose, e.g. `rig.py start --purpose "<session>: upkg base update <case>"`, against a
local feed holding a 1.2 base ipk.

1. 1.1 installed by the Installer: `upkg update; upkg install up-term`; answer the preinst ask Yes. PASS: VERSIONS
   says 1.2, UP-Term:MANIFEST is 1.2's, ENVARC:up-term/opts unchanged, S:User-Startup and S:Startup-Sequence blocks
   unchanged, stage drawer gone, `upkg list-installed` shows up-term 1.2; after reboot the desktop and an UP-Term
   window start.
2. Same, answer No. PASS: nothing changed, up-term not installed, stage gone.
3. 1.1 installed from a Shell (no opts): as 1, PASS also checks the rebuilt options match the probed blocks.
4. 1.2 to a 1.2.1 feed: `upkg upgrade` asks before downloading; No skips only the base; Yes updates. No console
   (run from a script): the base is skipped with a message.
5. Truncated ipk in the feed: PASS: sha256 refusal, base untouched.
6. UP-Term: volume too small: PASS: opkg refuses on room before unpacking.
7. Feed served with a bad certificate: PASS: curl refuses (settles point 5).
8. A part made to fail (a write-protected target in C:): PASS: exit 20, stage kept, old VERSIONS still shown; the
   printed re-run line finishes the update after the protection is removed.
9. amarchy-flat running and upkg overwriting UP-Term:bin/upkg during the update: PASS: no crash before reboot.
10. `upkg remove up-term`: PASS: refused, nothing deleted.

## Gaps

- opkg 0.9's handling of `configure` re-running a failed postinst, its cache cleanup, and whether its room check
  counts the download: not read in the opkg source.
- Which patches the 1.1 upkg carries: not checked against the 1.1 pin.
- Whether upkg browse can list and install a not-installed base: not read (upkg-ui source not opened).
- Whether the Amiga curl verifies certificates: not checked.

## Owner answers (2026-11-12, question tool, session s74)

1. First update for installed 1.1: the needs-base message plus a release note naming `upkg install up-term`. No add-on dependency on the base, no rerun of Get-UP-Term.
2. Stage drawer: the UP-Term: volume (`UP-Term:pkg/upkg-stage`); a clear message when it lacks room.
3. Non-interactive `upkg upgrade`: skip the base with one line; a `--base` flag forces it.
4. Shell-installed 1.1 without recorded options: rebuild the options by probing what is installed.
Not asked yet: reboot handling (design recommends: message only, upkg never reboots).
