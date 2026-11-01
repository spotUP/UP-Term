# L7: are fixed pens 32..47 the wrong abstraction? (s28, 2026-11-01)

## Measurement (rig 10, AmigaOS 3.2, Workbench depth 8, ColorMap count 256)

- Fresh boot, amarchy-flat not running: PalExtra sharable 255, nfree 245, free order 251 250 249 ...
  graphics hands out shared pens top-down; pens 32..47 are all free.
- palprobe WATCH 75 s (ObtainPen / ObtainBestPenA / ReleasePen patched): 48 events, all from
  amarchy-flat itself. Opening 9 Workbench drawers made no pen calls (classic icons on depth 8).
- Old build, after flat OFF the free order read `47 46 45 ... 32 244 243 ...`: ReleasePen puts a
  released pen at the HEAD of the free list. The next ObtainBestPen of a new colour by any client
  (the dim pen, theme pens, a colour icon) takes 47, then 46. That is the `dim=47` after OFF and the
  `pens-held 3` after a restart: the 16 pens were released by the flat switch and taken by others.

Holder per run: nobody holds 32..47 at boot; amarchy-flat holds them while flat is ON; after flat OFF
(or an amarchy-flat restart) the next shared allocator on the screen takes them from the free-list head
(measured: amarchy-flat's own dim pen, dim=47/46). The boot `missing 43` holder was not caught in a
WATCH window (unverified); the same mechanism fits it.

## Options

(a) Obtain early / exclusive. Already exclusive (ObtainPen PENF_EXCLUSIVE). Obtaining earlier only
    narrows the boot window; it does not stop the release on flat OFF, which is the measured cause.

(b) Obtain any 16 pens and remap. Icon images are classic planar data (iconfile_core strips FORM ICON
    and NewIcon blocks on APPLY) and Workbench blits them unremapped, so pixel value = pen. Remapping
    means rewriting every .info file whenever the runtime allocation changes. A palette-mapped FORM ICON
    instead would make every theme switch rewrite all icons, with colours approximated by ObtainBestPen
    precision. Wrong level: it moves a runtime-allocation problem into on-disk files.

(c) Hold the 16 pens for the whole amarchy-flat run, independent of the flat switch. Release only on
    quit, when the kill switch is held, or when the Workbench screen is gone. The pens never return to
    the free-list head while Amarchy runs, so no other client can take them.

## Chosen: (c)

Fixed indices are inherent to classic icons and stay. The defect was releasing the pens on flat OFF.
`ip_hold_wanted(quit, held, gfx_v39)` (amarchy/iconpen_core.c) is the decision; pens_sync in
amarchy/flat.c uses it. Regression test: test_free_list_model in tests/test_iconpen.c models PalExtra
(top-down boot, ObtainBestPen takes the head, ReleasePen inserts at the head) and fails with the old
behaviour (flat OFF releases): dim lands in 32..47 and only 12 pens come back on ON.

Residual risks (not fixed here): the gap between amarchy-flat exit and restart; on a 64-colour screen
top-down shared allocations (63, 62, ... 48, 47) made before amarchy-flat starts at boot would collide.

## Rig check after the fix (rig 10, 3.2)

Start (flat off): pens-held 16. ON 16, OFF 16, ON 16, OFF 16, ON 16; free order after each OFF starts
at 244 (32..47 not released). After opening 5 drawers: pens-held 16, dim=245.
