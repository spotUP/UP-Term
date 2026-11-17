# RIG-GC-SAVE-USER-FILES: rig gc saves user files before removing a rig folder

Worktree /Users/spot/Code/UP-Term/vtcon-rig-gc-save, branch rig-gc-save, commit aa5ff8d2 (not pushed, not merged).

## How gc decides (unchanged)
- gc() (tools/rig/rig.py, after save helpers) walks build/rig<N> and build/rig*.del. Skips rig 1 and own number.
  gc_decision: removal when no fs-uae/amiberry command line names the folder, start.lock not held, and
  last-command (else newest top-level mtime) older than GC_IDLE_SECONDS = 2 h. .del leftovers always go unless in use.
- Removal: remove_folder() (rename to .del, rmtree). Measured: rig-gc.log line 23, 2026-11-11 08:27:38 build/rig3
  1706474702 bytes "idle 463 min, no emulator".

## Where Amiga-writable files live (from rig_config ~line 1563 and setup ~1450)
- hard_drive_0 (DH0:): default sys.hdf = APFS clone of SYSTEM_HDF made anew at every start (fresh_sys_hdf) -> DISK IMAGE.
  --os32: os32/ (host dir, copytree of OS32_TREE). --os <x>: os-<x>/ (host dir unpacked from a Workbench hdf).
- hard_drive_1 VTC: = vtc/ (host dir, seeded from build/rig/vtc by seed_rig_vtc, then stage_rig_vtc from the repo/build).
- hard_drive_2 BOOTX: = boot/ (host-generated each start). hard_drive_3 Work: = work/ (--os only, starts empty).

## "Written on the rig" rule
- Drive dirs matched by RIG_DRIVE_DIRS (vtc|boot|work|os32|os-*).
- With a clone source (vtc -> build/rig/vtc, os32 -> OS32_TREE; rig_save_sources): file missing in the source or
  filecmp-different (size+mtime signature, else bytes).
- Without one (work, boot, os-*): mtime newer than drive creation (os-<x>.label mtime, else dir st_birthtime).
- Symlinks skipped. .uaem sidecars follow the same rule (a new file's .uaem is saved with it).
- Limits (over-save, safe direction): kit binaries refreshed from build/amiga newer/different than rig 1's VTC: are
  saved; boot/ files are rewritten each start so they are always saved (small); host post-unpack edits of os-<x>
  (User-Startup, Installer, 060 libs) are saved. Under-save: none known for host dirs.

## UNVERIFIED / not covered
- Files written into sys.hdf (default DH0:, e.g. ENVARC:, or a program drawer on DH0:) are NOT saved: the image is the
  user data there. DCTelnet reads PROGDIR:DCTelnet.Prefs (thoughts/shared/research/2026-10-30_dctelnet-modern-ansi.md),
  so the lost files were wherever DCTelnet was installed on rig 3; which drive is unknown. If it was DH0: on sys.hdf, this
  fix would NOT have saved them. Note fresh_sys_hdf already discards sys.hdf contents at every start (unless
  --keep-sys). Owner decision needed for images (e.g. move sys.hdf to rig-saved as an APFS clone, or xdftool diff).
- `rig.py stop` / `clear` (own folder) still remove without saving; only gc saves.
- Not run against a real rig folder (forbidden); test uses temp dirs only.

## Changed (tools/rig/rig.py in the worktree)
- RIG_DRIVE_DIRS, rig_save_sources, drive_created, rig_user_files, save_rig_user_files, saved_dest: ~lines 3000-3075.
- gc(..., sources=None): save before remove(); OSError -> [ERROR] line on stderr, folder kept; [INFO] line per collected
  rig; rig-gc.log reason gains "N files (B bytes) saved to <dest>". Usage text ~line 48, RULES.md rule 7.
- Dest: build/rig-saved/<N>/<YYYYmmdd-HHMMSS>[-k]; made only when something is saved, never reused.

## Test
- tests/test_rig_gc_user_files_survive.py; Makefile ONLY=rig_gc_user_files_survive.
- Fail-before (main's rig.py): 3 of 4 FAIL; first: line 63 `AssertionError: 0 != 1 : no save folder under build/rig-saved/3`
  (also line 78 `0 != 2`, line 85 `['rig3'] != []`).
- Pass-after: `Ran 4 tests ... OK`; tests/test_rig_gc.py OK.
- Gotcha: importing rig.py in a fresh worktree needs build/amiga to exist (kitset.py:47); an empty build/amiga was made
  in the worktree (gitignored). Pre-existing, affects test_rig_gc.py too.

## agent 15: part 2 (sys.hdf, stop, clear)

Commit `78c0adee` on branch `rig-gc-save2` (worktree `/Users/spot/Code/UP-Term/vtcon-rig-gc-save2`), not pushed, not merged.

(a) DH0: image. `save_rig_user_files` also unpacks the rig's `sys.hdf` (`xdf_unpack`: `xdftool -r <img> unpack`, taken
out of `unpack_volume`, which now calls it; on the RDB disk it opens partition 0, DH0) into `<folder>/sys.hdf.unpack`,
compares each file by sha1 with the master (`SRC_HDF` = `paths.SYSTEM_HDF`, the source `fresh_sys_hdf` clones), and copies
new or changed files to the same save folder under `sys/`. Failure raises OSError: folder kept, `[ERROR]`.
- Master measured read-only 2026-11-11: `images/nyhd2.hdf` 1.5 GB RDB, DH0 partition 106 MiB, 45 MiB used.
  `xdftool -r unpack`: 4.2 s, 51 MB, 3063 files; sha1 of all: 1.7 s. Scratch deleted.
- Cache: `build/rig-saved/sys-master.json` (digest per file, about 300 KB), keyed by master path, size, mtime.
  Rebuild unpacks to `build/rig-saved/sys-master.unpack`, deleted at once. Per removal: one rig unpack (about 4 s,
  about 51 MB transient), deleted before the save returns.
(b) Removal paths, counted from the structure: `remove_folder` is the only function that removes a rig folder; its
callers are `clear()` and `gc()` (default `remove`). `stop_command()` reaches it only through `clear()`. All three now go
through `save_and_remove` (save to `saved_dest`, then remove). No other module removes `build/rig<N>` (grep of
rmtree/.del across tools/ and scripts/: only sub-drawers inside a rig's VTC:).

Tests (both fail on aa5ff8d2, pass on 78c0adee; test_rig_gc and test_rig_gc_user_files_survive pass):
- `tests/test_rig_gc_save_sys_hdf.py` before: `AssertionError: 0 != 1 : no save folder under rig-saved/3`
- `tests/test_rig_stop_clear_save.py` before: `AssertionError: 0 != 1 : no save folder under rig-saved/9: the folder was removed unsaved`
- Part 1 and gc fixtures wrote a 10-byte junk sys.hdf; it now fails its unpack and keeps the folder, so they write fs-uae.log.

Open / found:
- `rig.py start` still re-clones sys.hdf (RIG-FRESH-SYS): DH0: writes are lost on stop + start with `--keep`, not saved.
- Start-time patches written into the clone (68060 libs, screenmode prefs, wasabid/shadows blocks) differ from the master
  and are saved under sys/ as noise (small).
- `rig.py clear --rig1`: rig 1's VTC: is its own clone source, so nothing from rig 1's vtc is saved.
- `make test ONLY=<name>` fails here before any test: TEST_BINS build needs fontTools (missing). Part 1's names are not in
  TEST_PY_ONLY either; the new ones were wired the same way.
