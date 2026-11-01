---
date: 2026-10-21
topic: upkg upgrade-base (upgrade the UP-Term base kit up-term.lha through upkg)
status: research
---

# upkg upgrade-base: what exists, what constrains it

Documentary only. Paths relative to /Users/spot/Code/UP-Term/vtcon (V) unless stated; P = /Users/spot/Code/UP-Term/upterm-ports.

## 1. build/upkg-index.txt

- Producer: `tools/pkgs.py index` (V/tools/pkgs.py:107-119), Makefile:1182 (`dist-index`, part of `dist`). No source file (thoughts/spot/p9-recon.md:38).
- Consumers found: Makefile:1218 (`dist-check` cmp against a fresh index); RULES.md:128,133 (release: upload it with the archives). NO Amiga-side consumer: the shipped upkg reads the opkg feed (Packages.gz + .ipk), not this index. upterm-ports patches: no hit.
- Line format (pkgs.py:111,116-118): header `# upkg index 1: id archive version needs-base record size sha256`; then one line per row of dist/packages.txt, fields one space apart: `id archive version needs-base record size sha256`, where size/sha256 are of `build/<archive>.lha` (whole file read, hashlib). It contains the base row (`up-term.lha` sha256 is compared in thoughts/spot/todos.md:1066,1076). `needs-base` and `record` are `-` when absent (packages.txt:23). The base row is the only way a client can learn the base version and its checksum: the .ipk feed has NO base entry (RULES.md:25 "the base is not a package").
- Release URL: `https://github.com/spotUP/vtcon/releases/latest/download/upkg-index.txt` (todos.md:1079 confirms 200 for upkg-index.txt and up-term.lha).

## 2. How the base update works today

- Update logic lives in the Installer layer, not in install.dos. `dist/common/update.installer:1-61` runs `Files/upupdate <old MANIFEST> <new MANIFEST> <drawer>`, which writes ENV:UPTUPD<part>, UPTUPDKB, UPTVEROLD/NEW, T:UPTUPD-<part>; then asks Update vs Full install (an Installer `askchoice`); options come back from `ENVARC:up-term/opts`; only changed parts run (P_STEP/P_COPY, dist/Install.installer:433-520).
- `dist/install.dos:1` template: `DIR/A,STEP/K,DEST/K,CONSOLE/S,NOCONSOLE/S,DEVICE/S,NODEVICE/S,SHELLICON/S,SERIAL/S,SSH/S,BEBBOGET/S,CURL/S,WASABI/S,CPU040/S,CPU060/S,UNIFONT/S,EMOJI/S`. Run by hand (no STEP) every part runs; STEP runs one part reading options from ENV:UPTOPT<keyword> (install.dos:28-34,54-100). So from a Shell, `Execute <kit>/Files/install.dos <kit>/Files NOCONSOLE NODEVICE <opts>` is non-interactive but is a FULL install (full copy, no manifest diff, no deletion of removed files, options NOT recovered from the last install: the caller must re-pass CURL, CPU0x0, WASABI etc. or they are dropped). Reference call: tools/rig/upkg_browse_rig.py:124-125 (`Execute VTC:distkit/Files/install.dos VTC:distkit/Files NOCONSOLE NODEVICE SSH BEBBOGET CURL WASABI CPU060 UNIFONT EMOJI`) via `ir.base_install` (install_rig.py:1114-1118).
- A non-interactive true Update would need a new entry that runs upupdate, reads ENVARC:up-term/opts, sets ENV:UPTOPT* and calls install.dos STEP per named part: not present today (verify; see Open questions).
- Copies / reboot: `Lab system` (install.dos:181-185) copies `vtcon-handler` and `pty-handler` to L:, XCON/PTY mountlists to DEVS:DOSDrivers, ixkill to C:; `Lab libs` (692) patched ixemul.library to LIBS:; `Lab boot` (596) S:User-Startup blocks; `Lab console` (779) CON:/RAW: takeover is "after a reboot" (install.dos:798-817); install.dos:756 "XCON: only appeared after a reboot" (unless mounted now). Libs/handler already in memory stay old until reboot (ixemul.library is loaded by running programs).
- check.dos (dist/common/check.dos:1-26) is the ADD-ON precheck (needs UP-Term >= NEEDSBASE via `Version FILE UP-Term:UP-Term.version`), not a base check.

## 3. How upkg is built and wired

- upkg = opkg 0.9.0 binary renamed, with 16 patches in `P/pkgs/opkg/patches/` (0001 curl backend + UP-Term paths + rename; 0002 WITH_SHA256; 0003-0007 short names/cache; 0008-0011 help/width; 0012 default PATH; 0013 free space on state volume; 0014 262144-byte vfork stack; 0015 prerm failure on upgrade; 0016 browse).
- Patch 0016 pattern: `opkg_browse_cmd` in libopkg/opkg_cmd.c runs `/bin/sh -c "${UPKG_BROWSE:-vsh UP-Term:bin/upkg-browse} 'arg'..."` via `xsystem`; registered in the command table `{"browse", 0, opkg_browse_cmd, 0, false}`; help row `upkg_help_row("browse", ...)` in src/opkg.c; `upkg_command_index()` makes argv after `browse` bypass getopt so `--help` reaches the script. The binary is the dispatcher; upkg-browse is a sh script (dist/upkg-browse; copied to UP-Term:bin by the rig, upkg_browse_rig.py:131-134; usage handling `--help -h help ?`, exit 0/1/2, UPKG env override, lines 14-40).
- A new subcommand in the same pattern: new patch 0017 in P/pkgs/opkg/patches/ (command table row, help row, a `upgrade-base` dispatcher; reuse `UPKG_BROWSE`-style env override e.g. UPKG_UPGRADE_BASE), a script dist/upkg-upgrade-base installed to UP-Term:bin (find how upkg-browse is listed in dist/parts.txt; the system part lists Files/upkg.conf at parts.txt:33), plus host tests like the browse ones.
- Feed config: dist/upkg.conf:6 `src/gz upterm https://github.com/spotUP/vtcon/releases/latest/download`, `arch m68k-amigaos 10`, `dest root /`, state under /UP-Term/pkg/upkg/ (lists, info, status, cache, lock), `ignore_uid 1`.

## 4. Amiga-side tools after a base install

- curl: only with the CURL option (install.dos:433-440, net/curl-8.22.0/curl.{060,040,020} -> UP-Term:bin/curl). upkg's download backend runs curl by name, so a base installed without CURL has no working upkg (upkg_browse_rig.py:122-123). curl needs AmiSSL 5 / amisslmaster.library even for http (upkg_browse_rig.py:128-129). curl ships dist/net/curl-8.22.0/SHA256SUMS (build-time only).
- lha: NOT shipped in the base. grep of install.dos/parts.txt finds nothing; Install.installer:49-52 and dist/README.txt:51 say an .lha "is unpacked there with C:lha first": the user's own C:lha. Aminet lha is a prerequisite, not a kit file.
- sha256 tool on the Amiga: none found. dist/gg/coreutils-5.2.1/bin has md5sum and sha1sum only (no sha256sum). The only sha256 on Amiga is libopkg's own (patch 0002 WITH_SHA256), used to verify .ipk SHA256sum from Packages (pkgs.py:332); that code path cannot be pointed at up-term.lha without a new command in the binary.
- Implication: upgrade-base must either (a) add a C command in upkg that fetches by URL and checks the index sha256 with libopkg's sha256 routine, or (b) publish the base as a feed item. Unpacking needs lha, which the base does not ship.

## 5. Self-replacement constraints

- RULES.md:25: "the base is not a package" (owner decision 2026-10-19, plans/2026-10-18-upkg.md:215; handoffs/2026-10-19_upkg-p10.md:14). Staged files wait in T:upkg-stage; add-ons postinst runs their own Files/install.dos.
- thoughts/shared/plans/2026-10-13-kit-split.md:664: "the copy the base runs cannot delete itself while read" (base drawer delete handles it): precedent that a running file cannot be replaced/deleted on AmigaDOS.
- Files in use during base copy: UP-Term:bin/upkg and upkg-ui (the running process; upkg's vfork children), vsh (the shell running the command, C:vsh), vtcon-handler / pty-handler (L:, mounted and in use by open windows), ixemul.library (loaded). A base copy over these must happen from a process that has exited upkg (detach: write a script to T:, `Run` it, upkg exits first), or use Copy CLONE to a temp name and rename on next boot. No existing doc spells this out for upkg. RULES.md:25 text does not mention in-use files.
- The curl backend fetches into /UP-Term/pkg/upkg/cache with FFS short-name limits (patches 0003, 0007): a ~15 MB+ base download name must obey them.

## 6. Prior owner decisions or plans

- No hit for "upgrade-base" or "base update via upkg" in thoughts/ (grep of thoughts/, RULES.md, docs). Related:
  - Owner decision: base is not a package, add-ons only (plans/2026-10-18-upkg.md:215; handoff 2026-10-19_upkg-p10.md:14).
  - opkg-port research line 81: P10.7 "Aminet keeps the base .lha" (research/2026-10-18-opkg-port.md:81); P10.8 "update check = upkg list-upgradable" for add-ons.
  - upkg-index.txt is the planned (D1) base+archive checksum file (plans/2026-10-18-upkg.md:22).
  - Base Update semantics (kit-split plan lines 105, 325, 592, 598: base update must not delete add-on (handed) files).
- Therefore upgrade-base reverses or at least extends the "base is not a package" decision: needs an owner call.

## 7. Rig harness to reuse

- `tools/rig/upkg_rig.py` `Release` (l.121-160): serves a temp dir over http on 127.0.0.1 on the Mac; `set(feed_dir, packages)` symlinks .ipk files plus Packages/Packages.gz (add up-term.lha and upkg-index.txt symlinks there for the base feed); `bumped_feed()` rebuilds a feed with a version bump; `conf_for(conf, url)` (l.78) makes the T:upkg-rig.conf with the src/gz line pointed at the Mac; `upkg(name, args, conf=True)` (l.195) runs upkg on the rig; case functions (case_upgrade l.275, case_needsbase l.322, case_badsum l.339 model a tamper test).
- `tools/rig/install_rig.py`: `prepare()` (l.424) unpacks the kit into VTC:distkit; `base_install()` (l.1114) and `base_whole()` (l.1121: vsh ls is coreutils, VERSIONS present); `run_long`, `rig_libs()` (l.126, restore LIBS: after an install).
- `tools/rig/upkg_browse_rig.py`: `base_stale()` (l.67-72; compares first line of UP-Term:VERSIONS to the kit's: "release, vtcon commit, build date"); install-over-older-base path (l.116-130); copy of upkg/upkg-ui/upkg-browse into UP-Term:bin with CLONE (l.131-134). A reachability test for upgrade-base: install an OLDER kit (or a VERSIONS edited down), serve the new up-term.lha + upkg-index.txt from Release, run `upkg upgrade-base`, assert VERSIONS first line equals the new kit, sentinel that the new code ran, base_whole, and a bad-sha256 case refusing. Rigs run as 060 (`rig.py start --max`, memory feedback-rig-060).
- Rig note: rig 5's System: (106M) is too small for base+curl+nvim (todos.md:787); the base lha plus unpacked copy plus install needs 2-3x the base size free.

## Open questions

- Is there a non-interactive way to run upupdate + STEP parts from a Shell? Looked at: install.dos:1-100, update.installer, Install.installer:433-520 only; did not read Files/upupdate source (install/upupdate.c per kit-split plan 590). Unconfirmed.
- Does `Execute install.dos` over a live base replace in-use vtcon-handler / ixemul.library successfully, or fail with "object in use"? No doc found; needs a rig run.
- Where does upkg-browse get installed to UP-Term:bin (which parts.txt row)? Only the rig copy at upkg_browse_rig.py:131-134 was confirmed; grep of dist/parts.txt for `upkg-browse` did not run.
- Where does the C:lha for unpacking come from on a real Amiga with a base-only install; dist/ grep finds no lha binary. Confirm via find over dist/ (only SHA256SUMS and sha1sum matched).
- Whether the base size (up-term.lha) and the cache/short-name limits allow a download through the opkg cache path (patches 0003/0007) were not measured.
- Whether the owner wants the base in the .ipk feed (a base .ipk with an install.dos postinst) rather than a separate command. Reverses RULES.md:25.
