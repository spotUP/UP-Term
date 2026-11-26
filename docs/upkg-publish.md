# Publishing a package to the upkg third-party feed

For tools outside vtcon (Release Dropper / makerelease) that ship an Amiga program as a upkg package.
vtcon owns this spec and the tool: `tools/pkgs.py build-ipk` builds the .ipk, `tools/pkgs.py feed-index`
writes the feed index. Callers run these two commands and do not write their own ipk or Packages code.

## Where packages go: the second feed

- The UP-Term feed (`src/gz upterm .../spotUP/vtcon/releases/latest/download` in `dist/upkg.conf`) carries
  only the packages of `dist/packages.txt`, built by `make dist-ipk`. Third-party packages never go there.
- Third-party packages go to a second feed: the GitHub release assets of `spotUP/upkg-feed`
  (owner decision 2026-11-26), one fixed release tag `feed`, marked latest, read through
  `https://github.com/spotUP/upkg-feed/releases/latest/download`.
- upkg reads it through a second line in `dist/upkg.conf`:
  `src/gz extra https://github.com/spotUP/upkg-feed/releases/latest/download`.
  That line lands only after the repository and the `feed` release exist (until then `upkg update`
  would report a missing list on every machine).
- Status 2026-11-26: the repository does not exist yet; the owner creates it (commands below).

## Package layout (.ipk)

An .ipk is what `pkgs.py build-ipk` writes; the layout is opkg's:

- `ar` archive, members in this order: `debian-binary` (`2.0\n`), `control.tar.gz`, `data.tar.gz`.
- `control.tar.gz`: `./`, `./control`, plus `./postinst` / `./prerm` when given (see Maintainer scripts).
- `data.tar.gz`: the stage tree. upkg's dest root is `/`, and ixemul reads `/X/y` as `X:y`, so the
  top-level drawers of the stage tree are AmigaDOS volume or assign names:

      stage/
        Libs/muimaster.library          -> Libs:muimaster.library
        UP-Term/pkg/foo/foo             -> UP-Term:pkg/foo/foo
        C/foo                           -> C:foo

  The top-level drawers get no tar entry (an assign is not created; an entry for it makes opkg warn
  "Can't create"), every drawer below them does.
- Archives are deterministic: mtime 0, uid/gid 0, sorted entries, gzip mtime 0. A rebuild of the same
  stage tree and control file is byte-equal.

## Control file (input to build-ipk)

`Key: value` lines, one line each (no continuation lines).

| Field | | Rule |
|---|---|---|
| Package | required | `[a-z0-9][a-z0-9+.-]*`, at most 20 characters, must not start with `up-` (reserved for the UP-Term feed) |
| Version | required | `<upstream>-r<rev>`: upstream starts with a digit, `[0-9A-Za-z.+~]`; rev a whole number from 1. Bump rev for a package-only change at the same upstream version |
| Architecture | required | `m68k-amigaos` (68020+), `m68k-amigaos-fpu` (68040/060 with FPU), or `all` |
| Maintainer | required | `Name <url or mail>` |
| Description | required | one line |
| Depends | optional | `a, b`: opkg package names, from either feed |
| Replaces, Conflicts, Provides | optional | as opkg |
| Launch | optional | the command line `upkg browse` runs on Enter |
| Installed-Size | not allowed | build-ipk computes it (KiB of the data files) |

Any other field is refused. The written control is the input fields in the order above, with
Installed-Size after Maintainer.

## Name length rules (AmigaDOS 30-character limit)

- File name `<Package>_<upstream>_<Architecture>.ipk` (no `-r<rev>`, as in the UP-Term feed): at most
  30 characters. The revision is in Packages; a new revision at the same upstream version replaces
  the asset of the same name (`--clobber`).
- Package at most 20 characters: opkg keeps `<Package>.conffiles` (and `.control`, `.list`) in
  `UP-Term:pkg/upkg/info`.
- Every path component of the stage tree: at most 30 characters.
- build-ipk refuses a package that breaks any of these.

## Maintainer scripts

A plain-copy package has none. opkg records every data file in `<Package>.list` and deletes them on
`upkg remove`; drawers it created are removed when empty. Unverified on a rig: remove of a package
with no prerm has not been run on the Amiga yet.

A package that needs more (an Assign, a user-startup line, a prefs change) passes its own scripts:
`--postinst <file>` and `--prerm <file>`. They are vsh scripts (`#!/bin/sh` first line), run by upkg
after the unpack and before the delete, like the UP-Term feed's. Rules:

- postinst exits 0 on success; a non-zero exit marks the install failed.
- prerm undoes exactly what postinst added (the Assign, the startup line) and exits 0 even when
  there is nothing to undo, so a remove or an upgrade is never blocked.
- No `install.dos` and no `T:upkg-stage`: that is the UP-Term feed's staged install. Files go in
  place through the data tree; scripts only do what a file copy cannot.
- A destination that is an assign must exist when the tree unpacks. `Libs:`, `C:`, `S:`, `SYS:`,
  `ENVARC:` always do. `MUI:` does not on a fresh machine: stage MUI files under `SYS/MUI/...` and
  have postinst add `Assign MUI: SYS:MUI` (and its S:User-Startup line); prerm removes both.

## Packages index (feed-index)

`pkgs.py feed-index <dir>` reads every `<dir>/*.ipk`, re-checks its control and file name, and writes
`<dir>/Packages` and `<dir>/Packages.gz` (gzip mtime 0). One stanza per .ipk, sorted by file name,
blank line between stanzas:

    Package: foo
    Version: 1.2-r1
    Architecture: m68k-amigaos
    Maintainer: Someone <https://example.org>
    Installed-Size: 12
    Depends: bar
    Filename: foo_1.2_m68k-amigaos.ipk
    Size: 4321
    SHA256sum: <hex>
    Description: Foo for the Amiga

The index is regenerated from all .ipk files in the directory, so the publish flow keeps a local
mirror of the feed: download the current assets (or keep the directory), add or replace the new .ipk,
remove superseded ones, run feed-index, upload.

## Commands

Build one package and index a feed directory (run from a vtcon checkout, Python 3.9+):

    python3 tools/pkgs.py build-ipk --stage <stage dir> --control <control file> --out <feed dir> \
        [--postinst <file>] [--prerm <file>]
    python3 tools/pkgs.py feed-index <feed dir>

build-ipk prints the path of the .ipk it wrote; both exit non-zero with a message on a rule break.

Local dry-run feed (no network, not in git): `/Users/spot/Code/UP-Term/vtcon/build/upkg-feed-dryrun`,
seeded with a sample package `hello_1.0_all.ipk`:

    python3 tools/pkgs.py build-ipk --stage /path/stage --control /path/control \
        --out /Users/spot/Code/UP-Term/vtcon/build/upkg-feed-dryrun
    python3 tools/pkgs.py feed-index /Users/spot/Code/UP-Term/vtcon/build/upkg-feed-dryrun

## Publish (after the owner created the repository)

Auth: the gh CLI token (`gh auth login` as spotUP, or `GH_TOKEN` in the environment). No other key.
Order, so a client never sees a Packages entry whose file is not there yet:

    gh release upload feed <feed dir>/*.ipk --repo spotUP/upkg-feed --clobber
    gh release upload feed <feed dir>/Packages <feed dir>/Packages.gz --repo spotUP/upkg-feed --clobber
    # then delete assets no longer in Packages:
    gh release delete-asset feed <old>.ipk --repo spotUP/upkg-feed --yes

Owner, once:

    gh repo create spotUP/upkg-feed --public --description "upkg third-party package feed for UP-Term"
    gh release create feed --repo spotUP/upkg-feed --title "upkg feed" --notes "Third-party upkg packages" --latest

Then vtcon adds the `src/gz extra ...` line to `dist/upkg.conf` and ships it with the next kit.

## Signing

None today. upkg (opkg 0.9.0, WITH_GPGME=OFF) does not check signatures; integrity is https plus the
SHA256sum in Packages. A later usign port (research/2026-10-18-opkg-port.md, P10.8) would add
`Packages.sig`; feed-index would then sign. Nothing to do for callers now.
