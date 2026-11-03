---
date: 2026-11-03
topic: S32-KNOB-NONE-FRESH - amarchy-flat scroller knob missing on fresh 3.2 install
status: cause narrowed to the installed build; not reproduced on main build
---

# S32-KNOB-NONE-FRESH

## Setup

- Rig 63, fresh-all install on 3.2:
  `UPTERM_RIG=63 INSTALLER_KEEP=1 python3 tools/rig/installer_rig.py ONLY fresh-all --os 32 --purpose "Amarchy session: knob not drawn on fresh boot"` gave [PASS].
- The kit came from build/dist at 20:56. Its amarchy-flat md5 is 8a574ff2, the fb6aa600 build. Rig 96 has the same build.

## Before (rig 63, fb6aa600 kit, after reboot)

- The knob is NOT drawn, so rig 63 reproduces rig 96.
- Test window: "Knob Shell" (XCON:20/30/400/150). Its prop column (x406-415, y45-170) is all 1A1B26. Only the arrow pixels are 7AA2F7.
- Shot: ~/Desktop/upterm-shots/rig63-knob-fresh.png

STATUS after boot:

```
flat=on calls=350 chained=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
rectfill installed=yes calls=2525 hits=36 shits=24 mhits=0 side=2 looks=2
clone draws=69 fallback=0
pens-held 16 refs=3 dim=251 knob=249 inbar=250
frame=hooked calls=0 flat=0
```

After opening Knob Shell: rectfill calls=2939 and hits=48, but shits stayed at 24. No new sfill entries were recorded for the prop in the new shell.

What this means: the knob fills of the new XCON prop never pass the scroller gate in `flat_rf_c` (amarchy/flat.c:708-790). Either the caller's return address is outside irom_lo/irom_hi, or `scroller_pen` (flat.c:617-640) / `flat_scroller_pen` (flat_core.c:565-605) does not match the fill to the container or knob rect. The fill then keeps its pen 0 / back colour, which is 1A1B26.

## md5 comparison

| Binary | main build/amiga | dist kit (installed on rigs 63 and 96) | VTC: copies (rigs 96/63/61) |
|---|---|---|---|
| amarchy-flat | 82f3f376 | 8a574ff2 | 82f3f376 |
| Amarchy | 4d98af93 | 4d98af93 | rig 61 is aad585f9 (differs) |
| amarchy-gui | 9302dde5 | 9302dde5 | same |
| amarchy-menu | 7e84b491 | 7e84b491 | same |

- The installed md5 on rig 96 (UP-Term:bin, inside sys.hdf) was not read directly. ami.py would refresh rig 96, which is not allowed.
- Rig 61 draws the knob. It is a fresh 3.2 install from current main (33d894d8).

## Cause

- The fault follows the fb6aa600 amarchy-flat build (8a574ff2), not "fresh boot versus restart".
- On rigs 63 and 96 this build leaves the knob undrawn. Rig 61, with the main build, draws it.
- Since fb6aa600, the only amarchy commit is 33d894d8 (S32-ARROW-KNOB-COLOUR). It adds `knob_pen = flat_knob_state_pen(&r->st, wdepth, wactive)` in `c2_draw` (amarchy/flat.c ~260-370) and passes it to `flat_clone_draw` / `flat_glyph_draw`. That gives the knob a second draw path through the sysiclass clone hook, so the knob no longer depends only on the RectFill gate, which misses the XCON borderless newlook prop.
- Unproven: rig 63 was not re-measured with the main amarchy-flat (82f3f376) copied in. The rig stopped answering ("Connection refused") at the last step.

## Proposed fix

- Reinstall rig 96 from a kit built at main (33d894d8 or later), then re-measure the prop column.
- Expected result: the knob shows in pen 249 (knob), not 1A1B26.
- Do not change code until a main-build install still fails. If it does, the remaining suspect is the RectFill gate for borderless newlook props (flat.c:708-790, `irom` range or the container match).

## Regression test idea

- Add a host test to tests/test_flat_core.c, next to the scroller pen tests at lines 985-1010.
- Drive the XCON prop geometry from handler/sbar_gad.c:108-122: FREEVERT, NewLook, Borderless, Total=Visible=1, Top=0, which gives a full-body knob (VertBody 0xFFFF).
- Assert that `flat_scroller_knob` returns the full container height.
- Assert that `flat_scroller_pen` gives the knob state pen for a fill that equals the knob rect, and for a fill one pixel inside it.
- Assert that `flat_knob_state_pen` gives pen_outline for an active window and pen_inbar for an inactive one, so the clone path draws the knob pen.
