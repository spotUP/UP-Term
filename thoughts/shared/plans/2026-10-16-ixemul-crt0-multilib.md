---
date: 2026-10-16
topic: ixemul link with -m68020 picks newlib's crt0.o
status: fixed in the gcc driver spec (see Fix)
---

# ixemul + -m68020 links newlib's crt0.o

Symptom: `m68k-amigaos-gcc -mcrt=ixemul -m68020 [-m68881]` fails at link:
undefined `__nocommandline`, `DOSBase`, `ixemulbase`.

Cause (measured with `-Wl,-t`):
- STARTFILE_IXEMUL_SPEC names `crt0.o%s`. gcc's startfile search
  (gcc.c for_each_path) tries the multilib subdirectory of every prefix
  before any bare prefix.
- ixemul's `make install` writes crt0.o, bcrt0.o, lcrt0.o, ... only to the
  base `ixemul/lib/`.
- newlib ships its own crt0.o in `m68k-amigaos/lib/libm020/`,
  `lib/libm020/libm881/`, `libb/libm020/` and others.
- So `-m68020` finds `lib/libm020/crt0.o` (newlib's) before
  `ixemul/lib/crt0.o`. Plain, `-m68060` and `-fbaserel` happened to work
  (no newlib file of that name in the selected multilib dir).
- Libraries were never affected: ld resolves `-lc`/`-lm` through the
  `-L%:sdk_root(ixemul/lib)` path first (traced: ixemul libc.a, libm.a).
  libnix is not involved (its startfiles have unique names, ncrt0.o ...).

Options compared:
1. Driver spec names ixemul's startfiles by full path,
   `%:sdk_root(ixemul/lib/crt0.o)`. ixemul's startfiles are CPU
   independent and live only in the base dir, so this is exact. Chosen.
2. ixemul install copies the startfiles into every multilib dir gcc can
   select. Data-layer patch, breaks again for any new multilib dir.
3. Change the search order in gcc.c. Generic, changes every target's
   startfile lookup. Rejected.

Fix: gcc branch fix/ixemul-startfile (projects/gcc, off amiga6), same
commit on the installed tree's branch fix/ixemul-startfile-sret
(cpython-amiga/build/gcc/src). Regression test:
`make test ONLY=ixemul_startfile` (tests/test_ixemul_startfile.py) links
a printf+sqrt hello with 7 flag sets and asserts the traced startup object
is `m68k-amigaos/ixemul/lib/<name>` and the link succeeds; before the fix
`-m68020` and `-m68020 -m68881` fail.

Follow-up: the `build/amiga/fpuinf` rule no longer needs its separate
compile/link workaround.
