---
date: 2026-11-09
topic: DOpus 5 lister scrollers, arrows and size gadget under flat
status: design (not implemented, not committed)
related:
  - thoughts/shared/research/2026-11-09-dopus-lister-flat.md
  - thoughts/spot/todos.md:4860-4870 (rig 68 DOpus DrawInfo, c8 hunt)
---

# DOpus 5 scroller look under flat: design

## Summary (read this first)

The brief assumes the lister arrows are drawn by `dopusiclass`. The DOpus 5
source says otherwise for the lister border. Most of the "hook dopusiclass"
part of the brief therefore targets the wrong drawer for the lister window:

- Lister border arrows: four `sysiclass` images (`SYSIA_Which = LEFTIMAGE+a`,
  `SYSIA_Size LOWRES/MEDRES`, `SYSIA_DrawInfo GUI->draw_info`) wrapped in
  `buttongclass` gadgets. Source: `Program/lister_dual.c:985-1080`.
- Lister vertical knob: `frameiclass FRAME_PROPKNOB` passed as `PGA_KnobImage`
  to a `propgclass` with `PGA_Borderless|PGA_NewLook`, only on Intuition
  47.44+ (`DOPUS_NEW_PROP_KNOB`, `Include/dopus/common.h:145-160`).
- Lister size gadget: the system `SIZEIMAGE` (`WA_SizeGadget`,
  `Program/lister_window.c:211`).
- `dopusiclass IM_ARROW_UP/DOWN` is used only by the DOpus listview gadget
  (`Library/listview_class.c:263-316`), which lives in config windows,
  requesters and function editors, not in the lister border. Title-bar
  `dopusiclass` images are lock/iconify only (`Library/boopsi.c:470-510`).

So on OS 3.2 the lister arrows and size gadget already reach flat's C2
sysiclass hook and the clone path. The inverted look is a defect inside the
C2 clone path for DOpus's image parameters, or a state mismatch, not a missing
hook. That is unverified: step 1 of the work is a rig 68 measurement that tells
the candidates apart (see "Pen cause").

A `dopusiclass` hook is still needed for the listview arrows (config windows),
and the design for it is below. It reuses the same flat glyph code.

Evidence status: source lines are from the upstream tree in
`~/Code/Reference/dopus5/source` (shallow clone). The rig runs the vendor
binary 5.104 (`vendor/dopus5-5.104/DOpus5_104_os3.lha`, installed by
`Makefile:2140-2160`, `tools/rig/rig.py:1161`). Vendor `DirectoryOpus` strings
hold `sysiclass propgclass buttongclass frameiclass` side by side, which fits
the `lister_dual.c` code being in 5.104. That match is likely, not proven.

## 1. Flat's existing drawing functions (reuse, never duplicate)

All in `vtcon/amarchy/`.

| Function | Location | Role |
|---|---|---|
| `g_size` | `flat_glyph.c:72` | block glyph for the size gadget |
| `g_arrow` | `flat_glyph.c:119` | block glyph for LEFT/UP/RIGHT/DOWN arrows |
| `flat_glyph_known` | `flat_glyph.c:143` | Which values flat draws |
| `flat_glyph_state` | `flat_glyph.c:155` | IDS 1 to SELECTED; 2, 5, 6, 7 to INACTIVE |
| `flat_glyph_roles` | `flat_glyph.c:166` | glyph and box role per state |
| `flat_glyph_window_gadget` | `flat_glyph.c:180` | close/depth/zoom/iconify band |
| `flat_clone_route` | `flat_glyph.c:200` | which Which values use the clone path |
| `clone_ground` | `flat_glyph.c:222` | most common marker inside the box = ground |
| `flat_clone_map` | `flat_glyph.c:238` | ground vs glyph classification |
| `flat_clone_pen` / `clone_pens` / `clone_paint` | `flat_glyph.c:278/314/323` | repaint the map with role pens |
| `flat_clone_draw_in` | `flat_glyph.c:344` | host-testable clone draw |
| `flat_glyph_draw` | `flat_glyph.c:409` | block glyph entry: `(s, which, method, ids, ox, oy, w, h, frame_pen, knob_pen, side, fill, ctx)` |
| `flat_frame_draw` | `flat_glyph.c:419` | frame |
| `flat_dopus_box_draw` | `flat_glyph.c:439` | DOpus box (D4) |
| `flat_knob_frame_draw` | `flat_glyph.c:530` | rounded knob in `flat_knob_state_pen` |
| `flat_smart_knob` | `flat_glyph.c:540` | AUTOKNOB/SMARTKNOBIMAGE (0x21) test |

Which constants: `FLAT_W_SIZE 0x02`, `LEFT 0x0A`, `UP 0x0B`, `RIGHT 0x0C`,
`DOWN 0x0D`. `FLAT_IM_DRAW 0x202`. Roles: BACK 0, TEXT 1, LINE 2, FRAME 3, KNOB 4.

How they are invoked today (`flat.c`):

- C2 sysiclass dispatcher, `c2_draw` at `flat.c:563`. It returns to the saved
  routine for anything except `IM_DRAW` of a known Which. Otherwise it does
  `GetAttr(SYSIA_Which)`, then `flat_which_alias`, then `c2_get_pens`
  (`flat.c:364-395`: knob = `flat_knob_state_pen`; frame = BACKGROUNDPEN, or the
  band pen for close/depth/zoom/iconify; text = inactive pen for an inactive
  window). Then `flat_clone_route` picks `c2_clone` (`flat.c:489`, offscreen
  render of a private sysiclass object with marker DrawInfo, `clone_object`
  around `flat.c:194`, `clone_render` around `flat.c:270`) or `c2_glyph`
  (`flat.c:536`, calls `flat_glyph_draw`).
- Fallback: `c2_disp_reg` (registers a0/a2/a1) / `c2_disp_stk`,
  `flat.c:590-600`, call the saved routine.
- C8 frameiclass dispatcher from `flat.c:601`; `smart_knob_of` at
  `flat.c:634`; `raa_knob_frame_draw` around `flat.c:765` draws the knob.
- Install: `class_slot` at `flat.c:2001` makes a throw-away object and takes
  `OCLASS(o)->cl_Dispatcher.h_SubEntry`; `hook_install` at `flat.c:2030`
  swaps C2 then C8 under `Forbid` with `flat_swap_install`. D4 (dopus5.library
  `DrawBox` LVO -696, `DrawFieldBox` -702, bevel body) installs at
  `flat.c:2099-2175`, functions `d4_*` at `flat.c:811-899`.
- On OS 3.1 `c2_which_31` (`flat.c:547`) finds Which by walking
  `window->FirstGadget` through `flat_gadget_which` (`flat_core.c:350`),
  which returns -1 unless `GTYP_SYSGADGET (0x8000)` is set.

## 2. How dopusiclass draws

Source: `Library/image_class.c`.

- `image_dispatch` (line 29): `IM_DRAW` and `IM_DRAWFRAME` go to `image_draw`;
  every other method goes to `DoSuperMethodA` (imageclass).
- `image_draw` (line 83): `pens = imp_DrInfo->dri_Pens`. The pens come from the
  DrawInfo in the draw message, so they are the screen's flat pens.
- `IM_ARROW_UP/DOWN` (lines 109-153):
  1. `listview_border` THIN or THICK (`listview_class.c:1435-1452`): top/left in
     `SHINEPEN`, bottom/right in `SHADOWPEN`, swapped when `IDS_SELECTED`.
  2. `RectFill` interior in `pens[SELECTED ? FILLPEN : BACKGROUNDPEN]`.
  3. Glyph as `Move/Draw` lines in `pens[SELECTED ? FILLTEXTPEN : TEXTPEN]`.
  Only UP and DOWN exist. No left/right arrow, no size type.
- `IM_CHECK` line 157, `IM_BBOX` line 261 (SHINE/SHADOW lines, `EraseRect` if
  INACTIVENORMAL), `IM_BORDER_BOX` line 297 (`FILLPEN` fill unless
  INACTIVENORMAL), `IM_ICONIFY` line 307 (`FILLPEN` ground when active).
- Instance data `BoopsiImageData` (`Library/boopsi.h:140-146`):
  `short type` at offset 0, then `short fpen`, `struct LibData *data`,
  `unsigned short flags` (`BIF_THIN_BORDERS 1<<0`). The type field is at
  `INST_DATA(cl, o) + 0`. Offset is from source; verify against the 5.104
  binary before use (unverified).
- Registration: `init_class` (`Library/boopsi.c:34-64`) does
  `MakeClass(name, super, 0, size, 0)` then `AddClass`, so the class is public
  under the name `"dopusiclass"` (`libinit.c:1364`). On 68k (not MorphOS) it
  sets `cl_Dispatcher.h_Entry = dispatcher` directly; `h_SubEntry` is NOT set.
  `h_Data = libs`. Consequence: the hook point is `h_Entry`, not `h_SubEntry`
  as with the ROM classes in `class_slot`. The dispatcher is a register
  function (a0 class, a2 object, a1 message), like `c2_disp_reg` expects.

Hookable entry points, best first:

1. `dopusiclass` `cl_Dispatcher.h_Entry` (class-level swap, same pattern as
   C2/C8). Catches every dopusiclass image, independent of caller.
2. dopus5.library jump table (D4 pattern). Not usable: `image_draw` is an
   internal function, not an LVO.
3. graphics `RectFill`/`Text` patch (`flat_rf_c`, ~`flat.c:1700`). Not usable:
   gated on intuition-ROM callers and it would need pen guessing. Rejected.

## 3. Pen cause: why it looks inverted

Flat pen map (`desk_pens_flat`, `desk_core.c:83-93`; rig dump): DETAIL 0,
BLOCK 0, TEXT 2, SHINE 0, SHADOW 0, FILL 3 (accent), FILLTEXT 0, BACKGROUND 0.

Flat's Workbench arrow (unselected), from `flat_glyph_roles`
(`flat_glyph.c:166`): knob gadgets get glyph = KNOB (accent knob pen), box =
FRAME (surface). Selected: glyph = BACK (surface), box = KNOB (accent).
So the owner's "accent box, dark glyph" is exactly flat's PRESSED look, or the
unselected look with ground and glyph swapped.

Candidates, all unverified, in order of likelihood for the lister:

- (A) Clone ground mis-pick. `clone_object` builds the private sysiclass
  image with `SYSIA_DrawInfo`, `SYSIA_Which`, `IA_Width`, `IA_Height`, but no
  `SYSIA_Size`. DOpus asks for `SYSISIZE_LOWRES/MEDRES`. The ROM then draws a
  different glyph shape, and possibly a different frame, at DOpus's box size.
  `clone_ground` takes the most common marker in the box. If glyph pixels
  outnumber ground pixels (a fat glyph in a small box), ground and glyph swap:
  the box gets the knob (accent) pen and the glyph gets the frame (surface)
  pen. That is the reported symptom. The flat clone slot key does not include
  size either, so one cached clone can serve two sizes.
- (B) State. DOpus may draw the arrow images with `IDS_SELECTED` at rest, or
  the arrows may show the pressed look the owner happened to see. Flat maps
  IDS 1 to the accent box by design. Prior research (s66 fix section, rig 66,
  OS 3.2) saw at rest: surface fill, accent outline, glyph in text/accent pen.
  The pressed state was not measured.
- (C) dopusiclass listview arrows (config windows, not the lister). Unhooked.
  At rest: BACKGROUND (surface) box, TEXTPEN (pen 2) glyph, SHINE/SHADOW border
  both surface. Pressed: FILLPEN (accent) box, FILLTEXTPEN (surface) glyph.
  That is the "inverted" look, but only while pressed. At rest the glyph is
  pen 2, not the accent knob pen, so it does not match flat either way.
- (D) OS 3.1 only: the lister arrows are `buttongclass` (GTYP_CUSTOMGADGET),
  so `c2_which_31` returns NOWHICH and the ROM draws with raw pens. Not the
  owner's case if the screenshot is from OS 3.2 (unknown).

Open: the owner's screenshot conditions (OS version, rig, lister vs config
window, pressed or not) are not recorded. The measurement step settles it.

## 4. Hook design

### 4.0 Measure first (rig 68, before any edit)

- Read STATUS `cf_*` counters after a lister redraw (`cf_lwhich`, `cf_lmarks`)
  to prove the lister arrows go through `c2_clone`.
- Add a temporary diagnostic in the clone path (not committed) that records
  for Which 0x0A-0x0D and 0x02: ids, box w/h, `SYSIA_Size` read from the
  original object, and the ground marker plus its pixel count vs glyph count.
- Result decides: ground count < glyph count for DOpus arrows = (A);
  ids = 1 at rest = (B); no c2 hit for the lister = new unknown, stop and
  report.

### 4.1 Fix (A) where it lives: the clone path

Violated invariant: the clone must render the same image the caller asked
for. Root cause level: `clone_object` drops `SYSIA_Size`.

- `c2_clone` reads `SYSIA_Size` from the original object (`GetAttr`; if the
  attribute is not gettable on the ROM in use, infer it from the image
  height against the ROM's LOWRES/MEDRES/HIRES heights; unverified which
  works) and passes it to `clone_object`.
- The clone slot key gains the size, so one cached clone per (Which, size,
  w, h).
- `clone_ground` stays as is. If the measurement shows a correct-size clone
  still has more glyph than ground pixels, make the ground choice structural
  instead of a vote: ground = the marker at the box interior corner just
  inside the frame (the ROM always paints the ground there for arrows and
  size). That change goes in `clone_ground` itself, so every caller gains it.

### 4.2 Fix (B): no code change if pressed

If the owner saw the pressed look, flat behaves as designed and matches
Workbench pressed arrows. Report to the owner. If DOpus draws ids = 1 at rest,
that is a DOpus-side state fact; treat as an open decision for the owner, not
a silent remap.

### 4.3 dopusiclass hook (C): new D5 stage

Entry point: `dopusiclass` `cl_Dispatcher.h_Entry`.

Install (in `hook_install`, after D4, `flat.c:2099-2175` pattern):

1. Only when dopus5.library is open and its version is in the known D4
   version table. Fail closed: unknown version = no hook.
2. Find the class: `NewObject(0, "dopusiclass", DIA_Type, IM_ARROW_UP,
   IA_Width, 16, IA_Height, 16, TAG_END)`, take `OCLASS(o)`, `DisposeObject`.
   Same throw-away pattern as `class_slot` (`flat.c:2001`). Extend
   `class_slot` with a field selector (h_Entry vs h_SubEntry) and a class name
   argument rather than writing a second finder.
3. Swap `h_Entry` with `flat_swap_install` under `Forbid`, saving the old
   entry. Uninstall restores it only if the slot still holds flat's entry
   (same rule as C2/C8).

Dispatcher `d5_disp_reg` (a0 class, a2 object, a1 message):

1. `msg->MethodID != IM_DRAW` (and not `IM_DRAWFRAME`): call saved entry.
2. `type = *(short *)INST_DATA(cl, o)`. Recognise:
   - `IM_ARROW_UP` to `FLAT_W_UP`, `IM_ARROW_DOWN` to `FLAT_W_DOWN`.
   - Every other type (CHECK, BBOX, BORDER_BOX, ICONIFY, lock, ...): call
     saved entry. BBOX/BORDER_BOX are already flattened by D4 and the pen
     map; widening scope is a separate decision.
3. Pens: reuse `c2_get_pens` on the message DrawInfo (knob and frame pens
   exactly as for a Workbench arrow). Box = `imp_Offset` + image
   `LeftEdge/TopEdge/Width/Height` (DIMENSIONS from `IM_DRAWFRAME`).
4. Draw: `flat_glyph_draw(s, FLAT_W_UP|DOWN, FLAT_IM_DRAW, imp_State, ox, oy,
   w, h, frame_pen, knob_pen, side, c2_fill, ctx)`. The block glyph path, not
   the clone: dopusiclass is not sysiclass, so there is no ROM shape to clone.
   `flat_glyph_draw` treats arrows as knob gadgets, so the roles are the same
   as a Workbench arrow by construction.
5. Fallback: any failure (no state, unknown version, `flat_glyph_known`
   false) calls the saved entry, which draws DOpus's own look.

Count calls in a new STATUS counter `cf_d5` so the rig can prove the hook ran.

Alternative considered: teach `c2_draw` to accept a dopusiclass type as a
Which. Rejected: the class swap is separate per class anyway, and the shared
part (`c2_get_pens` + `flat_glyph_draw`) is already a function call.

## 5. Host regression test

Pattern: existing C host tests `tests/test_flat_core.c`,
`tests/test_flat_role_core.c` (compile `flat_glyph.c` on the host with a
recording fill callback).

- `test_flat_dopus_arrow_roles`: run `flat_glyph_draw` for `FLAT_W_UP` with the
  pens a Workbench arrow gets, ids 0 and 1. Record the fill calls. Assert the
  box pen equals the frame pen and the glyph pen equals the knob pen at ids 0,
  swapped at ids 1. Then run the D5 dispatch core (factored into a pure
  `flat_d5_draw(type, ids, box, pens, fill, ctx)` in `flat_glyph.c`) with
  `IM_ARROW_UP` and assert the identical fill trace.
- Clone ground regression for (A): feed `flat_clone_map` a marker grid
  captured from the rig for the DOpus LOWRES arrow (glyph pixels > ground
  pixels). Assert the ground is the surface marker and the box paints the
  frame pen. Fails before the fix, passes after, fails on revert.
- Reachability: a call counter in the real `d5` entry (the function placed in
  `h_Entry`) and in `c2_clone`. A Python test in the `tests/test_flat_*.py`
  style drives the installed hook on the rig (or the existing host harness, if
  it can load the dispatcher) and asserts `cf_d5 > 0` after opening a DOpus
  config window with a listview, and `cf_lwhich > 0` after a lister redraw.
  Unverified: whether the existing host harness can call a dispatcher
  through a fake Class; if not, the reachability proof is the rig counter.

## 6. Rig check steps for the owner (rig 68 only)

1. `tools/rig/rig.py reboot 68` (never `stop`). Start with purpose text
   `"<session>: DOpus scroller look check"`.
2. Delete `ENVARC:Amarchy/flat-starting` before flat runs.
3. Run `amarchy-flat`, then start DirectoryOpus, then `amarchy-flat DOPUS`.
4. Open a lister. Open a Workbench drawer window next to it.
5. Compare at rest: arrow box surface, glyph accent, same as the drawer.
6. Press and hold a lister arrow, then a drawer arrow: both accent box,
   surface glyph.
7. Compare the size gadget and the vertical knob the same way.
8. Open DOpus Settings > Environment (any page with a listview). Compare its
   up/down arrows at rest and pressed.
9. Read `amarchy-flat STATUS`: `cf_d5` and `cf_lwhich` above zero.

PASS: every arrow, the size gadget and the knob match the drawer window in
both states. FAIL as reported: accent box with dark glyph at rest.

## Open unknowns

- Owner's screenshot conditions: OS version, lister or config window, pressed.
- Whether the ROM `sysiclass` honours `SYSIA_Size` as a gettable attribute.
- `BoopsiImageData.type` offset in the 5.104 binary (source says offset 0).
- Whether 5.104 contains the `lister_dual.c` scroller code (strings say likely).
- Whether `DOPUS_NEW_PROP_KNOB` is active on rig 68 (Intuition 47.44+ needed);
  if not, the knob is the propgclass default and C8 does not see it.
- Whether the host harness can drive a dispatcher through a fake `Class`.
