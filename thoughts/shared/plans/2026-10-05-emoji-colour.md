---
date: 2026-10-05
topic: U4 -- full-colour emoji on RTG screens (Twemoji pages, drawn through cybergraphics)
tags: [plan, ledger, unicode, emoji, rtg, cybergraphics, kit]
status: implemented
---

# U4: colour emoji -- ledger

Branch `feature/emoji-colour` off main 0b26cdb. Owner 2026-10-05: emoji from U2 (607c940)
draw as monochrome Unifont bitmaps, "outlined and white" on the rig; wanted: full colour
on RTG screens.

Done = converter + pages (make target), colour glyph lookup on the U2 page cache, the
renderer draws a wide emoji cell in colour on RTG >= 15 bit with cybergraphics, kit
option, host tests (converter round trip + coverage, lookup order, blend, one
reachability test with a call-count sentinel), make test / test-ref / amiga / dist green.
Rig check: the main session's (no emulator from this branch).

## Decisions (main session, do not re-litigate)

- Source Twemoji (CC-BY 4.0), pinned release, fetched into build/, never committed.
- 16x16 RGBA per emoji plus a 16x8 variant for 8-pixel rows.
- Pages per 256 code points like Unifont's, installed next to them (SYS:UP-Term/emoji),
  optional kit part (Installer, install.dos, make dist).
- Drawing only on RTG, depth >= 15, cybergraphics.library open: indices through a
  table blended in software over the cell's effective background (inverse, selection).
  Everything else: the mono Unifont path, then the box.
- Sequences (ZWJ, skin tones, flags, VS16): base code point only; follow-up.

## Decisions made here

- Release: jdecked/twemoji v17.0.3 (2026-06-01), the GitHub tag archive
  (sha256 a0855654...9fca), its assets/72x72 PNGs. The SVGs would need a rasteriser
  (rsvg/cairo) on the build host; the 72 px PNGs are decoded by the converter itself
  (pure Python, stdlib only, deterministic) and area-averaged (premultiplied alpha) to
  16x16 and 16x8 -- the box filter is exact for 72 -> 16 and 72 -> 8, no ringing.
- Which code points: Twemoji's single-code-point files whose engine width
  (engine/vtwidth.h, parsed by the converter) is 2. Narrow ones (U+263A, U+2764 without
  VS16) never reach the colour path (it draws two-cell cells only), so they are left out.
- The 16x8 variant is stored, not derived at draw time: derived per pixel it needs
  colours outside the glyph's palette (a second, RGB drawing path or a pair table at
  each draw); stored, both sizes share one palette and one drawing call. Area-averaging
  72 -> 8 rows directly is what averaging the 16 rows in pairs gives, so the look is the
  same; the cost is ~64-128 bytes an emoji.
- Format: palette + indices per glyph, indices 4 bits when a 16-colour palette keeps
  every pixel within 48/255 of the art (blended over black and over white), else 8 bits
  with the exact colours (at most 255; index 255 is the box's background). Measured over
  all 1192 emoji, both sizes: palette + 8-bit indices lossless 1,081,844 B, RLE RGBA
  (count + 4 bytes a run) 1,334,145 B, zlib of the RGBA 725,629 B (no inflate on the
  Amiga side); 16 colours for all ~305 KB but max error 130/255 on some (a small
  feature's colour merged away). Adaptive 4/8: 481,432 B (see Sizes).
- Drawing: WriteLUTPixelArray (the call the sixel path already declares): the palette is
  blended over the background once per draw (n entries, not 256 pixels), the indices go
  out in one call. The cell box (2 x cell width by cell height) is filled in the same
  call: margins use index 255, which is the background.
- Order: the colour source stands in Unifont's place for a two-cell cell: native ->
  outline font (F1, the user's choice) -> stand-in (W33) -> colour (RTG, two cells) ->
  Unifont -> box.
- Cache: the U2 page cache (render/unifont) made generic (a page check and a slot count
  per cache: uf_init_pages, uf_page); the colour pages get their own instance, 4 slots.
- Memory: the window struct has a compile-time budget (con <= 26000 B in
  vtcon_handler.c). The cache and the box scratch (ce_store, ~3.3 KB) are one AllocVec,
  made at bind only when the screen can show colour (vr_can_colour); the renderer holds
  a pointer and the target description.
- Ledger id U4 (E1 is the parser in the master plan).
- Attribution: dist/emoji/SOURCE.txt + CC-BY-4.0.txt in the kit drawer, README, and
  one line in UP-Term > About.

## Checklist (11 of 11; rig check open, main session)

- [x] U4.1 tools/gen_emoji.py + make emoji (pages into build/emoji/pages)
- [x] U4.2 converter test (fixture PNGs -> golden pages, round trip, coverage), in make test
- [x] U4.3 render/unifont: cache generic (check fn, slot count), uf_page
- [x] U4.4 render/emoji.[ch]: page check, glyph lookup, blend, image unpack, painter
- [x] U4.5 glyphmap: colour source in Unifont's place (VT_GLYPH_COLOUR)
- [x] U4.6 host tests: format round trip in C, blend, order, reachability (call count)
- [x] U4.7 amiga_render.c: target (RTG, depth, CGX), draw_rows colour cell, vr_set_emoji
- [x] U4.8 vtwin.c: colour cache owner, loader (header, then the page) through the worker
- [x] U4.9 kit: make dist, Installer option, install.dos, SOURCE.txt + licence, README
- [x] U4.10 make test, make test-ref, make amiga, make dist
- [x] U4.11 fail-before proofs recorded; vtcon plan line; rig steps for the main session

## Sizes

- 1192 emoji in 17 pages, 481,432 bytes installed (964 at 4 bits, 228 at 8); largest
  page 1F9 108,596 B, then 1F3 96,072, 1F4 92,248, 1F6 82,868; BMP pages 23 25 26 27 2B
  30 32 together 22 KB. In the .lha the drawer is ~345 KB. `make emoji` ~35 s.
- RAM per window on a colour screen: the ce_store (~3.3 KB) and at most 4 pages
  (<= 380 KB with the four largest). Nothing on planar / 8-bit screens.

## Tests (make test; suite `emoji`, tests/test_gen_emoji.py)

- tests/test_gen_emoji.py: fixture PNGs drawn by the test (RGBA, 4-bit and 2-bit
  palette with tRNS, RGB, every row filter) packed as a release archive ->
  tests/emoji/golden; pages equal the goldens; only two-cell single code points
  (U+263A and a sequence left out); widths are the engine's; PNG reader per kind;
  header fields; exact round trip at 4 and 8 bits; antialiased art within MAX_ERR;
  deterministic.
- tests/test_emoji.c: golden_page_gives_the_converters_colours,
  bad_colour_pages_are_refused, image_centres_in_the_two_cell_box,
  colour_cache_keeps_at_most_its_slots, palette_blends_over_the_background,
  colour_page_on_rtg_of_15_bits_mono_unifont_elsewhere, text_reaches_the_colour_painter
  (the reachability test: "Grüße - ok 😀" through vt_cell_glyph -> ce_paint on a fake
  16-bit RTG target; one write_lut call, the yellow in the frame, the background
  around it; the same row on AGA: Unifont's glyph, no pixel).
- Fail-before (each reverted once, then restored):
  - colour branch in glyphmap's fallback off: 18 checks fail (order + reachability:
    write_lut calls 0, frame empty).
  - ce_target_ok at depth >= 8: 5 fail (8-bit RTG drew colour, read a page).
  - ce_blend truncating instead of rounding: 4 fail.
  - converter without the MAX_ERR fallback (always 4 bits): 2 fail (8-bit round trip,
    goldens).
  - the cache's eviction over UF_SLOTS, not nslots: 2 fail (slot cap).
- make test, make test-ref (149 streams), make amiga, make dist: green.

## Rig steps (main session; default rig must be in a 15/16/24-bit RTG mode)

1. In this worktree: `make amiga` and `make emoji` (or `make dist` and install with
   "Colour emoji" ticked). Copy build/amiga/vtcon-handler to the rig's VTC: drawer
   (`rig.py install` copies the handler), and build/emoji/pages/* to SYS:UP-Term/emoji/
   (the Unifont pages stay in SYS:UP-Term/unifont/).
2. Check the Workbench screen is RTG at 15 bits or more (Prefs/ScreenMode); on an 8-bit
   RTG or AGA screen the mono Unifont glyph is the designed result.
3. VTC:Claude against tools/claude_fixture.py, prompt `hello`: PASS = the answer line
   "Grüße - ok" followed by a coloured grinning face (yellow, brown eyes and mouth)
   over two cells. FAIL = the white outlined Unifont glyph (colour path not taken) or a
   box (no page).
4. `Type VTC:u2.txt`: its emoji line in colour; the other lines as before.
5. Optional: select the emoji (blended over the selection colour), `/font topaz 8`
   (16x8, squashed), `printf '\e[7m😀\e[m'` (over the inverse background).

## Follow-ups

- Sequences: ZWJ, skin-tone modifiers, flags (regional-indicator pairs), keycaps, and
  VS16 widening (glibc counts U+2764 U+FE0F as one cell, so ❤️ stays mono): today the
  base code point is drawn; Twemoji has 2,582 sequence files (roughly 1 MB more at this  format, an estimate from the per-emoji average, not measured) and the engine would need grapheme widths (?2027) first.
- Narrow Twemoji code points (U+263A, U+2764, U+2600...): left out; they need the
  VS16 decision above.
- Unicode 17 emoji Twemoji 17 has that the engine's 16.0 table counts as one cell are
  left out until `make widths UNICODE=17.0.0`.
- Cursor over a colour emoji: the COMPLEMENT inversion of the box (as for any wide
  cell); a block cursor colour is not blended.
- DECDWL/DECDHL rows show the box for emoji (as before).
- The GitHub tag archive's sha256 is pinned; GitHub has re-compressed archives before.
  If `make emoji` fails the check, compare the tree, then update TWEMOJI_SHA256.
