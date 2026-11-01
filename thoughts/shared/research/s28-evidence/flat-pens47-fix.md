---
title: s29 FLAT-PENS47 fix, icon pen holder identity + live role pens on the follow path
status: fixed-host-tested, rig-14-pass
date: 2026-11-01
---

# FLAT-PENS47 fix (branch fix/flat-pens47-identity, commit 6b96caad)

Follows flat-pens47-hunt.md (two causes: the follow path had no setter for pens 4-7, and the icon pen holder keyed a
Workbench screen on its address, so a same-address reopen gave IP_SYNC_NONE).

## Identity choice: the ColorMap pen allocation

- `ip_holder_verify(h, o, screen, depth)` (iconpen_core.c), called by `ip_sync` before `ip_sync_decide`. Same screen
  pointer: depth changed or any held pen found free in the ColorMap (`o->is_free`, ObtainPen PENF_EXCLUSIVE probe then
  ReleasePen) means a new screen: `ip_holder_init` (forget, no ReleasePen on the new ColorMap). A missing pen that became
  free sets `h->recheck`, so `ip_holder_stale` returns true and the next sync obtains it.
- Rejected ENV:AMARCHY_WBCLOSES: a test sentinel written by Amarchy main (leaf.c am_sentinel), counts only Amarchy's own
  Workbench closes, lives in another process, and misses reopens by ScreenMode prefs or other tools. The ColorMap is the
  one record of who holds a pen on this screen.

## Follow setter (the patch from the hunt, moved into core)

`ip_sync` (core, host-tested) does verify, decide, release/obtain; on OBTAIN it loads the theme through `o->theme`,
runs `desk_live_follow` and writes each pen through `o->set_rgb` (SetRGB32 via ip_scale32 in iconpen_dos.c). pens_sync
in iconpen_dos.c is glue only (lock pub screen list, fill ctx, ip_sync, unlock).

## Host tests

- tests/test_iconpen.c `test_reopen_same_address` (model ColorMap): boot OBTAIN 16 held + pens 4-7 themed; same screen
  NONE, no sets; same-address reopen OBTAIN, 16 held, pens 4-7 = desk_pen_colour, no bad release; same address other
  depth OBTAIN; missing pen freed later 15 -> 16; release. iconpen 2099 checks 0 failed.
- tests/test_desk.c `test_flat_reopen_keeps_theme_on_pens47` (desk_live_follow). desk 1889 checks 0 failed.
- Revert proofs: ip_holder_verify call removed -> FAIL test_iconpen.c:428 `ip_sync(...) = 0, want 1` and :430
  alloc 0 want 2. set_rgb call removed -> FAIL test_iconpen.c:405 `rgb[4+i] = 255, want 3164256` (x6). Restored, OK.
- vbcc build of amarchy-flat and amarchy-icons: 0 warnings.

## Rig 14 (OS 3.2, aga256 depth 8)

Build of 6b96caad (amarchy-flat, wbpens; Amarchy from the setup, unchanged logic), rig 14 `--max --os 32`, `rig.py aga256`,
amarchy_gui_rig --setup. Driver: 3 cycles of put_flat_prefs(True) + pick() then put_flat_prefs(False) + pick(), reusing
amarchy_desk_rig helpers (flat_stop, put_flat_prefs, pick) and T:wbpens. Start depth 8, pens 4-7 0000ff ff00ff 00ffff ffffff.

    cycle 1 ON: pens 4-7 1a1a1a 222222 b6b6b6 a4a4a4; flat=on; pens-held 16 refs=1   OFF: 0000ff ff00ff 00ffff ffffff
    cycle 2 ON: pens 4-7 1a1a1a 222222 b6b6b6 a4a4a4; flat=on; pens-held 16 refs=1   OFF: stock
    cycle 3 ON: pens 4-7 1a1a1a 222222 b6b6b6 a4a4a4; flat=on; pens-held 16 refs=1   OFF: stock

PASS 3 of 3 (hunt build: 0 of 3, pens-held 0 missing 32..47). Not recorded per cycle: the Workbench screen address
(T:wbpens without WATCH has no SCR line), so this run does not show which cycles reopened at the same address; the
same-address case is proven by the host test. ENV:AMARCHY_WBCLOSES read 0 throughout (unverified whether the sentinel
is set in this rig setup).
