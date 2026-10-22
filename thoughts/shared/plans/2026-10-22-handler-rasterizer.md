---
date: 2026-10-22
topic: UP-Term's own TrueType rasterizer in the handler
status: approved 2026-10-22; R0-R2 done in cb09a4bd (amarchy-a), R3 next
research: thoughts/shared/research/2026-10-22_handler-rasterizer.md
branch: amarchy-a (worktree .claude/worktrees/agent-a7edb2573dbabf68d)
---

# Handler TrueType rasterizer

## Goal

The outline glyph worker (render/outline.c, process "UP-Term glyphs") draws TrueType
glyphs with UP-Term's own C89 fixed-point rasterizer instead of ttf.library. Plane 15
(Material Design Icons, U+F0000 and up) becomes reachable: ttf.library 0.8.5 ignores
OT_GlyphCode_32 (render/glyphcode.h), so today those code points fall to the
replacement box. The rasterizer produces 8-bit coverage, so a later draw path can
antialias (RTG alpha, AGA shade pens) while today's 1-bit BltTemplate path keeps working.

## Owner requirements (decided, not re-litigated)

- Own rasterizer in the handler, not ttf.library and not ttengine.library.
- 32-bit code points: cmap format 4 and format 12.
- Grey coverage output. RTG: alpha blend. AGA: shade pens through render/pen_match.h.
  1-bit when no pens are free.
- Fixed point only (68020 without FPU stays usable). C89 (RULES rule 1).
- ttf.library stays until the rasterizer lands.

## Owner answers (2026-10-22)

- Remove ttf.library after rig parity: yes (R5 in this plan).
- Antialias draw in this plan as R4: yes.
- Start R0-R2 (host only): approved. R0.1 needs a rig and moves to the start of R3.

## Measured facts (2026-10-22, fontTools 4.61.1 on build/third_party/f1/SymNerdMono.ttf)

- Symbols Nerd Font Mono 3.5.1: 2,610,012 bytes, 10,627 glyphs, unitsPerEm 2048,
  indexToLocFormat 1 (long loca, 42,512 bytes), glyf 2,329,712 bytes, cmap 8,284 bytes.
- cmap subtables: (0,3) and (3,1) format 4 with 3,728 codes; (0,4) and (3,10) format 12
  with 10,624 codes; (1,0) format 0. 6,896 of 10,624 code points are above U+FFFF;
  the highest is U+F1AF0.
- maxp: maxPoints 2,902, maxContours 132, maxCompositePoints 0, maxComponentDepth 0.
  The font has no composite glyphs. Other outline fonts a user names with the
  font-fallback key (for example JetBrains Mono Nerd Font) do have composites, so
  composites stay in scope.
- Advance widths: 0 and 2048 only (monospace, one em).
- Tables present: GDEF OS/2 PfEd cmap cvt gasp glyf head hhea hmtx loca maxp name post.
  No kern, no CFF. Hinting bytecode (cvt, glyf instructions) is ignored by design.

## Design

### Seam (unchanged)

`vo_glyph(f, cp, cells, &bpr)` (render/outline.h:38) keeps its contract: a chip-RAM
1-bit mask of ch rows by bpr bytes, bpr = ((cells*cw+15)>>4)*2 (outline.c:463), glyph
origin on the bitmap font's baseline given at vo_open / vo_set_cell. Callers in
amiga_render.c and glyphmap.c do not change in phases 1 to 3.

### New module: render/ttf.c + render/ttf.h (portable C89, no OS calls)

Host-testable like render/otag.c. Two halves in one file pair, one public header.

Reader. The font is 2.6 MB, too big to load whole on a 2 MB-chip / small-fast machine,
so the parser reads through a callback:

```c
typedef long (*ttf_read_fn)(void *ctx, unsigned long off, void *buf, long len); /* bytes read */
typedef void *(*ttf_alloc_fn)(void *ctx, unsigned long size);   /* cleared, NULL on failure */
typedef void (*ttf_free_fn)(void *ctx, void *p);

typedef struct ttf_io { ttf_read_fn read; ttf_alloc_fn alloc; ttf_free_fn free; void *ctx; } ttf_io;

typedef struct ttf_font ttf_font;   /* opaque */

enum { TTF_OK, TTF_NOT_TTF, TTF_TRUNCATED, TTF_NO_TABLE, TTF_BAD_TABLE, TTF_NO_CMAP, TTF_NO_MEMORY };

int  ttf_open(const ttf_io *io, ttf_font **out);          /* TTF_* */
void ttf_close(ttf_font *f);
unsigned ttf_glyph_index(const ttf_font *f, unsigned long cp); /* 0 = .notdef / missing */
int  ttf_advance(const ttf_font *f, unsigned gid);         /* font units */
int  ttf_units_per_em(const ttf_font *f);

/* scale: 16.16 pixels per font unit, separate x and y; ox, baseline: the
 * glyph origin in the w x h coverage grid (pixels). cov: w*h bytes, cleared by
 * the caller, 0..255 written. 1 = drawn, 0 = blank glyph, <0 = TTF_* error. */
int  ttf_render(ttf_font *f, unsigned gid, long xscale, long yscale,
                int ox, int baseline, unsigned char *cov, int w, int h);

/* coverage to today's 1-bit mask: level >= 128 sets the bit; bpr bytes per row */
void ttf_threshold(const unsigned char *cov, int w, int h, unsigned char *mask, int bpr);
```

Kept in memory per font (measured for SymNerdMono): table directory, head/hhea/maxp
fields, the loca table (42,512 bytes), the chosen cmap subtable (format 12 preferred,
format 4 when no 12; under 128 KB worst case for format 12 per the spec's group size,
8 KB measured), and hmtx (10,627 * 4 bytes at most, 42 KB). glyf is read per glyph
through the callback. Everything is bounds-checked against the table length; a bad
offset answers TTF_BAD_TABLE, never a read past a buffer.

Glyph load: simple glyphs (flags with repeat, x/y short and same bits), composite
glyphs (ARG_1_AND_2_ARE_WORDS, ARGS_ARE_XY_VALUES, WE_HAVE_A_SCALE, X_AND_Y_SCALE,
TWO_BY_TWO, MORE_COMPONENTS; point-matching anchors answer TTF_BAD_TABLE and the glyph
is skipped), recursion depth limit 4 against loops. Instructions are skipped.

Rasterizer: signed-area coverage accumulation (the font-rs / stb_truetype v2 method):
1. Transform points to pixel space in 24.8 fixed point with the 16.16 scales; y flips.
2. Flatten quadratic segments by subdivision until the control point is within
   1/4 pixel of the chord (integer test on squared 24.8 distances), recursion cap 8.
3. For each line, add signed area and cover to an accumulator of (w+1)*h longs.
4. Prefix-sum each row; coverage = min(255, |acc| >> shift). Non-zero winding.
All arithmetic on 32-bit longs; products use a 32x32->32 helper that keeps 24.8 range
(pixel coordinates stay below 2^15 for any cell under 128 px, checked at entry).
Scratch (points, flags, accumulator) is allocated once per font through io->alloc,
sized from maxp (maxPoints 2,902: points 2,902*8 + flags 2,902 + contour ends 132*2 =
26 KB, plus the accumulator), never on the worker's 12,288-byte stack.

### Worker integration (render/outline.c)

`engine_open` (outline.c:198) reads the .otag (already parsed by render/otag.c). When
the font file the .otag names is TrueType (first 4 bytes 0x00010000 or 'true'), the
worker opens it with dos.library (Open/Seek/Read, worker process only, never the
handler: research section on DOS calls) and calls ttf_open with a ttf_io whose read
seeks and reads, and whose alloc is AllocVec(MEMF_ANY|MEMF_CLEAR).

- `calibrate` (157): same contract as today. yscale = ch pixels per em
  (OT_PointHeight ch at 72 dpi equals ch / unitsPerEm). xscale chosen so the advance of
  the first probe present ('M', 0xE0A0, 0x2588, '0') equals cw, the same rule as the
  xdpi computation at outline.c:181. No vt_gc_probe32: our engine always takes 32-bit codes.
- `render` (117) / `draw_current` (125): ttf_glyph_index, ttf_render into a fast-RAM
  coverage buffer of cells*cw by ch, ttf_threshold into the existing chip mask.
- Cache (outline.c:455-500) unchanged in phases 1 to 3. Phase 4 keeps the coverage
  beside the mask.
- When the font file is not TrueType or ttf_open fails, the worker falls back to the
  bullet engine path that exists today (until phase 5 removes it).

Open fact, resolved in R0 before any code: which .otag tag carries the .ttf path that
ttfinstall writes (research doc: unverified). R0 reads a real .otag from the rig kit
(written by ttfinstall in tools/rig/outline_rig.py) and dumps its tags with render/otag.c
on the host.

### Sentinel for reachability

The worker counts glyphs drawn by ttf_render. The count is shown by `/state` as a new
key `outline-own` (slash.h STATE keys; slash_state_text). The rig reads it through the
same /state path the tutorial rig uses.

### Antialias (phase 4)

- Cache entry keeps the 8-bit coverage (fast RAM) and the 1-bit chip mask.
- RTG: WritePixelArrayAlpha on the window's RastPort (P96 support unverified: R4.0
  probes it on the rig before any draw code).
- AGA: 3 shade levels per (fg, bg) pair, pens from pm_mix(bg, fg, level) matched with
  pm_match (render/pen_match.h); coverage quantised to 4 levels and drawn as up to 3
  BltTemplate passes (one per shade pen) plus the fg pass. No free pens: 1-bit path.
- Setting: profile key `outline-antialias` on | off, default on.

### Removal of ttf.library (phase 5)

After rig parity (R3.x all pass on rig 060 and the A1200 check), the bullet path,
vt_gc_probe32 and the ttf.library fetch in tools/rig/outline_rig.py go. The kit ships
no ttf.library. Owner question 1 decides whether phase 5 is in this plan.

## Checklist (IDs; ticked in the open during implementation)

### Implementation notes (R5, 2026-10-23)

- R4.5 (aa05fd97): rig 2, 060, RTG and AGA 256 both 9 of 9. Intermediate colours per icon
  cell with antialias on: RTG [18,7,31,47,11,41,35,35,8,36], AGA [3,1,3,3,2,3,3,3,1,3];
  off: all 0. AGA had free shade pens, so the 1-bit fallback did not run. The outline-own
  read had always returned None (UPTerm not on the rig path); run_line now copies
  build/amiga/UPTerm to VTC: and runs VTC:UPTerm.
- R5.1 (cba97b92): outline.c is own-engine only. engine_open reads FONTS:<name>.otag and
  uses its OT_Spec1 TrueType file, else opens FONTS:<name>.ttf directly, so a bare .ttf
  needs no ttfinstall. render/glyphcode.h and its test are gone. Design choice: an .otag
  for a non-TrueType engine (Intellifont) is no longer drawn.
- R5.2 (7da0b858): `make symbolsnerd` fetches NerdFontsSymbolsOnly 3.5.1, archive and ttf
  SHA-256 pinned. The fonts part installs FONTS:SymbolsNerdFontMono.ttf with a marker in
  ENVARC:up-term, licences to UP-Term:fonts/SymbolsNerdFont. Licence is MIT (archive
  LICENSE); OFL applies only to some icon sets inside, not to the font as a whole.
  Full `make dist` / dist-check not run; installer, addons and fonts dist tests pass.

Progress (R5.2, 7da0b858): 29 of 30 ticked. Open: R5.3 (rig pass, A1200 manual check).
Count correction: the checklist has 30 items (R0 3, R1 7, R2 4, R3 7, R4 6, R5 3),
counted from the checklist rows; the earlier "of 31" lines were a miscount. R3.3 (bullet
fallback) was superseded by R5.1.
- R5.3 rig part DONE (2026-10-23, handler 7da0b858 build 388940 bytes, rig 2, 060): RTG and
  AGA 256 9 of 9 each; bare .ttf in FONTS: (no .otag) draws, outline-own 10, outline-aa
  0/10/0, shade counts as R4.5. R5.3 stays open for the A1200 manual check only.
- R5.3 A1200 PASS (owner, 2026-10-23): ten icons draw from FONTS:SymbolsNerdFontMono, none a box. Progress: 30 of 30. Follow-ups R6.1 (pixel aspect) and R6.2 (two-cell icons) are in thoughts/spot/todos.md.

### Implementation notes (R4, 2026-10-23)

- R4.0/R4.2 deviation: no WritePixelArrayAlpha. The RTG path reuses vr_cgx_write_lut
  (the emoji and sixel path, proven on the rig): coverage bytes are LUT indices into a
  256-entry ULONG table of fg blended over bg (ce_mix, sRGB per channel), kept for the
  last (fg, bg) pair.
  Same visible result; P96 alpha support no longer matters. R4.0 is ticked as "probe
  replaced by the proven LUT path" (owner may veto).
- R4.3: "4 levels" read as 3 shade pens (1/4, 1/2, 3/4 of bg to fg) plus fg: four
  BltTemplate passes at coverage >= 32, 96, 160, 224. A shade pen is used only when its
  colour is nearer the wanted shade than fg and bg are; no usable shade: 1-bit path.
- R4.1: the worker fills a fast-RAM coverage buffer per slot on the own engine's path;
  the library path has none (1-bit as before).
- Shade targets and the LUT both come from ce_mix (render/emoji.c), which replaced
  ce_ramp: ce_ramp filled a vt_u32 table, 8 bytes per entry on the host, while
  vr_cgx_write_lut reads 32-bit ULONG entries. emoji_lut (amiga_render.c) still passes
  a vt_u32 table: host-only mismatch, harmless on the Amiga, left as found.
- Off switch: vr_set_antialias(r, 0) (profile key outline-antialias, R4.4); sentinel
  n_aa (/state outline-aa) counts glyphs drawn antialiased.

### Implementation notes (R3, 2026-10-22)

- own_open: an OT_Spec1 path with no ':' is looked up in FONTS:. A file that is not
  TrueType (first 4 bytes 0x00010000 or 'true') or fails ttf_open keeps the ttf.library
  path, which is not opened when the own engine is in use.
- Scale: y = cell height / units-per-em; x = cell width / advance of the first probe
  glyph whose advance is above upem/16.
- /state outline-own = vo_own(window's outline font); 0 on the library path.
- tests/test_tutorial_wiring.py p1 and p3 already failed at cb09a4bd (the p3 regex predates
  the font_size line in SLASH_STATE); not touched here.

### Implementation notes (R0-R2, 2026-10-22)

Progress (R4, 48d721dc): 26 of 31 ticked. Open: R4.5 (rig check), R5.1-R5.3.
Earlier (R3, 4d79a51d): 21 of 31.
Earlier: 12 of 31 ticked. Open: R0.1 (needs a rig; moved to the start of R3),
R1.7 (render/ttf.c is in RENDER and the test list; HANDLER_SRC and the amiga object
lists are wired in R3 with the worker), R3.x, R4.x, R5.x.

Deviations from the text below, decided during implementation:
- Names: the subset ships as tests/data/symnerd_subset.h (a C array, no binary file),
  the script is tools/fonts/make_ttf_testdata.py (`make ttf-testdata`), the goldens are
  tests/data/ttf_golden.h. The pin is the SHA-256 of SymNerdMono.ttf itself.
- 11 code points, not 12: U+2588 is not in SymNerdMono.ttf 3.5.1 (checked in its cmap).
  22 goldens (11 x 8x16 baseline 12, 11 x 16x32 baseline 25), FreeType 2.14.3, no hinting.
- Fixed point is 26.6 (64 subpixels), not 24.8: 24.8 products overflow 32 bits.
- Curve flattening uses FreeType's gray-rasterizer flatness (FLAT 16, 4 chords on a
  small arc). A tiny curve loses about 2.5% area against the true curve, as in FreeType;
  the off-curve square test asserts the flattened area (3315) within 1%, measured 3314.
- Measured against the goldens: worst per-pixel difference 12 of 255 (tolerance 24),
  worst 1-bit mismatch 1.1% of a glyph (tolerance 2%), 14 mismatched bits over all 22.
- Composites with point-matching components answer TTF_BAD_TABLE (the font has no
  composites at all; the synthetic tests cover offset and scale).
- R2.4: tests/bench_ttf.c, `make bench-ttf`: 10,627 glyphs x 2 cells, 21,248 drawn,
  6 blank, 0 errors, 0.09 s on the host.


Phase R0: facts
- [x] R0.1 Dump the .otag that ttfinstall writes on the rig; record the path tag in the research doc.
      Done on the host instead: ttfinstall 0.8.5 under vamos (`-C 68020 -m 8000`). OT_Spec1
      (0x80009101, indirect) holds the .ttf path as given, lower-cased. Test: test_otag.c `real[]`.
- [x] R0.2 Build tests/data/symnerd-subset.ttf with the fontTools subsetter: 12 glyphs
      (U+E0A0, U+E615, U+EA74, U+EBCF, U+F011, U+F021, U+F003B, U+F09D1, U+F0249,
      U+F14DE, U+F0B4C, U+2588) plus OFL 1.1 licence text beside it. Script in
      tools/fonts/make_subset.py, pinned to the 3.5.1 SHA-256.
- [x] R0.3 Oracle: tools/fonts/ft_oracle.c built against Homebrew FreeType 2.14.3
      (FT_LOAD_NO_HINTING, FT_RENDER_MODE_NORMAL) writes golden coverage for the 12
      glyphs at 8x16 and 16x32 cells to tests/data/symnerd-golden.h. Goldens committed;
      FreeType is not a test-time dependency.

Phase R1: parser (render/ttf.c)
- [x] R1.1 ttf_open: table directory, head, maxp, hhea, hmtx, loca (short and long), cmap pick.
- [x] R1.2 cmap format 4 and format 12 lookup; format 12 preferred.
- [x] R1.3 Simple glyph load.
- [x] R1.4 Composite glyph load with depth limit 4.
- [x] R1.5 Bounds checks: truncated file at every table boundary answers an error, no crash.
- [x] R1.6 tests/test_ttf.c: synthetic in-memory fonts (square, triangle, one quadratic,
      composite of two squares with offset and scale, format 4 only, format 12 only,
      truncated copies); subset font lookups for all 12 code points.
- [x] R1.7 Makefile: render/ttf.c in HANDLER_SRC (line 468), amiga object lists
      (867-900, 1031-1064), test list (line 46). Done in ae6ab1f1, handler and console.device.
      outline.c builds with -dontwarn=153,65 (the (void)ctx callbacks), as the proc tools do.

Phase R2: rasterizer
- [x] R2.1 Transform, flatten, accumulate, prefix-sum to 0..255 coverage.
- [x] R2.2 ttf_threshold to bpr-packed 1-bit mask.
- [x] R2.3 Tests: a full-cell square gives all 255; a half-cell rectangle gives 255 and 0
      columns with one 128 edge; a triangle's area matches within 1%; the 12 subset
      glyphs match the FreeType goldens (max per-pixel difference 24 of 255, 1-bit
      mismatch at most 2% of pixels; thresholds are first estimates, set from the
      measured spread in R2.3 and recorded here).
- [x] R2.4 Host benchmark: tests/bench_ttf.c renders all 10,627 glyphs of the real font
      (when present) and fails on any error return. Not part of `make test`; run once
      per change of the rasterizer.

Phase R3: worker integration and reachability
- [x] R3.1 engine_open: TrueType detection, ttf_io over dos.library in the worker.
- [x] R3.2 calibrate with the probe rule; render / draw_current through ttf_render + ttf_threshold.
- [x] R3.3 Fallback to the bullet path when the file is not TrueType or ttf_open fails.
- [x] R3.4 `outline-own` /state key (slash.c, slash.h, tests/test_slash.c).
- [x] R3.5 Rig: tools/rig/outline_rig.py prints U+F003B and U+E615 in a window on rig 060
      (`rig.py start --max`), asserts the cells differ from the replacement box and from
      blank, asserts `/state outline-own` grew, and flips OUTLINE_UNREACHED to expect a glyph.
      Done in 4d79a51d (OUTLINE_UNREACHED became OUTLINE_OWN): rig 4, 68060, passed 6 of 6;
      outline-own 0 without the fallback, 10 with it (8 icons + 2 own codes).
- [x] R3.6 Rig timing: worker time for 100 distinct plane-15 glyphs on rig 060 and on the
      rig at 68020 speed without FPU, logged by the rig script and recorded here.
      U+F0001..U+F0064 as 4 lines of 25 with `Type`, outline-own 100 each run; time from
      Return to the last screen change. 68060: 0.1 to 0.2 s (3 runs). `rig.py --a1200`
      (stock 68020, FPU absence not checked separately): 2.8 s (2 runs, +-0.9 s, the
      screenshot poll is slow there). Deviation: measured by a one-off script, not
      outline_rig.py; the number is in this file only.
- [x] R3.7 `make UPTERM_ROOT=/Users/spot/Code/UP-Term amiga` builds; handler size before
      and after recorded here. 378580 -> 387816 bytes (+9236; "before" is the build from cb09a4bd).

Phase R4: antialias draw
- [x] R4.0 Probe WritePixelArrayAlpha on the rig's P96 screen (one call, result logged).
- [x] R4.1 Cache keeps coverage beside the mask.
- [x] R4.2 RTG alpha draw path.
- [x] R4.3 AGA shade-pen path (pm_mix, pm_match), 1-bit when no pens.
- [x] R4.4 `outline-antialias` profile key, wired through the profile loader.
- [x] R4.5 Rig: RTG and AGA screens show intermediate pixel colours on a glyph edge
      (pixel read through the rig's shot); off gives only fg and bg.

Phase R5: ttf.library removal (if owner question 1 = yes)
- [x] R5.1 Remove the bullet path, vt_gc_probe32 use in outline.c, the ttf.library fetch.
- [x] R5.2 Docs and kit: no ttf.library; the Symbols font kit step (ledger open item) ships the font and licences.
- [x] R5.3 Full outline rig pass on rig 060 and the A1200 manual check.

Total: 31 items.

## Verification

Automated:
- `make test ONLY=ttf` and `make test ONLY=slash` (capped workers, nice 19; one run on the machine at a time).
- `make UPTERM_ROOT=/Users/spot/Code/UP-Term amiga`.
- tools/rig/outline_rig.py on rig 060 (R3.5, R3.6, R4.5).

Manual (owner):
- A1200: copy L:vtcon-handler, set the font-fallback key to SymbolsNerdFontMono, run
  amarchy-menu; PASS = the ten root menu icons (five of them plane 15) draw, none is a box.
- R4: antialiased edges look smooth on the A1200 AGA screen; text stays readable.

## Cost estimate

- Code: render/ttf.c about 800 lines (parser 450, rasterizer 350), outline.c changes
  about 150, tests about 400, tools (subset, oracle) about 150, rig additions about 100.
  Handler growth estimated 10 to 14 KB (not measured; R3.7 measures it).
- Sessions: R0 to R2 one session (host only, no rig). R3 one session (rig time).
  R4 one session. R5 half a session. About 3.5 sessions.
- Agents: R0.2/R0.3 tooling and R1.7 Makefile plumbing go to sonnet; the rasterizer core
  (R2.1) and integration (R3.1 to R3.3) stay in the main session; test runs to haiku.
  At most 2 agents at once.
- Risk: fixed-point precision on large cells (checked at entry, cell under 128 px);
  speed on 68020 without FPU (measured in R3.6 before R4 starts); P96 alpha support
  (R4.0 probe decides the RTG path).
