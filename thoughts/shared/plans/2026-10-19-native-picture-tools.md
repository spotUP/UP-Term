---
date: 2026-10-19
topic: native Amiga programs that show pictures in vtcon (kitty graphics users)
status: planned, later (owner: "its a task for later but feel free to plan it")
depends: thoughts/shared/plans/2026-10-19-kitty-graphics.md (K1-K14 must land first)
---

# Native picture tools

Goal (owner 2026-10-19): run on the Amiga AND view on the Amiga. Programs
that run in a vtcon window and show pictures through the kitty graphics
protocol. Over SSH to Linux the usual tools (icat, chafa, yazi...) already
work once kitty lands; this plan is only for native programs.

## What the kit has and lacks

- Has (upterm-ports/pkgs): ncurses, zlib, lua, perl, jq, fzy, ncdu, file,
  libarchive, vim, less, git. cpython-amiga is a separate repo; its curses
  module status is unverified.
- Lacks: GLib, libpng, libjpeg, cairo, Rust, Go.
- The Amiga itself has picture.datatype (V43+ gives ARGB) with PNG, JPEG,
  GIF, IFF ILBM classes installed per system. That is the decoder for every
  native tool here: no libpng port needed.

## Decisions

- N-D1 One shared piece: `render/kittyout.c` (name to confirm against
  existing files): encode pixels or a PNG file as kitty APC G chunks
  (base64, 4096-byte chunks, m=1), with q=2 to suppress replies. Used by
  every tool below. No second encoder anywhere (single source of truth).
- N-D2 Decoding via datatypes in the tool (not in vtcon): PNG files are
  sent as f=100 raw file bytes when vtcon's kitty supports PNG (vtcon then
  decodes them with its own datatypes hook); all other formats are decoded
  by the tool to ARGB and sent as f=32. Scaling: tool scales to fit the
  window size in pixels (CSI 14 t / cell size from CSI 16 t) before
  sending, so the 020 never moves more pixels than the window shows.
- N-D3 Tools live in vtcon and build in `make amiga` like vtshow, hl, mdv
  (Makefile:433), C with vbcc, 68000-safe, no ixemul.
- N-D4 Detection: each tool sends a=q first; no kitty reply within a short
  timeout means it prints a text fallback (file name, size, format) and
  exits 5 (WARN), never draws garbage.

## Items (later; run after K1-K14)

- [ ] P1 kittyout.c shared encoder + host test (round trip through the
      engine's kitty parser: the decoded image equals the input).
- [ ] P2 `icat` (Amiga command): `icat FILE [WIDTH n] [HEIGHT n] [PLACE x,y]
      [CLEAR]`, ReadArgs template, any datatypes picture, fit-to-window
      scaling, a=q detection (N-D4). Reachability: rig run in an XCON
      window shows a PNG and a JPEG; sentinel = vtcon kitty placed counter
      and the tool's exit code; screenshot checked by a haiku subagent.
- [ ] P3 Fetch tool with picture logo (neofetch-style): own code, CPU,
      FPU, chip/fast RAM, Kickstart/Workbench versions, screen mode,
      uptime; logo is a picture via kittyout when kitty answers, ASCII
      logo otherwise. Name to decide (owner). creep's amifetch
      (AmigaTools) has no licence: read for ideas only, write own code.
- [ ] P4 File manager preview: port nnn (C, ncurses; upterm-ports) with
      its preview hook calling icat on the selected file. Recommended over
      ranger: native C, no Python start-up cost on the 020. Message the
      upterm-ports owner session (up-term-45) before touching that repo.
      Alternative if cpython-amiga gets curses: ranger, with its kitty
      preview method; measure start-up time on the 020 first.
- [ ] P5 Optional, only if GLib is ever ported: chafa (also gives text-art
      fallback for non-kitty terminals).

## Not native, do not port

yazi (Rust), timg (ffmpeg/GraphicsMagick, C++), kitty's own icat (needs
kitty), gnuplot kitty terminals (cairo / libgd).

## Open for the owner

- Tool names (icat clashes with nothing on the Amiga; the fetch tool's name).
- Whether P4 is nnn (recommended) or ranger.
