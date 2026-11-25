
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
