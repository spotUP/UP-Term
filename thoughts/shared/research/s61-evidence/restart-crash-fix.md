---
date: 2026-11-06
branch: restart-crash-fix (from origin/main 65ab6335)
verdict: FIX BUILT AND CHECKED AT BINARY LEVEL; rig behaviour UNVERIFIED (separate rig proof owed)
---

# Restart crash fix: empty _chkabort in the Ctrl-C-handling Amarchy programs

Cause: restart-crash-chkabort.md. Fix: amarchy/nobreak.c defines an empty `void _chkabort(void)`, linked
before the library, so Ctrl-C stays pending for the program's own Wait.

## Per program
| Program | Own Ctrl-C handling | Override |
|---|---|---|
| Amarchy | yes, amarchy/amarchy.c:636-637 (main Wait) | yes (AMARCHY_SRC) |
| amarchy-gui | yes, amarchy/gui.c:1432-1436, 2077 | yes (AMARCHY_GUI_SRC) |
| amarchy-flat | yes, amarchy/flat.c:2011, 2034 (then dopus_unpatch, unload) | yes (link line + prereq) |
| amarchy-index | polls Ctrl-C to stop its walk (amarchy/index.c:257); AddSemaphore is "never freed" by design (index.c:74), no patch/port/broker | no: nothing to clean up, library exit(20) is equivalent |
| amarchy-icons | none (grep SIGBREAKF: 0 hits in amarchy/icons.c); lock semaphore never freed by design (icons.c:417) | no |
| upprefs | none (prefs/ 0 hits) | no |
| amarchy-menu | handles Ctrl-C (menu.c:188, 716) but is built with m68k-cc (gcc/ixemul libc), not vc.lib; no vbcc _chkabort exists in it | no: not affected (UNVERIFIED by binary: not built here, needs UPKGUI sysroot) |

## Prototype
No vbcc header declares `_chkabort` (grep over $(brew --prefix vbcc)/targets/m68k-amigaos --include='*.h': 0 hits).
The library unit takes no arguments and returns nothing (code read in restart-crash-chkabort.md section 2), so
`void _chkabort(void);` is declared in nobreak.c before the definition. Build: 0 warnings, -warnings-as-errors on.

## Check
tests/test_nobreak.py (named after the symptom), masks the two relocated longs, also confirms the pattern is
still present in the installed vc.lib (fails with a message if vc.lib changes). List of programs: one variable,
CHKABORT_PROGRAMS (Makefile ~971), shared by the check calls; the link lines use AMARCHY_NOBREAK.
Wired: Makefile `chkabort-check` target (~1085) and `dist-check` (~2072, next to test_amarchy_stack.py).
Run: `make chkabort-check` or `python3 tests/test_nobreak.py build/amiga/Amarchy build/amiga/amarchy-gui build/amiga/amarchy-flat`.
Note: a new family program that gets the override must be added to CHKABORT_PROGRAMS (its Makefile link line is separate).

## Proof
- with fix: `[OK]` for Amarchy, amarchy-gui, amarchy-flat, rc=0.
- override removed from AMARCHY_SRC, Amarchy rebuilt: `[FAIL] build/amiga/Amarchy holds the library _chkabort (offset 0x371C)`, rc=1.
- restored, rebuilt: `[OK]` x3, rc=0.
- unchanged amarchy-icons: `[FAIL] ... holds the library _chkabort (offset 0x5F3C)` (the check tells the difference).

## Unverified
- Rig: that Break + restart no longer crashes (separate rig proof).
- amarchy-menu binary (not built here).
- Whether other callers of exit(20)-style library aborts exist (the later ledger item for programs outside the family).
