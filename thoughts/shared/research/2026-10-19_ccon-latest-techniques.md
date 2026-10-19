---
date: 2026-10-19
topic: CCON speed techniques added after 1.2.8b9 (e82f96a) up to 1.3.1b1 (dafb149), against vtcon
tags: [speed, s1, ccon, painter, packets, pace]
status: draft
---

# CCON after 1.2.8b9: what is new, what vtcon lacks

Source: creep-ltx/AmigaTools `ccon/todo.md` (diff e82f96a..origin/HEAD), `ccon/README.md`; clone read only,
no licence: techniques below are described in my own words, no code copied. Earlier tables:
2026-10-04_ccon-b9-techniques-vs-ours.md (items numbered #1-#16 there).

## Range and his numbers

Covered: 1.2.8b10-b18, 1.3.0 (7.10.26, Aminet 8.10.26), 1.3.1b1 (14.10.26). After b13 (6.10.26) the
betas are Audit8 correctness fixes; 1.3.1b1 is a Shell Ctrl-C fix (CHANGE_SIGNAL). Speed work is b11-b13 only
(4-5.10.26) plus PACE=FAST (b12). Nothing after b13 changes speed except J25/J26 (pacing only if every flushed
console has FAST; WAIT_CHAR handed back to E only when only the blip is owed) and J3/J6 (glyph-cache and
geometry checks before a direct paint, "speed only" cost: none measured).

His conbench (fifteen workloads, SYNC barriers, stock A1200 FS-UAE 14 MHz 020, 2 MB chip, 77x30):
| build | full suite | note |
|---|---|---|
| 1.2.7 renderer | 101.3 s | |
| b10 | 31.18 s | start of this range |
| b11 (row lengths + model-direct painter) | 28.84 s | |
| b13c | 26.27 s | pmsetup, mscan, wsigok, TWOC |
| b13c + PACE=FAST (x2.5, 40 ms floor) | 25.0 s | his decision, option |
| 1.3.0 | 26.3 s | per-test: colour text 21.38 -> 6.42, plain lines 8.80 -> 1.60, scroll-nl 16.04 -> 1.34, clear page 9.74 -> 1.60 |
| null DOS handler (floor) | 12.26 s | answers every packet, draws nothing |
Real A1200 + PiStorm (97x72, SCALE 10): b10 8.52 -> b13c 8.30 s, floor 6.9 s (so only ~1.4 s is CCON's own there).
Cost model he states: straight-line code ~1.3 us per instruction on stock (every fetch a chip access), cached
loops cheap per instruction but ~2.5-3 us per data access: count data accesses in loops. Each step kept only if
it won in alternating A/B rounds (3 rounds), on both stock and PiStorm.

## New techniques (after b9)

Gain column: ESTIMATE for our 020 ncurses frame (4268 bytes: vt_feed 49 ms, painter 61 ms, packets 12 ms),
scaled from his ratios, not measured. Rank 1 = do first.

| technique (own words) | CCON | speeds | vtcon status | est. gain on our frame | rank |
|---|---|---|---|---|---|
| Per-row used length: every cell from the length to the margin is zero by rule, so clear / EOL / row copy / insert / delete touch only the used cells. b11 scroll-nl 2.50 -> 1.50 s | b11 | feed, scroll | HAVE. `l->used` (engine/vtengine.c:376, 632-660, 688, 1327, 1617, 1855-1891; `vt_row_used` vtengine.h:253), `vt_asm_ch_blank(l->c, l->used)` at :648 | 0 (already in) | - |
| Painter reads the model in place: chars one long per group, attrs in place, glyph addresses formed in registers; no padded copies, no pointer array. 6-12% a row (plain 1.96 -> 1.67 ms) | b11 | painter | PARTIAL. vp_span_fast builds a `gp[]` pointer array per span (render/painter.c:51-71), the cost the 10-19 profile names (gp[] setup 144k instr of 891k, shape 1). Vamos-refuted hoist (feed-perf, fix 3) was a different change (gather into a row buffer) | 3-6 ms of 61 (instruction side only; bus-bound part untouched) | 3 |
| Per-flush painter setup built once (pens tables, owner / prefix masks, plane list, glyph cache); per row only the span is placed. b13a -> b13b 28.21 -> 27.79 (1.5%) | b13 | painter | MISSING. per-span `fg/bg` bit decode per plane inside vp_span_fast (painter.c:62-68) and `planes_ok` / `target()` + LockLayer on every call (amiga_render.c:1735-1750) | 2-4 ms (the 144k+256k setup instr of the 10-19 profile, cf. his 1.5%) | 2 |
| Mixed group with two colours: masks from two pen lookups plus a prefix mask instead of 5 lookups a plane. 3-plane row 4.88 -> 4.50 ms (8%) | b11 | painter | N/A as built: our runs are uniform fg/bg (`vp_span_fast(..., fg, bg, mask)`), so a mixed run is split into several spans; the cost shows up as per-run setup (597 instr per 11-char run in vt_asm_put_run) | folds into the setup row above | - |
| Register path: no edge, 8 lines, 2-3 planes, X = 0 -> masks and destinations kept in registers | b11 | painter | MISSING (our vp_asm_plane is one call per plane per span, ~16 instr per plane per row for a 1-cell run, feed-perf) | 2-4 ms | 4 |
| putseg: copy + two fills + row length + dirty mark in one asm pass per text segment. btest bulk 4.85 -> 4.3 us/byte | b11 | feed | PARTIAL. we have vt_asm_put_run (engine/vtengine_68k.s) but it costs 597 instr per 11-char run "setup, not characters" (feed-perf). Grep it for the same fold of fill + used + dirty | 3-6 ms of 49 | 5 |
| prscan: no byte steps to reach a long boundary (020 reads misaligned longs) | b11 | feed | UNKNOWN (not grepped further; vt_asm scan loop in vtengine_68k.s) | <1 ms | 7 |
| mscan: the mask scan (which planes are in use) as long ORs, stopping at each row's length; ~45 ms a call on stock, asked for at the next flush after a form feed. clear-page 1.79 -> 1.55 | b13 | painter / clear | PARTIAL. we track `r->seen` / `r->mask` incrementally (amiga_render.c:1737-1745, 3339, 3566) and `vr_mask_begin/end` (:1018, :1039); no full scan to fix, so no direct equivalent | ~0 on the ncurses frame (matters after FF / clear) | 8 |
| wsigok: rebuild the main loop's Wait mask only after a pass that did more than fast-path writes. E's per-wakeup cost ~100 us; bytewise 2.23 -> 1.98, sgr 6.72 -> 6.43 (4%), b13n -> b13c 27.2 -> 26.6 | b13 | packets | MISSING. mask rebuilt every loop pass (handler/vtcon_handler.c:6638-6660, nine `if` ports + `vtwin_sigmask` + `own_screen_retry` + `screen_pending` before each `Wait`) | 0.3-0.8 ms of the 12 ms packet cost per frame (about 4-6%; bigger for bytewise writes) | 1 (cheapest, safest) |
| PACE=FAST: next flush waits 2.5x the last flush's cost (floor 40 ms, cap 160) instead of 1.5x; -1.2 s of 26.3 (4.5%) on stock, sgr 6.77 -> 6.15. Cost: flood redraws every ~0.23 s instead of ~0.18 s. Opt-in, only if every flushed console has it (J25) | b12/b13p | feed+painter via fewer frames | MISSING. `vt_pace_next` fixed 1.5x, 20-160 ms (render/pace.h:14-26; used render/vtwin.c:594) | not per-frame: on a flood of N frames, ~4-5% fewer repaints; zero for one ncurses frame. Needs product decision (visible update rate) | 6 |
| Single-plane / byte-aligned plain-text path (b9 #7, still the biggest unrealised painter win; not new but not done in vtcon) | b9 | painter | MISSING. vp_span_fast always runs every masked plane through BFINS (painter.c:51-71, painter_68k.s:3) | up to 20-35 ms of 61 (the 106 vs 36 us/cell gap, suspect RMW); needs rig bus-time measurement | 1 for impact (bigger than all rows above) |

Rank key: impact order for the 020 frame is the last row, then setup hoist, then the painter in-place read and
register path; wsigok is rank 1 for cost/risk (a few lines in one loop) but small.

## Tried and dropped by him (do not repeat)

- Whole packet loop in C ("serve"): retested with 3 rounds, bytewise 2.26 -> 2.44, sgr +0.28. Per-packet cost is exec's.
- Fast write inline in the main loop (no dopkt call): +-0.
- Newline precount to avoid per-row clears: scroll-nl -0.12 but wrap-long +0.18, vt-frame +0.15 (3 us a byte to count).
- Style-0 fill skip past the row length: no gain.
- ORing styles in the painter instead of the C scan: +128 us/row at 3 planes (asm added memory RMWs).
- Run list built in C, asm draws (prow8 / prun8): C loop alone 1.13 ms a plain row.
- Deferred row clear (recycled row left pending): model-exact but slower (block-4k 1.03 -> 1.10); a cached full-row zero fill beats the bookkeeping.
- Width-limited scroll (blit only columns that can hold pixels): pixel-correct, sgr unchanged, sync-line +0.13. His srbench: ScrollRaster 53 ms, 3 planes 27 ms, 1 plane 14 ms: planes are the cost, not width or clear.
- Short-span attr strip: the C strip cost more than the painter saved.
- Owner masks cached per bit phase: kept, no measurable gain.

Parallel to our refuted gather hoist (feed-perf fix 3): he also found instruction counts mislead; he decides by
A/B alternating rounds on the cycle-exact emulator and the real machine.

## Items from the b9 table (2026-10-04) that he changed or dropped

- #6 pre-shifted glyph cache per bit phase, four cells per long: still the core of his painter; b11 reshaped it
  (model-direct, no padded copies) and b13 hoisted its setup per flush. Cache validity became a bug class:
  J3 (b15) a global glyph cache shared by two windows in different fonts painted the wrong glyphs; `dgsync()`
  now rebuilds it per console before each direct paint. If we add a cache, key it per font and window.
- #7 pfused1 (one plane, uniform): superseded by pfused / pm reading the model in place with TWOC and the
  register path (b11); pfused1 remains the single-plane case.
- #9 cursor blip waits for rest: J26 (b16) refines it: engine hands WAIT_CHAR to E only when only the blip is owed.
- #10 "wakeup that brought only packets skips the port walk": made concrete and measured as wsigok (b13), 4%.
- #12 wacc (common ACTION_WRITE in C, hand ReplyPkt): kept, but his "serve" attempt to move the whole accept /
  reply round into C was dropped (slower); the per-packet floor is exec's (null handler 12.3 s of 26.3).
- #2 pacing 1.5x: unchanged default; PACE=FAST (2.5x, 40 ms) added as opt-in, shared-console rule J25.
- #13 jsync / jump scroll: now JUMP=AUTO default (a quarter window per blit during a flood); b1 (14.8.26) origin,
  not new in this range.
- #16 code size is speed: unchanged; his b11 note adds "count data accesses in loops, 1.3 us per fetched
  instruction on chip-only".
- Dropped from b9: nothing in the original technique list was removed outright; only the experiments above.

## Open for vtcon

1. Measure bus time on the rig for a single-plane / phase-0 plain store path in render/painter.c before any change.
2. Do the wsigok-style mask cache in handler/vtcon_handler.c:6638 (low risk) and measure packets (12 ms) on the
   rig with the existing PROF counters (`prof_idle`).
3. Per-flush painter setup: collect fg/bg plane decode and `target()` / `planes_ok` once per `vr` flush.
