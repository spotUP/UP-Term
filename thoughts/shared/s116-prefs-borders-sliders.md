---
date: 2026-11-25
row: PREFS-BORDERS-SLIDERS-NOT-FLAT (todos.md ~790)
status: findings only, no code change (agent budget)
---
# s116 Palette prefs: what is still not flat on rig 12 (main 30aa33d3)

Evidence: ami.grab of rig 12 front screen (616x315, Palette Preferences, SYS:Prefs/Palette 40.2 = the 3.1 editor),
uitree gadget boxes, pixel counts of SHINE colour a9b1d6 (pen 2) and SHADOW colour 0e0e14 (pen 1) per box.
Font and Input prefs were not grabbed (behind Palette; rig 12 left as the owner set it) - unverified.

## Element list (Palette Preferences)

| Element | Gadget / source | Pixel finding | Flat? |
|---|---|---|---|
| Save / Use / Cancel | GadTools BUTTON_KIND, frameiclass | rounded accent fill, no shine/shadow | yes (already fixed) |
| Red / Green / Blue sliders | GadTools SLIDER_KIND (prop 149x7 in a 157x11 frame) | 1 px accent 7aa2f7 outline, flat 6x7 knob 414868, 0 shine 0 shadow px | yes on the static screen; drag/active state not checked |
| Brightness bar (vertical, 18x91 at 194,42) | gradientslider.gadget (G15, custom) | 2 px SHINE L top/left + 2 px SHADOW L bottom/right (196 + 196 px) | NO - 3D bevel |
| Example "Active" window | Palette 40.2 own sample drawing | border: outer shine left/top, inner shadow; bottom row 306 shine / 307 shadow; "Eat Food" box shine top/left + shadow bottom/right; "Calories" string box same; close/zoom/depth glyphs stock; scroller container 50 % dither; size gadget stock | NO |
| Example "Inactive" window | same | same bevel pattern (1263 shine / 825 shadow px), dither scroller, stock radio/checkbox images | NO |
| Example "Restaurant" menu | same | separators as dotted (dither) rows, stock Amiga-key boxes | partly (no shadow, but dotted separators) |
| Pen swatch row under the list (G17, disabled) | Palette custom gadget, ghosted | 50 % ghost dither over swatches | stock ghost pattern (content, owner to judge) |
| List, list scroller, palette row, colour wheel | listview / custom | no bevel (shine/shadow px are swatch colours or text) | yes |

## Root cause

Violated invariant: "on a themed screen every bevel drawn with SHINEPEN/SHADOWPEN is replaced by the flat outline".
Patina-flat only enforces it on the paths it hooks: frameiclass (c8_skip -> flat_frame_decide, themed arg since s96),
sysiclass glyphs, ROM-called RectFill (flat_rf_c: rf_edge/rf_side/rf_scroller are gated on `rom`), the ROM menu Draw.
On KS 3.1 the Workbench DrawInfo pen map stays stock (SHINE 2, SHADOW 1, owner decision 2026-11-22, s96 doc), so any code
that draws its own bevel with Move/Draw/RectFill in pens 2 and 1 from outside the ROM draws 3D:
- gradientslider.gadget (disk class, SYS:Classes/Gadgets) draws its 2 px double bevel itself, no frameiclass.
- Palette 40.2 renders the three sample windows itself with the edited pen array (it previews the pens being edited), so
  neither the window-border hook nor frameiclass sees them; callers are not in the ROM, so the RectFill rules skip them.
Root-cause level: patina-flat hook coverage (graphics-level drawing from non-ROM callers), not the theme colours.

## Why no fix in this pass

A general rule ("non-ROM Draw/RectFill in pen 1 or 2 on a themed window becomes the outline pen") is wrong on 3.1:
SHADOWPEN == TEXTPEN == 1 and SHINEPEN == HIGHLIGHTTEXT == 2, so text-coloured lines (checkmarks, arrows, the sample's own
glyphs) would recolour. A safe fix needs one of:
1. gradientslider.gadget: hook its class dispatcher GM_RENDER (as c8 does frameiclass): let it render, then repaint the
   2 px border ring with the flat outline (box = gadget box; ring rows known). Small, contained. Recommended first.
2. Palette sample: a per-caller rule keyed on the Palette 40.2 segment (checksum-gated like the DOpus D4 table): Draw/RectFill
   pairs that form a shine L + shadow L over one box become a 1 px outline; dither fills (AreaPtSz) in its scroller box become
   the flat scroller ground. Bigger; needs an owner call whether the sample should show the edited Bright/Dark Edges pens
   (what Palette is for) or the flat look.
3. Not checked: RGB slider look while dragging/active; Font and Input prefs on rig 12.


## s116b fix 2: Palette example windows flat (branch flat-preview)

Violated invariant: on a themed Workbench every bevel (SHINE/SHADOW pair) and scroller dither is drawn flat. Palette 40.2 breaks
it because it draws the samples itself: RectFill from its own hunk (34 RectFill, 1 Draw call sites in the 40.2 disassembly).
Root level: patina-flat RectFill hook coverage for non-ROM callers. Levels considered: (a) pen map (wrong: owner keeps 3.1
SHINE 2/SHADOW 1, and pen 1/2 are text pens), (b) a generic non-ROM pen rule (recolours text/arrows), (c) per-call-site rule
keyed on the identified Palette hunk (chosen: exact, Palette-only).
Measured (objdump of 40.2): 0x5428 = bevel routine (pen A top/left at ret 54ac/54e4, pen B bottom/right at 5526/5556);
0x538e = two-pen dither box (ground at 53da, patterned fill at 5418); 0x5bee/0x5df6 = patterned scroller-container fills
(pens[BLOCK], pattern at data+2184). Identity: hunk 0 size 0x6704, jsr -306 before every site, FNV-1a of 0x5428..0x5568
(no relocs there) = 0x99a5f5e6. Rule: edge sites draw in flat_edge_pen; dither sites taller than 2 rows are skipped (ground
stays), 2-row dithers draw solid. Not covered: title-bar glyphs and size gadget (DrawImage at 0x59e0, data images), radio and
checkbox images.

Rig proof (rig 7, 2026-11-25, branch 2fe3c469 patina-flat over main's build): Palette sample boxes, px of a9b1d6 / 0e0e14
(a9b1d6 is also the text pen): Active 758 / 232 (baseline 1521 / 1095), Inactive 824 / 286 (1263 / 825), Restaurant
4535 / 0. Bevel edges gone; the dark residue is the title-bar close/zoom/depth glyphs and size gadget (DrawImage at 0x59e0)
and the Inactive radio/checkbox images. Restaurant separators are still dotted: their patterned fill is not at the
0x5bf2/0x5dfa dither sites. Both open.
