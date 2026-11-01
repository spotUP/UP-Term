---
date: 2026-10-31
topic: ROUND-CORNERS feasibility (real see-through window corners on 3.1 / 3.2)
status: research only, no code; 14 tool calls
ledger: thoughts/spot/todos.md:3819 ROUND-CORNERS
---

# Real rounded window corners on AmigaOS 3.1 / 3.2

## Verdict

| OS | Stock ROM layers | Feasible? | Route |
|----|------------------|-----------|-------|
| 3.1 (layers V40) | no shape API | yes, only by REPLACING layers.library | shaped-layer layers.library loaded with LoadModule before SetPatch |
| 3.2 (layers V45-47) | no shape API (checked NDK 3.2R4) | yes, same route, more risk | same, plus the V45 LVOs 3.2 Intuition expects |

Patching the stock layers with SetFunction is NOT tractable (see section 3).
No published tool did real see-through corners on classic 3.x (nothing found; unverified beyond two web searches).

## 1. 3.2 layers.library: no shape API

Source: /Users/spot/Code/Reference/NDK3.2R4/SFD/layers_lib.sfd (47.1, 30.11.2021) and Autodocs/layers.doc.
- Full function list ends at V45: LayerOccluded, HideLayer, ShowLayer, SetLayerInfoBounds. No ChangeLayerShape, no CreateLayerTagList, no LA_Shape* tags.
- grep "shape|ChangeLayer" over layers.doc, graphics/layers.h, graphics/clip.h: zero hits.
- InstallClipRegion (layers.doc ~line 650): "Installs a transparent Clip region ... All subsequent graphics calls will be clipped". This only limits DRAWING. The layer still owns its full bounds; layers behind stay obscured there, so the corner shows stale screen pixels, never the live content below.
- InstallLayerHook / BackFill hook: decides how damaged parts of THIS layer are filled. Also cannot give pixels to the layer behind.
- struct Layer (NDK 3.2 clip.h:41-79) has reserved1 and reserved2[18]; nothing shape-related public.
- ChangeLayerShape exists only in AROS hyperlayers (and MorphOS/OS4-family layers), never in a Commodore/Hyperion 3.x layers.

## 2. The working route: shaped layers.library replacement

Proof the delivery mechanism exists: Thomas Richter's layers V45 replaced ROM layers V40 on 3.1 via `LoadModule LIBS:layers.library` above SetPatch (https://aminet.net/package/util/sys/layers). A replacement must be in place before Intuition creates its Layer_Info; it cannot be swapped in a running session.

Shape-capable code to start from: AROS rom/hyperlayers (open source, AROS PL).
- ChangeLayerShape(layer, region, hook): https://raw.githubusercontent.com/aros-development-team/AROS/master/rom/hyperlayers/changelayershape.c (~250 lines). It backs up old content (_BackupPartsOfLayer), sets l->shaperegion / l->visibleshape, then gives the uncovered area to the layers behind (_ShowPartsOfLayer, and _BackFillRegion when the parent is the root). That is exactly "corners show what is underneath", with correct damage for simple-refresh windows below and backing store for smart-refresh ones.
- LA_ShapeHook (CreateLayerTagList) recomputes the shape on SizeLayer, so resizes are handled inside layers (https://www.freepascal.org/daily/packages/arosunits/layers/changelayershape.html, AROS status https://aros.sourceforge.io/pl/introduction/status/layers_ext.php).
- AROS struct Layer keeps the classic public offsets: the shape fields (shape, shaperegion, visibleshape, nesting, visible) are packed into the classic reserved2[18]; parent fills reserved[4]; VisibleRegion fills reserved1 (https://raw.githubusercontent.com/aros-development-team/AROS/master/compiler/include/graphics/clip.h). So ROM graphics and Intuition, which read public Layer fields, should still find them.

Amarchy side: after OpenWindow (or via an OpenWindowTagList patch, same style as flat_rf.s), call ChangeLayerShape(win->WLayer, NULL, cornerhook); the hook builds a Region = layer rect minus 4 corner staircases (radius r, ~2r-4r small rects per corner via XorRectRegion/ClearRectRegion). Intuition border drawing into the corners is then clipped away by the cliprects automatically.

## 3. Why SetFunction on the stock layers is not tractable

The decision "layer A covers rectangle R" is made in internal (non-LVO) routines from l->bounds, inside every geometry LVO. To carve corners from outside you would have to re-do the cliprect lists of the window AND every layer behind it after each of:
CreateUpfrontLayer -36, CreateBehindLayer -42, UpfrontLayer -48, BehindLayer -54, MoveLayer -60, SizeLayer -66, BeginUpdate -78, EndUpdate -84, DeleteLayer -90, MoveLayerInFrontOf -168, InstallClipRegion -174, MoveSizeLayer -180, CreateUpfrontHookLayer -186, CreateBehindHookLayer -192, and on 3.2 HideLayer -228, ShowLayer -234 (bias 30, order from layers_lib.sfd).
Each re-carve must: allocate ClipRects with ROM layers' private allocator/pool, move corner pixels between the window's and the lower layers' backing bitmaps (smart refresh), add damage regions + LAYERREFRESH for simple-refresh layers below, keep DamageList/Undamaged/clipped consistent across BeginUpdate/EndUpdate, and survive the next internal rebuild that throws it all away. That is a second layers engine glued onto a closed one whose private state differs between V40 (3.1) and V45/47 (3.2). Estimate 3-5k lines asm/C, two variants, very high risk; worse than a replacement.

## 4. Size estimate (route 2)

- Port AROS hyperlayers to classic 68k on Kickstart 3.1/3.2 graphics: strip composited/surface/HIDD/cybergraphics paths (basicfuncs.c alone ~1.4k lines; library total roughly 8-12k lines C, unmeasured), replace AROS_LH macros with SFD-generated stubs, keep the classic LVO table order and append ChangeLayerShape/CreateLayerTagList after -240.
- Add the four V45 functions 3.2 Intuition may call (LayerOccluded, HideLayer, ShowLayer, SetLayerInfoBounds); AROS has ChangeLayerVisibility as a base.
- Amarchy: ~150-250 lines (corner region builder, shape hook, install in OpenWindow path, LoadModule line in the boot script, kill switch).
- Effort: weeks, not days. Risk: high. Needs its own rig soak (open/move/size/depth/close storms, smart + simple + superbitmap + GZZ windows, backdrop WB).

## 5. Top risks

1. ClipRect backing-store convention. AROS stores the backing-bitmap offset in cr->_p1/_p2 (CR_BMOFFX/CR_SETBMOFF, ALIGN_OFFSET) and may share a layer "surface" bitmap (CR_SHAREDBITMAP). ROM graphics V40/V47 draws into obscured smart-refresh ClipRects using the classic implicit convention (bitmap origin = MinX & ~15) and ignores _p1/_p2. Mismatch = corrupted drawing in every obscured smart-refresh window. Must force ALIGN_OFFSET(x) = x & 15 and disable shared surfaces. UNVERIFIED which value AROS m68k uses.
2. 3.2 Intuition V47 depends on layers V45+ behaviour (HideLayer/ShowLayer, LayerOccluded, Richter's memory-failure semantics) and possibly private Layer_Info fields; a foreign layers may break iconify/hide, screen dragging or depth gadgets. 3.1 is safer than 3.2 here.
3. Whole-system blast radius: layers runs under every window of every program, and LoadModule makes it reset-resident. A bug means Guru at boot or corrupted windows everywhere, not just in Amarchy. Also: third-party patches that SetFunction layers (MCP, Birdie, VisualPrefs-like, PowerWindows) assume ROM internals; Intuition hit-testing (WhichLayer) must respect the shape or clicks in a corner still go to the window above; performance of region ops on 68020/030 during opaque moves.

## 6. Existing art

- AROS: hyperlayers ChangeLayerShape + Intuition WA_ShapeRegion/WA_ShapeHook/ChangeWindowShape (https://www.freepascal.org/daily/packages/arosunits/intuition/wa_shapehook.html).
- MorphOS and AmigaOS 4: shaped-window support in their own layers/Intuition (from memory; not confirmed by the search, which found no autodoc).
- Classic 3.x: no tool found that made real see-through corners. Visual patches of that era (VisualPrefs, Birdie, MagicFrames-type) only paint frames; Richter's layers V45 is the only known layers replacement for 3.1 and it has no shape API.

## Sources
- /Users/spot/Code/Reference/NDK3.2R4/SFD/layers_lib.sfd, Autodocs/layers.doc, Include_H/graphics/clip.h
- https://aminet.net/package/util/sys/layers
- https://raw.githubusercontent.com/aros-development-team/AROS/master/rom/hyperlayers/changelayershape.c
- https://raw.githubusercontent.com/aros-development-team/AROS/master/rom/hyperlayers/basicfuncs.c
- https://raw.githubusercontent.com/aros-development-team/AROS/master/compiler/include/graphics/clip.h
- https://aros.sourceforge.io/pl/introduction/status/layers_ext.php
- https://www.freepascal.org/daily/packages/arosunits/layers/changelayershape.html
