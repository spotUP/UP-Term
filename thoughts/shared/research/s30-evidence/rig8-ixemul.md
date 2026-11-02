# rig 8 ixemul check (static only; rig 8 not running)

Live check impossible: ami.py ping/exec on UPTERM_RIG=8 -> ConnectionRefused. build/rig8/watchdog.log:
"2026-11-02 03:38:58 idle 60 min: stopping rig 8". Only fs-uae running is rig35. Nothing started, nothing written.

On-disk copies under build/rig8 (md5 first 8):
- os-32/Libs/ixemul.library a8a7bf4c, ixnet.library 9247e6d4 (both Nov 1 03:08) = system Libs: CURRENT
- vtc/ixp6/ixemul.library a8a7bf4c (Nov 1 02:29), ixnet.library 9247e6d4: CURRENT
- vtc/distkit/Files/libs: a8a7bf4c / 9247e6d4: CURRENT
- STALE on disk, not system dirs: vtc/kit/UP-Term/Files/libs/ixemul f54fe081 (Oct 7), vtc/kit-python/Files/libs a2628afc, vtc/sa/ixkit 4e4a0729, vtc/ixnew ede03dde, vtc/ix295 8888df8d, vtc/ixold 68aee359, vtc/ixdbg 5becc2e2
Version strings: strings found no "ixemul.library N" idString in any file (not resolved).
boot.log has no ixemul note and no [WARN] drift line (only the go/shadows/XCON/assigns/wasabid markers).
Open: which dir resolves LIBS: at runtime and the resident version cannot be read until rig 8 is started.

## s30 resident check (rig 8 restarted 2026-11-02, fs-uae --os 32, read-only)
- Version ixemul.library FULL -> 80.1 (10/13/2026); Version ixnet.library FULL -> 80.1 (10/13/2026). Repeated after Avail FLUSH: same.
- Assign EXISTS LIBS: first dir VTCX:ixp6, then Libs:, Classes:, ncurses ixlibrary/sys/libs, VTCX:amissl.
- LIBS: ix#? sizes: ixemul.library 201256 (host build295 file 201256), ixnet.library 19932.
- VTCX:ixp6 files fetched via ami.py get: ixemul md5 a8a7bf4c, ixnet md5 9247e6d4 = build295 (host ixemul a8a7bf4c). Host file contains string "80.1".
- boot [WARN] lines: Directory Opus 5.104 not installed in VTC: (run rig.py dopus); Workbench stayed PAL (W43). No ixemul drift line.
- Verdict: CURRENT. Stale copies listed above are not on the LIBS: path ahead of ixp6.
