# Ghost dither removal: where the pattern comes from, what flat can catch (code reading only, no rig)

Sources: NDK3.2R4 Autodocs (intuition.doc:4112-4120, button_gc.doc:86, string_gc.doc:49, tabs_gc.doc:63/138, clicktab_gc.doc:148,
speedbar_gc.doc:339, chooser_gc.doc:84, space_gc.doc:70), Include_H/intuition/imageclass.h:96 (IA_SupportsDisable), intuition.h:361
(GFLG_IMAGEDISABLE); amarchy/flat.c, flat_core.c, flat_glyph.c; thoughts/shared/research/s28-evidence/button-disabled.md.
UNVERIFIED on the rig: the exact ghost call (pattern word, rect, caller). The NDK docs say only "ghosting pattern"; no header names
0x2222/0x8888. The s28 button-disabled check list was written "NOT RUN, host only"; the ghits hit is itself unproven on 3.1/3.2.

## What paints the ghost
Intuition ROM (all versions) draws a traditional gadget, then ghosts it: one pattern RectFill over the gadget box with rp->AreaPtrn set
(AreaPtSz != 0). From V39 an image that returns IA_SupportsDisable=TRUE (GFLG_IMAGEDISABLE set by Intuition) is NOT ghosted by Intuition;
the imageclass draws IDS_DISABLED itself (sysiclass does; flat_glyph.c:156 maps IDS_DISABLED -> FLAT_G_INACTIVE, line 165 uses FLAT_ROLE_LINE).
Class gadgets on 3.2 (button/string/tabs/clicktab/chooser/space/speedbar/datebrowser, ClassAct-style) ghost themselves, in disk libs
(Sys:Classes/Gadgets/*.gadget), "dusted in a ghosting pattern rendered in SHADOWPEN" (tabs_gc.doc:63, clicktab_gc.doc:148).

## Existing hook (already in tree)
flat_rf_c, amarchy/flat.c:642-672: gate = enabled && !held && rp->Layer->Window && ret in [irom_lo, irom_hi) (ROM caller, set at flat.c:731-732).
flat_gh_match (flat_core.c:453) = AreaPtSz != 0 AND the fill rect lies inside a recorded filled primary button box (same layer surface + task),
recorded by flat_gh_frame (flat.c:418, c8_skip) and the label by flat_gh_text (flat.c:491). On match: solid fill with flat_dim_pen
(60 percent of button fill, flat_core.c:424), label redrawn in pen_dimtext (DK_MUTED, desk_core.c:147), AreaPtrn restored. So the
"dim colour instead of dither" mechanism exists for ONE case only.

## Cases
| # | Source | Detectable by RectFill patch | Proposed replacement |
|---|--------|------------------------------|----------------------|
| 1 | Intuition ghost over a filled primary GadTools/plain button (ROM RectFill, AreaPtSz!=0) | YES, done (flat.c:642). Reliability rests on the box being recorded first | keep |
| 2 | Same over a secondary (outline) button | PARTLY: flat_gh_frame drops secondary boxes (flat_core.c:397), so the dither passes through (s28 doc "known limit") | record secondary boxes too; on match draw the dim outline pen (FLAT_ROLE_LINE) and dim label, skip the fill |
| 3 | Disabled string / integer gadgets (ROM ghost over the string box, no filled-button record) | YES for ROM caller + AreaPtSz!=0 + rect inside a gadget box found by walking w->FirstGadget with GFLG_DISABLED (as the scroller path does, flat.c:649-665); no record exists today | add a gadget-walk match (GFLG_DISABLED, rect within gad box); draw solid pen_dim fill and re-Text the contents in pen_dimtext, or skip the fill and repaint the box with the flat string frame in dim line pen |
| 4 | Disabled checkbox / mx / cycle / slider / scroller (GadTools, drawn by ROM sysiclass-like images + ghost) | YES same gadget-walk; V39+ sysiclass imagery handles IDS_DISABLED itself (no ghost fill), then flat_glyph already dims via FLAT_G_INACTIVE | gadget-walk match as case 3; glyph path already covered |
| 5 | Prop gadget disabled | ghost fill over knob/track; scroller_pen path (flat.c:675) runs first and has no disabled test | add GFLG_DISABLED test: pen_dimtext knob on dim track, no ghost |
| 6 | Ghosted menu items / subitems | NO as written: menu strips render in the screen BarLayer; rp->Layer->Window is NULL or the menu layer has no Window, so the gate at flat.c:646 fails. Caller is still ROM, AreaPtSz!=0, rect inside the item box | needs a second gate (Layer without Window, ROM caller, AreaPtSz!=0, inside an item box computed from the strip; the menu text Text-patch already sees the item string); then skip the fill and Text again in pen_dimtext. Unverified: whether 3.1 uses Layer->Window NULL |
| 7 | Disabled BOOPSI gadgets of ROM images with GFLG_IMAGEDISABLE (gadtools on 3.2, sysiclass) | no ghost is painted; imageclass draws IDS_DISABLED | covered by flat_glyph (FLAT_G_INACTIVE); check each IA_SupportsDisable image |
| 8 | Disk-library gadget classes (button/string/tabs/clicktab/chooser/space/speedbar/datebrowser .gadget, 3.2 Sys:Classes/Gadgets, plus ReAction/TextEditor) | NO: caller is outside [irom_lo, irom_hi) (disk lib), so the ROM gate rejects. Detectable only by widening the gate to those libs' ranges, or by AreaPtrn+Window alone (unsafe: user apps draw patterns) | extend the caller-range check to loaded class libraries (resident segment ranges), or patch their render (not possible from RectFill); ghost is rendered in SHADOWPEN, so a recolour of the SHADOWPEN... not possible without the class |
| 9 | Ghost via BltPattern (graphics call used by some app/class code) or direct blitter (BltBitMapRastPort with mask) | NO: flat patches RectFill and Text only. BltPattern would need a new LVO patch (and then the same caller gate) | add BltPattern LVO patch only if the rig shows it; RectFill is what ROM uses per NDK docs, unverified |
| 10 | Ghosted icons (Workbench) | NO: Workbench/icon.library ghost is a disk-library blit (icon.library 44 / wb), not ROM RectFill; also separate "ghosted" image bitmap in the .info on 3.2 | out of scope for the RectFill patch; needs icon.library/wb hook or theme icons with a disabled image |
| 11 | Application-made ghosts (apps call SetAfPt + RectFill themselves) | NO: caller is app code, correctly rejected by the ROM gate | leave |

## Reliability notes
- Detect = ROM caller AND AreaPtSz != 0 AND rect inside a known disabled-gadget box. AreaPtSz alone also catches window-border/scroller dithers (already handled separately), so the box match is required.
- The ghost rect is NOT guaranteed to equal the gadget box (Intuition may ghost the image region only; s28 doc line 11-12 flags this). flat_gh_match accepts any rect inside the box.
- Dim fill is pen-based (flat_dim_pen needs depth 3..8, falls back to button pen at depth<3, flat_core.c:424): a 4-colour screen gets no dim tone, only no-dither.
- pen_dim is obtained once (CTRL_D to refresh).

## Next step (if the owner wants it)
1. Rig-prove case 1 first (ghits sentinel; s28 checks 1-7 never ran). 2. Log the ghost call (caller, AreaPtrn word, rect) for a disabled string, checkbox, menu item and a 3.2 class gadget to settle cases 3,6,8,9. 3. Then add the gadget-walk match (cases 3-5) as one function in flat_core.c with host tests.

Scratch: none created, nothing deleted.
