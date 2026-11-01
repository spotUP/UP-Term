---
title: s28 FLAT-PENS47-RESET-HUNT, rig 14 (OS 3.2, aga256, depth 8), origin/main 092a380a
status: cause-measured, fix-part-1-not-enough
date: 2026-11-01
---

# Who writes Workbench pens 4-7 last after a flat ON pick

Build of origin/main 092a380a (make wasabi dist, rig-helpers), rig 14 `--max --os 32`, `rig.py aga256` (DEPTH 8), amarchy_gui_rig
setup (build Amarchy from VTCX:Apps/bin), RAM:amarchy-flat started with flat off, `T:wbpens WATCH 120` in the background, then
`put_flat_prefs(True)` + `pick()` (drawn theme menu -> gt_dos_select -> desk_dos_apply), 3 reads, then flat OFF pick.

## WATCH (ticks; pens 4-7; L = pens 252-255; F = flat block enabled; D = ENV:AMARCHY_WBCLOSES, the apply returned)

    T 0    SCR 0x100ee570 4 0000ff 5 ff00ff 6 00ffff 7 ffffff L ee4444 55dd55 0044dd ee9900 F 0 D 0
    T 54   SCR 0x100ee570 (same pens)                                                         F 1 D 0
    T 273  SCR 0x100ee570 4 1a1a1a 5 7a7a7a 6 b6b6b6 7 a4a4a4                                F 1 D 0   preview (gt_pv)
    T 295  SCR 0x100ee570 4 0000ff 5 ff00ff 6 00ffff 7 ffffff                                F 1 D 0   preview restore
    T 724  SCR 0x100ee570 4 1a1a1a 5 222222 6 b6b6b6 7 a4a4a4                                F 1 D 1   dk_live_pens (desk_dos_apply)
    T 727  SCR 0x10052ea0 4 0000ff 5 ff00ff 6 00ffff 7 ffffff                                F 1 D 1   NEW Workbench screen, stock 4-7
    T 1088 SCR 0x10052ea0 (stock)                                                             F 0 D 1   flat OFF pick
    T 1332 / 1355 preview + restore on the new screen; T 1781 D 1 (OFF apply returned). 252-255 never change.

Reads after the ON pick: pens 0-3 = 000000 222222 bebebe 8d8d8d (theme), pens 4-7 = 0000ff ff00ff 00ffff ffffff, 3 of 3.
amarchy-flat STATUS: flat=on, pens-held 16 refs=1 (the icon pen holder DID follow the new screen). OFF read = start (base).

## Cause

The last writer of pens 4-7 is the Workbench reopen itself (T 727, 3 ticks after desk_dos_apply's dk_live_pens): a new screen
starts with the graphics defaults on 4-7, which no palette.prefs entry reaches at depth 8 (entries 4-7 -> pens 252-255).

Invariant violated: "while flat is on, every Workbench screen carries the theme on the live role pens (desk_live_role_pens)".
Only a one-shot setter exists (dk_live_pens, on the screen that is open at apply time). The per-screen follow-up was
written in aacd873b (flat.c pens_sync, IP_SYNC_OBTAIN branch: desk_live_role_pens + desk_pen_colour + SetRGB32) and lost when
458249d0 (shared icon pen holder, F14/H1) moved that pens_sync into iconpen_dos.c without the FLAT-PEN5-MAGENTA block.
desk_dos.c's comment "amarchy-flat sets the same pens on every new Workbench screen (pens_sync)" has had no code behind it since.

Layer: the Workbench screen follow path (iconpen_dos.c pens_sync, IP_SYNC_OBTAIN: runs once per new Workbench screen or depth,
already loads the prefs and the theme). Not desk_dos (it cannot know about a later reopen) and not a timer in a new place.

## Fix part 1 (flat-pens47-follow.patch, NOT landed): the setter on the follow path

`desk_live_follow` in desk_core (pure; host test `test_flat_reopen_keeps_theme_on_pens47`, ONLY=desk 1884 checks 0 failed) and a
SetRGB32 loop in the IP_SYNC_OBTAIN branch of iconpen_dos.c pens_sync. Built amarchy-flat + amarchy-icons (vbcc, 0 warnings).
Apply with `git apply thoughts/shared/research/s28-evidence/flat-pens47-follow.patch` on main.

Rig 14 proof with that build: FAIL, ON reads 4-7 = 0000ff ff00ff 00ffff ffffff 3 of 3; OFF read = start. WATCH:

    T 702  SCR 0x10052ea0 4 1a1a1a 5 222222 6 b6b6b6 7 a4a4a4  F 1 D 1   dk_live_pens
    T 706  SCR 0x10052ea0 4 0000ff 5 ff00ff 6 00ffff 7 ffffff  F 1 D 1   reopen; the NEW screen has the SAME address

STATUS after the pick: `pens-held 0 missing 32 .. 47 refs=1` (run 1 with a new address: `pens-held 16`).

## Second cause: the follow path keys a screen on its address

ip_holder_stale (iconpen_core.c:112) is `h->screen != screen || h->depth != depth`. When the Workbench reopen allocates the new
Screen at the freed one's address (run 2), ip_sync_decide returns IP_SYNC_NONE: no re-obtain of the 16 icon pens (STATUS shows
them all missing on the new ColorMap) and no live role pens. So the address is not a screen identity; the follow path misses every
same-address reopen, which breaks the icon pens too (not only 4-7).

Next: give the holder a reopen-proof identity (e.g. the reopen count Amarchy already writes to ENV:AMARCHY_WBCLOSES, or a check
that a held pen is still allocated in sc->ViewPort.ColorMap), test it in tests/test_iconpen.c (same pointer + reopen -> OBTAIN),
then apply part 1 and rerun pens47 on rig 14: expect ON 4-7 = 1a1a1a 222222 b6b6b6 a4a4a4 3/3 and pens-held 16.
