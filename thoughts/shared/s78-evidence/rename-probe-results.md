---
date: 2026-11-13
session: s79
topic: renamed-package upgrade probe (up-hexedit -> up-hexed)
---

# Rename probe results

Host upkg from upterm-ports 5d95a51 (patch 0062). Probe: rename_probe.sh, installed up-hexedit 1.0, feed offers up-hexed with Replaces/Conflicts/Provides up-hexedit.

- New name at 1.0 (same version as old): list-upgradable empty, `upgrade` says all up to date, up-hexedit stays. Only an explicit `upkg install up-hexed` moves it. So the current live feed strands every v1.1 user on the old names.
- New name at 1.1: list-upgradable shows up-hexedit 1.0 -> 1.1, `upgrade` removes up-hexedit and installs up-hexed 1.1, rc 0.

Conclusion: opkg follows Replaces on upgrade only when the new package has a higher version. Bumping the three renamed packages to 1.1 is enough; no opkg patch needed.

## Raw

### 1.0
```
== installed before: up-hexedit - 1.0;
== list-upgradable: 
== upgrade rc=0: All installed add-ons are up to date.;
== installed after upgrade: up-hexedit - 1.0;
== install up-hexed rc=0: Removing up-hexedit (1.0) from root...;Installing up-hexed (1.0) on root.;[PROGRESS 1/1] install up-hexed;[PROGRESS 1/1] download up-hexed;Downloading file:///var/folders/w6/hc_wf7v94_dcn98mmjb_k9fh0000gn/T//renprobe.2m1bkb/feed/up-hexed_1.0_m68k-amigaos.ipk.;[PROGRESS 1/1]
== installed after install: up-hexed - 1.0;
== files: new-marker 
```
### 1.1
```
== installed before: up-hexedit - 1.0;
== list-upgradable: up-hexedit - 1.0 - 1.1;
== upgrade rc=0: Removing up-hexedit (1.0) from root...;Upgrading up-hexed (1.1) on root.;[PROGRESS 1/1] install up-hexed;[PROGRESS 1/1] download up-hexed;Downloading file:///var/folders/w6/hc_wf7v94_dcn98mmjb_k9fh0000gn/T//renprobe.JA64ry/feed/up-hexed_1.1_m68k-amigaos.ipk.;[PROGRESS 1/1] unpack up
== installed after upgrade: up-hexed - 1.1;
== install up-hexed rc=0: Package up-hexed (1.1) installed in root is up to date.;
== installed after install: up-hexed - 1.1;
== files: new-marker 
```

## Feed re-upload at 1.1 (s79, 2026-11-13, owner decision "Bump three to 1.1")

dist/packages.txt now gives up-hexed, up-picom and up-sqlite version 1.1. The live feed (release v1.1 of
spotUP/vtcon) was updated without a full `make dist`: vtcon HEAD is many commits past the built v1.1 kit, and a
rebuild would have changed every other ipk and archive in the same release. Instead repack_renamed_1.1.py took the
live 1.0 ipks, changed only the control file's Version line (data.tar.gz byte-equal, all other control members
equal), and patched the three stanzas of the live Packages (Version, Filename, Size, SHA256sum); Packages.gz is the
same text. Uploaded the three *_1.1_* ipks, then Packages and Packages.gz with --clobber. The old *_1.0_* ipks stay
on the release so a client holding the previous index still downloads. Checked: Packages.gz fetched from
releases/latest/download matches the local file, and the sha256 of each 1.1 ipk fetched from there matches.
The offline kit up-term-full.lha still carries the 1.0 feed (self-consistent); the next kit cut picks up 1.1.
