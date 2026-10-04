---
date: 2026-10-04
topic: CCON 1.2.8b9 -- what moved conbench most (creep's own notes, shared with the owner)
tags: [s1, speed, ccon, conbench, stock-a1200]
status: final
---

Source: creep (CCON's author), Discord, 2026-10-04, pasted by the owner. Verbatim content, lightly
formatted. Friendly race: we share ours back (see ledger S1).

Rig: FS-UAE stock A1200, 68EC020 14 MHz, 2 MB chip, 16-colour hires, conbench SYNC SCALE 1 REPS 3.
Totals: 1.2.7 100.98 s -> 1.2.8b9 30.68 s (77x30); 86.46 -> 28.58 s (77x20).

1. Hot path in C inside the E handler: a position-independent blob INCBIN'd into the E source --
   text, newlines, SGR, cursor and region codes, the write packet, WaitForChar and the whole flush.
   E only gets the unusual sequences back. A harness runs blob and E original on random output and
   compares every model byte (zero mismatches over tens of thousands of chunks).
2. CODE SIZE IS SPEED on a stock 020: every instruction outside the 256-byte cache is a chip
   access. A 1.8 KB C painter lost to Text(); split into asm loops that each fit the cache, it won.
   Count instructions per function under vamos before timing anything. Printable scan: four bytes
   per long read with bit tricks. Copies and fills: two instructions per long. One 78-char line in
   the engine: 1705 -> 863 instructions.
3. CPU planar painter: glyph cache pre-shifted for the window's bit phase, four cells per output
   long, glyph longs transposed in registers, (bits AND A) XOR X per plane. A run of same-colour
   groups goes through a loop with nothing else in it; plain text needs no masking.
   sgr-perchar 8.08 -> 3.20 s.
4. Paced flushes: the next flush waits 1.5x what the last one cost (20-160 ms). Past a screenful
   the page is repainted instead of blitted. plain-lines 8.74 -> 1.70 s, clear-page 9.60 -> 1.96 s.
5. Adaptive jump scroll: automatic jumps during long output; when WaitForChar forces a flush after
   nearly every line the jump grows to a full screen; at rest it settles back so the window looks
   unchanged. sync-line 5.46 -> 1.34 s.
6. Exact plane mask: OR of every pen used, not the ROM's 1/3/all tiers. ANSI colours 0-7 scroll
   three planes, not four.
7. Newlines in runs: once a page has gone by, a scroll is ring bookkeeping plus one cleared row, in
   one register loop. Dirty-row tables slide as a window, so a scroll is O(1).
   scroll-nl 15.98 -> 2.40 s.
8. The cooked-mode cursor blip waits until output stops (or a read arrives) instead of being erased
   and redrawn every flush. About 3 ms a flush on a stock 020.
9. Packet-only wakeups skip the other ports; the common Write is accepted, copied and replied in C.
10. gcc 16 m68k traps: -flate-combine-instructions (on at -O2) miscompiled their engine (turned
   off). -mpcrel makes A5 a fixed register gcc never saves, so a library call taking A5
   (LockLayerRom) must set it by hand. -Os was slower than -O2 on the real target.
11. Measure apart: EClock phase profiler in the handler, instruction counts per function, pixel
   A/B (row hashes, direct painter vs Text, at five bit phases, with a planted bug to prove the
   check can fail).

Honesty rule (shared): WaitForChar draws everything pending before it answers.
