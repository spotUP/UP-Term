---
date: 2026-10-19
topic: kitty graphics protocol (APC G) in vtcon
status: K1-K18 done; K19-K35 planned; K36 done (c63dcb4f, rig proof 6e0e2fe0); K37-K41 truecolor done (a14fe089, ceacf685, eaa21b90, b0a0a42a, cbd9e471; s52 verified)
source: ~/Code/creep-cterm (github.com/creep-ltx/cterm 51a9ba3, MIT, Tobias Karlsson) src/vt.c 1425-1610, 2191-2480; src/inflate.c; src/pics.c
research: thoughts/shared/research/2026-10-19_cterm-draw-and-vt-techniques.md
---

# Kitty graphics in vtcon

Owner asked 2026-10-19: "add kitty graphics".

## What exists

- vtcon already keeps and draws pictures: DEC sixel (engine/vtengine.h:329-352).
  An image is indexed pixels + a palette of up to 256 0xRRGGBB entries, placed
  on cells (VT_CELL_IMAGE), scrolled into scrollback, reflowed, and drawn by
  render/amiga_render.c through vt_row_image(). Memory: VT_IMAGE_MEMORY 2 MB
  shared by all images; max 1024x1024.
- APC strings are parsed and dropped (engine/vtengine.c:51, 200: S_STRING,
  str_kind '_').

So kitty needs: parse APC G, assemble the pixels, turn them into the same
indexed image the sixel path makes, and place it. The renderer needs nothing
new for placement at the cursor.

## Design (decisions)

- D1 Engine: APC strings starting with 'G' go to a new kitty module
  (engine/kitty.c), not dropped. Keys parsed as in CTerm kitty_keys.
- D2 Supported: a=t (transmit), a=T (transmit+display), a=p (display by id),
  a=d (delete: all, by id, by placement; d=a/A/i/I at minimum), a=q (query).
  f=24 (RGB), f=32 (RGBA), f=100 (PNG). t=d (direct, base64) only; t=f/t/s
  (file, temp file, shared memory) answered with an error reply. o=z (zlib) via inflate. m=1 chunking. i= / I= / p= ids,
  q=1/2 reply suppression, c= / r= (cell size), C=1 (cursor does not move),
  x/y/w/h source rectangle. z= layer: below-text (z<0) not supported, drawn
  on top; documented.
- D3 Replies exactly as kitty: `ESC _ G i=<id>;OK ESC \` and the error forms
  (ENOENT, EINVAL, ENOMEM, EBADF...), sent through the engine's existing
  reply path (the one DA/DECRQSS use).
- D4 Colour: the engine converts RGB(A) to an indexed image of at most 256
  colours. Alpha < 50% becomes index 0 (unset, background), matching the
  sixel convention. Quantiser: fixed 6x6x6 cube + 40 greys (fast, no
  per-image palette search, works on 8-bit screens) with ordered 4x4
  dither, as CTerm pics.c does. The renderer's existing pen mapping
  (vr_images) then maps to screen pens.
- D5 PNG: the engine does not decode PNG. A host hook
  `int (*vt_decode)(const vt_u8 *, long, vt_u32 **argb, int *w, int *h)` +
  free, set by the handler. On the Amiga the handler decodes through
  picture.datatype V43 (ARGB), like CTerm pics.c, loaded on first use. Host
  tests set a small test decoder. Hook NULL: f=100 replies an error.
- D6 inflate: port CTerm src/inflate.c (229 lines, MIT; keep the notice)
  into engine/inflate.c, used for o=z. No zlib dependency.
- D7 Memory: stored-but-not-placed images count against VT_IMAGE_MEMORY.
  Port CTerm pic_room / pic_fits / pic_gc logic: drop oldest unplaced
  images first; a transmit that cannot fit replies ENOMEM. Chunk buffer
  capped (e.g. 4 MB base64 -> reject with EFBIG/ENOMEM).
- D8 Detection: programs probe with a=q. Answering OK is the whole
  detection; nothing in terminfo/DA changes.
- D9 Licence: CTerm code ported keeps "Copyright (c) 2026 Tobias Karlsson,
  MIT" in the file header and in THIRD-PARTY / LICENSE notices.

## Checklist

- [x] K1 engine/kitty.c: APC G dispatch from S_STRING '_' end; key parser.
- [x] K2 chunk assembly (m=1), base64 decode, size caps.
- [x] K3 f=24/f=32 to indexed image (D4), placed at cursor with c/r/x/y/w/h,
      cursor move rules (C=1).
- [x] K4 engine/inflate.c port (D6) + o=z.
- [x] K5 vt_decode hook (D5) + f=100 path; error when NULL. (engine hook is vt_set_image_decoder; host test stand-in only, the Amiga decoder is K9)
- [x] K6 a=t store by id, a=p place by id, a=d delete variants, a=q query.
- [x] K7 replies and q=1/2 suppression (D3).
- [x] K8 memory budget + gc (D7).
- [x] K9 handler: datatypes PNG decoder behind vt_decode (handler side,
      opened lazily, closed at exit).
- [x] K10 host tests (tests/test_kitty.c, wired into `make test`, ONLY=kitty):
      raw RGB, RGBA alpha->unset, chunked, zlib, PNG via test decoder,
      query reply, delete by id, ENOENT, ENOMEM on budget, unsupported t=f
      error, image visible through vt_row_image().
- [x] K11 render host test (make test ONLY=render): a kitty image draws its
      pixels on the fake canvas (proves renderer wiring).
- [x] K12 reachability on rig: handler fed a real kitty PNG stream (small
      file, e.g. `kitty +kitten icat` capture or a python generator) in an
      XCON window; sentinel: engine counter kitty_images_placed > 0 and
      datatypes decode count > 0 in the PROF/debug output; screenshot checked
      by a haiku subagent.
      Done 2026-10-19, rig 3 --max --os 322, PROF build: the Adam7 stream
      goes to the datatypes, `PROF kitty decodes/placed 00000001 00000002`
      (cumulative with the plain stream), stripes visible top-left. Needed
      the png_worker fix: DoMethodA straight to the class; DoDTMethodA
      never returned on OS 3.2.2.
- [x] K13 docs: README / man page feature list; THIRD-PARTY notice.
- [x] K14 vt_unhandled no longer reports APC G.
- [x] K15 engine/png.c (owner decision 2026-10-19: "both"): own PNG decoder
      in the engine, #included by kitty.c like inflate.c, built on
      vt_zinflate. Reason: the owner's system disk has no active
      picture.datatype, so the datatypes path (K9) fails there. Supports
      non-interlaced bit depth 1/2/4/8 for colour types 0, 2, 3, 4 and 6,
      plus 16-bit (keep the high byte); PLTE and tRNS; all five filters;
      several IDAT chunks joined. CRC not checked. Output: VT_MALLOC'd
      0xAARRGGBB words.
      Return codes: 1 decoded, 0 broken, -1 not supported (interlaced).
- [x] K16 kit_load f=100: own decoder first; on -1 (not supported) only,
      fall back to t->decode (the datatypes hook). On 0 (broken), reply
      EBADPNG without trying datatypes. Own buffer freed through s->own.
- [x] K17 host tests (tests/test_kitty.c): real PNG bytes for colour types
      0/2/3/4/6, depths 1/8/16, all filters, split IDAT, tRNS; interlaced
      falls back to the test decoder (call count proves it); a broken
      stream does not reach the decoder.
- [x] K18 rig: the K12 stream on sys.hdf (no picture.datatype) places the
      picture; PROF line counts own decodes and datatypes decodes apart.
      Done 2026-10-19, same rig 3 run: the plain stream decodes in the
      engine, `PROF kitty own png decodes 00000001 00000000` (own, then
      datatypes-after-own = 0), placed. Run on --os 322 rather than sys.hdf:
      the counter shows the datatypes were not used for that stream.

Not in scope: z<0 below-text layer, unicode placeholders (U=1), animation
(a=f/a=a/a=c), t=f/t=t/t=s file transfer. Each replies an error.
Superseded 2026-11-01: the owner wants the whole protocol, animation included; see K19+.

## K19+ protocol completion (s48)

Owner decision 2026-11-01: finish the protocol, "everything including
animation". Spec: https://sw.kovidgoyal.net/kitty/graphics-protocol/ (read
2026-11-01). Plan only; no code yet.

### The gap, from the code (engine/kitty.c, read 2026-11-01)

Read from kit_parse, kitty_begin/kitty_end, kit_load, kit_show, kit_store,
kit_delete and kitty_run, not from grep:

- kit_parse drops a '-' sign (`s++`, comment "z<0"): every negative value
  becomes positive. z=-1 is parsed as z=1; H/V and the a=f gap z<0 cannot
  be told apart. kv[] is vt_u32: no signed values at all. (Bug today, not
  only a gap: a client asking z=-1 gets an image over the text it meant
  to be under. Harmless only because z is ignored.)
- kit_load: t= other than d answers `ENOTSUP:only direct data (t=d)`; S= and
  O= are never read.
- kitty_run: U=1 with a=T/p answers `EINVAL:no unicode placeholders`;
  a=f, a=a, a=c fall to `EINVAL:not supported`. i= and I= together are
  accepted (spec: EINVAL). a=p with neither i= nor I= reaches kit_find
  and answers ENOENT without an id (no reply is sent at all, since
  kit_reply needs an id).
- kit_show ignores X=, Y= (pixel offset in the first cell), z=, P= Q= H= V=
  (relative placement), N= (transient hint). The image is a scaled copy
  per placement (kind 1), drawn by blanking the cells (VT_CELL_IMAGE).
- kit_store: a re-transmit with the same i= frees the stored copy (kind 2)
  but leaves the old placements (kind 1) on the screen; the spec deletes
  the image's placements too.
- kit_delete: d= a A i I n N c C only. Missing: p P (cell x,y), q Q (cell
  x,y with z), r R (id range x..y), x X (column), y Y (row), z Z (z-index),
  f F (animation frames). An a=d arriving while an m=1 upload is open does
  not abort the upload (kitty_begin with k->more set keeps assembling).
- kitty_end: the keys of later chunks are ignored except m= and q=; the
  spec allows that, but a=f chunks must carry a=f (check stays as is).

### Placement and drawing today (what z changes)

- image_place / sixel_place_row (engine/vtengine.c ~5213-5330) overwrite
  the covered cells with blanks (fg = bg = default) marked VT_CELL_IMAGE
  and link a vt_place (img, gen, row, col0) into the line's list
  (place_add drops older placements this one covers whole).
- vt_cell.pad note (engine/vtengine.h:86-90): writing or erasing a cell
  rewrites it whole, so text over an image takes its place (the mark goes).
- render/amiga_render.c draw_rows paints the cells (text and background),
  then, when vt_images() > 0, draw_images (~2627) walks vt_row_image in
  oldest-first order and writes each image's tile with img_run only into
  runs of cells still marked VT_CELL_IMAGE. Newest on top; no z.

So today every kitty image is "z >= 0 and the text under it is gone". The
z work (K27/K28) needs: a placement that does not blank the cells (z<0),
a draw pass for it between the cell background and the glyphs, and an
ordering key (z, then image id) for vt_row_image instead of age.

### Items

Every item: host test through the real entry point (vt_write of the APC
bytes into a vt_term made by kitty_term in tests/test_kitty.c), with a
sentinel counter the new code bumps (vt_kitty_* accessor, as
vt_kitty_placed / vt_kitty_png_own) and the reply bytes asserted; the test
is written first and fails on today's main (the fail-before is named per
item). Items that draw get a rig proof: PROF build, `UPTERM_RIG=<free n>
python3 tools/rig/rig.py start --max --purpose "<session>: kitty <Kn>"`, a
generated stream file `Type`d into an XCON window, the PROF counter line
quoted, the screenshot measured in pixels by a haiku subagent (memory:
visual proof = pixels), rig stopped and cleared, `ps` line in the report.
RULES 2: each behaviour change updates the conformance matrix in the same
commit.

#### Keys and replies

- [x] K19 Signed keys. kit_parse keeps the sign: kv[] becomes long (C89,
      32 bits on vbcc), values past 2^31 (i=, p= up to 4294967295) kept
      as unsigned through a cast at the use sites. Letters stay as today.
      Test: `z=-1` reaches kit_show as -1 (sentinel: a vt_kitty_last_z
      test accessor, or the placement's z through vt_row_image after
      K27); fails before (reads 1).
- [x] K20 Reply rules: i= with I= answers `EINVAL`; a=p without i/I
      answers EINVAL; a delete (a=d) that arrives while an m=1 upload is
      open aborts the upload (kitty_abort before the delete runs). Test:
      the three replies / the abort (a following m=0 chunk finds no
      upload: no image placed, vt_kitty_placed unchanged); each fails
      before.
- [x] K21 Re-transmit with the same i= deletes the old image's placements
      as well (kind 1 with that kid freed in kit_store). Test: T i=5, T
      i=5 with another picture: one image on the screen
      (vt_row_image count 1); fails before (2).
- [x] K22 N=1 transient: such a stored image is evicted before any other
      in kit_room. Test: fill the budget, the N=1 one goes first while an
      older plain one stays; fails before (oldest goes).
- [x] K23 (engine, render code, host test done 2026-11-01; rig pixel proof done 2026-11-01, rig 34 --max: 16x16 raw RGB frame, control X=0,Y=0 red bbox (4,34)-(19,49); X=4,Y=4 red bbox (8,38)-(23,53): +4,+4, same size, 60 red px each; shots ~/Desktop/upterm-shots/kitty-k23-s48-ctl.png / -off.png) X=, Y= pixel offset inside the first cell (must be smaller than
      the cell; else clamp to cell-1). Engine: vt_image_view gains px0,
      py0 (pixel offset of the image inside col0); vt_row_image fills
      them; the image covers ceil((X + w) / cw) cells. Render: img_run
      starts the tile px0 pixels into the cell, the uncovered part of the
      first cell row/column shows the cell background. Test: X=3 on 8-px
      cells, a 10-px picture covers 2 cells and the view says px0 = 3;
      fails before (px0 does not exist: compile-time field, the test is
      the cell count, 2 vs today's 2 for 10 px... use 14 px: 3+14 = 17 ->
      3 cells, today 2). Rig: a 1-px red frame placed with X=4,Y=4: pixel
      measurement shows the red edge 4 px into the cell.
- [x] K24 The missing deletes: d=p/P (cell x,y), q/Q (cell x,y, z), r/R
      (ids x..y), x/X (column x), y/Y (row y), z/Z (z-index). Cell
      coordinates are 1-based. Capital letters also free stored images
      with no placement left. One table-driven test, one row per letter,
      each asserting the image count after; every row fails before
      (nothing deleted). No rig (deleting is drawn by the same path as
      d=i, which K12 covers).

#### Files (t=f, t=t, t=s, S=, O=)

- [x] K25 Engine hook (done 75a8c755: host test k25, 204 checks) for files. The engine has no OS calls (RULES 1), so
      as with vt_set_image_decoder: `vt_set_file_reader(t, read, unlink)`
      where read(path, offset, size, &buf, &n) returns 1/0 and fills a
      host-owned buffer (freed through the existing release hook),
      unlink(path) returns 1/0. kit_load for t=f/t=t: path = the
      base64-decoded payload; O= offset, S= byte count (0: to the end),
      capped at KIT_RAW_MAX; the bytes then go through the same f=24/32/100
      and o=z paths as direct data. Any failure answers exactly
      `EBADF:Failed to read image file` (the spec: one message so a client
      cannot probe the file system). No reader set: same EBADF. Test: a
      stand-in reader with a call count; t=f reads, O=/S= arrive at the
      reader as given, a reader failure gives the EBADF text, no reader
      gives EBADF; fail-before: today `ENOTSUP:only direct data`.
- [x] K26 t=t deletes (done 75a8c755: host test k26) after the read, only when the full path contains
      `tty-graphics-protocol` AND the host says the path is in a temp
      place (the unlink hook decides the place, the engine checks the
      name). The engine calls unlink only after a successful read, and
      never for t=f. Test: name without the marker -> no unlink call;
      marker -> one unlink call; t=f -> none. Fails before (no reads).
- [x] K27 Handler reader (done 75a8c755 + tools/rig/kitty_files_rig.py on rig 40: both pictures show, 512 px per stripe colour; T: loses tty-graphics-protocol-1.rgb, keeps kf2.rgb; PROF kitty files read/deleted 2/1; CON: t=f leaves the window alive. Not read back: the EBADF reply bytes themselves, covered by the host test only) (handler/vtcon_handler.c, next to the K9
      datatypes decoder). AmigaDOS path as given (`T:x`, `RAM:x`,
      `Work:pics/x.png`). A path starting with '/' is an ixemul program's
      Unix path: `/tmp/...` -> `T:...` (D11) and `/vol/rest` -> `vol:rest`
      (ixemul's own mapping), relative paths are refused (the handler has
      no notion of the client's current directory). Refusals, all EBADF:
      the volume is not a file system (IsFileSystem() false: CON:, SER:,
      PIPE:, XCON: itself; Open on those blocks or reads the user's
      keyboard), not a plain file (Examine: fib_DirEntryType >= 0), soft
      link loops (Lock fails), short read. Temp place for t=t:
      SameLock(ParentDir(file), Lock("T:")) == LOCK_SAME, or the
      ixemul /tmp mapping. Reads with the handler's own stack (RULES 6:
      the read runs on the handler process, size the buffer from S=, not
      the stack). Sentinel: PROF line "kitty files read/deleted". Rig:
      a script writes `T:tty-graphics-protocol-1.rgb` (raw 32x32 RGB),
      sends `a=T,t=t,f=24,s=32,v=32` with the base64 of that path, the
      picture shows (pixel check) and `List T:` no longer has the file;
      the same with t=f leaves it; `t=f` of `CON:` answers EBADF without
      hanging (reply bytes read back through the rig's capture).
- [x] K28 t=s shared (done 75a8c755: host test k28. ixemul grep: shm_open/shm_unlink in 3169 .c/.h files of /Users/spot/Code/UP-Term/ixemul-vtcon, no match; only SysV shmget man pages) memory answers `EBADF:Failed to read image file`
      (D10). Reason: AmigaOS has no named shared memory objects (no
      shm_open; one address space, but nothing names a region a client
      could hand over), and ixemul has no shm_open either (unverified: grep
      the ixemul sources the kit ships before the task closes it). EBADF
      is what kitty answers for any unreadable medium, so clients that
      probe (icat's transfer-mode detect) fall back to t=f or t=d. Test:
      t=s gives the EBADF text; fails before (ENOTSUP text).

#### Placements: relative and z

- [x] K29 Relative placements P=, Q=, H=, V= (done c0a4a712: host tests k29_*, rig check tools/rig/kitty_rel_rig.py). The child is placed at the
      parent placement's top-left cell plus H columns, V rows (signed,
      K19); it never moves the cursor whatever C= says. vt_image gains
      ppid/pkid (the parent's kid/pid) and a depth; deleting a parent
      deletes its children (recursively, in kit_delete and in image_free
      for kind 1); errors ENOPARENT (no such parent placement), ECYCLE,
      ETOODEEP (depth > 8, the spec's minimum). The parent position
      when the parent is a virtual (U=1) placement: the minimum of its
      placeholder cells' positions (needs K31; until then ENOPARENT). A
      U=1 placement that names a parent: EINVAL. "Moves with the
      parent": both sit on cells, so scrolling and reflow move them
      together already; nothing else moves a placement in vtcon. Tests:
      child at (parent + H,V); delete parent -> child gone; the three
      error replies; cursor unchanged with C=0. All fail before
      (P/Q ignored: the child lands at the cursor). Rig: a parent and
      two children at H=4 and V=2 drawn where expected (pixel check).
- [x] K30 z-index (done on branch s51-kitty-te: host tests k30_* in kitty and render, rig check tools/rig/kitty_z_rig.py, evidence thoughts/shared/research/s51-evidence/kitty-te.md). Engine: vt_image gains z (signed). vt_row_image returns
      a row's images ordered by (z, kid), lowest first (the spec: equal z,
      lower id is lower), not by age; sixels keep z = 0 and their age
      order among themselves (kid 0, serial). vt_image_view gains z.
      z < 0: image_place does not blank the cells and does not set
      VT_CELL_IMAGE; it sets a new flag VT_CELL_UNDER on them, which text
      writes keep (the one exception to "writing rewrites the cell
      whole": pad bit 0x02 survives print and erase-with-default-bg; ED/EL
      that blank the line drop the placement as today). z >= 0: sets
      VT_CELL_IMAGE; since K40d (O6) a kitty picture keeps the cells' text
      and colours under it (a sixel still blanks them); text written later
      takes the cell (D12). Render: draw_rows learns one more step for
      cells with VT_CELL_UNDER: fill the cell background, draw the z<0
      image tiles (z <= -1073741824 only into cells whose bg is the
      default; above that into every cell, drawn over the cell's bg),
      then the glyphs transparently (JAM1) over them. Glyph edges over
      image pixels: D13. Cost: the extra pass only runs on rows with an
      UNDER cell (the vt_images() test stays the per-frame gate). Tests:
      (engine) order by z then id on one row; z=-1 text written into the
      area keeps the image (VT_CELL_UNDER set, the glyph in the cell);
      (render, ONLY=render, fake canvas) a z=-1 image with an 'X' over it:
      the X's pixels in the fg pen, the rest of the cell the image's
      pixels; a z=-2000000000 image under a cell with a red bg: the red
      wins. Fail-before: the order test (age order today) and the glyph
      test (the cell is blank today). Rig: `ls` output over a z=-1 picture
      on both an RTG and an AGA screen (RULES 5): pixel check of a glyph
      pixel and an image pixel in the same cell.

#### Unicode placeholders (U=1)

- [x] K31 (done on branch s51-kitty-k31: host tests k31_* in ONLY=kitty; virtual placement = vt_image kind 3 with vc, vr; vt_cell_placeholder, vt_kitty_placeholders; diacritic table 297 entries counted twice from gen/rowcolumn-diacritics.txt) Engine. a=p,U=1 (or a=T,U=1) makes a virtual placement: a
      stored image (kind 2) with vc, vr (c=, r=; default from the picture
      size in cells) and a placement id; nothing on the cells, the cursor
      untouched. Placeholder cells are ordinary text: U+10EEEE (not BMP:
      it lives in the cluster table like any non-BMP code point) plus up
      to 3 combining marks from kitty's rowcolumn-diacritics.txt
      (row, column, the id's high byte); vt_cluster keeps them (6 code
      points a cell, VT_CLUSTER_CPS). The image id is the cell's fg colour
      (a 256-colour index: id 0-255; 24-bit RGB: id = R<<16|G<<8|B), the
      high byte from the third mark; the underline colour (vt_cell_ext) is
      the placement id, 0 = any. Missing marks are inferred from the cell
      to the left (same fg and underline colour, same row, column + 1),
      in the engine, not the renderer. The diacritic table (297 code
      points in the file; count to confirm from the file itself, twice)
      becomes a sorted const array in the engine for a binary search.
      New API: `int vt_cell_placeholder(const vt_term *t, int row, int col,
      vt_image_view *v, int *tile_col, int *tile_row)` -> 1 and the
      virtual placement's image scaled to vc x vr cells, plus which cell
      tile this cell shows. Deletes: virtual placements are touched only
      by d= i I r R n N (K24 table gains rows). Test: virtual placement
      of a 2x2-cell picture, then four placeholder cells written with SGR
      38;5;<id> and the marks: vt_cell_placeholder answers each tile
      (sentinel vt_kitty_placeholders resolved count); the inference
      case (marks only on the first cell); d=a leaves the virtual
      placement. Fail-before: U=1 answers `EINVAL:no unicode
      placeholders` today.
- [x] K32 Render. draw_rows: a cell whose base code point is U+10EEEE
      draws, instead of a glyph, its tile of the virtual placement's
      scaled picture (scaled once per virtual placement and cell size,
      cached under the image's serial like the pen cache; freed with the
      image). Transparent pixels show the cell's own background (the
      spec), not the default bg. No image found: the cell draws as a blank
      in its bg (kitty draws nothing). Test (ONLY=render): the 2x2 case
      above draws the picture's four quarters into the four cells; fails
      before (the renderer draws the fallback glyph for U+10EEEE). Rig:
      a script doing what tmux would (virtual placement, then the cells
      printed as text, then the screen scrolled by 3 lines with plain
      text): the picture scrolls with the text (pixel check).
- [x] K33 What ncurses/tmux programs need, checked, not built: vtcon's
      terminfo already has RGB and Tc (terminfo/vtcon.terminfo:25), so
      tmux passes 24-bit fg ids; tmux needs `allow-passthrough on` for
      the APC upload (DCS tmux; wrapper: confirm vtcon's DCS parser
      unwraps nothing itself, tmux does) and keeps combining marks in its
      grid; ncurses programs only print the cells. Deliverable: a doc
      note in the man page plus one rig run of a tmux build from the
      kit, if the kit has tmux (unverified; if not, the note says so and
      the run is dropped by the owner, not by the agent).
      PARTIAL 2026-11-02: facts checked (terminfo/vtcon.terminfo:25 has
      RGB, Tc; engine/vtengine.c dcs_dispatch :4899 handles only $q and
      +q, any other DCS goes to note_value, so no tmux unwrap), note in
      dist/README.txt TMUX section, the kit has tmux 3.6a (dist/packages.txt
      base row). Rig run OPEN: `kitty_z_rig.py <out> phtmux` (rig 82) found
      no tmux server after the typed command, window text not read; the
      next step is a screenshot read of that window (error text).

      DONE 2026-11-02 (s51): `kitty_z_rig.py phtmux` PASS on rig 86 (four
      boxes) with tmux-amiga branch s51-pane-console. Blocker was
      TMUX-PANE-CONSOLE: native commands in a pane printed nowhere; root
      cause and fix in thoughts/shared/research/s51-evidence/k33-tmux.md.

#### Animation (a=f, a=a, a=c)

- [x] K34 Frames (a=f) in the engine. A stored image gains a frame list:
      frame 1 is the root (the a=t picture), each a=f adds a frame (or
      edits frame r=), built on the canvas of frame c= (default: a
      transparent canvas filled with Y=, RGBA) by writing the sent
      rectangle x,y,s,v; X=1 overwrites, the default blends (D16:
      alpha >= 50% overwrites, below keeps the base pixel, as D4 does).
      z= the gap in ms (0 ignored, default 40, negative: gapless, never
      shown). Frames are stored as full indexed canvases on the fixed
      6x6x6 + 40 grey cube with no packing (kit_quant's index as is), so
      every frame of an image shares one palette and the renderer's pen
      cache (keyed by serial) maps it once. Memory: frames count against
      a frame budget VT_ANIM_MEMORY (D15), ENOSPC when it is full; d=f/F
      delete the frames (K24 table gains the row; F frees the image when
      no placement is left). A 200x200 picture is 40 KB a frame: 50
      frames per MB. Test: root + 2 frames, the second built on the first
      with a 1-px rectangle: vt_kitty_frames == 3 and the pixels of frame
      3 are frame 2's plus the rectangle; ENOSPC past the budget. Fails
      before (`EINVAL:not supported`).
      Evidence 2026-11-02: tests/test_kitty.c k34_* (root + 2 frames,
      vt_kitty_frames == 3, frame 3 = frame 2 + rectangle, blend/X=1,
      ENOSPC past VT_ANIM_MEMORY, d=f/d=F): `make test ONLY=kitty` 307
      checks, 22 failed before (a=f branch off), 0 after; make amiga builds.
- [x] K35 Control and compose (a=a, a=c). a=a: s=1 stop, s=2 run and wait
      for frames at the end, s=3 run and loop; v= loops (0 ignored, 1
      forever, n plays n-1 loops; stopping resets the count); c= makes a
      frame current (client-driven); r=+z= sets a frame's gap (the root
      has no gap until set). a=c: frame r= onto frame c=, rectangle w,h
      from X,Y to x,y, C=1 overwrites else blends; ENOENT for a missing
      frame or image, EINVAL for a rectangle out of bounds or the same
      frame with overlapping rectangles. Engine clock: `long
      vt_anim_tick(vt_term *t, long us)` advances every running
      animation by us, switches frames whose gap ran out (gapless frames
      skipped), marks the rows of every placement of a changed image
      dirty (mark()), and returns the microseconds to the next due frame
      (-1: nothing runs). Placements: a placed animated image keeps one
      scaled buffer per placement and rescales the current frame into it
      on a switch (one w x h copy, nearest pixel, the same loop as
      kit_make without the quantiser); unscaled placements point at the
      frame directly. Tests: s=3 with gaps 40/40: vt_anim_tick(40000)
      switches to frame 2 (sentinel vt_kitty_frame_switches), returns
      40000; v=2 stops after one loop; s=2 waits at the last frame and
      goes on when a frame arrives; c=2 shows frame 2 with no tick; a=c
      results and its two errors. All fail before.
      Evidence 2026-11-02 / DONE (the K35 commit on s51-kitty-k35):
      tests/test_kitty.c k35_* (every a=a key, loops/wait/gapless/errors,
      a=c pixels and errors, reachability through the byte stream: a=p,
      a=a s=3, vt_anim_tick, sentinel +1, damage after vt_flush, placement
      pixels = frame 2, root edit r=1 reaches the placement): `make test
      ONLY=kitty` 351 checks, 44 new, all failed before (a=a/a=c answered
      `EINVAL:not supported`), 0 failed after; make amiga builds.
      Resolved: a=c copies frame r= onto frame c= as written here; this
      matches the kitty spec ("source frame r=, destination frame c=",
      checked 2026-11-02 against sw.kovidgoyal.net graphics-protocol); the
      root gap stays K34's 40 ms; a=a on a picture without frames answers
      `ENOENT:the picture has no frames`; c= counts in the sentinel.
- [x] K36 (done: host part c63dcb4f, AGA and RTG rig proof 6e0e2fe0, evidence thoughts/shared/research/s53-evidence/k36-rig-proof.md) Handler timer, CPU and the profile key. The window already has a
      frame clock: render/vtwin.c frame_start arms a timer.device
      TR_ADDREQUEST for frame_wait us, vtwin_tick runs on its signal and
      re-arms it while vr_blink_tick / vr_flash_tick say something is
      still changing (the cursor blink is driven this way). Animation
      joins the same clock: vtwin_tick calls vt_anim_tick(t,
      frame_wait); when it returns >= 0, frame_start re-arms with
      min(that, the blink's period) instead of a new timer. Nothing runs
      when no animation plays (no CPU when idle). Paused while the window
      is hidden/iconified (r->hidden) or scrolled back (r->view): the tick
      is not called, so no time passes for it. A gap shorter than the
      frame clock (VT_PACE_MIN_US) is shown for one frame clock. CPU: a
      switch costs the rescale plus a redraw of the image's cells through
      WriteChunkyPixels; on AGA that is the chunky-to-planar conversion of
      the whole picture each frame (a 200x200 picture at 25 fps is 1 MB/s
      of c2p); the PROF build counts switches and their render time
      (vr_prof slot) so the rig run reports the cost on RTG and AGA.
      Profile key `image-animation` (read where `cursor-blink` is,
      handler/vtcon_handler.c ~1432): on (default, D14) / off; off stops
      terminal-driven playback (the image shows its current frame,
      normally the root), client-driven c= still switches. Test (host):
      the profile parse sets the flag and vt_anim_tick is not called
      with it off (call count through a test seam in vtwin, or the engine
      flag vt_set_animation(t, 0) making vt_anim_tick return -1 with no
      switch). Rig: a 4-frame 64x64 animation (generated stream, gaps 100
      ms) in an XCON window on RTG and on AGA: two screenshots 150 ms
      apart differ in the measured pixels; PROF switches > 0 and the
      per-switch time quoted; with `image-animation off` in the profile
      the two shots are equal and PROF switches = 0.

#### Truecolor on deep screens

Owner 2026-11-02: kitty pictures in full colour on screens deeper than 8
bits (RTG 15/16/24/32), in this release; 8-bit screens (AGA, 8-bit RTG)
exactly as today. Measured today: rig 91 RTG 800x600x24 shows demo.png
(320 px wide, 29954 source colours) in 18 colours.

Verified 2026-11-02 (two independent cybergraphics_lib.fd copies, CGX/AMOS SDK
and MorphOS SDK, both agree; their -120/-126/-150/-198 match our proven calls):
WritePixelArrayAlpha is LVO -216, registers
(src a0, srcx d0, srcy d1, srcmod d2, rp a1, destx d3, desty d4, width d5,
height d6, globalalpha d7), source format fixed 32-bit ARGB (alpha byte first),
globalalpha 0xFFFFFFFF = per-pixel alpha only. BltTemplateAlpha is -222.
cybergraphics.h: RECTFMT_RGB 0, RECTFMT_RGBA 1, RECTFMT_ARGB 2. -126 is
WritePixelArray (not the alpha call). Still unproven: that the rig's P96
cybergraphics.library exports -216 (version >= 43, lib_NegSize >= 216).

Read 2026-11-02 on a6de061d (what the design rests on):

- The colour is lost in the engine, not the renderer. kit_make
  (engine/kitty.c ~150) runs every pixel through kit_quant (cube + 40
  greys, 4x4 Bayer) and packs the used entries into a palette. On a deep
  screen the renderer is already exact: img_colours copies the palette
  into a 0xRRGGBB table and img_run draws with WriteLUTPixelArray
  (render/amiga_render.c ~2574-2627), so only cube colours reach it.
- Deep-screen detection exists: vr_init obtains scratch pens and opens
  cybergraphics.library V41 only when the screen's BMA_DEPTH > 8, and
  closes it again when not truecolor (amiga_render.c ~586-635). So
  `r->cgx != 0` already means "deeper than 8 bits and the cgx API is there"
  (P96 ships the same library). The cgx calls are declared as vbcc inlines
  in render/aa_ink.h (WriteLUTPixelArray -198, FillPixelArray -150) with a
  host fake in tests/render_host/gfx_fake.c (GF_CGXLUT, GF_CGXFILL).
- a=T makes two pictures: kit_store keeps the root (kind 2, full size) and
  kit_show places a scaled copy (kind 1) made from the same source. a=p
  rebuilds the placement from the stored indices (or from the current
  frame's cube canvas, K34/K35). Frames are cube-index canvases
  (vt_anim.fr), counted in t->anim_mem against VT_ANIM_MEMORY.
- Sixel images stay as they are: their palette is at most 256 exact
  registers and already draws exactly on deep screens.

##### Decisions (T1-T8)

- T1 Where RGB lives. The engine gains a per-terminal picture format,
  set by the host: `void vt_set_image_rgb(vt_term *t, int on)`. Off (the
  default, and what DCTelnet and every 8-bit screen get): every line of
  today's code runs unchanged, so 8-bit output is today's by construction,
  not by conversion. On: kitty pictures (kinds 1, 2, 3 and every frame)
  hold 4 bytes a pixel, R G B A in memory order (the f=32 wire order), and
  no cube, dither or palette is made. Not "RGB always plus lazy indices":
  that costs 5 bytes a pixel and a second quantiser path to keep
  byte-equal to today's; one format per terminal is simpler and the
  host knows the screen before any picture arrives.
- T2 Alpha stays D4's binary rule: alpha < 128 stores A=0 with R=G=B=0
  (the pixel the picture leaves unset, drawn in the default background as
  index 0 is today), alpha >= 128 stores A=255. So z, VT_CELL_IMAGE /
  VT_CELL_UNDER, D12/D13 and the K30 draw order do not change, and nothing
  reads the screen back (R6). vt_image gains `vt_u8 fmt` (0 indices, 1
  RGBA) and `vt_u8 opaque` (no pixel has A=0, set by the maker); the
  slot-alive test `pix != 0` keeps working because pix holds the RGBA
  bytes. vt_image_view gains the same two fields; pal/npal are unused for
  fmt 1.
- T3 Making a picture in RGB mode: kit_make gets an RGBA twin
  (kit_make_rgb: the same nearest-pixel scale loop, `kit_px` read, alpha
  binarised, no kit_quant). a=p reads the stored RGBA root back as a
  kit_src with bpp 4, which kit_px already reads (R, G, B, A bytes): no new
  reader. Placement scaling to cells stays in the engine (tw x th as
  today); the renderer never scales.
- T4 Frames (K34/K35) in RGB mode: vt_anim.fr[] are RGBA canvases;
  kit_root_canvas is a memcpy of the root (no kit_root_map), kit_new_canvas
  fills with Y= as one binarised RGBA value, kit_blit and a=c copy 4-byte
  pixels with D16's rule (alpha >= 50% overwrites, else the base pixel
  stays), kit_anim_show rescales 4-byte pixels and skips the "packed
  palette becomes the cube" step (no palette, no serial bump needed beyond
  today's damage marks). vt_kitty_frame_px returns 0xAARRGGBB in RGB mode
  (cube index in index mode, as today).
- T5 Memory: the budgets scale with the pixel size so the pixel capacity
  is today's (owner question O1). Today 1 byte a pixel; RGB 4.

  | | today (1 B/px) | RGB, same 2 MB | RGB, budgets x4 (proposed) |
  |---|---|---|---|
  | VT_IMAGE_MEMORY | 2 MB = 2,097,152 px | 524,288 px | 8 MB = 2,097,152 px |
  | largest stored picture (kit_store: half the budget) | 1,048,576 px (1024x1024) | 262,144 px (512x512) | 1,048,576 px |
  | demo.png 320x200 via a=T (store + placement) | 128,000 B | 512,000 B (24% of budget) | 512,000 B (6%) |
  | largest placement 1024x1024 | 1 MB | does not fit | 4 MB |
  | VT_ANIM_MEMORY, 200x200 frames | 2 MB / 40,000 B = 52 | 13 | 8 MB / 160,000 B = 52 |

  Implementation: `t->img_budget` and `t->anim_budget` set by
  vt_set_image_rgb (VT_IMAGE_MEMORY, VT_ANIM_MEMORY times 1 or 4);
  kit_room, kit_store's half-budget test and the frame ENOSPC test read
  them instead of the macros. Sixel keeps counting 1 byte a pixel in the
  same img_mem. Worst case in RGB mode: 16 MB of fast RAM with both pools
  full (RTG machines carry it; DCTelnet never turns RGB on).
- T6 Switching while pictures exist (a window reopened on another screen
  with the same vt_term): on -> off converts every kitty picture and frame
  in place to cube indices through kit_quant at each pixel's own (x, y),
  packing the palette as kit_make does, and divides img_mem/anim_mem by 4;
  off -> on leaves existing pictures indexed (they stay cube colours until
  re-sent; the renderer draws fmt 0 through WriteLUTPixelArray as today).
  Converted pictures are close to, not byte-equal with, a fresh 8-bit
  upload (K34's Y= fill dithers at (0,0) today); only new uploads are the
  8-bit guarantee.
- T7 Renderer, deep path. render/aa_ink.h declares cybergraphics V41
  WritePixelArray (LVO -126; srcRect a0, SrcX d0, SrcY d1, SrcMod d2,
  RastPort a1, DestX d3, DestY d4, SizeX d5, SizeY d6, SrcFormat d7) as
  vr_cgx_write_pixels, beside vr_cgx_write_lut, with VR_RECTFMT_RGBA 1;
  LVO and format value checked against the AROS cybergraphics.conf / CGX
  SDK before the build (as the -198 note says). img_run, after its
  existing clip (first-cell px0, first-row py0, image edges; the layer
  clips the window): view fmt 1 and r->cgx: opaque picture -> one
  WritePixelArray straight from v->pix (SrcX sx, SrcY sy, SrcMod v->w*4,
  no copy); else copy the run's w x h into r->img_buf (grown to 4*w*h;
  a full-width 800 px run of 16-px cells is 51,200 B) with A=0 pixels
  replaced by the background's RGB (ink_rgb(r, r->bg_ink), the value
  img_colours puts in ctab[0] today), then write from it. img_colours is
  skipped for fmt 1. Partial redraw and scrollback need nothing new: they
  already call img_run per damaged run with the view's py/col0, and the
  placeholder path (K32, pv view) goes through the same img_run. Sentinel:
  `r->n_img_rgb_runs` (PROF/test counter beside n_img_runs).
- T8 Wiring and fallback. vr_init calls `vt_set_image_rgb(t, r->cgx != 0)`
  once, after the truecolor decision (one place, both the XCON handler's
  and vtwin's windows). Fallbacks: depth <= 8 or no cybergraphics.library
  -> r->cgx is 0 -> RGB off -> today's pens / WriteChunkyPixels /
  WriteLUTPixelArray paths. A fmt 1 view reaching img_run without r->cgx
  (cannot happen after T6) draws nothing and counts vr_gc; the renderer
  never quantises (the engine owns colour, rule 3's spirit). 15/16-bit
  screens: WritePixelArray's driver conversion truncates to 5-6-5, no
  dither (O3). K36's AGA c2p cost is unchanged (RGB never on there); an
  RTG frame switch costs a 4-byte rescale plus one WritePixelArray.

##### Items

- [x] K37 (a14fe089, 5ff4b661) 8-bit golden, first. Host test that pins today's 8-bit output
      before any change: tests/test_render.c `k37_eight_bit_golden`, run
      on gf_planar(8) (AGA) and on a chunky 8-bit screen (gf_truecolor(0),
      not planar): a 64x64 f=32 gradient (r=4x, g=4y, b=2(x+y), column 0
      alpha 127, column 1 alpha 128; 4096 source colours) by a=T, the same
      by a=p c=4, an a=f frame with a 4x4 rectangle and a=a c=2, and a
      4x4 sixel. Asserts an FNV-1a hash of the window's pens and of every
      WriteChunkyPixels buffer equal constants captured on a6de061d (the
      captured values written in the test with that commit named). Files:
      tests/test_render.c (and gfx_fake.c if the chunky buffers are not
      yet hashable). Passes before and after every later item; a change
      that moves it fails. Lands before K38.
- [x] K38 (ceacf685, merge 41acbb56) Engine RGB store (T1, T2, T3, T5, T6). Files: engine/vtengine.h
      (vt_set_image_rgb, view fmt/opaque, `unsigned long
      vt_kitty_rgb_made(const vt_term *)` sentinel), engine/vtengine.c
      (vt_image fmt/opaque, budgets, vt_row_image and vt_cell_placeholder
      fill the new view fields), engine/kitty.c (kit_make_rgb, kit_store,
      a=p, kit_room budgets, T6 conversion), tests/test_kitty.c k38_*.
      Host tests (`make test ONLY=kitty`): the 64x64 gradient by a=T in
      RGB mode: view fmt 1, the placement's RGBA bytes equal the source for
      every pixel of columns 1-63 (4032 distinct colours; index mode on
      the same stream gives npal <= 256), column 0 A=0, column 1 A=255,
      opaque 0; an all-opaque picture has opaque 1; img_mem after a=T is
      2 x 16,384 = 32,768 B (8,192 B in index mode); a=p c=4 (cw 8) gives
      a 32x32 placement with 992 distinct colours; 1024x1024 f=24 stores,
      1024x1025 answers ENOMEM in both modes (same pixel capacity);
      vt_kitty_rgb_made +1 per picture in RGB mode, 0 in index mode;
      switching to off converts: fmt 0, npal <= 256, img_mem divided by 4,
      vt_images unchanged. Fail before (no vt_set_image_rgb). K37 green.
- [x] K39 (eaa21b90, merge b0f5c9da) Engine frames in RGB (T4, T5). Files: engine/kitty.c (canvas,
      blit, compose, kit_anim_show, frame_px), tests/test_kitty.c (the
      k34_*/k35_* cases run in both modes through one loop; RGB mode
      checks pixels as 0xAARRGGBB). Numbers: root + 2 frames, frame 3 =
      frame 2 + the 1-px rectangle in exact RGB (source 0x123456 reads
      back 0xFF123456; in index mode it is a cube index); 200x200 frames:
      the 53rd canvas answers ENOSPC in both modes (52 fit: 2 MB and 8 MB
      at 1 and 4 B/px); vt_anim_tick(40000) switches to frame 2 with
      vt_kitty_frame_switches +1 and the placement's bytes equal frame 2
      scaled. Fail before (frames are indices). K37 green.
- [x] K40 Renderer deep path (T7, T8). Files: render/aa_ink.h
      (vr_cgx_write_pixels, VR_RECTFMT_RGBA), render/amiga_render.c
      (img_run fmt 1 branch, img_buf growth, n_img_rgb_runs, vr_init
      calls vt_set_image_rgb), render/amiga_render.h (counter),
      tests/render_host/gfx_fake.c/.h (GF_CGXPIX painting RGBA into the
      canvas, recording src pointer and SrcX/SrcY/SrcMod),
      tests/test_render.c k40_*. Host tests (`make test ONLY=render`):
      reachability through the byte stream: gf_truecolor(1), vr_init,
      pass() the 64x64 gradient a=T -> n_img_rgb_runs >= 1, gf_count(
      GF_CGXPIX) >= 1, no GF_CGXLUT for the picture, every canvas pixel of
      columns 1-63 equals GF_CGX | source RGB, column 0 equals the
      default background, the picture rectangle holds 4033 distinct
      colours (4032 + background); opaque picture: the recorded src
      pointer is the view's pix (no copy); partial redraw: damage of the
      picture's second cell row only -> one GF_CGXPIX with SrcY 16 and
      SizeY 16; scrollback: 30 lines scrolled, view moved back, the
      picture's pixels equal the unscrolled ones at their new row; K30 z<0
      case in RGB mode: picture before text in the call log; vr_init on
      gf_truecolor(0) leaves RGB off (vt_kitty_rgb_made stays 0). K37
      green. `make amiga` builds.
      Probe result (2026-11-02, tests/amiga/wpaprobe.c): the rig's P96
      cybergraphics.library is 42.7 with NegSize 240; its slots -204 to
      -228 all jump to one stub, and WritePixelArrayAlpha (-216) draws
      nothing, so the version number says nothing about whether the call
      is real. The renderer therefore detects it by the jump targets
      (vr_cgx_has_alpha: the table must reach -228, every slot read must
      be a JMP abs.l, and -216 must have a target of its own). On this rig
      it takes the software blend (K40b, O5): ReadPixelArray of the run,
      per-channel blend, WritePixelArray; sentinel n_img_rgb_soft_blend
      replaces the binary 50% fallback. K41's rig proof therefore exercises
      true alpha (translucent pixels blended with text and background); the
      WritePixelArrayAlpha path itself stays host-tested only.
- [x] K41 Rig proof (after K38-K40 merge to main; one build on the rig).
      tools/rig/kitty_truecolor_rig.py, reusing colour_check.py's PNG
      reader for both the source and the shot. Stream: demo.png decoded on
      the host and sent raw (f=32, a=T, no c=/r=, so 1:1 pixels), so the
      proof does not depend on picture.datatype 40.4 on OS 3.1 or on
      ICAT-STACK; icat VTC:demo.png (f=100) as a second, optional run once
      ICAT-STACK is closed. RTG: rig in `rig.py rtg` 800x600x24 (rig 91
      when free; purpose text "<session>: kitty truecolor proof"), XCON
      window, stream, screenshot, crop the picture rectangle from the
      cell origin: distinct colours >= 90% of the source's distinct colours
      in the shown rectangle (>= 26,959 of 29,954 if fully shown; 100%
      expected on 24-bit) and >= 99% of pixels equal to the source RGB;
      today's measure is 18, and the index path cannot exceed 255, so the
      count itself proves the deep path ran. AGA: same stream on an
      aga256 rig, shot pixel-equal to a reference shot taken from main
      a6de061d's kit with the same stream before K38 merges (stored at
      thoughts/shared/research/s51-evidence/truecolor-aga-ref.png), and
      distinct colours <= 256. Evidence and shots in
      thoughts/shared/research/s51-evidence/kitty-truecolor.md and
      ~/Desktop/upterm-shots. Stop the rig, confirm by ps.

Order: K37, then K38, K39 (engine, one agent), then K40, then K41. K36
(timer, PROF cost) runs after K40 so its RTG numbers measure the RGB
switch it will ship with.

##### Risks

- LVO -126 and RECTFMT_RGBA = 1 are from the CGX API as remembered;
  K40 checks them against the AROS cybergraphics.conf / SDK fd first.
  If the rig's P96 refuses RECTFMT_RGBA, RECTFMT_ARGB (2) needs the store
  order A R G B and a bpp branch in kit_px: found in K41's first run,
  so run a 4x2 picture on the RTG rig before the full proof.
- Memory: with O1's x4 budgets a full RGB session holds 16 MB; a 4 MB
  single allocation (1024x1024 placement) may fail on fragmented RAM and
  answers ENOMEM (no crash path changes).
- CPU: scaling and frame copies move 4x the bytes on RTG; K36's PROF
  numbers cover the switch cost.
- The window-moves-screens case (T6) is rare and only best effort.
- 15/16-bit RTG bands slightly (no dither); counted as full colour.

##### Owner decisions (2026-11-02)

- O1: x4 budgets on deep screens (8 MB images + 8 MB frames).
- O2: TRUE ALPHA, not the binary rule. Keep the full alpha byte in the
  RGBA store (no 50% cut on deep screens). Draw through
  WritePixelArrayAlpha (cybergraphics V43) so the driver blends the picture
  with what is under it (text, background, z<0 order). Where V43 or the
  driver lacks it, fall back to the binary 50% rule and count the fallback
  in a sentinel. K38/K40 tests change accordingly: a 50%-alpha pixel over a
  known background gives the blended value (state the expected RGB), and
  redraw order (text first, z<0 picture under, z>=0 over) is pinned by a
  host test. R6 (no screen read) is revisited in K40.
- O3: driver truncation on 15/16-bit screens, no engine dither.
- O4: frames become RGBA canvases on deep screens (D16 reversed there).
- O5: R6 (no screen read) is lifted for kitty alpha. Where the driver has no
  working WritePixelArrayAlpha (the rig's P96 42.7), the renderer blends in
  software: ReadPixelArray (-120) the run's rectangle from the rastport it
  draws into, blend per channel (src*a + dst*(255-a) + 127) / 255, write back
  with WritePixelArray RECTFMT_RGBA (K40b, sentinel n_img_rgb_soft_blend).
  The binary 50% fallback is gone.
- O6 (owner 2026-11-02, after K41 case 2 / K40c diagnosis): a kitty z>=0
  placement keeps the text and cell background under it, as real kitty
  does; translucent picture pixels show them. It sets VT_CELL_IMAGE without
  blanking the cells (engine/vtengine.c sixel_place_row ~5332 today writes
  b.fg = b.bg = VT_COLOR_DEFAULT blanks). Sixel keeps blanking. Changes D12
  and K30; the host test k40_soft_blend_reads_the_text_under_a_front_picture
  ("A z>=0 picture owns its cells") is rewritten to the new model. Work item
  K40d; K41 case 2 then expects (128,0,119) over blue cells.
  Done on branch s51-k40d: engine/vtengine.c sixel_place_row, host tests
  k40d_kitty_front_picture_keeps_the_cells (kitty) and
  k40_soft_blend_reads_the_text_under_a_front_picture (render), rig
  evidence s51-evidence/k41-kitty-rgb.md "K40d".

##### Open questions for the owner (answered above)

- O1 RGB memory budgets. Recommended: x4 (8 MB images + 8 MB frames),
  same pixel capacity as 8-bit screens. Other: keep 2 MB + 2 MB (largest
  stored picture 512x512, 13 frames of 200x200 on deep screens).
- O2 Alpha on deep screens. Recommended: D4's binary rule (same z/text
  model, no read of the screen, R6). Other: true alpha through
  WritePixelArrayAlpha (V43, the driver blends with the screen; edges of
  semi-transparent pictures exact, but redraw order and R6 to revisit).
- O3 15/16-bit screens. Recommended: driver truncation, no dither. Other:
  an ordered dither to 5-6-5 in the engine (needs the depth, not just
  on/off).
- O4 D16 on deep screens. Recommended: frames become 4 B/px RGBA canvases
  on deep screens only (full-colour animation; D16's blend rule kept).
  Other: frames stay cube indices everywhere (animations in <= 255
  colours on RTG, still pictures in full colour).

### Implementer tasks (one agent, one item group, one rig each; in order)

1. T-A keys and deletes: K19, K20, K21, K22, K24 (engine only; no rig; K24
   needs K19 for signed z in d=q/z).
2. T-B pixel offsets: K23 (engine + render; rig: one).
3. T-C files: K25, K26, K27, K28 (engine hook, handler reader; rig: one).
   Needs D10, D11.
4. T-D relative placements: K29 (engine; rig: one). Before T-F so the
   parent code exists when virtual parents come.
5. T-E z-index: K30 (engine + render; rig: one, RTG and AGA screens).
   Needs D12, D13. The largest render change; its own agent.
6. T-F placeholders: K31, K32, K33 (engine + render; rig: one). Finishes
   K29's virtual-parent case.
7. T-G animation engine: K34, K35 (engine only; host tests). Needs D15,
   D16.
8. T-H animation clock and key: K36 (vtwin + handler + profile; rig: one,
   RTG and AGA). Needs D14.

T-A first because K19's signed values are used by K23, K29, K30, K34. Each
task updates the conformance matrix and the kitty.c header comment
("Supported: ...") in its own commit.

### Owner decisions (answered 2026-11-01)

Owner picked the recommended option for D11, D12, D14 and D15 on
2026-11-01. D10, D13 and D16 take the recommended default as stated
below. Every decision is closed; T-B to T-H need no further input.

- D10 t=s shared memory. Recommended: answer EBADF (no named shared
  memory on AmigaOS or in ixemul; clients fall back). Other: emulate
  through a named public memory area (no client would send one).
- D11 Unix paths from ixemul programs. Recommended: the handler maps
  `/tmp/x` to `T:x` and `/vol/x` to `vol:x` (ixemul's convention) and
  takes AmigaDOS paths as they are. Other: AmigaDOS paths only (ixemul
  programs such as a ported icat then fail with EBADF).
- D12 Text written over a z >= 0 kitty image. Recommended: keep today's
  model, the text replaces the image in that cell (same as sixel; no render
  change). Other: kitty's, the image stays on top and hides the text
  (needs VT_CELL_UNDER-like bookkeeping for every image).
  Amended by O6 / K40d (2026-11-02): the text written later still takes
  the cell, but a kitty z >= 0 placement no longer blanks the cells it
  lands on: their text and background stay under the picture and show
  through its translucent pixels (the render draws bg + glyph, then blends
  the picture over them). Sixel placements still blank.
- D13 Glyph edges over a z < 0 image. R6 (owner) rules out reading the
  screen back to blend. Recommended: glyphs in cells over an image are
  drawn without antialiasing (hard edges, no fringe). Other: antialiased
  against the cell's bg colour (a fringe of the bg colour around each
  glyph on the picture).
- D14 Animation default. Recommended: `image-animation on` on every screen,
  with the PROF cost measured on AGA in K36; revisit if AGA cost is high.
  Other: on for RTG, off for AGA by default.
- D15 Frame memory. Recommended: a separate VT_ANIM_MEMORY of 2 MB on top
  of VT_IMAGE_MEMORY (frames never evict still pictures), ENOSPC past it.
  Other: frames share the 2 MB VT_IMAGE_MEMORY.
- D16 Frame blending. Recommended: alpha >= 50% overwrites, below keeps the
  base pixel (D4's rule; frames stay 1 byte a pixel). Other: true RGBA
  canvases (4 bytes a pixel, 4x the memory, exact blends).
