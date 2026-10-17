# git on RAM: (2026-10-17) - partial root cause, mmap EACCES not reproduced

## Evidence (rig 3, git 2.56.0 kit binary, ixemul-vtcon feature/ixemul-80)
- ixemul mmap (ixemul-vtcon/library/mmap.c) is an emulation: malloc + lseek/read via read_mapping_data(). It has no EACCES of its own; the only way to get EACCES out of mmap() is read() returning EACCES (ERROR_READ_PROTECTED/ERROR_DELETE_PROTECTED/ERROR_WRITE_PROTECTED in __ioerr_to_errno.c).
- On rig 3, `git init/add/commit` in RAM:m worked (no mmap error), same as on VTC:. So the A1200 message was NOT reproduced.
- What differs on RAM:: `list RAM:m/r2/.git/objects/2e` shows the loose object file as `81171448eb9f2ee3821e3d447aa6b2` (30 chars). git names it with 38 chars. The RAM: handler of this OS truncates file names to 30 characters.
- Consequences seen on RAM: `git fsck` -> "bad sha1 file: .git/objects/2e/81171448eb9f2ee3821e3d447aa6b2", "missing blob/commit/tree" (exit 2); `git cat-file -p <full hash>` never returns (hangs, slot had to be left running on rig 3 slots 1-2: g2.sh).
- Same script on VTC:userland/work/m: commit, cat-file, fsck, log all pass.

## Conclusion so far
- The loose-object name length (38) exceeds RAM:'s 30-character limit: the layer is the filesystem handler (RAM:), not git config and not ixemul mmap. The mmap EACCES on the A1200 is probably a downstream symptom of git opening/mapping an object path that RAM: mangles (not proven; exact EACCES source on hardware unknown).
- No fix is written: a fix is a judgement call (document RAM: as unsupported for git, or make ixemul/kit refuse; not a git-side workaround). No repo changes committed.

## Open
- Re-run on the A1200 with `git cat-file -p` of a loose object on RAM: and `list` of the objects dir to confirm the 30-char truncation there.
- Rig 3 has stuck vsh commands in slots 1 and 2 (ran `git cat-file` on RAM:); rig 3 was left up for that reason (stopping it was not done: wasabi kill was denied).
