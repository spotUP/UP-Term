---
date: 2026-11-12
topic: bootstrap kit end to end with the fixed bebboget
status: PASS
---

# Bootstrap kit (up-term-boot.lha) end to end, rig 82

## Overall: PASS

## Build of the fixed bebboget
- Source: `dist/net/bebboget-1.11/bebboget-1.11-src.tar.bz2` (equals `git archive b18e500` "Release 1.11" exactly)
  plus `thoughts/shared/s74-evidence/bebboget-all-fixes-v2.diff` (applies cleanly to b18e500).
- The diff holds FIVE changes, not three: md.cpp (VLA h2 -> `uint8_t h2[64]`, gcc 6.5.0b `move.b (sp)+`
  miscompile of memcmp), fastmath.cpp (memset of Montgomery scratch in `fill()` and `oddModPow`),
  bebboget.cpp (HTTP status >= 400 is an error, exit code 10 on failure), biginteger.cpp (NIST P-384
  reduction adds missing S5 terms c[20..23]), pkcs6.cpp (`readTimes` uses counter `n`, not `!ci.validFrom`).
- Command: `PATH=$HOME/opt/amiga/bin:$PATH nice -n 19 make Release=1 TESTS= CC="m68k-amigaos-gcc -include <compat.h>" Release/bebboget`
  with `m68k-amigaos-gcc (GCC) 6.5.0b 20260819091705`.
- Unstripped 183068 bytes; `sp@+` byte scan: only `bebboget.o 3f6: orb d2,sp@+` (same as hunt build, data).
- Stripped (`m68k-amigaos-strip`): 156020 bytes (old 138256),
  sha256 `cc5dc019e6397931eff0e8722cb0d3563d781ef9a1b7e0971c72793a24baa069`.
- Shipped diff `dist/net/bebboget-1.11/bebboget-up-term.diff`: 6242 bytes, 5 files, made against the shipped
  tarball; fresh extract + `patch -p1 --binary` gives the build tree byte for byte.

## Kit changes
- `dist/net/bebboget-1.11/bebboget` (new binary), `bebboget-up-term.diff` (new), `SOURCE.txt` (modified build
  described, five changes listed, bebboget00 still upstream).
- `Makefile` dist-boot: diff copied into the boot archive, check count 6 -> 7. No rm added.
- `tools/kitsize.py`: NOT_COPIED gains `net/*/*.diff`.
- `dist/LICENSES.txt`: bebboget row names the modified build and the diff.
- `dist/Get-UP-Term`: header (TO = download/unpack drawer, install.dos installs to SYS:UP-Term), separate
  failure checks for bebboget (rc) and `lha t` (damaged archive), clearer Echo lines.
- install.dos not changed.

## Host tests (rc 0 each)
- `tests/test_dist_bebbossh.py` OK; `tests/test_dist_installer.py` OK (skipped=1);
  `tests/test_install_dos_room.py` OK; `tests/test_dist_kitsize.py` OK, 15 tests (skipped=1).

## Archive
- `make dist-boot` rc 0: `[OK] build/up-term-boot.lha: 255748 bytes`
- sha256 `f0b2d04bb9a15ac383a759422e5bb8ecc164893e42583fb98f190bf34001cd91`
- Content: COPYING 35148, Get-UP-Term 3016, SOURCE.txt 1897, bebboget 156020, bebboget-up-term.diff 6242,
  cacert.pem 200116, installcerts 68936 (7 files + dir).

## Rig 82 test
Start: `UPTERM_RIG=82 python3 tools/rig/rig.py start --max --os 32 --purpose "up-term-ba s74: bootstrap end to end"`.
Driver: `tools/rig/ami.py exec/put` (scratchpad drive.sh, phases prep/main/post/fail/again).

| Criterion | Evidence | Verdict |
|---|---|---|
| Clean pre-state | `Which lha` rc 5; `List ENVARC:certs` rc 20 "object not found" | PASS |
| installcerts on first run | "Installing the root certificates (ENVARC:certs) ...", 118 "installing" lines; after: `List ENVARC:certs` 113 files, 118K | PASS |
| Download over https | `Execute Get-UP-Term TO Work:dl`; bebboget "used stack 4548", no error | PASS |
| Archive test before unpack | `lha -q t` passed, no "damaged" line | PASS |
| install.dos runs and installs | AmiSSL installed, copy list, Ask defaults, "UP-Term installed. Try: NewShell XCON:0/20/640/300/UP-Term/CLOSE"; `GETRC=0` | PASS |
| Install target SYS:UP-Term | `Assign UP-Term: EXISTS` -> `Empty-12MB-FFS:UP-Term`; `SYS:UP-Term/bin/upkg` 930248 bytes | PASS |
| Archive removed after unpack | `List Work:dl`: only drawer up-term | PASS |
| HTTP 404 fails | ".../latest/download/no-such-dir/up-term.lha": "error: HTTP status 404", "Get-UP-Term: the download failed (is the TCP/IP stack running?).", GETRC=20, stale up-term.lha gone | PASS |
| Bad host fails | `https://no-such-host.invalid/...`: "[ERROR] no server hello received", "error -996", failure line, GETRC=20, stale file gone | PASS |
| Repeat full download | `bebboget -O -D Work:dl2 -o up-term.lha <feed>/up-term.lha` rc 0, 34083462 bytes; `lha t` "Operation successful." (lha rc line not isolated) | PASS |

ALL not run (not requested).

## Cleanup
- Rig 82 stopped (`rig.py stop`: "stopped"); saved tree `build/rig-saved/82/20261112-111902` (1.1 MB) deleted.
- Scratch deleted: v11 (70 MB), dg (2.7 MB), b18. Scratchpad left: about 24 MB (build trees, logs).

## Notes
- `cp -R dist/net $(KIT)/Files/net` (Makefile ~2084) also copies the git-ignored cacert.pem into the kit
  (pre-existing, not changed).
- install.dos line 785 comment names bebboget 1.11 (not changed, by rule).
