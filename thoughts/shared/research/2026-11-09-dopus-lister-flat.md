---
topic: DOpus 5 lister under amarchy-flat: why it does not match other flat windows
date: 2026-11-09
status: read-only analysis; V = read in code, U = UNVERIFIED (inferred, not measured on a rig)
---

# DOpus lister vs flat windows

Paths: A = vtcon/amarchy/, D = ~/Code/Reference/dopus5/source (vendor 5.104 source; dopus5.library on the rig is the vendor binary, Makefile:2140).

## 1. Intuition window path (what "ordinary flat windows" are)
- Pen map: desk_pens_flat (A/desk_core.c:83-93): SHINE, SHADOW = surface pen; slot 12 = BARBLOCK. FILLPEN deliberately stays the ACCENT and FILLTEXTPEN the surface (comment desk_core.c:90-92, S57-LOCALE-LIST-NO-SELECT). So Intuition's own active frame fill (FILLPEN) is accent. Rig dump of a flat Workbench: DETAIL 0, BLOCK 0, TEXT 2, SHINE 0, SHADOW 0, FILL 3, FILLTEXT 0, BACKGROUND 0 (thoughts/shared/s66-evidence/a5-gadget-fill.md:25).
- Pens alone leave: bevels vanish, accent frame (research 2026-10-29-amarchy-flat-ui-patch.md section 7/12). amarchy-flat repaints on top by patching graphics RectFill (A/flat.c:1745 rf_install, hook flat_rf_c :1700-1730) and Text, and the sysiclass dispatcher (flat.c:82 C2, clone render :261):
  - title band stays accent, title text in pen_ptext: flat_title_decide (flat_core.h:327-340), FLAT-TITLE-ACCENT.
  - left/right/bottom border strips filled in FILLPEN are repainted in the surface pen: rf_side (flat.c:1693-1709) -> flat_side_fill_pen (flat_core.c:805-815). Condition: s->enabled, pen_outline set, fg == pen_outline, fill wholly in a side strip.
  - 1 px ring: rf_edge (flat.c:1680-1691).
  - window gadgets (close, depth, zoom, size, arrows) are sysiclass images: replaced by flat glyphs drawn by the sysiclass hook (flat.c:82+, C2/C8), band pen = flat_frame_pen.
- EVERY RectFill rewrite above is gated `rom` = caller return address inside intuition ROM (flat.c:1712-1727: `rom = ret >= r->irom_lo && ret < r->irom_hi`, `if (on && w && rom && ...)`). V.
- Pens are per screen: st_scr -> flat_draw_state (flat.c:317-338); a screen with no slot has its four own pens = 0 and the functions fall back to stock (flat.c:317-319). V.

## 2. DOpus lister path
Parts and who draws them:
| Part | Drawn by | Pens |
|---|---|---|
| window frame, title text, close/depth/zoom (system ones) | Intuition ROM + sysiclass (same path as section 1) | FILLPEN etc. |
| title-bar gadgets lock/zoom/iconify | dopusiclass images (D/Library/image_class.c IM_ICONIFY :307-330, created by CreateTitleGadget D/Program/graphics.c:150) | background RectFill with `pens[FILLPEN]` (active) / BACKGROUNDPEN (inactive), then inner square SHADOWPEN, box lines SHINE/SHADOW (:316-318) |
| scroll arrows | dopusiclass IM_ARROW_UP/DOWN (image_class.c:109-116): listview_border THICK (D/Library/listview_class.c:1435-1452, 2 px SHINE/SHADOW pairs) then RectFill `pens[FILLPEN]` when selected else BACKGROUNDPEN | |
| size gadget, border boxes | dopusiclass IM_BORDER_BOX: `RectFill` with FILLPEN unless inactive (image_class.c:297-303) | |
| info/status/path/command areas ("D:[0/0] F:[0/10] B:..") | DirectoryOpus program, lister_title_pens (D/Program/lister_title.c:379-421): the SOURCE lister uses `ENVCOL_SOURCE` pens (fg,bg), type 1 does SetAPen(bg) so the area is cleared in the source background pen | env source_col |
| toolbar empty-area stipple | lister_toolbar.c:237-260 (0xaaaa/0x5555 pattern, SetAfPt) | |
| toolbar cell boxes, field boxes | DrawBox / DrawFieldBox (library gui.c:27, :60) | SHINE/SHADOW |
- All of these are calls into dopus5.library or the DirectoryOpus program, never from Intuition ROM. V (source), the draw path of each is read, not traced on a rig.
- The theme sets the source pens itself: menu_theme.c:85-89 `dopus set pens source 0 3`, `selfiles 0 3`, `seldirs 0 3`, `dest 1 2`, `gauge 3 0`; menu_theme.h:75-78: palette colour 3 = ACCENT, 0 = bg. So source bg = accent. V.
- Flat's DOpus hooks (flat.c:889 d4_lvo, :820-870, :2107-2160) patch ONLY DrawBox (LVO -696), DrawFieldBox (-702) and the internal bevel body (FLAT_EL_BEVEL, flat_core.h:128-131). They skip or redraw the outline; they do not touch image_class RectFills (flat.c header :18-20 lists the unreached ones: path field, scroller knob and arrows, sort arrow). V.

## 3. Differences that produce what the screenshot shows
1. Solid accent in title-bar gadgets, arrows, size gadget: dopusiclass fills with `pens[FILLPEN]` (image_class.c:116, :161, :300, :316, :512). Under flat FILLPEN = accent (desk_core.c:90). Not from ROM, so the `rom` gate rejects them and no sysiclass/flat glyph replaces them. Stock Intuition gadgets get the flat glyph instead. V (both ends read).
2. Info bar solid accent: lister_title_pens uses source_col bg = pen 3 = accent (section 2). V for code and theme string; that the screenshot lister is the SOURCE lister is U.
3. Thick dark dividers (title gadgets about 3 px, arrows): the images draw SHINE/SHADOW border pairs (listview_border THICK 2 px, IM_ICONIFY box lines) and under the flat map both pens are the surface (dark) colour, so each pair becomes a dark line between two accent fills. DrawBox is patched to draw nothing/flat outline, but listview_border and the image box lines draw with Move/Draw directly, unhooked. Inferred from code + pen map: U for the pixel widths.
4. Window border / title solid accent: expected for the title band (same as ordinary flat windows). The SIDES being accent too means rf_side did not run. Candidates (none measured): the lister's screen has no flat slot or pen_outline 0 (DOpus on its own cloned screen; st_scr falls back to pens 0, flat.c:317-319), or the border fill is not the ROM RectFill. U. Test: STATUS rhits and screen list while a lister is open; compare lister on Workbench screen vs `--own-screen`.
5. Hatched dither on the horizontal scroller track: the stock prop container dither (research 2026-10-29-amarchy-flat-ui-patch.md section 12: "dither fill in a pen-0 container") from Intuition propgclass, plus lister_stipple in the toolbar tail (lister_toolbar.c:254). flat's rf_scroller (flat.c:1670-1676) handles only ROM-caller fills on windows it knows, same gate. U which one the screenshot shows.
6. Toolbar icons light grey, unframed: image bank bitmaps (DT-x work, thoughts/spot/todos.md:4783-4830); they are DOpus images drawn with RenderImage on DOpus pens (todos.md DT-11 note), no flat code path. V (todos), look U.

## 4. Root-cause level
Not the pen map and not DrawBox. The invariant "an active frame fill in FILLPEN is repainted by flat" is implemented only for callers inside intuition ROM and for sysiclass images. DOpus draws its own gadget imagery through dopusiclass (dopus5.library, no source rebuild possible) and its own status/title areas in env pens. Layer to change: amarchy-flat hooks (same layer as FLAT-D4), by adding the dopusiclass dispatcher (IM_BORDER_BOX, IM_ARROW_UP/DOWN, IM_ICONIFY and the other title/size types, image_class.c switch) as a hooked class, drawn through the existing sysiclass flat glyph path (flat_glyph, clone/private-image), and a listview_border/Draw hook or a body hook like FLAT_EL_BEVEL for the 2 px borders. The source/dest title pens are a theme-side mapping in menu_theme.c:85-89 (change `source` to surface-on-accent text or to the lister background) and belong to the theme layer, not flat. Do NOT change image_class.c: the vendor library is a binary (Makefile:2140). Before coding, measure (U items 4 and 5) on one rig lister: which screen, STATUS rhits.

## Fix (s66)

Measured on rig 66 (OS 3.2 tree, `rig.py dopus` install of 5.104 as the Workbench, flat test palette
tests/data/desk/palette-flat-pen8.prefs: pen 0 13a907 surface, 3 6688bb accent, 249 6cc68a knob), worktree build of
main at the branch point (includes the FLAT_EL_BEVEL hook 1eb68dd6). Order: amarchy-flat started, then DirectoryOpus,
then `amarchy-flat DOPUS`, then a Device List and a RAM Disk lister. STATUS: `dopus=73.8 patched=7 calls=15 flat=15`,
`dopus bevel calls=2`, `rectfill calls=806 hits=33 shits=48 side=2`, `clone draws=36 fallback=0`.

Per difference, at that build:

1. Accent fills in dopusiclass images: NOT reproduced at rest. The lister title-area gadgets (down arrow, `<`, `OFF`/`SRCE`)
   and the scroll arrows draw surface fill (pen 0), accent outline, glyph in text/accent pen. image_class.c fills FILLPEN only
   for a selected (pressed) arrow, an active IM_BORDER_BOX and IM_ICONIFY; neither the size box nor an iconify gadget showed
   accent fill here. The pressed state was not measured. No dispatcher hook was added: nothing at rest to fix, and the
   owner's screenshot conditions (own screen, palette, OS) are not known. UNVERIFIED: pressed arrows, iconify gadget.
2. Info bar (theme layer, fixed): the source lister's info bar was a filled accent bar (y=12..14 of the screen, x 450..630:
   142..147 of 180 px pen 3) stacked under the active accent title band; the destination bar was fg on dim, also filled.
   Invariant: in a flat window the only filled bar is the active title band. Fix in menu_theme.c (the theme string, our
   layer): `source 3 0` (accent text on surface), `dest 2 0` (dim text on surface). Rig after the same `set pens` lines and
   `refresh lister`: 157..159 of 180 px pen 0, text pen 3. Regression test
   `dopus_lister_info_bars_are_never_filled_bars` (tests/test_keymap.c) plus the FLAT-D2 golden: with the menu_theme.c
   change reverted the keymap suite goes from 11 to 17 failures (the 6 new are these checks); the 11 others fail on the
   branch point too (toolbar tests at test_keymap.c ~4068..4206, pre-existing, not touched here).
3. Thick SHINE/SHADOW dividers: toolbar button separators are 2 px surface lines between the icon cells; not a bevel pair.
   Not changed.
4. Accent frame sides: NOT a bug on this rig. Left edge x0..1 pen 3, x2..4 pen 0; inactive lister edge 6cc68a (inactive
   edge pen); `side=2`, so the 2 px outer ring is the flat edge (rf_edge, ROM RectFill, rhits rising) exactly as on other
   flat windows. The earlier "accent sides" reading was a 2 px edge at side_px 2, or a screenshot from before 1eb68dd6.
5. Scroller: knob accent on surface with accent outline, both bars (sfill entries at x430..439 and y248..253): flat.
6. Toolbar icons: DOpus's own coloured icon bank; out of scope for the frame hook (toolbar bank work, dopustb).

Open: the owner's own screenshot conditions are needed to reproduce 1 and 3 (rig 63, own screen 1344x1080 with the theme
palette?); an eye check of the same lister on the owner's setup decides whether a dopusiclass hook is still wanted.
