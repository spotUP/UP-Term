# TE-10.4 per-screen pen sync (host and m68k build proof only)

Rig proof is NOT part of this step (plan case TE-9.6). Nothing here is verified on an Amiga.

## Changed per file
- amarchy/iconpen_core.c/.h: `ip_snap_add`, `ip_screens_sync` (pure multi-slot sync: per slot select callback, ip_sync, NEED_THEME reported in `need[]`),
  `ip_screens_held` (non-Workbench slots holding pens), `ip_screens_epoch` (slot 0 = sh.epoch, others = slot.epoch, unknown = 0).
- amarchy/iconpen_dos.h: `ipd_rec` and `ipd_stheme` moved here (no new file, no Makefile change); semaphore name `Amarchy.IconPens.2`;
  declares `ipd_epoch_of`, `ipd_screen_rules_load`, `ipd_rules_changed`.
- amarchy/iconpen_dos.c: record holds ip_screens, snap, cur, diff, parsed rules, main theme, per-slot theme cache, rules_dirty (all heap, AllocVec, none on a stack).
  `pens_sync` rewritten (pass loop below). `ipd_status_line` appends ` screens=<n>` at the end. `ipd_recolour` stays Workbench only and marks rules dirty.
- tests/amiga/iconpens.c uses the real `ipd_rec` and `IPD_SEM_NAME`; tools/rig/amarchy_iconpens_rig.py docstring and amarchy/flat_shared.h comment name `Amarchy.IconPens.2`. Parsed text fields unchanged.
- tests/test_iconpen.c: test_screens_sync (two screens, gone screen, hold 0, private node then public, slot 0 hold 0, exit with a private slot, reopen epoch) and test_screens_held_count.

## Pass bound
`IPD_PASSES` = 3, a `for` over a constant. Each pass: lock list, snapshot, diff, apply, per-slot sync, unlock. If any slot answers NEED_THEME, ALL needed themes are read in one
unlocked section, then the next pass runs. Normally ends in 2 passes. A slot still unsynced after 3 passes (list changing on every pass) is retried on the next 1 s tick,
because ip_sync_decide works from the holder state, not from the diff. Lock order: holder semaphore, then list lock. No DOS I/O, Wait, Delay or WaitTOF under the list lock; no LockPubScreen.
A tick with no change reads nothing from DOS: rules are reread only when `rules_dirty` (record start, `ipd_rules_changed`, `ipd_recolour`); themes only on an obtain.

## Deviations and accepted gap
- Slot 0 (Workbench) is synced always, as before, even when its node is private or absent. Only other slots are skipped when private or absent.
- Epochs of slots 1.. are bumped only by `ip_screens_apply` (avoids a double bump); slot 0's sh.epoch is bumped by the sync (new_identity).
- Accepted gap: pens on a screen that is private at exit time stay until that screen closes (never release on a private or gone slot).

## Measured sizes (host sizeof)
gtheme=296, ip_screen_slot=272, ip_screens=10888, ip_snap=6728, ip_diff=13448, gt_screen_rules=6028. ipd_rec is about 47 KB in total (adds 2 snaps, diff, rules, 8 theme caches), AllocVec MEMF_PUBLIC.

## Proof
- Fix disabled in the core: loop in `ip_screens_sync` changed to `i < 1 && i < t->count` (slot 0 only).
  - RED: `RUNTEST iconpen rc=2 [FAIL] iconpen: 2332 checks, 12 failed` (terminates).
  - GREEN (restored): `RUNTEST iconpen rc=0 [OK] iconpen: 2332 checks, 0 failed`
- `RUNTEST wb_lock_guard rc=0`
- m68k: `make build/amiga/Amarchy ... upprefs` rc=0; `make rig-helpers` rc=0.

## Not proven
Live behaviour on an Amiga (two real public screens, private to public turn, exit with a private screen): plan case TE-9.6. Make suites make_hygiene and gthemes not run (Makefile and gthemes_core untouched).
