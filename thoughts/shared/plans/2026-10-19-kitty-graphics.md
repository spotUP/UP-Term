---
date: 2026-10-19
topic: kitty graphics protocol (APC G) in vtcon
status: planned
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
- [ ] K9 handler: datatypes PNG decoder behind vt_decode (handler side,
      opened lazily, closed at exit).
- [x] K10 host tests (tests/test_kitty.c, wired into `make test`, ONLY=kitty):
      raw RGB, RGBA alpha->unset, chunked, zlib, PNG via test decoder,
      query reply, delete by id, ENOENT, ENOMEM on budget, unsupported t=f
      error, image visible through vt_row_image().
- [ ] K11 render host test (make test ONLY=render): a kitty image draws its
      pixels on the fake canvas (proves renderer wiring).
- [ ] K12 reachability on rig: handler fed a real kitty PNG stream (small
      file, e.g. `kitty +kitten icat` capture or a python generator) in an
      XCON window; sentinel: engine counter kitty_images_placed > 0 and
      datatypes decode count > 0 in the PROF/debug output; screenshot checked
      by a haiku subagent.
- [ ] K13 docs: README / man page feature list; THIRD-PARTY notice.
- [x] K14 vt_unhandled no longer reports APC G.

Not in scope: z<0 below-text layer, unicode placeholders (U=1), animation
(a=f/a=a/a=c), t=f/t=t/t=s file transfer. Each replies an error.
