---
date: 2026-10-04
topic: The speed campaign against CCON, UPDemo, wasabi in the kit, fonts by pixel shape, Workbench colours on the own screen
tags: [vtcon, s1, speed, asm, conbench, ccon, updemo, wasabi, fontpair, ownscreen, rig]
status: final
---

# Handoff 2026-10-04 (session of 2026-10-03/04)

## Tasks

All in `~/Code/vtcon`, branch `main`, 89 commits ahead of origin, NOT pushed (the owner
decides). The tree is clean except the files never to stage (`tests/streams/*`,
`tests/amiga/mallocbench.c`).

Done this session, oldest first:

1. **P2 fonts by pixel shape + the Workbench's colours on UP-Term's own screen** (`7b5157b`).
2. **X1 wasabi** (a friend's remote-development daemon): read, run on the rig, an optional
   install with a generated key (`fd35ab0`, `50d9819`, `3e259bd`).
3. **UPDemo**: the terminal's show-off and benchmark, 11 scenes (`cec7994`).
4. **S1 the speed campaign** (owner: "our term is super slow", "all hotpaths ... in
   assembler", "use the amiga custom chips to the max", "close the gap and beat him",
   "let's win!"): four perf commits (`ce92389`, `9db6c29`, `ae5cfea`, `b1a181a`), the CCON
   read (`f6fa77e`), the side-by-side race (`2ddd064`).

Where the campaign stands: conbench `REPS 3 SYNC`, the stock-speed rig, UP-Term's own PAL
hires screen (16 colours), both consoles in the same 640x180 window (77x20), Amiga dialect:
**UP-Term 30.26 s, CCON 1.2.7 38.32 s**; 11 of 15 rows ours. At the start of the day our
full-screen run was 70.94 s.

Not done, in the order the numbers suggest (all in the ledger under S1):

- The four rows CCON still wins, each by a few percent (seconds, CCON / ours): bytewise
  0.98 / 1.02, wrap-long 1.18 / 1.24, cursor-pos 1.86 / 1.92, sync-line 2.46 / 2.62.
- Colour-heavy output (sgr-colour 7.14 s, sgr-perchar 4.42 s; both consoles are slow).
- Block and blank cells through the planar path (UPDemo's plasma is still ~1 fps on a
  stock machine).
- S2 flicker: the frame clock on the vertical blank; a second screen buffer.
- The 24-bit colour defect (below).

## Critical References

- Ledger: `thoughts/shared/plans/2026-09-28-vtcon.md` -- item **S1** holds every
  measurement of the campaign in order (first pass, CCON read, second pass, third pass),
  **X1** wasabi, **W1** reflow (not started).
- Plan `thoughts/shared/plans/2026-10-03-screens-and-dctelnet.md`: P1.4 and P2 ticked;
  P3-P10 open (P3, the direct path, is partly done by S1).
- Asm: `engine/vtengine_68k.s` (`VT_ASM`), `render/amiga_render_68k.s` (`VR_ASM`). The C
  beside each call is the reference; host tests run the C.
- Engine hot paths: `engine/vtengine.c` -- `vt_line.used` / `dx0` / `dx1`, `mark()`,
  `damage_rows()`, `pend_prepare()` / `pend_scroll()`, `put_ascii_run()`, `csi_fast()`,
  `insert_chars()` / `delete_chars()`, `sgr()`.
- Renderer: `render/amiga_render.c` -- `ink_pen()` (mask widening, `seen`),
  `vr_mask_begin()` / `vr_mask_end()`, `draw_rows()` (blank style once a pass, row tails,
  the full-pass narrowing, `direct_wins()`), `direct_row()` / `direct_cell()`,
  `cursor_flip()` / `cursor_draw()`, `vr_scroll()` (blank skip), `vr_redraw()`.
- Frame pacing and `?2026`: `render/vtwin.c` `vtwin_render()`, `vtwin_write()`.
- Handler loop fast path and the barrier: `handler/vtcon_handler.c` main loop (after
  `wait = Wait(wait)`), `ACTION_WAIT_CHAR`, `ACTION_READ`.
- creep's sources (read only, cloned in the session scratchpad, gone with it):
  github `creep-ltx/AmigaTools` (`ccon/`, `conbench/`), `creep-ltx/wasabi`.

## Recent Changes

### Screens and fonts (`7b5157b`)
- `render/fontpair.[ch]`: topaz 8 <-> TopazPro 16, IBM 8 <-> IBM 16, chosen by the
  screen's pixel shape; profile key `font-aspect = off`. `/font-size` steps through the
  face with the pair's square font as one of its sizes (`fontpair_face`).
- Kit: `dist/fonts/`; install.dos copies only the sizes FONTS: lacks (a marker each),
  FixFonts; Uninstall removes the marked ones.
- Own screen: `wb_pens()` copies the Workbench's DrawInfo pens' colours to pens past the
  16 ANSI ones (5 planes on AGA by default, 8 on a card, nearest ANSI pen on ECS).
- Fixed on the way: `SCREEN name` kept working; a P96 mode is recognised by its depth.

### wasabi (`3e259bd`)
- Installer choice "wasabi: remote development from another computer", off by default.
  `install/wasabikey.c` makes a 20-character key unless one is set (wasabid with no key
  takes any client); started now and from a `;BEGIN UP-Term wasabi` block; the last
  Installer page shows the key. Uninstall stops it and removes daemon / key by markers.
- `dist/wasabi/`: wasabid (the owner's download, sha256 d63e70b0...355e), client, LICENSE
  (MIT), SOURCE.txt.

### UPDemo (`cec7994`)
- `demo/updemo.c` portable core, `updemo_amiga.c` (`C:UPDemo [scene | BENCH]`),
  `updemo_posix.c` (`make demo-host`, `build/updemo`). Scenes: text, colours, scroll
  regions, copper bars, plasma, fire, rotozoomer, vector cubes, sine scroller, palette
  cycling, the end. Frames are bracketed with `?2026` since `b1a181a`.
- `tests/test_updemo.c`: every scene through the engine, the screen comes back.

### Speed (`ce92389`, `9db6c29`, `ae5cfea`, `b1a181a`)
- Engine: `vt_line.used` (cells written since the last clear; host tests abort on a cell
  changed without `mark()`, `VT_CHECK_USED`); dirty spans live in the line; a scroll inside
  rows already damaged whole tracks nothing; a pending scroll that cannot be added to
  becomes damage, not a flush inside the write; `csi_fast` parses a whole
  `ESC [ params final`; SGR checks the colours first; insert / delete character touch only
  the cells in use.
- Asm: `vt_asm_put_run`, `vt_asm_fill`, `vt_asm_rows_up` / `_down`, `vt_asm_cells_move`,
  `vr_asm_cell`. `engbench` checks each on the 68k before timing.
- Renderer: plane mask during a render pass, narrowed at every draw of the whole grid to
  the pens that pass drew; blank-scroll skip; a row's tail of default blanks is one fill or
  nothing; rows with 4+ colour runs go straight into the planes when the text is on a byte
  boundary; the planar cursor is an inversion in the planes in use.
- Handler: the loop skips its other ports when only a DOS packet woke it; WAIT_CHAR and
  READ draw pending output first.
- `?2026` synchronized output: nothing drawn between begin and end (3 frames at most).

### Tools added
- `tests/amiga/engbench.c` (engine alone on the 68k + asm self-checks), `cellbench.c`
  (cells a second per cell kind), `wprobe.c` (conbench's per-write shapes), `dripens.c`.
- `tools/rig/race_rig.py [gif]`, `aspect_rig.py`; `rig.py start --fast`.

## Learnings

- **The rig is a stock-speed A1200.** FS-UAE has no JIT on an ARM Mac; `jit_compiler = 1`
  does nothing. Every timing taken on it is a 14 MHz 68020 timing. `rig.py start --fast`
  (uae_cpu_speed max) runs at host speed, but the Workbench then comes up native PAL 4
  colours (cause not looked into), so colours are not comparable.
- **Our barrier was not honest until `b1a181a`.** WAIT_CHAR did not draw pending output,
  so conbench's SYNC left a frame's work out. sync-line read 0.60 s and is 2.62 s. Any
  UP-Term conbench number before that commit is optimistic where a test has barriers
  (33.22 s, 32.50 s, 50.34 s, 55.78 s ...). CCON's numbers were always honest.
- **Guesses were wrong three times; the profile was right.** The row clear (until
  `mark_rows` stopped raising `used`), the cursor's drawing cost (no effect), the scroll
  blit (it was the plane mask stuck at 3 planes). Use `sample` on a host build
  (`cc -O1 -g -fno-inline`), `engbench` on the rig, and a probe that splits the shapes.
- **A coloured cell is bound by its bytes, not its drawing**: the escape parser was the
  slowest path (52 us a byte). After `csi_fast`: colour a char 19 -> 45 KB/s.
- **ScrollRaster of 616x160 costs 5.6 ms a plane** on the rig (mask 01: 14 ticks for 50,
  FF: 55). The plane mask is worth exactly that, and only when it is narrow.
- **vbcc's memmove moves bytes**: 1136 bytes took 1.6 ms. Never use it on a hot path.
- **CCON has no assembler and no blitter code of its own**; it is fast by not drawing
  (one scroll per write, plane mask, blank-scroll skip, model first).
- The permission classifier refuses running or bundling outside binaries in auto mode
  (wasabid). The owner copied the file with `!`, or left auto mode; conbench and
  ccon-handler were accepted after "you have my permission just go".
- vbcc: `(void)x;` warns (153) and an assigned-but-unused variable warns (65); with
  `-warnings-as-errors` use `-dontwarn=153,65` or `#ifdef` the declaration.
- An asm object must use `section "CODE",code` to link into the handler (vlink: R_PC
  relocation across differently named sections).
- A direct-path build must not pad the layout: the planar path is used only where the
  text is already on a byte boundary; `DIRECT=1` keeps the old always-direct behaviour.

## Artifacts

- Ledger S1 / X1: `thoughts/shared/plans/2026-09-28-vtcon.md`.
- `~/Desktop/upterm-vs-ccon.gif`: the race, 14 frames (UP-Term 29 s, CCON 31 s, shared CPU).
- `~/Desktop/UP-Term-beta-2026-10-03.lha`: OLD -- predates fonts, wasabi, UPDemo and all
  of S1. `make dist` builds a current `build/UP-Term.lha`; the last one built is from
  `3e259bd` (before UPDemo and the speed work).
- `build/updemo`: the demo for the Mac's terminal.
- On the rig disk (`build/rig/vtc`, not in git): `ccon-handler`, `ccon.mount`, `conbench`,
  `wasabid`, `engbench`, `cellbench`, `wprobe`, `UPDemo`.
- Memory: `vtcon_speed_asm_and_custom_chips.md`, `vtcon_rig_runs_at_stock_a1200_speed.md`.

## Next Steps

1. **Win the last four rows.** They share the per-write and per-pass path:
   - narrow the plane mask more often than at a full-grid draw (cursor-pos runs after the
     colour tests with the mask left wide; on a cleared screen wprobe has us ahead, 39
     ticks against 50);
   - trim the write path further (packet -> output -> vtwin_write -> vt_feed -> reply);
   - the wrap: `put_char` twice a row (last column, first of the next).
   Measure with `wprobe`, then conbench `REPS 3 SYNC` through the same window as CCON.
2. **Colour rows**: `cell_style` / `pen_for` / `vt_resolve_colors` per cell in `draw_rows`
   is the next asm target; block and blank cells through the planar path; the planar path
   for text not on a byte boundary (compose a row in fast RAM, shift on the copy).
3. **S2 flicker**: the frame clock from a VERTB interrupt server instead of timer.device
   (`FRAME_MICROS` is 50000: 20 frames a second), so a small update finishes before the
   beam reaches it. Then ask the owner to look.
4. **24-bit colour on a true-colour screen with no free pens** (the rig's Workbench:
   ColorMap 32 entries, `pe_NFree` 0): every RGB and 256 colour falls to the nearest of 15
   shared pens. The RTG path should draw RGB itself (WritePixelArray a span).
5. An `--exact` (cycle-exact) conbench row before any public claim; creep's own run on his
   PiStorm machine is the real verdict (his paste: CCON 24.28 s at SCALE 10, 127x93).
6. A new beta archive when the owner asks (`make dist`, copy to the Desktop).
7. Older open items unchanged: W1 reflow on resize, P1.3 remainder (ScreenMode requester,
   Prefs fields), P3-P10, T3.5 / T3.6 (3.2), the features CCON has and we lack (ledger S1:
   iconify, dropped icons, autosuggestions, queued paste ...).

## Other Notes

- Owner checks still open: the flicker at the bottom during UPDemo scenes after `?2026`
  (rig or A1200); which screen the mouse pointer blinked on (graphics card or PAL); the
  Installer's wasabi page (not run through the Installer UI, only its script blocks);
  the own screen's Workbench colours on their setup; the leftover cursor after Enter on an
  empty prompt (never reproduced).
- The rig hung once at boot this session (the agent refused connections); a restart fixed
  it, eight boots after were fine, no link to a change found.
- In an Amiga-dialect window on UP-Term's own screen the text is red: programs draw with
  screen pen 1, which is ANSI red there. CCON shows the same. Left alone.
- Commit trailer changed during the session: `Co-Authored-By: Claude Fable 5.1
  <noreply@anthropic.com>` (from `cec7994` on).
- Compare runs, as used all session (scripts were in the session scratchpad; the same can
  be typed by hand): keep the screen with
  `NewShell "XCON:0/200/200/50/keeper/OWNSCREEN/SCREENMODE 0x29000/DEPTH 4"`, then
  `NewShell "XCON:0/12/640/180/upwin/SCREEN UP-Term/AMIGA"` or
  `Mount CCON: FROM VTC:ccon.mount` + `NewShell "CCON:0/12/640/180/ccwin/SCREENUP-Term"`,
  and in the window `VTC:conbench NAME TO RAM:conbench.txt REPS 3 SYNC`.
