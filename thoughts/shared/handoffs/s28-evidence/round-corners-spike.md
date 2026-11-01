---
date: 2026-10-31
topic: ROUND-CORNERS spike - port AROS rom/hyperlayers to Kickstart 3.1 / 3.2 graphics
status: research only, no code, no rig; 27 tool calls
parent: thoughts/shared/handoffs/s28-evidence/round-corners.md
---

# Spike: AROS hyperlayers as a replacement layers.library on 3.1 / 3.2

Source: sparse shallow clone of github.com/aros-development-team/AROS master (2026-10-31),
rom/hyperlayers + compiler/include/graphics + arch/m68k-amiga. Clone deleted after the spike;
paths below are AROS-tree relative. NDK = /Users/spot/Code/Reference/NDK3.2R4.
3.2 binaries checked: build/rig/os-322/Libs (intuition 47.52, layers 46.2).

## Verdict

- Effort: 6-10 weeks for one engineer to a soak-clean 3.1 + 3.2 build (3.1 only: 4-6 weeks).
- Recommendation: NO-GO as a direct "port"; conditional GO only if the one cheap test below passes.
  hyperlayers is not a drop-in: it calls AROS-only graphics region functions throughout, its
  smart-refresh backing store uses an offset convention ROM graphics does not read, and its V45+
  LVO slots collide with the four 3.2 V45 functions that 3.2 Intuition calls.
- Biggest unknown: whether Kickstart graphics (V40 ROM / 3.2 V47) draws correctly into an AROS-built
  ClipRect list at all, i.e. whether ROM graphics really uses the implicit `MinX & 15` offset for
  obscured smart-refresh ClipRects and nothing in layers-private fields (vlink/home/Flags, Layer_Info
  privates) is read by ROM graphics or Intuition.
- Cheapest test (1-2 days): build hyperlayers for m68k with ONLY the classic 31 LVOs + ChangeLayerShape
  appended at -246, regions shim linked in, views (basicfuncs.c:864) and surfaces disabled; LoadModule it
  on a 3.1 rig; open two overlapping smart-refresh shell windows, draw/scroll in the obscured one, depth-
  and drag-arrange, then a superbitmap window. Clean pixels = convention matches, the port is real work but
  bounded. Corruption = ROM graphics and AROS cliprects disagree; stop.

## 1. Size (wc -l, rom/hyperlayers, 8322 lines C/H total)

| Part | Lines | Files |
|------|------:|-------|
| Classic 3.1 LVO entry points (31 funcs, InitLayers..DoHookClipRects, layers.conf:28-62) | 3381 | 32 .c |
| Shared engine (basicfuncs.c 2004, basicfuncs.h, layers_intern.h, intregions.h, layers_init.c) | 2535 | 5 |
| Internal helpers the classic calls also use (createlayertaglist.c 529, movelayerz.c 258, getfirstfamilymember.c) | 817 | 3 |
| AROS/MorphOS extras (ChangeLayerShape 254, ChangeLayerInputShape, ChangeLayerVisibility 223, CollectPixelsLayer 298, ScaleLayer 238, Is* x3, Create*LayerTagList x2) | 1589 | 10 |

A shape-only port needs ~6700 of the 8322 (everything except ScaleLayer, CollectPixelsLayer, input
shape, IsLayerHitAt, and the composited paths). 42 files use AROS_LH* library-function macros.

## 2. AROS-only dependencies and their cost

graphics.library calls with NO entry in NDK3.2R4 SFD/graphics_lib.sfd (counted uses in rom/hyperlayers/*.c):

| Call | Uses | Replace with |
|------|-----:|--------------|
| SetRegion | 30 | shim: ClearRegion + OrRegionRegion copy |
| ClearRegionRegion | 19 | shim: XOR/AND-NOT region subtract (port AROS rom/graphics region code) |
| AndRegionRegionND | 8 | shim (non-destructive AND; may be layers-local) |
| AndRectRect | 7 | trivial inline |
| IsPointInRegion | 2 | shim walking RegionRectangles |
| NewRectRegion | 3 | already local (layers_intern.h / intregions.h:48) |
| LayerSurfacesChanged, LayerSurfaceAlpha, CompositeLayersSupported, HIDD_CompositeRequest (basicfuncs.c:720-750) | ~6 | delete: compositor path |
| GetBitMapAttr | ok | exists in V39+ |

Cost: a private region library of ~600-900 lines C (port of AROS rom/graphics regions), plus the
3.x region ops are O(n^2) on 68k; ChangeLayerShape staircase regions (2r rects per corner) on every
move/size make this a speed risk on 020/030.

Other AROS-isms:
- AROS_LH*/AROS_UFH* macros (42 files): mechanical, SFD-generated stubs + `__asm`/register args (1-2 days).
- `#include <cybergraphx/cybergraphics.h>` (basicfuncs.c:15, createlayertaglist.c:14): drop with compositor.
- `TaggedOpenLibrary(TAGGEDOPEN_GRAPHICS)` (layers_init.c:18): AROS exec; replace with OpenLibrary.
- `GfxBase->gb_LayersBase = LIBBASE` (layers_init.c:36): exists in classic GfxBase (NDK gfxbase.h:110) - OK, and required.
- IntLayer wraps struct Layer with private tail (layers_intern.h:53-75: shapehook, window, intflags, own rp, surface): fine as long as the layer is allocated by layers itself (it is).
- Hook calling convention: AROS_UFH3 with A0/A2/A1 (clip.h shape hook comment) = classic hook ABI on m68k. OK.

## 3. Risk 1: struct layouts vs NDK 3.2 graphics/clip.h and graphics/layers.h

Sizes match; field meanings differ in private/reserved slots.

struct Layer (AROS compiler/include/graphics/clip.h:32-92 vs NDK clip.h:41-79):
| Offset field (NDK) | AROS | Status |
|---|---|---|
| front, back, ClipRect, rp, bounds | same | match |
| nlink (V45, was reserved[4]) | parent (PRIVATE) | collision in meaning only; 3.2 Intuition/graphics reading nlink would read the AROS parent. Unknown if anything outside layers reads it. |
| priority, Flags, SuperBitMap, SuperClipRect, Window, Scroll_X/Y | same | match |
| OnScreen, OffScreen, Backup, SuperSaveClipRects, Undamaged | cr, cr2, crnew, SuperSaveClipRects, _cliprects | same slots, layers-private use; Undamaged vs _cliprects: both "saved list during BeginUpdate" |
| LayerInfo, Lock, BackFill | same | match |
| reserved1 | VisibleRegion | OK (reserved) |
| ClipRegion | same | match |
| clipped | saveClipRects (struct Region *) | type mismatch: NDK says ClipRect list, AROS stores a Region. Only matters if Intuition/graphics read it. |
| Width, Height | same | match |
| reserved2[18] | shape, shaperegion, visibleshape, nesting, SuperSaveClipRectCounter, visible, reserved2[2] | OK (reserved) |
| DamageList | same | match |

Layer flags: AROS LAYERCOMPOSITED = bit 3 (clip.h:141), free in NDK. NDK V44/V45 flags LAYERSAVEBACK 0x800 and
LAYERHIDDEN 0x1000 (NDK layers.h:42-43) are absent from AROS (it uses l->visible). 3.2 Intuition may test LAYERHIDDEN.

struct ClipRect (AROS clip.h:96-108 vs NDK clip.h:96-124), same size (36 bytes):
| NDK | AROS | Status |
|---|---|---|
| Next | Next | match |
| reservedlink | prev | OK |
| obscured (LONG, V45: test non-zero only) | lobs (Layer *; set to TRUE for views, basicfuncs.c:891,565) | compatible as a non-zero test |
| BitMap | BitMap | match, but see offset below |
| bounds | bounds | match |
| vlink | _p1 = backing-store X offset (CR_BMOFFX) | MISMATCH in role |
| home (Layer_Info *, pool owner) | _p2 = backing-store Y offset (CR_BMOFFY) | MISMATCH in role |
| reserved, Flags | reserved, Flags (CR_SHAREDBITMAP=4) | OK |

The offset convention is the real risk:
- Own-bitmap ClipRects: AllocBitMap(w + 16), offset ALIGN_OFFSET(MinX + aligndx), Y 0 (basicfuncs.c:450-460;
  swapbitsrastportcliprect.c:98). After the pending MoveLayer this equals `MinX & 15` - the classic implicit
  convention ROM graphics assumes (from memory, not verified in ROM code).
- Views: during BeginUpdate (beginupdate.c:123) and clip-region install (basicfuncs.c:1973) hidden smart-refresh
  ClipRects become views into another ClipRect's bitmap at an arbitrary offset (basicfuncs.c:437-443, 864-893).
  ROM graphics ignores _p1/_p2, so it would draw at the wrong place. Must be removed and replaced by the
  classic copy-out/copy-in of obscured parts. This is engine surgery in basicfuncs.c (est. 300-500 lines).
- Surfaces: LAYERS_BACKING_SURFACE defaults to 0 (layers_intern.h:49-51); composited layers set
  CR_SHAREDBITMAP offset 0,0 (basicfuncs.c:474, 566). Off by default for plain CreateUpfrontLayer; strip.

struct Layer_Info (AROS compiler/include/graphics/layers.h vs NDK layers.h:55-84):
| NDK 3.2 | AROS | Status |
|---|---|---|
| top_layer | top_layer | match |
| resPtr1, resPtr2 | check_lp, obs | V40 names; AROS matches the 3.1 layout |
| FreeClipRects | FreeClipRects | match |
| bounds (struct Rectangle, V45) | PrivateReserve1, PrivateReserve2 (2 x LONG) | MISMATCH for 3.2: SetLayerInfoBounds must exist and the bounds must clip every layer |
| Lock, gs_Head, PrivateReserve3/4, Flags | same | match |
| res_count, LockLayersCount | fatten_count, LockLayersCount | OK |
| PrivateReserve5 (BYTE), UserClipRectsCount (BYTE) | PrivateReserve5 (WORD) | OK |
| BlankHook, resPtr5 | BlankHook, LayerInfo_extra | OK |

## 4. 3.2 Intuition and the V45 calls

NDK SFD/layers_lib.sfd:45-49 adds LayerOccluded -222, HideLayer -228, ShowLayer -234, SetLayerInfoBounds -240.
AROS layers.conf puts different functions in exactly those slots: ChangeLayerShape -222, ScaleLayer -228,
CreateUpfrontLayerTagList -234, CreateBehindLayerTagList -240 (layers.conf ".version 45" / ".version 50").
A drop-in AROS layers on 3.2 would make Intuition's HideLayer call ScaleLayer. The extras must move to -246+.

Byte scan of build/rig/os-322/Libs/intuition.library 47.52 for `movea.l $56c(aN),a6; jsr -nn(a6)`
(base at $56c taken as LayersBase: it also hits -30 and -150 = DisposeLayerInfo; heuristic, other load
patterns not counted): HideLayer -228 x2, ShowLayer -234 x2, SetLayerInfoBounds -240 x1, LayerOccluded 0 seen.
So 3.2 Intuition uses at least three of the four.

hyperlayers equivalents:
- HideLayer / ShowLayer(l, in_front_of): ChangeLayerVisibility (223 lines) + MoveLayerInFrontOf; must also set LAYERHIDDEN.
- LayerOccluded: IsLayerHiddenBySibling (109 lines) is close; ~50 lines.
- SetLayerInfoBounds: none. AROS clips against the root/screen, not li->bounds; new code in the
  VisibleRegion computation (~100-200 lines) plus semantics reverse-engineered from layers.doc.
- Also 3.2 layers is 46.2 (Richter): memory-failure semantics, LAYERSAVEBACK. Unknown what Intuition 47 relies on.

## 5. Has hyperlayers ever run on Kickstart graphics?

No evidence. arch/m68k-amiga/boot/mmakefile.src:184 and :393 build `graphics layers intuition ...` together into
the AROS ROM: AROS m68k ships its own graphics.library (with HIDD amigavideo / p96gfx) and its own Intuition.
hyperlayers there talks only to AROS graphics, which reads CR_BMOFFX/Y. Nothing found that loads AROS layers
under Commodore/Hyperion graphics; the only 3.x layers replacement known is Richter's V45 (no shapes).
rom/hyperlayers/todo still lists "handling of errors: out of memory error!" open.

## Effort breakdown (estimate)

| Item | Weeks |
|---|---|
| Build harness (m68k gcc, SFD stubs, LoadModule-able library, kill switch) | 0.5 |
| Region shim library | 0.5-1 |
| Strip compositor/surface/scale/collect paths | 0.5 |
| Replace view ClipRects with classic copy-out/copy-in; verify offset convention against ROM graphics | 1.5-3 |
| V45 API for 3.2: Hide/Show/LayerOccluded/SetLayerInfoBounds + LVO relocation + LAYERHIDDEN | 1-2 |
| Amarchy side (corner region, shape hook, OpenWindow install) | 0.5 |
| Rig soak 3.1 + 3.2 (smart/simple/super/GZZ/backdrop, iconify, screen drag, low-memory) + A1200 | 1.5-2.5 |
| Total | 6-10 (3.1 only: 4-6) |
