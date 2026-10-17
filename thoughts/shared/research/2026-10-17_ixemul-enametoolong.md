# ixemul: ENAMETOOLONG instead of a silently shortened name (2026-10-17)

Follow-up of `2026-10-17_git-mmap-ram.md`. Owner decision: ixemul refuses every create
of a name the handler would shorten, with ENAMETOOLONG.

## Truncation confirmed (rig 3, Kickstart 40.63, Workbench 40.42, AmigaDOS commands, no ixemul)
- `Echo >RAM:nt/abcdefghijklmnopqrstuvwxyz0123456789AB x` then `List RAM:nt`: the file is
  `abcdefghijklmnopqrstuvwxyz0123` (30 characters).
- `Type RAM:nt/abcdefghijklmnopqrstuvwxyz0123456789ZZ` (another 38-character name) prints `x`:
  the handler matches LOOKUPS on the first 30 characters as well. So a long name both lands under
  a shortened name and finds an unrelated object that has the same first 30 characters.

## Layer and design
- Where: ixemul's `__plock()` callbacks, the one place where every create packet is sent with
  the parent directory lock and the final name component in hand: `__open_func` (`__open.c`, any
  action other than MODE_OLDFILE: open/creat with O_CREAT), `__mkdir_func` (`mkdir.c`),
  `__rename_func` (`rename.c`, target side), `__link_func` (`__make_link.c`: link and symlink).
  Not in git (a workaround in one consumer), not in open.c's path string (the final component as
  the handler sees it is only known inside `__plock`).
- Querying the handler's maximum name length was rejected: AmigaOS 3.x has no packet for it
  (ACTION_FILESYSTEM_ATTR / FSA_MaxFileNameLengthR is OS4 only; DosEnvec has no name length), and
  the limit differs per handler (RAM: 30, FFS 30, long-name FFS 107, CrossDOS shorter), so a
  hard-coded 30 would be wrong both ways.
- Chosen: compare the name the handler stored with the one asked for.
  `__name_truncated(sp, handler, dir, name)` in `__plock.c`: ACTION_LOCATE_OBJECT of the name in
  the parent directory, dos.library `Examine`, UnLock; 1 when the stored name is shorter and a
  case-insensitive prefix of the requested one, 0 when it matches, -1 when nothing is there (or it
  cannot be examined: a console, a soft link).
  - Before the create: 1 -> fail at once, nothing touched. This matters because lookups match on
    30 characters too: without it, open(O_TRUNC) would truncate and rename() would unlink
    (rename.c unlinks an existing target on ERROR_OBJECT_EXISTS) an UNRELATED file whose name is
    the shortened one.
  - After the create, only when the name was new (-1 before): 1 -> undo (open: ACTION_END on the
    new handle, then ACTION_DELETE_OBJECT; mkdir: UnLock + delete; rename: rename the object back
    to its source name; link: delete the link) and fail.
  - The failure is IoErr `IX_ERROR_NAME_TOO_LONG` (7272, private, after ixemul's 4242/5252/6262;
    AmigaDOS has no "name too long" code), mapped to ENAMETOOLONG in `__ioerr_to_errno.c`.
- Cost: a create of a new name costs one LOCATE before and LOCATE + EXAMINE + UNLOCK after; a
  create over an existing name costs LOCATE + EXAMINE + UNLOCK. Plain opens (MODE_OLDFILE), reads,
  stat are unchanged.

## Regression test
- vtcon `tests/amiga/ixnamelen.c` (`make build/amiga/ixnamelen`), run by
  `UPTERM_RIG=<n> python3 -I tools/rig/ixnamelen_rig.py [--orig]` (installs the build295
  ixemul first in LIBS: as ixpty_rig does; `--orig` uses the rig's DH0:Libs ixemul).
  Cases on RAM:: open O_CREAT|O_TRUNC, open O_CREAT, mkdir, rename, link, symlink of a 38-character
  name (ENAMETOOLONG, no 30-character entry left); a file already under the 30-character name
  (open O_TRUNC and rename refused, the file and the rename source unchanged); a 30-character
  create + rename still works.
- Results: see "Verdicts" below.

## Not covered (owner's call if wanted)
- Lookups: open(O_RDONLY), stat, unlink of a 38-character name on RAM: still find an object with
  the same first 30 characters. With creates refused, ixemul programs cannot make such an object
  any more, but a file made by an AmigaDOS command can still be found under a longer name.
- open(O_CREAT|O_EXCL) when an object with the same first 30 characters exists: open.c's stat()
  finds it first and returns EEXIST, not ENAMETOOLONG (nothing is changed).
- symlink on RAM: of this OS fails with ENOENT before and after the change (soft links not made
  by this RAM:), so the symlink case is skipped there; the code path is the same `__link_func`
  as link().

## Verdicts
- Before (rig 3, `--orig`, the rig's DH0:Libs ixemul): `ixnamelen: FAIL (9)`. Every create of the
  38-character name succeeded under the 30-character name, rename lost `keep`, open(O_TRUNC) and
  rename clobbered the unrelated 30-character file; the 30-character control passed. The test fails
  before the fix.
- After: NOT RUN. The ixemul change (ixemul-vtcon working tree, uncommitted: `__plock.c`,
  `ixemul.h`, `__ioerr_to_errno.c`, `__open.c`, `mkdir.c`, `rename.c`, `__make_link.c`) is not
  compiled yet: Docker Desktop stopped answering (`docker image inspect` and `docker ps` hang; its
  backend log shows "no space left on device" at 21:10, disk 4.4 GiB free afterwards), so
  `KEEP=1 sh docker/build.sh` never started. Next: restart Docker Desktop, run the build, then
  `UPTERM_RIG=3 python3 -I tools/rig/ixnamelen_rig.py` (expect `ixnamelen: [OK]`), then commit the
  ixemul change.
