# B3 chkabort suite audit

Dispatch table: 147 `$(BUILD)/amiga/<name>:` targets in Makefile (`grep -c`, counted again from `programs()` in tests/test_chkabort_suite.py: 147).
Binary column: `python3 tests/test_nobreak.py build/amiga/<name>` on all 147 (22 were unbuilt; the 4 with state patterns were built, the rest are
probes with no state pattern). 47 binaries carry vc.lib `_chkabort`.

## Fixed (join CHKABORT_PROGRAMS, link $(AMARCHY_NOBREAK))
| program | _chkabort | holds state | verdict |
| amarchy-icons | yes | ObtainSemaphore on public amarchy-icons.lock held for the whole run (amarchy/icons.c:419-425): exit(20) leaves it held, the next copy blocks forever | fixed |
| upprefs | yes | StartNotify requests on a port (prefs/upprefs.c:807): exit(20) leaves notify armed on a freed port | fixed |
| upgetty | yes | serial OpenDevice, two ports, PTY shell process (device/upgetty.c:242-265); printf at 248/259/265/269 while open | fixed |
| top | yes | AddIntServer VERTB (proc/top_amiga.c:68) with fputs/printf in the loop: exit(20) leaves the server armed in freed code | fixed |
| allocwatch | yes | SetFunction AllocMem patch, printf at tests/amiga/allocwatch.c:54 before restore | fixed |
| patchcon | yes | SetFunction RawKeyConvert, printf at tests/amiga/patchcon.c:28 before restore | fixed |
| lvoprobe | no after relink (built new) | SetFunction on intuition LVOs, printf at tests/amiga/lvoprobe.c:52 | fixed (was missing; linked with nobreak) |
| frameprobe | no after relink (built new) | frame class dispatcher slot patched, printf at tests/amiga/frameprobe.c:127-139 | fixed |
| sizewatch | yes | IND_ADDHANDLER, printf between add and remove (tests/amiga/sizewatch.c:65-80) | fixed |

## Exempt (also named with the reason in EXEMPT in tests/test_chkabort_suite.py)
amarchy-menu: test_nobreak.py says [OK], no vc.lib unit (gcc/ixemul build). amarchy-index: has _chkabort; its only state is a public record never freed by design, and
a dead scanner is told from a live one by its task (ix_scan_live, amarchy/index.c:80-95). UPConsole: has _chkabort; stdio runs before the patch and after the
persistent semaphore. addrowner: SetFunction only in a comment. taskdump: read lock, no stdio while held. inhandlers/inputlist: handler removed before any stdio.
deafport, palprobe, AmTile, amnotify, UPTerm, vsh, vsh_hw, vtcon-handler: no vc.lib _chkabort in the binary. rexxbound: client only, opens no host port.
All other programs with _chkabort (closeread, cudump, devwho, memprobe, rkcprobe, sigprobe, stamp, flatprobe, penprobe, wbpens, ...) hold only private resources
(own console.device unit, own message port, a screen lock): exit(20) leaks them but leaves nothing armed for another task, so no next start can crash.

## Regression test
tests/test_chkabort_suite.py (wired into `make chkabort-check`): static, reads the Makefile dispatch table and each program's linked C sources.
- fail before (CHKABORT_PROGRAMS reverted to Amarchy amarchy-gui amarchy-flat): exit 1, 7 x [FAIL] (allocwatch, amarchy-icons, lvoprobe, patchcon, sizewatch, top, upprefs),
  last line `[INFO] 147 Amiga programs, 3 in CHKABORT_PROGRAMS, 16 exempt`
- pass after: exit 0, `[OK] every program that holds system state is listed or exempt`; `make chkabort-check` exit 0, 12 binaries [OK].
Gap: the STATE regex does not include OpenDevice or dispatcher-slot writes, so upgetty and frameprobe are protected by test_nobreak.py (listed) but not by the completeness check.
