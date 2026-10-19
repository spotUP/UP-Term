---
date: 2026-10-19
topic: where the handler's time goes on an ncurses 6.6 frame write (ide-spike "137 ms per 4.4 KB frame, 30 us/byte in vt_feed"); A1200 68020 rig 2
status: measured and profiled, fix not started (agent budget)
---

# vtcon write-path cost per ncurses frame (020)

Question: the ide-spike measured 137 ms per 4.4 KB doupdate write (`ide-spike/results/redraw.md`, progress ledger lines 224-230) and attributed it to vt_feed. That number came from ncurses' side of write(); nothing inside the handler was split. This splits it.

## Workloads added (committed with this note)

- `tests/amiga/engbench.c` workload 7 "ncurses frame": the frame Scinterm paints (24 rows of cup, five style runs a row, each `ESC ( B ESC [ m` + setaf/setab in the 8-colour and the `38;5` form in turn + 11 characters, `ESC [ K`); ten frames, 42,636 bytes. Engine alone, no drawing.
- `tests/amiga/phaseprobe.c` shape 8 "ncurses-frame": the same frame, one Write per frame, 20 frames, through the handler; a PROF=1 handler prints the phase split on close.
- `tools/prof68k.py` knows workload 7 (`run ONLY 7 PERS 0`).

## Measurements (rig 2, `rig.py start --a1200`, PROF=1 handler, EClock 709,379 Hz)

Trap first: `ami.py long` runs its script in a `CON:` window. phaseprobe opens `*`, so run that way it measures console.device, not vtcon (153 ticks for 20 frames). The vtcon numbers below come from an `XCON:0/0/640/210/pp` window (`NewShell ... FROM RAM:pp.s`, xterm dialect, 80x24), `build/rig2/serial.log` after the last `ncurses-frame` marker.

| what | raw (hex ticks / n) | per frame |
|---|---|---|
| phaseprobe wall, 20 frames | 122 ticks of 1/50 s | 122 ms per 4,268-byte write |
| vt_feed (`idle/out` out) | 0xaafe0 = 0.987 s / 20 | 49.4 ms = 11.6 us/byte |
| write packet (`pkwrite/n`) | 0xab60c = 0.991 s / 20 | 49.6 ms: the packet is vt_feed + 0.2 ms |
| render (`render/frames`) | 0xd36ca = 1.221 s / 6 renders | 204 ms per render, 61 ms per write (frame pacing coalesced 20 writes into 6 renders) |
| of which rows (`rows/scroll`) | 0xcfd81 = 1.200 s | 98 % of render: the row painter; 204 ms / 1920 cells = 106 us a cell (~1,500 020 cycles) |
| fills / blits / beam waits | 17 ms over 83 fills / 0 / 0 | the window paints rows straight to chip RAM (no composed-frame blit) |
| engbench workload 7 | 24 ticks for 42,636 bytes | 88,825 bytes/s = 11.3 us/byte, 48 ms a frame: matches the handler's vt_feed figure |

Split of the 122 ms: vt_feed 49 ms (40 %), render 61 ms amortised (50 %), packets and loop ~12 ms. From a writer's side a write() that lands while the previous frame renders waits for that render, so ncurses sees feed + render = 110-250 ms a write: the ide-spike's 137 ms is this sum, not vt_feed alone, and "30 us/byte in vt_feed" is wrong: vt_feed is 11.6 us/byte.

## Where vt_feed's 49 ms go (from the engbench table and this frame's shape)

Plain text runs cost 4 us/byte (workload 0, 251 KB/s). A frame row is cup + 5 x (sgr0 + setaf + setab + 11 chars) + EL = 2.06 ms; the 55 characters cost ~0.2 ms, so the 17 control sequences cost ~1.8 ms, about 100 us (~1,400 cycles) EACH, whatever their length (3-byte `ESC ( B`, 3-byte `ESC [ m`, 10-byte `ESC [ 38;5;nnn m`). Workload 2 "colour a char" (`ESC [ 3n m` + 1 char, 6 bytes at 60 KB/s) gives the same 95 us a sequence. So the engine's per-sequence fixed cost, not the per-byte scan, is the cost. The vamos instruction profile (`tools/prof68k.py run ONLY 7 PERS 0`, 40 minutes, 6,317,378 instructions for 42,636 bytes = 148 an input byte, engbench's own frame builder 9 % of it) names the fees; the frame has 4,080 CSIs, 1,200 `ESC ( B` and 1,200 eleven-character runs:

| function | instructions | share | per call |
|---|---|---|---|
| asm:vt_asm_csi (CSI parameter scan) | 958,992 | 15.2 % | 235 per CSI of 3-10 bytes |
| vtengine:vt_feed (the top loop) | 870,645 | 13.8 % | 20 per byte |
| asm:vt_asm_put_run | 716,454 | 11.3 % | 597 per 11-character run: setup, not characters |
| vtengine:csi_fast | 547,398 | 8.7 % | 134 per CSI |
| vtengine:sgr | 439,920 | 7.0 % | 108 per SGR |
| feed + decode + esc_dispatch + sgr_reset (the `ESC ( B` byte path) | 791,700 | 12.5 % | 620 per 3-byte `ESC ( B`, which changes nothing (G0 is already B) |
| put_ascii_run | 366,087 | 5.8 % | 305 per run |
| vt_asm_fill + erase_cells (`ESC [ K`) | 292,566 | 4.6 % | 1,219 per EL |

No single function dominates: a CSI costs ~480 instructions across scan + csi_fast + sgr, a no-op charset designation 620, a text run 900 of setup for 11 characters. The two clear algorithmic wastes, in order: (a) `ESC ( B` going byte by byte through the state machine (12.5 % for a no-op: a vt_feed fast path that compares the designation with the current charset and skips it is the first fix); (b) vt_asm_csi + csi_fast + sgr at ~480 instructions for `ESC [ m` / `ESC [ 3n m` (param clearing and the sgr chain; the single-colour fast path in sgr() already exists, so the cost sits in the scan and csi_fast's dispatch).

## Violated invariants and root-cause levels

1. Engine (vt_feed, `engine/vtengine.c`): a control sequence must cost O(its bytes) on the scan path, not a ~1,400-cycle fee per sequence. Level: the SGR / charset dispatch after the scan (sgr(), pen_ext(), style_index(), esc_dispatch()), not the parser's byte loop. Fix there, after the one-frame vamos profile names the function.
2. Painter (`render/`, the rows path): a full-page paint of 1,920 cells costs 106 us a cell. An 8-plane 8x16 cell written to chip RAM from a 020 is bus-bound near 36 us (128 word accesses at ~280 ns); 106 us says the per-cell path does roughly three times the bus work or has per-run setup (pen resolution, fill then glyph) that a per-row span could hoist. Level: `render/painter*` row span code. This is the larger half of the write cost and is what the IDE's scroll (every cell changes) pays; no damage tracking can cut it for a scroll.

## Next step

1. Add the `ESC ( B` no-op fast path in vt_feed (next to csi_fast), then trim the CSI scan + csi_fast + sgr chain; regression test = an operation counter on that path in the engine suite (`make test ONLY=xterm`), failing if the per-sequence work returns.
2. Profile the rows path with the same PROF handler on phaseprobe shape 8 after `make build/amiga/vtcon-handler PROF=1 SERIAL=1`; compare to UPDemo BENCH's frames a second for a full-page scene.

## Results of the first three fixes (2026-10-19)

1. `ESC ( B` with G0 already B: commit 5e174eab, `esc_g_fast` in vt_feed before csi_fast. Test
   `a_designation_that_changes_nothing_skips_the_parser` (tests/test_xterm.c) fails with the call disabled
   ("= 0, want 4"). Host instructions after: not measured (the 40 minute vamos run of workload 7 had not
   finished when this was written). Rig 2 ms per frame: not measured.
2. `ESC [ m` and `ESC [ 3n m` / `4n m` short forms in csi_fast: commit 41d67baa. Test
   `the_short_sgr_forms_skip_the_scan_and_set_the_same_pen` fails with the path disabled ("= 0, want 6").
   Host and rig numbers: not measured, same reason.
3. Painter rows path: hypothesis tested and REFUTED, no commit. Hoisting the plane-independent glyph
   gather out of vp_asm_plane (one `vp_asm_gather` per span into a row-major buffer, planes read longs)
   was built and run under `prof68k.py run-paint SHAPE 4 30` (vamos instructions, program total):
   - shape 0 (8-cell runs): 294,373 before, 311,113 after (worse)
   - shape 1 (1-cell runs): 891,943 before, 1,074,943 after (worse)
   The per-run and per-cell gather set-up costs more than the per-plane byte gathers it saves at 4 planes.
   The original profile (shape 1, 4 planes, 200 rows): vp_asm_plane 71.7% (about 16 instructions per
   plane per row for a 1-cell run), vp_span_fast 24.5% (gp[] setup 144k, per-plane fb/bb decode 256k,
   call set-up 332k+232k). Instruction count is not bus time: the 106 us per cell is mostly BFINS
   read-modify-write on chip RAM, which instruction counts cannot show. Next candidates: byte-aligned
   plain stores when the bit phase is 0, and merging the per-plane fb/bb decode into one pass; both need
   a rig measurement of bus time, not vamos.
