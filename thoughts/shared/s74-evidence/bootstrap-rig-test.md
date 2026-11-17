---
date: 2026-11-12
topic: bootstrap kit build/up-term-boot.lha on an AmigaOS 3.2 rig
rig: UPTERM_RIG=80, `rig.py start --os 32 --fresh --purpose "up-term-ba s74: bootstrap kit Get-UP-Term test"` (Kickstart 47.115, Workbench 47.2, 68060 JIT, FS-UAE bsdsocket emulation)
checkout: vtcon main c950fd39
verdict: FAIL at question 3 (bebboget rejects every certificate; its return code is 0 on every failure)
---

# Bootstrap kit rig test (s74)

## Was the system without UP-Term?
No. The rig harness had already assigned `UP-Term: -> VTCX:Apps`, `VTC: -> VTCX:`, `LIBS: + VTCX:ixp6 + VTCX:amissl`
(rig start "copying rig 1's VTC: drawer", "refreshed vtcon-handler"). `SYS:` (Empty-12MB-FFS:, a host
drawer, 4 GB) held no UP-Term and `C:` held no lha. ENVARC:certs did not exist. The download, unpack and
install.dos steps were tested regardless; install.dos reassigned `UP-Term:` to `Empty-12MB-FFS:UP-Term`.
bebboget uses its own TLS code (readme), not AmiSSL, so the AmiSSL in LIBS: did not help it.

Kit staging: `build/up-term-boot.lha` copied to the host drawer `build/rig80/vtc/boottest/`, unpacked on the
Amiga with `VTC:boottest/tools/LhA -q x VTC:boottest/up-term-boot.lha RAM:` -> `RAM:up-term-boot` holds
installcerts 68936, cacert.pem 200116, bebboget 138256, SOURCE.txt, Get-UP-Term 2039, COPYING. The kit's
Get-UP-Term has the feed URL filled in (`https://github.com/spotUP/vtcon/releases/latest/download/up-term.lha`).

## Q1 lha in the path on clean 3.2: FAIL as expected, script handles it - PASS
`Which lha` -> rc 5. `CD RAM:up-term-boot` + `Execute Get-UP-Term TO Work:`:
```
Get-UP-Term: lha is not in the path (Aminet util/arc/lha.run). Nothing was changed.
outer rc=20
```
lha put on the rig: `~/Downloads/testbbs/C/LHA` (`$VER: LhA 2.15 68020+ Jan 3 2011`, 72216 bytes, the
Aminet util/arc/lha.run build) copied to `C:lha`.
Note: the kit itself is an .lha, so a user without lha cannot unpack the kit to reach this message at all.
The check only helps someone who unpacked the kit elsewhere.

## Q2 installcerts cacert.pem: PASS
Run from the kit drawer as the script does (`CD RAM:up-term-boot`, `installcerts cacert.pem`). No prompt.
Output: one `installing: sys:Prefs/Env-Archive/certs/<name>.cer` line per cert; `installcerts rc=0`.
Afterwards `List ENVARC:certs` -> `118 files - 124K bytes`. (It writes to the literal `sys:Prefs/Env-Archive`,
which is ENVARC: on a standard system.)

## Q3 bebboget, certificate check on: FAIL (hard)
Command, exactly the script's: `bebboget -O -D "Work:" -o up-term.lha https://github.com/spotUP/vtcon/releases/latest/download/up-term.lha`
```
the certificate is not valid today:
Owner     : github.com
Issuer    : Sectigo Public Server Authentication CA DV E36, Sectigo Limited, GB
valid from: Fri Oct 30 05:00:00 2026
valid to  : Fri Oct 30 05:00:00 2026
(a)bort or (c)continue (a/c): error: certificate not accepted
bebboget rc=0
LIST: Could not get information for "Work:up-term.lha".
```
Real certificate (host openssl): notBefore Oct 30 00:00:00 2026 GMT, notAfter Jan 27 23:59:59 2027 GMT.
The rig clock was right (Date: Thursday 12-Nov-26). With `c` lines fed on stdin the intermediate fails the same
way: `Sectigo Public Server Authentication CA DV E36 ... valid from: Mon Mar 22 05:00:00 2021 / valid to: Mon Mar 22 05:00:00 2021`
(the real one runs to 2036), then `(a)bort or (c)continue (a/c): error: certificate not accepted`.
Diagnosis: bebboget 1.11 shows "valid to" equal to "valid from" for every certificate (leaf and intermediate),
so it reads or keeps notAfter wrong and every certificate fails the "valid today" check. This is a bebboget
bug, not a missing root (the roots were installed in Q2). Whether it is specific to OS 3.2 (time/locale
functions) or happens on 3.1 too: unverified. Answering `c` from a redirected stdin did not get past it
(unverified whether a typed `c` in a real console does; the newline may count as the answer).

Second defect: bebboget returns rc 0 on failure, so `If WARN` never fires:
- certificate rejected: `bebboget rc=0`, no file written.
- bad URL (`.../latest/download/no-such-file.lha`, with --sloppy): the GitHub 404 page is saved as the
  output file (`bad.lha 225900 bytes`), `sloppy bad-url rc=0`.
The script would then reach `lha x` (no file / HTML file) and report "lha could not unpack", a wrong message.

With `--sloppy` only (to test the rest; the task forbids it as the answer):
- `-D "RAM:"`: `RAM:up-term.lha 34083462` in 15 s (02:24:42 -> 02:24:57, about 2.2 MB/s); redirect via
  release-assets.githubusercontent.com followed; size equals the host's content-length 34083462; `lha t rc=0`, `Operation successful.`
- `-D "Work:dl"` (drawer, no trailing slash): `Work:dl/up-term.lha 34083462`, rc 0. Path joining is right for a
  volume name and a drawer path.

## Q4 unpack via UPTGET: and install.dos: PASS (on the --sloppy download)
```
Assign UPTGET: "Work:dl"
lha -q x UPTGET:up-term.lha UPTGET:      -> lha x rc=0, UPTGET:up-term/Files/install.dos 65816
Execute UPTGET:up-term/Files/install.dos UPTGET:up-term/Files      -> install.dos rc=0 (under 150 s)
```
install.dos ran to the end: `[INFO] AmiSSL: LIBS:amisslmaster.library version 5 is missing or older, running install-amissl.dos.`,
`[INFO] AmiSSL installed`, the copy list, then `UP-Term installed.`; `Assign UP-Term: EXISTS` -> `Empty-12MB-FFS:UP-Term`.
It asks two questions (Ask, y/N): "Should UP-Term also serve CON: and RAW: (every new Shell window)? y/N" and
"Should console.device be UP-Term's too (every program's console window)? y/N". With no console input both
took the default No. A script run can answer them with install.dos switches: CONSOLE/NOCONSOLE, DEVICE/NODEVICE.
Note: install.dos installs to `SYS:UP-Term` (its DEST default), not to the TO drawer; TO only holds the
download and the unpacked kit (about 110 MB). A small SYS: partition gets install.dos's own "nothing has been
copied" refusal (its room check); not tested here (SYS: was a 4 GB host drawer).

## Q5 ALL: not run (stopped at the Q3 hard failure, per brief)

## Proposed changes (not applied)
1. Root cause: fix bebboget's certificate validity (notAfter) in the bebboget 1.11 source shipped in
   up-term.lha (Files/net/bebboget-1.11), or report upstream; check whether OS 3.1 shows the same. Until
   then the kit cannot download with the check on. `--sloppy` works but drops the certificate check (owner decision).
2. Do not trust bebboget's rc. Sketch for dist/Get-UP-Term:
```
-bebboget -O -D "{TO}" -o up-term.lha @FEED@/up-term.lha
-If WARN
-  Echo "Get-UP-Term: the download failed (is the TCP/IP stack running?)."
-  Quit 20
-EndIf
-Assign UPTGET: "{TO}"
+Assign UPTGET: "{TO}"
+Delete >NIL: UPTGET:up-term.lha QUIET
+bebboget -O -D "{TO}" -o up-term.lha @FEED@/up-term.lha
+; bebboget 1.11 returns 0 on a rejected certificate and saves an HTTP error page as the file: test the archive
+lha >NIL: -q t UPTGET:up-term.lha
+If WARN
+  Delete >NIL: UPTGET:up-term.lha QUIET
+  Assign UPTGET: REMOVE
+  Echo "Get-UP-Term: the download failed (TCP/IP stack running? certificate accepted?)."
+  Quit 20
+EndIf
```
3. Pass the console answers through, or document that install.dos asks two y/N questions (e.g. accept
   CONSOLE/NOCONSOLE/DEVICE/NODEVICE and DEST on Get-UP-Term's .KEY and hand them to install.dos), and say in the
   header that UP-Term itself goes to SYS:UP-Term unless DEST is given.
4. The kit is an .lha: the lha check cannot be reached by a user without lha. Say in the release text that
   lha (Aminet util/arc/lha.run, self-extracting) comes first.
