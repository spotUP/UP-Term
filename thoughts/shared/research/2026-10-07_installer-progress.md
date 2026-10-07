---
date: 2026-10-07
topic: Install.installer progress (INSTALLER-PROGRESS) and the real-Installer rig test (REAL-INSTALLER-TEST)
tags: [installer, dist, install.dos, rig]
status: draft
---

# Installer progress: design note

## What the Installers can do (sources)

- **43.3** (AmigaOS 3.1): Installer.guide 1.19, Aminet `util/misc/Installer-43_3.lha`,
  read directly. **47.19** (AmigaOS 3.2, the owner's install): its own
  `NDK3.2/ReleaseNotes/Installer-RelNotes` and `System/Installer` (`$VER: Installer 47.19`),
  both in `~/Downloads/AmigaOS-3.2-full (1)`. Its CLI template is the same as 43.3's
  (`SCRIPT/A,APPNAME,MINUSER,DEFUSER,LOGFILE,LANGUAGE,NOPRETEND/S,NOLOG/S,NOPRINT/S`).
- `copyfiles`: `source`, `dest` (made if missing), `all` / `choices` / `pattern` (one of them),
  subdrawers copied unless `files`, `infos`, `noposition`, `fonts`, `nogauge`, `newname`,
  `confirm`, `optional "force"|"askuser"|"nofail"`. A status gauge is shown unless `nogauge`.
  Whether the gauge names each file, and that it shows in NOVICE: **unconfirmed** (the guide does not say).
- `copylib`: copies only when the version is newer; `confirm` lets an EXPERT overwrite. Not
  used: the patched ixemul must replace a library that can carry the same version, and
  install.dos keeps the old one as `.orig` first.
- `complete N`: the guide says the percent is printed **in the window's title bar**. A bar
  graphic: **unconfirmed** on 43.3 and on 47.
- `working "..."`: shown under "Working on Installation" until the next display.
- `message ... (all)`: not shown in NOVICE unless `(all)` (V42.4).
- `welcome`: delays the welcome/user-level page; strings go into its help. Not used.
- `startup`: its own `;BEGIN <app>`/`;END <app>` lines (marker format **unconfirmed**).
  Not used: install.dos writes named blocks (`;BEGIN UP-Term assign`, `... python`, ...)
  in a set order that Uninstall's `unstartup.sh` and the `.before-UP-Term` byte-for-byte
  restore depend on.
- `execute`: returns the script's return code; `@ioerr` the secondary result. Output when
  started from Workbench: **unconfirmed**.
- `procedure P_X [args]` (args since V42.7), `getdiskspace` (-1 when unknown; 47.x fixed the
  overflow past 2 GB; 43.3 can still overflow), `getassign`, `getenv`, `database "cpu"`.
- Strings: at most 512 bytes per literal; `\n` breaks a line. An empty string is FALSE.
- **Images**: `showmedia`/`setmedia`/`closemedia`/`effect` are OS 3.5/3.9 (Installer 44)
  statements. 43.3 does not have them; Installer 47.2-47.19 (3.2) "prepared ... they don't
  work yet, but will be ignored" (3.2's release notes). So no image: the banner is text.
  A later 3.2.x Installer that draws them: **unconfirmed** (none found).

## Decision: install.dos stays the one list; the Installer runs it part by part

**Option A, keep install.dos the single source, run it in parts.** install.dos gets
`STEP/K` and `Lab <part>` lines; `Skip {STEP}` jumps to the part and each part ends with
`Quit 0` when a STEP was given. No STEP runs every part in the old order, so the Shell
install, `install_rig.py` (unchanged) and its checks keep working, and S:User-Startup is
written by the same lines as before, so Uninstall's byte-for-byte restore is untouched.
The Installer runs each part under its own `working` text and moves `complete` by the
part's size. The five big drawers (coreutils, Unifont, emoji, Python 15 MB, Neovim 32 MB:
48 of the ~55 MB) are `copy-<part>` parts that the Installer does not run: it copies them
itself with `copyfiles (all)`, the gauge it has. Cost: one extra file-to-file pairing
(five copy lines named twice), which `tests/test_dist_installer.py` checks on the host;
each part start re-parses install.dos (about a second each on a 68020). Covers: every
option and existing behaviour, both entry points.

**Option B, move the copying into the Installer, install.dos calls the same per-step
scripts.** Every Copy becomes `copyfiles`/`copylib`, the rest moves into per-step
`.dos` files that both the Installer and a new install.dos driver call. Cost: install.dos
rewritten into ~12 files plus a driver, each re-declaring the `.KEY` template and re-quoting
`REMOTE` through nested Executes; the Shell path would no longer copy what the Installer
copies unless the copy list is written twice anyway (the Installer cannot run AmigaDOS
Copy lines and a Shell cannot run copyfiles), so it ends with the same duplication as A
for the copies plus a larger file split. copylib's version rule is wrong for the patched
ixemul. Misses nothing A covers, adds risk to the byte-for-byte restore.

**Picked: A.** Same coverage, a fraction of the change, and the one duplication it has
(five drawer copies) is checked by a host test that fails on any drift.

## What the Installer shows now (dist/Install.installer)

1. Welcome page `(all)`: a text banner (`U P - T E R M` between star rules: centred lines,
   safe in a proportional font) and what UP-Term is.
2. Drawer (`askdir`, default: an earlier install's UP-Term: or SYS:UP-Term), help.
3. Three option pages, each with its own help: network tools; extras (Unifont, emoji,
   Python, Neovim, Shell icon); started at every boot (serial login, wasabi). The Claude
   server address (`askstring`). The console page (3.1) or the 3.2 note.
4. Disk-space check (`getdiskspace`, asks only on a positive answer that is too small).
5. "Ready to install" page with the drawer and size.
6. 13 install.dos parts + up to 5 copyfiles, each with a `working` text saying what it
   does and why, and `complete` weighted by KB (sizes of the kit of 2026-10-07).
7. Before Python/Neovim: a page saying they are the slow part (not in NOVICE).
8. Summary page `(all)`: what went where (drawer, C:, L:/DOSDrivers, LIBS:, SYS:,
   ENVARC:), the assigns made (UP-Term:, Python3:, GG:/TMP: when Install made them, read
   from install.dos's markers), the S:User-Startup blocks; then `exit` with how to start
   it (and the wasabi key).

NOVICE: every question takes its default; only the welcome and summary pages show.

## Open (to see on the rig)

- Does 43.3/47 show the copied file's name with the gauge, in AVERAGE and NOVICE?
- Is `complete` a bar or only the title-bar percent on 47?
- The UITREE labels of the Installer pages (installer_rig.py matches them loosely).
- "Nice icon positions": not done (the icons come from tools/mkicon.py; install.dos copies them).
