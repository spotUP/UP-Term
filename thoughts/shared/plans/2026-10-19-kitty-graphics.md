---
date: 2026-10-19
topic: kitty graphics protocol (APC G) in vtcon
status: K1-K18 done; K19-K36 planned (s48, owner 2026-11-01)
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
- [ ] K23 X=, Y= pixel offset inside the first cell (must be smaller than
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

- [ ] K25 Engine hook for files. The engine has no OS calls (RULES 1), so
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
- [ ] K26 t=t deletes after the read, only when the full path contains
      `tty-graphics-protocol` AND the host says the path is in a temp
      place (the unlink hook decides the place, the engine checks the
      name). The engine calls unlink only after a successful read, and
      never for t=f. Test: name without the marker -> no unlink call;
      marker -> one unlink call; t=f -> none. Fails before (no reads).
- [ ] K27 Handler reader (handler/vtcon_handler.c, next to the K9
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
- [ ] K28 t=s shared memory answers `EBADF:Failed to read image file`
      (D10). Reason: AmigaOS has no named shared memory objects (no
      shm_open; one address space, but nothing names a region a client
      could hand over), and ixemul has no shm_open either (unverified: grep
      the ixemul sources the kit ships before the task closes it). EBADF
      is what kitty answers for any unreadable medium, so clients that
      probe (icat's transfer-mode detect) fall back to t=f or t=d. Test:
      t=s gives the EBADF text; fails before (ENOTSUP text).

#### Placements: relative and z

- [ ] K29 Relative placements P=, Q=, H=, V=. The child is placed at the
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
- [ ] K30 z-index. Engine: vt_image gains z (signed). vt_row_image returns
      a row's images ordered by (z, kid), lowest first (the spec: equal z,
      lower id is lower), not by age; sixels keep z = 0 and their age
      order among themselves (kid 0, serial). vt_image_view gains z.
      z < 0: image_place does not blank the cells and does not set
      VT_CELL_IMAGE; it sets a new flag VT_CELL_UNDER on them, which text
      writes keep (the one exception to "writing rewrites the cell
      whole": pad bit 0x02 survives print and erase-with-default-bg; ED/EL
      that blank the line drop the placement as today). z >= 0 keeps
      today's model (D12). Render: draw_rows learns one more step for
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

- [ ] K31 Engine. a=p,U=1 (or a=T,U=1) makes a virtual placement: a
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
- [ ] K32 Render. draw_rows: a cell whose base code point is U+10EEEE
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
- [ ] K33 What ncurses/tmux programs need, checked, not built: vtcon's
      terminfo already has RGB and Tc (terminfo/vtcon.terminfo:25), so
      tmux passes 24-bit fg ids; tmux needs `allow-passthrough on` for
      the APC upload (DCS tmux; wrapper: confirm vtcon's DCS parser
      unwraps nothing itself, tmux does) and keeps combining marks in its
      grid; ncurses programs only print the cells. Deliverable: a doc
      note in the man page plus one rig run of a tmux build from the
      kit, if the kit has tmux (unverified; if not, the note says so and
      the run is dropped by the owner, not by the agent).

#### Animation (a=f, a=a, a=c)

- [ ] K34 Frames (a=f) in the engine. A stored image gains a frame list:
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
- [ ] K35 Control and compose (a=a, a=c). a=a: s=1 stop, s=2 run and wait
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
- [ ] K36 Handler timer, CPU and the profile key. The window already has a
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

### Open owner decisions

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
