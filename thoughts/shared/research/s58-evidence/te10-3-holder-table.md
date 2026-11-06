# TE-10.3 Holder table and diff

Branch te10-3 (base e79a4705). Files: amarchy/iconpen_core.h, amarchy/iconpen_core.c, tests/test_iconpen.c. No Amiga wiring (TE-10.4).

## API added (own block at the end of each file)
- `ip_screens` { count, slot[IP_SCREENS] }, slot { name[IP_PSN_NAME=140], ip_shared sh, unsigned epoch }.
- `ip_screens_init` (slot 0 "Workbench"), `ip_screens_find`, `ip_screens_add`, `ip_screens_drop`.
- `ip_snap_entry` { name, screen, flags, depth, ehb }, `ip_snap` { count, e[] }, `ip_diff` with kinds IP_DIFF_SAME / APPEARED / GONE / REOPENED.
- `ip_screens_diff(old, new, out)` (pure), `ip_screens_apply(table, diff)`.

## Decisions
- Name comparison is case-insensitive ASCII (Intuition public screen names). No existing comparison in iconpen code to reuse.
- IP_SCREENS = 40 (Workbench + 32 rules + 7 spare). Above it: add returns -1, table unchanged (tested).
- Names longer than 139 characters or empty: add refused.
- GONE keeps the slot (the map still names it) and forgets pens with ip_holder_init, no release. REOPENED forgets pens the same way (the ColorMap went) and bumps only that slot's `epoch`. Slot 0 is never touched by apply (its epoch stays `sh.epoch`).
- PRIVATE node: slot kept exactly as is (no forget, no epoch); a PRIVATE APPEARED adds no slot.
- APPEARED for a name that already has a slot (after GONE) counts as a reopen: pens forgotten, epoch +1.

## Proof
- Green (full change): `RUNTEST iconpen rc=0 [OK] iconpen: 2263 checks, 0 failed` (base 2164, +99).
- Red (ip_screens_apply stubbed to return 0, tests kept): `RUNTEST iconpen rc=2 [FAIL] iconpen: 2263 checks, 6 failed`; first: `tests/test_iconpen.c:727: ip_screens_apply(&t, &d) = 0, want 1` (also :728 held=16 want 0, :747 epoch, :774, :775).
- Green restored (file identical to the green copy, cmp): `RUNTEST iconpen rc=0 [OK] iconpen: 2263 checks, 0 failed`.
- m68k: make of Amarchy, amarchy-gui, amarchy-menu, amarchy-index, amarchy-icons, amarchy-flat, upprefs: rc=0. No program added; no `__stack` or `$STACK` cookie removed (only the three files above changed).

## Tests
T1 (slot 0 unchanged by GONE and REOPENED Workbench diffs), T2 (GONE: pens forgotten, release counter 0), T3 (pointer and depth reopen: that slot's epoch +1 only), T4 (PRIVATE keeps slot; private APPEARED adds none), T5 (APPEARED adds; full table refuses unchanged; find/add/drop; 139/140 character names), T6 (identical snapshots only SAME).

## Open points
- Overlapping the s58-pens47 branch in the same three files is expected; additions are in a block at the end of each file.
- Wiring (snapshot walk under LockPubScreenList, ipd_epoch_of, exit release) is TE-10.4.
- The IP_SCREENS table is a plain array of ~40 slots of about 1 KB each (ip_holder is large); TE-10.4 must allocate it as the one "Amarchy.IconPens" record, not on a stack.

## Review fix (main session, 2026-11-06)
Defects found in review of 5a3b234b (all in `ip_screens_apply`):
1. A screen first seen PRIVATE, then public on the same address, never got a slot (the diff says SAME, apply skipped SAME).
2. A reopen across a private phase was lost (old snapshot PRIVATE at one address, new public at another: slot missed, or pens kept).
3. REOPENED for a name with no slot was skipped (no slot added).
4. GONE with the PRIVATE flag was skipped (pens kept for a closed screen).

Rules now (apply decides from the slot's own holder identity, not from the diff kind alone):
- Slot 0 (Workbench) is never touched.
- GONE: slot kept, pens forgotten with `ip_holder_init`, never released; private or not.
- New entry PRIVATE: no slot added, an existing slot stays exactly as it is (design section 5 rule 5: "not open yet", never "gone").
- New entry public, no slot: add the slot, whatever the diff kind.
- New entry public, slot holds pens: same screen address and depth as the holder = nothing; else forget pens, epoch +1.
- New entry public, slot holds no pens: SAME = nothing; any other kind = epoch +1.
`ip_diff_entry` carries `screen` and `depth` of the new entry (of the old entry for GONE) for this.

Accepted gap: a slot with no holder identity does not bump its epoch on a reopen hidden by a private phase. Harmless: no pens are held, the next sync obtains from the live screen. Same-address reuse between two polls is the job of `ip_holder_verify`.

Proof:
- Red (tests T7-T12 added, apply as delivered): `[FAIL] iconpen: 2295 checks, 13 failed`; lines 792, 793, 805, 806, 823, 824, 825, 847, 848, 852, 853, 862, 863 of tests/test_iconpen.c.
- Green (apply fixed): `RUNTEST iconpen rc=0 [OK] iconpen: 2295 checks, 0 failed` (delivered 2263, +32).
- m68k: built in the stacked merge validation before the push (see the merge commit).

Tests added: T7 (private first, then public on the same address: slot added once), T8 (private at one address, public at another: slot added), T9 (public at 0x100 with pens, private at 0x180, public at 0x180: the private step keeps the pens, the public step forgets them without a release and bumps that slot's epoch only), T10 (private to public on the holder's own address: unchanged), T11 (APPEARED and REOPENED matching the holder's address and depth: unchanged), T12 (GONE of a private entry: pens forgotten, no release, slot kept).
