# TE-10.1 (Amarchy part): one guarded way to lock any public screen by name

Background: s57-evidence/freeze-9boot-rig145.md. LockPubScreen on a Workbench that is unlisted or still PSNF_PRIVATE
makes Intuition take the reopen path that can deadlock at boot.

## Changed
- iconpen_core: `ip_wb_lockable` renamed `ip_pub_lockable(listed, has_screen, flags)` (same body; header comment generalised).
- iconpen_dos: `pub_node_in(pl, name)`, `ipd_pub_screen_in(pl, name)`, `ipd_lock_pub_screen(name)` (list lock, find node,
  LockPubScreen only when `ip_pub_lockable`, unlock; 0 for null or empty name). `ipd_lock_wb_screen()` and
  `ipd_wb_screen_in(pl)` are one-line wrappers; BOOT-31 / S57-FREEZE comments moved to the generic function.
- amarchy/bar.c act_screen_front: `ipd_lock_pub_screen(g_scrname[i])` (was an unguarded LockPubScreen; "Workbench" in the
  list was the same freeze). bar.c is only in the Amarchy target, which already links iconpen_dos/iconpen_core.
- amarchy/strut_patch.c on_bar_screen: a named screen now goes through `ipd_lock_pub_screen(r->name)` (it was another
  unguarded by-name lock; the wbench flag still uses ipd_lock_wb_screen). Needed so the widened guard can hold.
- tests/test_wb_lock_guard.py: no amarchy/*.c except iconpen_dos.c may call `LockPubScreen(` with any argument other
  than 0/NULL. Existing helper-order checks kept, updated to the new names.
- tests/test_iconpen.c: renamed cases plus `test_pub_lockable_generic_rule` (not listed / no screen / private / public).

## Name comparison
`strcmp` (case-sensitive) on `psn_Node.ln_Name`, exactly as the old `wb_node_in` did.

## Deviation: default-screen locks stay
`LockPubScreen(0)` in gui.c:1768, leaf.c:434 and strut_patch.c (default branch) is the default-screen lookup of an
interactive caller, not a by-name lookup; `ipd_lock_pub_screen` returns 0 for a null name by spec, so these cannot route
through it. The guard exempts exactly 0/NULL. Open question: the default screen can resolve to the Workbench too; not
analysed here.

## Known open exception (other session, untouched)
handler/frontscr.c:43 `LockPubScreen((UBYTE *)name)` is unguarded.

## Proof
- Red (bar.c change reverted, final test code): `AssertionError: Lists differ: ['bar.c:613'] != []` ... FAILED (failures=1)
- Green (restored): `Ran 2 tests ... OK`
- Queue helper: `RUNTEST iconpen rc=0 [OK] iconpen: 2164 checks, 0 failed` (main baseline 2160, +4 new cases);
  `RUNTEST desk rc=0 [OK] desk: 2311 checks, 0 failed`; wb_lock_guard rc=0 (2 tests OK).
- m68k: `make build/amiga/Amarchy amarchy-gui amarchy-menu amarchy-index amarchy-icons amarchy-flat upprefs` rc=0
  (only pre-existing warnings: atiming.h unused statics, win_core.c `main` variable).
