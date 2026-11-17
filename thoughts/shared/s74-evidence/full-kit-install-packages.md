---
date: 2026-11-12
topic: Offline full kit (up-term-full.lha v1.1) base install + Install-Packages on a rig
verdict: FAIL
rig: 99 (purpose "up-term-ba s74: full kit Install-Packages test"), stopped
---

# Full kit install test: up-term-full.lha v1.1

Test run only. Nothing fixed, committed, pushed or uploaded.

## Archive

- Source: https://github.com/spotUP/vtcon/releases/tag/v1.1, asset `up-term-full.lha`
- Size: 72,937,587 bytes
- sha256: `49e6c30a531693e633f7da21647550a8880e5ed0ad604fe84732764d62bec4ae`
- `lha l`: 1583 files; 28 `up-term/Packages/*.ipk` plus `Packages` feed and `up-term/Install-Packages` (1982 bytes).
- Packages in archive (28): bitchx, claude, ctags, dopus, fzy, git, hexedit, joe, jq, lua, lynx, mg, mutt, nano, nc, ncdu, nnn, nvim, perl, pi, picocom, python, sqlite3, tig, uhc, vbcc, vim, zip.
- Kit ships no Amiga `lha`. The rig had none (`Which lha` rc 5); Aminet `util/arc/lha.run` (sha256 `76bae515264fcc3e1c69058ff03a4bcb096152a732cf19fdb03cceee18932497`) supplied `C:lha` (lha_68020).

## Rig

- `UPTERM_RIG=99 python3 tools/rig/rig.py start --max --os 32 --purpose "up-term-ba s74: full kit Install-Packages test"`
- Network could NOT be turned off: `tools/rig/rig.py` lines ~1367/1543 force `bsdsocket_library = 1` (wasabid control channel needs it). Only up-uhc attempted a download.
- Pre-state: `Which upkg` rc 5; `List SYS:UP-Term` rc 20. Harness assigns `UP-Term: VTCX:Apps` (harness tree, no upkg).

## Steps and return codes

| Step | Command | rc |
|---|---|---|
| Unpack | `lha x up-term-full.lha` in Work:kit | 0 ("Operation successful.") |
| Base install, documented Shell form (README step 2) | `CD Work:kit/up-term` then `Execute Files/install.dos Files` | 20 |
| Base install, absolute DIR (workaround) | `Execute Work:kit/up-term/Files/install.dos Work:kit/up-term/Files` | 0 |
| Packages (README step 4) | `CD Work:kit/up-term` then `Execute Install-Packages` | 14 (rc of last line; `FailAt 21` lets every line run) |

## Failure 1: documented Shell base install, relative DIR

Proving lines:
```
Files/UPBigVol: Unknown command
UP-Term: nothing has been copied. Free some room, or give DEST a drawer on a bigger disk.
```
Cause: `dist/install.dos` line 32 does `CD "{DIR}"`; line 181 then runs `"{DIR}/UPBigVol" ROOM ...`. With relative `DIR=Files` this resolves to `Files/Files/UPBigVol`. The unknown-command WARN trips the room check (lines 182-191), `Quit 20`. UPBigVol itself works when called by absolute path. The bootstrap test passes only because Get-UP-Term passes an absolute DIR (`UPTGET:up-term/Files`). The Installer (double-click Install) path was not tested.

## Failure 2: Install-Packages, 23 of 28 installed

`upkg status`: 23 installed; 5 left "install user unpacked" (configure failed): up-git, up-nvim, up-perl, up-vim, up-uhc. Note `upkg list-installed` lists all 28 including unpacked ones, so it is not a reliable count.

Installed (23): bitchx, claude, ctags, dopus, fzy, hexedit, joe, jq, lua, lynx, mg, mutt, nano, nc, ncdu, nnn, pi, picocom, python, sqlite3, tig, vbcc, zip.

### 2a. git, nvim, perl, vim: UPTREAL: dropped before copy

First error line in the log (up-git, pkgs.log line 74):
```
Can't create directory UPTREAL:bin
device (or volume) is not mounted
Copy: UPTREAL: - device (or volume) is not mounted
```
Cause: `tools/mkinstaller.py` lines 177-181 build `RENAMERECORDREAL` for packages listed in `dist/package-renames.txt` whose old record lives under `UP-Term:` (UP-Term-git, -nvim, -perl, -vim). The block is resolve-uptreal include + rename lines + `Assign >NIL: UPTREAL:`. It is inserted at `dist/common/check.dos` line 16, inside `Lab check`. With no STEP (the upkg postinst path) the script falls through to `Lab copy-<pkg>` after the assign is removed, so MakeDir/Copy to `UPTREAL:` fail and the script quits 20. Installer path with STEP is unaffected (check exits with `Quit 0`). claude, pi, python are also in the renames list but unaffected (old records not under `UP-Term:`).

### 2b. uhc: needs network, not offline-capable

```
Couldn't find aget or wget on your system so the installation can't continue.
[ERROR] UHC's installer ran but SYS:UHC/UHC-Startup is not there: UHC was not installed.
```
Cause: `dist/addons/uhc/install.dos` line 7 runs uhcinstall, which downloads UHC. README says the full kit installs offline; this package cannot.

### 2c. dopus: interactive Installer blocks the batch

up-dopus postinst runs `dist/addons/dopus/install.dos` line 29: `Installer SCRIPT "Install DOpus5" ... MINUSER Novice DEFUSER Novice NOLOG`. The upstream Installer window opened and the batch blocked about 8 minutes until "Proceed With Install" was clicked twice (driven through ami.py gclick). It then installed to Work:DOpus5. An unattended batch would hang here.

### 2d. Knock-on effects (not fatal on their own)

- Each later `upkg install` retries configure of every unpacked package: repeated `vsh: T:upkg-stage/up-git/Files/install.dos: cannot open` (git retried 23 times, nvim 11, perl 10, uhc 4), because the postinst had already deleted the stage.
- Every postinst's `Delete T:upkg-stage/<pkg> ALL` fails: `T:upkg-stage/<pkg>/Files  Not Deleted: object is in use` (install.dos `CD "{DIR}"` leaves the Shell CD inside the stage). 14+ staged trees remain in RAM:T/upkg-stage. Postinst text is written by `tools/pkgs.py`.

## Spot checks after install

- `UP-Term:bin/upkg --help`: usage shown, rc 0
- `UP-Term:bin/jq --version`: `jq-1.7.1`, rc 0 (443,108 bytes)
- `UP-Term:bin/lua -v`: `Lua 5.4.7`, rc 0
- `UP-Term:bin/git`: object not found

## Not tested

Installer GUI path (double-click Install); network-off emulator (not possible with current rig); reboot after install; starting packages beyond upkg/jq/lua; uninstall.

## Cleanup

Rig 99 stopped (`rig.py stop` "stopped"); `ps` shows no rig 99 fs-uae. Deleted: build/rig-saved/99 (132 MB), downloaded archive (73 MB), lha download, extracted git ipk. Small logs left in the session scratchpad (pkgs.log, install*.log, unpack.log, status.txt, listinst.txt). `df -h /`: 11 GiB free at end (14 GiB at start). Rigs 2 and 3 not touched.

## Register row text (for thoughts/shared/research/test-register.md, not written there)

| 2026-11-12 | Full kit v1.1 offline install (rig 99) | FAIL | Documented Shell install `Execute Files/install.dos Files` rc 20 (relative DIR, UPBigVol not found); absolute DIR rc 0. Install-Packages 23/28: git/nvim/perl/vim fail (UPTREAL: assign dropped in check.dos rename block), uhc needs network, dopus blocks on interactive Installer. sha256 49e6c30a...4bec4ae | thoughts/shared/s74-evidence/full-kit-install-packages.md |

## Fixes (2026-11-12)

### KIT-INSTALL-RELATIVE-DIR (Failure 1) - commit f6298db9
- Cause: after `CD "{DIR}"` the scripts reused the caller-relative `{DIR}`: dist/install.dos `"{DIR}/UPBigVol" ROOM` and `Execute "{DIR}/install-amissl.dos" DIR "{DIR}"` (install-amissl.dos then used `{DIR}/AmiSSL/...` too); dist/addons/dopus/install.dos `CD "{DIR}"` twice after `CD "$dopusbuild"`. No other add-on used `{DIR}` after its CD. upkg's postinst passes absolute T:upkg-stage paths, so upkg was unaffected.
- Fix: new fragment dist/common/kit-dir.dos (`CD "{DIR}"` + `Set uptkit "`CD`"`), included by dist/install.dos, all 29 dist/addons/*/install.dos and the tools/mkappaddons.py template; later uses go through `$uptkit`. Test: tests/test_dist_addons.py test_no_install_dos_names_the_relative_dir_after_its_cd; room/amissl tests updated.
- Pre-existing drift (not fixed): `tools/mkappaddons.py --check` fails at HEAD; 16 committed app add-ons (mutt, lynx, joe, mg, nano, tig, ncdu, nnn, lua, jq, hexedit, picocom, nc, fzy, ctags, zip) use the RECORDRESOLVE include and UPTREAL:, the generator still writes UP-Term: without the resolve. Regenerating would undo that.

### UPKG-UPTREAL-GONE (Failure 2a) - commit bbc61d23
- Cause: tools/mkinstaller.py RENAMERECORDREAL (substituted at dist/common/check.dos:16) ended with `Assign >NIL: UPTREAL:`. Without STEP (upkg postinst) the script falls through to `Lab copy-<pkg>`, which writes through UPTREAL:. Only rows in dist/package-renames.txt with an old record under UP-Term: get this block: git, nvim, perl, vim. Emitted regardless of an old record, so fresh installs fail too.
- Published v1.1 online `upkg install git/nvim/perl/vim`: affected by reasoning (same generated script, same postinst); not tested online. Installer (STEP) path unaffected.
- Why tools/rig/upkg_install_all_rig.py missed it: its fixture packages are not in the renames list; the host rename test asserted the drop line itself.
- Fix: rename block keeps the assign; record-end-addon.dos drops it. Test: test_uptreal_stays_assigned_from_check_to_the_copy_part.

### Rig proof (rig 95, kit from `make dist` / `make dist-ipk` with UPTERM_PORTS=upterm-ports/.claude/worktrees/kit-main)
- Base install via `CD VTC:distkit` + `Execute Files/install.dos Files DEST=... CPU060`: rc 0, no Unknown command, UP-Term.version written: PASS.
- upkg from local feed: up-git, up-nvim, up-perl, up-vim install rc 0, status installed: PASS. Versions: git 2.56.0, NVIM v0.12.5, perl 5.42.3, VIM 9.2: PASS. 16/16 checks.
- Build note: dopus-toolbar regeneration needed fontTools (absent from every host python); used a scratch venv. The kit packs the other session's dirty dist/Uninstall.
- Not tested: online feed, Installer (STEP) path, the other add-ons, uhc/dopus-Installer/T:upkg-stage items.

### Independent re-proof (second agent, rig 99, same commits f6298db9 / bbc61d23)
- Host tests, each alone under nice 19 (`python3 -m unittest`): test_dist_addons, test_install_dos_amissl, test_install_dos_room OK; test_dist_installer OK (1 skipped).
- Rig 99 (`--max --os 32`, purpose "up-term-ba s74: full kit install fixes"), pre-state as above (UP-Term: = VTCX:Apps, no upkg). Kit build/dist/up-term (13:48 cut) copied to VTC:kitrel/up-term; script `FailAt 21` / `CD VTC:kitrel/up-term` / `Execute Files/install.dos Files` (exact README form, no DEST): BASERC=0, no "Unknown command", no "nothing has been copied": PASS.
- `UP-Term:bin/upkg install VTC:pk/<ipk>` (build/ipk 13:56-13:57 cut) one by one, Stack 262144: up-git, up-nvim, up-perl, up-vim each PKRC=0, no "not mounted"; `upkg status` all four "install user installed"; `Assign UPTREAL: EXISTS` afterwards rc 5 (assign dropped at record end, not leaked): PASS.
- Code check for v1.1: `git show v1.1:tools/mkinstaller.py` lines 180-181 still append `Assign >NIL: UPTREAL:`, so the published git/nvim/perl/vim ipks carry the bug (online install not run).
- Not tested here: Installer (STEP) path, online feed, other add-ons, version strings of the four programs.
- Cleanup: rig 99 stopped; deleted build/rig-saved/99/20261112-141403 (86 MB) and the VTC staging; scratch logs (rig-*.log, drive.py, pkgs-driver.out, under 1 MB) left in the session scratchpad.

## uhc offline (2026-11-12)

- What is downloaded: `dist/addons/uhc/install.dos` (fresh mode) fetches `http://uhc.megaburken.net/aget.os3` (when no aget/wget) and `http://uhc.megaburken.net/uhcinstall`, then runs uhcinstall, which itself downloads `uhctools_os3.lha` from the UHC mirrors. The ipk holds no UHC file.
- Licence: `dist/addons/uhc/LICENSES.txt:5` says only "UHC tools (downloaded)  by the author (Patrik, UHC)"; lines 9-11: "This package contains no UHC file". No redistribution grant anywhere in the tree (`dist/LICENSES.txt` has no uhc entry). `Makefile:2189` records the owner decision of 2026-10-27: no UHC files in up-uhc. Redistribution is NOT clearly allowed, so nothing was packed. Open question for the owner: ask the UHC author for permission to pack uhcinstall + uhctools_os3.lha + aget.os3.
- Fix (Makefile, `dist-full`): Install-Packages gets one line for up-uhc in place of `upkg install`: `Echo "[INFO] up-uhc skipped: UHC comes from its author's server at install time (not ours to pack), so it needs a network: later run  UP-Term:bin/upkg install Packages/up-uhc_1.5_m68k-amigaos.ipk"`. The .ipk stays in Packages. The archive count check counts install lines plus this skip line. No sibling add-on had a skip pattern (dist-full is the only Install-Packages writer); the case line follows the shell loop already there.
- Proof: `make dist-full` (UPTERM_PORTS not needed) rc 0, "[OK] build/up-term-full.lha: base + 28 packages", 27 `upkg install` lines + 1 skip line, sha256 8fffb9a78bb49966... Rig 99 (`--max --os 32`, purpose "up-term-ba s74: uhc offline install"): Install-Packages extracted from that archive, its FailAt/Stack header and the uhc line run as `Execute RAM:ip-uhc`: rc 0, the [INFO] line printed, `$RC` after it 0, `List T:up-uhc-bin T:uhcinstall` object not found (nothing downloaded).
- Not tested: whole Install-Packages after a base install (rig had no base and no lha); no host test covers dist-full (no test names dist-full or Install-Packages).
- Cleanup: rig 99 stopped; deleted build/rig-saved/99/20261112-142515 (11 MB, my stop's save); scratch extract deleted. build/up-term-full.lha and build/dist-full left (build output).

## dopus batch and stage leftovers (2026-11-12)

### Causes (written before the first edit)
- KIT-FULL-DOPUS-BLOCKS. Violated rule: the offline batch Install-Packages must never wait for a person. Layer: the batch writer (Makefile `dist-full`), not dopus's install.dos. Upstream `Install DOpus5` (build/dist/up-dopus/Files/os3) has no unattended path: its getenv calls (DOpus5Icons, DOpus5Startup, DOpus5Preload, DOpus5Assign) only preset defaults of askchoice/askbool/askdir, and it always ends on `(message ...)`; Installer has no batch switch (Novice still shows messages and Proceed). Our dist/addons/dopus/install.dos has STEP labels but no step skips the Installer call; no sibling add-on copies DOpus files itself. No feed package Depends on up-dopus (build/ipk/Packages). So the fix is the fallback: Install-Packages runs up-dopus last, after one [INFO] line announcing the window. Pattern reused: the up-uhc case line of the same loop (commit 8c902bc2).
- UPKG-STAGE-LEFTOVER. Violated rule: the postinst that unpacks a stage must leave nothing in T:upkg-stage; a directory cannot be deleted while it is a process's current directory. Layer: the postinst text written by tools/pkgs.py `_scripts` (the one place every package's stage delete comes from). vsh `Execute` runs the AmigaDOS script in the vsh process itself (shell/sh_exec.c execute_file -> sh_amiga_execute), so the script's `CD "{DIR}"` (dist/common/kit-dir.dos) is still in force when the postinst's `Delete T:upkg-stage/<pkg> ALL` runs. Fix: the postinst CDs to T: after Execute and before Delete. Same idea as the prerm's existing guard (run the Uninstall from a copy in T: so the record drawer holds nothing open). Not in kit-dir.dos / install.dos: those also run under the Installer, and they have many Quit exits, so restoring the directory there would touch 31 scripts instead of one generator.

### Fixes
- UPKG-STAGE-LEFTOVER: tools/pkgs.py `_scripts` postinst (both forms, with and without questions) is now `Execute .../install.dos ...` / `rc=$?` / `cd T:` / `Delete >NIL: T:upkg-stage/<pkg> ALL QUIET` / `exit $rc`. Test: tests/test_dist_ipk.py asserts the `cd T:` before the Delete in both forms.
- KIT-FULL-DOPUS-BLOCKS: Makefile `dist-full` loop holds back `up-dopus_*` and writes, after every other line, `Echo "[INFO] up-dopus last: the Directory Opus 5 Installer window opens now and waits for your answers"` and then its `upkg install` line. The count check is unchanged (still one install line per package plus the uhc skip line).
- Retries (not fixed): opkg's `install` configures every package left in state unpacked (a postinst that exited non-zero), so each later `upkg install` reran the failed postinsts of git/nvim/perl/vim (UPTREAL bug, fixed in bbc61d23) and uhc (network, now skipped in 8c902bc2); the postinst had already deleted the stage, hence `cannot open .../install.dos`. With those two causes gone no postinst in the batch fails, so nothing is left unpacked. Keeping the stage on failure would turn every retry into a full re-install (dopus: a new Installer window), so it was left as is. opkg's configure-all path was read from upstream opkg behaviour; our opkg source was not found in this repo or the ports worktree (unverified there).

### Proof (rig 99, OS 3.2, build of 632ed9c1)

- Commits: f284bae9 (stage leftover: postinst does `rc=$?`, `cd T:`, Delete, `exit $rc`), 632ed9c1 (up-dopus last in Install-Packages, after an `[INFO]` notice).
- Host tests, one at a time, nice 19, UPTERM_PORTS=kit-main: test_dist_ipk OK (17), test_dist_addons OK (71), test_dist_installer OK (100, 1 skipped).
- `make dist` rc 0, `make dist-full` rc 0, "[OK] build/up-term-full.lha: base + 28 packages". Install-Packages ends with the [INFO] dopus line, then the up-dopus install.
- Base install from the archive (README Shell form): BASERC=0.
- `Execute Install-Packages`: the batch ran unattended up to dopus. The [INFO] uhc skip line and the [INFO] dopus line were printed. The Installer window "Install DOpus5" opened last and was answered (gadget 90 on the welcome page, then the final page). No "in use", no "cannot open .../install.dos" line in the log.
- IPRC=14, not 0. The up-dopus postinst failed after the Installer, in the record step: `Can't open VERSIONS for input - object not found`.
- `upkg status`: 26 `install user installed` + up-dopus `install user unpacked` (27 packages; uhc skipped by design).
- `List T:upkg-stage ALL`: `Directory "T:upkg-stage" is empty`. Bug B fixed (this includes the failed dopus postinst, because `drop` deletes on any rc).

### New bug: KIT-DOPUS-RECORD-CWD (not fixed)

dist/common/kit-dir.dos does `Set uptkit "`CD`"`; dist/addons/dopus/install.dos goes back with `CD "$uptkit"` after the Installer. Rig test (rig 99): the same 6-line script run with `Execute` from the AmigaDOS Shell gives `uptkit=[RAM Disk:]`, but run through vsh (`UP-Term:bin/vsh script` that does `Execute`) gives `uptkit=[]`. Under vsh, the backtick `CD` yields nothing (vsh is bash-first: a bare `cd` goes home and prints nothing), so `CD ""` leaves the Files drawer and the relative `Copy VERSIONS ...` in the record step fails. Only dopus CDs away and back, so only dopus breaks. Root cause level: vsh's AmigaDOS Execute path (shell/sh_exec.c sh_amiga_execute) and how it treats a bare `CD` in a backtick. The fix (vsh `CD` with no argument printing the current dir inside an Executed AmigaDOS script, versus kit-dir.dos getting the drawer another way) is an owner decision (vsh: bash first, Amiga forms only where bash leaves room). Not tested: whether the dopus Installer copied all of its files (only that it returned without WARN).

### Cleanup

Rig 99 stopped; its save build/rig-saved/99/20261112-160703 (164 MB, mine) deleted. VTC:fullkit and the run scripts removed. Scratch left (5.4 MB): full-*.log, ba-*.log, t-*.log, fulldrive.py, cdt.py in the session scratchpad. build/up-term-full.lha and build/dist-full kept.

## kit-dir under vsh and uhc online (2026-11-12)

### KIT-DOPUS-RECORD-CWD (commit 1299aec0)

Cause: under vsh a bare `CD` is bash `cd`: it prints nothing and does not move, so `Set uptkit "`CD`"` left uptkit empty; up-dopus then failed `CD "$uptkit"` and `Copy VERSIONS`, Install-Packages rc 14, up-dopus unpacked.

Forms tried on rig 99 (script: `CD RAM:kt`, `CD "Files"` caller-relative, form, then checks stayed / `CD "$uptkit"` back / `"$uptkit/install.dos"`), each run by `Execute` (AmigaDOS Shell) and by `UP-Term:bin/vsh`:
- A current form: AmigaDOS `[Ram Disk:kt/Files]`; vsh `[]`, back=no.
- B `List install.dos LFORMAT %P`: empty in both shells (relative path). C quoted variant: broken.
- D (chosen) `Set uptkit "`CD`"`, then `If "$uptkit" EQ ""` / `Set uptkit "$PWD"` / `EndIf`: OS 3.1 and OS 3.2, both shells: `[Ram Disk:kt/Files]`, stayed, back and slash all yes.
- E `Set PWD ""` first, then `"`CD`$PWD"`: works but leaves a stray local variable.

tests/test_dist_addons.py updated to the new fragment text: OK. test_dist_installer: OK.

### KIT-FULL-UHC-ONLINE (commit c5ce2f39)

Makefile dist-full writes for up-uhc: `Files/tcpprobe >NIL:`; no stack: [INFO] skip line; stack: `Which >NIL: LhA`; no LhA: [INFO] skip line; else `upkg install`. Count check counts `^ *UP-Term:bin/upkg install ` (one per package). uhc comes before dopus, dopus last.

The LhA gate was added after the first rig run: rig 99 (OS 3.2, bsdsocket on, so tcpprobe passed) has no LhA. uhc install.dos line 144 stopped with `[ERROR] LhA is not installed`, upkg left up-uhc `unpacked`, and every later `upkg install` re-ran up-uhc's configure with its stage gone (`vsh: T:upkg-stage/up-uhc/Files/install.dos: cannot open`), IPRC=14, 27 installed + uhc unpacked. Open (not fixed here): upkg retries a failed package's configure on every later install even when its stage files are gone.

### Final run (fresh rig 99, OS 3.2, purpose "up-term-ba s74: full kit Install-Packages final")

`make dist` rc 0; `make dist-full` `[OK] build/up-term-full.lha: base + 28 packages`. Base by README form: BASERC=0. `Execute Install-Packages`: the uhc LhA skip line printed, dopus last, Installer answered (gadget 90 on welcome page, gadget 90 on the 100% page), IPRC=0. `upkg status`: 27 packages `install user installed`, no unpacked (up-uhc not installed, by the skip). `List T:upkg-stage ALL`: empty.

Not tested: the online UHC download itself (needs LhA on the rig); an Amiga with no stack (rig forces bsdsocket on).

test_dist_ipk: rc 1, `pkgs: recipe /Users/spot/Code/UP-Term/upterm-ports/pkgs/git/recipe.mk has no git_SUMMARY` (the main upterm-ports checkout, not touched by these changes).

One rig start failed in between (3 boots, no wasabid answer, boot.log stops before Amarchy, after the rig refreshed Amarchy and other binaries from a newer build); the retry booted without refreshes.

### Cleanup

Rig 99 stopped; its saves under build/rig-saved/99/ deleted (199 MB and 148 MB). VTC:fullkit and run scripts removed. Scratch left under 10 MB: kd-*.log, full-*.log, kdt.py, fulldrive.py.

## Republish 2026-11-12

Release spotUP/vtcon v1.1 republished on owner approval (2026-11-12). Built in a clean worktree of vtcon at c5ce2f39 with `make dist` and `make dist-full` (UPTERM_PORTS = upterm-ports kit-main worktree). All 9 .lha archives passed `lha t`. Packages lists 28 packages; Packages.gz decompresses to the same bytes.

Replaced 40 assets with `gh release upload --clobber` as spotUP; every remote sha256 digest matches the local file. Asset count stays 41. up-term-boot.lha was not replaced (size 5944733, sha256 ba63085a... unchanged).

| Asset | Size (bytes) |
|---|---|
| Packages | 7947 |
| Packages.gz | 2675 |
| up-bitchx_1.0_m68k-amigaos.ipk | 1232936 |
| up-claude.lha | 542646 |
| up-claude_1.2_m68k-amigaos.ipk | 518480 |
| up-ctags_1.0_m68k-amigaos.ipk | 140022 |
| up-dopus.lha | 3793777 |
| up-dopus_1.1_m68k-amigaos.ipk | 3245130 |
| up-fzy_1.0_m68k-amigaos.ipk | 33894 |
| up-git_1.0_m68k-amigaos.ipk | 3373084 |
| up-hexedit_1.0_m68k-amigaos.ipk | 178106 |
| up-joe_1.0_m68k-amigaos.ipk | 592094 |
| up-jq_1.0_m68k-amigaos.ipk | 238976 |
| up-lua_1.0_m68k-amigaos.ipk | 269586 |
| up-lynx_1.0_m68k-amigaos.ipk | 922248 |
| up-mg_1.0_m68k-amigaos.ipk | 254060 |
| up-mutt_1.0_m68k-amigaos.ipk | 630574 |
| up-nano_1.0_m68k-amigaos.ipk | 388624 |
| up-ncdu_1.0_m68k-amigaos.ipk | 204336 |
| up-nc_1.0_m68k-amigaos.ipk | 52970 |
| up-nnn_1.0_m68k-amigaos.ipk | 234688 |
| up-nvim.lha | 9603717 |
| up-nvim_1.0_m68k-amigaos.ipk | 8547184 |
| up-perl_1.0_m68k-amigaos.ipk | 1714154 |
| up-pi.lha | 1133979 |
| up-picocom_1.0_m68k-amigaos.ipk | 62826 |
| up-pi_1.0_m68k-amigaos.ipk | 1089142 |
| up-python.lha | 6668065 |
| up-python_1.0_m68k-amigaos.ipk | 6213198 |
| up-sqlite3_1.0_m68k-amigaos.ipk | 1090464 |
| up-term-full.lha | 72958634 |
| up-term-src.lha | 139674153 |
| up-term.lha | 34091027 |
| up-tig_1.0_m68k-amigaos.ipk | 1035264 |
| up-uhc.lha | 24252 |
| up-uhc_1.5_m68k-amigaos.ipk | 22764 |
| up-vbcc_1.0_m68k-amigaos.ipk | 3080250 |
| up-vim_1.0_m68k-amigaos.ipk | 3604704 |
| up-zip_1.0_m68k-amigaos.ipk | 171516 |
| upkg-index.txt | 846 |

Notes change: the build line now names c5ce2f39 instead of 5f5a7061; the "Known faults in this build" paragraph (git/nvim/perl/vim not installing, uhc needing a download, Directory Opus waiting for a click, relative-drawer Shell install failing) was removed; a "Fixed 2026-11-12" paragraph was added listing those fixes and the Amarchy guard fixes. The "all 28 packages" text was checked against the new Packages file (28) and kept. The notes before this republish are in thoughts/shared/release/v1.1-notes-before-2026-11-12-republish.md.

## Rig test of republished v1.1 (2026-11-12)

Rig 99, OS 3.2 (`rig.py start --max --os 32 --purpose "up-term-ba s74: v1.1 republish rig test"`), stand-in for the owner's real hardware (owner: a rig is good enough). Only the published assets: `gh release download v1.1 -R spotUP/vtcon -p up-term-full.lha -p up-term-boot.lha` (up-term-full.lha 72958634 bytes, sha256 6a018a15...; up-term-boot.lha 5944733 bytes, sha256 ba63085a..., the unchanged one). The rig has no lha, so an Amiga LhA 1.51 (68k hunk binary, from the owner's SanctuaryBBS reference C:) was copied to the rig and called by full path; all unpacking ran inside the emulator.

### Check 1: full kit: PASS

- `VTC:v11kit/LhA -q x up-term-full.lha`: UNRC=0. 1468 files vs 1469 from a host unpack: `Files/net/bebbossh-1.46.1/patches/0017-shared-secret-normalize.patch` (34-character name, over the AmigaDOS 30-character limit) is not on the rig. Only a source patch; nothing reads it at install.
- README Shell form `CD VTC:v11kit/up-term`, `Execute Files/install.dos Files`: "[INFO] AmiSSL installed ...", "UP-Term installed. Try: NewShell XCON:0/20/640/300/UP-Term/CLOSE", BASERC=0.
- `Execute Install-Packages`: IPRC=0. "[INFO] up-uhc skipped: UHC's own installer needs LhA to unpack UHC and this Amiga has none ..." (LhA not in the path). "[INFO] up-dopus last: the Directory Opus 5 Installer window opens now ...", the last package; Installer 43.3 window "Install DOpus5" answered with gadget 90 (welcome) and gadget 90 on "Install DOpus5 (100% done)"; "Directory Opus 5 installed (DOpus5:), or found already installed." No "unpacked" line in the log.
- `upkg status`: 27 packages installed, no unpacked entry.
- Online: `upkg update` downloaded `.../releases/latest/download/Packages.gz` ("Updated source 'upterm'"); `upkg install --force-reinstall up-git`: "Downloading .../up-git_1.0_m68k-amigaos.ipk", "Reinstalling up-git (1.0)", "git installed ...", FRRC=0; `UP-Term:bin/git --version`: "git version 2.56.0", rc 0.
- Side note: `upkg remove up-git` in the same script printed "No packages removed." although up-git was installed (cause not looked at; the force-reinstall path was used instead).

### Check 3: Time Preferences under the flat look: PASS

- `Run >NIL: UP-Term:bin/amarchy-flat BOOT` (the installed kit's binary, the same line the Startup-Sequence block runs): STATUS before "flat=absent", after "flat=on calls=0 chained=0".
- `Run >NIL: SYS:Prefs/Time`, `Wait 6`: TIMERUN=0; uitree "W 0 154 57 332x199 active "Time Preferences""; wasabid ping answered; amarchy-flat STATUS "frame=hooked calls=118 flat=0 busy=0 nostk=0", "time row installed=yes calls=18".
- Screenshot (looked at, then deleted): window drawn complete (calendar November 2026, Hours/Minutes sliders, Save/Use/Cancel), title bar a flat band, buttons and the year/month gadgets flat single-line boxes, no bevels. No hang.

### Check 2: bootstrap (up-term-boot.lha, Get-UP-Term) - PASS

- Rig 99 stopped, `rig.py clear`, restarted; verified fresh: `Which upkg` rc 5, `SYS:UP-Term` object not found.
- The rig has no `lha`; LhA 1.51 (68k) from the owner's reference tree copied to `C:lha` for the unpack only.
- `lha x up-term-boot.lha` in RAM:, `CD up-term-boot`, `Execute Get-UP-Term TO Work:`.
- Log: download "100% 34091KB 2029.2KB/s 0:16", "used stack 4548", "[INFO] AmiSSL installed", "UP-Term installed. Try: NewShell XCON:0/20/640/300/UP-Term/CLOSE", rc 0.
- Note: "UP-Term installed." is printed but is not the literal last line; the last line is the hint "The Unix-style shell: type vsh in an XCON: window (exit returns to the Shell)".
- After install: `Work:` holds `AmiSSL`, `up-term` (plus a `0017-shared-secret-normalize.patch` entry left from check 1: `rig.py clear` does not clear `Work:`).
- NOT TESTED: the no-network-stack case. rig.py hardcodes `bsdsocket_library = 1` because the wasabid control channel needs it.

### Cleanup

- Rig 99 stopped and cleared; its rig-saved snapshots deleted; scratch downloads and host extract deleted.
