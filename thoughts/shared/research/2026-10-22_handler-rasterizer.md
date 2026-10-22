---
date: 2026-10-22
topic: Built-in TrueType rasterizer in the handler (seams in render/outline.c)
status: research
---
Paths relative to the worktree .claude/worktrees/agent-a7edb2573dbabf68d. Documentary; "unverified" marks what was not read.

## 1. Engine API today (render/outline.c, outline.h)
- Public API render/outline.h:29-54: vo_open(name,cw,ch,base), vo_close, vo_set_cell, vo_glyph(f,cp,cells,&bpr), vo_mark (combining), vo_name, vo_has_engine, vo_read(path,buf,max) (worker does DOS reads for Unifont pages).
- Returned bitmap: 1-bit mask, ch rows x bpr bytes (bpr = ((cells*cw+15)>>4)*2, outline.c:463), set bit = ink, no per-glyph metrics returned; baseline is the bitmap font's `base`, passed in at open/set_cell. Mask is AllocVec(MEMF_CHIP|MEMF_CLEAR) per glyph (outline.c:483).
- Cache: open-addressing hash, VO_SLOTS 1024, VO_FILL 768, then whole cache cleared (outline.c:24-26, 377, 455-500); states EMPTY/HAVE/MISSING; missing is remembered. Key (cp, cells|mark).
- Worker: process "UP-Term glyphs", NP_StackSize 12288 (outline.c:402), message ops VO_OPEN/CELL/GLYPH/CLOSE/READ (outline.c:27, worker 290-366); handler may not call DOS (comment outline.h:7-10), so everything file-related lives in worker. Caller blocks in ask(f) (outline.c:368). Worker state `struct vo_engine` on its stack (outline.c:67-77) includes notdef/probe buffers VO_MAXH*VO_MAXW/8 (64x64 bit max, outline.c:24-25).
- Engine glue: engine_open (198-260) reads FONTS:<name>.otag, OpenLibrary(<OT_Engine>.library), OpenEngine, SetInfoA(OT_OTagPath/OT_OTagList); render() 117 = vt_gc_select + draw_current; draw_current 125-155 copies OT_GlyphMap 1-bit glm_BitMap into mask using glm_BlackLeft/Top/Width/Height, X0/Y0, shifted by base and ox; same_as_notdef 278 filters replacement boxes.
- Calibrate: outline.c:157-196: OT_DeviceDPI 72x72, OT_PointHeight = ch<<16, probes 'M',0xE0A0,0x2588,'0' for advance, xdpi = 72*cw*65536/(ch*glm_Width) so advance = one cell (monospace assumption), renders U+0001 as notdef, then vt_gc_probe32.
- Cell size: renderer vr_render cw/ch/base from TextFont (tf_XSize/YSize/Baseline) amiga_render.c:870-877, 887-892; vtwin.c:329 passes them to vo_open.
- Draw: amiga_render.c draw_outline ~2372-2384: fill bg, ink_a(fg), JAM1, BltTemplate(mask,...); bold = second blit at px+1. Cursor cell uses BltTemplate at 4052-4055. Marks 2356-2365.
- Seam for a second engine: (a) inside the worker: engine_open/render/draw_current/calibrate (the `struct vo_engine`); a built-in engine replaces BulletBase/ge with an own struct and fills `mask` identically; (b) alternatively bypass the worker for glyph work (pure CPU, no DOS) but file reading still needs the worker (vo_read-like). Public API vo_glyph is the stable seam; grey output would need a new accessor (e.g. vo_glyph_grey) since vo_glyph returns 1-bit only (render side: BltTemplate needs 1-bit; grey needs per-shade pen blit, see 4).
- glyphcode.h (60 lines): vt_gc_probe32/vt_gc_select; engines w/o 32-bit codes yield missing for cp>0xFFFF (glyphcode.h:44-60). Own rasterizer would take 32-bit cp natively (codes32=1), no probe needed.

## 2. render/glyphmap.c fallback order
- fallback() glyphmap.c:~177-203: cp<0x80 or native-in-font (map() Q_NATIVE) -> none; then f->outline(...) FIRST (192); then colour emoji (2-cell, 197); then unifont (200); else caller uses vt_map_glyph (stand-in or MISSING box).
- PUA/plane-15: map() returns Q_NONE and mk(VT_GLYPH_MISSING,1) for cp>0xFFFF (glyphmap.c:110-111), approx_find for BMP; neither is Q_NATIVE so fallback() reaches the outline callback with the raw cp. vt_cell_glyph 207-225 gets cp[0] for cluster cells via vt_cell_text/vt_compose_cell. Hook set at amiga_render.c:2342-2343 (r->fb.outline = outline_src when r->outline), outline_src at 2325-2331.

## 3. TTF location and file reading
- Only via .otag: engine_open builds "FONTS:<name>.otag" (strips .font/.otag) outline.c:198-215; name from profile key font-fallback (handler/vtcon_handler.c:1245-1246, copy into c->w.fallback), slash command font-fallback NAME|none (handler/slash.c:89; handled vtcon_handler.c:4204-4209, error text mentions FONTS:<name>.otag, LIBS:ttf.library). Prefs field Font > Fallback font (plan F1.6). vtwin.c:318-333 vtwin_set_outline opens vo_open when name changes.
- The .ttf path lives inside the .otag (OT_OTagPath/indirect tags; the .ttf itself is opened by ttf.library). Parsing the .otag for the TTF path: render/otag.c (41 lines, otag_check only validates; tags walked in outline.c:240-246). UNVERIFIED: which tag holds the .ttf path (diskfonttag.h OT_... file name tag not read here).
- File read: .otag is read WHOLE (Open, ExamineFH size, <=65536, AllocVec MEMF_ANY, Read) outline.c:216-230. No seek-based reader exists; TTF read would be new (worker context, DOS allowed there). Symbols Nerd Font Mono is large (UNVERIFIED size; build/third_party/f1/SymNerdMono.ttf is on disk, check with ls) so whole-file load vs Seek table loads is a design decision.
- Allocation conventions: AllocVec with MEMF_PUBLIC|MEMF_CLEAR for state, MEMF_CHIP for masks; no pools in outline.c (grep AllocVec only; render/unifont.c and emoji.c grep for Alloc returned nothing - UNVERIFIED how they allocate).

## 4. Pen matching (render/pen_match.h)
- Host-testable header-only (tests/test_render.c); sole user amiga_render.c few_pick (pm_match call at ~321, picks table ~267-282).
- API: pm_palette_set(p, pal[], n) pen_match.h:171 (0xRRGGBB, PM_MAXPENS); pm_match(p, usable_mask, target_rgb, dither, &pick) :203 -> pm_pick{solid, a, b, lev(0..64)}; pm_mix(a,b,lev) :149 mixes in linear light (pm_lin sRGB->4095 table, pm_srgb inverse); Bayer 8x8 pm_pattern(lev,y) :62. For AGA shade pens: for each coverage level, target = pm_mix(bg,fg,lev) -> pm_match gives nearest pen; precompute N shade pens per (fg,bg) pair. UNVERIFIED: how pens are obtained/released (few_obtain in amiga_render.c).

## 5. Host tests
- tests/test_glyphcode.c: fake engine struct (set/draw callbacks); includes harness.h and render/glyphcode.h only. tests/test_otag.c: builds .otag bytes in memory, includes render/otag.h. Neither includes outline.c (needs AmigaOS headers). Test list in Makefile:46; no outline.c host build exists.
- A host test loading a real TTF: new rasterizer must be a pure C file (no exec/dos) taking a byte buffer, then fopen/fread in the test. TTF available: build/third_party/f1/SymNerdMono.ttf (extracted from NerdFontsSymbolsOnly.zip, SymbolsNerdFontMono-Regular.ttf by tools/rig/outline_rig.py:54-70, URL outline_rig.py:32; ttf.library from aminet ttflib68020.lha :31). No .ttf is tracked in git (find showed only build/ copy); Makefile has no .ttf fetch (grep empty). A test must skip or fetch when absent.

## 6. tools/rig/outline_rig.py
- Fetches ttf.library+ttfinstall+Nerd font, runs ttfinstall into RAM:f1fonts, sets font-fallback, draws icon line and compares screen cells (diff_cells :79). Checks (4 per plan): ttfinstall made an .otag; icons drawn from outline font and nothing else on the line moves; CJK U+4E2D keeps '?'; em dash keeps '-'; plus for each UNREACHED code the replacement box stays (rig line ~143: "keeps its replacement box (no 32-bit codes)").
- Env: OUTLINE_ICONS=hex,hex overrides ICONS (default E0A0 F07B F121 E7A8 F015 E712 F09B F1D3, outline_rig.py:34-40); OUTLINE_UNREACHED=hex,hex = codes the font has but the engine can't reach (e.g. F003B,F0249). With an own 32-bit rasterizer UNREACHED would flip to reachable: that check must change.

## 7. Plan thoughts/shared/plans/2026-10-03-outline-fonts.md (87 lines)
- Phases = checklist F1.1 otag, F1.2 vt_glyph_native, F1.3 outline worker/cache, F1.4 renderer draws, F1.5 vtwin+handler profile key, F1.6 Prefs field, F1.7 rig check, F1.8 notes (lines 52-70). Result 8/8 line 72+. Design at 15+: worker process, one font per window.
- Built-in rasterizer: not mentioned (grep rasteri/built-in empty). Antialias: only the earlier thoughts/shared/plans/2026-09-28-vtcon.md:752 notes OT_GlyphMap8Bit (anti-aliased bitmap) and "the 8-bit map quantised to the pens on 2-8 planes". Not done today (outline.c uses 1-bit OT_GlyphMap only). Also 2026-10-05-emoji-colour.md:98 mentions antialiased art in the colour-emoji path (separate).
- Plan result notes: at topaz 8 icons ~6 px tall; double-width, cursor over outline glyph, bold, 68020 time per glyph not covered/measured.

## 8. Size and fixed-point
- Built handler build/amiga/vtcon-handler = 378452 bytes (ls, 2026-10-22 build). No budget found for the handler: RULES.md:29 only gives Amarchy 32 KB resident target (make amarchy-size, Makefile:614); Makefile:548-551 notes -O=1 -size for vsh. dist kit size test tests/test_dist_kitsize.py (Makefile:1279) UNVERIFIED whether it covers the handler.
- Fixed-point: no shared helper or 16.16 library found. Shifts by 16 appear in outline.c (OT_PointHeight 16.16, dpi), render/emoji.c, painter.c, amiga_render.c, vtwin.c, glyphmap.c, otag.c, engine/vtengine_68k.s, glyph_tables.inc (grep "<< 16|>> 16"; each use UNVERIFIED as fixed-point vs bit packing). pen_match.h has pm_isqrt (:102) integer sqrt and linear-light tables, reusable for coverage gamma.
- Sources added in Makefile: HANDLER_SRC line 468 (outline.c, otag.c), HANDLER_HDR 469, devobj 867-900 and obj 1031-1064 per-file compile lines: a new render/ttfraster.c needs entries in all three places plus tests list line 46.
