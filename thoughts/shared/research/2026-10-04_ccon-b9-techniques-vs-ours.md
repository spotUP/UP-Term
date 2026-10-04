---
date: 2026-10-04
topic: CCON 1.2.8b9/b10 speed techniques (creep's public notes) against UP-Term's
tags: [speed, s1, ccon, race, painter, packets]
status: draft
---

# CCON 1.2.8b9 techniques vs UP-Term

Source: creep's public repository github.com/creep-ltx/AmigaTools, `ccon/todo.md` sections
"1.2.8b9 - speed on a stock A1200" and "b9, continued" (commit e82f96a, 2026-10-04 17:21),
cloned read-only to ~/Code/creep-AmigaTools. **The repository states no licence: all rights
reserved. We read the techniques he describes (he shared them for the race) and write our
own code. No code is copied.** His newer near-floor work (2026-10-04 22:25: ~82% of his
stock-A1200 time is the OS floor) is not pushed.

His b9 TOTAL on his stock rig: 30.50 s (77x30 window). Ours 48.62 s (77x20, 8221eed).

| # | His technique (his words, condensed) | Ours today | Action |
|---|---|---|---|
| 1 | Dirty arrays slide: windows into 512-entry buffers, a scroll moves the window, O(1) | pri_mem sliding window (4860981) for rows; dirty marks? check vr dirty arrays | check render dirty tracking on scroll |
| 2 | Paced flushes 1.5x cost, 20-160 ms | same (9d674f6, pace.h) | done |
| 3 | Write buffer 4K -> 16K | check handler's write buffer size | measure |
| 4 | C engine for printables, LF/CR/BS/TAB/FF, CSI m H f A-D K J L M S T @ P | C + 68k asm engine; engine asm agent running | in progress |
| 5 | **Exact plane mask**: only the planes the pens use | check painter: planes written per run | likely gain on sgr rows |
| 6 | Planar painter: **pre-shifted glyph cache per window bit phase**, four cells per output long, glyph longs transposed in registers, per-plane (bits AND A) XOR X; slab loop under 256 bytes | BFINS painter any phase (8a48cd1); our transposed/aligned tries were slower (d11df66, 6263839) | his pre-shift-per-phase + one long per 4 cells is a different shape from what we tried: prototype under paintbench |
| 7 | **pfused1: one plane, uniform runs, plain pen-1-on-0 needs no masking**, hot loop with nothing else | no single-plane special path | add: plain text is the commonest case |
| 8 | Region ops L M S T @ P in C | ours in C | done |
| 9 | Cursor blip waits for rest (one quiet tick / a read / a choke point) | cursor drawn at flush end? CC2 sprite cursor branch | CC2 timing on the rig |
| 10 | **Main loop: a wakeup that brought only packets skips the port walk** | check handler main loop | likely "rest" share |
| 11 | Newline runs in one register loop; long copies/fills of model rows | lf_run (e769df1) | done |
| 12 | **wacc: the common ACTION_WRITE accepted in C, ReplyPkt by hand via exec PutMsg** | packet path in C with DOS ReplyPkt? | measure per-packet cost; bytewise/sync rows |
| 13 | jsync: four WAIT_CHAR flushes inside one burst grow the jump to rows-1 | jump scroll doubling (8221eed) | compare growth rule |
| 14 | Printable scan four bytes a long | engine asm agent (fast_cp) | in progress |
| 15 | clearrow one asm pass; abfill/abcopy in asm | check clear-page path | measure |
| 16 | On stock, code size is speed (256-byte instruction cache); gcc -Os vs -O2 measured | painter loop size measured? | check loop sizes with vasm -L |

His per-window memory: "on a 2 MB machine one window takes ~467K". Ours since 2fe7cc4: 134 KB.

Next (ledger S1): rows 5, 7, 10, 12 first (largest expected gain per hour), each measured
with phase_rig/conbench on the stock rig before and after.
